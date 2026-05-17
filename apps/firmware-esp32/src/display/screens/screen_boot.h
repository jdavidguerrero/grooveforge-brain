#pragma once

/**
 * @file screen_boot.h
 * @brief Pantalla de arranque animada — GrooveForge Brain
 *
 * Secuencia de animacion (duracion total ~2.5s):
 *
 *   0 ms    — Arc purple crece de 270 deg barriendo 360 deg clockwise (800ms)
 *   600 ms  — "GROOVEFORGE" fade-in blanco, Montserrat 16 (400ms)
 *   1000 ms — "BRAIN" fade-in gris, Montserrat 12 (400ms)
 *   1600 ms — dot teal "●" fade-in como ready indicator (300ms)
 *   2200 ms — screen_boot_done() retorna true → main.cpp crea screen_main
 *
 * Implementacion: LVGL 8 lv_anim_t con lv_anim_path_ease_out para el arc
 * y fade lineal para los labels via lv_obj_set_style_opa().
 */

/**
 * @brief Crea la pantalla de boot y lanza las animaciones LVGL.
 *
 * Debe llamarse despues de lv_port_disp_init().
 * No debe llamarse mas de una vez.
 */
void screen_boot_create(void);

/**
 * @brief Retorna true cuando la secuencia de boot termino.
 *
 * Implementado con lv_tick_get() > 2200 para ser determinista
 * independientemente de la velocidad del loop().
 *
 * @return true si han pasado >= 2200ms desde lv_init()
 */
bool screen_boot_done(void);
