#pragma once

/**
 * @file screen_main.h
 * @brief Pantalla principal — GrooveForge Brain
 *
 * Layout circular GC9A01 240x240 con design language GroovePilot:
 *
 *        purple ring (completo, 8px stroke)
 *           ┌─────────────────────┐
 *          ╱   [ENGINE]  (gris12)  ╲
 *         │                         │
 *         │   MOOG                  │  ← Montserrat 24, blanco
 *         │   MODEL D               │
 *         │                         │
 *         │   TAPE · CHORUS         │  ← Montserrat 12, gris
 *          ╲                       ╱
 *            ●  READY  (teal+gris)
 *           └─────────────────────┘
 *
 * En Sprint 3.2 los datos llegaran via Bridge Protocol desde Teensy.
 * Por ahora usa placeholders hardcodeados.
 */

/**
 * @brief Crea la pantalla principal con layout GroovePilot.
 *
 * Llamar desde main.cpp cuando screen_boot_done() retorna true,
 * despues de lv_obj_clean(lv_scr_act()) para liberar widgets del boot.
 */
void screen_main_create(void);

/**
 * @brief Actualiza el nombre del engine sintetizador en pantalla.
 *
 * @param name  Nombre del engine, puede incluir '\n' para multilinea.
 *              Ej: "MOOG\nMODEL D"
 *
 * @note Stub para Sprint 3.2 (Bridge Protocol integration).
 */
void screen_main_set_engine(const char* name);

/**
 * @brief Actualiza la cadena de FX activos en pantalla.
 *
 * @param fx_chain  String descriptivo del chain activo.
 *                  Ej: "TAPE \xC2\xB7 CHORUS" (UTF-8 middle dot)
 *
 * @note Stub para Sprint 3.2.
 */
void screen_main_set_fx(const char* fx_chain);

/**
 * @brief Actualiza el indicador de conexion con Teensy.
 *
 * @param connected  true = dot verde + "READY", false = dot rojo + "OFFLINE"
 *
 * @note Stub para Sprint 3.2.
 */
void screen_main_set_status(bool connected);
