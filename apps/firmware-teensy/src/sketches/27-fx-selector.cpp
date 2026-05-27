// GrooveForge Brain — Sketch 27: FX Selector
// Sprint 2.12 — Selector de FX con navegación dual, encoders, botones y Bridge Protocol
//
// Grafo de audio:
//   AudioInputI2S line_in → 9 FX en paralelo (begin con line_in)
//   cascade de mixers:
//     mixL_A: FX[0..3], mixL_B: FX[4..7], mixL_C: FX[8] + mixL_A + mixL_B → line_out ch0
//     mismo para R
//
// Solo el FX activo tiene setBypass(false) con setMix(wet_dry). Los demás: setBypass(true).

#include <Audio.h>
#include <Wire.h>
#include <EEPROM.h>
#include <string.h>

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
#include "../bridge/bridge_master.h"
#include <protocol.h>

// ── Constantes ─────────────────────────────────────────────────────────────────

static constexpr uint8_t  NUM_FX               = 9;
static constexpr uint8_t  NUM_PRESETS          = 12;
static constexpr uint16_t PRESET_EEPROM_BASE   = 0;
static constexpr uint32_t PRESET_MAGIC         = 0xBEEF01;
static constexpr uint32_t AI_HOLD_MS           = 4000;  // hold para AI mode
static constexpr uint32_t AI_PROG_INTERVAL     = 100;   // ms entre progress updates
static constexpr uint32_t STATUS_PRINT_MS      = 5000;

// ── Audio I/O ──────────────────────────────────────────────────────────────────

AudioInputI2S            line_in;
AudioOutputI2S           line_out;
AudioControlSGTL5000     codec;

// ── FX instances ───────────────────────────────────────────────────────────────

GhostEcho         fx_ghost;       // 0
ModalReverb       fx_modal;       // 1
PhaseChorus       fx_chorus;      // 2
BitSculpt         fx_bit;         // 3
TapeSaturate      fx_tape;        // 4
CymaticResonator  fx_cymatic;     // 5
GranularCloud     fx_granular;    // 6
SpringPlate       fx_spring;      // 7
SubGenesis        fx_sub;         // 8

// Punteros para despacho genérico — se llenan en setup()
static AudioStream* fx_out_l[NUM_FX];
static AudioStream* fx_out_r[NUM_FX];

// ── Mixer cascade L ────────────────────────────────────────────────────────────
// AudioMixer4 acepta 4 entradas. Cascade: A(FX0-3), B(FX4-7), C(FX8 + A + B) → out

AudioMixer4 mixL_A;   // FX[0..3] L
AudioMixer4 mixL_B;   // FX[4..7] L
AudioMixer4 mixL_C;   // FX[8].L, mixL_A, mixL_B → line_out ch0

AudioMixer4 mixR_A;   // FX[0..3] R
AudioMixer4 mixR_B;   // FX[4..7] R
AudioMixer4 mixR_C;   // FX[8].R, mixR_A, mixR_B → line_out ch1

// Conexiones del cascade de mixers (estáticas — salen de getFX, no de line_in)
// Se crean con new en setup() porque dependen de fx_out_l[]/fx_out_r[] que se
// asignan antes de llamar begin() en cada FX.
static AudioConnection* patchL[NUM_FX];
static AudioConnection* patchR[NUM_FX];
static AudioConnection  patchMixLA_to_C{mixL_A, 0, mixL_C, 1};
static AudioConnection  patchMixLB_to_C{mixL_B, 0, mixL_C, 2};
static AudioConnection  patchMixRA_to_C{mixR_A, 0, mixR_C, 1};
static AudioConnection  patchMixRB_to_C{mixR_B, 0, mixR_C, 2};
static AudioConnection  patchOutL{mixL_C, 0, line_out, 0};
static AudioConnection  patchOutR{mixR_C, 0, line_out, 1};

// Analizador FFT — conectado al output del cascade mixer (señal post-FX)
AudioAnalyzeFFT256  fft_analyze;
static AudioConnection patchFFT{mixL_C, 0, fft_analyze, 0};

// ── UI ─────────────────────────────────────────────────────────────────────────

Encoders  encoders;
Buttons   buttons;

// ── Bridge ─────────────────────────────────────────────────────────────────────

BridgeMaster bridge;
IntervalTimer hb_timer;

// IntervalTimer ISR no puede llamar Serial1.write() — usa flag para serializar
// el heartbeat en loop() context. Patrón seguro en Teensy 4.1.
static volatile bool g_hb_flag = false;
static void hb_isr() { g_hb_flag = true; }

// ── Parámetros por FX ──────────────────────────────────────────────────────────

struct FxParam {
    const char* name;
    float       value;
    float       default_val;
    float       min_val;
    float       max_val;
    float       step;
    bool        is_int;
};

// GhostEcho (4 params)
static FxParam params_ghost[] = {
    {"delay_ms",  0.0f,   0.0f,   0.0f,    200.0f, 10.0f,  false},
    {"feedback",  0.4f,   0.4f,   0.0f,    0.95f,   0.01f,  false},
    {"tape_lp",   5000.0f,5000.0f,1000.0f, 12000.0f,200.0f, false},
    {"mix",       0.5f,   0.5f,   0.0f,    1.0f,    0.02f,  false},
};

// ModalReverb (4 params)
static FxParam params_modal[] = {
    {"material",  0.0f, 0.0f, 0.0f, 3.0f, 1.0f, true},
    {"size",      1.0f, 1.0f, 0.0f, 2.0f, 1.0f, true},
    {"decay",     1.0f, 1.0f, 0.5f, 3.0f, 0.1f, false},
    {"mix",       0.5f, 0.5f, 0.0f, 1.0f, 0.02f,false},
};

// PhaseChorus (4 params)
static FxParam params_chorus[] = {
    {"rate",   0.5f, 0.5f, 0.1f, 10.0f, 0.1f,  false},
    {"depth",  0.5f, 0.5f, 0.0f, 1.0f,  0.02f, false},
    {"drift",  0.3f, 0.3f, 0.0f, 1.0f,  0.02f, false},
    {"voices", 2.0f, 2.0f, 1.0f, 4.0f,  1.0f,  true},
};

// BitSculpt (4 params)
static FxParam params_bit[] = {
    {"bits",   8.0f,    8.0f,    1.0f,    16.0f,   1.0f,    true},
    {"srate",  22050.0f,22050.0f,1000.0f, 44100.0f,1000.0f, false},
    {"sculpt", 0.5f,    0.5f,    0.0f,    1.0f,    0.02f,   false},
    {"mix",    0.5f,    0.5f,    0.0f,    1.0f,    0.02f,   false},
};

// TapeSaturate (4 params — sin mix aquí, mix es wet_dry global)
static FxParam params_tape[] = {
    {"drive",   4.0f,  4.0f,  0.0f, 12.0f, 0.5f,  false},
    {"wow",     0.3f,  0.3f,  0.0f, 1.0f,  0.02f, false},
    {"flutter", 0.2f,  0.2f,  0.0f, 1.0f,  0.02f, false},
    {"age",     0.3f,  0.3f,  0.0f, 1.0f,  0.02f, false},
};

