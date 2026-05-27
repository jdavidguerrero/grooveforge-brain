/**
 * @file bridge_master.cpp
 * @brief UART Bridge Protocol — Teensy 4.1 master implementation
 *
 * State machine de recepción byte-a-byte, idéntica a BridgeSlave en estructura
 * para garantizar simetría de parsing en ambos extremos del bus.
 *
 * El Teensy es el master del protocolo:
 *   - Inicia todos los comandos (excepto respuestas del ESP32: ACK, VERSION, STATUS).
 *   - Envía HEARTBEAT cada 1s para mantener el link UP.
 *   - Espera ACK con SEQ matching para comandos síncronos (send_and_wait).
 *   - No necesita enviar ACK de vuelta al ESP32 — el slave solo responde.
 *
 * Por qué byte-a-byte (no DMA/buffer completo):
 *   process_byte() se llama desde poll() que corre en el loop de Arduino.
 *   Teensy 4.1 corre a 600MHz — procesar un byte por ciclo de loop (típico
 *   5-10µs) es más que suficiente para 921600 baud (≈1 byte cada 10µs).
 *   DMA añadiría complejidad sin beneficio medible dado el bajo throughput
 *   del Bridge Protocol vs el reloj del CPU.
 *
 * Refs: apps/docs/02-bridge-protocol.md §1, §4.3, §7.3
 */

#include "bridge_master.h"
#include <Arduino.h>
#include <string.h>

BridgeMaster::BridgeMaster() {
    /* _handlers se inicializa via value-init en la declaración del header (= {}).
     * _rx_frame también, pero lo explicitamos para claridad del lector. */
    memset(&_rx_frame, 0, sizeof(_rx_frame));
}

/* ── init ──────────────────────────────────────────────────────────────────── */

void BridgeMaster::init() {
    /* Teensy 4.1: Serial1 está cableado por hardware a pin 0 (RX) y pin 1 (TX).
     * No se pasan pines como argumentos — Teensyduino los fija en la variante
     * HardwareSerial de Serial1. ESP32 GPIO43=TX, GPIO44=RX. */
    Serial1.begin(921600, SERIAL_8N1);

    _rx_state     = RxState::IDLE;
    _esp32_alive  = false;
    _last_hb_ack_ms = 0;
    _tx_seq       = 0;
    _ack_received = false;

    /* Handler interno para GF_CMD_ACK — actualiza estado de conexión y
     * desbloquea send_and_wait() cuando el SEQ coincide. */
    on_command(GF_CMD_ACK, [](const GF_Frame* f, void* ctx) {
        BridgeMaster* self = static_cast<BridgeMaster*>(ctx);
        /* Payload del ACK: 1 byte = SEQ del frame que se está confirmando */
        if (f->len >= 1) {
            uint8_t acked_seq = f->payload[0];
            if (acked_seq == self->_pending_ack_seq) {
                self->_ack_received = true;
            }
            /* Todo ACK del ESP32 indica que está vivo */
            self->_esp32_alive    = true;
            self->_last_hb_ack_ms = millis();
        }
    }, this);

    /* Handler interno para GF_CMD_NACK — el ESP32 rechazó un frame nuestro.
     * Payload: [seq_nacked: 1B, reason: 1B]
     * Loguea el motivo para facilitar debug. No reintenta automáticamente:
     * los comandos del sketch son fire-and-forget (async), el retry es
     * responsabilidad de la capa superior si se implementa en el futuro. */
    on_command(GF_CMD_NACK, [](const GF_Frame* f, void* ctx) {
        BridgeMaster* self = static_cast<BridgeMaster*>(ctx);
        /* El NACK también confirma que el ESP32 está vivo y parseando */
        self->_esp32_alive    = true;
        self->_last_hb_ack_ms = millis();

        if (f->len >= 2) {
            uint8_t nacked_seq = f->payload[0];
            uint8_t reason     = f->payload[1];
            static const char* reasons[] = {
                "?",           /* 0x00 — no definido */
                "CRC_ERR",     /* 0x01 */
                "BAD_CMD",     /* 0x02 */
                "BAD_LEN",     /* 0x03 */
                "BAD_PARAM",   /* 0x04 */
                "OOM",         /* 0x05 */
            };
            const char* r_str = (reason < 6) ? reasons[reason] : "UNKNOWN";
            Serial.printf("[master] NACK seq=%u reason=0x%02X (%s)\n",
                          nacked_seq, reason, r_str);
        } else {
            Serial.printf("[master] NACK (malformed, len=%u)\n", f->len);
        }
    }, this);

    Serial.printf("[master] init — Serial1 @ 921600 baud, RX=pin0, TX=pin1\n");
    Serial.flush();
}

/* ── poll ──────────────────────────────────────────────────────────────────── */

