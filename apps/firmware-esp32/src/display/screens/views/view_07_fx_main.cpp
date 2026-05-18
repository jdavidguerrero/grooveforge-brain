/**
 * @file view_07_fx_main.cpp
 * @brief Vista 07 — FX · MAIN (mock GFB-UI-007)
 *
 * Modo FX: chrome purpura. Dos arcos laterales (WET izquierda, DEPTH derecha)
 * enmarcan el nombre del FX activo. Tres layer dots (INS/SND/MST).
 */

#include "views.h"
#include "../../ui_theme.h"
#include "../../widgets/gf_widgets.h"

void view_07_create(lv_obj_t* parent) {
    gf_screen_bg(parent);
    /* A. Tinte purpura de modo FX (~0.18). */
    gf_tint(parent, GF_COLOR_PURPLE, 46);

    /* B. Titulo de la capa activa. */
    gf_arc_title(parent, "INSERT", GF_COLOR_PURPLE_BRIGHT);

    /* D. Arco WET — lado izquierdo. */
    gf_arc(parent, 226, 6, 118, 242, 65,
           GF_COLOR_PURPLE_BRIGHT, GF_COLOR_TEAL_DIM);
    /* E. Arco DEPTH — lado derecho (envuelve por 0°). */
    gf_arc(parent, 226, 6, 298, 62, 40,
           GF_COLOR_PURPLE_BRIGHT, GF_COLOR_TEAL_DIM);

    /* C. Nombre del FX — hero blanco 2 lineas. */
    gf_hero_label(parent, "TAPE\nSATURATE", GF_FONT_TITLE, GF_COLOR_WHITE, -4);

    /* Lecturas de los arcos. */
    lv_obj_t* wl = gf_label(parent, "WET 65", GF_FONT_MICRO, GF_COLOR_GRAY);
    lv_obj_align(wl, LV_ALIGN_LEFT_MID, 10, 44);
    lv_obj_t* dl = gf_label(parent, "DEP 40", GF_FONT_MICRO, GF_COLOR_GRAY);
    lv_obj_align(dl, LV_ALIGN_RIGHT_MID, -10, 44);

    /* F. Layer dots — INS/SND/MST, activo INS. */
    gf_page_dots(parent, 3, 0, GF_COLOR_PURPLE_BRIGHT);

    gf_mode_pill(parent, "FX", GF_COLOR_PURPLE_BRIGHT);
}

void view_07_destroy(void) {}
