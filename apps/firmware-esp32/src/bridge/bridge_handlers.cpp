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
static bool s_scale_lock_bypass    = false;
static bool s_beat_follower_bypass = false;

/* Navegación SYNTH (Sprint 31) */
static uint8_t s_synth_level    = 0;   // 0=ENGINE_LIST, 1=SUBHOME, 2=GROUP_VIEW
static uint8_t s_synth_group    = 0;   // 0=OSC, 1=ENV, 2=FILTER, 3=LFO
static uint8_t s_synth_param    = 0;   // cursor dentro del grupo
static uint8_t s_engine_cursor  = 0;   // cursor en ENGINE_LIST
static float   s_synth_cutoff     = 0.5f;   // último cutoff recibido (grupo FILTER param 0)
static float   s_synth_resonance  = 0.1f;   // último resonance recibido (grupo FILTER param 1)

/* Caché completa de parámetros synth: [engine 0-2][grupo 0-3][param 0-3]
 * Poblada por cada PARAM_CHANGED con byte-alto 0x10-0x12.
 * Permite a las vistas GROUP_VIEW inicializar todos los valores correctamente. */
static float   s_synth_cache[3][4][4] = {};

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

bool bridge_get_scale_lock_bypass(void)    { return s_scale_lock_bypass; }
bool bridge_get_beat_follower_bypass(void) { return s_beat_follower_bypass; }

uint8_t bridge_get_synth_level(void)    { return s_synth_level; }
uint8_t bridge_get_synth_group(void)    { return s_synth_group; }
uint8_t bridge_get_synth_param(void)    { return s_synth_param; }
uint8_t bridge_get_engine_cursor(void)  { return s_engine_cursor; }
float   bridge_get_synth_cutoff(void)   { return s_synth_cutoff; }
float   bridge_get_synth_resonance(void){ return s_synth_resonance; }

float bridge_get_synth_param_cached(uint8_t engine, uint8_t group, uint8_t pidx) {
    if (engine >= 3 || group >= 4 || pidx >= 4) return 0.0f;
    return s_synth_cache[engine][group][pidx];
}

/* ── Helpers ─────────────────────────────────────────────────────────────── */

static void auto_ack(const GF_Frame* f, BridgeSlave* slave) {
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
        /* wet/dry global — siempre salta a FX MAIN con la animación del arco.
         * ENC L es un control "global" que hace visible el nivel wet/dry
         * independientemente de en qué vista estaba el usuario.
         * Forzar sub-modo FX_MAIN (param_cursor=0xFF) para que build_fx_main()
         * corra y muestre el arco animado + spectrum. */
        s_wet_dry      = value;
        s_param_cursor = 0xFF;
        carousel_goto(VIEW_IDX_FX_MAIN);
    } else if (param_id == 0x00F4) {
        /* top_mode — anuncio directo desde setup() sin animación de transición.
         * 0xFD (double-click NAV) sí muestra view_09; este llega silencioso. */
        uint8_t new_mode = (uint8_t)(value + 0.5f);
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
            case 0: carousel_goto(VIEW_IDX_SYNTH_OSC);      break;
            case 1: carousel_goto(VIEW_IDX_SYNTH_ENV);      break;
            case 2: carousel_goto(VIEW_IDX_SYNTH_SUBHOME);  break;  // FILTER → arc view
            case 3: carousel_goto(VIEW_IDX_SYNTH_LFO);      break;
            default: carousel_goto(VIEW_IDX_SYNTH_SUBHOME); break;
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
        bool is_synth_param = (fx_idx >= 0x10 && fx_idx <= 0x12);
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
            uint8_t eng  = (fx_idx - 0x10) & 0x03;   // engine 0-2
            uint8_t grp  = (p_idx >> 4) & 0x0F;
            uint8_t pidx = p_idx & 0x0F;
            if (grp == 2 && pidx == 0) s_synth_cutoff    = value;
            if (grp == 2 && pidx == 1) s_synth_resonance = value;
            // Popula caché completa para que las vistas inicialicen correctamente
            if (eng < 3 && grp < 4 && pidx < 4) {
                s_synth_cache[eng][grp][pidx] = value;
            }
            s_param_value        = value;
            s_last_real_param_id = param_id;
            s_last_real_param_ms = lv_tick_get();
        }
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
    slave.on_command(GF_CMD_NOTE_ON,    on_generic, &slave);
    slave.on_command(GF_CMD_NOTE_OFF,   on_generic, &slave);
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
    slave.on_command(GF_CMD_KEY_DETECTED,   on_generic, &slave);
    slave.on_command(GF_CMD_CHORD_DETECTED, on_generic, &slave);
    slave.on_command(GF_CMD_BEAT_DETECTED,  on_generic, &slave);
    slave.on_command(GF_CMD_GENRE_DETECTED, on_generic, &slave);
    slave.on_command(GF_CMD_BEGIN_TRANSFER, on_generic, &slave);
    slave.on_command(GF_CMD_CHUNK,          on_generic, &slave);
    slave.on_command(GF_CMD_END_TRANSFER,   on_generic, &slave);

    Serial.println("[bridge] handlers registered");
    Serial.flush();
}