// CymaticResonator (4 params)
static FxParam params_cymatic[] = {
    {"tune",      1.0f,  1.0f,  0.5f, 2.0f,  0.05f, false},
    {"density",   2.0f,  2.0f,  1.0f, 4.0f,  1.0f,  true},
    {"resonance", 5.0f,  5.0f,  1.0f, 25.0f, 1.0f,  false},
    {"lfo_rate",  0.5f,  0.5f,  0.1f, 5.0f,  0.1f,  false},
};

// GranularCloud (4 params)
static FxParam params_granular[] = {
    {"grain_ms", 100.0f, 100.0f, 5.0f,  500.0f, 5.0f,  false},
    {"speed",    1.0f,   1.0f,   0.5f,  2.0f,   0.05f, false},
    {"freeze",   0.0f,   0.0f,   0.0f,  1.0f,   1.0f,  true},
    {"mix",      0.5f,   0.5f,   0.0f,  1.0f,   0.02f, false},
};

// SpringPlate (4 params)
static FxParam params_spring[] = {
    {"algorithm", 0.0f, 0.0f, 0.0f, 3.0f, 1.0f,  true},
    {"room",      0.6f, 0.6f, 0.0f, 1.0f, 0.02f, false},
    {"damp",      0.5f, 0.5f, 0.0f, 1.0f, 0.02f, false},
    {"mix",       0.5f, 0.5f, 0.0f, 1.0f, 0.02f, false},
};

// SubGenesis (4 params)
static FxParam params_sub[] = {
    {"sub_level", 0.8f,  0.8f,  0.0f,   1.0f,   0.02f, false},
    {"drive",     2.0f,  2.0f,  1.0f,   8.0f,   0.5f,  false},
    {"cutoff",    100.0f,100.0f,40.0f,  200.0f, 5.0f,  false},
    {"mix",       0.5f,  0.5f,  0.0f,   1.0f,   0.02f, false},
};

struct FxDescriptor {
    const char* name;
    FxParam*    params;
    uint8_t     num_params;
};

static FxDescriptor fx_desc[NUM_FX] = {
    {"Ghost Echo",        params_ghost,   4},
    {"Modal Reverb",      params_modal,   4},
    {"Phase Chorus",      params_chorus,  4},
    {"Bit Sculpt",        params_bit,     4},
    {"Tape Saturate",     params_tape,    4},
    {"Cymatic Resonator", params_cymatic, 4},
    {"Granular Cloud",    params_granular,4},
    {"Spring Plate",      params_spring,  4},
    {"Sub Genesis",       params_sub,     4},
};

// ── EEPROM Presets ─────────────────────────────────────────────────────────────

struct FxPreset {
    uint32_t magic;
    char     name[16];
    uint8_t  fx_index;
    float    wet_dry;
    bool     bypass;
    float    params[8];
};

static int8_t  g_last_saved_slot = -1;

// ── Estado global ──────────────────────────────────────────────────────────────

// Vista FX actual: SELECT muestra el carrusel de selección, MAIN es performance.
enum class FxView : uint8_t { FX_SELECT = 0, FX_MAIN };

static FxView   g_fx_view         = FxView::FX_SELECT;
static uint8_t  g_fx_cursor       = 0;   // cursor en FX_SELECT
static uint8_t  g_active_fx       = 0;   // FX activo (tiene audio)

static float    g_wet_dry         = 0.5f;
static bool     g_bypass          = false;
static bool     g_freeze_state    = false;

// Performance picks: índice de parámetro asignado a ENC L (sub1) y ENC R (sub2)
// por FX. Updateables via bridge 0xF7 en runtime.
static uint8_t g_fx_sub1[NUM_FX] = {0, 0, 0, 0, 0, 0, 0, 0, 0};
static uint8_t g_fx_sub2[NUM_FX] = {1, 2, 1, 1, 1, 2, 1, 1, 1};
// Mapping de referencia:
//   Ghost Echo(0):      sub1=delay_ms,  sub2=feedback
//   Modal Reverb(1):    sub1=material,  sub2=decay
//   Phase Chorus(2):    sub1=rate,      sub2=depth
//   Bit Sculpt(3):      sub1=bits,      sub2=srate
//   Tape Saturate(4):   sub1=drive,     sub2=wow
//   Cymatic Res(5):     sub1=tune,      sub2=resonance
//   Granular Cloud(6):  sub1=grain_ms,  sub2=speed
//   Spring Plate(7):    sub1=algorithm, sub2=room
//   Sub Genesis(8):     sub1=sub_level, sub2=drive

// Modo top-level: 0=FX, 1=SYNTH. B2 cicla entre los dos.
static uint8_t  g_top_mode      = 0;

// ── Estado navegación SYNTH (modo 1) ──────────────────────────────────────
// Espeja la jerarquía de sketch 28 / Sprint 31.
static constexpr uint8_t NUM_SYNTH_ENGINES = 3;   // Moog / Juno / Prophet
static uint8_t  g_synth_level   = 0;   // 0=ENGINE_LIST, 1=SUBHOME, 2=GROUP_VIEW
static uint8_t  g_engine_cursor = 0;   // cursor en ENGINE_LIST
static uint8_t  g_active_group  = 0;   // 0=OSC, 1=ENV, 2=FILTER, 3=LFO
static uint8_t  g_group_cursor  = 0;   // cursor dentro del grupo activo
// Valores normalizados [grupo][param] para envío al ESP32.
// FILTER inicial: cutoff=0.5 (voz media), resonance=0.1 (limpio).
static float    g_synth_vals[4][4] = {
    {0.5f, 0.5f, 0.5f, 0.5f},   // OSC
    {0.7f, 0.3f, 0.5f, 0.5f},   // ENV  (attack bajo, release medio)
    {0.5f, 0.1f, 0.5f, 0.5f},   // FILTER (cutoff mitad, resonance limpio)
    {0.3f, 0.5f, 0.5f, 0.5f},   // LFO
};

// Estado del arm de AI mode (hold 4s en ENC NAV en FX_MAIN)
static bool     g_ai_arming     = false;
static uint32_t g_ai_arm_start  = 0;
static uint32_t g_ai_last_prog  = 0;

// Edge detection de NAV para detectar release (necesario para cancelar AI arm)
static bool     g_nav_prev_down = false;

// Tap tempo
static uint32_t g_last_tap_ms     = 0;
static bool     g_tap_armed       = false;

// Status print timer
static uint32_t g_last_status_ms  = 0;

// dt tracking para FX que lo necesitan
static uint32_t g_last_loop_ms    = 0;

// ── Helpers — apply param to FX ────────────────────────────────────────────────

