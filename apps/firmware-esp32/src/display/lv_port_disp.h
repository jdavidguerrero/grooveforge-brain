#pragma once

/**
 * @file lv_port_disp.h
 * @brief LVGL display port — GC9A01 240x240 via TFT_eSPI
 *
 * Inicializa el pipeline de rendering:
 *   TFT_eSPI (driver SPI del GC9A01) → LVGL draw buffer → flush callback
 *
 * Requiere -DUSER_SETUP_LOADED=1 + -DGC9A01_DRIVER=1 + -DUSE_HSPI_PORT=1 en
 * build_flags. Sin estos flags TFT_eSPI crashea (StoreProhibited @ 0x10) porque
 * spiStartBus() en IDF5 recibe un bus_num invalido y deja spi->dev == NULL.
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
