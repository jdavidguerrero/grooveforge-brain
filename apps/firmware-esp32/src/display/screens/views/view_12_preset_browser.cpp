/**
 * @file view_12_preset_browser.cpp
 * @brief Vista 12 — PRESET BROWSER (mock GFB-UI-012)
 *
 * Navegador de presets: nombre hero enmarcado por brackets, categoria,
 * contador #N/total, y preview strip horizontal de 5 presets.
 */

#include "views.h"
#include "../../ui_theme.h"
#include "../../widgets/gf_widgets.h"

void view_12_create(lv_obj_t* parent) {
    gf_screen_bg(parent);
    gf_arc_title(parent, "PRESET", GF_COLOR_TEAL);

    /* F. Brackets de seleccion alrededor del nombre. */
    lv_obj_t* fr = lv_obj_create(parent);
    lv_obj_set_size(fr, 168, 38);
    lv_obj_set_style_radius(fr, 3, 0);
    lv_obj_set_style_bg_opa(fr, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_color(fr, GF_COLOR_TEAL_PLUS, 0);
    lv_obj_set_style_border_width(fr, 1, 0);
    lv_obj_clear_flag(fr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(fr, LV_ALIGN_CENTER, 0, -22);

    /* B. Nombre del preset. */
    gf_hero_label(parent, "WARM PADS", GF_FONT_TITLE, GF_COLOR_WHITE, -22);

    /* A. Indicador de favorito. */
    lv_obj_t* fav = gf_dot(parent, 7, GF_COLOR_TEAL_PLUS);
    lv_obj_align(fav, LV_ALIGN_CENTER, -74, -22);

    /* C. Categoria. */
    gf_hero_label(parent, "CATEGORY  BASS", GF_FONT_MICRO, GF_COLOR_TEAL_PLUS, 8);

    /* D. Contador. */
    gf_hero_label(parent, "#7 / 64", GF_FONT_MICRO, GF_COLOR_GRAY, 26);

    /* E. Preview strip — 5 presets, el central resaltado. */
    const char*      strip[5] = {"DEEP SUB", "PLUCK", "WARM PADS", "GLASS", "BRASS"};
    const lv_coord_t sx[5]    = {-90, -46, 0, 46, 90};
    for (uint8_t i = 0; i < 5; i++) {
        const bool mid = (i == 2);
        lv_obj_t* l = gf_label(parent, strip[i], GF_FONT_MICRO,
                               mid ? GF_COLOR_WHITE : GF_COLOR_GRAY);
        if (!mid) lv_obj_set_style_opa(l, 90, 0);
        lv_obj_align(l, LV_ALIGN_CENTER, sx[i], 54);
    }

    gf_mode_pill(parent, "BROWSER", GF_COLOR_TEAL);
}

void view_12_destroy(void) {}