static void apply_param_ghost(uint8_t p) {
    float v = params_ghost[p].value;
    switch (p) {
        case 0: fx_ghost.setDelayMs(v);  break;
        case 1: fx_ghost.setFeedback(v); break;
        case 2: fx_ghost.setTapeLP(v);   break;
        case 3: fx_ghost.setMix(v);      break;
    }
}

static void apply_param_modal(uint8_t p) {
    float v = params_modal[p].value;
    switch (p) {
        case 0: fx_modal.setMaterial(static_cast<ModalReverb::Material>((uint8_t)v)); break;
        case 1: fx_modal.setSize(static_cast<ModalReverb::Size>((uint8_t)v));         break;
        case 2: fx_modal.setDecay(v);    break;
        case 3: fx_modal.setMix(v);      break;
    }
}

static void apply_param_chorus(uint8_t p) {
    float v = params_chorus[p].value;
    switch (p) {
        case 0: fx_chorus.setRate(v);             break;
        case 1: fx_chorus.setDepth(v);            break;
        case 2: fx_chorus.setDrift(v);            break;
        case 3: fx_chorus.setVoices((uint8_t)v);  break;
    }
}

static void apply_param_bit(uint8_t p) {
    float v = params_bit[p].value;
    switch (p) {
        case 0: fx_bit.setBits((uint8_t)v);       break;
        case 1: fx_bit.setSampleRate(v);          break;
        case 2: fx_bit.setSculpt(v);              break;
        case 3: fx_bit.setMix(v);                 break;
    }
}

static void apply_param_tape(uint8_t p) {
    float v = params_tape[p].value;
    switch (p) {
        case 0: fx_tape.setDrive(v);   break;
        case 1: fx_tape.setWow(v);     break;
        case 2: fx_tape.setFlutter(v); break;
        case 3: fx_tape.setAge(v);     break;
    }
}

static void apply_param_cymatic(uint8_t p) {
    float v = params_cymatic[p].value;
    switch (p) {
        case 0: fx_cymatic.setTune(v);            break;
        case 1: fx_cymatic.setDensity((uint8_t)v); break;
        case 2: fx_cymatic.setResonance(v);       break;
        case 3: fx_cymatic.setLfoRate(v);         break;
    }
}

static void apply_param_granular(uint8_t p) {
    float v = params_granular[p].value;
    switch (p) {
        case 0: fx_granular.setGrainSize(v);      break;
        case 1: fx_granular.setSpeed(v);          break;
        case 2: fx_granular.setFreeze((bool)(int)v); break;
        case 3: fx_granular.setMix(v);            break;
    }
}

static void apply_param_spring(uint8_t p) {
    float v = params_spring[p].value;
    switch (p) {
        case 0: fx_spring.setAlgorithm((uint8_t)v); break;
        case 1: fx_spring.setRoomSize(v);            break;
        case 2: fx_spring.setDamping(v);             break;
        case 3: fx_spring.setMix(v);                 break;
    }
}

static void apply_param_sub(uint8_t p) {
    float v = params_sub[p].value;
    switch (p) {
        case 0: fx_sub.setSubLevel(v); break;
        case 1: fx_sub.setDrive(v);    break;
        case 2: fx_sub.setCutoff(v);   break;
        case 3: fx_sub.setMix(v);      break;
    }
}

typedef void (*apply_fn_t)(uint8_t);
static apply_fn_t apply_fns[NUM_FX] = {
    apply_param_ghost,
    apply_param_modal,
    apply_param_chorus,
    apply_param_bit,
    apply_param_tape,
    apply_param_cymatic,
    apply_param_granular,
    apply_param_spring,
    apply_param_sub,
};

// ── Mixer routing ──────────────────────────────────────────────────────────────

// Cada FX va a un slot fijo en los mixers cascade:
//   FX0-3 → mixL_A/R_A ch0-3
//   FX4-7 → mixL_B/R_B ch0-3
//   FX8   → mixL_C/R_C ch0

static void set_mixer_gains(uint8_t active) {
    for (uint8_t i = 0; i < NUM_FX; i++) {
        float gain = (i == active) ? 1.0f : 0.0f;
        if (i < 4) {
            mixL_A.gain(i, gain);
            mixR_A.gain(i, gain);
        } else if (i < 8) {
            mixL_B.gain(i - 4, gain);
            mixR_B.gain(i - 4, gain);
        } else {
            // FX8 → mixL_C/R_C ch0
            mixL_C.gain(0, gain);
            mixR_C.gain(0, gain);
        }
    }
    // Los sub-mixers siempre al 100% cuando hay señal activa en ellos.
    // Si no hay FX activo en A o B, su salida es 0 de todas formas.
    mixL_C.gain(1, 1.0f);
    mixL_C.gain(2, 1.0f);
    mixR_C.gain(1, 1.0f);
    mixR_C.gain(2, 1.0f);
}

// ── Activar FX ─────────────────────────────────────────────────────────────────

static void activate_fx(uint8_t idx) {
    // Bypass todos
    fx_ghost.setBypass(true);
    fx_modal.setBypass(true);
    fx_chorus.setBypass(true);
    fx_bit.setBypass(true);
    fx_tape.setBypass(true);
    fx_cymatic.setBypass(true);
    fx_granular.setBypass(true);
    fx_spring.setBypass(true);
    fx_sub.setBypass(true);

    // Activar el seleccionado
    FxParam* p = fx_desc[idx].params;
    float    mix = g_bypass ? 0.0f : g_wet_dry;

    switch (idx) {
        case 0: fx_ghost.setMix(mix);    fx_ghost.setBypass(g_bypass);    break;
        case 1: fx_modal.setMix(mix);    fx_modal.setBypass(g_bypass);    break;
        case 2: fx_chorus.setMix(mix);   fx_chorus.setBypass(g_bypass);   break;
        case 3: fx_bit.setMix(mix);      fx_bit.setBypass(g_bypass);      break;
        case 4: fx_tape.setMix(mix);     fx_tape.setBypass(g_bypass);     break;
        case 5: fx_cymatic.setMix(mix);  fx_cymatic.setBypass(g_bypass);  break;
        case 6: fx_granular.setMix(mix); fx_granular.setBypass(g_bypass); break;
        case 7: fx_spring.setMix(mix);   fx_spring.setBypass(g_bypass);   break;
        case 8: fx_sub.setMix(mix);      fx_sub.setBypass(g_bypass);      break;
    }
    (void)p; // evitar warning si no se usa directamente

    set_mixer_gains(idx);
    g_active_fx = idx;
}

// ── Bridge helpers ─────────────────────────────────────────────────────────────

/** @brief Envía ENGINE_CHANGED al ESP32 (sin params — uso interno). */
static void bridge_send_fx_changed(uint8_t fx_index, const char* fx_name) {
    GF_Frame f{};
    f.cmd        = GF_CMD_ENGINE_CHANGED;
    f.payload[0] = fx_index;
    size_t name_len = strlen(fx_name);
    if (name_len > 253) name_len = 253;
    memcpy(&f.payload[1], fx_name, name_len + 1);
    f.len = (uint8_t)(1 + name_len + 1);
    f.seq = bridge.next_seq();
    // CRC se calcula internamente en send_async vía gf_frame_crc(f)
    bridge.send_async(f);
    Serial.printf("[BRIDGE] FX_CHANGED %s → ESP32\n", fx_name);
}

