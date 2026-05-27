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

/** Wet/dry del FX activo (0.0–1.0), actualizado por PARAM_CHANGED param_id=0xFF. */
float bridge_get_wet_dry(void);

/** Cursor en FX_SELECT (0-8), actualizado por PARAM_CHANGED param_id=0xFE. */
uint8_t bridge_get_fx_cursor(void);

/** Cursor en PARAM_EDIT (0-7), 0xFF = modo FX_MAIN (no editando param). */
uint8_t bridge_get_param_cursor(void);

/** Último valor de parámetro recibido, actualizado por PARAM_CHANGED real. */
float bridge_get_param_value(void);

/** Nombre del parámetro [fx_idx][param_idx] — tabla espejo del sketch 27. */
const char* bridge_get_param_name(uint8_t fx, uint8_t param);

/** Nivel de banda espectral [0-7] — actualizado por PARAM_CHANGED 0xE0-0xE7.
 *  0.0 = silencio, 1.0 = nivel máximo. */
float bridge_get_spectrum(uint8_t band);

/** Modo top-level activo: 0=FX, 1=SYNTH, 2=AI.
 *  Actualizado por PARAM_CHANGED param_id=0xFD. */
uint8_t bridge_get_top_mode(void);

/** Progreso del arm de AI mode (0.0–1.0). -1.0 = cancelado / inactivo.
 *  Actualizado por PARAM_CHANGED param_id=0xFB. */
float bridge_get_ai_progress(void);

/** Caché de valores de parámetros por FX. Actualizado por PARAM_CHANGED. */
float bridge_get_param_cached(uint8_t fx, uint8_t param);

/** Índice del sub-parámetro 1 (ENC L) para un FX dado. */
uint8_t bridge_get_sub1(uint8_t fx);

/** Índice del sub-parámetro 2 (ENC R) para un FX dado. */
uint8_t bridge_get_sub2(uint8_t fx);

/** Último param_id de un PARAM_CHANGED real (excluye especiales 0xF8-0xFF y spectrum 0xE0-0xE7). */
uint16_t bridge_get_last_real_param_id(void);

/** Timestamp (lv_tick_get()) del último PARAM_CHANGED real. */
uint32_t bridge_get_last_real_param_ms(void);

/** Nivel de navegación SYNTH: 0=ENGINE_LIST, 1=ENGINE_SUBHOME, 2=GROUP_VIEW. */
uint8_t bridge_get_synth_level(void);

/** Grupo activo en modo SYNTH: 0=OSC, 1=ENV, 2=FILTER, 3=LFO. */
uint8_t bridge_get_synth_group(void);

/** Cursor de sub-parámetro dentro del grupo activo. */
uint8_t bridge_get_synth_param(void);

/** Cursor en ENGINE_LIST (0-based), actualizado por PARAM_CHANGED param_id=0xFE en modo SYNTH. */
uint8_t bridge_get_engine_cursor(void);

/** Cutoff del filtro del engine activo (0.0–1.0). Actualizado por PARAM_CHANGED grupo FILTER param 0. */
float bridge_get_synth_cutoff(void);

/** Resonancia del filtro del engine activo (0.0–1.0). Actualizado por PARAM_CHANGED grupo FILTER param 1. */
float bridge_get_synth_resonance(void);

/** true si Scale Lock está bypassed. Actualizado por PARAM_CHANGED param_id=0x00F2. */
bool bridge_get_scale_lock_bypass(void);

/** true si Beat Follower está bypassed. Actualizado por PARAM_CHANGED param_id=0x00F3. */
bool bridge_get_beat_follower_bypass(void);

/**
 * @brief Valor cacheado de un parámetro synth (normalizado 0.0–1.0).
 *
 * Indexado por engine (0=Moog, 1=Juno, 2=Prophet), grupo (0=OSC, 1=ENV,
 * 2=FILTER, 3=LFO) y param dentro del grupo (0-3).
 * Actualizado por cada PARAM_CHANGED con param_id byte-alto 0x10-0x12.
 * Retorna 0.0f si el param aún no ha sido enviado por el Teensy.
 */
float bridge_get_synth_param_cached(uint8_t engine, uint8_t group, uint8_t pidx);
