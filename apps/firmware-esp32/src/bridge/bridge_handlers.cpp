/**
 * @file bridge_handlers.cpp
 * @brief Handlers de comandos Bridge — ESP32-S3 (Sprint 3.2)
 *
 * Implementa los callbacks despachados por BridgeSlave::dispatch_frame().
 * Cada handler recibe el frame completo y el puntero de contexto (BridgeSlave*).
 */

#include "bridge_handlers.h"
#include "../display/carousel/carousel.h"
#include <Arduino.h>
#include <string.h>
#include "lvgl.h"

/* ── Estado interno del bridge (visible via getters públicos) ────────────── */

/* ── Tabla de nombres de parámetros (espejo de fx_desc[] del sketch 27) ──── */

static const char* const FX_PARAM_NAMES[9][4] = {
    {"delay ms",  "feedback",  "tape lp",   "mix"},       // 0 Ghost Echo
    {"material",  "size",      "decay",     "mix"},       // 1 Modal Reverb
    {"rate",      "depth",     "drift",     "voices"},    // 2 Phase Chorus
    {"bits",      "srate",     "sculpt",    "mix"},       // 3 Bit Sculpt
    {"drive",     "wow",       "flutter",   "age"},       // 4 Tape Saturate
    {"tune",      "density",   "resonance", "lfo rate"},  // 5 Cymatic Res
    {"grain ms",  "speed",     "freeze",    "mix"},       // 6 Granular Cloud
    {"algorithm", "room",      "damp",      "mix"},       // 7 Spring Plate
    {"sub level", "drive",     "cutoff",    "mix"},       // 8 Sub Genesis
};
static const uint8_t FX_PARAM_COUNT[9] = {4, 4, 4, 4, 4, 4, 4, 4, 4};

/* ── Estado interno del bridge ────────────────────────────────────────────── */

static char    s_engine_name[32]   = "---";
static uint8_t s_engine_id         = 0xFF;
static bool    s_cloud_connected   = false;
static float   s_wet_dry           = 0.0f;   // param_id=0xFF
static uint8_t s_fx_cursor         = 0;      // param_id=0xFE — cursor FX_SELECT
static uint8_t s_param_cursor      = 0xFF;   // param_id=0xFC — 0xFF=FX_MAIN, 0-7=PARAM_EDIT
static float   s_param_value       = 0.0f;   // último valor de parámetro recibido
static float   s_spectrum[8]       = {};      // bandas 0-7, actualizado a ~20fps
static uint8_t s_top_mode          = 0;      // 0=FX, 1=SYNTH, 2=AI (param_id=0xFD)
static float   s_ai_progress       = -1.0f;  // 0.0-1.0 durante arm; -1=inactivo (0xFB)

/* Bypass de modelos AI (Sprint 31 Batch E+F) */
static bool    s_scale_lock_bypass    = false;
static bool    s_beat_follower_bypass = false;
static uint8_t s_chromagram_ordering  = 1;    ///< 0=cromático, 1=CoF (default)

/* Bypass del FX activo — param_id=0xFA. Controla visual del arco wet en view_07. */
static bool    s_fx_bypass            = false;

/* Manual scale override (Sprint 34 Batch J — param_id 0x00EF) */
static bool    s_scale_manual_mode = false;   ///< true = usuario fijó escala en view_16
static uint8_t s_scale_manual_key  = 0xFF;    ///< 0-23 key_idx; 0xFF = AUTO

/* AI Rack state (Sprint 45 — param_ids 0x00E9-0x00EE) */
static uint8_t s_ai_rack_cursor    = 0;      ///< fila activa (0-5)
static bool    s_auto_harm_enabled = false;  ///< Auto-Harmonize ON/OFF
static uint8_t s_auto_harm_interval = 0;    ///< 0=THIRD, 1=SIXTH (HarmonyInterval enum)
static uint8_t s_arp_mode          = 4;     ///< SmartArp::Mode (4=SMART default)
static uint8_t s_arp_division      = 1;     ///< SmartArp::Division (1=EIGHTH default)
static uint8_t s_groove_style      = 1;     ///< GrooveHumanizer::Style (1=HUMAN default)
static float   s_groove_amount     = 0.5f;  ///< intensidad 0.0-0.99

/* Resultados de inferencia ML (Sprint 32 — GF_CMD_KEY/CHORD/BEAT_DETECTED) */
static char     s_ai_key_name[12]   = "---";  ///< "C MAJ" / "F# MIN" / "---"
static char     s_ai_chord_name[12] = "---";  ///< "Am7" / "C" / "---"
static uint16_t s_ai_bpm            = 0;      ///< BPM entero; 0 = desconocido
/* Raw indices para view_10 chromagram + chord triad (Sprint 33) */
static uint8_t  s_ai_key_idx        = 0xFF;   ///< 0-23; 0xFF = no detectado
static uint8_t  s_ai_chord_root     = 0xFF;   ///< 0-11; 0xFF = no detectado
static uint8_t  s_ai_chord_quality  = 0xFF;   ///< 0-5; 0xFF = no detectado

/* Estado GROOVE_STATE live (Sprint 33 — GF_CMD_GROOVE_STATE 0x83) */
static uint8_t  s_pitch_activity[12]         = {0};  ///< EMA tau=2s, 0-255 por pitch class
static uint8_t  s_last_snap_from             = 0;
static uint8_t  s_last_snap_to               = 0;
static uint32_t s_last_snap_ms               = 0;    ///< millis() del último snap; 0 = nunca
static uint16_t s_snapped_count              = 0;    ///< notas cuantizadas desde entrada a AI mode
static uint16_t s_total_count               = 0;    ///< notas tocadas en AI mode (via NOTE_ON)
static uint8_t  s_beat_phase                = 0;
static uint32_t s_last_groove_state_log_ms  = 0;    ///< throttle de Serial logging (1Hz)

/* Navegación SYNTH (Sprint 31) */
static uint8_t s_synth_level    = 0;   // 0=ENGINE_LIST, 1=SUBHOME, 2=GROUP_VIEW
static uint8_t s_synth_group    = 0;   // 0=OSC, 1=ENV, 2=FILTER, 3=LFO
static uint8_t s_synth_param    = 0;   // cursor dentro del grupo
static uint8_t s_engine_cursor  = 0;   // cursor en ENGINE_LIST
static float   s_synth_cutoff     = 0.5f;   // último cutoff recibido (grupo FILTER param 0)
static float   s_synth_resonance  = 0.1f;   // último resonance recibido (grupo FILTER param 1)