static void bridge_send_param_changed(uint8_t fx_index, uint8_t param_index, float value) {
    FxParam& p = fx_desc[fx_index].params[param_index];

    // Todos los params se normalizan a [0,1] — continuo y categórico por igual.
    // El arc del ESP32 siempre opera en 0-100 sin importar el rango original.
    // categorical_str() en el ESP32 usa roundf(val * N_max) para reconstuir el índice.
    // Ej. material 0-3: 0→0.0, 1→0.33, 2→0.67, 3→1.0 → arc 0/33/67/100%.
    float send_val = value;
    float range = p.max_val - p.min_val;
    if (range > 0.0f) {
        send_val = (value - p.min_val) / range;
        if (send_val < 0.0f) send_val = 0.0f;
        if (send_val > 1.0f) send_val = 1.0f;
    }

    GF_Frame f{};
    f.cmd = GF_CMD_PARAM_CHANGED;
    uint16_t param_id = ((uint16_t)fx_index << 8) | param_index;
    memcpy(&f.payload[0], &param_id, 2);
    memcpy(&f.payload[2], &send_val, 4);
    f.len = 6;
    f.seq = bridge.next_seq();
    bridge.send_async(f);
}

/* Variante con param_id raw (para convenciones 0xFF=wet, 0xFE=cursor). */
static void bridge_send_param_raw(uint16_t param_id, float value) {
    GF_Frame f{};
    f.cmd = GF_CMD_PARAM_CHANGED;
    memcpy(&f.payload[0], &param_id, 2);
    memcpy(&f.payload[2], &value,    4);
    f.len = 6;
    f.seq = bridge.next_seq();
    bridge.send_async(f);
}

/**
 * @brief Envía ENGINE_CHANGED + sync sub1/sub2 + todos los params al ESP32.
 *
 * Llamar cada vez que se activa un FX. Sin el resend de params, el display
 * ESP32 muestra 0% en todos los arcos porque la caché arranca vacía.
 *   1) ENGINE_CHANGED  — establece fx_id y nombre en el ESP32
 *   2) 0xF7            — sincroniza assignments sub1/sub2 del FX
 *   3) PARAM_CHANGED × N — rellena la caché con valores actuales normalizados
 */
static void bridge_announce_fx(uint8_t fx_index) {
    // ENGINE_CHANGED
    bridge_send_fx_changed(fx_index, fx_desc[fx_index].name);

    // 0xF7 — sync sub1/sub2 (encoding: fx*100 + sub1*10 + sub2)
    float sync_val = (float)(fx_index * 100 + g_fx_sub1[fx_index] * 10 + g_fx_sub2[fx_index]);
    bridge_send_param_raw(0x00F7, sync_val);

    // Resend de todos los params actuales (normalizados para el arc del ESP32)
    FxDescriptor& d = fx_desc[fx_index];
    for (uint8_t pi = 0; pi < d.num_params; pi++) {
        bridge_send_param_changed(fx_index, pi, d.params[pi].value);
    }
}

// ── Wet/dry update ─────────────────────────────────────────────────────────────

static void apply_wet_dry() {
    float mix = g_bypass ? 0.0f : g_wet_dry;
    switch (g_active_fx) {
        case 0: fx_ghost.setMix(mix);    break;
        case 1: fx_modal.setMix(mix);    break;
        case 2: fx_chorus.setMix(mix);   break;
        case 3: fx_bit.setMix(mix);      break;
        case 4: fx_tape.setMix(mix);     break;
        case 5: fx_cymatic.setMix(mix);  break;
        case 6: fx_granular.setMix(mix); break;
        case 7: fx_spring.setMix(mix);   break;
        case 8: fx_sub.setMix(mix);      break;
    }
}

static void apply_bypass() {
    switch (g_active_fx) {
        case 0: fx_ghost.setBypass(g_bypass);    break;
        case 1: fx_modal.setBypass(g_bypass);    break;
        case 2: fx_chorus.setBypass(g_bypass);   break;
        case 3: fx_bit.setBypass(g_bypass);      break;
        case 4: fx_tape.setBypass(g_bypass);     break;
        case 5: fx_cymatic.setBypass(g_bypass);  break;
        case 6: fx_granular.setBypass(g_bypass); break;
        case 7: fx_spring.setBypass(g_bypass);   break;
        case 8: fx_sub.setBypass(g_bypass);      break;
    }
}

// ── EEPROM presets ─────────────────────────────────────────────────────────────

static uint16_t preset_addr(uint8_t slot) {
    return PRESET_EEPROM_BASE + slot * sizeof(FxPreset);
}

static void save_preset(uint8_t slot) {
    FxPreset pr{};
    pr.magic    = PRESET_MAGIC;
    pr.fx_index = g_active_fx;
    pr.wet_dry  = g_wet_dry;
    pr.bypass   = g_bypass;

    FxDescriptor& d = fx_desc[g_active_fx];
    // Nombre: "FX_NAME"
    snprintf(pr.name, sizeof(pr.name), "%s", d.name);

    uint8_t n = d.num_params < 8 ? d.num_params : 8;
    for (uint8_t i = 0; i < n; i++) pr.params[i] = d.params[i].value;

    uint16_t addr = preset_addr(slot);
    const uint8_t* src = reinterpret_cast<const uint8_t*>(&pr);
    for (size_t i = 0; i < sizeof(FxPreset); i++) {
        EEPROM.update(addr + i, src[i]);
    }
    g_last_saved_slot = (int8_t)slot;
    Serial.printf("[PRESET] saved slot %d: \"%s\"\n", slot, pr.name);
}

static bool load_preset(uint8_t slot) {
    FxPreset pr{};
    uint16_t addr = preset_addr(slot);
    uint8_t* dst  = reinterpret_cast<uint8_t*>(&pr);
    for (size_t i = 0; i < sizeof(FxPreset); i++) {
        dst[i] = EEPROM.read(addr + i);
    }
    if (pr.magic != PRESET_MAGIC) return false;
    if (pr.fx_index >= NUM_FX)    return false;

    g_wet_dry = pr.wet_dry;
    g_bypass  = pr.bypass;

    FxDescriptor& d = fx_desc[pr.fx_index];
    uint8_t n = d.num_params < 8 ? d.num_params : 8;
    for (uint8_t i = 0; i < n; i++) {
        d.params[i].value = pr.params[i];
        apply_fns[pr.fx_index](i);
    }
    activate_fx(pr.fx_index);
    g_fx_cursor = pr.fx_index;
    bridge_send_fx_changed(pr.fx_index, d.name);
    Serial.printf("[PRESET] loaded slot %d: \"%s\"\n", slot, pr.name);
    return true;
}

