#pragma once

/**
 * @file bridge_handlers.h
 * @brief Handlers de comandos Bridge — ESP32-S3
 *
 * Registra los handlers específicos de display/engine en un BridgeSlave.
 * Llama a bridge_handlers_init() una vez en setup() antes de bridge.init().
 */

#include "bridge_slave.h"

/**
 * @brief Registra todos los handlers de comandos en el slave.
 *
 * Handlers en scope de Sprint 3.2:
 *   HEARTBEAT       → keepalive, marca conexión UP
 *   GET_VERSION     → responde con VERSION(1,0,0)
 *   ENGINE_CHANGED  → loguea engine_id + nombre
 *   PARAM_CHANGED   → loguea param_id + value
 *   SET_ENGINE      → ACK
 *   DISPLAY_UPDATE  → ACK (display es autónomo en carrusel por ahora)
 *   CLOUD_STATUS    → loguea connected + latency
 *   resto de CMDs   → ACK genérico (sin acción)
 */
void bridge_handlers_init(BridgeSlave& slave);

/** Nombre del engine activo (actualizado por ENGINE_CHANGED). */
const char* bridge_get_engine_name(void);

/** ID del engine activo (actualizado por ENGINE_CHANGED). */
uint8_t bridge_get_engine_id(void);

/** true si el ESP32 está conectado al cloud (actualizado por CLOUD_STATUS). */
bool bridge_get_cloud_connected(void);