/* MIDI note state — capturado por on_note_on / on_note_off (Sprint 31)
 * Usado por view_02 para animar el osciloscopio según la nota presionada. */
static uint8_t s_note_midi     = 69;    ///< número MIDI (0-127); default A4
static float   s_note_velocity = 0.0f;  ///< velocidad normalizada [0,1]; 0 = sin nota
static bool    s_note_active   = false; ///< true mientras la nota está presionada

/* Caché completa de parámetros synth: [engine 0-5][grupo 0-3][param 0-5]
 * Poblada por cada PARAM_CHANGED con byte-alto 0x10-0x15.
 * Permite a las vistas GROUP_VIEW inicializar todos los valores correctamente.
 * Sprint 38: expandido de [3] a [6] engines para OB-6/DX7/ARP.
 * Sprint 41: expandido de [4] a [6] params para VCO3 del Moog (params 4-5 en OSC). */
static float   s_synth_cache[6][4][6] = {};

/* Caché de valores por FX y parámetro */
static float   s_param_cache[9][4]  = {};

/* Asignaciones sub1/sub2 por FX — defaults del sketch 27 */
static uint8_t s_sub1[9]            = {0, 0, 0, 0, 0, 0, 0, 0, 0};
static uint8_t s_sub2[9]            = {1, 2, 1, 1, 1, 2, 1, 1, 1};

/* Tracking del último PARAM_CHANGED real (excluye especiales y spectrum) */
static uint16_t s_last_real_param_id = 0xFFFF;
static uint32_t s_last_real_param_ms = 0;

const char* bridge_get_engine_name(void)    { return s_engine_name; }
uint8_t     bridge_get_engine_id(void)      { return s_engine_id; }
bool        bridge_get_cloud_connected(void){ return s_cloud_connected; }
float       bridge_get_wet_dry(void)        { return s_wet_dry; }
uint8_t     bridge_get_fx_cursor(void)      { return s_fx_cursor; }
uint8_t     bridge_get_param_cursor(void)   { return s_param_cursor; }
float       bridge_get_param_value(void)    { return s_param_value; }

const char* bridge_get_param_name(uint8_t fx, uint8_t param) {
    if (fx >= 9 || param >= 4) return "???";
    return FX_PARAM_NAMES[fx][param];
}

float   bridge_get_spectrum(uint8_t band)  { if (band >= 8) return 0.0f; return s_spectrum[band]; }
uint8_t bridge_get_top_mode(void)          { return s_top_mode; }
float   bridge_get_ai_progress(void)       { return s_ai_progress; }

float   bridge_get_param_cached(uint8_t fx, uint8_t param) {
    if (fx >= 9 || param >= 4) return 0.0f;
    return s_param_cache[fx][param];
}
uint8_t  bridge_get_sub1(uint8_t fx)            { return (fx < 9) ? s_sub1[fx] : 0; }
uint8_t  bridge_get_sub2(uint8_t fx)            { return (fx < 9) ? s_sub2[fx] : 1; }
uint16_t bridge_get_last_real_param_id(void)    { return s_last_real_param_id; }
uint32_t bridge_get_last_real_param_ms(void)    { return s_last_real_param_ms; }

bool    bridge_get_scale_lock_bypass(void)    { return s_scale_lock_bypass; }
bool    bridge_get_beat_follower_bypass(void) { return s_beat_follower_bypass; }
uint8_t bridge_get_chromagram_ordering(void)  { return s_chromagram_ordering; }
bool    bridge_get_scale_manual_mode(void)    { return s_scale_manual_mode; }
uint8_t bridge_get_scale_manual_key(void)     { return s_scale_manual_key; }
bool    bridge_get_fx_bypass(void)            { return s_fx_bypass; }

/* AI Rack getters (Sprint 45) */
uint8_t bridge_get_ai_rack_cursor(void)     { return s_ai_rack_cursor; }
bool    bridge_get_auto_harm_enabled(void)  { return s_auto_harm_enabled; }
uint8_t bridge_get_auto_harm_interval(void) { return s_auto_harm_interval; }
uint8_t bridge_get_arp_mode(void)           { return s_arp_mode; }
uint8_t bridge_get_arp_division(void)       { return s_arp_division; }
uint8_t bridge_get_groove_style(void)       { return s_groove_style; }
float   bridge_get_groove_amount(void)      { return s_groove_amount; }

const char* bridge_get_ai_key_name(void)    { return s_ai_key_name; }
const char* bridge_get_ai_chord_name(void)  { return s_ai_chord_name; }
uint16_t    bridge_get_ai_bpm(void)         { return s_ai_bpm; }
uint8_t     bridge_get_ai_key_idx(void)     { return s_ai_key_idx; }
uint8_t     bridge_get_ai_chord_root(void)  { return s_ai_chord_root; }
uint8_t     bridge_get_ai_chord_quality(void) { return s_ai_chord_quality; }

uint8_t bridge_get_synth_level(void)    { return s_synth_level; }
uint8_t bridge_get_synth_group(void)    { return s_synth_group; }
uint8_t bridge_get_synth_param(void)    { return s_synth_param; }
uint8_t bridge_get_engine_cursor(void)  { return s_engine_cursor; }
float   bridge_get_synth_cutoff(void)   { return s_synth_cutoff; }
float   bridge_get_synth_resonance(void){ return s_synth_resonance; }
uint8_t bridge_get_note_midi(void)      { return s_note_midi; }
float   bridge_get_note_velocity(void)  { return s_note_velocity; }
bool    bridge_get_note_active(void)    { return s_note_active; }

float bridge_get_synth_param_cached(uint8_t engine, uint8_t group, uint8_t pidx) {
    if (engine >= 6 || group >= 4 || pidx >= 6) return 0.0f;
    return s_synth_cache[engine][group][pidx];
}

/* ── Helpers ─────────────────────────────────────────────────────────────── */

static void auto_ack(const GF_Frame* f, BridgeSlave* slave) {
    slave->send_ack(f->seq);
}

