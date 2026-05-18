#pragma once

/**
 * @file gf_anim.h
 * @brief Registro de animaciones/timers del carrusel — GrooveForge Brain
 *
 * Problema: al destruir una vista con lv_obj_clean(), cualquier lv_anim o
 * lv_timer pendiente cuyo callback apunte a un objeto ya liberado dereferencia
 * memoria invalida → crash.
 *
 * Solucion: gf_anim_kill_all() se llama SIEMPRE antes de lv_obj_clean() en cada
 * cambio de vista. Borra todas las animaciones (lv_anim_del global) y todos los
 * lv_timer recurrentes que las vistas hayan registrado.
 *
 * Ver apps/docs/sprints/18-display-ui-carousel.md §5 (ciclo de vida de anims).
 */

#include "lvgl.h"

/**
 * @brief Registra un lv_timer recurrente para que el carrusel lo destruya.
 *
 * Las vistas con animacion continua (LFO, orbita AI, anillo idle, etc.) crean
 * un lv_timer y lo registran aqui. gf_anim_kill_all() lo borra en el cambio de
 * vista, asi el view_destroy() es opcional incluso si la vista olvido definirlo.
 *
 * @param t Timer a registrar (NULL se ignora).
 */
void gf_anim_register_timer(lv_timer_t* t);

/**
 * @brief Mata todas las animaciones y timers registrados.
 *
 * Llamar SIEMPRE antes de lv_obj_clean() al cambiar de vista. Hace:
 *   - lv_anim_del(NULL, NULL): borra toda animacion en curso.
 *   - lv_timer_del() de cada timer registrado.
 */
void gf_anim_kill_all(void);
