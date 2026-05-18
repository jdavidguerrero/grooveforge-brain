#pragma once

/**
 * @file protocol.h
 * @brief GrooveForge Bridge Protocol v0.1 — shared header
 *
 * Usado por firmware-teensy (master) y firmware-esp32 (slave).
 * Sin dependencias externas: solo stdint.h y stddef.h.
 * Spec de referencia: apps/docs/02-bridge-protocol.md
 *
 * Wire format: [CMD 1B][LEN 1B][SEQ 1B][PAYLOAD 0-255B][CRC8 1B]
 * CRC-8 poly 0x07 (Dallas/Maxim, SMBUS) sobre CMD+LEN+SEQ+PAYLOAD.
 * UART: 921600 baud, 8N1, sin flow control.
 */

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>

/* ── Frame struct ───────────────────────────────────────────────────────────── */

#define GF_PAYLOAD_MAX 255U
#define GF_FRAME_HEADER_SIZE 3U   /* CMD + LEN + SEQ */
#define GF_FRAME_MIN_SIZE    4U   /* header + CRC (0-payload frame) */

typedef struct {
    uint8_t cmd;
    uint8_t len;                      /* longitud del payload en bytes (0-255) */
    uint8_t seq;                      /* sequence number rolling 0-255 */
    uint8_t payload[GF_PAYLOAD_MAX + 1]; /* payload[0..len-1] */
} GF_Frame;

/* ── Command IDs ────────────────────────────────────────────────────────────── */

typedef enum : uint8_t {
    /* 0x00 — System */
    GF_CMD_HEARTBEAT        = 0x00,
    GF_CMD_ACK              = 0x01,
    GF_CMD_NACK             = 0x02,
    GF_CMD_GET_VERSION      = 0x03,
    GF_CMD_VERSION          = 0x04,
    GF_CMD_RESET            = 0x05,
    GF_CMD_GET_STATUS       = 0x06,
    GF_CMD_STATUS           = 0x07,

    /* 0x10 — Engine */
    GF_CMD_SET_ENGINE       = 0x10,
    GF_CMD_GET_ENGINE       = 0x11,
    GF_CMD_ENGINE_CHANGED   = 0x12,
    GF_CMD_SET_PARAM        = 0x13,
    GF_CMD_PARAM_CHANGED    = 0x14,
    GF_CMD_LOAD_PRESET      = 0x15,
    GF_CMD_SAVE_PRESET      = 0x16,
    GF_CMD_PRESET_LOADED    = 0x17,

    /* 0x20 — FX */
    GF_CMD_FX_ENABLE        = 0x20,
    GF_CMD_FX_SET_PARAM     = 0x21,
    GF_CMD_FX_PARAM_CHANGED = 0x22,
    GF_CMD_FX_CHAIN_LOAD    = 0x23,
    GF_CMD_FX_CHAIN_SAVE    = 0x24,

    /* 0x30 — MIDI/Audio */
    GF_CMD_NOTE_ON          = 0x30,
    GF_CMD_NOTE_OFF         = 0x31,
    GF_CMD_CC               = 0x32,
    GF_CMD_PITCH_BEND       = 0x33,
    GF_CMD_TEMPO            = 0x34,
    GF_CMD_TRANSPORT        = 0x35,

    /* 0x40 — UI (Teensy → ESP32) */
    GF_CMD_DISPLAY_UPDATE   = 0x40,
    GF_CMD_DISPLAY_TEXT     = 0x41,
    GF_CMD_LED_SET          = 0x42,
    GF_CMD_LED_PATTERN      = 0x43,

    /* 0x50 — Cloud (ESP32 ↔ Teensy) */
    GF_CMD_CLOUD_STATUS         = 0x50,
    GF_CMD_PATCH_SEARCH         = 0x51,
    GF_CMD_PATCH_RESULTS        = 0x52,
    GF_CMD_PROGRESSION_REQUEST  = 0x53,
    GF_CMD_PROGRESSION_RESPONSE = 0x54,
    GF_CMD_OTA_AVAILABLE        = 0x55,
    GF_CMD_OTA_START            = 0x56,
    GF_CMD_OTA_PROGRESS         = 0x57,

    /* 0x60 — DAW Bridge */
    GF_CMD_DAW_CONNECTED    = 0x60,
    GF_CMD_MIX_SCORE        = 0x61,
    GF_CMD_FREQ_CONFLICT    = 0x62,
    GF_CMD_LAYER_SUGGEST    = 0x63,
    GF_CMD_MACRO_BIND       = 0x64,
    GF_CMD_MACRO_VALUE      = 0x65,

    /* 0x70 — Pogo/Slave */
    GF_CMD_SLAVE_DETECTED   = 0x70,
    GF_CMD_SLAVE_REMOVED    = 0x71,
    GF_CMD_SLAVE_DATA       = 0x72,

    /* 0x80 — ML inference */
    GF_CMD_KEY_DETECTED     = 0x80,
    GF_CMD_CHORD_DETECTED   = 0x81,
    GF_CMD_BEAT_DETECTED    = 0x82,
    GF_CMD_GENRE_DETECTED   = 0x83,

    /* 0xF0 — Multi-frame transfer */
    GF_CMD_BEGIN_TRANSFER   = 0xF0,
    GF_CMD_CHUNK            = 0xF1,
    GF_CMD_END_TRANSFER     = 0xF2,
} GF_Cmd;