/* ── Handlers MIDI (Sprint 31 — osciloscope reactivo) ────────────────────── */

/**
 * NOTE_ON: payload [note:1B, velocity:1B, channel:1B]
 * Almacena nota y velocity para que view_02 anime el osciloscopio.
 * Attack instantáneo: el envelope lo sube la vista en el siguiente tick.
 */
static void on_note_on(const GF_Frame* f, void* ctx) {
    BridgeSlave* slave = static_cast<BridgeSlave*>(ctx);
    if (f->len >= 2) {
        s_note_midi     = f->payload[0];
        s_note_velocity = (float)f->payload[1] / 127.0f;
        s_note_active   = true;
        /* Contar notas en AI mode para la estadística snap_stats.
         * s_total_count se resetea al salir de AI mode (handler 0xFD/0x00F4). */
        if (s_top_mode == 2) {
            s_total_count++;
        }
    }
    slave->send_ack(f->seq);
}

/**
 * NOTE_OFF: payload [note:1B, channel:1B]
 * Solo silencia si es la nota activa (simplificación: sin polyphony tracking).
 * El decay del envelope lo gestiona view_02 de forma suave.
 */
static void on_note_off(const GF_Frame* f, void* ctx) {
    BridgeSlave* slave = static_cast<BridgeSlave*>(ctx);
    if (f->len >= 1 && f->payload[0] == s_note_midi) {
        s_note_active = false;
    }
    slave->send_ack(f->seq);
}

/* ── Handlers ────────────────────────────────────────────────────────────── */

static void on_heartbeat(const GF_Frame* f, void* ctx) {
    BridgeSlave* slave = static_cast<BridgeSlave*>(ctx);
    slave->mark_heartbeat();
    slave->send_ack(f->seq);
}

static void on_get_version(const GF_Frame* f, void* ctx) {
    BridgeSlave* slave = static_cast<BridgeSlave*>(ctx);
    /* VERSION: [major:1B, minor:1B, patch:1B, build:4B little-endian] */
    uint8_t payload[7] = { 1, 0, 0, 0x00, 0x00, 0x00, 0x00 };
    GF_Frame resp;
    gf_frame_build(&resp, GF_CMD_VERSION, f->seq, payload, sizeof(payload));
    slave->send(resp);
}

static void on_engine_changed(const GF_Frame* f, void* ctx) {
    BridgeSlave* slave = static_cast<BridgeSlave*>(ctx);
    if (f->len < 1) {
        slave->send_nack(f->seq, GF_NACK_INVALID_LEN);
        return;
    }
    s_engine_id = f->payload[0];
    if (f->len > 1) {
        /* payload[1..] es el nombre null-terminated */
        uint8_t name_len = f->len - 1;
        if (name_len >= sizeof(s_engine_name)) name_len = sizeof(s_engine_name) - 1;
        memcpy(s_engine_name, f->payload + 1, name_len);
        s_engine_name[name_len] = '\0';
    } else {
        snprintf(s_engine_name, sizeof(s_engine_name), "ENGINE_%02X", s_engine_id);
    }
    Serial.printf("[bridge] ENGINE_CHANGED — id=%u name=%s\n", s_engine_id, s_engine_name);
    Serial.flush();
    slave->send_ack(f->seq);

    /* Primera vez que el Teensy manda ENGINE_CHANGED: pausar el demo carousel.
     * Rutear al sub-home del modo activo — en SYNTH al arc-view, en FX al FX MAIN. */
    carousel_pause();
    if (s_top_mode == 1) {
        carousel_goto(VIEW_IDX_SYNTH_SUBHOME);
    } else {
        carousel_goto(VIEW_IDX_FX_MAIN);
    }
}

