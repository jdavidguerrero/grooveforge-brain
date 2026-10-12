#include "sim/demo_source.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <initializer_list>

#include "state/fx_catalog.h"
#include "state/osc_catalog.h"

namespace {

// Respuesta aproximada de un low-pass de 4 polos con pico de resonancia (studio-data.js: filterDb).
float filter_db(float f, float fc, float res) {
    const float r = f / fc;
    const float lp = -10.0f * std::log10(1.0f + std::pow(r, 8.0f));
    const float l2 = std::log2(r);
    return lp + res * 14.0f * std::exp(-(l2 * l2) / 0.05f);
}

struct KeyDemo {
    uint8_t camelot; bool minor; const char* name; const char* chord; uint8_t conf;
    uint16_t chord_pcs;  ///< bit n = clase de nota n (0 = C) presente en el acorde
};
constexpr uint16_t pcs(std::initializer_list<int> l) { uint16_t m = 0; for (int n : l) m |= uint16_t(1u << n); return m; }
const KeyDemo KEYS[] = {
    {8, true, "A MINOR", "Am7", 87, pcs({9, 0, 4, 7})},
    {7, true, "D MINOR", "Dm9", 81, pcs({2, 5, 9, 0, 4})},
    {9, true, "E MINOR", "Em7", 84, pcs({4, 7, 11, 2})},
    {8, false, "C MAJOR", "Cmaj7", 78, pcs({0, 4, 7, 11})},
};

// ── FX MAIN (FX Viz: nt, wave, amps, wet, specData) ─────────────────────────────

constexpr float TWO_PI = 6.2831853f;
constexpr float SEQ[4] = {110.0f, 110.0f, 146.83f, 98.0f};
constexpr float MODE_RATIO[8] = {1.0f, 1.59f, 2.14f, 2.3f, 2.65f, 2.92f, 3.16f, 3.5f};
constexpr uint32_t ATTACK_PERIOD_MS = 4000;  // cabezal de las firmas Ghost / Modal / Spring

struct Note { float f, fp, env, tn; };

/// Nota que suena en t: secuencia de 4 notas de 0.6 s con envolvente percusiva.
Note note_at(float t) {
    const float L = 0.6f;
    const int k = int(std::floor(t / L));
    const float tn = t - k * L;
    return {SEQ[k % 4], SEQ[(k + 3) % 4], 0.3f + 0.65f * std::exp(-tn * 2.5f), tn};
}

float wave(float f, float s, float ph = 0.0f) {
    const float a = TWO_PI * f * s + ph;
    return (0.7f * std::sin(a) + 0.25f * std::sin(2 * a + 0.5f) + 0.12f * std::sin(3 * a + 1.1f)) / 1.1f;
}

void mode_amps(float t, const Note& n, float out[8]) {
    for (int k = 0; k < 8; ++k)
        out[k] = std::clamp((0.35f + 0.65f * n.env) * (0.55f + 0.45f * std::sin(t * (0.7f + 0.23f * k) + k * 1.9f)), 0.08f, 1.0f);
}

/// Salida del FX (antes de DRY/WET) para la muestra en s segundos de la ventana.
float fx_wet(FxSig sig, const float* val, float s, const Note& n, float t, const float* amps) {
    const float d = n.env * wave(n.f, s);
    switch (sig) {
        case FxSig::Tape: {
            const float k = 0.5f + val[1] * 0.49f;  // DRIVE 7.4 dB → k 4.1, la curva del diseño
            return std::tanh(k * d) / std::tanh(k) * 0.92f;
        }
        case FxSig::Bit: {
            const float levels = std::pow(2.0f, std::max(1.0f, val[1]) - 1.0f);
            return std::round(d * levels) / levels;
        }
        case FxSig::Cymatic: {
            float o = 0.55f * d;
            for (int k = 0; k < 8; ++k) o += 0.1f * amps[k] * n.env * std::sin(TWO_PI * n.f * MODE_RATIO[k] * s + k);
            return std::clamp(o, -1.0f, 1.0f);
        }
        default: {  // eco de la nota anterior, corrido en fase
            const float e = wave(n.fp, s, t * 2.1f) * (0.35f + 0.4f * std::exp(-n.tn));
            return std::clamp(0.7f * d + 0.45f * e, -1.0f, 1.0f);
        }
    }
}

float band_of(float f) { return 31.0f * std::log(f / 20.0f) / std::log(1000.0f); }
float gauss(float z, float s) { return std::exp(-z * z / (2 * s * s)); }
double hash(double n) { const double v = std::sin(n * 127.1 + 311.7) * 43758.5453; return v - std::floor(v); }
int8_t q8(float v) { return int8_t(std::lround(std::clamp(v, -1.0f, 1.0f) * 127.0f)); }
uint8_t qdb(float db) { return uint8_t(std::lround((std::clamp(db, -60.0f, 0.0f) + 60.0f) * 4.0f)); }

void demo_fx(StudioState& s, float t, uint32_t now_ms) {
    const FxInfo& fx = FX_CATALOG[s.fx_slot];
    const float* val = s.fx_val[s.fx_slot];
    const float mix = std::min(1.0f, val[0] / 50.0f);  // WET = salida después de DRY/WET
    const Note n = note_at(t);
    float amps[8];
    mode_amps(t, n, amps);
    FxStream& o = s.fx;

    // ONDA: ventana de 42.7 ms, 7 muestras por columna, mín/máx (FX Viz C1).
    constexpr float WIN = 0.0427f;
    for (int i = 0; i < FX_COLS; ++i) {
        float d0 = 9, d1 = -9, w0 = 9, w1 = -9;
        for (int k = 0; k <= 6; ++k) {
            const float sm = (i + k / 6.0f) * WIN / FX_COLS;
            const float dv = n.env * wave(n.f, sm);
            const float wv = dv + (fx_wet(fx.sig, val, sm, n, t, amps) - dv) * mix;
            d0 = std::min(d0, dv); d1 = std::max(d1, dv);
            w0 = std::min(w0, wv); w1 = std::max(w1, wv);
        }
        o.dry_min[i] = q8(d0); o.dry_max[i] = q8(d1);
        o.wet_min[i] = q8(w0); o.wet_max[i] = q8(w1);
    }
    o.free_run = false;

    // ESPECTRO: 32 bandas, la WET agrega o quita según el tipo de FX (FX Viz specData).
    const float fb = band_of(n.f);
    for (int i = 0; i < FX_BANDS; ++i) {
        const float d = -6 - std::max(0.0f, i - fb) * 1.7f - std::max(0.0f, fb - i) * 9
                      + 2 * std::sin(i * 1.7f + t * 3.1f) + 1.5f * std::sin(i * 0.6f - t * 1.3f) + 10 * std::log10(n.env);
        float w = d;
        switch (fx.sig) {
            case FxSig::Tape:
                if (i > fb + 1 && i < 27) w += 4 + 7 * std::sin(float(M_PI) * (i - fb - 1) / (26 - fb));
                if (i > 26) w -= (i - 26) * 3.5f;
                break;
            case FxSig::Bit:
                if (i > fb + 1) w += 5 + 3 * std::sin(i * 2.3f);  // escalones = armónicos y alias arriba
                break;
            case FxSig::Cymatic:
                for (int k = 0; k < 8; ++k) w += 15 * amps[k] * gauss(i - band_of(n.f * MODE_RATIO[k]), 0.6f);
                break;
            default:
                if (i > 19) w -= (i - 19) * 1.6f;
                w += 9 * gauss(i - band_of(n.fp * 2), 1.1f);
                break;
        }
        o.spec_dry[i] = qdb(d);
        o.spec_wet[i] = qdb(d + (w - d) * mix);
    }

    // FIRMA: los datos chicos de cada tipo (FX Viz §E).
    o.in_peak = std::min(0.98f, n.env * 0.98f + 0.02f * std::sin(t * 9));
    o.attack_ms = now_ms - now_ms % ATTACK_PERIOD_MS;
    for (int k = 0; k < 8; ++k) o.mode_amp[k] = uint8_t(amps[k] * 255);
    o.f0_hz = n.f;
    o.in_env = 0.35f + 0.65f * n.env;
    o.lfo_phase = uint16_t(std::fmod(t * (fx.sig == FxSig::Chorus ? val[2] : 0.4f), 1.0f) * 65535);

    // Granular: granos que nacen a DENSITY por segundo y viven N/DENSITY s; el cabezal escribe a 1×.
    const float dens = fx.sig == FxSig::Granular ? std::max(1.0f, val[2]) : 40.0f;
    const int count = std::clamp(int(std::lround(dens * 0.45f)), 1, FX_GRAINS);
    const float life = count / dens;
    const long g0 = long(std::floor(t * dens));
    static const int8_t LANES[6] = {-12, 0, 0, 0, 7, 12};
    for (int j = 0; j < count; ++j) {
        const long g = g0 - j;
        const float birth = g / dens, age = t - birth;
        const float am = std::sin(float(M_PI) * std::min(1.0f, age / life));
        const float head = std::fmod(birth * 1000.0f, 2000.0f);
        const float pos = std::fmod(head - 80 - float(hash(g)) * 700 + 4000, 2000.0f);
        o.grains[j] = {uint16_t(pos), uint8_t(50 + hash(g + 0.37) * 90), LANES[int(hash(g + 0.71) * 6)], uint8_t(std::max(0.0f, am) * 255)};
    }
    o.grain_count = uint8_t(count);
    o.write_head_ms = uint16_t(std::fmod(t * 1000.0f, 2000.0f));

    // Medidor IN: nivel según la nota y la ganancia; clip si pasa de 0 dBFS.
    const float gain = std::pow(10.0f, (s.in_gain_db - 4.5f) / 20.0f);
    for (int j = 0; j < 2; ++j) {
        const float l = std::clamp((0.5f + 0.1f * std::sin(t * 5.3f + j * 1.3f)) * (0.75f + 0.35f * n.env), 0.1f, 1.0f) * gain;
        s.in_level[j] = uint8_t(std::min(1.0f, l) * 255);
        s.in_clip[j] = l > 1.0f;
    }
}

const char* const PRESETS[] = {
    "INIT PATCH", "ACID ROOTS", "WARM PAD", "DEEP KEYS", "NIGHT BUS", "DUST LEAD",
    "BASS FOUNDRY", "LOW CATHEDRAL", "GLASS STAIRS", "ORGANIC PLUCK", "SLOW BLOOM", "MELODIC ARP",
};

}  // namespace

