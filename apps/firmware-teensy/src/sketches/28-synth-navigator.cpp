// GrooveForge Brain — Sketch 28: Synth Navigator
// Sprint 38 — OB-6 engine + expansión de audio graph a 6 slots de engine
//
// Jerarquía de pantallas:
//   Nivel 0  ENGINE_LIST  — seleccionar engine (Moog / Juno / Prophet / OB-6)
//   Nivel 1  ENGINE_SUBHOME — performance: Cutoff y Resonance en ENC L/R
//   Nivel 2  GROUP_VIEW   — edición por grupos (OSC / ENV / FILTER / LFO)
//
// Audio: solo MoogModelD produce audio en esta demo. Juno y Prophet tienen
// sus descriptores de parámetros completos para navegación y bridge, pero
// su audio requiere instancias separadas de AudioOutputI2S y AudioControlSGTL5000,
// lo cual conflicta con el codec compartido. Ver TODO abajo.
//
// Flujo de audio:
//   MoogModelD (contiene internamente AudioOutputI2S + AudioControlSGTL5000)
//   MidiHost USB-A → noteOn/noteOff/CC → MoogModelD
//
// Bridge: UART Serial1 921600 8N1 → ESP32-S3
//   param_id namespace:
//     0x00F4 — top_mode (1 = SYNTH)
//     0x00F5 — synth_level (0=LIST, 1=SUBHOME, 2=GROUP_VIEW)
//     0x00F6 — group + cursor (group*10 + cursor)
//     0x1xyz — synth params (engine<<8 | group<<4 | param_idx)

#include <Audio.h>
#include <Wire.h>
#include <string.h>

#include "../engines/moog_model_d.h"
#include "../engines/juno_106.h"
#include "../engines/prophet_5.h"
#include "../engines/ob6.h"
#include "../engines/arp2600.h"
#include "../engines/dx7.h"
#include "../audio/audio_graph.h"
#include "../fx/fx_manager.h"
// FX concrete types — necesarios en apply_fx_param() para static_cast<TYPE*>
#include "../fx/ghost_echo.h"
#include "../fx/modal_reverb.h"
#include "../fx/phase_chorus.h"
#include "../fx/bit_sculpt.h"
#include "../fx/tape_saturate.h"
#include "../fx/cymatic_resonator.h"
#include "../fx/granular_cloud.h"
#include "../fx/spring_plate.h"
#include "../fx/sub_genesis.h"
#include "../ui/encoders.h"
#include "../ui/buttons.h"
#include "../ui/led_ring.h"
#include "../ui/device_state.h"
#include "../usb/midi_host.h"
#include "../bridge/bridge_master.h"
#include "../ml/ml_engine.h"
#include "../ml/scale_lock.h"
#include "../ml/auto_harmonize.h"
#include "../ml/smart_arp.h"
#include "../ml/velocity_learn.h"
#include "../fx/beat_sync.h"
#include <protocol.h>

// ── Constantes ──────────────────────────────────────────────────────────────────

static constexpr uint8_t  NUM_ENGINES          = 6;  // 0=Moog, 1=Juno, 2=Prophet, 3=OB-6, 4=DX7, 5=ARP2600
static constexpr uint32_t AI_HOLD_MS           = 4000;
static constexpr uint32_t AI_PROG_INTERVAL     = 100;
static constexpr uint32_t STATUS_PRINT_MS      = 2000;  // diagnóstico — reducir a 5000 cuando audio funcione

// ── Tablas de waveforms (Teensy Audio Library) ──────────────────────────────────
// Mapeados desde índice entero a constante de la librería.
// El índice coincide con el campo is_int en SynthParam.

static const uint16_t WAVEFORMS_4[4] = {
    WAVEFORM_SAWTOOTH, WAVEFORM_SINE, WAVEFORM_SQUARE, WAVEFORM_TRIANGLE
};

// ── Descriptores de parámetros ──────────────────────────────────────────────────

struct SynthParam {
    const char* name;
    float       value;
    float       default_val;
    float       min_val;
    float       max_val;
    float       step;
    bool        is_int;   // true → valor se muestra como entero, step es 1.0f+
};

struct GroupDesc {
    const char* name;
    SynthParam  params[6];   // máx 6 params por grupo (Moog OSC usa 6: VCO1/2/3)
    uint8_t     num_params;
};

struct EngineDesc {
    const char* name;
    GroupDesc   groups[4];   // 0=OSC, 1=ENV, 2=FILTER, 3=LFO
};

// ──────────────────────────────────────────────────────────────────────────────
// Moog Model D
// OSC: 6 params (VCO1 + VCO2 + VCO3 independiente), ENV: 4, FILTER: 2, LFO: 2
// ──────────────────────────────────────────────────────────────────────────────

static EngineDesc g_desc[6] = {
    // ── 0: Moog Model D ─────────────────────────────────────────────────────
    {
        "Moog Model D",
        {
            { // OSC — 6 params: VCO1 + VCO2 + VCO3 independiente
                "OSC",
                {
                    {"wave",     0,    0,    0,    3,    1,    true},   // 0: VCO1 waveform
                    {"oct",      0,    0,   -2,    2,    1,    true},   // 1: octave offset (aplicado en noteOn)
                    {"wave2",    1,    1,    0,    3,    1,    true},   // 2: VCO2 waveform
                    {"detune",   5,    5,    0,   50,    1,    false},  // 3: VCO2 detune (cents)
                    {"wave3",    0,    0,    0,    3,    1,    true},   // 4: VCO3 waveform (NEW)
                    {"vco3 int",-12, -12,  -24,   24,    1,    true},  // 5: VCO3 interval (semitones) (NEW)
                },
                6
            },
            { // ENV
                "ENV",
                {
                    {"attack",  20,   20,    0, 5000, 10,   false},
                    {"decay",   300,  300,  10, 5000, 10,   false},
                    {"sustain", 0.4f, 0.4f,  0,    1, 0.01f,false},
                    {"release", 400,  400,  10, 5000, 10,   false},
                },
                4
            },
            { // FILTER
                "FILTER",
                {
                    {"cutoff",    800,  800,   20, 8000, 20,   false},
                    {"resonance", 0.7f, 0.7f, 0.1f, 5.0f, 0.05f, false},
                    {nullptr, 0, 0, 0, 0, 0, false},
                    {nullptr, 0, 0, 0, 0, 0, false},
                },
                2
            },
            { // LFO — solo almacenamiento para display; LFO real es trabajo futuro
                "LFO",
                {
                    {"lfo_rate",  1.0f, 1.0f, 0.1f, 10, 0.1f, false},
                    {"lfo_depth", 0.3f, 0.3f,    0,  1, 0.01f,false},
                    {nullptr, 0, 0, 0, 0, 0, false},
                    {nullptr, 0, 0, 0, 0, 0, false},
                },
                2
            },
        }
    },

    // ── 1: Juno-106 ─────────────────────────────────────────────────────────
    {
        "Juno-106",
        {
            { // OSC
                "OSC",
                {
                    {"wave",      0,    0,  0,  3, 1,    true},
                    {"sub_level", 0.3f, 0.3f, 0, 1, 0.02f,false},
                    {"noise",     0.1f, 0.1f, 0, 1, 0.02f,false},
                    {nullptr, 0, 0, 0, 0, 0, false},
                },
                3
            },
            { // ENV
                "ENV",
                {
                    {"attack",  20,   20,    0, 5000, 10,    false},
                    {"decay",   300,  300,  10, 5000, 10,    false},
                    {"sustain", 0.5f, 0.5f,  0,    1, 0.01f, false},
                    {"release", 400,  400,  10, 5000, 10,    false},
                },
                4
            },
            { // FILTER
                "FILTER",
                {
                    {"cutoff",    1000, 1000, 20, 8000, 20,    false},
                    {"resonance", 0.7f, 0.7f, 0.1f, 5.0f, 0.05f, false},
                    {nullptr, 0, 0, 0, 0, 0, false},
                    {nullptr, 0, 0, 0, 0, 0, false},
                },
                2
            },
            { // LFO — chorus del Juno: campo categorico (off/I/II) + modo
                "LFO",
                {
                    {"chorus",      0, 0, 0, 1, 1, true},
                    {"chorus_mode", 1, 1, 1, 2, 1, true},
                    {nullptr, 0, 0, 0, 0, 0, false},
                    {nullptr, 0, 0, 0, 0, 0, false},
                },
                2
            },
        }
    },

    // ── 2: Prophet-5 ────────────────────────────────────────────────────────
    {
        "Prophet-5",
        {
            { // OSC
                "OSC",
                {
                    {"wave_a",   1,    1,    0, 3,  1,     true},
                    {"wave_b",   1,    1,    0, 3,  1,     true},
                    {"interval", 0,    0,    0, 24, 1,     true},
                    {"crossmod", 0,    0,    0, 1,  0.02f, false},
                },
                4
            },
            { // ENV
                "ENV",
                {
                    {"attack",  20,   20,    0, 5000, 10,    false},
                    {"decay",   300,  300,  10, 5000, 10,    false},
                    {"sustain", 0.5f, 0.5f,  0,    1, 0.01f, false},
                    {"release", 400,  400,  10, 5000, 10,    false},
                },
                4
            },
            { // FILTER
                "FILTER",
                {
                    {"cutoff",    1000, 1000, 20, 8000, 20,    false},
                    {"resonance", 0.7f, 0.7f, 0.1f, 5.0f, 0.05f, false},
                    {nullptr, 0, 0, 0, 0, 0, false},
                    {nullptr, 0, 0, 0, 0, 0, false},
                },
                2
            },
            { // LFO
                "LFO",
                {
                    {"osc_mix",   0.5f, 0.5f, 0, 1, 0.02f, false},
                    {"lfo_depth", 0.0f, 0.0f, 0, 1, 0.02f, false},
                    {nullptr, 0, 0, 0, 0, 0, false},
                    {nullptr, 0, 0, 0, 0, 0, false},
                },
                2
            },
        }
    },

    // ── 3: OB-6 ─────────────────────────────────────────────────────────────
    {
        "OB-6",
        {
            { // OSC
                "OSC",
                {
                    {"wave1",  0,    0,    0,   3,    1,     true},
                    {"wave2",  0,    0,    0,   3,    1,     true},
                    {"detune", 0,    0,  -12,  12,    0.5f,  false},
                    {"sub",    0.1f, 0.1f, 0,   1,    0.01f, false},
                },
                4
            },
            { // ENV
                "ENV",
                {
                    {"attack",  10,    10,    0, 5000, 10,    false},
                    {"decay",   300,   300,  10, 5000, 10,    false},
                    {"sustain", 0.6f,  0.6f,  0,    1, 0.01f, false},
                    {"release", 500,   500,  10, 5000, 10,    false},
                },
                4
            },
            { // FILTER
                "FILTER",
                {
                    {"cutoff",  2000,  2000, 20, 20000, 100,   false},
                    {"res",     1.5f,  1.5f, 0.7f, 5.0f, 0.1f, false},
                    {"env_amt", 0.4f,  0.4f,  0,    1,   0.01f, false},
                    {nullptr, 0, 0, 0, 0, 0, false},
                },
                3
            },
            { // LFO — placeholder
                "LFO",
                {
                    {"rate",  0, 0, 0, 1, 0.01f, false},
                    {"depth", 0, 0, 0, 1, 0.01f, false},
                    {nullptr, 0, 0, 0, 0, 0, false},
                    {nullptr, 0, 0, 0, 0, 0, false},
                },
                2
            },
        }
    },

    // ── 4: DX7 ─────────────────────────────────────────────────────────────────
    {
        "DX7",
        {
            { // FM / OSC
                "FM",
                {
                    {"ratio",  2.0f, 2.0f, 0.5f, 8.0f, 0.5f,  false},
                    {"depth",  0.5f, 0.5f, 0.0f, 2.0f, 0.05f, false},
                    {"c_wave", 0,    0,    0,    3,    1,     true},
                    {"m_wave", 0,    0,    0,    3,    1,     true},
                },
                4
            },
            { // ENV
                "ENV",
                {
                    {"attack",  5,    5,    0, 5000, 5,    false},
                    {"decay",   200,  200,  5, 3000, 10,   false},
                    {"sustain", 0.8f, 0.8f, 0, 1,   0.01f,false},
                    {"release", 300,  300,  5, 5000, 10,   false},
                },
                4
            },
            { // FILTER
                "FILTER",
                {
                    {"cutoff",  15000, 15000, 500, 20000, 500,   false},
                    {"res",     0.7f,  0.7f,  0.7f, 3.0f, 0.1f, false},
                    {"env_amt", 0.2f,  0.2f,  0,    1,    0.01f, false},
                },
                3
            },
            { // LFO — placeholder
                "LFO",
                {
                    {"rate",  0, 0, 0, 1, 0.01f, false},
                    {"depth", 0, 0, 0, 1, 0.01f, false},
                    {nullptr, 0, 0, 0, 0, 0, false},
                    {nullptr, 0, 0, 0, 0, 0, false},
                },
                2
            },
        }
    },

    // ── 5: ARP 2600 ─────────────────────────────────────────────────────────────
    {
        "ARP 2600",
        {
            { // OSC
                "OSC",
                {
                    {"wave",    0,    0,    0,    3,    1,     true},
                    {"vco2rt",  1.0f, 1.0f, 0.5f, 4.0f, 0.1f, false},
                    {"ring",    0.5f, 0.5f, 0,    1,    0.01f, false},
                    {"noise",   0.0f, 0.0f, 0,    1,    0.01f, false},
                },
                4
            },
            { // ENV
                "ENV",
                {
                    {"attack",  10,   10,   0, 5000, 10,   false},
                    {"decay",   300,  300,  10, 5000, 10,   false},
                    {"sustain", 0.6f, 0.6f, 0,  1,   0.01f,false},
                    {"release", 500,  500,  10, 5000, 10,   false},
                },
                4
            },
            { // FILTER
                "FILTER",
                {
                    {"cutoff",  2000,  2000, 20, 20000, 100,   false},
                    {"res",     1.5f,  1.5f, 0.7f, 5.0f, 0.1f, false},
                    {"env_amt", 0.5f,  0.5f, 0,    1,   0.01f, false},
                },
                3
            },
            { // LFO — placeholder
                "LFO",
                {
                    {"rate",  0, 0, 0, 1, 0.01f, false},
                    {"depth", 0, 0, 0, 1, 0.01f, false},
                    {nullptr, 0, 0, 0, 0, 0, false},
                    {nullptr, 0, 0, 0, 0, 0, false},
                },
                2
            },
        }
    },
};

// ── Descriptores de parámetros FX ─────────────────────────────────────────────
// Orden: espeja el sketch 27 y la tabla FX_LABEL_NAMES del ESP32 (view_07/08).
// No mezclar con FxManager IDs — ver FX_MANAGER_ID[] más abajo para el mapeo.

struct FxParam {
    const char* name;
    float       value;
    float       default_val;
    float       min_val;
    float       max_val;
    float       step;
    bool        is_int;
};

