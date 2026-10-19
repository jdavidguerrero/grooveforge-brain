#pragma once

#include <atomic>
#include <cstdint>
#include <mutex>
#include <thread>

#include <protocol.h>

#include "state/studio_state.h"
#include "ui/input.h"

/**
 * @file bridge_link.h
 * @brief Hilo de I/O del bridge en la cabeza de Studio (theory/01 §2–4, tarea 38).
 *
 * La cabeza es el slave del Bridge Protocol (`02-bridge-protocol.md`): el Teensy manda y la cabeza
 * contesta ACK a cada frame (VERSION a GET_VERSION). Este hilo no incluye LVGL: decodifica los
 * frames en un espejo propio protegido por un mutex corto, y el hilo de UI lo copia a StudioState
 * una vez por cuadro con apply() ("el último valor gana", §3.2).
 */

/// Parser de frames con resincronización deslizante (theory/01 §4.2). Sin E/S: se prueba solo.
class FrameParser {
public:
    using Handler = void (*)(void* ctx, uint8_t cmd, uint8_t seq, const uint8_t* payload, uint8_t len);

    FrameParser(Handler h, void* ctx) : handler_(h), ctx_(ctx) {}

    /// Agrega bytes recibidos y despacha todos los frames completos y válidos.
    void feed(const uint8_t* data, size_t n, uint32_t now_ms);
    /// Descarta un frame a medias si pasaron más de 5 ms sin bytes.
    void tick(uint32_t now_ms);

    uint32_t frames_ok() const { return frames_ok_; }
    uint32_t bytes_dropped() const { return dropped_; }

    static bool known_cmd(uint8_t cmd);

private:
    void parse();

    Handler handler_;
    void* ctx_;
    uint8_t buf_[1024];
    size_t len_ = 0;
    uint32_t last_byte_ms_ = 0;
    uint32_t frames_ok_ = 0, dropped_ = 0;
};

class BridgeLink {
public:
    BridgeLink();
    ~BridgeLink();

    /// Abre el UART (921600 8N1 raw) y arranca el hilo. false si el dispositivo no abre.
    bool start(const char* device);
    void stop();

    /// Hilo de UI: copia al estado lo que llegó desde el último cuadro.
    void apply(StudioState& s);

    /// Envía un gesto táctil como GF_CMD_UI_INPUT (0x45) al Teensy cuando corre conectado por UART.
    void send_ui_input(Ctl c, bool current_shift = false);

private:
    struct Mirror {
        bool hb_seen = false;
        uint32_t last_hb_ms = 0;
        uint8_t engine = 0;
        bool engine_dirty = false;
        float filter[6][5] = {};       ///< normalizado 0–1: [engine][cutoff, resonancia, FLT ENV AMT, GLIDE, DRIVE]
        float lfo[6][7] = {};          ///< normalizado 0–1: [engine][RATE, SHAPE, DIV, cantidad ×4] (gf_params.h)
        float sfx[6][20] = {};         ///< cadena de FX: [engine][4 encendidos, 4 MIX, 12 parámetros]
        float envp[6][8] = {};         ///< normalizado 0–1: [engine][A D S R del amplificador, A D S R del filtro]
        float osc[6][32] = {};         ///< normalizado 0–1: [engine][parada · 4 + slot] slots 0–3, +16 la capa SHIFT
        float mod[6][16] = {};         ///< normalizado 0–1: [engine][parada · 8 + slot] (GF_MOD_SLOT)
        uint8_t filter_dirty = 0, env_dirty = 0, lfo_dirty = 0, sfx_dirty = 0, osc_dirty = 0;   ///< bit por engine
        int16_t key_idx = -1;          ///< 0–11 mayor, 12–23 menor (KEY_DETECTED 0x80)
        uint8_t key_conf = 0;
        bool key_dirty = false;
        uint8_t chord_root = 0xFF, chord_qual = 0xFF, chord_conf = 0;
        bool chord_dirty = false;
        uint16_t bpm_x10 = 0;
        bool bpm_dirty = false;
        uint8_t chroma[12] = {};
        uint8_t snap_from = 0, snap_to = 0, beat_phase = 0;
        bool groove_dirty = false, snap_dirty = false;
        GF_UiState ui{};
        bool ui_dirty = false;
        char preset_names[6][PRESET_MAX][20] = {};   ///< banco por engine (PRESET_ENTRY 0x90)
        uint8_t preset_count[6] = {};
        uint8_t preset_loaded[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};   ///< PRESET_LOADED 0x17
        uint8_t preset_dirty = 0;                    ///< bit por engine
    };

    void run();
    void on_frame(uint8_t cmd, uint8_t seq, const uint8_t* p, uint8_t len);
    void send(uint8_t cmd, uint8_t seq, const uint8_t* payload, uint8_t len);
    static void dispatch(void* ctx, uint8_t cmd, uint8_t seq, const uint8_t* p, uint8_t len);

    int fd_ = -1;
    std::thread thread_;
    std::atomic<bool> running_{false};
    std::atomic<uint8_t> tx_seq_{0};
    std::mutex mtx_;
    Mirror m_;
    FrameParser parser_;
    bool was_up_ = false;
    uint32_t last_apply_ms_ = 0;
};