// ── Print status ───────────────────────────────────────────────────────────────

static void print_fx_select_status() {
    Serial.print("[FX_SELECT] cursor=");
    Serial.print(g_fx_cursor);
    Serial.print(" |");
    for (uint8_t i = 0; i < NUM_FX; i++) {
        if (i == g_active_fx && i == g_fx_cursor) Serial.print(" >>");
        else if (i == g_active_fx)               Serial.print(" *");
        else if (i == g_fx_cursor)               Serial.print(" >");
        else                                      Serial.print(" ");
        Serial.print(fx_desc[i].name);
        if (i < NUM_FX - 1) Serial.print(" |");
    }
    Serial.println();
}

// print_param_edit_status: no-op en el modelo RMX-style (eliminado PARAM_EDIT mode).
// Conservada para evitar errores si hay referencias residuales en tests futuros.
static void print_param_edit_status() {}

// ── Handle encoders ────────────────────────────────────────────────────────────

static void handle_encoders() {
    int32_t delta_nav = encoders.read_enc_nav();
    int32_t delta_l   = encoders.read_enc_l();
    int32_t delta_r   = encoders.read_enc_r();
    bool    l_push    = encoders.sw_l_pressed();
    bool    r_push    = encoders.sw_r_pressed();

    bool nav_down    = encoders.sw_nav_is_down();
    bool nav_push    = nav_down && !g_nav_prev_down;   // rising edge
    bool nav_release = !nav_down && g_nav_prev_down;   // falling edge
    g_nav_prev_down  = nav_down;

    uint32_t now = millis();

    // ── Cancelar AI arm si hay movimiento de encoder ─────────────────────
    if ((delta_nav != 0 || delta_l != 0 || delta_r != 0) && g_ai_arming) {
        g_ai_arming = false;
        bridge_send_param_raw(0x00FB, -1.0f);
        Serial.println("[AI] arm cancelled (encoder moved)");
    }

    // ══════════════════════════════════════════════════════════════════════
    // MODO SYNTH — navegación a 3 niveles (ENGINE_LIST → SUBHOME → GROUP_VIEW)
    // Bug fix: sketch 27 no tenía encoder logic para modo SYNTH; todo encoder
    // input enviaba param_ids de FX que confundían al ESP32.
    // ══════════════════════════════════════════════════════════════════════
    if (g_top_mode == 1) {

        // ── Nivel 0 — ENGINE_LIST ─────────────────────────────────────────
        // ENC NAV gira → mueve cursor entre engines (wrap modular)
        // ENC NAV push → selecciona engine, sube a SUBHOME
        if (g_synth_level == 0) {
            if (delta_nav != 0) {
                int32_t next = ((int32_t)g_engine_cursor + delta_nav
                                + NUM_SYNTH_ENGINES) % NUM_SYNTH_ENGINES;
                g_engine_cursor = (uint8_t)next;
                bridge_send_param_raw(0x00FE, (float)g_engine_cursor);
                Serial.printf("[SYNTH LIST] engine=%u\n", g_engine_cursor);
            }
            if (nav_push) {
                g_synth_level = 1;
                bridge_send_param_raw(0x00F5, 1.0f);    // → SUBHOME
                // Enviar cutoff/resonance actuales para que los arcs carguen con valores reales
                bridge_send_param_raw(0x1020, g_synth_vals[2][0]);
                bridge_send_param_raw(0x1021, g_synth_vals[2][1]);
                Serial.println("[SYNTH] → SUBHOME");
            }
        }

        // ── Nivel 1 — ENGINE_SUBHOME ──────────────────────────────────────
        // ENC L gira → cutoff (grupo FILTER, param 0)
        // ENC R gira → resonance (grupo FILTER, param 1)
        // ENC NAV push → entra a GROUP_VIEW
        // ENC L/R push → vuelve a ENGINE_LIST
        else if (g_synth_level == 1) {
            if (delta_l != 0) {
                float& v = g_synth_vals[2][0];
                v = fmaxf(0.0f, fminf(1.0f, v + delta_l * 0.02f));
                bridge_send_param_raw(0x1020, v);  // FILTER cutoff norm → s_synth_cutoff
                Serial.printf("[SUBHOME] cutoff=%.2f\n", v);
            }
            if (delta_r != 0) {
                float& v = g_synth_vals[2][1];
                v = fmaxf(0.0f, fminf(1.0f, v + delta_r * 0.02f));
                bridge_send_param_raw(0x1021, v);  // FILTER resonance norm → s_synth_resonance
                Serial.printf("[SUBHOME] resonance=%.2f\n", v);
            }
            if (nav_push) {
                g_synth_level = 2;
                // Notificar grupo y cursor al ESP32 antes de cambiar de nivel
                bridge_send_param_raw(0x00F6, (float)(g_active_group * 10 + g_group_cursor));
                bridge_send_param_raw(0x00F5, 2.0f);    // → GROUP_VIEW
                Serial.printf("[SYNTH] → GROUP_VIEW group=%u param=%u\n",
                              g_active_group, g_group_cursor);
            }
            if (l_push || r_push) {
                g_synth_level = 0;
                bridge_send_param_raw(0x00F5, 0.0f);    // → ENGINE_LIST
                Serial.println("[SYNTH] → ENGINE_LIST");
            }
        }

        // ── Nivel 2 — GROUP_VIEW ──────────────────────────────────────────
        // ENC NAV gira → cicla grupos OSC/ENV/FILTER/LFO (wrap modular)
        // ENC L gira → mueve cursor de param dentro del grupo (clamped 0-3)
        // ENC R gira → edita valor del param bajo cursor (norm 0.0-1.0, step 0.02)
        // ENC L/R push → vuelve a SUBHOME
        else {
            if (delta_nav != 0) {
                g_active_group = (uint8_t)((g_active_group + delta_nav + 4) % 4);
                g_group_cursor = 0;
                bridge_send_param_raw(0x00F6, (float)(g_active_group * 10));
                Serial.printf("[GROUP_VIEW] grupo=%u\n", g_active_group);
            }
            if (delta_l != 0) {
                int32_t nc = (int32_t)g_group_cursor + delta_l;
                if (nc < 0) nc = 0;
                if (nc > 3) nc = 3;
                g_group_cursor = (uint8_t)nc;
                bridge_send_param_raw(0x00F6,
                    (float)(g_active_group * 10 + g_group_cursor));
                Serial.printf("[GROUP_VIEW] param=%u\n", g_group_cursor);
            }
            if (delta_r != 0) {
                // Edita el valor del param seleccionado y lo envía al ESP32.
                // param_id encoding (mismo que sketch 28):
                //   (0x10 + engine_id) << 8 | (group << 4) | pidx
                // Para engine 0: 0x1000 | (group<<4) | cursor
                float& v = g_synth_vals[g_active_group][g_group_cursor];
                v = fmaxf(0.0f, fminf(1.0f, v + delta_r * 0.02f));
                uint16_t pid = (uint16_t)(0x1000
                               | ((uint16_t)g_active_group << 4)
                               | g_group_cursor);
                bridge_send_param_raw(pid, v);
                // Si el param es FILTER cutoff/resonance, actualiza también la caché local
                // para que SUBHOME tenga valores coherentes si el usuario vuelve.
                if (g_active_group == 2) {
                    g_synth_vals[2][g_group_cursor] = v;
                }
                Serial.printf("[GROUP_VIEW] grp=%u p=%u val=%.2f\n",
                              g_active_group, g_group_cursor, v);
            }
            if (l_push || r_push) {
                g_synth_level = 1;
                bridge_send_param_raw(0x00F5, 1.0f);    // → SUBHOME
                // Refrescar arcs con valores actuales de FILTER
                bridge_send_param_raw(0x1020, g_synth_vals[2][0]);
                bridge_send_param_raw(0x1021, g_synth_vals[2][1]);
                Serial.println("[SYNTH] → SUBHOME");
            }
        }

        return;  // ← No ejecutar el bloque FX que sigue
    }

    // ══════════════════════════════════════════════════════════════════════
    // MODO FX (g_top_mode == 0) — lógica original
    // ══════════════════════════════════════════════════════════════════════

    // ── ENC NAV gira ──────────────────────────────────────────────────────
    if (delta_nav != 0) {
        if (g_fx_view == FxView::FX_SELECT) {
            // Wrap modular: rotar entre 0..NUM_FX-1 sin clamp
            int32_t next = ((int32_t)g_fx_cursor + delta_nav + NUM_FX) % NUM_FX;
            g_fx_cursor = (uint8_t)next;
            bridge_send_param_raw(0x00FE, (float)g_fx_cursor);
            print_fx_select_status();
        } else {
            // FX_MAIN: ENC NAV controla wet/dry
            g_wet_dry += delta_nav * 0.02f;
            if (g_wet_dry < 0.0f) g_wet_dry = 0.0f;
            if (g_wet_dry > 1.0f) g_wet_dry = 1.0f;
            // Si estaba en bypass y el usuario sube el wet → desactivar bypass.
            // El bypass manual (ENC NAV push) deja wet=0; subir el encoder es
            // la señal intencional de "quiero volver a escuchar el FX".
            if (g_bypass && g_wet_dry > 0.0f) {
                g_bypass = false;
                apply_bypass();
                Serial.println("[BYPASS] auto-off via wet_dry raise");
            }
            apply_wet_dry();
            bridge_send_param_raw(0x00FF, g_wet_dry);
            Serial.printf("[WET_DRY via NAV] %.2f\n", g_wet_dry);
        }
    }

    // ── ENC NAV push ──────────────────────────────────────────────────────
    if (nav_push) {
        if (g_fx_view == FxView::FX_SELECT) {
            // Seleccionar y activar el FX bajo cursor → ir a FX_MAIN
            activate_fx(g_fx_cursor);
            bridge_announce_fx(g_fx_cursor);   // ENGINE_CHANGED + sub1/sub2 sync + params
            g_fx_view = FxView::FX_MAIN;
            bridge_send_param_raw(0x00FF, g_wet_dry);
            Serial.printf("[FX MAIN] activado: %s\n", fx_desc[g_fx_cursor].name);
        } else {
            // FX_MAIN: push = BYPASS toggle
            g_bypass = !g_bypass;
            if (g_bypass) {
                g_wet_dry = 0.0f;
            }
            apply_bypass();
            bridge_send_param_raw(0x00FF, 0.0f);
            Serial.printf("[BYPASS] %s\n", g_bypass ? "ON" : "OFF");
        }
        // Iniciar AI arm solo en FX_MAIN (en FX_SELECT no tiene sentido)
        if (g_fx_view == FxView::FX_MAIN) {
            g_ai_arming    = true;
            g_ai_arm_start = now;
            g_ai_last_prog = now;
        }
    }

    // ── ENC NAV release: cancelar AI arm si soltó antes de 4s ────────────
    if (nav_release && g_ai_arming) {
        uint32_t held = now - g_ai_arm_start;
        if (held < AI_HOLD_MS) {
            g_ai_arming = false;
            bridge_send_param_raw(0x00FB, -1.0f);
            Serial.println("[AI] arm cancelled (released early)");
        }
    }

    // ── AI arm: progreso y disparo ────────────────────────────────────────
    if (g_ai_arming) {
        uint32_t held = now - g_ai_arm_start;
        if (held >= AI_HOLD_MS) {
            g_ai_arming = false;
            g_fx_view   = FxView::FX_SELECT;
            bridge_send_param_raw(0x00FD, 2.0f);  // 2=AI mode
            Serial.println("[AI] mode activated!");
        } else if (held > 200) {
            if ((now - g_ai_last_prog) >= AI_PROG_INTERVAL) {
                float progress = (float)held / (float)AI_HOLD_MS;
                bridge_send_param_raw(0x00FB, progress);
                g_ai_last_prog = now;
            }
        }
    }

    // ── ENC L gira → sub-param 1 del FX activo ───────────────────────────
    if (delta_l != 0) {
        uint8_t pidx = g_fx_sub1[g_active_fx];
        FxParam& p   = fx_desc[g_active_fx].params[pidx];
        p.value += delta_l * p.step;
        if (p.value < p.min_val) p.value = p.min_val;
        if (p.value > p.max_val) p.value = p.max_val;
        apply_fns[g_active_fx](pidx);
        bridge_send_param_changed(g_active_fx, pidx, p.value);
        if (p.is_int)
            Serial.printf("[SUB1] %s: %d\n", p.name, (int)p.value);
        else
            Serial.printf("[SUB1] %s: %.3f\n", p.name, p.value);
    }

    // ── ENC L push → rotar qué parámetro controla sub1 ───────────────────
    if (l_push) {
        uint8_t num = fx_desc[g_active_fx].num_params;
        uint8_t s1  = g_fx_sub1[g_active_fx];
        uint8_t s2  = g_fx_sub2[g_active_fx];
        for (uint8_t i = 0; i < num; i++) {
            s1 = (s1 + 1) % num;
            if (s1 != s2) break;
        }
        g_fx_sub1[g_active_fx] = s1;
        float sync = (float)(g_active_fx * 100 + s1 * 10 + s2);
        bridge_send_param_raw(0x00F7, sync);
        FxParam& p = fx_desc[g_active_fx].params[s1];
        bridge_send_param_changed(g_active_fx, s1, p.value);
        Serial.printf("[SUB1 ROT] FX%u sub1→%u (%s)\n", g_active_fx, s1, p.name);
    }

    // ── ENC R gira → sub-param 2 del FX activo ───────────────────────────
    if (delta_r != 0) {
        uint8_t pidx = g_fx_sub2[g_active_fx];
        FxParam& p   = fx_desc[g_active_fx].params[pidx];
        p.value += delta_r * p.step;
        if (p.value < p.min_val) p.value = p.min_val;
        if (p.value > p.max_val) p.value = p.max_val;
        apply_fns[g_active_fx](pidx);
        bridge_send_param_changed(g_active_fx, pidx, p.value);
        if (p.is_int)
            Serial.printf("[SUB2] %s: %d\n", p.name, (int)p.value);
        else
            Serial.printf("[SUB2] %s: %.3f\n", p.name, p.value);
    }

    // ── ENC R push → rotar qué parámetro controla sub2 ───────────────────
    if (r_push) {
        uint8_t num = fx_desc[g_active_fx].num_params;
        uint8_t s1  = g_fx_sub1[g_active_fx];
        uint8_t s2  = g_fx_sub2[g_active_fx];
        for (uint8_t i = 0; i < num; i++) {
            s2 = (s2 + 1) % num;
            if (s2 != s1) break;
        }
        g_fx_sub2[g_active_fx] = s2;
        float sync = (float)(g_active_fx * 100 + s1 * 10 + s2);
        bridge_send_param_raw(0x00F7, sync);
        FxParam& p = fx_desc[g_active_fx].params[s2];
        bridge_send_param_changed(g_active_fx, s2, p.value);
        Serial.printf("[SUB2 ROT] FX%u sub2→%u (%s)\n", g_active_fx, s2, p.name);
    }
}

