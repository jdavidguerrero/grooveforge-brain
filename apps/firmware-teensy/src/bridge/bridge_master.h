#pragma once

/**
 * @file bridge_master.h
 * @brief UART Bridge Protocol — Teensy 4.1 master (skeleton Sprint 3.2)
 *
 * Esqueleto de la clase master. Se implementa completamente en el sprint
 * de firmware Teensy (audio + engines + bridge).
 *
 * Pines: Teensy RX=0 ← ESP32 TX (GPIO 43); Teensy TX=1 → ESP32 RX (GPIO 44).
 * UART: 921600 baud, 8N1.
 */

#include <protocol.h>
#include <stdint.h>
#include <stdbool.h>

typedef void (*gf_master_handler_t)(const GF_Frame* frame, void* ctx);

class BridgeMaster {
public:
    /** Inicializa Serial1 (Teensy pin 0/1) a 921600 8N1. */
    void init();

    /** Procesa bytes recibidos del ESP32. Llamar en cada iteración de loop(). */
    void poll();

    /**
     * Envía un frame y espera ACK (timeout_ms = 100ms por defecto).
     * @return true si el ACK llegó antes del timeout.
     */
    bool send_and_wait(const GF_Frame& frame, uint32_t timeout_ms = 100);

    /** Envía un frame sin esperar ACK (comandos async). */
    void send_async(const GF_Frame& frame);

    /** Registra un handler para un CMD recibido del ESP32. */
    void on_command(GF_Cmd cmd, gf_master_handler_t handler, void* ctx = nullptr);

    /** Envía HEARTBEAT — llamar cada 1s desde loop() o un IntervalTimer. */
    void send_heartbeat();

    /** true si el ESP32 ha respondido al último heartbeat. */
    bool is_esp32_alive() const { return _esp32_alive; }

private:
    bool     _esp32_alive = false;
    uint8_t  _tx_seq      = 0;

    /* Implementación completa en Sprint Teensy */
};