static void on_param_changed(const GF_Frame* f, void* ctx) {
    BridgeSlave* slave = static_cast<BridgeSlave*>(ctx);
    if (f->len < 6) {
        slave->send_nack(f->seq, GF_NACK_INVALID_LEN);
        return;
    }
    uint16_t param_id;
    float    value;
    memcpy(&param_id, f->payload,     2);
    memcpy(&value,    f->payload + 2, 4);

    if (param_id == 0xFF) {
        /* wet/dry global — el timer de view_07 lee s_wet_dry cada 50ms y actualiza
         * el arco in-place, sin necesidad de reconstruir la vista.
         * Solo navegar a FX_MAIN si el usuario estaba en otra vista. */
        s_wet_dry      = value;
        s_param_cursor = 0xFF;
        if (carousel_get_current() != VIEW_IDX_FX_MAIN) {
            carousel_goto(VIEW_IDX_FX_MAIN);
        }
    } else if (param_id == 0x00F4) {
        /* top_mode — anuncio directo desde setup() sin animación de transición.
         * 0xFD (double-click NAV) sí muestra view_09; este llega silencioso. */
        uint8_t new_mode = (uint8_t)(value + 0.5f);
        /* Resetear contadores de AI mode al salir (entrar a cualquier otro modo). */
        if (s_top_mode == 2 && new_mode != 2) {
            s_snapped_count = 0;
            s_total_count   = 0;
        }
        s_top_mode = new_mode;
        carousel_pause();
        if (new_mode == 1) {
            carousel_goto(VIEW_IDX_ENGINE_LIST);
        }
        Serial.printf("[bridge] TOP_MODE(direct) → %u\n", (unsigned)new_mode);
        Serial.flush();
    } else if (param_id == 0x00F5) {
        /* synth_level: 0=ENGINE_LIST, 1=ENGINE_SUBHOME, 2=GROUP_VIEW */
        s_synth_level = (uint8_t)(value + 0.5f);
        carousel_pause();
        switch (s_synth_level) {
            case 0: carousel_goto(VIEW_IDX_ENGINE_LIST);    break;
            case 1: carousel_goto(VIEW_IDX_SYNTH_SUBHOME);  break;
            /* case 2: group view — se espera 0x00F6 con el grupo concreto */
            default: break;
        }
        Serial.printf("[bridge] SYNTH_LEVEL → %u\n", (unsigned)s_synth_level);
        Serial.flush();
    } else if (param_id == 0x00F6) {
        /* group*10 + cursor — activa la vista del grupo correcto */
        int code = (int)(value + 0.5f);
        s_synth_group = (uint8_t)(code / 10);
        s_synth_param = (uint8_t)(code % 10);
        carousel_pause();
        switch (s_synth_group) {
            case 0: carousel_goto(VIEW_IDX_SYNTH_OSC);     break;
            case 1: carousel_goto(VIEW_IDX_SYNTH_ENV);     break;
            case 2: carousel_goto(VIEW_IDX_SYNTH_FILTER);  break;  // FILTER → vista dedicada (view_24)
            case 3: carousel_goto(VIEW_IDX_SYNTH_LFO);     break;
            default: carousel_goto(VIEW_IDX_SYNTH_OSC);    break;
        }
        Serial.printf("[bridge] SYNTH_GROUP → %u param=%u\n",
                      (unsigned)s_synth_group, (unsigned)s_synth_param);
        Serial.flush();
    } else if (param_id == 0xFE) {
        /* Cursor de lista — comportamiento depende del modo activo. */
        if (s_top_mode == 1) {
            /* SYNTH mode: cursor en ENGINE_LIST */
            s_engine_cursor = (uint8_t)(value + 0.5f);
            carousel_pause();
            if (carousel_get_current() != VIEW_IDX_ENGINE_LIST) {
                carousel_goto(VIEW_IDX_ENGINE_LIST);
            }
        } else {
            /* FX mode: cursor en FX_SELECT.
             * Solo navegar a la vista si no estamos ya en ella; el timer de view_08
             * actualiza el texto en-place sin rebuild costoso. */
            s_fx_cursor = (uint8_t)(value + 0.5f);
            s_param_cursor = 0xFF;
            carousel_pause();
            if (carousel_get_current() != VIEW_IDX_FX_SELECT) {
                carousel_goto(VIEW_IDX_FX_SELECT);
            }
        }
    } else if (param_id == 0xFC) {
        /* Cursor de param — ENC NAV en modo FX edita params del FX activo;
         * en modo SYNTH navega por los params del engine.
         * Navegar a la vista correcta según el modo activo.
         * El timer de cada vista actualiza el contenido en-place. */
        s_param_cursor = (uint8_t)(value + 0.5f);
        carousel_pause();
        uint8_t dest = (s_top_mode == 1) ? VIEW_IDX_SYNTH_MAIN : VIEW_IDX_FX_MAIN;
        if (carousel_get_current() != dest) {
            carousel_goto(dest);
        }
    } else if (param_id == 0xFD) {
        /* Double-click NAV → cycle de modo. value encoda destino: 0=FX, 1=SYNTH, 2=AI.
         * Muestra view_09 (MODE SWITCH) 1s, luego navega al modo destino.
         * Para AI (value=2): el arm ya se completó — s_ai_progress viene de 0xFB. */
        uint8_t new_mode = (uint8_t)(value + 0.5f);
        /* Resetear contadores de AI mode al salir (entrar a cualquier otro modo). */
        if (s_top_mode == 2 && new_mode != 2) {
            s_snapped_count = 0;
            s_total_count   = 0;
        }
        s_top_mode    = new_mode;
        s_ai_progress = -1.0f;   // arm completado, limpiar progreso
        carousel_pause();
        carousel_goto(VIEW_IDX_MODE_SWITCH);
        lv_timer_create([](lv_timer_t* t) {
            lv_timer_del(t);
            /* Navegar al modo destino almacenado en s_top_mode.
             * SYNTH → ENGINE_LIST: el usuario selecciona engine antes de editar
             * parámetros (mismo destino que el handler directo 0x00F4).
             * FX → FX_MAIN: muestra el efecto activo con arco wet/dry. */
            switch (s_top_mode) {
                case 1:  carousel_goto(VIEW_IDX_ENGINE_LIST); break;  // SYNTH → elige engine
                case 2:  carousel_goto(VIEW_IDX_AI_PROC);     break;  // AI full
                default: carousel_goto(VIEW_IDX_FX_MAIN);     break;  // FX
            }
        }, 1000, nullptr);  /* 1s es suficiente para MODE SWITCH — 3s era excesivo */
        Serial.printf("[bridge] MODE SWITCH → %u\n", (unsigned)new_mode);
        Serial.flush();
    } else if (param_id == 0xFB) {
        /* Progreso del arm de AI mode (0.0-1.0) mientras el usuario sostiene ENC NAV.
         *
         *   value < 0  → arm cancelado (usuario soltó antes de 4s).
         *   value >= 0 (primer frame, prev era -1) → iniciar overlay sobre vista actual.
         *   0 < value < 1 → overlay en curso; timer de la vista actualiza el arco.
         *   value >= 1.0 → hold completado: flash + transición a view_10 full.
         *
         * El overlay NO destruye la vista anterior (spec §3.6.3d): crea un dimmer
         * semitransparente + arco + dot encima de ella. Solo al completar el hold
         * se hace carousel_goto(VIEW_IDX_AI_PROC) que reemplaza todo. */
        float prev = s_ai_progress;
        s_ai_progress = value;

        if (value < 0.0f) {
            /* ── CANCELADO: fade-out overlay, vista anterior reaparece ── */
            s_ai_progress = -1.0f;
            carousel_ai_overlay_cancel();
            Serial.println("[bridge] AI arm cancelled — overlay removed");
        } else if (prev < 0.0f) {
            /* ── PRIMER TICK: iniciar overlay (no destruir vista activa) ── */
            carousel_ai_overlay_start();
            Serial.printf("[bridge] AI arm started  progress=%.2f\n", (double)value);
        } else if (value >= 0.999f) {
            /* ── COMPLETADO (4s): flash blanco → view_10 full ─────────── */
            s_ai_progress = -1.0f;   /* limpiar: la vista full no necesita el valor */
            carousel_ai_overlay_complete();
            Serial.println("[bridge] AI arm complete → flash → view_10 full");
        }
        /* Para 0 < value < 1: el timer en view_10_overlay actualiza el arco en-place. */
    } else if (param_id == 0xFA) {
        /* HOME (B1): volver al "menú principal" del modo activo.
         *   FX mode   → FX SELECT   (elegir efecto — RMX B.SELECT)
         *   SYNTH mode→ ENGINE LIST (elegir engine — equivalente a RMX B.SELECT)
         * Bug fix: antes siempre iba a FX_SELECT sin revisar s_top_mode. */
        carousel_pause();
        if (s_top_mode == 1) {
            carousel_goto(VIEW_IDX_ENGINE_LIST);
            Serial.println("[bridge] HOME → ENGINE LIST (SYNTH mode)");
        } else {
            carousel_goto(VIEW_IDX_FX_SELECT);
            Serial.println("[bridge] HOME → FX SELECT (FX mode)");
        }
        Serial.flush();
    } else if (param_id == 0xF9) {
        /* TAP TEMPO (B3): el Teensy calcula el BPM; el display solo loguea.
         * En Sprint 31 se mostrará el BPM en view_07 cuando Ghost Echo esté activo. */
        Serial.printf("[bridge] TAP — bpm=%.1f\n", (double)value);
        Serial.flush();
    } else if (param_id == 0xF8) {
        /* PANIC (B4): silencio inmediato — el Teensy bajó wet/dry a 0.
         * El display refleja: WET arc a 0%, navega a FX MAIN para que
         * el usuario vea el efecto del PANIC en el arco. */
        s_wet_dry = 0.0f;
        carousel_pause();
        carousel_goto(VIEW_IDX_FX_MAIN);
        Serial.println("[bridge] PANIC — wet=0 → FX MAIN");
        Serial.flush();
    } else if (param_id == 0x00F1) {
        /* AI sub-view navigation — ENC NAV cicla 3 sub-vistas (Sprint 45 consolidación).
         * 0 → view_10  AI PROC (Camelot hero — default al entrar)
         * 1 → view_28  AI RACK (control surface 6 features)
         * 2 → view_16  SCALE LOCK (idx 15)
         * view_25/26/27 quedan registradas pero fuera del ciclo principal.
         * Spec: apps/docs/sprints/45-ai-mode-consolidation.md */
        uint8_t sub = (uint8_t)(value + 0.5f);
        if (sub > 2) sub = 0;
        static const uint8_t SUBVIEW_MAP[3] = {
            VIEW_IDX_AI_PROC,    /* 0 → view_10 Camelot */
            VIEW_IDX_AI_RACK,    /* 1 → view_28 AI Rack (Sprint 45) */
            15,                   /* 2 → view_16 Scale Lock (idx 15) */
        };
        carousel_pause();
        carousel_goto(SUBVIEW_MAP[sub]);
        Serial.printf("[bridge] AI_NAV -> sub=%u (view_idx=%u)\n",
                      (unsigned)sub, (unsigned)SUBVIEW_MAP[sub]);
        Serial.flush();
    } else if (param_id == 0x00F2) {
        /* Scale Lock bypass — ENC L click en view_10.
         * 0.0 = algoritmo activo, 1.0 = bypassed (notas sin cuantizar). */
        s_scale_lock_bypass = (value > 0.5f);
        Serial.printf("[bridge] SCALE_LOCK bypass=%u\n", (unsigned)s_scale_lock_bypass);
        Serial.flush();
    } else if (param_id == 0x00F3) {
        /* Beat Follower bypass — ENC R click en view_10.
         * 0.0 = activo (sigue el tempo), 1.0 = bypassed (tempo libre). */
        s_beat_follower_bypass = (value > 0.5f);
        Serial.printf("[bridge] BEAT_FOLLOWER bypass=%u\n", (unsigned)s_beat_follower_bypass);
        Serial.flush();
    } else if (param_id == 0x00F0) {
        /* Ordenamiento del chromagram en view_10.
         * 0.0 = cromático (C C# D ... B), 1.0 = Circle of Fifths (CoF). */
        uint8_t ord = (uint8_t)(value + 0.5f);
        if (ord > 1) ord = 1;
        s_chromagram_ordering = ord;
        Serial.printf("[bridge] CHROMAGRAM ORDERING → %s\n",
                      ord == 0 ? "CROMATIC" : "COF");
        Serial.flush();
    } else if (param_id == 0x00EF) {
        /* Manual scale override (Sprint 34 Batch J).
         * value < 0  → AUTO mode: AI detector toma control.
         * value 0-23 → MANUAL mode con key_idx fijo (0-11=major, 12-23=minor). */
        if (value < 0.0f) {
            s_scale_manual_mode = false;
            s_scale_manual_key  = 0xFF;
            Serial.println("[bridge] SCALE override -> AUTO");
        } else {
            uint8_t k = (uint8_t)(value + 0.5f);
            if (k > 23) k = 0;
            s_scale_manual_mode = true;
            s_scale_manual_key  = k;
            Serial.printf("[bridge] SCALE override -> MANUAL key_idx=%u\n", (unsigned)k);
        }
        Serial.flush();
    } else if (param_id == 0x00E9) {
        /* AI Rack cursor — fila activa (0-5). Sprint 45. */
        s_ai_rack_cursor = (uint8_t)(value + 0.5f);
        if (s_ai_rack_cursor > 5) s_ai_rack_cursor = 0;
    } else if (param_id == 0x00EA) {
        /* Auto-Harmonize enabled — 0.0=OFF, 1.0=ON. Sprint 45. */
        s_auto_harm_enabled = (value > 0.5f);
        Serial.printf("[bridge] AUTO_HARM enabled=%u\n", (unsigned)s_auto_harm_enabled);
        Serial.flush();
    } else if (param_id == 0x00EB) {
        /* Auto-Harmonize interval — 0=THIRD, 1=SIXTH. Sprint 45. */
        s_auto_harm_interval = (uint8_t)(value + 0.5f);
        if (s_auto_harm_interval > 1) s_auto_harm_interval = 0;
        Serial.printf("[bridge] AUTO_HARM interval=%u\n", (unsigned)s_auto_harm_interval);
        Serial.flush();
    } else if (param_id == 0x00EC) {
        /* Arp mode (0=UP,1=DOWN,2=UP_DOWN,3=RANDOM,4=SMART). Sprint 45. */
        s_arp_mode = (uint8_t)(value + 0.5f);
        if (s_arp_mode > 4) s_arp_mode = 4;
        Serial.printf("[bridge] ARP mode=%u\n", (unsigned)s_arp_mode);
        Serial.flush();
    } else if (param_id == 0x00ED) {
        /* Arp division (0=QUARTER,1=EIGHTH,2=SIXTEENTH). Sprint 45. */
        s_arp_division = (uint8_t)(value + 0.5f);
        if (s_arp_division > 2) s_arp_division = 2;
        Serial.printf("[bridge] ARP division=%u\n", (unsigned)s_arp_division);
        Serial.flush();
    } else if (param_id == 0x00EE) {
        /* Groove packed — parte entera=style(0-2), parte fraccional=amount(0-0.99).
         * Sprint 45. */
        s_groove_style  = (uint8_t)value;
        s_groove_amount = value - (float)s_groove_style;
        if (s_groove_style > 2) s_groove_style = 2;
        if (s_groove_amount < 0.0f) s_groove_amount = 0.0f;
        if (s_groove_amount > 0.99f) s_groove_amount = 0.99f;
        Serial.printf("[bridge] GROOVE style=%u amount=%.2f\n",
                      (unsigned)s_groove_style, (double)s_groove_amount);
        Serial.flush();
    }

    /* Cachear valores de parámetros reales (param_id encoding: fx_idx<<8 | param_idx).
     * Excluir: especiales (0xF7-0xFF), spectrum (0xE0-0xE7).
     * 0xF7 = assignment sync: fx_id*100 + sub1*10 + sub2 como float. */
    if (param_id == 0x00F7) {
        int code    = (int)(value + 0.5f);
        uint8_t fid = (uint8_t)(code / 100);
        uint8_t s1  = (uint8_t)((code % 100) / 10);
        uint8_t s2  = (uint8_t)(code % 10);
        if (fid < 9) { s_sub1[fid] = s1; s_sub2[fid] = s2; }
    } else {
        // Bug fix: la comprobación anterior era (param_id >= 0x00E0) que es TRUE para
        // CUALQUIER FX con índice >= 1 (ej. FX 1 → param_id=0x0100=256 >= 0xE0=224).
        // Los IDs especiales (spectrum 0xE0-0xE7, 0xF8-0xFF) solo existen cuando el
        // byte alto es 0x00. Los params reales tienen byte alto = fx_idx (1-8) y
        // byte bajo = param_idx (0-3), por lo que nunca coinciden con el rango especial.
        uint8_t fx_idx = (uint8_t)(param_id >> 8);
        uint8_t p_idx  = (uint8_t)(param_id & 0xFF);
        bool is_synth_param = (fx_idx >= 0x10 && fx_idx <= 0x15);  // 0x10-0x15 = engines 0-5
        bool is_special = (fx_idx == 0) && (p_idx >= 0xE0);  // solo byte-alto=0 con p_idx especial
        if (!is_special && !is_synth_param && fx_idx < 9 && p_idx < 4) {
            s_param_cache[fx_idx][p_idx] = value;
            s_param_value = value;
            s_last_real_param_id = param_id;
            s_last_real_param_ms = lv_tick_get();
        }
        /* Params del synth engine (byte-alto 0x10-0x12):
         * encoding: (0x10 + engine_id) << 8 | (group << 4) | param_idx
         * Extraer cutoff y resonance del grupo FILTER (group=2). */
        if (is_synth_param) {
            uint8_t eng  = (uint8_t)(fx_idx - 0x10);  // engine 0-5 (OB6=3, DX7=4, ARP=5)
            uint8_t grp  = (p_idx >> 4) & 0x0F;
            uint8_t pidx = p_idx & 0x0F;
            if (grp == 2 && pidx == 0) s_synth_cutoff    = value;
            if (grp == 2 && pidx == 1) s_synth_resonance = value;
            // Popula caché completa para que las vistas inicialicen correctamente
            if (eng < 6 && grp < 4 && pidx < 6) {
                s_synth_cache[eng][grp][pidx] = value;
            }
            s_param_value        = value;
            s_last_real_param_id = param_id;
            s_last_real_param_ms = lv_tick_get();
        }
    }

    /* Bypass FX (0xE8) — solo almacenar; view_07 lo lee en el timer. */
    if (param_id == 0x00E8) {
        s_fx_bypass = (value > 0.5f);
    }

    /* Bandas de spectrum analyzer (0xE0-0xE7) — solo almacenar.
     * El timer de view_07 actualiza las barras en-place a 20fps sin carousel_goto. */
    if (param_id >= 0x00E0 && param_id <= 0x00E7) {
        s_spectrum[(uint8_t)(param_id - 0x00E0)] = value;
    }

    Serial.printf("[bridge] PARAM_CHANGED — param=0x%04X val=%.3f\n", param_id, (double)value);
    Serial.flush();
    slave->send_ack(f->seq);
}

