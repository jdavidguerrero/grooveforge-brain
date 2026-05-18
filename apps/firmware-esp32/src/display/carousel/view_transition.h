#pragma once

/**
 * @file view_transition.h
 * @brief Transicion radial-wipe entre vistas del carrusel — GrooveForge Brain
 *
 * Mock 09 "Mode Switch": wipe circular purple→teal expandiendose desde el
 * centro, 200ms de salida (cubre) + 200ms de entrada (revela).
 *
 * El objeto del wipe vive en lv_layer_top() — una capa que se dibuja por
 * encima de la pantalla activa y NO es hija de lv_scr_act(), asi sobrevive al
 * lv_obj_clean() que el carrusel hace para destruir la vista saliente.
 */

#include "lvgl.h"

/** Callback que se invoca cuando una mitad de la transicion termina. */
typedef void (*gf_transition_cb_t)(void);

/** Crea el objeto del wipe en el top layer (llamar una vez al iniciar). */
void gf_transition_init(void);

/**
 * @brief Wipe-out: el circulo crece hasta cubrir la pantalla.
 * @param on_done Se invoca cuando la pantalla queda totalmente cubierta
 *                (momento seguro para destruir/crear la vista).
 */
void gf_transition_out(gf_transition_cb_t on_done);

/**
 * @brief Wipe-in: el circulo se contrae revelando la nueva vista.
 * @param on_done Se invoca cuando el wipe desaparece.
 */
void gf_transition_in(gf_transition_cb_t on_done);
