/**
 * @file view_21_error_states.cpp
 * @brief Vista 21 — ERROR STATES (mock GFB-UI-021)
 *
 * Estado de alerta: triangulo de warning, titulo, sub-mensaje, codigo de
 * error y prompt de accion. Tinte rojo de fondo.
 */

#include "views.h"
#include "../../ui_theme.h"
#include "../../widgets/gf_widgets.h"
#include "../../widgets/gf_glyphs.h"

void view_21_create(lv_obj_t* parent) {
    gf_screen_bg(parent);
    /* F. Tinte rojo (~0.18). */
    gf_tint(parent, GF_COLOR_RED, 44);

    /* A. Triangulo de warning. */
    lv_obj_t* warn = gf_glyph_warning(parent, 44, GF_COLOR_RED);
    if (warn != nullptr) lv_obj_align(warn, LV_ALIGN_CENTER, 0, -48);

    /* B. Titulo. */
    gf_hero_label(parent, "BRIDGE TIMEOUT", GF_FONT_BODY, GF_COLOR_RED, 6);

    /* C. Sub-mensaje. */
    gf_hero_label(parent, "NO RESPONSE FROM TEENSY", GF_FONT_MICRO, GF_COLOR_GRAY, 28);

    /* D. Codigo de error. */
    gf_hero_label(parent, "ERR 0x12", GF_FONT_MICRO, GF_COLOR_WHITE, 46);

    /* E. Prompt de accion. */
    gf_hero_label(parent, "PUSH TO RETRY", GF_FONT_MICRO, GF_COLOR_AMBER, 74);
}

void view_21_destroy(void) {}
