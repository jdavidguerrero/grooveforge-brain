#include "link/bridge_link.h"

#include <fcntl.h>
#include <poll.h>
#include <pthread.h>
#include <sched.h>
#include <termios.h>
#include <unistd.h>

#include <cstdio>
#include <cstring>

#include <protocol.h>

#include "studio_tick.h"

namespace {

constexpr uint32_t INTERBYTE_TIMEOUT_MS = 5;    // theory/01 §4.2: un frame de 259 B tarda ~2.8 ms
constexpr uint32_t HEARTBEAT_TIMEOUT_MS = 3000; // 02 §4.3

// Rangos de ENV y FILTER por engine (descriptores del firmware, 28-synth-navigator.cpp). El Teensy
// manda el valor normalizado 0–1 y la cabeza lo vuelve a unidades para la UI.
struct Range { float lo, hi; };
constexpr Range CUTOFF[6] = {{20, 8000}, {20, 8000}, {20, 8000}, {20, 20000}, {500, 20000}, {20, 20000}};
constexpr Range ATTACK[6] = {{0, 5000}, {0, 5000}, {0, 5000}, {0, 5000}, {0, 5000}, {0, 5000}};
constexpr Range DECAY[6] = {{10, 5000}, {10, 5000}, {10, 5000}, {10, 5000}, {5, 3000}, {10, 5000}};
constexpr Range RELEASE[6] = {{10, 5000}, {10, 5000}, {10, 5000}, {10, 5000}, {5, 5000}, {10, 5000}};

float denorm(Range r, float v) { return r.lo + v * (r.hi - r.lo); }

const char* const NOTE[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
const char* const NOTE_UP[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
// Rueda de Camelot: número por tónica (mayor = B, menor = A)
constexpr uint8_t CAMELOT_MAJ[12] = {8, 3, 10, 5, 12, 7, 2, 9, 4, 11, 6, 1};
constexpr uint8_t CAMELOT_MIN[12] = {5, 12, 7, 2, 9, 4, 11, 6, 1, 8, 3, 10};
const char* const QUAL[5] = {"", "m", "7", "m7", "M7"};   // ChordRecognizer: 0=maj 1=min 2=7 3=m7 4=maj7 5=N

void note_name(char* out, size_t n, uint8_t midi) { std::snprintf(out, n, "%s%d", NOTE[midi % 12], midi / 12 - 1); }

}  // namespace

// ── FrameParser ─────────────────────────────────────────────────────────────────────────────────

bool FrameParser::known_cmd(uint8_t c) {
    return c <= 0x07 || (c >= 0x10 && c <= 0x17) || (c >= 0x20 && c <= 0x24) || (c >= 0x30 && c <= 0x35) ||
           (c >= 0x40 && c <= 0x43) || (c >= 0x50 && c <= 0x57) || (c >= 0x60 && c <= 0x65) ||
           (c >= 0x70 && c <= 0x72) || (c >= 0x80 && c <= 0x84) || (c >= 0xF0 && c <= 0xF2);
}

void FrameParser::feed(const uint8_t* data, size_t n, uint32_t now_ms) {
    for (size_t i = 0; i < n; ++i) {
        if (len_ == sizeof(buf_)) {           // no debería pasar: parse() consume siempre; por las dudas
            std::memmove(buf_, buf_ + 1, --len_);
            ++dropped_;
        }
        buf_[len_++] = data[i];
    }
    if (n) last_byte_ms_ = now_ms;
    parse();
}

void FrameParser::tick(uint32_t now_ms) {
    if (len_ && now_ms - last_byte_ms_ > INTERBYTE_TIMEOUT_MS) {
        dropped_ += uint32_t(len_);
        len_ = 0;
    }
}

void FrameParser::parse() {
    size_t pos = 0;
    while (len_ - pos >= GF_FRAME_MIN_SIZE) {
        const uint8_t* f = buf_ + pos;
        if (!known_cmd(f[0])) {               // CMD fuera de los rangos de 02 §2 = como un CRC malo
            ++pos;
            ++dropped_;
            continue;
        }
        const size_t total = GF_FRAME_MIN_SIZE + f[1];
        if (len_ - pos < total) break;        // falta el resto del frame
        if (gf_crc8(f, total - 1) != f[total - 1]) {
            ++pos;                            // resync deslizante: un byte de basura cuesta un frame
            ++dropped_;
            continue;
        }
        handler_(ctx_, f[0], f[2], f + 3, f[1]);
        ++frames_ok_;
        pos += total;
    }
    if (pos) {
        len_ -= pos;
        std::memmove(buf_, buf_ + pos, len_);
    }
}

// ── BridgeLink ──────────────────────────────────────────────────────────────────────────────────

BridgeLink::BridgeLink() : parser_(&BridgeLink::dispatch, this) {}

BridgeLink::~BridgeLink() { stop(); }

bool BridgeLink::start(const char* device) {
    fd_ = ::open(device, O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (fd_ < 0) {
        std::perror("bridge: open");
        return false;
    }
    termios t{};
    tcgetattr(fd_, &t);
    cfmakeraw(&t);
    t.c_cflag |= CLOCAL | CREAD;
    t.c_cflag &= ~tcflag_t(CRTSCTS | CSTOPB | PARENB);
    t.c_cc[VMIN] = 0;
    t.c_cc[VTIME] = 0;
#ifdef B921600
    cfsetispeed(&t, B921600);
    cfsetospeed(&t, B921600);
#else
    cfsetspeed(&t, 921600);                   // macOS: sin constante, el driver acepta el valor
#endif
    if (tcsetattr(fd_, TCSANOW, &t) != 0) {
        std::perror("bridge: tcsetattr");
        ::close(fd_);
        fd_ = -1;
        return false;
    }
    tcflush(fd_, TCIOFLUSH);
    running_ = true;
    thread_ = std::thread(&BridgeLink::run, this);
    std::printf("bridge: escuchando %s a 921600\n", device);
    return true;
}

void BridgeLink::stop() {
    running_ = false;
    if (thread_.joinable()) thread_.join();
    if (fd_ >= 0) ::close(fd_);
    fd_ = -1;
}

void BridgeLink::run() {
    // theory/01 §4.3: SCHED_FIFO para que el render nunca retrase un ACK. Sin permisos, sigue normal.
    sched_param sp{};
    sp.sched_priority = 50;
    if (pthread_setschedparam(pthread_self(), SCHED_FIFO, &sp) != 0)
        std::printf("bridge: sin SCHED_FIFO (correr con CAP_SYS_NICE para el target de latencia)\n");

    uint8_t rx[512];
    uint32_t last_report = studio_tick_ms();
    while (running_) {
        pollfd p{fd_, POLLIN, 0};
        const int r = ::poll(&p, 1, 5);
        const uint32_t now = studio_tick_ms();
        if (r > 0 && (p.revents & POLLIN)) {
            const ssize_t n = ::read(fd_, rx, sizeof(rx));
            if (n > 0) parser_.feed(rx, size_t(n), now);
        }
        parser_.tick(now);
        if (now - last_report > 10000) {
            last_report = now;
            std::printf("bridge: %u frames ok · %u bytes descartados\n", parser_.frames_ok(), parser_.bytes_dropped());
        }
    }
}

void BridgeLink::dispatch(void* ctx, uint8_t cmd, uint8_t seq, const uint8_t* p, uint8_t len) {
    static_cast<BridgeLink*>(ctx)->on_frame(cmd, seq, p, len);
}

void BridgeLink::send(uint8_t cmd, uint8_t seq, const uint8_t* payload, uint8_t len) {
    GF_Frame f;
    gf_frame_build(&f, GF_Cmd(cmd), seq, payload, len);
    uint8_t wire[GF_PAYLOAD_MAX + GF_FRAME_MIN_SIZE];
    const size_t n = gf_frame_serialize(&f, wire, sizeof(wire));
    if (n && ::write(fd_, wire, n) != ssize_t(n)) std::perror("bridge: write");
}

void BridgeLink::on_frame(uint8_t cmd, uint8_t seq, const uint8_t* p, uint8_t len) {
    // Respuestas primero, fuera del mutex: un ACK nunca espera al render (theory/01 §2.3, regla 2).
    if (cmd == GF_CMD_ACK || cmd == GF_CMD_NACK) return;   // la cabeza todavía no manda comandos
    if (cmd == GF_CMD_GET_VERSION) {
        const uint8_t ver[7] = {1, 0, 0, 0, 0, 0, 0};       // como la cabeza ESP32: [maj, min, patch, build LE]
        send(GF_CMD_VERSION, seq, ver, sizeof(ver));
    } else {
        send(GF_CMD_ACK, seq, nullptr, 0);
    }

    std::lock_guard<std::mutex> lock(mtx_);
    switch (cmd) {
    case GF_CMD_HEARTBEAT:
        m_.hb_seen = true;
        m_.last_hb_ms = studio_tick_ms();
        break;
    case GF_CMD_ENGINE_CHANGED:
        if (len >= 1 && p[0] < 6) {
            m_.engine = p[0];
            m_.engine_dirty = true;
        }
        break;
    case GF_CMD_PARAM_CHANGED: {
        if (len < 6) break;
        uint16_t id;
        float v;
        std::memcpy(&id, p, 2);                         // little-endian, como el Teensy
        std::memcpy(&v, p + 2, 4);
        if (v < 0.0f) v = 0.0f;
        if (v > 1.0f) v = 1.0f;
        // Parámetros del synth: ((0x10 + engine) << 8) | (grupo << 4) | índice; grupo 1 = ENV, 2 = FILTER
        const unsigned hi = id >> 8, group = (id >> 4) & 0xF, idx = id & 0xF;
        if (hi >= 0x10 && hi < 0x16) {
            const unsigned e = hi - 0x10;
            if (group == 2 && idx < 2) {
                m_.filter[e][idx] = v;
                m_.filter_dirty |= uint8_t(1u << e);
            } else if (group == 1 && idx < 4) {
                m_.envp[e][idx] = v;
                m_.env_dirty |= uint8_t(1u << e);
            }
        }
        break;
    }
    case GF_CMD_KEY_DETECTED:
        if (len >= 1 && p[0] < 24) {
            m_.key_idx = p[0];
            m_.key_conf = len >= 2 ? p[1] : 0;
            m_.key_dirty = true;
        }
        break;
    case GF_CMD_CHORD_DETECTED:
        if (len >= 2) {
            m_.chord_root = p[0];
            m_.chord_qual = p[1];
            m_.chord_conf = len >= 3 ? p[2] : 0;
            m_.chord_dirty = true;
        }
        break;
    case GF_CMD_BEAT_DETECTED:
        if (len >= 2) {
            std::memcpy(&m_.bpm_x10, p, 2);
            m_.bpm_dirty = true;
        }
        break;
    case GF_CMD_GROOVE_STATE:
        if (len == 16) {
            std::memcpy(m_.chroma, p, 12);
            if (p[12]) {
                m_.snap_from = p[13];
                m_.snap_to = p[14];
                m_.snap_dirty = true;
            }
            m_.beat_phase = p[15];
            m_.groove_dirty = true;
        }
        break;
    default:
        break;
    }
}

void BridgeLink::apply(StudioState& s) {
    std::lock_guard<std::mutex> lock(mtx_);
    const uint32_t now = studio_tick_ms();
    const bool up = m_.hb_seen && now - m_.last_hb_ms <= HEARTBEAT_TIMEOUT_MS;
    s.link = up ? LinkState::Up : (m_.hb_seen ? LinkState::Lost : LinkState::Connecting);
    if (up != was_up_) {
        std::printf("bridge: enlace %s\n", up ? "ARRIBA" : "PERDIDO");
        was_up_ = up;
    }

    if (m_.engine_dirty) {
        s.engine = m_.engine;
        m_.engine_dirty = false;
        m_.filter_dirty |= uint8_t(1u << m_.engine);   // mostrar los valores ya conocidos de ese engine
        m_.env_dirty |= uint8_t(1u << m_.engine);
    }
    const unsigned e = s.engine;
    if (m_.filter_dirty & (1u << e)) {
        s.cutoff_hz = denorm(CUTOFF[e], m_.filter[e][0]);
        s.resonance = m_.filter[e][1];
    }
    if (m_.env_dirty & (1u << e)) {
        s.env_attack_ms = denorm(ATTACK[e], m_.envp[e][0]);
        s.env_decay_ms = denorm(DECAY[e], m_.envp[e][1]);
        s.env_sustain = m_.envp[e][2];
        s.env_release_ms = denorm(RELEASE[e], m_.envp[e][3]);
    }
    m_.filter_dirty = m_.env_dirty = 0;

    if (m_.key_dirty) {
        const int root = m_.key_idx % 12;
        s.key_minor = m_.key_idx >= 12;
        s.key_camelot = s.key_minor ? CAMELOT_MIN[root] : CAMELOT_MAJ[root];
        std::snprintf(s.key_name, sizeof(s.key_name), "%s %s", NOTE_UP[root], s.key_minor ? "MINOR" : "MAJOR");
        s.key_conf = m_.key_conf;
        m_.key_dirty = false;
    }
    if (m_.chord_dirty) {
        if (m_.chord_root < 12 && m_.chord_qual < 5)
            std::snprintf(s.chord, sizeof(s.chord), "%s%s", NOTE[m_.chord_root], QUAL[m_.chord_qual]);
        else
            std::snprintf(s.chord, sizeof(s.chord), "---");
        s.chord_conf = m_.chord_conf;
        m_.chord_dirty = false;
    }
    if (m_.bpm_dirty) {
        s.bpm = m_.bpm_x10 / 10.0f;
        m_.bpm_dirty = false;
    }
    if (m_.groove_dirty) {
        for (int i = 0; i < 12; ++i) s.chroma[i] = m_.chroma[i] / 255.0f;
        s.bar_phase = m_.beat_phase / 256.0f;
        m_.groove_dirty = false;
    }
    if (m_.snap_dirty) {
        note_name(s.snap_from, sizeof(s.snap_from), m_.snap_from);
        note_name(s.snap_to, sizeof(s.snap_to), m_.snap_to);
        m_.snap_dirty = false;
    }
}
