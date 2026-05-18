#pragma once

/**
 * @file gf_glyphs.h
 * @brief Glifos line-art vectoriales sobre lv_canvas — GrooveForge Brain
 *
 * Los simbolos geometricos de los mocks (waveforms, flechas, checkmark,
 * triangulo de warning, glifos FX) NO existen en IBM Plex Mono. Se dibujan
 * como vector line-art sobre un lv_canvas — coherente con la spec de los mocks,
 * que los describe como "vector line-art, stroke 1.2-1.4px".
 *
 * Gestion de memoria: cada canvas reserva su buffer (TRUE_COLOR_ALPHA, 3 B/px)
 * en PSRAM y registra un callback LV_EVENT_DELETE que lo libera. Asi el
 * lv_obj_clean() del carrusel limpia los buffers automaticamente.
 */

#include "lvgl.h"

/** Formas de onda para gf_glyph_wave(). */
typedef enum {
    GF_WAVE_SINE,
    GF_WAVE_TRI,
    GF_WAVE_SAW,
    GF_WAVE_SQUARE,
    GF_WAVE_PULSE,
} gf_wave_t;

/** Direccion de gf_glyph_arrow(): a donde apunta el triangulo. */
typedef enum {
    GF_ARROW_RIGHT,
    GF_ARROW_LEFT,
    GF_ARROW_UP,
    GF_ARROW_DOWN,
} gf_arrow_dir_t;

/**
 * @brief Crea un lv_canvas con buffer propio (PSRAM, autoliberado al destruir).
 *
 * El canvas queda relleno transparente, listo para dibujar encima. El caller
 * lo posiciona con lv_obj_align(). Tipico para construir un glifo.
 *
 * @return El canvas, o NULL si no se pudo reservar el buffer.
 */
lv_obj_t* gf_canvas_create(lv_obj_t* parent, lv_coord_t w, lv_coord_t h);

/**
 * @brief Glifo de forma de onda (un periodo) dibujado como polilinea.
 */
lv_obj_t* gf_glyph_wave(lv_obj_t* parent, gf_wave_t wave, lv_coord_t w, lv_coord_t h,
                        lv_color_t color);

/**
 * @brief Triangulo solido apuntando en una direccion (flecha de edicion/nav).
 * @param size Lado del bounding box en px.
 */
lv_obj_t* gf_glyph_arrow(lv_obj_t* parent, lv_coord_t size, lv_color_t color,
                         gf_arrow_dir_t dir);

/**
 * @brief Checkmark (✓) dibujado como dos segmentos de stroke redondeado.
 */
lv_obj_t* gf_glyph_check(lv_obj_t* parent, lv_coord_t size, lv_color_t color);

/**
 * @brief Triangulo de warning con signo de exclamacion (estados de error).
 */
lv_obj_t* gf_glyph_warning(lv_obj_t* parent, lv_coord_t size, lv_color_t color);

/**
 * @brief Glifo line-art de uno de los 12 FX (indice 0-11).
 *        Cada FX tiene una marca geometrica distinta.
 */
lv_obj_t* gf_glyph_fx(lv_obj_t* parent, uint8_t fx_index, lv_coord_t size, lv_color_t color);