// [0] Ghost Echo — §1.3
// delay_ms arranca en 10ms (mínimo que acepta GhostEcho::setDelayMs); 0ms se clampea
// internamente a 10ms, pero el display mostraría 0% cuando el FX suena a 10ms.
static FxParam params_ghost[] = {
    {"delay_ms",  10.0f,   10.0f,   0.0f,    200.0f,  10.0f,  false},
    {"feedback",  0.4f,    0.4f,    0.0f,    0.95f,   0.01f,  false},
    {"tape_lp",   5000.0f, 5000.0f, 1000.0f, 12000.0f,200.0f, false},
    {"mix",       0.5f,    0.5f,    0.0f,    1.0f,    0.02f,  false},
};
// [1] Modal Reverb — §1.7
static FxParam params_modal[] = {
    {"material",  0.0f, 0.0f, 0.0f, 3.0f, 1.0f,  true},
    {"size",      1.0f, 1.0f, 0.0f, 2.0f, 1.0f,  true},
    {"decay",     1.0f, 1.0f, 0.5f, 3.0f, 0.1f,  false},
    {"mix",       0.5f, 0.5f, 0.0f, 1.0f, 0.02f, false},
};
// [2] Phase Chorus — §1.8
static FxParam params_chorus[] = {
    {"rate",   0.5f, 0.5f, 0.1f, 10.0f, 0.1f,  false},
    {"depth",  0.5f, 0.5f, 0.0f, 1.0f,  0.02f, false},
    {"drift",  0.3f, 0.3f, 0.0f, 1.0f,  0.02f, false},
    {"voices", 2.0f, 2.0f, 1.0f, 4.0f,  1.0f,  true},
};
// [3] Bit Sculpt — §1.6
static FxParam params_bit[] = {
    {"bits",   8.0f,     8.0f,     1.0f,    16.0f,   1.0f,    true},
    {"srate",  22050.0f, 22050.0f, 1000.0f, 44100.0f,1000.0f, false},
    {"sculpt", 0.5f,     0.5f,     0.0f,    1.0f,    0.02f,   false},
    {"mix",    0.5f,     0.5f,     0.0f,    1.0f,    0.02f,   false},
};
// [4] Tape Saturate — §1.5
static FxParam params_tape[] = {
    {"drive",   4.0f,  4.0f,  0.0f, 12.0f, 0.5f,  false},
    {"wow",     0.3f,  0.3f,  0.0f, 1.0f,  0.02f, false},
    {"flutter", 0.2f,  0.2f,  0.0f, 1.0f,  0.02f, false},
    {"age",     0.3f,  0.3f,  0.0f, 1.0f,  0.02f, false},
};
// [5] Cymatic Resonator — §1.1
static FxParam params_cymatic[] = {
    {"tune",      1.0f,  1.0f,  0.5f, 2.0f,  0.05f, false},
    {"density",   2.0f,  2.0f,  1.0f, 4.0f,  1.0f,  true},
    {"resonance", 5.0f,  5.0f,  1.0f, 25.0f, 1.0f,  false},
    {"lfo_rate",  0.5f,  0.5f,  0.1f, 5.0f,  0.1f,  false},
};
// [6] Granular Cloud — §1.2
static FxParam params_granular[] = {
    {"grain_ms", 100.0f, 100.0f, 5.0f,  500.0f, 5.0f,  false},
    {"speed",    1.0f,   1.0f,   0.5f,  2.0f,   0.05f, false},
    {"freeze",   0.0f,   0.0f,   0.0f,  1.0f,   1.0f,  true},
    {"mix",      0.5f,   0.5f,   0.0f,  1.0f,   0.02f, false},
};
// [7] Spring Plate — §1.10
static FxParam params_spring[] = {
    {"algorithm", 0.0f, 0.0f, 0.0f, 3.0f, 1.0f,  true},
    {"room",      0.6f, 0.6f, 0.0f, 1.0f, 0.02f, false},
    {"damp",      0.5f, 0.5f, 0.0f, 1.0f, 0.02f, false},
    {"mix",       0.5f, 0.5f, 0.0f, 1.0f, 0.02f, false},
};
// [8] Sub Genesis — §1.12
static FxParam params_sub[] = {
    {"sub_level", 0.8f,  0.8f,  0.0f,  1.0f,   0.02f, false},
    {"drive",     2.0f,  2.0f,  1.0f,  8.0f,   0.5f,  false},
    {"cutoff",    100.0f,100.0f,40.0f, 200.0f, 5.0f,  false},
    {"mix",       0.5f,  0.5f,  0.0f,  1.0f,   0.02f, false},
};

struct FxDescriptor {
    const char* name;
    FxParam*    params;
    uint8_t     num_params;
};

// Ordered by UI cursor (sketch 27 / bridge_get_param_name() / view_07 table order).
static FxDescriptor fx_desc[FxManager::NUM_FX] = {
    {"Ghost Echo",        params_ghost,   4},  // UI idx 0  → FxManager::FX_GHOST_ECHO
    {"Modal Reverb",      params_modal,   4},  // UI idx 1  → FxManager::FX_MODAL_REVERB
    {"Phase Chorus",      params_chorus,  4},  // UI idx 2  → FxManager::FX_PHASE_CHORUS
    {"Bit Sculpt",        params_bit,     4},  // UI idx 3  → FxManager::FX_BIT_SCULPT
    {"Tape Saturate",     params_tape,    4},  // UI idx 4  → FxManager::FX_TAPE_SATURATE
    {"Cymatic Resonator", params_cymatic, 4},  // UI idx 5  → FxManager::FX_CYMATIC_RESONATOR
    {"Granular Cloud",    params_granular,4},  // UI idx 6  → FxManager::FX_GRANULAR_CLOUD
    {"Spring Plate",      params_spring,  4},  // UI idx 7  → FxManager::FX_SPRING_PLATE
    {"Sub Genesis",       params_sub,     4},  // UI idx 8  → FxManager::FX_SUB_GENESIS
};

// Maps UI cursor index → FxManager audio ID.
// UI order (sketch 27) ≠ FxManager order (05-fx-architecture.md §1).
// Este mapeo es la única fuente de verdad entre los dos sistemas de IDs.
static constexpr uint8_t FX_MANAGER_ID[FxManager::NUM_FX] = {
    FxManager::FX_GHOST_ECHO,          // UI[0] Ghost Echo
    FxManager::FX_MODAL_REVERB,        // UI[1] Modal Reverb
    FxManager::FX_PHASE_CHORUS,        // UI[2] Phase Chorus
    FxManager::FX_BIT_SCULPT,          // UI[3] Bit Sculpt
    FxManager::FX_TAPE_SATURATE,       // UI[4] Tape Saturate
    FxManager::FX_CYMATIC_RESONATOR,   // UI[5] Cymatic Resonator
    FxManager::FX_GRANULAR_CLOUD,      // UI[6] Granular Cloud
    FxManager::FX_SPRING_PLATE,        // UI[7] Spring Plate
    FxManager::FX_SUB_GENESIS,         // UI[8] Sub Genesis
};

// ── Engines de audio ────────────────────────────────────────────────────────────
// Sprint 36: AudioOutputI2S y AudioControlSGTL5000 viven en src/audio/audio_graph.cpp
// como singletons compartidos. Cada engine expone su output via getOutput() y nos
// conectamos manualmente a g_engine_mix con AudioConnections globales (abajo).
// audio_graph::set_active_engine(idx) controla qué canal del mixer suena (gain 0.7
// activo, gain 0 muteado) con crossfade 20ms para evitar clicks.
MoogModelD  moog;
Juno106     juno;
Prophet5    prophet;
OB6         ob6;
DX7Engine   dx7;
ARP2600     arp;

// ── Conexiones de engines → g_engine_mix_a (Sprint 38) ──────────────────────────
// Estas AudioConnections deben ser globales (no static dentro de setup()) para que
// se construyan ANTES del entry-point de Arduino y queden registradas en el grafo
// global del Teensy Audio Library antes de que el DMA del I2S arranque.
//
// Orden de canales en g_engine_mix_a:
//   ch0 = Moog    (audio_graph::ENGINE_MOOG)
//   ch1 = Juno    (audio_graph::ENGINE_JUNO)
//   ch2 = Prophet (audio_graph::ENGINE_PROPHET)
//   ch3 = OB-6    (audio_graph::ENGINE_OB6)
// Orden de canales en g_engine_mix_b: ch0=DX7, ch1=ARP2600
static AudioConnection c_moog2mix   (moog.getOutput(),    0, g_engine_mix_a, 0);
static AudioConnection c_juno2mix   (juno.getOutput(),    0, g_engine_mix_a, 1);
static AudioConnection c_prophet2mix(prophet.getOutput(), 0, g_engine_mix_a, 2);
static AudioConnection c_ob62mix    (ob6.getOutput(),     0, g_engine_mix_a, 3);
static AudioConnection c_dx72mix    (dx7.getOutput(),     0, g_engine_mix_b, 0);
static AudioConnection c_arp2mix    (arp.getOutput(),     0, g_engine_mix_b, 1);

// ── Nota sobre AudioOutputUSB ────────────────────────────────────────────────────
// ELIMINADO: AudioOutputUSB conflicta con AudioOutputI2S en Teensy 4.x.
//
// En la Teensy Audio Library, solo UN objeto puede tener update_responsibility=true
// y disparar el interrupt del DMA de I2S. Cuando AudioOutputUSB está presente junto
// a AudioOutputI2S, el runtime intenta sincronizar ambos clocks (USB 44100 Hz vs
// I2S 44117.647 Hz del PLL interno), lo que puede impedir que el DMA de I2S arranque,
// dejando AudioProcessorUsage() en 0.0%.
//
// La ruta correcta para esta etapa es siempre I2S → SGTL5000.
// USB audio (si se necesita en el futuro) requiere reemplazar AudioOutputI2S
// por AudioOutputUSB+I2S en modo sincronizado — ver Sprint 32.

// ── UI ──────────────────────────────────────────────────────────────────────────

Encoders encoders;
Buttons  buttons;
MidiHost midi_host;

// ── Bridge ──────────────────────────────────────────────────────────────────────

BridgeMaster bridge;
IntervalTimer hb_timer;

// ISR no puede llamar Serial1.write() directamente — flag para loop() context
static volatile bool g_hb_flag = false;
static void hb_isr() { g_hb_flag = true; }

// ── Estado global ───────────────────────────────────────────────────────────────

static uint8_t  g_engine_cursor  = 0;     // cursor en ENGINE_LIST
static uint8_t  g_active_engine  = 0;     // 0=Moog, 1=Juno, 2=Prophet, 3=OB-6, 4=DX7, 5=ARP2600
static uint8_t  g_synth_level    = 0;     // 0=ENGINE_LIST, 1=SUBHOME, 2=PARAM_LIST
static uint8_t  g_active_group   = 0;     // grupo activo derivado del cursor plano

// ── Tabla de navegación plana (todos los params del engine en un solo listado) ──
// Reemplaza g_group_cursor[][]. El usuario navega params con ENC NAV, sin tener
// que entrar a subgrupos. El bridge sigue enviando (group, pidx) derivados de aquí.
struct FlatParam { uint8_t group; uint8_t pidx; };
static FlatParam g_flat_params[NUM_ENGINES][16];   // máx 16 params por engine
static uint8_t   g_flat_count[NUM_ENGINES] = {};   // total de params por engine
static uint8_t   g_param_cursor[NUM_ENGINES] = {}; // posición en la lista plana

static bool     g_scale_lock_bypass    = false;
static bool     g_beat_follower_bypass = false;

// Modo del display ESP32: 0=FX, 1=SYNTH. Sketch 28 arranca en SYNTH (1).
// B2 cicla entre los dos enviando 0x00FD al ESP32 — el Teensy sigue haciendo
// audio SYNTH independientemente de lo que muestra el display.
static uint8_t  g_display_mode = 1;

// ── Estado de navegación FX (activo cuando g_display_mode == 0) ───────────────
// Estado completo portado de sketch 27 (Sprint 36 Batch E → F merge).

// Sub-vista del modo FX: SELECT = carrusel de selección, MAIN = performance.
enum class FxView : uint8_t { FX_SELECT = 0, FX_MAIN };

static FxView  g_fx_view   = FxView::FX_SELECT;
static uint8_t g_fx_cursor = 0;      // cursor en FX_SELECT (UI index 0-8)
static uint8_t g_active_fx = 0;      // UI index del FX activado en audio
static float   g_fx_wet_dry = 0.5f;  // wet/dry del FX activo [0,1]
static bool    g_fx_bypass   = false; // true = wet=0, FX en bypass

// Performance picks: índice de parámetro asignado a ENC L (sub1) y ENC R (sub2)
// por FX, indexados por UI cursor (mismo orden que fx_desc[] y view_07 tablas).
// Defaults: sub1=param0, sub2=param1 para la mayoría; excepto Modal(sub2=decay=2)
// y Cymatic(sub2=resonance=2) donde el param1 es size/density (int, menos expressive).
static uint8_t g_fx_sub1[FxManager::NUM_FX] = {0, 0, 0, 0, 0, 0, 0, 0, 0};
static uint8_t g_fx_sub2[FxManager::NUM_FX] = {1, 2, 1, 1, 1, 2, 1, 1, 1};
// Ref: [0]Ghost=delay/feedback, [1]Modal=material/decay, [2]Chorus=rate/depth,
//      [3]Bit=bits/srate, [4]Tape=drive/wow, [5]Cymatic=tune/resonance,
//      [6]Granular=grain_ms/speed, [7]Spring=algorithm/room, [8]Sub=sub_level/drive

// FxManager — única instancia, maneja new/delete del FX activo (Sprint 36 Batch E).
// Usamos new/delete para tener en heap solo el FX seleccionado; AudioEffectDelay
// (GhostEcho, ModalReverb) reserva ~134 KB de heap — no pueden coexistir todos.
static FxManager g_fx_manager;
// g_in_ai_mode: true cuando view_10 está activa.
// Se activa cuando el arm completa (0xFD=2.0 enviado), se desactiva con B1 (HOME).
static bool     g_in_ai_mode          = false;

// ML Engine + Scale Lock (Sprint 32 Batch B) + Beat-synced FX (Sprint 5.4)
static MLEngine      g_ml_engine;
static ScaleLock     g_scale_lock;
static BeatSync      g_beat_sync;      // BPM → delay/LFO auto-sync cuando AI mode activo
static AutoHarmonize g_auto_harm;      // Armonización diatónica automática (Sprint 37)
static SmartArp      g_smart_arp;      // Arpeggiador inteligente Markov + chord (Sprint 42)
static VelocityCurve g_vel_curve;      // Curva de velocity adaptativa (Sprint 6.5)

// AI arm — hold ENC NAV 4s para activar pantalla AI processing
static bool     g_ai_arming    = false;
static uint32_t g_ai_arm_start = 0;
static uint32_t g_ai_last_prog = 0;

