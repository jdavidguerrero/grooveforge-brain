/**
 * @file view_16_scale_lock.cpp
 * @brief Vista 16 — SCALE LOCK (mock GFB-UI-016)
 *
 * Nivel 1 (TinyML / teclado MIDI). Nombre de escala, tonica, 7 degree dots
 * (el marcador IN resalta la ultima nota tocada) y badge TinyML.
 */

#include "views.h"
#include "../../ui_theme.h"
#include "../../widgets/gf_widgets.h"

void view_16_create(lv_obj_t* parent) {
    gf_screen_bg(parent);
    gf_arc_title(parent, "SCALE LOCK", GF_COLOR_TEAL);

    /* A. Nombre de la escala. */
    gf_hero_label(parent, "D DORIAN", GF_FONT_TITLE, GF_COLOR_WHITE, -30);

    /* B. Tonica. */
    gf_hero_label(parent, "TONIC  D", GF_FONT_MICRO, GF_COLOR_TEAL_PLUS, -2);

    /* C/D. 7 degree dots — el marcador IN (grado 3) resalta. */
    const uint8_t in_marker = 2;
    for (uint8_t i = 0; i < 7; i++) {
        const bool is_in = (i == in_marker);
        const lv_coord_t d = is_in ? 11 : 7;
        lv_obj_t* dot = gf_dot(parent, d, is_in ? GF_COLOR_TEAL_PLUS : GF_COLOR_TEAL);
        lv_obj_align(dot, LV_ALIGN_CENTER, -54 + i * 18, 30);
    }

    /* E. Badge TinyML. */
    gf_mode_pill(parent, "TINYML", GF_COLOR_TEAL_PLUS);
}

void view_16_destroy(void) {}
