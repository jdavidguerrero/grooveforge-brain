/**
 * @file view_09_mode_switch.cpp
 * @brief Vista 09 — MODE SWITCH (mock GFB-UI-009)
 *
 * Keyframe estatico de la transicion de modo: fondo con gradiente purple→teal
 * (el radial-wipe), anillo LED en el borde, y el swap SYNTH → FX al centro.
 * La transicion real entre vistas la ejecuta carousel/view_transition.
 */

#include "views.h"
#include "../../ui_theme.h"
#include "../../widgets/gf_widgets.h"
#include "../../widgets/gf_glyphs.h"

void view_09_create(lv_obj_t* parent) {
    /* B. Fondo con gradiente purple→teal. */
    lv_obj_set_style_bg_color(parent, GF_COLOR_PURPLE, 0);
    lv_obj_set_style_bg_grad_color(parent, GF_COLOR_TEAL, 0);
    lv_obj_set_style_bg_grad_dir(parent, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_bg_opa(parent, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(parent, 0, 0);
    lv_obj_set_style_border_width(parent, 0, 0);
    lv_obj_clear_flag(parent, LV_OBJ_FLAG_SCROLLABLE);

    /* C. Anillo LED en el borde. */
    gf_arc(parent, 228, 3, 0, 360, 100, GF_COLOR_TEAL_PLUS, GF_COLOR_TEAL_PLUS);

    gf_arc_title(parent, "MODE SWITCH", GF_COLOR_WHITE);

    /* A/D/E. El swap SYNTH → FX. */
    gf_hero_label(parent, "SYNTH", GF_FONT_TITLE, GF_COLOR_TEAL_PLUS, -28);
    lv_obj_t* arr = gf_glyph_arrow(parent, 16, GF_COLOR_WHITE, GF_ARROW_DOWN);
    if (arr != nullptr) lv_obj_align(arr, LV_ALIGN_CENTER, 0, 2);
    gf_hero_label(parent, "FX", GF_FONT_HERO, GF_COLOR_WHITE, 38);

    gf_mode_pill(parent, "FX", GF_COLOR_PURPLE_BRIGHT);
}

void view_09_destroy(void) {}