static void on_set_engine(const GF_Frame* f, void* ctx) {
    BridgeSlave* slave = static_cast<BridgeSlave*>(ctx);
    if (f->len < 1) {
        slave->send_nack(f->seq, GF_NACK_INVALID_LEN);
        return;
    }
    Serial.printf("[bridge] SET_ENGINE — id=%u\n", f->payload[0]);
    Serial.flush();
    slave->send_ack(f->seq);
}

static void on_display_update(const GF_Frame* f, void* ctx) {
    BridgeSlave* slave = static_cast<BridgeSlave*>(ctx);
    /* Display manejado por el carrusel autónomo en Sprint 3.2.
     * En Sprint 3.4 con encoders, aquí se actualizará la vista activa. */
    slave->send_ack(f->seq);
}

static void on_cloud_status(const GF_Frame* f, void* ctx) {
    BridgeSlave* slave = static_cast<BridgeSlave*>(ctx);
    if (f->len < 3) {
        slave->send_nack(f->seq, GF_NACK_INVALID_LEN);
        return;
    }
    s_cloud_connected = (f->payload[0] != 0);
    uint16_t latency_ms;
    memcpy(&latency_ms, f->payload + 1, 2);
    Serial.printf("[bridge] CLOUD_STATUS — connected=%u latency=%ums\n",
                  (unsigned)s_cloud_connected, (unsigned)latency_ms);
    Serial.flush();
    slave->send_ack(f->seq);
}

