#pragma once

#include <cstdint>

/**
 * @file studio_state.h
 * @brief Espejo del estado del Brain en la cabeza de Studio (theory doc §3).
 *
 * La UI solo dibuja este struct. Hoy lo llena el modo demo; después lo llenará el hilo de I/O
 * con los frames del bridge (02-bridge-protocol.md).
 */

enum class Mode : uint8_t { Synth, Fx };
enum class LinkState : uint8_t { Demo, Connecting, Up, Lost };
enum class FxView : uint8_t { Wave, Spectrum, Signature };   ///< ONDA · ESPECTRO · FIRMA
enum class FxRoute : uint8_t { Insert, Send, Master };       ///< B1–B3 en FX (05 §0.2)

constexpr int FX_SLOTS = 12;
constexpr int FX_COLS = 120;     ///< columnas del osciloscopio (FX Viz C1)
constexpr int FX_BANDS = 32;
constexpr int FX_GRAINS = 24;
constexpr int PRESET_MAX = 16;

constexpr int OSC_STOPS = 4;      ///< osciladores u operadores que NAV puede elegir
constexpr int OSC_SLOTS = 4;      ///< P1 · P2 · SHIFT P1 · SHIFT P2
constexpr int LFO_DESTS = 4;      ///< CUTOFF · PITCH · AMP · PWM

enum class LfoShape : uint8_t { Sine, Tri, Saw, Sqr, SampleHold };

struct FxGrain { uint16_t pos_ms; uint8_t size_ms; int8_t pitch_st; uint8_t amp; };

/**
 * @brief Flujos de la visualización de FX MAIN (FX Viz §E). El motor manda solo los de la vista
 *        visible; hoy los calcula el demo.
 */
struct FxStream {
    int8_t dry_min[FX_COLS], dry_max[FX_COLS];   ///< ONDA: mín/máx por columna, ±127 = ±1
    int8_t wet_min[FX_COLS], wet_max[FX_COLS];   ///< WET = salida después de DRY/WET
    bool free_run;                               ///< sin cruce por cero en 2 ventanas
    uint8_t spec_dry[FX_BANDS], spec_wet[FX_BANDS];  ///< ESPECTRO: 0 = −60 dB, 240 = 0 dB (0.25 dB/LSB)
    float in_peak;               ///< FIRMA Tape/Bit: pico de entrada, lineal 0–1
    uint32_t attack_ms;          ///< FIRMA Ghost/Modal/Spring: instante del último ataque
    uint8_t mode_amp[8];         ///< FIRMA Cymatic: amplitud de los 8 modos
    FxGrain grains[FX_GRAINS];   ///< FIRMA Granular
    uint8_t grain_count;
    uint16_t write_head_ms;      ///< posición de escritura en el buffer de 2 s
    uint16_t lfo_phase;          ///< FIRMA Chorus: 0–65535 = un ciclo
    float f0_hz;                 ///< FIRMA Sub: fundamental detectada
    float in_env;                ///< FIRMA Sub: nivel de entrada 0–1
};

struct StudioState {
    Mode mode = Mode::Synth;
    LinkState link = LinkState::Demo;

    uint8_t engine = 0;              ///< engine_id 0x00–0x05 (02 §2.2)
    char preset[24] = "A-07 BASS FOUNDRY";
    char preset_names[PRESET_MAX][20] = {};  ///< banco del engine activo; el slot es A-(i+1)
    uint8_t preset_count = 0;
    uint8_t preset_loaded = 6;

    float cutoff_hz = 1240.0f;       ///< 20–20000
    float resonance = 0.62f;         ///< 0–1
    bool filter_bypass = false;