void BridgeMaster::poll() {
    /* Timeout de frame parcial: protege contra bytes perdidos que dejarían
     * la máquina de estados colgada en READING_PAYLOAD indefinidamente. */
    if (_rx_state != RxState::IDLE && _rx_state != RxState::GOT_CMD) {
        if (millis() - _rx_start_ms > BRIDGE_FRAME_TIMEOUT_MS) {
            Serial.println("[master] frame timeout — discarding partial frame");
            reset_rx();
        }
    }

    /* Watchdog de heartbeat: si el ESP32 no responde en 3s, marca DOWN. */
    if (_esp32_alive && (millis() - _last_hb_ack_ms > BRIDGE_HEARTBEAT_TIMEOUT_MS)) {
        _esp32_alive = false;
        Serial.println("[master] ESP32 DOWN — heartbeat ACK timeout");
        Serial.flush();
    }

    /* Procesar todos los bytes disponibles este ciclo */
    while (Serial1.available() > 0) {
        process_byte((uint8_t)Serial1.read());
    }
}

/* ── send_and_wait ─────────────────────────────────────────────────────────── */

bool BridgeMaster::send_and_wait(const GF_Frame& frame, uint32_t timeout_ms) {
    _pending_ack_seq = frame.seq;
    _ack_received    = false;

    send_raw(frame);

    uint32_t deadline = millis() + timeout_ms;
    while (millis() < deadline) {
        /* Procesar bytes que llegan mientras esperamos el ACK.
         * dispatch_frame() seteará _ack_received cuando el ACK llegue. */
        while (Serial1.available() > 0) {
            process_byte((uint8_t)Serial1.read());
        }
        if (_ack_received) return true;
        /* yield() cede el control al sistema operativo cooperativo de Arduino/Teensyduino
         * (yield hook). Evita watchdog reset durante waits largos. */
        yield();
    }

    Serial.printf("[master] send_and_wait timeout — cmd=0x%02X seq=%u\n",
                  frame.cmd, frame.seq);
    return false;
}

/* ── send_async ────────────────────────────────────────────────────────────── */

void BridgeMaster::send_async(const GF_Frame& frame) {
    send_raw(frame);
}

/* ── on_command ────────────────────────────────────────────────────────────── */

void BridgeMaster::on_command(GF_Cmd cmd, gf_master_handler_t handler, void* ctx) {
    _handlers[(uint8_t)cmd].fn  = handler;
    _handlers[(uint8_t)cmd].ctx = ctx;
}

/* ── send_heartbeat ────────────────────────────────────────────────────────── */

void BridgeMaster::send_heartbeat() {
    GF_Frame f;
    gf_frame_build(&f, GF_CMD_HEARTBEAT, _tx_seq++, nullptr, 0);
    send_raw(f);
}

/* ── send_raw ──────────────────────────────────────────────────────────────── */

void BridgeMaster::send_raw(const GF_Frame& frame) {
    uint8_t wire[GF_PAYLOAD_MAX + GF_FRAME_MIN_SIZE];
    size_t len = gf_frame_serialize(&frame, wire, sizeof(wire));
    if (len > 0) {
        Serial1.write(wire, len);
    }
}

/* ── process_byte ──────────────────────────────────────────────────────────── */

void BridgeMaster::process_byte(uint8_t byte) {
    switch (_rx_state) {
    case RxState::IDLE:
        _rx_frame.cmd = byte;
        _rx_start_ms  = millis();
        _rx_state     = RxState::GOT_CMD;
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
            /* Longitud inválida: el slave nunca debería enviar esto,
             * pero si ocurre (corrupción de bus) hay que descartarlo. */
            Serial.printf("[master] NACK invalid len=%u — discarding\n", _rx_frame.len);
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
            Serial.printf("[master] CRC error cmd=0x%02X seq=%u expected=0x%02X got=0x%02X\n",
                          _rx_frame.cmd, _rx_frame.seq, expected, byte);
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

/* ── dispatch_frame ────────────────────────────────────────────────────────── */

void BridgeMaster::dispatch_frame() {
    GF_Cmd cmd = (GF_Cmd)_rx_frame.cmd;

    HandlerEntry& h = _handlers[(uint8_t)cmd];
    if (h.fn != nullptr) {
        h.fn(&_rx_frame, h.ctx);
    } else {
        /* Frame sin handler registrado — log pero no crashear.
         * El ESP32 puede enviar comandos iniciados por él mismo (CLOUD_STATUS,
         * OTA_AVAILABLE...) que el master aún no maneja en este sprint. */
        Serial.printf("[master] unhandled cmd=0x%02X seq=%u len=%u\n",
                      _rx_frame.cmd, _rx_frame.seq, _rx_frame.len);
    }
}

/* ── reset_rx ──────────────────────────────────────────────────────────────── */

void BridgeMaster::reset_rx() {
    _rx_state = RxState::IDLE;
    _rx_bytes = 0;
    memset(&_rx_frame, 0, sizeof(_rx_frame));
}