/* ── Handlers ML inference (Sprint 32) ───────────────────────────────────── */

/**
 * KEY_DETECTED: payload[0]=key_idx (0-23), payload[1]=confidence (0-100).
 * key_idx 0-11 = C–B mayor; 12-23 = C–B menor.
 * Almacena el nombre formateado en s_ai_key_name para que view_10 lo lea.
 */
static void on_key_detected(const GF_Frame* f, void* ctx) {
    BridgeSlave* slave = static_cast<BridgeSlave*>(ctx);
    if (f->len >= 1) {
        static const char* const NOTES[12] = {
            "C","C#","D","D#","E","F","F#","G","G#","A","A#","B"
        };
        uint8_t idx = f->payload[0];
        /* Guardar raw index ANTES del snprintf — view_10 lo usa para diatonic mask */
        s_ai_key_idx  = (idx < 24) ? idx : 0xFF;
        bool is_minor = (idx >= 12);
        uint8_t note  = is_minor ? (idx - 12) : idx;
        if (note < 12) {
            snprintf(s_ai_key_name, sizeof(s_ai_key_name),
                     "%s %s", NOTES[note], is_minor ? "MIN" : "MAJ");
        }
        Serial.printf("[bridge] KEY_DETECTED — %s (conf=%u)\n",
                      s_ai_key_name, (unsigned)(f->len >= 2 ? f->payload[1] : 0));
        Serial.flush();
    }
    slave->send_ack(f->seq);
}