    // AI (02 §2.9)
    uint8_t key_camelot = 8;         ///< 1–12
    bool key_minor = true;           ///< A = menor, B = mayor
    char key_name[12] = "A MINOR";
    uint8_t key_conf = 87;
    char chord[8] = "Am7";
    uint8_t chord_conf = 82;
    char genre[10] = "HOUSE";
    uint8_t genre_conf = 71;
    float bpm = 124.0f;
    float bar_phase = 0.0f;          ///< 0–1 dentro del compás de 4 tiempos
    float chroma[12] = {};           ///< actividad por clase de nota C..B, 0–1 (GROOVE_STATE 0x83)
    bool scale_lock = true;
    char snap_from[6] = "G#3";       ///< último snap de Scale Lock (nota tocada → nota corregida)
    char snap_to[6] = "A3";

    /// Página OSC: valor de cada slot por engine y por oscilador (plantilla en state/osc_catalog.h).
    /// Los slots globales del engine (NOISE, CROSSMOD, MIX…) se guardan en el oscilador 0.
    float osc_val[6][OSC_STOPS][OSC_SLOTS] = {};

    // DX7 de 4 operadores (ADR-005; firmware en desarrollo)
    float fm_ratio[4] = {1.0f, 2.0f, 1.0f, 3.5f};
    uint8_t fm_level[4] = {99, 78, 62, 45};
    uint8_t fm_algorithm = 4;        ///< 1–8
    uint8_t fm_feedback = 3;         ///< 0–7, sobre el operador 4

    // LFO (diseño 12a/12b: especificación, todavía no existe en el firmware)
    LfoShape lfo_shape = LfoShape::Tri;
    float lfo_rate_hz = 2.4f;        ///< 0.05–20 Hz en modo libre
    bool lfo_sync = false;
    uint8_t lfo_div = 1;             ///< índice en 1/4 · 1/8 · 1/16 · 1/8T
    uint8_t lfo_dest = 0;            ///< destino que edita P2 y muestra el medidor
    /// PITCH en ST · AMP 0–1 · PWM 0–1. CUTOFF no se guarda acá: es p2 de PERFORM (mismo valor, diseño 12a).
    float lfo_depth[LFO_DESTS] = {0.0f, 3.0f, 0.0f, 0.0f};
    float lfo_phase = 0.0f;          ///< en ciclos, 0–8 (la ventana muestra 2 u 8 ciclos)

    // Envolvente del filtro de la página ENV
    float env_attack_ms = 12.0f;
    float env_decay_ms = 340.0f;
    float env_sustain = 0.62f;       ///< 0–1
    float env_release_ms = 480.0f;

    /// Espectro de 32 bandas log 20 Hz–20 kHz; 0 = −60 dB, 255 = 0 dB (frame propuesto, ADR-002)
    uint8_t spectrum[32] = {};

    // Controles de página (ADR-004): P1/P2 y SHIFT
    float p1 = 0.48f;                ///< PERFORM: FLT ENV AMT
    float p2 = 0.20f;                ///< PERFORM: LFO→CUTOFF
    bool shift = false;

    float volume_db = -6.0f;

    // Entrada estéreo (ADR-004 rev. 2: pot IN + pad trasero)
    float in_gain_db = 4.5f;
    int8_t in_pad_db = -10;          ///< 0, −10 o −20
    uint8_t in_level[2] = {};        ///< medidor IN: 0–255 = barra completa
    bool in_clip[2] = {};

    // FX (05-fx-architecture.md; UI: FX Viz)
    uint8_t fx_slot = 2;             ///< 0–11, slot a la vista en FX MAIN
    uint16_t fx_on = (1u << 1) | (1u << 2) | (1u << 6);  ///< bit n = slot n encendido
    bool fx_hold = false;            ///< B6 apretado en FX: suena aunque esté apagado
    FxView fx_view = FxView::Wave;
    FxRoute fx_route[FX_SLOTS] = {}; ///< ruteo por slot; por defecto, la capa del FX en 05 §1
    float fx_val[FX_SLOTS][4] = {};  ///< [0] DRY/WET %, [1–3] parámetros en unidades del catálogo
    FxStream fx = {};
};
