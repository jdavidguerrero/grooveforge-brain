/**
 * @file view_02_synth_main.cpp
 * @brief Vista 02 — SYNTH · MAIN (mock GFB-UI-002)
 *
 * Elementos del mock:
 *   A. Engine arc title  — teal, arco superior
 *   B. Hero note         — blanco 64px, nota tocada
 *   C. Param readout     — gris, parametro vivo
 *   D. Velocity bar      — barra teal
 *   E. Page dots         — 5 dots, activo el 2do
 *   F. Mode pill         — "SYNTH", outline teal
 */

#include "views.h"
#include "../../ui_theme.h"
#include "../../widgets/gf_widgets.h"

void view_02_create(lv_obj_t* parent) {
    gf_screen_bg(parent);

    /* A. Titulo del engine siguiendo el arco superior. */
    gf_arc_title(parent, "MOOG MODEL D", GF_COLOR_TEAL);

    /* B. Nota hero — 64px (fuente GIANT, set restringido 0-9 # A-G). */
    gf_hero_label(parent, "A3", GF_FONT_GIANT, GF_COLOR_WHITE, -12);

    /* C. Lectura de parametro vivo. */
    gf_hero_label(parent, "CUTOFF  62%", GF_FONT_LABEL, GF_COLOR_GRAY, 40);

    /* D. Barra de velocidad — teal, ~72%. */
    lv_obj_t* vel = gf_bar(parent, 84, 3, 72, GF_COLOR_TEAL);
    lv_obj_align(vel, LV_ALIGN_CENTER, 0, 60);

    /* E. Page dots — 5 paginas del synth, activa la 2da. */
    gf_page_dots(parent, 5, 1, GF_COLOR_TEAL);

    /* F. Mode pill SYNTH. */
    gf_mode_pill(parent, "SYNTH", GF_COLOR_TEAL);
}

void view_02_destroy(void) {}