// ── Handle buttons ─────────────────────────────────────────────────────────────

static void handle_buttons() {
    uint32_t now = millis();

    // B1 = HOME: comportamiento depende del modo activo
    if (buttons.pressed(0)) {
        g_ai_arming = false;
        if (g_top_mode == 1) {
            // SYNTH mode: reset a ENGINE_LIST (nivel 0)
            g_synth_level = 0;
            bridge_send_param_raw(0x00FA, 0.0f);  // ESP32 → VIEW_IDX_ENGINE_LIST
            bridge_send_param_raw(0x00FE, (float)g_engine_cursor);
            Serial.println("[HOME] SYNTH → ENGINE_LIST");
        } else {
            // FX mode: reset a FX_SELECT
            g_fx_view = FxView::FX_SELECT;
            bridge_send_param_raw(0x00FA, 0.0f);  // ESP32 → VIEW_IDX_FX_SELECT
            bridge_send_param_raw(0x00FE, (float)g_fx_cursor);
            print_fx_select_status();
            Serial.println("[HOME] FX → FX_SELECT");
        }
    }

    // B2 = MODE TOGGLE FX ↔ SYNTH
    if (buttons.pressed(1)) {
        g_top_mode = (g_top_mode + 1) % 2;   // 0=FX, 1=SYNTH
        if (g_top_mode == 1) {
            // Entrando a SYNTH: siempre arranca en ENGINE_LIST
            g_synth_level = 0;
        } else {
            // Volviendo a FX: reset a FX_SELECT
            g_fx_view = FxView::FX_SELECT;
        }
        bridge_send_param_raw(0x00FD, (float)g_top_mode);
        Serial.printf("[MODE] → %s\n", g_top_mode == 0 ? "FX" : "SYNTH");
    }

    // B3 = TAP TEMPO (universal — aplica a Ghost Echo delay_ms si está activo)
    if (buttons.pressed(2)) {
        if (g_tap_armed) {
            uint32_t interval = now - g_last_tap_ms;
            if (interval >= 50 && interval <= 2000) {
                if (g_active_fx == 0) {  // Ghost Echo: param 0 = delay_ms
                    params_ghost[0].value = (float)interval;
                    if (params_ghost[0].value > params_ghost[0].max_val)
                        params_ghost[0].value = params_ghost[0].max_val;
                    fx_ghost.setDelayMs(params_ghost[0].value);
                    bridge_send_param_changed(0, 0, params_ghost[0].value);
                    Serial.printf("[TAP] delay_ms: %.0f\n", params_ghost[0].value);
                }
            }
        }
        g_last_tap_ms = now;
        g_tap_armed   = true;
    }

    // B4 = PANIC: bypass inmediato + wet=0 + señal al ESP32
    if (buttons.pressed(3)) {
        g_bypass  = true;
        g_wet_dry = 0.0f;
        apply_bypass();
        apply_wet_dry();
        bridge_send_param_raw(0x00FF, 0.0f);
        bridge_send_param_raw(0x00F8, 0.0f);   // PANIC signal al ESP32
        Serial.println("[PANIC] bypass=ON wet=0");
    }
}

