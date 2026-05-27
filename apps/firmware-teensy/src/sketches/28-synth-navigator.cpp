// GrooveForge Brain — Sketch 28: Synth Navigator
// Sprint 31 Batch B — Navegación de 3 niveles en modo SYNTH con Bridge Protocol
//
// Jerarquía de pantallas:
//   Nivel 0  ENGINE_LIST  — seleccionar engine (Moog / Juno / Prophet)
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
#include "../ui/encoders.h"
#include "../ui/buttons.h"
#include "../usb/midi_host.h"
#include "../bridge/bridge_master.h"
#include <protocol.h>

// ── Constantes ──────────────────────────────────────────────────────────────────

static constexpr uint8_t  NUM_ENGINES          = 3;
static constexpr uint32_t AI_HOLD_MS           = 4000;
static constexpr uint32_t AI_PROG_INTERVAL     = 100;
static constexpr uint32_t STATUS_PRINT_MS      = 5000;

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
    SynthParam  params[4];
    uint8_t     num_params;
};

struct EngineDesc {
    const char* name;
    GroupDesc   groups[4];   // 0=OSC, 1=ENV, 2=FILTER, 3=LFO
};

// ──────────────────────────────────────────────────────────────────────────────
// Moog Model D
// OSC: 4 params, ENV: 4, FILTER: 2, LFO: 2
// ──────────────────────────────────────────────────────────────────────────────

