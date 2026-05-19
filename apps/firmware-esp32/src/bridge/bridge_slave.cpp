/**
 * @file bridge_slave.cpp
 * @brief UART Bridge Protocol — ESP32-S3 slave implementation
 *
 * State machine de recepción byte-a-byte para coexistir con lv_task_handler().
 * Ver teoría completa en apps/docs/sprints/19-bridge-protocol.md §Teoría §3.
 */

#include "bridge_slave.h"
#include <Arduino.h>
#include <string.h>

/* GPIO17(TX) / GPIO16(RX) — pines UART del conector Waveshare ESP32-S3-Touch-LCD-1.28.
 * GPIO43/44 = UART0 = Serial (debug) — NO usar para bridge.
 * Serial1 con pines explícitos 16/17 no tiene conflicto con Serial debug. */
#define BRIDGE_UART_RX_PIN  16
#define BRIDGE_UART_TX_PIN  17
#define BRIDGE_UART_BAUD    921600

BridgeSlave::BridgeSlave() {
    memset(_handlers, 0, sizeof(_handlers));
    memset(&_rx_frame, 0, sizeof(_rx_frame));
}

void BridgeSlave::init() {
    // Serial1 con pines explícitos GPIO18(RX) / GPIO17(TX).
    // GPIO43/44 = UART0/Serial (debug) → no conflicto con Serial1 en 17/18.
    Serial1.begin(BRIDGE_UART_BAUD, SERIAL_8N1, BRIDGE_UART_RX_PIN, BRIDGE_UART_TX_PIN);
    _rx_state   = RxState::IDLE;
    _connected  = false;
    _last_hb_ms = 0;
    _tx_seq     = 0;
    Serial.printf("[bridge] init — Serial1 @ %u baud (GPIO%d=RX GPIO%d=TX)\n",
                  BRIDGE_UART_BAUD, BRIDGE_UART_RX_PIN, BRIDGE_UART_TX_PIN);
    Serial.flush();
}

void BridgeSlave::poll() {
    /* Timeout de frame parcial: si el frame no termina en BRIDGE_FRAME_TIMEOUT_MS,
     * descartar y volver a IDLE para no bloquear el bus. */
    if (_rx_state != RxState::IDLE && _rx_state != RxState::GOT_CMD) {
        if (millis() - _rx_start_ms > BRIDGE_FRAME_TIMEOUT_MS) {
            Serial.println("[bridge] frame timeout — discarding partial frame");
            reset_rx();
        }
    }

    /* Heartbeat watchdog */
    if (_connected && (millis() - _last_hb_ms > BRIDGE_HEARTBEAT_TIMEOUT_MS)) {
        _connected = false;
        Serial.println("[bridge] connection DOWN — heartbeat timeout");
        Serial.flush();
    }

    /* Procesar todos los bytes disponibles este ciclo */
    while (Serial1.available() > 0) {
        process_byte((uint8_t)Serial1.read());
    }
}

void BridgeSlave::process_byte(uint8_t byte) {
    switch (_rx_state) {
    case RxState::IDLE:
        _rx_frame.cmd  = byte;
        _rx_start_ms   = millis();
        _rx_state      = RxState::GOT_CMD;
        break;

    case RxState::GOT_CMD:
        _rx_frame.len = byte;
        _rx_bytes     = 0;
        _rx_state     = RxState::GOT_LEN;
        break;

    case RxState::GOT_LEN:
        _rx_frame.seq = byte;
        if (_rx_frame.len == 0) {
            _rx_state = RxState::AWAIT_CRC;
        } else if (_rx_frame.len > GF_PAYLOAD_MAX) {
            /* Longitud inválida — NACK y reset */
            send_nack(_rx_frame.seq, GF_NACK_INVALID_LEN);
            reset_rx();
        } else {
            _rx_state = RxState::READING_PAYLOAD;
        }
        break;

    case RxState::READING_PAYLOAD:
        _rx_frame.payload[_rx_bytes++] = byte;
        if (_rx_bytes >= _rx_frame.len) {
            _rx_state = RxState::AWAIT_CRC;
        }
        break;

    case RxState::AWAIT_CRC: {
        uint8_t expected = gf_frame_crc(&_rx_frame);
        if (byte != expected) {
            send_nack(_rx_frame.seq, GF_NACK_CRC_ERROR);
            reset_rx();
        } else {
            dispatch_frame();
            reset_rx();
        }
        break;
    }

    default:
        reset_rx();
        break;
    }
}

void BridgeSlave::dispatch_frame() {
    GF_Cmd cmd = (GF_Cmd)_rx_frame.cmd;

    /* Handler registrado para este CMD */
    HandlerEntry& h = _handlers[(uint8_t)cmd];
    if (h.fn != nullptr) {
        h.fn(&_rx_frame, h.ctx);
    } else {
        /* Sin handler específico: ACK genérico (no bloquear el bus) */
        send_ack(_rx_frame.seq);
    }
}

void BridgeSlave::send(const GF_Frame& frame) {
    uint8_t wire[GF_PAYLOAD_MAX + GF_FRAME_MIN_SIZE];
    size_t len = gf_frame_serialize(&frame, wire, sizeof(wire));
    if (len > 0) {
        Serial1.write(wire, len);
    }
}

void BridgeSlave::on_command(GF_Cmd cmd, gf_cmd_handler_t handler, void* ctx) {
    _handlers[(uint8_t)cmd].fn  = handler;
    _handlers[(uint8_t)cmd].ctx = ctx;
}

void BridgeSlave::send_ack(uint8_t seq) {
    GF_Frame f;
    uint8_t payload[1] = { seq };
    gf_frame_build(&f, GF_CMD_ACK, _tx_seq++, payload, 1);
    send(f);
}

void BridgeSlave::send_nack(uint8_t seq, GF_NackReason reason) {
    GF_Frame f;
    uint8_t payload[2] = { seq, (uint8_t)reason };
    gf_frame_build(&f, GF_CMD_NACK, _tx_seq++, payload, 2);
    send(f);
}

void BridgeSlave::mark_heartbeat() {
    _last_hb_ms = millis();
    _connected  = true;
}

void BridgeSlave::reset_rx() {
    _rx_state = RxState::IDLE;
    _rx_bytes = 0;
    memset(&_rx_frame, 0, sizeof(_rx_frame));
}
