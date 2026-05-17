#pragma once

/**
 * @file lv_port_disp.h
 * @brief LVGL display port — GC9A01 240x240 via TFT_eSPI
 *
 * Inicializa el pipeline de rendering:
 *   TFT_eSPI (driver SPI del GC9A01) → LVGL draw buffer → flush callback
 *
 * Llamar una sola vez desde setup() antes de crear cualquier pantalla LVGL.
 * No thread-safe — LVGL en ESP32 corre en un solo core (core 1, Arduino task).
 */

/**
 * @brief Inicializa TFT_eSPI, configura buffers de LVGL y registra el display driver.
 *
 * Post-condicion: lv_scr_act() es valido y puede usarse para crear widgets.
 * El backlight (GPIO 40) queda al 100% de brillo.
 */
void lv_port_disp_init(void);
