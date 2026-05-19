#pragma once

/**
 * @file bridge_slave.h
 * @brief UART Bridge Protocol — ESP32-S3 slave
 *
 * Recibe frames del Teensy (master) via UART a 921600 baud.
 * Despacha comandos a los handlers registrados.
 * Envía ACK/NACK y monitorea el heartbeat.
 *
 * Uso:
 *   BridgeSlave bridge;
 *   bridge.on_command(GF_CMD_ENGINE_CHANGED, my_handler);
 *   bridge.init();
 *   // en loop():
 *   bridge.poll();
 */

#include <protocol.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* Timeout parcial: si un frame no termina en este tiempo, se descarta */
#define BRIDGE_FRAME_TIMEOUT_MS  100U
/* Timeout de heartbeat: si no llega en este tiempo, conexión = DOWN */
#define BRIDGE_HEARTBEAT_TIMEOUT_MS  3000U
/* Max handlers registrados (uno por CMD ID) */
#define BRIDGE_HANDLER_TABLE_SIZE  256U

typedef void (*gf_cmd_handler_t)(const GF_Frame* frame, void* ctx);

class BridgeSlave {
public:
    BridgeSlave();

    /** Inicializa Serial0/UART0 (GPIO44=RX, GPIO43=TX nativos) a 921600 8N1. */
    void init();

    /** Procesa bytes disponibles en Serial0. Llamar en cada iteración de loop(). */
    void poll();

    /** Envía un frame ya construido. */
    void send(const GF_Frame& frame);

    /**
     * Registra un handler para un CMD específico.
     * @param cmd     Command ID a interceptar
     * @param handler Callback — void(const GF_Frame*, void* ctx)
     * @param ctx     Puntero de contexto pasado al callback (puede ser nullptr)
     */
    void on_command(GF_Cmd cmd, gf_cmd_handler_t handler, void* ctx = nullptr);

    /** Envía ACK para el SEQ indicado. */
    void send_ack(uint8_t seq);

    /** Envía NACK con la razón indicada. */
    void send_nack(uint8_t seq, GF_NackReason reason);

    /** Retorna true si se recibió heartbeat en los últimos BRIDGE_HEARTBEAT_TIMEOUT_MS. */
    bool is_connected() const { return _connected; }

    /** Timestamp del último heartbeat recibido (millis()). */
    uint32_t last_heartbeat_ms() const { return _last_hb_ms; }

    /** Actualiza el timestamp de heartbeat y marca la conexión UP.
     *  Llamado por on_heartbeat() en bridge_handlers.cpp. */
    void mark_heartbeat();

private:
    /* ── State machine de recepción ────────────────────────────────────────── */

    enum class RxState : uint8_t {
        IDLE,
        GOT_CMD,
        GOT_LEN,
        GOT_SEQ,
        READING_PAYLOAD,
        AWAIT_CRC,
    };

    RxState  _rx_state     = RxState::IDLE;
    GF_Frame _rx_frame     = {};
    uint8_t  _rx_bytes     = 0;        /* bytes de payload recibidos hasta ahora */
    uint32_t _rx_start_ms  = 0;        /* timestamp del inicio del frame en curso */

    /* ── Handlers ─────────────────────────────────────────────────────────── */

    struct HandlerEntry {
        gf_cmd_handler_t fn  = nullptr;
        void*            ctx = nullptr;
    };
    HandlerEntry _handlers[BRIDGE_HANDLER_TABLE_SIZE];

    /* ── Estado de conexión ───────────────────────────────────────────────── */

    bool     _connected   = false;
    uint32_t _last_hb_ms  = 0;

    /* ── SEQ counter outgoing ─────────────────────────────────────────────── */
    uint8_t  _tx_seq      = 0;

    /* ── Internos ─────────────────────────────────────────────────────────── */

    void process_byte(uint8_t byte);
    void dispatch_frame();
    void reset_rx();
};