// ── Call FX update ─────────────────────────────────────────────────────────────

static void call_fx_update(float dt_ms) {
    fx_ghost.update();
    fx_modal.update();
    fx_chorus.update(dt_ms);
    fx_bit.update();
    fx_tape.update(dt_ms);
    fx_cymatic.update();
    fx_granular.update();
    fx_spring.update();
    fx_sub.setBypass(g_active_fx != 8 || g_bypass); // mantiene bypass en no-activos
}

// ── Setup ───────────────────────────────────────────────────────────────────────

void setup() {
    Serial.begin(115200);
    delay(200);
    Serial.println("[SKETCH 27] FX Selector — GrooveForge Brain");

    AudioMemory(90);  // +10 bloques para AudioAnalyzeFFT256

    Wire.begin();
    delay(10);
    codec.enable();
    codec.inputSelect(AUDIO_INPUT_LINEIN);
    codec.lineInLevel(5);
    codec.volume(0.8f);
    codec.lineOutLevel(13);

    // ── Asignar punteros de salida antes de begin() ───────────────────────────

    // Todos los FX se conectan a line_in ch0 (mono in)
    fx_ghost.begin(line_in);
    fx_modal.begin(line_in);
    fx_chorus.begin(line_in);
    fx_bit.begin(line_in);
    fx_tape.begin(line_in);
    fx_cymatic.begin(line_in);
    fx_granular.begin(line_in);
    fx_spring.begin(line_in);
    fx_sub.begin(line_in);

    fx_out_l[0] = &fx_ghost.getOutputL();
    fx_out_r[0] = &fx_ghost.getOutputR();
    fx_out_l[1] = &fx_modal.getOutputL();
    fx_out_r[1] = &fx_modal.getOutputR();
    fx_out_l[2] = &fx_chorus.getOutputL();
    fx_out_r[2] = &fx_chorus.getOutputR();
    fx_out_l[3] = &fx_bit.getOutputL();
    fx_out_r[3] = &fx_bit.getOutputR();
    fx_out_l[4] = &fx_tape.getOutputL();
    fx_out_r[4] = &fx_tape.getOutputR();
    fx_out_l[5] = &fx_cymatic.getOutputL();
    fx_out_r[5] = &fx_cymatic.getOutputR();
    fx_out_l[6] = &fx_granular.getOutputL();
    fx_out_r[6] = &fx_granular.getOutputR();
    fx_out_l[7] = &fx_spring.getOutputL();
    fx_out_r[7] = &fx_spring.getOutputR();
    fx_out_l[8] = &fx_sub.getOutputL();
    fx_out_r[8] = &fx_sub.getOutputR();

    // ── Conectar FX al cascade de mixers ─────────────────────────────────────

    // FX[0..3] → mixL_A / mixR_A
    for (uint8_t i = 0; i < 4; i++) {
        patchL[i] = new AudioConnection(*fx_out_l[i], 0, mixL_A, i);
        patchR[i] = new AudioConnection(*fx_out_r[i], 0, mixR_A, i);
    }
    // FX[4..7] → mixL_B / mixR_B
    for (uint8_t i = 4; i < 8; i++) {
        patchL[i] = new AudioConnection(*fx_out_l[i], 0, mixL_B, i - 4);
        patchR[i] = new AudioConnection(*fx_out_r[i], 0, mixR_B, i - 4);
    }
    // FX[8] → mixL_C / mixR_C ch0
    patchL[8] = new AudioConnection(*fx_out_l[8], 0, mixL_C, 0);
    patchR[8] = new AudioConnection(*fx_out_r[8], 0, mixR_C, 0);

    // ── Activar FX 0 por default, bypass el resto ─────────────────────────────

    activate_fx(0);

    // ── Aplicar todos los params por default ──────────────────────────────────

    for (uint8_t fx = 0; fx < NUM_FX; fx++) {
        for (uint8_t p = 0; p < fx_desc[fx].num_params; p++) {
            apply_fns[fx](p);
        }
    }

    // SubGenesis: frecuencia raíz por default C3
    fx_sub.setRootFreq(130.81f);
    fx_sub.setWaveform(WAVEFORM_SINE);

    // ── UI ────────────────────────────────────────────────────────────────────

    encoders.init();
    buttons.init();

    // ── Bridge ────────────────────────────────────────────────────────────────

    bridge.init();

    // Handler para 0xF7: el ESP32 envía assignment sub1/sub2 actualizado.
    // Encoding: param_id = (fx_id << 8) | (sub1_idx << 4) | sub2_idx
    // BridgeMaster soporta on_command() — se registra aquí para recepción one-way.
    bridge.on_command(GF_CMD_PARAM_CHANGED, [](const GF_Frame* f, void*) {
        if (f->len < 6) return;
        uint16_t param_id;
        memcpy(&param_id, &f->payload[0], 2);
        if ((param_id & 0xFF00) == 0xF700) {
            // 0xF7xx: assignment update del ESP32 → Teensy
            // high byte de payload[0..1]: 0xF7; low byte: (fx_id de la parte alta... )
            // Re-read: param_id completo = 0xF7xx donde xx = (fx_id<<4)|no_usado
            // El encoding real usa el float payload como (fx_id*100 + sub1*10 + sub2)
            // para evitar ambigüedad. TODO Sprint 31: definir encoding final en 02-bridge-protocol.md
            float raw;
            memcpy(&raw, &f->payload[2], 4);
            uint8_t fx_id   = (uint8_t)((int)raw / 100) % NUM_FX;
            uint8_t sub1    = (uint8_t)(((int)raw / 10) % 10) % 4;
            uint8_t sub2    = (uint8_t)((int)raw % 10) % 4;
            g_fx_sub1[fx_id] = sub1;
            g_fx_sub2[fx_id] = sub2;
            Serial.printf("[0xF7] assign FX%u: sub1=%u sub2=%u\n", fx_id, sub1, sub2);
        }
    }, nullptr);

    hb_timer.begin(hb_isr, 1000000UL); // microsegundos — ISR solo setea flag

    // Anunciar FX 0 al ESP32: ENGINE_CHANGED + sub1/sub2 sync + todos los params.
    // Sin esto el display arranca con cache vacío (0% en todos los arcos).
    // Pequeño delay para que el ESP32 termine su boot antes de recibir frames.
    delay(500);
    bridge_announce_fx(0);
    bridge_send_param_raw(0x00FF, g_wet_dry);

    g_last_loop_ms = millis();
    print_fx_select_status();
    Serial.println("[SETUP] done");
}