static EngineDesc g_desc[NUM_ENGINES] = {
    // ── 0: Moog Model D ─────────────────────────────────────────────────────
    {
        "Moog Model D",
        {
            { // OSC
                "OSC",
                {
                    {"wave",   0,  0,  0,    3,    1,    true},
                    {"oct",    0,  0, -2,    2,    1,    true},
                    {"wave2",  1,  1,  0,    3,    1,    true},
                    {"detune", 5,  5,  0,   50,    1,    false},
                },
                4
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
};

// ── Engines de audio ────────────────────────────────────────────────────────────
// TODO: Juno106 y Prophet5 tienen su propio AudioOutputI2S y AudioControlSGTL5000
// internamente. El Teensy 4.1 solo puede tener UNA instancia activa de AudioOutputI2S
// (el grafo de audio comparte el DMA único del I2S). Activar más de uno causa
// conflicto en el codec y undefined behavior en el DMA.
// Solución para producción (Sprint 32+): refactorizar los engines para recibir
// un AudioOutputI2S y AudioControlSGTL5000 externos como referencia en begin(),
// compartiendo el mismo periférico. Por ahora solo moog produce audio.
// Juno y Prophet están declarados pero NO se llama begin() en ellos.
MoogModelD  moog;
Juno106     juno;    // TODO: activar cuando engines usen I2S externo compartido
Prophet5    prophet; // TODO: ídem

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
static uint8_t  g_active_engine  = 0;     // 0=Moog, 1=Juno, 2=Prophet
static uint8_t  g_synth_level    = 0;     // 0=ENGINE_LIST, 1=SUBHOME, 2=GROUP_VIEW
static uint8_t  g_active_group   = 0;     // 0=OSC, 1=ENV, 2=FILTER, 3=LFO
// Cursor de param seleccionado por [engine][group] para recordar posición al volver
static uint8_t  g_group_cursor[NUM_ENGINES][4] = {};

static bool     g_scale_lock_bypass    = false;
static bool     g_beat_follower_bypass = false;
// g_in_ai_mode: true cuando view_10 está activa.
// Se activa cuando el arm completa (0xFD=2.0 enviado), se desactiva con B1 (HOME).
static bool     g_in_ai_mode          = false;

// AI arm — hold ENC NAV 4s para activar pantalla AI processing
static bool     g_ai_arming    = false;
static uint32_t g_ai_arm_start = 0;
static uint32_t g_ai_last_prog = 0;

// Edge detection de NAV para detectar press/release por separado
static bool     g_nav_prev_down = false;

// Tap tempo
static uint32_t g_last_tap_ms  = 0;
static bool     g_tap_armed    = false;

// Status periódico
static uint32_t g_last_status_ms = 0;

// Nota MIDI activa para Prophet (polyphonic — debe pasarse a noteOff)
static uint8_t  g_last_midi_note = 255;

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
 * @param engine  Índice de engine (0=Moog, 1=Juno, 2=Prophet)
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

// ── MIDI callbacks ──────────────────────────────────────────────────────────────

// Callbacks con linkage C para que MidiHost los registre via puntero a función.
// La state machine de audio está en el engine activo — todos los demás ignoran las
// notas incluso si reciben calls (sus VCA envelopes no están iniciados).

static void midi_note_on(uint8_t ch, uint8_t note, uint8_t vel) {
    (void)ch;
    if (vel == 0) {
        // velocity 0 en NoteOn es equivalente a NoteOff según spec MIDI 1.0
        if (g_active_engine == 2) prophet.noteOff(note);
        else if (g_active_engine == 1) juno.noteOff();
        else                           moog.noteOff();
        return;
    }
    float velocity_norm = vel / 127.0f;

    // Octave offset del Moog OSC param 1 (oct): transportar la nota MIDI
    // sumando el offset de octava del descriptor antes de pasarla al engine.
    // Solo aplica al engine activo.
    int oct_offset = 0;
    if (g_active_engine == 0) {
        oct_offset = (int)g_desc[0].groups[0].params[1].value; // param "oct"
    }
    int transposed = (int)note + oct_offset * 12;
    if (transposed < 0)   transposed = 0;
    if (transposed > 127) transposed = 127;
    uint8_t tnote = (uint8_t)transposed;

    g_last_midi_note = note; // guardar nota original para el noteOff del Prophet

    switch (g_active_engine) {
    case 0: moog.noteOn(tnote, velocity_norm);    break;
    case 1: juno.noteOn(tnote, velocity_norm);    break;
    case 2: prophet.noteOn(tnote, velocity_norm); break;
    }
}

static void midi_note_off(uint8_t ch, uint8_t note, uint8_t vel) {
    (void)ch; (void)vel;
    switch (g_active_engine) {
    case 0: moog.noteOff();         break;
    case 1: juno.noteOff();         break;
    case 2: prophet.noteOff(note);  break;
    }
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
    moog.noteOff();
    juno.noteOff();
    // Prophet: silenciar todas las notas posibles (voice stealing interno)
    // No hay método allNotesOff() — noteOff con nota inválida es no-op.
    // Enviamos noteOff con la última nota registrada para liberar voces activas.
    if (g_last_midi_note <= 127) {
        for (uint8_t n = 0; n <= 127; n++) {
            prophet.noteOff(n);
        }
    }
}

// ── Activar engine ──────────────────────────────────────────────────────────────

/**
 * @brief Silencia todos los engines y anuncia el nuevo engine activo al ESP32.
 *
 * No llama begin() en Juno o Prophet porque tendrían conflicto de AudioOutputI2S.
 * El audio siempre sale por Moog en esta iteración del sprint.
 *
 * @param engine  Índice del engine a activar
 */
static void activate_engine(uint8_t engine) {
    // Silenciar todos antes de cambiar para evitar notas colgadas
    all_engines_panic();

    g_active_engine = engine;
    g_active_group  = 0;
    // Resetear cursores de grupo del engine nuevo
    // (los del anterior se conservan para cuando vuelva)

    Serial.printf("[ENGINE] activado: %s\n", g_desc[engine].name);
    bridge_announce_engine(engine);
}

// ── Bridge — handle encoders ────────────────────────────────────────────────────

static void handle_encoders() {
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

    // ── Bypass de modo AI (prioridad máxima — consume el push antes de la navegación normal) ──
    if (l_push && g_in_ai_mode) {
        g_scale_lock_bypass = !g_scale_lock_bypass;
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

    // Cancelar AI arm si cualquier encoder se mueve
    if ((delta_nav != 0 || delta_l != 0 || delta_r != 0) && g_ai_arming) {
        g_ai_arming = false;
        bridge_send_param_raw(0x00FB, -1.0f);
        Serial.println("[AI] arm cancelado (encoder)");
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

            // Iniciar AI arm en nivel SUBHOME
            g_ai_arming    = true;
            g_ai_arm_start = now;
            g_ai_last_prog = now;
        }

    // ──────────────────────────────────────────────────────────────────────────
    // Nivel 1: ENGINE_SUBHOME
    // ENC L gira → adjust FILTER.cutoff
    // ENC R gira → adjust FILTER.resonance
    // ENC NAV push → ir a GROUP_VIEW
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
            // Enviar nivel y grupo+cursor actual para que el display sepa qué mostrar
            bridge_send_param_raw(0x00F5, 2.0f);
            float gc_encoded = (float)((int)g_active_group * 10
                                       + g_group_cursor[g_active_engine][g_active_group]);
            bridge_send_param_raw(0x00F6, gc_encoded);
            Serial.println("[GROUP_VIEW] entró");

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
    // Nivel 2: GROUP_VIEW
    // ENC NAV gira → cicla entre grupos (OSC/ENV/FILTER/LFO, wrap)
    // ENC L gira → navega sub-params dentro del grupo
    // ENC R gira → cambia valor del param seleccionado
    // ENC L push / ENC R push → volver a SUBHOME
    // ──────────────────────────────────────────────────────────────────────────
    } else { // g_synth_level == 2

        if (delta_nav != 0) {
            g_active_group = (uint8_t)((g_active_group + delta_nav + 4) % 4);
            float gc_encoded = (float)((int)g_active_group * 10
                                       + g_group_cursor[g_active_engine][g_active_group]);
            bridge_send_param_raw(0x00F6, gc_encoded);
            Serial.printf("[GROUP_VIEW] grupo=%u (%s)\n",
                          g_active_group, g_desc[g_active_engine].groups[g_active_group].name);
        }

        if (delta_l != 0) {
            GroupDesc& gd  = g_desc[g_active_engine].groups[g_active_group];
            uint8_t&   cur = g_group_cursor[g_active_engine][g_active_group];
            // clamp — no wrap para cursor de param (el usuario siente el límite)
            int32_t next = (int32_t)cur + delta_l;
            if (next < 0)              next = 0;
            if (next >= gd.num_params) next = gd.num_params - 1;
            cur = (uint8_t)next;
            float gc_encoded = (float)((int)g_active_group * 10 + cur);
            bridge_send_param_raw(0x00F6, gc_encoded);
            Serial.printf("[GROUP_VIEW] param=%u (%s)\n",
                          cur, gd.params[cur].name);
        }

        if (delta_r != 0) {
            uint8_t pidx   = g_group_cursor[g_active_engine][g_active_group];
            SynthParam& p  = g_desc[g_active_engine].groups[g_active_group].params[pidx];
            p.value = clampf(p.value + delta_r * p.step, p.min_val, p.max_val);
            apply_param(g_active_engine, g_active_group, pidx);
            bridge_send_synth_param(g_active_engine, g_active_group, pidx);
            if (p.is_int)
                Serial.printf("[PARAM] %s = %d\n", p.name, (int)p.value);
            else
                Serial.printf("[PARAM] %s = %.3f\n", p.name, p.value);
        }

        if (l_push || r_push) {
            g_synth_level = 1;
            bridge_send_param_raw(0x00F5, 1.0f);
            Serial.println("[SUBHOME] volvió desde GROUP_VIEW");
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
            Serial.println("[AI] activado!");
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

    // B1 = HOME: volver a ENGINE_LIST, cancelar AI arm y salir de modo AI
    if (buttons.pressed(0)) {
        g_synth_level = 0;
        g_ai_arming   = false;
        g_in_ai_mode  = false;
        bridge_send_param_raw(0x00F5, 0.0f);
        bridge_send_param_raw(0x00FE, (float)g_engine_cursor);
        bridge_send_param_raw(0x00FB, -1.0f);  // cancelar progress bar en ESP32
        Serial.println("[HOME] → ENGINE_LIST");
    }

    // B2 = MODE TOGGLE SYNTH → FX: envía 0xFD=0 al ESP32 (FX mode).
    // En sketch 28 solo existe modo SYNTH, así que B2 siempre manda de vuelta a FX.
    // El Teensy se mantiene en sketch 28 (audio SYNTH sigue activo), pero el
    // ESP32 cambia su carrusel a FX_SELECT. En producción main.cpp coordinará ambos.
    if (buttons.pressed(1)) {
        bridge_send_param_raw(0x00FD, 0.0f);  // 0 = FX mode
        Serial.println("[B2] MODE → FX (ESP32 display)");
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
    }

    // B4 = PANIC — silenciar todos los engines
    if (buttons.pressed(3)) {
        all_engines_panic();
        bridge_send_param_raw(0x00F8, 0.0f);  // señal panic al ESP32
        Serial.println("[PANIC] all engines silenciados");
    }
}

// ── Handle heartbeat ────────────────────────────────────────────────────────────

static void handle_heartbeat() {
    if (g_hb_flag) {
        g_hb_flag = false;
        bridge.send_heartbeat();
    }
}

// ── Setup ────────────────────────────────────────────────────────────────────────

void setup() {
    Serial.begin(115200);
    delay(200);
    Serial.println("[SKETCH 28] Synth Navigator — GrooveForge Brain");

    // AudioMemory: MoogModelD llama AudioMemory(20) internamente en begin().
    // Agregamos 20 bloques extra para el overhead de la navegación y el bridge.
    // Total: 40 bloques ≈ 10.2KB RAM (budget: 400KB).
    AudioMemory(40);

    // Solo activar Moog — ver TODO arriba sobre conflicto de AudioOutputI2S.
    moog.begin(0.5f);

    // Aplicar todos los parámetros por defecto al Moog
    for (uint8_t g = 0; g < 4; g++) {
        for (uint8_t p = 0; p < g_desc[0].groups[g].num_params; p++) {
            apply_param(0, g, p);
        }
    }

    // UI
    encoders.init();
    buttons.init();

    // MIDI USB-A host — registrar callbacks antes de init()
    midi_host.on_note_on(midi_note_on);
    midi_host.on_note_off(midi_note_off);
    midi_host.on_cc(midi_cc);
    midi_host.init();

    // Bridge
    bridge.init();
    hb_timer.begin(hb_isr, 1000000UL); // 1Hz — ISR solo setea flag

    // Dar tiempo al ESP32 para completar su boot antes de recibir frames
    delay(500);

    // Anunciar modo SYNTH y engine inicial al ESP32
    bridge_send_param_raw(0x00F4, 1.0f);   // top_mode = SYNTH
    bridge_announce_engine(0);             // Moog es el engine por defecto
    bridge_send_param_raw(0x00F2, 0.0f);  // scale lock activo por defecto
    bridge_send_param_raw(0x00F3, 0.0f);  // beat follower activo por defecto

    g_last_status_ms = millis();
    Serial.println("[SETUP] done");
}

// ── Loop ─────────────────────────────────────────────────────────────────────────

void loop() {
    bridge.poll();
    handle_heartbeat();
    midi_host.poll();

    encoders.update();
    buttons.update();

    moog.update();
    // Juno y Prophet: update() no genera audio si begin() no se llamó, pero
    // no es seguro llamarlos en este estado — sus objetos de audio internos
    // no están inicializados. Solo moog.update() está habilitado.
    // TODO Sprint 32: llamar update() en el engine activo únicamente.

    handle_encoders();
    handle_buttons();

    // Status periódico cada 5s
    uint32_t now = millis();
    if (now - g_last_status_ms >= STATUS_PRINT_MS) {
        g_last_status_ms = now;
        Serial.printf("[STATUS] engine=%s level=%u group=%u cpu=%.1f%%\n",
                      g_desc[g_active_engine].name,
                      g_synth_level,
                      g_active_group,
                      AudioProcessorUsageMax());
        AudioProcessorUsageMaxReset();
        Serial.printf("[MIDI] connected=%s\n",
                      midi_host.is_connected() ? "yes" : "no");
    }
}
