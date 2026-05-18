/**
 * @file bridge_handlers.cpp
 * @brief Handlers de comandos Bridge — ESP32-S3 (Sprint 3.2)
 *
 * Implementa los callbacks despachados por BridgeSlave::dispatch_frame().
 * Cada handler recibe el frame completo y el puntero de contexto (BridgeSlave*).
 */

#include "bridge_handlers.h"
#include <Arduino.h>
#include <string.h>

/* ── Estado interno del bridge (visible via getters públicos) ────────────── */

static char    s_engine_name[32]   = "---";
static uint8_t s_engine_id         = 0xFF;
static bool    s_cloud_connected   = false;

const char* bridge_get_engine_name(void)    { return s_engine_name; }
uint8_t     bridge_get_engine_id(void)      { return s_engine_id; }
bool        bridge_get_cloud_connected(void){ return s_cloud_connected; }

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
    Serial.printf("[bridge] PARAM_CHANGED — param=%u val=%.4f\n", param_id, (double)value);
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
