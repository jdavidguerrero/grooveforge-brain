#pragma once

/**
 * @file device_state.h
 * @brief Persistencia de estado en EEPROM — GrooveForge Brain (Sprint 35)
 *
 * Guarda y restaura el estado del dispositivo entre arranques usando el
 * EEPROM interno del Teensy 4.1 (emulado en flash, 4284 bytes disponibles).
 *
 * ## ¿Qué se guarda?
 *
 * Solo las variables de navegación de alto nivel — no parámetros de síntesis
 * (muy frecuentes → wear en EEPROM) ni configuración de AI (requiere hold gesture):
 *
 *   display_mode   — último modo visible (FX=0, SYNTH=1)
 *   active_engine  — último engine activo (Moog=0, Juno=1, Prophet=2)
 *
 * Al arrancar, el sketch navega al SUBHOME del engine guardado en el modo guardado.
 * El usuario no ve la pantalla inicial por defecto — ve exactamente donde estaba.
 *
 * ## Wear de EEPROM
 *
 * El Teensy 4.1 usa EEPROM emulada en la flash (100K ciclos por byte).
 * Solo guardamos en eventos de cambio de modo y de engine — típicamente
 * 5-15 veces por sesión. A 20 cambios/día → 100K/20 = 13 años de vida útil.
 * NO se guarda en cada cambio de parámetro de síntesis.
 *
 * ## Layout en EEPROM
 *
 *   Addr 0: magic byte (0xAB = estado válido)
 *   Addr 1: display_mode (0=FX, 1=SYNTH)
 *   Addr 2: active_engine (0=Moog, 1=Juno, 2=Prophet)
 *   Addr 3-7: reserved (0x00)
 *   Total: 8 bytes
 *
 * Spec: apps/docs/sprints/29-hardware-integration.md §"Persistencia de estado"
 */

#include <stdint.h>

/* ── Magic byte para detección de EEPROM virgen ──────────────────────────── */
static constexpr uint8_t DEVICE_STATE_MAGIC = 0xAB;

/* ── Dirección base en EEPROM ────────────────────────────────────────────── */
static constexpr uint16_t DEVICE_STATE_ADDR = 0;

/**
 * @brief Estructura de estado persistible.
 *
 * Campos dentro de rango — validados en device_state_load().
 * Si algún campo está fuera de rango al leer, se usa el estado por defecto.
 */
struct DeviceState {
    uint8_t magic;          ///< 0xAB = estado guardado válido
    uint8_t display_mode;   ///< 0=FX, 1=SYNTH
    uint8_t active_engine;  ///< 0=Moog, 1=Juno, 2=Prophet
    uint8_t reserved[5];    ///< reservado para versiones futuras
};

/**
 * @brief Guarda el estado actual en EEPROM.
 *
 * Usa EEPROM.put() que solo escribe bytes que cambian (wear-leveling básico
 * de Teensyduino — no escribe si el byte ya tiene el mismo valor).
 *
 * @param s  Estado a guardar.
 */
void device_state_save(const DeviceState& s);

/**
 * @brief Lee el estado desde EEPROM.
 *
 * Si el magic byte es inválido o algún campo está fuera de rango,
 * retorna false y `s` queda sin modificar.
 *
 * @param[out] s  Estado leído.
 * @return true si el estado leído es válido y fue aplicado a `s`.
 */
bool device_state_load(DeviceState& s);

/**
 * @brief Retorna el estado por defecto (SYNTH mode, Moog engine).
 * Usado en el primer arranque o cuando el EEPROM está virgen.
 */
DeviceState device_state_default();