void demo_init(StudioState& s) {
    fx_load_defaults(s);
    osc_load_defaults(s);
    s.preset_count = uint8_t(sizeof(PRESETS) / sizeof(PRESETS[0]));
    for (int i = 0; i < s.preset_count; ++i) std::snprintf(s.preset_names[i], sizeof(s.preset_names[i]), "%s", PRESETS[i]);
    s.preset_loaded = 6;
    std::snprintf(s.preset, sizeof(s.preset), "A-07 %s", PRESETS[6]);
}

void demo_tick(StudioState& s, uint32_t now_ms) {
    const float t = now_ms / 1000.0f;

    // Cutoff: barrido logarítmico lento 300 Hz ↔ 6 kHz (periodo 12 s) para ver moverse la línea.
    const float sweep = 0.5f - 0.5f * std::cos(t * 2.0f * float(M_PI) / 12.0f);
    s.cutoff_hz = 300.0f * std::pow(20.0f, sweep);
    s.resonance = 0.62f;

    // Compás de 4 tiempos a s.bpm.
    const float beats = t * s.bpm / 60.0f;
    s.bar_phase = std::fmod(beats, 4.0f) / 4.0f;

    // Espectro: ruido musical de base + bajo pulsante + forma del filtro (studio-data.js: spectrum).
    for (int i = 0; i < 32; ++i) {
        const float x = i / 31.0f;
        const float f = 20.0f * std::pow(1000.0f, (i + 0.5f) / 32.0f);
        float db = -12.0f - x * 20.0f
                 + std::exp(-std::pow((i - 4) / 2.2f, 2.0f)) * 9.0f * (0.55f + 0.45f * std::sin(t * 13.0f))
                 + std::sin(t * 2.3f + i * 0.9f) * 4.0f
                 + std::sin(t * 5.7f + i * 2.7f) * 3.0f;
        if (!s.filter_bypass) db += filter_db(f, s.cutoff_hz, s.resonance) * 0.85f;
        db = std::clamp(db, -60.0f, 0.0f);
        s.spectrum[i] = uint8_t((db + 60.0f) / 60.0f * 255.0f);
    }

    // Tonalidad y acorde: cambian cada 8 s.
    const KeyDemo& k = KEYS[(now_ms / 8000) % (sizeof(KEYS) / sizeof(KEYS[0]))];
    s.key_camelot = k.camelot;
    s.key_minor = k.minor;
    std::snprintf(s.key_name, sizeof(s.key_name), "%s", k.name);
    std::snprintf(s.chord, sizeof(s.chord), "%s", k.chord);
    s.key_conf = k.conf;

    // Cromagrama: las notas del acorde dominan; el resto queda bajo (studio-data.js: chroma).
    for (int n = 0; n < 12; ++n) {
        const float base = (k.chord_pcs >> n) & 1u ? 0.85f : 0.12f;
        s.chroma[n] = std::clamp(base * (0.8f + 0.3f * std::sin(t * 2.1f + n * 1.7f)), 0.05f, 1.0f);
    }

    // P1/P2 respiran despacio para ver las barras de la fila 2.
    s.p1 = 0.48f + 0.10f * std::sin(t * 0.7f);
    s.p2 = 0.20f + 0.08f * std::sin(t * 0.43f + 1.0f);

    demo_fx(s, t, now_ms);

    // Fase del LFO: avanza a RATE (o a la división sincronizada con el BPM). En el firmware la
    // calcula el motor y la manda a 30 Hz (diseño 12a, d: lfo.phase).
    static uint32_t last_ms = now_ms;
    static const float DIV_BEATS[4] = {1.0f, 0.5f, 0.25f, 1.0f / 3.0f};
    const float hz = s.lfo_sync ? s.bpm / 60.0f / DIV_BEATS[s.lfo_div % 4] : s.lfo_rate_hz;
    s.lfo_phase = std::fmod(s.lfo_phase + hz * (now_ms - last_ms) / 1000.0f, 8.0f);
    last_ms = now_ms;
}