/**
 * CHORD_DETECTED: payload[0]=root (0-11), payload[1]=chord_type, payload[2]=confidence.
 * chord_type del ChordRecognizer: 0=maj, 1=min, 2=7, 3=m7, 4=maj7, 5=N (no chord).
 */
static void on_chord_detected(const GF_Frame* f, void* ctx) {
    BridgeSlave* slave = static_cast<BridgeSlave*>(ctx);
    if (f->len >= 2) {
        static const char* const NOTES[12] = {
            "C","C#","D","D#","E","F","F#","G","G#","A","A#","B"
        };
        /* chord_type del ChordRecognizer: 0=maj,1=min,2=7,3=m7,4=maj7,5=N */
        static const char* const QUAL[5] = { "", "m", "7", "m7", "M7" };
        uint8_t root = f->payload[0];
        uint8_t qual = f->payload[1];
        /* Guardar raw valores ANTES del snprintf — view_10 los usa para triad viz */
        s_ai_chord_root    = (root < 12) ? root : 0xFF;
        s_ai_chord_quality = (qual <= 5) ? qual : 0xFF;
        if (qual == 5 || root >= 12) {
            /* N (no chord) o root inválido — mostrar "---" */
            snprintf(s_ai_chord_name, sizeof(s_ai_chord_name), "---");
        } else if (qual < 5) {
            snprintf(s_ai_chord_name, sizeof(s_ai_chord_name),
                     "%s%s", NOTES[root], QUAL[qual]);
        }
        Serial.printf("[bridge] CHORD_DETECTED — %s (conf=%u)\n",
                      s_ai_chord_name, (unsigned)(f->len >= 3 ? f->payload[2] : 0));
        Serial.flush();
    }
    slave->send_ack(f->seq);
}

/**
 * BEAT_DETECTED: payload[0:1] = BPM×10 uint16 little-endian.
 * Ej.: 1200 = 120.0 BPM, 935 = 93.5 BPM. 0 = desconocido.
 * Se almacena el BPM entero (÷10) para display "120 BPM".
 */
static void on_beat_detected(const GF_Frame* f, void* ctx) {
    BridgeSlave* slave = static_cast<BridgeSlave*>(ctx);
    if (f->len >= 2) {
        uint16_t bpm_x10;
        memcpy(&bpm_x10, f->payload, 2);
        s_ai_bpm = (uint16_t)(bpm_x10 / 10);
        Serial.printf("[bridge] BEAT_DETECTED — %u BPM\n", (unsigned)s_ai_bpm);
        Serial.flush();
    }
    slave->send_ack(f->seq);
}

/* ── Handler GROOVE_STATE (Sprint 33 — 0x83) ─────────────────────────────── */

/**
 * GROOVE_STATE: payload fijo de 16 bytes @ 4Hz desde Teensy mientras g_in_ai_mode.
 *   [0..11]  pitch_activity[12] — EMA tau=2s de NOTE_ON velocity, mapeado 0-255
 *   [12]     snap_event         — rising-edge: 1 por un solo frame tras un snap
 *   [13]     snap_from          — MIDI note original (0-127)
 *   [14]     snap_to            — MIDI note cuantizada (0-127)
 *   [15]     beat_phase_256     — fase dentro del bar (0=downbeat, 255=casi siguiente)
 */
static void on_groove_state(const GF_Frame* f, void* ctx) {
    BridgeSlave* slave = static_cast<BridgeSlave*>(ctx);
    if (f->len != 16) {
        slave->send_nack(f->seq, GF_NACK_INVALID_LEN);
        return;
    }

    memcpy(s_pitch_activity, f->payload, 12);

    if (f->payload[12] != 0) {
        /* snap_event rising-edge: registrar nota corregida */
        s_last_snap_from = f->payload[13];
        s_last_snap_to   = f->payload[14];
        s_last_snap_ms   = millis();
        s_snapped_count++;
    }

    s_beat_phase = f->payload[15];

    /* Logging throttled 1Hz — solo 7 pitch classes de la escala mayor de C
     * para no inundar el monitor (indices 0,2,4,5,7,9,11). */
    uint32_t now = millis();
    if (now - s_last_groove_state_log_ms > 1000) {
        s_last_groove_state_log_ms = now;
        Serial.printf("[bridge] GROOVE_STATE — activity[C,D,E,F,G,A,B]=[%u,%u,%u,%u,%u,%u,%u]"
                      "  snap=%u/%u  phase=%u\n",
                      s_pitch_activity[0],  s_pitch_activity[2],  s_pitch_activity[4],
                      s_pitch_activity[5],  s_pitch_activity[7],  s_pitch_activity[9],
                      s_pitch_activity[11],
                      (unsigned)s_snapped_count, (unsigned)s_total_count,
                      (unsigned)s_beat_phase);
        Serial.flush();
    }

    slave->send_ack(f->seq);
}

/* ── Getters de GROOVE_STATE ─────────────────────────────────────────────── */