/* ── NACK reasons ───────────────────────────────────────────────────────────── */

typedef enum : uint8_t {
    GF_NACK_CRC_ERROR       = 0x01,
    GF_NACK_INVALID_CMD     = 0x02,
    GF_NACK_INVALID_LEN     = 0x03,
    GF_NACK_INVALID_PARAM   = 0x04,
    GF_NACK_OOM             = 0x05,
    GF_NACK_TIMEOUT         = 0x06,
    GF_NACK_NOT_CONNECTED   = 0x07,
    GF_NACK_AUTH_FAILED     = 0x08,
} GF_NackReason;

/* ── Engine IDs ─────────────────────────────────────────────────────────────── */

typedef enum : uint8_t {
    GF_ENGINE_MOOG_MODEL_D  = 0x00,
    GF_ENGINE_JUNO_106      = 0x01,
    GF_ENGINE_PROPHET_5     = 0x02,
} GF_EngineId;

/* ── CRC-8 (poly 0x07, Dallas/Maxim SMBUS) ──────────────────────────────────── */

/**
 * @brief Calcula CRC-8 poly 0x07 sobre un bloque de datos.
 * Vector canónico: gf_crc8("123456789", 9) == 0xF4
 */
static inline uint8_t gf_crc8(const uint8_t* data, size_t len) {
    uint8_t crc = 0x00;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (uint8_t bit = 0; bit < 8; bit++) {
            crc = (crc & 0x80u) ? ((uint8_t)((crc << 1) ^ 0x07u)) : (uint8_t)(crc << 1);
        }
    }
    return crc;
}

/* ── Frame helpers ──────────────────────────────────────────────────────────── */

/**
 * @brief Construye un frame dado CMD, SEQ y payload.
 * @return true si el frame es válido (len <= GF_PAYLOAD_MAX).
 */
static inline bool gf_frame_build(GF_Frame* f, GF_Cmd cmd, uint8_t seq,
                                   const uint8_t* payload, size_t payload_len) {
    if (payload_len > GF_PAYLOAD_MAX) return false;
    f->cmd = (uint8_t)cmd;
    f->len = payload_len;
    f->seq = seq;
    if (payload_len > 0 && payload != NULL) {
        memcpy(f->payload, payload, payload_len);
    }
    return true;
}

/**
 * @brief Calcula el CRC esperado para el frame (sobre CMD+LEN+SEQ+PAYLOAD).
 */
static inline uint8_t gf_frame_crc(const GF_Frame* f) {
    uint8_t header[3] = { f->cmd, f->len, f->seq };
    uint8_t crc = gf_crc8(header, 3);
    /* Continuar CRC sobre el payload acumulando desde el valor anterior */
    for (uint8_t i = 0; i < f->len; i++) {
        uint8_t byte = f->payload[i];
        crc ^= byte;
        for (uint8_t bit = 0; bit < 8; bit++) {
            crc = (crc & 0x80u) ? ((uint8_t)((crc << 1) ^ 0x07u)) : (uint8_t)(crc << 1);
        }
    }
    return crc;
}

/**
 * @brief Valida el CRC de un frame ya construido comparando con el CRC calculado.
 * @param f     Frame a validar
 * @param crc8  Byte de CRC recibido del cable
 */
static inline bool gf_frame_valid_crc(const GF_Frame* f, uint8_t crc8) {
    return gf_frame_crc(f) == crc8;
}

/**
 * @brief Serializa el frame en un buffer de wire (CMD LEN SEQ PAYLOAD... CRC).
 * @param buf     Buffer destino (mínimo 4 + payload_len bytes)
 * @param buf_len Tamaño del buffer
 * @return Número de bytes escritos, o 0 si el buffer es demasiado pequeño.
 */
static inline size_t gf_frame_serialize(const GF_Frame* f, uint8_t* buf, size_t buf_len) {
    size_t needed = (size_t)f->len + GF_FRAME_MIN_SIZE;
    if (buf_len < needed) return 0;
    buf[0] = f->cmd;
    buf[1] = f->len;
    buf[2] = f->seq;
    if (f->len > 0) memcpy(buf + 3, f->payload, f->len);
    buf[3 + f->len] = gf_frame_crc(f);
    return needed;
}