// ── Manual scale override (Batch J — Sprint 34) ─────────────────────────────
// Activo cuando el usuario gira ENC L/R estando en AI mode + view_16 (sub=2).
// Se limpia al salir de AI mode (B1 HOME). Inicializa en AUTO.
static bool    g_scale_manual_mode    = false;
static uint8_t g_scale_manual_root    = 0;     // 0-11 (C=0 … B=11)
static uint8_t g_scale_manual_quality = 0;     // 0=major, 1=minor

// Sub-vista activa dentro de AI mode (0-4).
// 0=Camelot(view_10), 1=Models(view_25), 2=Scale(view_16), 3=Karaoke(view_26), 4=Feed(view_27)
// Se resetea a 0 al salir de AI mode (B1 HOME) para que la re-entrada arranque en view_10.
static uint8_t  g_ai_subview   = 0;

// Edge detection de NAV para detectar press/release por separado
static bool     g_nav_prev_down = false;

// Tap tempo
static uint32_t g_last_tap_ms  = 0;
static bool     g_tap_armed    = false;

// Status periódico
static uint32_t g_last_status_ms = 0;

// Nota MIDI activa para Prophet (polyphonic — debe pasarse a noteOff)
static uint8_t  g_last_midi_note = 255;

// ── LED ring update timer ───────────────────────────────────────────────────────
// El ring se actualiza cada 50ms (~20Hz) desde loop() para animar el modo AI.
// Para FX y SYNTH es suficiente actualizar en cada cambio de estado, pero
// el modo AI necesita el chromagram vivo → se re-renderiza cada 50ms.
static uint32_t g_led_last_ms = 0;
static constexpr uint32_t LED_UPDATE_MS = 50;

// ── State persistence (Sprint 35) ──────────────────────────────────────────────
// Dirty flag: se activa cuando display_mode o active_engine cambian.
// El save real ocurre en el punto de cambio (no en loop) para minimizar writes.
static DeviceState g_saved_state;   // estado guardado en EEPROM (o default)

// ── Helpers — clamp ─────────────────────────────────────────────────────────────