uint8_t bridge_get_pitch_activity(uint8_t pc) {
    return (pc < 12) ? s_pitch_activity[pc] : 0;
}

void bridge_get_last_snap(uint8_t* from, uint8_t* to, uint32_t* age_ms) {
    if (s_last_snap_ms == 0) {
        *from   = 0;
        *to     = 0;
        *age_ms = 0xFFFFFFFF;
        return;
    }
    *from       = s_last_snap_from;
    *to         = s_last_snap_to;
    uint32_t age = millis() - s_last_snap_ms;
    *age_ms     = (age > 5000) ? 0xFFFFFFFF : age;   // expira a los 5s
}

void bridge_get_snap_stats(uint16_t* snapped, uint16_t* total) {
    *snapped = s_snapped_count;
    *total   = s_total_count;
}

uint8_t bridge_get_beat_phase(void) { return s_beat_phase; }

/* Handler genérico para CMDs sin handler específico: ACK sin acción */
static void on_generic(const GF_Frame* f, void* ctx) {
    BridgeSlave* slave = static_cast<BridgeSlave*>(ctx);
    slave->send_ack(f->seq);
}

/* ── Registro ────────────────────────────────────────────────────────────── */

void bridge_handlers_init(BridgeSlave& slave) {
    /* Sistema */
    slave.on_command(GF_CMD_HEARTBEAT,    on_heartbeat,      &slave);
    slave.on_command(GF_CMD_GET_VERSION,  on_get_version,    &slave);
    slave.on_command(GF_CMD_RESET,        on_generic,        &slave);
    slave.on_command(GF_CMD_GET_STATUS,   on_generic,        &slave);

    /* Engine */
    slave.on_command(GF_CMD_SET_ENGINE,     on_set_engine,     &slave);
    slave.on_command(GF_CMD_ENGINE_CHANGED, on_engine_changed, &slave);
    slave.on_command(GF_CMD_SET_PARAM,      on_generic,        &slave);
    slave.on_command(GF_CMD_PARAM_CHANGED,  on_param_changed,  &slave);
    slave.on_command(GF_CMD_LOAD_PRESET,    on_generic,        &slave);
    slave.on_command(GF_CMD_SAVE_PRESET,    on_generic,        &slave);
    slave.on_command(GF_CMD_PRESET_LOADED,  on_generic,        &slave);

    /* FX */
    slave.on_command(GF_CMD_FX_ENABLE,        on_generic, &slave);
    slave.on_command(GF_CMD_FX_SET_PARAM,     on_generic, &slave);
    slave.on_command(GF_CMD_FX_PARAM_CHANGED, on_generic, &slave);
    slave.on_command(GF_CMD_FX_CHAIN_LOAD,    on_generic, &slave);
    slave.on_command(GF_CMD_FX_CHAIN_SAVE,    on_generic, &slave);

    /* MIDI/Audio */
    slave.on_command(GF_CMD_NOTE_ON,    on_note_on,  &slave);
    slave.on_command(GF_CMD_NOTE_OFF,   on_note_off, &slave);
    slave.on_command(GF_CMD_CC,         on_generic, &slave);
    slave.on_command(GF_CMD_PITCH_BEND, on_generic, &slave);
    slave.on_command(GF_CMD_TEMPO,      on_generic, &slave);
    slave.on_command(GF_CMD_TRANSPORT,  on_generic, &slave);

    /* UI */
    slave.on_command(GF_CMD_DISPLAY_UPDATE, on_display_update, &slave);
    slave.on_command(GF_CMD_DISPLAY_TEXT,   on_generic,        &slave);
    slave.on_command(GF_CMD_LED_SET,        on_generic,        &slave);
    slave.on_command(GF_CMD_LED_PATTERN,    on_generic,        &slave);

    /* Cloud */
    slave.on_command(GF_CMD_CLOUD_STATUS,         on_cloud_status, &slave);
    slave.on_command(GF_CMD_PATCH_SEARCH,         on_generic,      &slave);
    slave.on_command(GF_CMD_PATCH_RESULTS,        on_generic,      &slave);
    slave.on_command(GF_CMD_PROGRESSION_REQUEST,  on_generic,      &slave);
    slave.on_command(GF_CMD_PROGRESSION_RESPONSE, on_generic,      &slave);
    slave.on_command(GF_CMD_OTA_AVAILABLE,        on_generic,      &slave);
    slave.on_command(GF_CMD_OTA_START,            on_generic,      &slave);
    slave.on_command(GF_CMD_OTA_PROGRESS,         on_generic,      &slave);

    /* DAW, Pogo, ML → todos genéricos en Sprint 3.2 */
    slave.on_command(GF_CMD_DAW_CONNECTED,  on_generic, &slave);
    slave.on_command(GF_CMD_MIX_SCORE,      on_generic, &slave);
    slave.on_command(GF_CMD_FREQ_CONFLICT,  on_generic, &slave);
    slave.on_command(GF_CMD_LAYER_SUGGEST,  on_generic, &slave);
    slave.on_command(GF_CMD_MACRO_BIND,     on_generic, &slave);
    slave.on_command(GF_CMD_MACRO_VALUE,    on_generic, &slave);
    slave.on_command(GF_CMD_SLAVE_DETECTED, on_generic, &slave);
    slave.on_command(GF_CMD_SLAVE_REMOVED,  on_generic, &slave);
    slave.on_command(GF_CMD_SLAVE_DATA,     on_generic, &slave);
    slave.on_command(GF_CMD_KEY_DETECTED,   on_key_detected,   &slave);
    slave.on_command(GF_CMD_CHORD_DETECTED, on_chord_detected, &slave);
    slave.on_command(GF_CMD_BEAT_DETECTED,  on_beat_detected,  &slave);
    slave.on_command(GF_CMD_GROOVE_STATE,   on_groove_state,   &slave);
    slave.on_command(GF_CMD_GENRE_DETECTED, on_generic,        &slave);
    slave.on_command(GF_CMD_BEGIN_TRANSFER, on_generic, &slave);
    slave.on_command(GF_CMD_CHUNK,          on_generic, &slave);
    slave.on_command(GF_CMD_END_TRANSFER,   on_generic, &slave);

    Serial.println("[bridge] handlers registered");
    Serial.flush();
}