// ── Loop ────────────────────────────────────────────────────────────────────────

void loop() {
    uint32_t now   = millis();
    float    dt_ms = (float)(now - g_last_loop_ms);
    if (dt_ms < 0.0f) dt_ms = 0.0f;
    g_last_loop_ms = now;

    bridge.poll();
    encoders.update();
    buttons.update();

    // Heartbeat: flag seteado por ISR → despacho en loop() para evitar
    // llamar Serial1.write() desde ISR context.
    if (g_hb_flag) {
        g_hb_flag = false;
        bridge.send_heartbeat();
    }

    handle_encoders();
    handle_buttons();
    call_fx_update(dt_ms);

    // Status periódico cada 5s
    if (now - g_last_status_ms >= STATUS_PRINT_MS) {
        g_last_status_ms = now;
        print_fx_select_status();
        Serial.printf("[CPU] %.1f%%\n", AudioProcessorUsageMax());
        AudioProcessorUsageMaxReset();
    }

    // ── Spectrum analyzer — 8 bandas → ESP32 a ~20fps ─────────────────────────
    // Bin size a 44100Hz: 44100/256 ≈ 172 Hz/bin
    // Rangos de bandas (índices de bin inclusivos):
    //   0: 1-2   (172-516 Hz)    sub/bass
    //   1: 3-4   (516-860 Hz)    bass
    //   2: 5-7   (860-1376 Hz)   low-mid
    //   3: 8-12  (1376-2236 Hz)  mid
    //   4: 13-20 (2236-3612 Hz)  upper-mid
    //   5: 21-35 (3612-6116 Hz)  presence
    //   6: 36-60 (6116-10404 Hz) air
    //   7: 61-100(10404-17244 Hz)brilliance
    static const uint8_t BAND_LO[8] = { 1,  3,  5,  8, 13, 21, 36,  61};
    static const uint8_t BAND_HI[8] = { 2,  4,  7, 12, 20, 35, 60, 100};
    static float s_bands_smooth[8]  = {};
    static uint32_t g_last_spec_ms  = 0;

    if ((millis() - g_last_spec_ms) >= 50 && fft_analyze.available()) {
        g_last_spec_ms = millis();
        constexpr float ALPHA = 0.5f;   // smoothing IIR: 0=muy suave, 1=raw
        for (uint8_t b = 0; b < 8; b++) {
            float raw = fft_analyze.read(BAND_LO[b], BAND_HI[b]);
            /* Escalar: la señal real no llega a 1.0 fácilmente. Factor ×3 para
             * que niveles moderados (−12 dBFS) llenen las barras a ~70%.        */
            raw = raw * 3.0f;
            if (raw > 1.0f) raw = 1.0f;
            s_bands_smooth[b] = ALPHA * raw + (1.0f - ALPHA) * s_bands_smooth[b];
            bridge_send_param_raw((uint16_t)(0x00E0 + b), s_bands_smooth[b]);
        }
    }
}