static float clampf(float v, float lo, float hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

// ── Apply param → engine ────────────────────────────────────────────────────────

/**
 * @brief Aplica el valor actual de un parámetro al engine correspondiente.
 *
 * Despacha la llamada al setter específico del engine usando el índice de grupo
 * y de parámetro. Los setters no existen para grupos LFO del Moog (solo display).
 *
 * @param engine  Índice de engine (0=Moog, 1=Juno, 2=Prophet, 3=OB-6)
 * @param group   Índice de grupo (0=OSC, 1=ENV, 2=FILTER, 3=LFO)
 * @param pidx    Índice de parámetro dentro del grupo
 */
static void apply_param(uint8_t engine, uint8_t group, uint8_t pidx) {
    SynthParam& p = g_desc[engine].groups[group].params[pidx];
    float v = p.value;

    switch (engine) {

    // ── Moog Model D ──────────────────────────────────────────────────────────
    case 0:
        switch (group) {
        case 0: // OSC
            switch (pidx) {
            case 0: moog.setWaveform(0, WAVEFORMS_4[(uint8_t)clampf(v, 0, 3)]); break;
            // oct: transposición guardada en el descriptor — se aplica en noteOn via
            // la frecuencia calculada. MoogModelD no tiene setOctave() explícito.
            // El offset se procesa en midi_note_on antes de llamar moog.noteOn().
            case 1: /* oct — aplicado en noteOn, no setter directo */ break;
            case 2: moog.setWaveform(1, WAVEFORMS_4[(uint8_t)clampf(v, 0, 3)]); break;
            case 3: moog.setDetune(v); break;
            case 4: moog.setWaveform(2, WAVEFORMS_4[(uint8_t)clampf(v, 0, 3)]); break;  // VCO3 waveform
            case 5: moog.setVco3Interval(v); break;                                       // VCO3 semitones
            }
            break;
        case 1: // ENV
            switch (pidx) {
            case 0: moog.setFilterAttack(v);  moog.setVCAAttack(v);  break;
            case 1: moog.setFilterDecay(v);   moog.setVCADecay(v);   break;
            case 2: moog.setFilterSustain(v); moog.setVCASustain(v); break;
            case 3: moog.setFilterRelease(v); moog.setVCARelease(v); break;
            }
            break;
        case 2: // FILTER
            switch (pidx) {
            case 0: moog.setFilterCutoff(v);    break;
            case 1: moog.setFilterResonance(v); break;
            }
            break;
        case 3: // LFO — solo almacenamiento para display; LFO real pendiente Sprint 32
            break;
        }
        break;

    // ── Juno-106 ──────────────────────────────────────────────────────────────
    // TODO: activar cuando Juno106 use I2S externo (Sprint 32)
    case 1:
        switch (group) {
        case 0: // OSC
            switch (pidx) {
            case 0: juno.setWaveform(WAVEFORMS_4[(uint8_t)clampf(v, 0, 3)]); break;
            case 1: juno.setSubLevel(v);   break;
            case 2: juno.setNoiseLevel(v); break;
            }
            break;
        case 1: // ENV
            switch (pidx) {
            case 0: juno.setAttack(v);  break;
            case 1: juno.setDecay(v);   break;
            case 2: juno.setSustain(v); break;
            case 3: juno.setRelease(v); break;
            }
            break;
        case 2: // FILTER
            switch (pidx) {
            case 0: juno.setFilterCutoff(v);    break;
            case 1: juno.setFilterResonance(v); break;
            }
            break;
        case 3: // LFO → chorus
            {
                // chorus y chorus_mode comparten el mismo setter — leer ambos
                float chorus_on_f = g_desc[1].groups[3].params[0].value;
                float chorus_mode_f = g_desc[1].groups[3].params[1].value;
                bool  enabled = chorus_on_f > 0.5f;
                uint8_t mode  = (uint8_t)clampf(chorus_mode_f, 1, 2);
                juno.setChorus(enabled, mode);
            }
            break;
        }
        break;

    // ── Prophet-5 ─────────────────────────────────────────────────────────────
    // TODO: activar cuando Prophet5 use I2S externo (Sprint 32)
    case 2:
        switch (group) {
        case 0: // OSC
            switch (pidx) {
            case 0: prophet.setOscAWaveform(WAVEFORMS_4[(uint8_t)clampf(v, 0, 3)]); break;
            case 1: prophet.setOscBWaveform(WAVEFORMS_4[(uint8_t)clampf(v, 0, 3)]); break;
            case 2: prophet.setOscBInterval(v); break;
            case 3: prophet.setCrossmod(v);     break;
            }
            break;
        case 1: // ENV
            switch (pidx) {
            case 0: prophet.setAttack(v);  break;
            case 1: prophet.setDecay(v);   break;
            case 2: prophet.setSustain(v); break;
            case 3: prophet.setRelease(v); break;
            }
            break;
        case 2: // FILTER
            switch (pidx) {
            case 0: prophet.setFilterCutoff(v);    break;
            case 1: prophet.setFilterResonance(v); break;
            }
            break;
        case 3: // LFO
            switch (pidx) {
            case 0: prophet.setOscMix(v, 1.0f - v); break;
            case 1: /* lfo_depth — solo almacenamiento */ break;
            }
            break;
        }
        break;

    // ── OB-6 ──────────────────────────────────────────────────────────────────
    case 3:
        switch (group) {
        case 0: // OSC
            switch (pidx) {
            case 0: ob6.setVco1Waveform(WAVEFORMS_4[(uint8_t)clampf(v, 0, 3)]); break;
            case 1: ob6.setVco2Waveform(WAVEFORMS_4[(uint8_t)clampf(v, 0, 3)]); break;
            case 2: ob6.setVco2Interval(v); break;
            case 3:
                // sub level: setOscMix con sub=v, vco1/vco2 en defaults, noise off
                ob6.setOscMix(0.45f, 0.45f, v, 0.0f);
                break;
            }
            break;
        case 1: // ENV
            switch (pidx) {
            case 0: ob6.setAttack(v);  break;
            case 1: ob6.setDecay(v);   break;
            case 2: ob6.setSustain(v); break;
            case 3: ob6.setRelease(v); break;
            }
            break;
        case 2: // FILTER
            switch (pidx) {
            case 0: ob6.setFilterCutoff(v);    break;
            case 1: ob6.setFilterResonance(v); break;
            case 2: ob6.setFilterEnvAmount(v); break;
            }
            break;
        case 3: // LFO — placeholder, no implementado en v1
            break;
        }
        break;

    // ── DX7 ───────────────────────────────────────────────────────────────────
    case 4:
        switch (group) {
        case 0: // FM
            switch (pidx) {
            case 0: dx7.setFmRatio(v);                                          break;
            case 1: dx7.setFmDepth(v);                                          break;
            case 2: dx7.setCarrierWaveform(WAVEFORMS_4[(uint8_t)clampf(v,0,3)]); break;
            case 3: dx7.setModWaveform(WAVEFORMS_4[(uint8_t)clampf(v,0,3)]);    break;
            }
            break;
        case 1: // ENV
            switch (pidx) {
            case 0: dx7.setAttack(v);  break;
            case 1: dx7.setDecay(v);   break;
            case 2: dx7.setSustain(v); break;
            case 3: dx7.setRelease(v); break;
            }
            break;
        case 2: // FILTER
            switch (pidx) {
            case 0: dx7.setFilterCutoff(v);    break;
            case 1: dx7.setFilterResonance(v); break;
            case 2: dx7.setFilterEnvAmount(v); break;
            }
            break;
        case 3: break;  // LFO placeholder
        }
        break;

    // ── ARP 2600 ───────────────────────────────────────────────────────────────
    case 5:
        switch (group) {
        case 0: // OSC
            switch (pidx) {
            case 0: arp.setVco1Waveform(WAVEFORMS_4[(uint8_t)clampf(v,0,3)]); break;
            case 1: arp.setVco2Ratio(v);     break;
            case 2: arp.setRingMix(v);       break;
            case 3: arp.setNoiseLevel(v);    break;
            }
            break;
        case 1: // ENV
            switch (pidx) {
            case 0: arp.setAttack(v);  break;
            case 1: arp.setDecay(v);   break;
            case 2: arp.setSustain(v); break;
            case 3: arp.setRelease(v); break;
            }
            break;
        case 2: // FILTER
            switch (pidx) {
            case 0: arp.setFilterCutoff(v);    break;
            case 1: arp.setFilterResonance(v); break;
            case 2: arp.setFilterEnvAmount(v); break;
            }
            break;
        case 3: break;  // LFO placeholder
        }
        break;
    }
}

// ── Bridge helpers ──────────────────────────────────────────────────────────────

/** @brief Envía un PARAM_CHANGED con param_id raw y valor float. */
static void bridge_send_param_raw(uint16_t param_id, float value) {
    GF_Frame f{};
    f.cmd = GF_CMD_PARAM_CHANGED;
    memcpy(&f.payload[0], &param_id, 2);
    memcpy(&f.payload[2], &value,    4);
    f.len = 6;
    f.seq = bridge.next_seq();
    bridge.send_async(f);
}

// ── Bridge — ML inference frames (Sprint 32 Batch B) ─────────────────────────

static void bridge_send_note_on(uint8_t note, uint8_t vel) {
    GF_Frame f{};
    f.cmd        = GF_CMD_NOTE_ON;
    f.len        = 3;
    f.payload[0] = note;
    f.payload[1] = vel;
    f.payload[2] = 0;    // canal 0
    f.seq        = bridge.next_seq();
    bridge.send_async(f);
}

static void bridge_send_note_off(uint8_t note) {
    GF_Frame f{};
    f.cmd        = GF_CMD_NOTE_OFF;
    f.len        = 2;
    f.payload[0] = note;
    f.payload[1] = 0;
    f.seq        = bridge.next_seq();
    bridge.send_async(f);
}

static void bridge_send_ml_key(const KeyResult& r) {
    GF_Frame f{};
    f.cmd        = GF_CMD_KEY_DETECTED;
    f.len        = 2;
    f.payload[0] = r.key_index;
    f.payload[1] = (uint8_t)(r.confidence * 100.0f + 0.5f);
    f.seq        = bridge.next_seq();
    bridge.send_async(f);
}

static void bridge_send_ml_chord(const ChordResult& r) {
    GF_Frame f{};
    f.cmd        = GF_CMD_CHORD_DETECTED;
    f.len        = 3;
    f.payload[0] = r.root_note;   // 0-11, o 255 si chord_type==5 (N)
    f.payload[1] = r.chord_type;  // 0=maj,1=min,2=7,3=m7,4=maj7,5=N
    f.payload[2] = (uint8_t)(r.confidence * 100.0f + 0.5f);
    f.seq        = bridge.next_seq();
    bridge.send_async(f);
}

static void bridge_send_ml_beat(const BeatResult& r) {
    GF_Frame f{};
    f.cmd = GF_CMD_BEAT_DETECTED;
    f.len = 2;
    uint16_t bpm_x10 = (uint16_t)(r.bpm * 10.0f + 0.5f);
    memcpy(f.payload, &bpm_x10, 2);
    f.seq = bridge.next_seq();
    bridge.send_async(f);
}

// ── GROOVE_STATE 0x83 — 4Hz en AI mode (Sprint 33 Batch 2) ──────────────────
// Transporta el estado continuo de la AI: chromagram vivo (EMA pitch_activity),
// el último snap de Scale Lock (rising-edge, un frame solo) y la fase de compás.
// Cadencia 250ms elegida por percepción: el display interpola en lvgl_anim.
// Ref: apps/docs/sprints/33-ai-hero-viz.md §Theory

static void bridge_send_groove_state(uint32_t now) {
    uint8_t payload[16];

    // Pitch activity per pitch class — EMA tau=2s, mapeado a [0,255]
    for (uint8_t i = 0; i < 12; i++) {
        payload[i] = g_ml_engine.get_pitch_activity_u8(i);
    }

    // Snap event — rising-edge: vale 1 en el único frame posterior al snap
    uint8_t from = 0, to = 0;
    bool had_snap       = g_scale_lock.consume_snap_event(&from, &to);
    payload[12] = had_snap ? 1u : 0u;
    payload[13] = from;
    payload[14] = to;

    // Bar phase [0-255]: posición dentro del compás de 4 beats al BPM detectado.
    // Default 120 BPM si el BeatFollower aún no tiene una medición válida.
    uint16_t bpm = (uint16_t)(g_ml_engine.last_beat().bpm + 0.5f);
    if (bpm < 40 || bpm > 240) bpm = 120;
    uint32_t bar_period_ms = (60000UL * 4UL) / (uint32_t)bpm;
    payload[15] = (uint8_t)((now % bar_period_ms) * 256UL / bar_period_ms);

    GF_Frame f{};
    gf_frame_build(&f, GF_CMD_GROOVE_STATE, bridge.next_seq(), payload, 16);
    bridge.send_async(f);
}

// ── AI inference — 500ms cadence (Sprint 32 Batch B) ─────────────────────────

static void handle_ai_inference(uint32_t now) {
    if (!g_in_ai_mode || !g_ml_engine.is_ready()) return;

    // GROOVE_STATE 0x83 — cadencia 250ms (4Hz) independiente de la inferencia (500ms)
    static uint32_t s_last_groove_state_ms = 0;
    if (now - s_last_groove_state_ms >= 250) {
        s_last_groove_state_ms = now;
        bridge_send_groove_state(now);
    }

    uint8_t changed = g_ml_engine.tick(now);
    if (!changed) return;

    if (changed & MLEngine::KEY_CHANGED) {
        const KeyResult& k = g_ml_engine.last_key();
        g_scale_lock.update(k, 0.75f);
        bridge_send_ml_key(k);
        // Actualizar contexto del arp: key_index 0-11 = mayor, 12-23 = menor
        uint8_t root  = (k.key_index < 12) ? k.key_index : (uint8_t)(k.key_index - 12);
        bool    minor = (k.key_index >= 12);
        g_smart_arp.set_key(root, minor);
        Serial.printf("[AI] KEY  %s  conf=%u%%\n", k.name,
                      (unsigned)(k.confidence * 100.0f));
    }
    if (changed & MLEngine::CHORD_CHANGED) {
        const ChordResult& c = g_ml_engine.last_chord();
        bridge_send_ml_chord(c);
        g_smart_arp.set_chord(c.root_note, c.chord_type);
        Serial.printf("[AI] CHORD %s  conf=%u%%\n", c.name,
                      (unsigned)(c.confidence * 100.0f));
    }
    if (changed & MLEngine::BEAT_CHANGED) {
        const BeatResult& b = g_ml_engine.last_beat();
        g_beat_sync.update(b);   // actualiza BPM suavizado para FX sync
        g_smart_arp.set_bpm(b.bpm);
        bridge_send_ml_beat(b);
        Serial.printf("[AI] BPM  %.0f  conf=%u%%\n", (double)b.bpm,
                      (unsigned)(b.confidence * 100.0f));
    }
}

/**
 * @brief Envía un parámetro de synth normalizado a [0,1] al ESP32.
 *
 * Usa el namespace 0x1xyz donde:
 *   x = engine_idx, y = group_idx, z = param_idx
 *
 * El valor se normaliza para que el display siempre opere en 0-100%.
 *
 * @param engine  Índice de engine
 * @param group   Índice de grupo
 * @param pidx    Índice de parámetro
 */
static void bridge_send_synth_param(uint8_t engine, uint8_t group, uint8_t pidx) {
    SynthParam& p = g_desc[engine].groups[group].params[pidx];
    float range = p.max_val - p.min_val;
    float norm  = (range > 0.0f) ? (p.value - p.min_val) / range : 0.0f;
    norm = clampf(norm, 0.0f, 1.0f);

    uint16_t param_id = (uint16_t)(((0x10 + engine) << 8) | (group << 4) | pidx);
    bridge_send_param_raw(param_id, norm);
}

/**
 * @brief Anuncia un cambio de engine al ESP32.
 *
 * Orden de envío:
 *   1. ENGINE_CHANGED (engine_idx + nombre)
 *   2. 0x00F5 (synth_level actual)
 *   3. Todos los parámetros de todos los grupos (normalizados)
 *   4. FILTER cutoff y resonance por separado (redundante, pero garantiza que
 *      el display los tenga aunque cache los filtre por param_id)
 *
 * @param engine  Índice del engine a anunciar
 */
static void bridge_announce_engine(uint8_t engine) {
    // ENGINE_CHANGED
    GF_Frame f{};
    f.cmd        = GF_CMD_ENGINE_CHANGED;
    f.payload[0] = 0x10 + engine;
    const char* name = g_desc[engine].name;
    size_t name_len  = strlen(name);
    if (name_len > 253) name_len = 253;
    memcpy(&f.payload[1], name, name_len + 1);
    f.len = (uint8_t)(1 + name_len + 1);
    f.seq = bridge.next_seq();
    bridge.send_async(f);

    // Nivel actual de la UI de synth
    bridge_send_param_raw(0x00F5, (float)g_synth_level);

    // Todos los params de todos los grupos
    for (uint8_t g = 0; g < 4; g++) {
        GroupDesc& gd = g_desc[engine].groups[g];
        for (uint8_t p = 0; p < gd.num_params; p++) {
            bridge_send_synth_param(engine, g, p);
        }
    }

    Serial.printf("[BRIDGE] ENGINE_ANNOUNCED engine=%u (%s)\n",
                  engine, g_desc[engine].name);
}

// ── Smart Arpeggiator callbacks ─────────────────────────────────────────────
// Routean las notas generadas por el arp al engine activo, igual que midi_note_on.
// No tienen octave offset ni scale lock — el arp ya entrega notas listas para sonar.

static void arp_on_note(uint8_t note, uint8_t vel, void* /*ctx*/) {
    float vf = vel / 127.0f;
    switch (g_active_engine) {
    case 0: moog.noteOn(note, vf);    break;
    case 1: juno.noteOn(note, vf);    break;
    case 2: prophet.noteOn(note, vf); break;
    case 3: ob6.noteOn(note, vf);     break;
    case 4: dx7.noteOn(note, vf);     break;
    case 5: arp.noteOn(note, vf);     break;  // ARP 2600 engine
    }
    bridge_send_note_on(note, vel);
}

static void arp_off_note(uint8_t note, void* /*ctx*/) {
    switch (g_active_engine) {
    case 0: moog.noteOff();           break;
    case 1: juno.noteOff(note);       break;
    case 2: prophet.noteOff(note);    break;
    case 3: ob6.noteOff(note);        break;
    case 4: dx7.noteOff(note);        break;
    case 5: arp.noteOff(note);        break;  // ARP 2600 engine
    }
    bridge_send_note_off(note);
}

// ── MIDI callbacks ──────────────────────────────────────────────────────────────

// Callbacks con linkage C para que MidiHost los registre via puntero a función.
// La state machine de audio está en el engine activo — todos los demás ignoran las
// notas incluso si reciben calls (sus VCA envelopes no están iniciados).

static void midi_note_on(uint8_t ch, uint8_t note, uint8_t vel) {
    (void)ch;
    if (vel == 0) {
        // velocity 0 en NoteOn es equivalente a NoteOff según spec MIDI 1.0
        Serial.printf("[MIDI-OFF(v0)] note=%u\n", note);
        if      (g_active_engine == 5) arp.noteOff(note);
        else if (g_active_engine == 4) dx7.noteOff(note);
        else if (g_active_engine == 3) ob6.noteOff(note);
        else if (g_active_engine == 2) prophet.noteOff(note);
        else if (g_active_engine == 1) juno.noteOff(note);
        else                           moog.noteOff();
        return;
    }
    // Velocity Curve Learn: registrar el velocity raw y aplicar la curva aprendida (Sprint 6.5).
    // record() antes de apply() para que la primera nota de la sesión ya alimente el histograma.
    g_vel_curve.record(vel);
    uint8_t vel_corrected = g_vel_curve.apply(vel);
    float velocity_norm = vel_corrected / 127.0f;

    // ML: acumular pitch class y registrar onset para beat detection.
    // Usa nota original (pre-snap, pre-transpose) para reflejar intención del músico.
    // velocity alimenta el EMA de pitch_activity para el chromagram del frame 0x83.
    g_ml_engine.update_pitch(note, vel);
    g_ml_engine.on_onset(millis());

    // Scale Lock: cuantizar nota si está activo (solo en AI mode y sin bypass)
    uint8_t play_note = note;
    if (g_in_ai_mode && !g_scale_lock_bypass) {
        play_note = g_scale_lock.snap(note);
    }

    // Octave offset del Moog OSC param 1 (oct): transportar la nota MIDI
    // sumando el offset de octava del descriptor antes de pasarla al engine.
    // Solo aplica al engine activo.
    int oct_offset = 0;
    if (g_active_engine == 0) {
        oct_offset = (int)g_desc[0].groups[0].params[1].value; // param "oct"
    }
    int transposed = (int)play_note + oct_offset * 12;
    if (transposed < 0)   transposed = 0;
    if (transposed > 127) transposed = 127;
    uint8_t tnote = (uint8_t)transposed;

    Serial.printf("[MIDI-ON] note=%u vel=%u → play=%u transposed=%u engine=%s cpu=%.1f%%\n",
                  note, vel, play_note, tnote, g_desc[g_active_engine].name,
                  AudioProcessorUsage());

    g_last_midi_note = note; // guardar nota original para el noteOff del Prophet

    // Smart Arp intercept: si arp activo en AI mode, colectar nota en lugar de tocar directo.
    // El arp se encarga de emitir las notas al engine con su propio timing.
    if (g_smart_arp.enabled && g_in_ai_mode) {
        g_smart_arp.note_on(play_note);
        // Notificar Bridge igualmente (para animar osciloscopio en view_02)
        bridge_send_note_on(play_note, vel);
        return;
    }

    switch (g_active_engine) {
    case 0: moog.noteOn(tnote, velocity_norm);    break;
    case 1: juno.noteOn(tnote, velocity_norm);    break;
    case 2: prophet.noteOn(tnote, velocity_norm); break;
    case 3: ob6.noteOn(tnote, velocity_norm);     break;
    case 4: dx7.noteOn(tnote, velocity_norm);     break;
    case 5: arp.noteOn(tnote, velocity_norm);     break;
    }

    // Auto-Harmonization (Sprint 37 — Fase 6 Layer 1 AI):
    // Activo solo en AI mode con escala detectada y engine polifónico.
    if (g_auto_harm.enabled && g_in_ai_mode &&
        g_scale_lock.has_scale() &&
        (g_active_engine == 1 || g_active_engine == 2 || g_active_engine == 3 ||
         g_active_engine == 4 || g_active_engine == 5)) {
        uint8_t h = g_auto_harm.get_harmony_note(
            tnote,
            g_scale_lock.get_key_root(),
            g_scale_lock.get_is_minor(),
            g_auto_harm.interval);
        if (h != 0xFF) {
            if      (g_active_engine == 1) juno.noteOn(h, velocity_norm);
            else if (g_active_engine == 2) prophet.noteOn(h, velocity_norm);
            else if (g_active_engine == 3) ob6.noteOn(h, velocity_norm);
            else if (g_active_engine == 4) dx7.noteOn(h, velocity_norm);
            else                           arp.noteOn(h, velocity_norm);
            g_auto_harm.store_harmony(tnote, h);
            Serial.printf("[HARM] %u → %u (root=%u %s)\n",
                          tnote, h,
                          g_scale_lock.get_key_root(),
                          g_scale_lock.get_is_minor() ? "min" : "maj");
        }
    }

    // Bridge: notificar nota al ESP32 para animar osciloscopio en view_02
    bridge_send_note_on(play_note, vel);
}

static void midi_note_off(uint8_t ch, uint8_t note, uint8_t vel) {
    (void)ch; (void)vel;
    Serial.printf("[MIDI-OFF] note=%u\n", note);

    // Smart Arp intercept: liberar la nota del arp note set (nota original, no transpuesta)
    if (g_smart_arp.enabled && g_in_ai_mode) {
        g_smart_arp.note_off(note);
        bridge_send_note_off(note);
        return;
    }

    // Aplicar el mismo octave offset que en noteOn para calcular tnote correcto
    int oct_offset = (g_active_engine == 0)
        ? (int)g_desc[0].groups[0].params[1].value
        : 0;
    int transposed = (int)note + oct_offset * 12;
    if (transposed < 0)   transposed = 0;
    if (transposed > 127) transposed = 127;
    uint8_t tnote = (uint8_t)transposed;

    switch (g_active_engine) {
    case 0: moog.noteOff();          break;
    case 1: juno.noteOff(tnote);     break;
    case 2: prophet.noteOff(tnote);  break;
    case 3: ob6.noteOff(tnote);      break;
    case 4: dx7.noteOff(tnote);      break;
    case 5: arp.noteOff(tnote);      break;
    }

    // Liberar la voz harmónica si estaba activa para esta nota
    if (g_auto_harm.enabled &&
        (g_active_engine == 1 || g_active_engine == 2 || g_active_engine == 3 ||
         g_active_engine == 4 || g_active_engine == 5)) {
        uint8_t h = g_auto_harm.pop_harmony(tnote);
        if (h != 0xFF) {
            if      (g_active_engine == 1) juno.noteOff(h);
            else if (g_active_engine == 2) prophet.noteOff(h);
            else if (g_active_engine == 3) ob6.noteOff(h);
            else if (g_active_engine == 4) dx7.noteOff(h);
            else                           arp.noteOff(h);
        }
    }

    bridge_send_note_off(note);
}

static void midi_cc(uint8_t ch, uint8_t cc, uint8_t val) {
    (void)ch;
    float norm = val / 127.0f;

    if (cc == 74) {
        // Brightness / Cutoff → FILTER group, param 0
        float& cutoff = g_desc[g_active_engine].groups[2].params[0].value;
        float  lo     = g_desc[g_active_engine].groups[2].params[0].min_val;
        float  hi     = g_desc[g_active_engine].groups[2].params[0].max_val;
        cutoff = lo + norm * (hi - lo);
        apply_param(g_active_engine, 2, 0);
        bridge_send_synth_param(g_active_engine, 2, 0);
        Serial.printf("[CC74] cutoff=%.0f\n", cutoff);
    } else if (cc == 71) {
        // Resonance → FILTER group, param 1
        float& res = g_desc[g_active_engine].groups[2].params[1].value;
        float  lo  = g_desc[g_active_engine].groups[2].params[1].min_val;
        float  hi  = g_desc[g_active_engine].groups[2].params[1].max_val;
        res = lo + norm * (hi - lo);
        apply_param(g_active_engine, 2, 1);
        bridge_send_synth_param(g_active_engine, 2, 1);
        Serial.printf("[CC71] resonance=%.3f\n", res);
    }
}

// ── Panic — silenciar todos los engines ─────────────────────────────────────────

static void all_engines_panic() {
    g_smart_arp.enabled = false;  // silenciar arp antes de los engines
    moog.noteOff();
    // Engines polifónicos: silenciar todas las notas posibles (voice stealing interno).
    // No hay método allNotesOff() — noteOff con nota que no está asignada es no-op.
    for (uint8_t n = 0; n <= 127; n++) {
        juno.noteOff(n);
        prophet.noteOff(n);
        ob6.noteOff(n);
        dx7.noteOff(n);
        arp.noteOff(n);
    }
}

// ── Activar engine ──────────────────────────────────────────────────────────────

/**
 * @brief Silencia todos los engines, conmuta el audio graph, y anuncia al ESP32.
 *
 * Sprint 36: audio_graph::set_active_engine() ejecuta crossfade 20ms entre el
 * engine previo y el nuevo (los 3 corren simultáneamente, mixer routing decide
 * qué se escucha). El all_engines_panic() previo libera notas colgadas para
 * que el crossfade no contenga audio espurio del engine saliente.
 *
 * @param engine  Índice del engine a activar (audio_graph::ENGINE_*)
 */
static void activate_engine(uint8_t engine) {
    // Silenciar todos antes de cambiar para evitar notas colgadas durante el fade
    all_engines_panic();

    g_active_engine = engine;
    g_active_group  = g_flat_params[engine][g_param_cursor[engine]].group;

    // Conmutar el audio graph (crossfade 20ms entre engines)
    audio_graph::set_active_engine(engine);

    Serial.printf("[ENGINE] activado: %s\n", g_desc[engine].name);
    bridge_announce_engine(engine);
}

// ── Tabla de navegación plana ───────────────────────────────────────────────────

/**
 * @brief Construye la tabla plana de params para cada engine a partir de g_desc.
 *
 * Itera grupos → params y los lineariza. Así el usuario navega con un solo encoder
 * sin tener que seleccionar subgrupos. El bridge sigue recibiendo (group, pidx)
 * derivados de g_flat_params para que el display ESP32 muestre la vista correcta.
 *
 * Llamar una vez en setup(), después de que g_desc[] está listo.
 */
static void build_flat_params() {
    for (uint8_t e = 0; e < NUM_ENGINES; e++) {
        uint8_t idx = 0;
        for (uint8_t g = 0; g < 4; g++) {
            for (uint8_t p = 0; p < g_desc[e].groups[g].num_params; p++) {
                if (idx < 16) {
                    g_flat_params[e][idx++] = {g, p};
                }
            }
        }
        g_flat_count[e] = idx;
    }
}

/** @brief Convierte cursor plano → (group, pidx) del engine activo. */
static void flat_to_gp(uint8_t engine, uint8_t flat_idx, uint8_t& group, uint8_t& pidx) {
    group = g_flat_params[engine][flat_idx].group;
    pidx  = g_flat_params[engine][flat_idx].pidx;
}

// ── FX apply helpers ────────────────────────────────────────────────────────────

/**
 * @brief Aplica el valor actual de un parámetro al FX activo en el audio graph.
 *
 * Guarda: para castear el void* de FxManager al tipo concreto correcto,
 * verificamos primero que el FX activo en FxManager coincide con lo que
 * FX_MANAGER_ID[fx_idx] indica. El switch usa el UI index (fx_desc[] ordering),
 * igual que sketch 27, para consistencia con el resto del display pipeline.
 *
 * @param fx_idx    UI cursor index (0-8, sketch 27 ordering)
 * @param param_idx Índice de parámetro en fx_desc[fx_idx].params[] (0-3)
 */
static void apply_fx_param(uint8_t fx_idx, uint8_t param_idx) {
    if (!g_fx_manager.is_active()) return;
    if (g_fx_manager.active_fx() != FX_MANAGER_ID[fx_idx]) return;
    void* inst = g_fx_manager.get_instance();
    if (!inst) return;
    const float v = fx_desc[fx_idx].params[param_idx].value;

    switch (fx_idx) {
        case 0: { // Ghost Echo
            GhostEcho* o = static_cast<GhostEcho*>(inst);
            switch (param_idx) {
                case 0: o->setDelayMs(v);  break;
                case 1: o->setFeedback(v); break;
                case 2: o->setTapeLP(v);   break;
                case 3: o->setMix(v);      break;
            }
            break;
        }
        case 1: { // Modal Reverb
            ModalReverb* o = static_cast<ModalReverb*>(inst);
            switch (param_idx) {
                case 0: o->setMaterial(static_cast<ModalReverb::Material>((uint8_t)v)); break;
                case 1: o->setSize(static_cast<ModalReverb::Size>((uint8_t)v));         break;
                case 2: o->setDecay(v);  break;
                case 3: o->setMix(v);    break;
            }
            break;
        }
        case 2: { // Phase Chorus
            PhaseChorus* o = static_cast<PhaseChorus*>(inst);
            switch (param_idx) {
                case 0: o->setRate(v);            break;
                case 1: o->setDepth(v);           break;
                case 2: o->setDrift(v);           break;
                case 3: o->setVoices((uint8_t)v); break;
            }
            break;
        }
        case 3: { // Bit Sculpt
            BitSculpt* o = static_cast<BitSculpt*>(inst);
            switch (param_idx) {
                case 0: o->setBits((uint8_t)v); break;
                case 1: o->setSampleRate(v);    break;
                case 2: o->setSculpt(v);        break;
                case 3: o->setMix(v);           break;
            }
            break;
        }
        case 4: { // Tape Saturate
            TapeSaturate* o = static_cast<TapeSaturate*>(inst);
            switch (param_idx) {
                case 0: o->setDrive(v);   break;
                case 1: o->setWow(v);     break;
                case 2: o->setFlutter(v); break;
                case 3: o->setAge(v);     break;
            }
            break;
        }
        case 5: { // Cymatic Resonator
            CymaticResonator* o = static_cast<CymaticResonator*>(inst);
            switch (param_idx) {
                case 0: o->setTune(v);              break;
                case 1: o->setDensity((uint8_t)v);  break;
                case 2: o->setResonance(v);         break;
                case 3: o->setLfoRate(v);           break;
            }
            break;
        }
        case 6: { // Granular Cloud
            GranularCloud* o = static_cast<GranularCloud*>(inst);
            switch (param_idx) {
                case 0: o->setGrainSize(v);          break;
                case 1: o->setSpeed(v);              break;
                case 2: o->setFreeze((bool)(int)v);  break;
                case 3: o->setMix(v);                break;
            }
            break;
        }
        case 7: { // Spring Plate
            SpringPlate* o = static_cast<SpringPlate*>(inst);
            switch (param_idx) {
                case 0: o->setAlgorithm((uint8_t)v); break;
                case 1: o->setRoomSize(v);            break;
                case 2: o->setDamping(v);             break;
                case 3: o->setMix(v);                 break;
            }
            break;
        }
        case 8: { // Sub Genesis
            SubGenesis* o = static_cast<SubGenesis*>(inst);
            switch (param_idx) {
                case 0: o->setSubLevel(v); break;
                case 1: o->setDrive(v);    break;
                case 2: o->setCutoff(v);   break;
                case 3: o->setMix(v);      break;
            }
            break;
        }
    }
}

/**
 * @brief Envía un parámetro FX normalizado a [0,1] al ESP32.
 *
 * param_id encoding: (fx_ui_idx << 8) | param_idx — mismo que sketch 27 y
 * bridge_get_param_cached() en el ESP32.
 *
 * @param fx_idx    UI cursor index (0-8)
 * @param param_idx Índice del parámetro (0-3)
 */
static void bridge_send_fx_param(uint8_t fx_idx, uint8_t param_idx) {
    FxParam& p     = fx_desc[fx_idx].params[param_idx];
    float range    = p.max_val - p.min_val;
    float norm     = (range > 0.0f) ? (p.value - p.min_val) / range : 0.0f;
    if (norm < 0.0f) norm = 0.0f;
    if (norm > 1.0f) norm = 1.0f;
    uint16_t param_id = (uint16_t)(((uint16_t)fx_idx << 8) | param_idx);
    bridge_send_param_raw(param_id, norm);
}

/**
 * @brief Anuncia el FX activo al ESP32: ENGINE_CHANGED + sub sync + todos los params.
 *
 * Sin este anuncio al activar un FX, el display ESP32 muestra 0% en todos los
 * arcos porque la caché arranca vacía cada vez que cambia el FX.
 *
 * Orden de envío:
 *   1) ENGINE_CHANGED  — establece fx_id + nombre en ESP32 (view_07 lo usa para el label)
 *   2) 0x00F7          — sincroniza assignments sub1/sub2
 *   3) PARAM_CHANGED × 4 — rellena caché con valores actuales normalizados
 *
 * @param fx_idx  UI cursor index (0-8, sketch 27 ordering)
 */
static void bridge_announce_fx(uint8_t fx_idx) {
    // ENGINE_CHANGED — payload: [fx_idx, name bytes, null]
    GF_Frame f{};
    f.cmd        = GF_CMD_ENGINE_CHANGED;
    f.payload[0] = fx_idx;
    const char* name  = fx_desc[fx_idx].name;
    size_t      nlen  = strlen(name);
    if (nlen > 253) nlen = 253;
    memcpy(&f.payload[1], name, nlen + 1);
    f.len = (uint8_t)(1 + nlen + 1);
    f.seq = bridge.next_seq();
    bridge.send_async(f);

    // 0x00F7 — sync sub1/sub2 (encoding: fx_idx*100 + sub1*10 + sub2)
    float sync_val = (float)(fx_idx * 100 + g_fx_sub1[fx_idx] * 10 + g_fx_sub2[fx_idx]);
    bridge_send_param_raw(0x00F7, sync_val);

    // Todos los params actuales (normalizados para los arcos del ESP32)
    for (uint8_t pi = 0; pi < fx_desc[fx_idx].num_params; pi++) {
        bridge_send_fx_param(fx_idx, pi);
    }

    Serial.printf("[BRIDGE] FX_ANNOUNCED ui_idx=%u (%s)\n",
                  fx_idx, fx_desc[fx_idx].name);
}

// ── Forward declarations ─────────────────────────────────────────────────────────
static void save_state_if_changed();

// ── Bridge — handle encoders ────────────────────────────────────────────────────

static void handle_encoders() {
    // Cursor de fila en la AI Rack — compartido entre el bloque ENC NAV y ENC L/R.
    // Static local para que persista entre llamadas a handle_encoders().
    static uint8_t s_rack_cursor = 0;

    int32_t delta_nav = encoders.read_enc_nav();
    int32_t delta_l   = encoders.read_enc_l();
    int32_t delta_r   = encoders.read_enc_r();
    bool    l_push    = encoders.sw_l_pressed();
    bool    r_push    = encoders.sw_r_pressed();

    bool nav_down     = encoders.sw_nav_is_down();
    bool nav_push     = nav_down && !g_nav_prev_down;   // rising edge
    bool nav_release  = !nav_down && g_nav_prev_down;   // falling edge
    g_nav_prev_down   = nav_down;

    uint32_t now = millis();

    // ── COMBO SECRETO: HOME held (>500ms) + NAV push → USB audio passthrough ───
    // Activa/desactiva que el audio del Mac salga por los headphones de la Teensy.
    // No hay indicación visual en el display — solo Serial y un flash tenue del LED ring.
    // 2s de cooldown para evitar doble-toggle accidental.
    if (nav_push && buttons.held(0)) {
        static uint32_t s_usb_toggle_ms = 0;
        if (now - s_usb_toggle_ms > 2000) {
            s_usb_toggle_ms = now;
            bool en = !audio_graph::get_usb_passthrough();
            audio_graph::set_usb_passthrough(en);
            led_ring.tap_tempo_flash();   // flash sutil — mismo efecto que TAP TEMPO
            Serial.printf("[~] USB audio %s\n", en ? "ON" : "OFF");
        }
        return;  // consumir NAV push — no ejecutar navegación normal
    }

    // ── Bypass de modo AI (prioridad máxima — consume el push antes de la navegación normal) ──
    if (l_push && g_in_ai_mode) {
        g_scale_lock_bypass = !g_scale_lock_bypass;
        g_scale_lock.set_bypass(g_scale_lock_bypass);
        bridge_send_param_raw(0x00F2, g_scale_lock_bypass ? 1.0f : 0.0f);
        Serial.printf("[AI] Scale Lock bypass → %s\n",
                      g_scale_lock_bypass ? "OFF" : "ON");
        return;
    }
    if (r_push && g_in_ai_mode) {
        g_beat_follower_bypass = !g_beat_follower_bypass;
        bridge_send_param_raw(0x00F3, g_beat_follower_bypass ? 1.0f : 0.0f);
        Serial.printf("[AI] Beat Follower bypass → %s\n",
                      g_beat_follower_bypass ? "OFF" : "ON");
        return;
    }

    // ── ENC NAV rotation en AI mode: cicla 3 sub-vistas (Sprint 45) ─────────
    // Orden: Camelot(0) → AI Rack(1) → Scale Lock(2) → wrap
    // view_25/26/27 quedan fuera del ciclo; solo 3 vistas.
    // Solo activo si g_in_ai_mode y NO estamos armando (hold en curso).
    // Consume el delta_nav y retorna para no caer al handler normal de SYNTH.
    //
    // NOTA: el bloque de AI Rack (g_ai_subview == 1) se procesa antes de este
    // bloque en el flujo — si sub=1 con ENC NAV debe mover el cursor de fila,
    // no cambiar de sub-vista. Ver el bloque AI RACK más abajo.
    if (g_in_ai_mode && delta_nav != 0 && !g_ai_arming) {
        // AI Rack (sub=1): ENC NAV mueve cursor de fila, no cambia sub-vista
        if (g_ai_subview == 1) {
            int c = (int)s_rack_cursor + (delta_nav > 0 ? 1 : -1);
            if (c < 0) c = 5;
            if (c > 5) c = 0;
            s_rack_cursor = (uint8_t)c;
            bridge_send_param_raw(0x00E9, (float)s_rack_cursor);
            Serial.printf("[AI RACK] cursor -> %u\n", (unsigned)s_rack_cursor);
            return;
        }

        int new_sub = (int)g_ai_subview + (delta_nav > 0 ? 1 : -1);
        if (new_sub < 0) new_sub = 2;
        if (new_sub > 2) new_sub = 0;
        g_ai_subview = (uint8_t)new_sub;
        bridge_send_param_raw(0x00F1, (float)g_ai_subview);
        Serial.printf("[AI] subview -> %u\n", (unsigned)g_ai_subview);
        return;  // consumir evento, no caer al handler normal
    }

    // ── AI RACK (sub=1): ENC L/R controlan el feature de la fila activa ─────
    // ENC NAV ya fue consumido arriba (mueve cursor entre filas).
    // ENC L/R ajustan el feature según la fila (s_rack_cursor).
    // Los cambios se aplican al audio Teensy Y se envían al ESP32 para actualizar
    // el display. El cursor compartido entre NAV y este bloque es s_rack_cursor.
    if (g_in_ai_mode && g_ai_subview == 1 && !g_ai_arming) {
        // ENC NAV ya fue consumido arriba; solo procesar ENC L/R aquí
        if (delta_l != 0 || delta_r != 0) {
            switch (s_rack_cursor) {

            case 0:  // SCALE LOCK — ENC L toggle
                if (delta_l != 0) {
                    g_scale_lock_bypass = !g_scale_lock_bypass;
                    g_scale_lock.set_bypass(g_scale_lock_bypass);
                    bridge_send_param_raw(0x00F2, g_scale_lock_bypass ? 1.0f : 0.0f);
                    Serial.printf("[RACK] Scale Lock -> %s\n",
                                  g_scale_lock_bypass ? "OFF" : "ON");
                }
                break;

            case 1:  // HARMONIZE — ENC L on/off, ENC R cicla intervalo
                if (delta_l != 0) {
                    g_auto_harm.enabled = !g_auto_harm.enabled;
                    if (!g_auto_harm.enabled) all_engines_panic();  // liberar voces huérfanas
                    bridge_send_param_raw(0x00EA, g_auto_harm.enabled ? 1.0f : 0.0f);
                    Serial.printf("[RACK] AutoHarm -> %s\n",
                                  g_auto_harm.enabled ? "ON" : "OFF");
                }
                if (delta_r != 0) {
                    // Cicla THIRD ↔ SIXTH (solo 2 valores del enum HarmonyInterval)
                    HarmonyInterval cur_iv = g_auto_harm.interval;
                    HarmonyInterval new_iv = (cur_iv == HarmonyInterval::THIRD)
                                            ? HarmonyInterval::SIXTH
                                            : HarmonyInterval::THIRD;
                    g_auto_harm.interval = new_iv;
                    uint8_t iv_idx = (new_iv == HarmonyInterval::SIXTH) ? 1u : 0u;
                    bridge_send_param_raw(0x00EB, (float)iv_idx);
                    Serial.printf("[RACK] AutoHarm interval -> %s\n",
                                  (new_iv == HarmonyInterval::SIXTH) ? "SIXTH" : "THIRD");
                }
                break;

            case 2:  // ARP — ENC L cicla modo, ENC R cicla división
                {
                    static uint8_t s_arp_mode_idx = 4;  // SMART default
                    static uint8_t s_arp_div_idx  = 1;  // EIGHTH default
                    if (delta_l != 0) {
                        int nm = (int)s_arp_mode_idx + (delta_l > 0 ? 1 : -1);
                        if (nm < 0) { nm = 4; } else if (nm > 4) { nm = 0; }
                        s_arp_mode_idx = (uint8_t)nm;
                        g_smart_arp.set_mode((SmartArp::Mode)s_arp_mode_idx);
                        bridge_send_param_raw(0x00EC, (float)s_arp_mode_idx);
                        Serial.printf("[RACK] Arp mode -> %u\n", (unsigned)s_arp_mode_idx);
                    }
                    if (delta_r != 0) {
                        int nd = (int)s_arp_div_idx + (delta_r > 0 ? 1 : -1);
                        if (nd < 0) { nd = 2; } else if (nd > 2) { nd = 0; }
                        s_arp_div_idx = (uint8_t)nd;
                        g_smart_arp.set_division((SmartArp::Division)s_arp_div_idx);
                        bridge_send_param_raw(0x00ED, (float)s_arp_div_idx);
                        Serial.printf("[RACK] Arp div -> %u\n", (unsigned)s_arp_div_idx);
                    }
                }
                break;

            case 3:  // GROOVE — ENC L cicla estilo, ENC R ajusta amount
                {
                    static uint8_t s_groove_style  = 1;    // HUMAN default
                    static float   s_groove_amount = 0.5f;
                    bool changed = false;
                    if (delta_l != 0) {
                        int ns = (int)s_groove_style + (delta_l > 0 ? 1 : -1);
                        if (ns < 0) { ns = 2; } else if (ns > 2) { ns = 0; }
                        s_groove_style = (uint8_t)ns;
                        changed = true;
                    }
                    if (delta_r != 0) {
                        s_groove_amount += delta_r * 0.05f;
                        if (s_groove_amount < 0.0f)  s_groove_amount = 0.0f;
                        if (s_groove_amount > 0.99f) s_groove_amount = 0.99f;
                        changed = true;
                    }
                    if (changed) {
                        g_smart_arp.set_groove((GrooveHumanizer::Style)s_groove_style,
                                               s_groove_amount);
                        // Packed: parte entera=style, parte fraccional=amount
                        bridge_send_param_raw(0x00EE, (float)s_groove_style + s_groove_amount);
                        Serial.printf("[RACK] Groove style=%u amount=%.2f\n",
                                      (unsigned)s_groove_style, (double)s_groove_amount);
                    }
                }
                break;

            case 4:  // BEAT FX — ENC L toggle
                if (delta_l != 0) {
                    g_beat_follower_bypass = !g_beat_follower_bypass;
                    bridge_send_param_raw(0x00F3, g_beat_follower_bypass ? 1.0f : 0.0f);
                    Serial.printf("[RACK] Beat FX -> %s\n",
                                  g_beat_follower_bypass ? "OFF" : "ON");
                }
                break;

            case 5:  // VELOCITY — auto-calibra, sin control de usuario
            default:
                break;
            }
            return;
        }
    }

    // ── view_16 SCALE LOCK: ENC L/R ajustan override manual de escala ────────
    // Condición: AI mode activo + sub-vista 2 (view_16) + no en arm hold.
    // Cualquiera de los dos giros entra en MANUAL mode; B1 HOME resetea a AUTO.
    // Param 0x00EF usado en lugar de 0x00F6 (ya ocupado para SYNTH group nav).
    if (g_in_ai_mode && g_ai_subview == 2 && !g_ai_arming) {
        if (delta_l != 0) {
            int new_root = (int)g_scale_manual_root + (delta_l > 0 ? 1 : -1);
            if (new_root < 0)  new_root = 11;
            if (new_root > 11) new_root = 0;
            g_scale_manual_root = (uint8_t)new_root;
            g_scale_manual_mode = true;
            uint8_t key_idx = g_scale_manual_root + (g_scale_manual_quality ? 12 : 0);
            g_scale_lock.set_manual_key(key_idx);
            bridge_send_param_raw(0x00EF, (float)key_idx);
            static const char* const NOTE_NAMES[12] = {
                "C","C#","D","D#","E","F","F#","G","G#","A","A#","B"
            };
            Serial.printf("[SCALE] manual root -> key_idx=%u (%s%s)\n",
                          key_idx, NOTE_NAMES[g_scale_manual_root],
                          g_scale_manual_quality ? "m" : "");
            return;
        }
        if (delta_r != 0) {
            g_scale_manual_quality = g_scale_manual_quality ? 0u : 1u;
            g_scale_manual_mode = true;
            uint8_t key_idx = g_scale_manual_root + (g_scale_manual_quality ? 12 : 0);
            g_scale_lock.set_manual_key(key_idx);
            bridge_send_param_raw(0x00EF, (float)key_idx);
            Serial.printf("[SCALE] manual quality -> %s\n",
                          g_scale_manual_quality ? "minor" : "major");
            return;
        }
    }

    // Cancelar AI arm si cualquier encoder se mueve
    if ((delta_nav != 0 || delta_l != 0 || delta_r != 0) && g_ai_arming) {
        g_ai_arming = false;
        bridge_send_param_raw(0x00FB, -1.0f);
        Serial.println("[AI] arm cancelado (encoder)");
    }

    // ── FX MODE: state machine FX_SELECT ↔ FX_MAIN ──────────────────────────────
    if (g_display_mode == 0 && !g_in_ai_mode) {

        if (g_fx_view == FxView::FX_SELECT) {
            // ────────────────────────────────────────────────────────────────
            // FX_SELECT: ENC NAV cicla cursor, push activa FX y va a FX_MAIN
            // ────────────────────────────────────────────────────────────────

            if (delta_nav != 0) {
                int32_t next = ((int32_t)g_fx_cursor + delta_nav + FxManager::NUM_FX)
                               % FxManager::NUM_FX;
                g_fx_cursor = (uint8_t)next;
                bridge_send_param_raw(0x00FE, (float)g_fx_cursor);
                Serial.printf("[FX_SELECT] cursor=%u (%s)\n",
                              g_fx_cursor, fx_desc[g_fx_cursor].name);
            }

            if (nav_push) {
                // Activar FX en el audio graph
                g_active_fx  = g_fx_cursor;
                g_fx_bypass  = false;
                // FX Processor mode: conectar FX a g_audio_in (line-in SGTL5000),
                // no a g_engine_mix. FX_MANAGER_ID traduce UI index → FxManager audio ID.
                g_fx_manager.activate(FX_MANAGER_ID[g_active_fx], g_audio_in);
                // Arrancar con el wet predeterminado listo (performance-ready).
                // set_wet() llama setMix() interno — g_master_mix.gain(1) queda fijo en 1.0.
                g_fx_manager.set_wet(g_fx_wet_dry);

                // Sincronizar el objeto FX con la tabla de parámetros de la UI.
                // apply_fx_param() necesita que el FX ya esté activo (verificado arriba).
                // Sin esto, el FX arranca con sus defaults internos y la UI puede mostrar
                // valores distintos a los que el FX realmente usa.
                for (uint8_t pi = 0; pi < fx_desc[g_active_fx].num_params; pi++) {
                    apply_fx_param(g_active_fx, pi);
                }

                // Anunciar al ESP32: ENGINE_CHANGED + sub sync + params
                bridge_announce_fx(g_active_fx);
                bridge_send_param_raw(0x00FF, g_fx_wet_dry);  // wet predeterminado
                bridge_send_param_raw(0x00E8, 0.0f);          // bypass=OFF al activar
                bridge_send_param_raw(0x00FC, 255.0f);        // → VIEW_IDX_FX_MAIN

                led_ring.update_fx_wet(g_fx_wet_dry, false);  // ring muestra nivel inicial

                g_fx_view = FxView::FX_MAIN;
                Serial.printf("[FX] activate ui_idx=%u (%s) wet=%.2f mem=%u/%u → FX_MAIN\n",
                              g_active_fx, fx_desc[g_active_fx].name, (double)g_fx_wet_dry,
                              AudioMemoryUsage(), AudioMemoryUsageMax());
            }

        } else { // FxView::FX_MAIN
            // ────────────────────────────────────────────────────────────────
            // FX_MAIN — bypass toggle:
            //   ENC NAV gira  → wet/dry live (si no bypassed) o solo dial (si bypassed).
            //                   Permite presetear el nivel antes de un-bypassear.
            //   ENC NAV push  → toggle bypass:
            //                   OFF→ON: wet=0, ch2=1 (monitor directo)
            //                   ON→OFF: wet=g_fx_wet_dry (restaura dial predeterminado)
            //   ENC L/R gira  → sub-params asignados.
            //   ENC L/R push  → rota qué param controla cada encoder.
            // ────────────────────────────────────────────────────────────────

            // ENC NAV gira → siempre acumula g_fx_wet_dry.
            // Si bypass=OFF aplica al audio live; si bypass=ON solo mueve el dial
            // para presetear el nivel antes del próximo un-bypass.
            if (delta_nav != 0) {
                g_fx_wet_dry += delta_nav * 0.02f;
                if (g_fx_wet_dry < 0.0f) g_fx_wet_dry = 0.0f;
                if (g_fx_wet_dry > 1.0f) g_fx_wet_dry = 1.0f;
                bridge_send_param_raw(0x00FF, g_fx_wet_dry);
                if (!g_fx_bypass) {
                    // set_wet() → setMix() del FX activo (crossfade, no volumen)
                    g_fx_manager.set_wet(g_fx_wet_dry);
                }
                led_ring.update_fx_wet(g_fx_wet_dry, g_fx_bypass);
                Serial.printf("[FX_MAIN] wet=%.2f%s\n",
                              (double)g_fx_wet_dry, g_fx_bypass ? " (bypass)" : "");
            }

            // ENC NAV push → toggle bypass
            if (nav_push) {
                g_fx_bypass = !g_fx_bypass;
                if (g_fx_bypass) {
                    // Bypass ON: resetear dial a 0 + silenciar FX + monitor directo.
                    // El reset permite presetear desde 0 con el encoder mientras bypassed.
                    g_fx_wet_dry = 0.0f;
                    g_master_mix.gain(1, 0.0f);
                    g_master_mix.gain(2, 1.0f);
                    bridge_send_param_raw(0x00FF, 0.0f);
                    bridge_send_param_raw(0x00E8, 1.0f);  // bypass ON → fondo arco invisible
                } else {
                    // Bypass OFF: restaurar canal master + aplicar wet interno
                    g_master_mix.gain(1, 1.0f);
                    g_master_mix.gain(2, 0.0f);
                    g_fx_manager.set_wet(g_fx_wet_dry);
                    bridge_send_param_raw(0x00FF, g_fx_wet_dry);
                    bridge_send_param_raw(0x00E8, 0.0f);  // bypass OFF → fondo arco visible
                }
                led_ring.update_fx_wet(g_fx_wet_dry, g_fx_bypass);
                Serial.printf("[FX] bypass %s wet=%.2f\n",
                              g_fx_bypass ? "ON" : "OFF", (double)g_fx_wet_dry);
            }

            // ENC L gira → sub-param 1
            if (delta_l != 0) {
                uint8_t pidx = g_fx_sub1[g_active_fx];
                FxParam& p   = fx_desc[g_active_fx].params[pidx];
                p.value += delta_l * p.step;
                if (p.value < p.min_val) p.value = p.min_val;
                if (p.value > p.max_val) p.value = p.max_val;
                apply_fx_param(g_active_fx, pidx);
                bridge_send_fx_param(g_active_fx, pidx);
                if (p.is_int)
                    Serial.printf("[SUB1] %s: %d\n", p.name, (int)p.value);
                else
                    Serial.printf("[SUB1] %s: %.3f\n", p.name, (double)p.value);
            }

            // ENC L push → rotar qué parámetro controla sub1 (skip el que ya tiene sub2)
            if (l_push) {
                uint8_t num = fx_desc[g_active_fx].num_params;
                uint8_t s1  = g_fx_sub1[g_active_fx];
                uint8_t s2  = g_fx_sub2[g_active_fx];
                for (uint8_t i = 0; i < num; i++) {
                    s1 = (s1 + 1) % num;
                    if (s1 != s2) break;
                }
                g_fx_sub1[g_active_fx] = s1;
                float sync = (float)(g_active_fx * 100 + s1 * 10 + g_fx_sub2[g_active_fx]);
                bridge_send_param_raw(0x00F7, sync);
                bridge_send_fx_param(g_active_fx, s1);
                Serial.printf("[SUB1 ROT] FX%u sub1→%u (%s)\n",
                              g_active_fx, s1, fx_desc[g_active_fx].params[s1].name);
            }

            // ENC R gira → sub-param 2
            if (delta_r != 0) {
                uint8_t pidx = g_fx_sub2[g_active_fx];
                FxParam& p   = fx_desc[g_active_fx].params[pidx];
                p.value += delta_r * p.step;
                if (p.value < p.min_val) p.value = p.min_val;
                if (p.value > p.max_val) p.value = p.max_val;
                apply_fx_param(g_active_fx, pidx);
                bridge_send_fx_param(g_active_fx, pidx);
                if (p.is_int)
                    Serial.printf("[SUB2] %s: %d\n", p.name, (int)p.value);
                else
                    Serial.printf("[SUB2] %s: %.3f\n", p.name, (double)p.value);
            }

            // ENC R push → rotar qué parámetro controla sub2 (skip el que ya tiene sub1)
            if (r_push) {
                uint8_t num = fx_desc[g_active_fx].num_params;
                uint8_t s1  = g_fx_sub1[g_active_fx];
                uint8_t s2  = g_fx_sub2[g_active_fx];
                for (uint8_t i = 0; i < num; i++) {
                    s2 = (s2 + 1) % num;
                    if (s2 != s1) break;
                }
                g_fx_sub2[g_active_fx] = s2;
                float sync = (float)(g_active_fx * 100 + g_fx_sub1[g_active_fx] * 10 + s2);
                bridge_send_param_raw(0x00F7, sync);
                bridge_send_fx_param(g_active_fx, s2);
                Serial.printf("[SUB2 ROT] FX%u sub2→%u (%s)\n",
                              g_active_fx, s2, fx_desc[g_active_fx].params[s2].name);
            }
        }

        return;  // NO caer al handler de SYNTH
    }

    // ──────────────────────────────────────────────────────────────────────────
    // Nivel 0: ENGINE_LIST
    // ENC NAV gira → mueve cursor en lista (wrap modular)
    // ENC NAV push → seleccionar engine → ir a SUBHOME
    // ──────────────────────────────────────────────────────────────────────────
    if (g_synth_level == 0) {

        if (delta_nav != 0) {
            int32_t next = ((int32_t)g_engine_cursor + delta_nav + NUM_ENGINES) % NUM_ENGINES;
            g_engine_cursor = (uint8_t)next;
            bridge_send_param_raw(0x00FE, (float)g_engine_cursor);
            Serial.printf("[LIST] cursor=%u (%s)\n",
                          g_engine_cursor, g_desc[g_engine_cursor].name);
        }

        if (nav_push) {
            activate_engine(g_engine_cursor);
            g_synth_level = 1;
            bridge_send_param_raw(0x00F5, 1.0f);  // ir a SUBHOME

            // LED: ir a SUBHOME — todos tenue teal + save engine seleccionado
            led_ring.update_synth(g_synth_level, g_active_engine, g_active_group);
            save_state_if_changed();

            // Iniciar AI arm en nivel SUBHOME
            g_ai_arming    = true;
            g_ai_arm_start = now;
            g_ai_last_prog = now;
        }

    // ──────────────────────────────────────────────────────────────────────────
    // Nivel 1: ENGINE_SUBHOME
    // ENC L gira → adjust FILTER.cutoff (quick access)
    // ENC R gira → adjust FILTER.resonance (quick access)
    // ENC NAV push → ir a PARAM_LIST (lista plana de todos los params)
    // ENC L push / ENC R push → volver a ENGINE_LIST
    // ──────────────────────────────────────────────────────────────────────────
    } else if (g_synth_level == 1) {

        if (delta_l != 0) {
            SynthParam& cutoff = g_desc[g_active_engine].groups[2].params[0];
            cutoff.value = clampf(cutoff.value + delta_l * cutoff.step,
                                  cutoff.min_val, cutoff.max_val);
            apply_param(g_active_engine, 2, 0);
            bridge_send_synth_param(g_active_engine, 2, 0);
            Serial.printf("[SUBHOME] cutoff=%.0f\n", cutoff.value);
        }

        if (delta_r != 0) {
            SynthParam& res = g_desc[g_active_engine].groups[2].params[1];
            res.value = clampf(res.value + delta_r * res.step,
                               res.min_val, res.max_val);
            apply_param(g_active_engine, 2, 1);
            bridge_send_synth_param(g_active_engine, 2, 1);
            Serial.printf("[SUBHOME] resonance=%.3f\n", res.value);
        }

        if (nav_push) {
            g_synth_level = 2;
            bridge_send_param_raw(0x00F5, 2.0f);

            // Anunciar param actual al display — derivado del cursor plano
            uint8_t grp, pidx;
            flat_to_gp(g_active_engine, g_param_cursor[g_active_engine], grp, pidx);
            g_active_group = grp;
            bridge_send_param_raw(0x00F6, (float)(grp * 10 + pidx));
            Serial.printf("[PARAM_LIST] entró — flat=%u group=%s param=%s\n",
                          g_param_cursor[g_active_engine],
                          g_desc[g_active_engine].groups[grp].name,
                          g_desc[g_active_engine].groups[grp].params[pidx].name);

            // Iniciar AI arm
            g_ai_arming    = true;
            g_ai_arm_start = now;
            g_ai_last_prog = now;
        }

        if (l_push || r_push) {
            g_synth_level = 0;
            bridge_send_param_raw(0x00F5, 0.0f);
            bridge_send_param_raw(0x00FE, (float)g_engine_cursor);
            Serial.println("[LIST] volvió desde SUBHOME");
        }

    // ──────────────────────────────────────────────────────────────────────────
    // Nivel 2: PARAM_LIST — lista plana de todos los params del engine
    // ENC NAV gira → navega params en secuencia (OSC→ENV→FILTER→LFO, sin saltos)
    // ENC R gira   → ajusta el valor del param seleccionado
    // ENC L push / ENC R push → volver a SUBHOME
    //
    // No hay selección de grupo explícita — el cursor plano lo determina solo.
    // El display ESP32 recibe (group, pidx) derivados del cursor para mostrar
    // la vista correcta por grupo (OSC/ENV/FILTER/LFO) sin cambio en el protocolo.
    // ──────────────────────────────────────────────────────────────────────────
    } else { // g_synth_level == 2

        if (delta_nav != 0) {
            uint8_t total = g_flat_count[g_active_engine];
            int32_t next  = (int32_t)g_param_cursor[g_active_engine] + delta_nav;
            // clamp con sensación de límite (no wrap) — el usuario siente el borde
            if (next < 0)       next = 0;
            if (next >= total)  next = total - 1;
            g_param_cursor[g_active_engine] = (uint8_t)next;

            uint8_t grp, pidx;
            flat_to_gp(g_active_engine, g_param_cursor[g_active_engine], grp, pidx);
            g_active_group = grp;
            bridge_send_param_raw(0x00F6, (float)(grp * 10 + pidx));
            Serial.printf("[PARAM_LIST] flat=%u  %s › %s\n",
                          g_param_cursor[g_active_engine],
                          g_desc[g_active_engine].groups[grp].name,
                          g_desc[g_active_engine].groups[grp].params[pidx].name);
        }

        // ENC R o ENC L editan el param seleccionado — cualquiera de los dos sirve.
        // ENC L = pasos positivos hacia la derecha (mismo sentido que ENC R).
        // Permite editar con el encoder más cómodo según la posición del param en pantalla.
        {
            int32_t delta_edit = delta_r + delta_l;   // sumar ambos deltas
            if (delta_edit != 0) {
                uint8_t grp, pidx;
                flat_to_gp(g_active_engine, g_param_cursor[g_active_engine], grp, pidx);
                SynthParam& p = g_desc[g_active_engine].groups[grp].params[pidx];
                p.value = clampf(p.value + delta_edit * p.step, p.min_val, p.max_val);
                apply_param(g_active_engine, grp, pidx);
                bridge_send_synth_param(g_active_engine, grp, pidx);
                if (p.is_int)
                    Serial.printf("[PARAM] %s = %d\n", p.name, (int)p.value);
                else
                    Serial.printf("[PARAM] %s = %.3f\n", p.name, p.value);
            }
        }

        if (l_push || r_push) {
            g_synth_level = 1;
            bridge_send_param_raw(0x00F5, 1.0f);
            Serial.println("[SUBHOME] volvió desde PARAM_LIST");
        }
    }

    // ── ENC NAV release: cancelar AI arm si soltó antes del threshold ──────────
    if (nav_release && g_ai_arming) {
        uint32_t held = now - g_ai_arm_start;
        if (held < AI_HOLD_MS) {
            g_ai_arming = false;
            bridge_send_param_raw(0x00FB, -1.0f);
            Serial.println("[AI] arm cancelado (solté antes)");
        }
    }

    // ── AI arm: progreso y disparo ─────────────────────────────────────────────
    if (g_ai_arming) {
        uint32_t held = now - g_ai_arm_start;
        if (held >= AI_HOLD_MS) {
            g_ai_arming = false;
            bridge_send_param_raw(0x00FD, 2.0f);  // 2 = AI mode
            g_in_ai_mode = true;
            g_smart_arp.enabled = true;
            led_ring.enter_ai_mode();
            Serial.println("[AI] activado!");

            // Sincronizar estado inicial de la AI Rack con el ESP32.
            // Sin este bloque la Rack mostraría valores stale al entrar.
            // HarmonyInterval::THIRD=0, SIXTH=1 — se envía el índice del enum.
            {
                uint8_t harm_iv = (g_auto_harm.interval == HarmonyInterval::SIXTH) ? 1u : 0u;
                bridge_send_param_raw(0x00E9, 0.0f);  // cursor fila 0 (SCALE LOCK)
                bridge_send_param_raw(0x00EA, g_auto_harm.enabled ? 1.0f : 0.0f);
                bridge_send_param_raw(0x00EB, (float)harm_iv);
                bridge_send_param_raw(0x00EC, 4.0f);  // SMART arp mode (default)
                bridge_send_param_raw(0x00ED, 1.0f);  // EIGHTH division (default)
                bridge_send_param_raw(0x00EE, 1.5f);  // HUMAN style + amount 0.5 (packed)
            }
        } else if (held > 200) {
            if ((now - g_ai_last_prog) >= AI_PROG_INTERVAL) {
                float progress = (float)held / (float)AI_HOLD_MS;
                bridge_send_param_raw(0x00FB, progress);
                g_ai_last_prog = now;
            }
        }
    }
}

// ── Handle buttons ──────────────────────────────────────────────────────────────

static void handle_buttons() {
    uint32_t now = millis();

    // B1 = HOME: comportamiento según el modo activo
    if (buttons.pressed(0)) {
        // En FX mode + FX_MAIN: B1 regresa a FX_SELECT (no ejecutar HOME de SYNTH)
        if (g_display_mode == 0 && g_fx_view == FxView::FX_MAIN) {
            g_fx_view = FxView::FX_SELECT;
            bridge_send_param_raw(0x00FE, (float)g_fx_cursor);  // → FX_SELECT
            Serial.println("[HOME] FX_MAIN → FX_SELECT");
            return;
        }

        // SYNTH mode (o FX_SELECT): HOME completo → ENGINE_LIST + cancelar AI
        g_synth_level        = 0;
        g_ai_arming          = false;
        g_in_ai_mode         = false;
        g_auto_harm.enabled  = false;   // reset harmonía al salir de AI mode
        g_smart_arp.enabled  = false;   // silenciar arp al salir de AI mode
        // Resetear estadísticas de Scale Lock para que la próxima sesión AI arranque limpia.
        g_scale_lock.reset_stats();
        // Resetear manual scale override → AUTO mode al salir de AI.
        if (g_scale_manual_mode) {
            g_scale_manual_mode = false;
            g_scale_lock.clear_manual();
            bridge_send_param_raw(0x00EF, -1.0f);
            Serial.println("[SCALE] manual override cleared -> AUTO");
        }
        // Resetear sub-vista AI: la próxima entrada arranca siempre en view_10 (Camelot).
        g_ai_subview = 0;
        bridge_send_param_raw(0x00F1, 0.0f);   // ESP32 vuelve a view_10 al re-entrar
        bridge_send_param_raw(0x00F5, 0.0f);
        bridge_send_param_raw(0x00FE, (float)g_engine_cursor);
        bridge_send_param_raw(0x00FB, -1.0f);  // cancelar progress bar en ESP32
        led_ring.enter_synth_mode(0, g_active_engine, 0);  // volver a ENGINE_LIST visual
        Serial.println("[HOME] → ENGINE_LIST");
    }

    // B2 = MODE TOGGLE FX ↔ SYNTH (display ESP32).
    // El audio Teensy sigue siendo SYNTH independientemente; solo cambia la
    // vista del ESP32. En producción main.cpp coordinará ambos sketches.
    if (buttons.pressed(1)) {
        g_display_mode = (g_display_mode == 0) ? 1 : 0;
        bridge_send_param_raw(0x00FD, (float)g_display_mode);
        // LED + estado de navegación al cambiar modo
        if (g_display_mode == 0) {
            // Entrar a FX mode: desactivar cualquier FX previo + activar monitor de line-in
            g_fx_view = FxView::FX_SELECT;
            g_fx_manager.deactivate();           // limpia conexiones del FX anterior si las había
            audio_graph::set_fx_select_mode();   // ch0=0, ch1=0, ch2=1 (line-in directo)
            led_ring.enter_fx_mode();
        } else {
            // Volver a SYNTH mode: deactivar FX + restaurar dry path de engines
            g_fx_manager.deactivate();           // libera FX y sus AudioConnections
            audio_graph::set_synth_mode();       // ch0=1 (engines), ch1=0, ch2=0
            g_synth_level = 0;
            led_ring.enter_synth_mode(0, g_active_engine, 0);
        }
        save_state_if_changed();
        Serial.printf("[B2] DISPLAY MODE → %s\n", g_display_mode == 1 ? "SYNTH" : "FX");
    }

    // B3 = TAP TEMPO — universal
    if (buttons.pressed(2)) {
        if (g_tap_armed) {
            uint32_t interval = now - g_last_tap_ms;
            if (interval >= 50 && interval <= 2000) {
                // TODO Sprint 32: enrutar tap tempo al LFO rate del engine activo
                Serial.printf("[TAP] intervalo=%ums\n", (unsigned)interval);
            }
        }
        g_last_tap_ms = now;
        g_tap_armed   = true;
        led_ring.tap_tempo_flash();  // flash B3 teal 80ms
    }

    // B4: comportamiento dual según modo activo
    //   En AI mode → toggle Auto-Harmonization (Tier S, Sprint 37)
    //   En cualquier otro modo → PANIC (silenciar todos los engines)
    if (buttons.pressed(3)) {
        if (g_in_ai_mode) {
            // Toggle harmonía — solo efectivo con Prophet5 (polyfónico)
            g_auto_harm.enabled = !g_auto_harm.enabled;
            // Si se desactiva, liberar todas las voces harmónicas pendientes
            if (!g_auto_harm.enabled) {
                all_engines_panic();  // limpio: silencia todo, incluye voces huérfanas
            }
            led_ring.tap_tempo_flash();  // flash teal sutil como confirmación
            Serial.printf("[HARM] auto-harmonize %s | engine=%u\n",
                          g_auto_harm.enabled ? "ON" : "OFF",
                          (unsigned)g_active_engine);
            if (g_auto_harm.enabled && g_active_engine < 2) {
                Serial.println("[HARM] WARNING: solo activo en motores polifónicos (engines 2-5)");
            }
        } else {
            all_engines_panic();
            bridge_send_param_raw(0x00F8, 0.0f);  // señal panic al ESP32
            led_ring.panic_flash();                // flash rojo ring 120ms
            Serial.println("[PANIC] all engines silenciados");
        }
    }
}

// ── Handle heartbeat ────────────────────────────────────────────────────────────

static void handle_heartbeat() {
    if (g_hb_flag) {
        g_hb_flag = false;
        bridge.send_heartbeat();
    }
}

// ── State save helper ────────────────────────────────────────────────────────────

/**
 * @brief Persiste el estado actual si cambió respecto al guardado en EEPROM.
 *
 * Compara display_mode + active_engine contra g_saved_state.
 * Solo escribe si hay diferencia (EEPROM.put() interno también filtra por byte,
 * pero esta comparación evita incluso el overhead de la llamada).
 */
static void save_state_if_changed() {
    bool changed = (g_saved_state.display_mode != g_display_mode) ||
                   (g_saved_state.active_engine != g_active_engine);
    if (!changed) return;

    g_saved_state.magic         = DEVICE_STATE_MAGIC;
    g_saved_state.display_mode  = g_display_mode;
    g_saved_state.active_engine = g_active_engine;
    device_state_save(g_saved_state);
}

// ── Beat-synced FX (Sprint 5.4) ──────────────────────────────────────────────────
//
// Cuando AI mode está activo y BeatSync tiene un BPM válido (confidence >= 0.7),
// ajusta automáticamente:
//   GhostEcho (UI idx 0) → delay_ms a corchea con punto (el "Edge delay")
//   PhaseChorus (UI idx 2) → rate a 1 Hz por beat (BPM/60)
//
// El cambio es solo audio — NO actualiza fx_desc ni envía al ESP32 para evitar
// spam en el bus UART. El encoder del usuario puede sobreescribir en cualquier
// momento (el siguiente tick de beat-sync lo restaurará si AI mode sigue activo).
//
// Cadencia: 500ms (coincide con el intervalo de inferencia del MLEngine para
// que el BPM suavizado haya convergido antes de aplicarse al audio).

static void handle_beat_sync_fx(uint32_t now) {
    if (!g_in_ai_mode || !g_fx_manager.is_active()) return;
    if (g_fx_bypass || !g_beat_sync.has_bpm()) return;

    static uint32_t s_last_ms = 0;
    if (now - s_last_ms < 500) return;
    s_last_ms = now;

    switch (g_active_fx) {

    case 0: {  // Ghost Echo — sync delay a corchea con punto
        float ms = g_beat_sync.delay_ms(Subdivision::DOTTED_EIGHTH);
        // GhostEcho::setDelayMs() acepta [10, 1400] ms — rango mayor que el UI
        // descriptor (0-200 ms). Beat-sync usa el rango real del hardware para
        // tiempos musicales de tempo moderado (ej. 375 ms a 120 BPM).
        if (ms >= 10.0f && ms <= 1400.0f) {
            fx_desc[0].params[0].value = ms;
            apply_fx_param(0, 0);
            Serial.printf("[BeatSync] GhostEcho delay → %.0f ms (BPM %.1f)\n",
                          (double)ms, (double)g_beat_sync.bpm());
        }
        break;
    }

    case 2: {  // Phase Chorus — sync LFO rate a 1 beat por ciclo (BPM/60 Hz)
        // A 120 BPM → 2 Hz → el phaser completa un ciclo por negra.
        // Rango de PhaseChorus::setRate(): [0.1, 10.0] Hz, cubriendo 6-600 BPM.
        float rate_hz = g_beat_sync.bpm() / 60.0f;
        if (rate_hz < 0.1f)  rate_hz = 0.1f;
        if (rate_hz > 10.0f) rate_hz = 10.0f;
        fx_desc[2].params[0].value = rate_hz;
        apply_fx_param(2, 0);
        Serial.printf("[BeatSync] PhaseChorus rate → %.2f Hz (BPM %.1f)\n",
                      (double)rate_hz, (double)g_beat_sync.bpm());
        break;
    }

    }
}

// ── Setup ────────────────────────────────────────────────────────────────────────

void setup() {
    Serial.begin(115200);
    delay(200);
    Serial.println("[SKETCH 28] Synth Navigator — GrooveForge Brain");

    // ── Sprint 36: Audio graph backbone ─────────────────────────────────────
    // audio_graph::init() hace AudioMemory(80) + Wire.begin() + codec.enable()
    // + gains iniciales del engine_mix (Moog activo, otros muteados).
    // Reemplaza el AudioMemory(40) + moog.begin(0.5f) que vivía aquí antes.
    audio_graph::init(0.5f);

    // Inicializar los 6 engines (solo setean sus audio objects internos —
    // NO tocan I2S/codec, eso ya fue hecho por audio_graph::init()).
    moog.begin();
    juno.begin();
    prophet.begin();
    ob6.begin();
    dx7.begin();
    arp.begin();

    // ── Test tone de arranque ──────────────────────────────────────────────────
    // Dispara A4 (MIDI 69) por 1.2 s inmediatamente después de begin() para
    // verificar que el audio interrupt del I2S DMA arrancó correctamente.
    //
    // Diagnóstico:
    //   cpu > 0.0%  → interrupt corriendo, audio path OK. Si no se oye → volumen
    //                 o routing del SGTL5000.
    //   cpu = 0.0%  → interrupt NO corriendo. Buscar conflicto de AudioMemory,
    //                 AudioOutputI2S sin DMA, o fallo de I2C con SGTL5000.
    //
    // El loop llama moog.update() para que el filter envelope avance — sin
    // update() la frecuencia del filtro no se aplica y el sonido puede estar mudo
    // aunque el VCA envelope esté activo.
    Serial.println("[AUDIO TEST] A4 (440 Hz) — 1.2 s — revisar salida de headphones");
    moog.noteOn(69, 1.0f);
    for (int i = 0; i < 24; i++) {
        delay(50);
        moog.update();
        if (i % 6 == 5) {
            Serial.printf("[AUDIO TEST] t=%dms  cpu=%.2f%%  (>0 = interrupt OK)\n",
                          (i + 1) * 50, AudioProcessorUsage());
        }
    }
    moog.noteOff();
    delay(300);
    moog.update();
    Serial.printf("[AUDIO TEST] done — cpu_max=%.2f%%\n", AudioProcessorUsageMax());
    AudioProcessorUsageMaxReset();
    // ──────────────────────────────────────────────────────────────────────────

    // Construir tabla de navegación plana — debe hacerse ANTES de encoders/bridge
    // porque activate_engine() la usa para derivar g_active_group.
    build_flat_params();

    // Aplicar todos los parámetros por defecto al Moog
    for (uint8_t g = 0; g < 4; g++) {
        for (uint8_t p = 0; p < g_desc[0].groups[g].num_params; p++) {
            apply_param(0, g, p);
        }
    }

    // UI
    encoders.init();
    buttons.init();

    // LED ring (Sprint 35) — inicializar ANTES del delay del ESP32
    led_ring.init();

    // MIDI USB-A host — registrar callbacks antes de init()
    midi_host.on_note_on(midi_note_on);
    midi_host.on_note_off(midi_note_off);
    midi_host.on_cc(midi_cc);
    midi_host.init();

    // Bridge
    bridge.init();
    hb_timer.begin(hb_isr, 1000000UL); // 1Hz — ISR solo setea flag

    // ── Restaurar estado desde EEPROM (Sprint 35) ─────────────────────────────
    // Carga display_mode + active_engine del último arranque.
    // Si el EEPROM está virgen (primer arranque), usa SYNTH + Moog por defecto.
    {
        DeviceState ds;
        if (!device_state_load(ds)) ds = device_state_default();
        g_saved_state   = ds;
        g_display_mode  = ds.display_mode;
        g_active_engine = ds.active_engine;
        Serial.printf("[SETUP] boot state — mode=%u engine=%u\n",
                      (unsigned)g_display_mode, (unsigned)g_active_engine);
    }

    // Dar tiempo al ESP32 para completar su boot antes de recibir frames
    delay(500);

    // ── Anunciar estado restaurado al ESP32 ───────────────────────────────────
    // El ESP32 usa estos mensajes para navegar al carousel correcto en boot.
    //
    //   0x00F4  = top_mode:  0=FX, 1=SYNTH (AI requiere hold gesture, no se restaura)
    //   engine announce = ENGINE_CHANGED + nombre
    //   0x00F5  = synth_level: 1=SUBHOME (siempre al subhome, no a la lista plana)
    //
    // FX mode: display mostrará view_07 (FX MAIN) con el FX activo al entrar
    // SYNTH mode: display mostrará ENGINE_SUBHOME del engine guardado

    // Activar el engine guardado en el DSP (audio siempre SYNTH)
    activate_engine(g_active_engine);

    if (g_display_mode == 0) {
        // FX mode — display a FX_SELECT para que el usuario elija FX
        bridge_send_param_raw(0x00F4, 0.0f);             // top_mode = FX
        // 0x00FE con el cursor actual → ESP32 navega a VIEW_IDX_FX_SELECT.
        // Sin este segundo comando, el ESP32 handler de 0x00F4 no navega a ninguna
        // vista para modo FX (solo navega automáticamente para SYNTH mode=1).
        bridge_send_param_raw(0x00FE, (float)g_fx_cursor); // navegar a FX_SELECT
        audio_graph::set_fx_select_mode();  // ch2=1 para monitoreo de line-in en boot
        led_ring.enter_fx_mode();
        Serial.println("[SETUP] boot → FX mode → FX_SELECT (line-in monitor activo)");
    } else {
        // SYNTH mode — display a ENGINE_SUBHOME del engine guardado.
        // set_synth_mode() es el default de audio_graph::init(), pero se llama
        // explícitamente para dejar el intent claro en el boot path.
        audio_graph::set_synth_mode();
        bridge_send_param_raw(0x00F4, 1.0f);   // top_mode = SYNTH
        bridge_announce_engine(g_active_engine);
        g_synth_level = 1;                     // ir a SUBHOME directamente
        bridge_send_param_raw(0x00F5, 1.0f);   // synth_level = SUBHOME
        led_ring.enter_synth_mode(g_synth_level, g_active_engine, g_active_group);
        Serial.printf("[SETUP] boot → SYNTH SUBHOME engine=%u\n",
                      (unsigned)g_active_engine);
    }

    bridge_send_param_raw(0x00F2, 0.0f);  // scale lock activo por defecto
    bridge_send_param_raw(0x00F3, 0.0f);  // beat follower activo por defecto

    // Smart Arpeggiator (Sprint 42)
    g_smart_arp.set_callbacks(arp_on_note, arp_off_note, nullptr);
    g_smart_arp.set_mode(SmartArp::Mode::SMART);
    g_smart_arp.set_division(SmartArp::Division::EIGHTH);
    g_smart_arp.set_gate(0.7f);
    // Groove Humanizer (Sprint 6.4) — humanización moderada por default al arrancar en AI mode
    g_smart_arp.set_groove(GrooveHumanizer::Style::HUMAN, 0.5f);

    // Velocity Curve Learn (Sprint 6.5) — limpiar histograma al arrancar
    g_vel_curve.reset();

    // ML Engine (Sprint 32 Batch B)
    if (g_ml_engine.init()) {
        g_scale_lock.set_bypass(g_scale_lock_bypass);
        Serial.println("[SETUP] MLEngine OK");
    } else {
        Serial.println("[SETUP] MLEngine FAILED — sin inferencia AI");
    }

    g_last_status_ms = millis();
    Serial.println("[SETUP] done");
}

// ── Loop ─────────────────────────────────────────────────────────────────────────

void loop() {
    bridge.poll();
    handle_heartbeat();

    // ── USB-A host MIDI (teclado MIDI externo conectado al puerto USB-A) ────────
    midi_host.poll();

    // ── USB device MIDI (computador / DAW → micro-USB) ───────────────────────────
    // Con USB_MIDI_AUDIO_SERIAL el Teensy es simultáneamente host (USB-A) y device
    // (micro-USB). usbMIDI consume los mensajes que el computador envía al Teensy.
    // Ruteamos a los mismos callbacks del host para unificar la lógica de audio.
    // Útil para probar con una app MIDI virtual en el computador sin teclado físico.
    while (usbMIDI.read()) {
        uint8_t type = usbMIDI.getType();
        uint8_t ch   = usbMIDI.getChannel();
        uint8_t d1   = usbMIDI.getData1();
        uint8_t d2   = usbMIDI.getData2();
        if      (type == usbMIDI.NoteOn)          midi_note_on(ch, d1, d2);
        else if (type == usbMIDI.NoteOff)         midi_note_off(ch, d1, d2);
        else if (type == usbMIDI.ControlChange)   midi_cc(ch, d1, d2);
        else if (type == usbMIDI.PitchBend) {
            // usbMIDI.getData1()/getData2() para PitchBend: d1=LSB, d2=MSB (7-bit cada uno)
            int16_t bend = (int16_t)(((uint16_t)d2 << 7) | d1) - 8192;
            (void)bend; // pitch bend: reservado para Sprint 32 (portamento control)
        }
    }

    encoders.update();
    buttons.update();

    // Sprint 38-40: los 6 engines tienen begin() llamado y audio paths conectados
    // al engine_mix_a/b. update() avanza los filter envelopes (software) — barato,
    // ~1µs por engine. Los engines inactivos están muteados via mixer gain pero
    // mantenemos update() llamado para que su estado interno no quede stale.
    moog.update();
    juno.update();
    prophet.update();
    ob6.update();
    dx7.update();
    arp.update();

    // FxManager update — solo TapeSaturate tiene LFOs que necesitan avanzar.
    // El resto de FX tienen update() como no-op, pero llamamos siempre para
    // no tener que chequear el tipo activo en loop().
    {
        static uint32_t s_fx_last_ms = 0;
        const uint32_t  t_now = millis();
        const float     dt    = (float)(t_now - s_fx_last_ms);
        s_fx_last_ms = t_now;
        g_fx_manager.update(dt);
    }

    uint32_t now = millis();
    g_smart_arp.update(now);
    handle_ai_inference(now);
    handle_beat_sync_fx(now);
    handle_encoders();
    handle_buttons();

    // ── LED ring update ~20Hz (Sprint 35) ────────────────────────────────────
    // En FX y SYNTH solo se actualiza si el estado cambió (handle en update_synth/fx).
    // En AI mode el chromagram necesita refresh constante desde pitch_activity.
    if (now - g_led_last_ms >= LED_UPDATE_MS) {
        g_led_last_ms = now;
        if (g_in_ai_mode) {
            uint8_t act[12];
            for (uint8_t i = 0; i < 12; i++) act[i] = g_ml_engine.get_pitch_activity_u8(i);
            led_ring.update_ai(act);
        }
        // FX y SYNTH se actualizan desde sus respectivos handle_ cuando cambia estado
    }

    // Status periódico cada 5s
    if (now - g_last_status_ms >= STATUS_PRINT_MS) {
        g_last_status_ms = now;
        Serial.printf("[STATUS] engine=%s level=%u flat=%u group=%s cpu=%.1f%%\n",
                      g_desc[g_active_engine].name,
                      g_synth_level,
                      g_param_cursor[g_active_engine],
                      g_desc[g_active_engine].groups[g_active_group].name,
                      AudioProcessorUsageMax());
        AudioProcessorUsageMaxReset();
        // AudioMemory: si llega a 80 el pool está lleno → audio dropout silencioso.
        // mem=X/Y donde X=uso actual, Y=pico desde el último reset.
        Serial.printf("[STATUS] mem=%u/%u (pool=80) fx=%s wet=%.2f\n",
                      AudioMemoryUsage(),
                      AudioMemoryUsageMax(),
                      g_fx_manager.is_active()
                          ? fx_desc[g_active_fx].name
                          : "none",
                      (double)g_fx_wet_dry);
        AudioMemoryUsageMaxReset();
        Serial.printf("[MIDI] connected=%s\n",
                      midi_host.is_connected() ? "yes" : "no");
    }
}
