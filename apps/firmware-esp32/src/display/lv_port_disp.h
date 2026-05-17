#pragma once

/**
 * @file lv_port_disp.h
 * @brief LVGL display port — GC9A01 240x240 via LovyanGFX
 *
 * Inicializa el pipeline de rendering:
 *   LovyanGFX (driver SPI del GC9A01) → LVGL draw buffer → flush callback
 *
 * LovyanGFX reemplaza TFT_eSPI por compatibilidad con Arduino ESP32 v3.x (IDF 5.x).
 * TFT_eSPI 2.5.x crashea en IDF5 porque spiStartBus() retorna un spi_t* con
 * spi->dev == NULL → StoreProhibited al escribir al offset 0x10.
 *
 * Llamar una sola vez desde setup() antes de crear cualquier pantalla LVGL.
 * No thread-safe — LVGL en ESP32 corre en un solo core (core 1, Arduino task).
 */

/**
 * @brief Inicializa LovyanGFX, configura buffers de LVGL y registra el display driver.
 *
 * Post-condicion: lv_scr_act() es valido y puede usarse para crear widgets.
 * El backlight (GPIO 40) queda al 100% de brillo via PWM de LovyanGFX.
 */
void lv_port_disp_init(void);
