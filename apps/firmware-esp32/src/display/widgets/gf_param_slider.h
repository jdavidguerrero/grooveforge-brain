#pragma once

/**
 * @file gf_param_slider.h
 * @brief Slider horizontal hero+snap para vistas de SYNTH GROUP
 *
 * Cada vista de GROUP tiene N páginas (una por parámetro). Solo una página
 * está visible a la vez. Al cambiar de página, animación de slide horizontal
 * (300ms ease-out). Las páginas que entran/salen lo hacen del lado correcto
 * según la dirección del cambio.
 *
 * Layout:
 *   - El slider ocupa toda la pantalla (240×210 efectivos, dejando 30px abajo
 *     para los dots de paginación).
 *   - Cada página es un lv_obj_t* hijo, posicionado con offset relativo al
 *     centro: página activa en x=0, inactiva en x=±240 (offscreen).
 *   - Los dots se renderizan separadamente con gf_page_dots() en la vista.
 *
 * Uso típico:
 *   lv_obj_t* sl = gf_pslider_create(parent, 4);
 *   for (uint8_t i = 0; i < 4; i++) {
 *       lv_obj_t* page = gf_pslider_get_page(sl, i);
 *       // ... pintar contenido en page ...
 *   }
 *   gf_pslider_set_active(sl, 2, true);  // animar a página 2
 */

#include "lvgl.h"
#include <stdint.h>

/**
 * @brief Crea el slider con N páginas vacías (offscreen excepto la 0).
 *
 * El llamante rellena cada página con su contenido custom usando
 * gf_pslider_get_page(). Las páginas son lv_obj_t* sin bg, sin border, no
 * scrollables — el contenido es totalmente del view.
 *
 * @param parent     Pantalla activa.
 * @param num_pages  Número de páginas (2-8 típicamente).
 * @return El contenedor del slider.
 */
lv_obj_t* gf_pslider_create(lv_obj_t* parent, uint8_t num_pages);

/** Retorna la página en posición idx (0-based). */
lv_obj_t* gf_pslider_get_page(lv_obj_t* slider, uint8_t idx);

/**
 * @brief Cambia la página activa con animación de slide horizontal.
 *
 * Si animate=true, las páginas se mueven 300ms ease-out. Si animate=false,
 * el cambio es instantáneo (uso típico: inicialización).
 *
 * La dirección del slide se calcula automáticamente: si idx > current las
 * páginas se mueven hacia la izquierda (página entrante viene de la derecha).
 */
void gf_pslider_set_active(lv_obj_t* slider, uint8_t idx, bool animate);

/** Retorna el índice de la página activa actual. */
uint8_t gf_pslider_get_active(lv_obj_t* slider);

/** Retorna el número total de páginas. */
uint8_t gf_pslider_get_count(lv_obj_t* slider);
