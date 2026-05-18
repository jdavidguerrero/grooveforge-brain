/**
 * @file bridge_master.cpp
 * @brief UART Bridge Protocol — Teensy 4.1 master (skeleton Sprint 3.2)
 *
 * Implementación completa pendiente para el sprint de firmware Teensy.
 * Este archivo compilará sin errores y sirve como placeholder estructural.
 */

#include "bridge_master.h"

void BridgeMaster::init() {
    /* TODO Sprint Teensy: Serial1.begin(921600, SERIAL_8N1); */
}

void BridgeMaster::poll() {
    /* TODO Sprint Teensy: leer bytes de Serial1, state machine análoga a BridgeSlave */
}

bool BridgeMaster::send_and_wait(const GF_Frame& frame, uint32_t timeout_ms) {
    (void)frame; (void)timeout_ms;
    /* TODO Sprint Teensy: serializar + esperar ACK con timeout */
    return false;
}

void BridgeMaster::send_async(const GF_Frame& frame) {
    (void)frame;
    /* TODO Sprint Teensy: serializar + escribir a Serial1 */
}

void BridgeMaster::on_command(GF_Cmd cmd, gf_master_handler_t handler, void* ctx) {
    (void)cmd; (void)handler; (void)ctx;
    /* TODO Sprint Teensy: tabla de handlers análoga a BridgeSlave */
}

void BridgeMaster::send_heartbeat() {
    /* TODO Sprint Teensy: construir frame HEARTBEAT y enviar async */
    GF_Frame f;
    gf_frame_build(&f, GF_CMD_HEARTBEAT, _tx_seq++, nullptr, 0);
    send_async(f);
}
