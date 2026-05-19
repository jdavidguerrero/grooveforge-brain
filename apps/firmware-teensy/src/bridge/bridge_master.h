#pragma once

/**
 * @file bridge_master.h
 * @brief UART Bridge Protocol — Teensy 4.1 master
 *
 * Implementa el lado master del protocolo de 02-bridge-protocol.md.
 * El Teensy inicia y controla la comunicación; el ESP32 responde con ACK/NACK.
 *
 * Wire format: [CMD 1B][LEN 1B][SEQ 1B][PAYLOAD 0-255B][CRC8 1B]
 * UART: Teensy Serial1, RX=pin0 ← ESP32 GPIO43(TX), TX=pin1 → ESP32 GPIO44(RX)
 * Baud: 921600 8N1 — pines 0/1 son la asignación por hardware de Serial1 en Teensy 4.1
 *
 * State machine de recepción idéntica a BridgeSlave para garantizar
 * compatibilidad de parsing en ambos extremos.
 *
 * Uso:
 *   BridgeMaster bridge;
 *   bridge.on_command(GF_CMD_VERSION, my_handler);
 *   bridge.init();
 *   // en loop():
 *   bridge.poll();
 *   bridge.send_heartbeat();  // cada 1s via IntervalTimer
 */

#include <protocol.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

typedef void (*gf_master_handler_t)(const GF_Frame* frame, void* ctx);

static constexpr uint32_t BRIDGE_FRAME_TIMEOUT_MS     = 100U;
static constexpr uint32_t BRIDGE_HEARTBEAT_TIMEOUT_MS = 3000U;

class BridgeMaster {
public:
    BridgeMaster();

    /**
     * @brief Inicializa Serial1 (Teensy pin 0/1) a 921600 8N1.
     *        Pin 0 = RX, Pin 1 = TX — fijados por hardware en Teensy 4.1.
     */
    void init();

    /**
     * @brief Procesa bytes recibidos del ESP32 y actualiza el watchdog.
     *        Llamar en cada iteración de loop() — no bloquea.
     */
    void poll();

    /**
     * @brief Envía un frame y espera ACK del ESP32 con matching SEQ.
     * @param frame       Frame a enviar (CMD, payload ya construidos).
     * @param timeout_ms  Timeout máximo en ms (default 100ms).
     * @return true si el ACK llegó antes del timeout, false en timeout o NACK.
     *
     * Bloquea el loop() durante timeout_ms en el peor caso — usar solo para
     * comandos críticos que requieren confirmación (GET_VERSION, LOAD_PRESET...).
     * Para telemetría continua usar send_async().
     */
    bool send_and_wait(const GF_Frame& frame, uint32_t timeout_ms = 100);

    /**
     * @brief Envía un frame sin esperar ACK.
     *        Apropiado para ENGINE_CHANGED, NOTE_ON, CC — comandos de alta frecuencia
     *        donde un timeout de 100ms sería inaceptable para la latencia de audio.
     */
    void send_async(const GF_Frame& frame);

    /**
     * @brief Registra un handler para un CMD recibido del ESP32.
     * @param cmd     Command ID (GF_Cmd) a interceptar.
     * @param handler Callback void(const GF_Frame*, void* ctx).
     * @param ctx     Contexto opcional pasado al callback.
     */
    void on_command(GF_Cmd cmd, gf_master_handler_t handler, void* ctx = nullptr);

    /**
     * @brief Envía un frame HEARTBEAT al ESP32.
     *        Llamar cada 1s desde un IntervalTimer para mantener la conexión UP.
     *        El ESP32 responde con ACK; si no responde en BRIDGE_HEARTBEAT_TIMEOUT_MS
     *        → _esp32_alive = false.
     */
    void send_heartbeat();

    /**
     * @brief Retorna true si el ESP32 ha respondido al heartbeat dentro de
     *        BRIDGE_HEARTBEAT_TIMEOUT_MS. Falso hasta el primer ACK post-init.
     */
    bool is_esp32_alive() const { return _esp32_alive; }

    /**
     * @brief Retorna el SEQ counter actual (útil para construir frames externos).
     */
    uint8_t tx_seq() const { return _tx_seq; }

    /**
     * @brief Retorna referencia al SEQ counter y lo incrementa (post-increment).
     *        Usar al construir frames manuales fuera de send_raw().
     */
    uint8_t next_seq() { return _tx_seq++; }

private:
    /* ── State machine de recepción ─────────────────────────────────────────── */

    enum class RxState : uint8_t {
        IDLE,
        GOT_CMD,
        GOT_LEN,
        READING_PAYLOAD,
        AWAIT_CRC,
    };

    RxState  _rx_state    = RxState::IDLE;
    GF_Frame _rx_frame    = {};
    uint8_t  _rx_bytes    = 0;
    uint32_t _rx_start_ms = 0;

    /* ── Tracking ACK para send_and_wait ─────────────────────────────────── */

    bool    _ack_received    = false;
    uint8_t _pending_ack_seq = 0;

    /* ── Estado de conexión ───────────────────────────────────────────────── */

    bool     _esp32_alive    = false;
    uint32_t _last_hb_ack_ms = 0;   // última vez que el ESP32 respondió a un HB

    /* ── SEQ counter outgoing ─────────────────────────────────────────────── */

    uint8_t  _tx_seq = 0;

    /* ── Tabla de handlers ────────────────────────────────────────────────── */

    struct HandlerEntry {
        gf_master_handler_t fn  = nullptr;
        void*               ctx = nullptr;
    };
    HandlerEntry _handlers[256] = {};

    /* ── Internos ─────────────────────────────────────────────────────────── */

    void send_raw(const GF_Frame& frame);
    void process_byte(uint8_t byte);
    void dispatch_frame();
    void reset_rx();
};
