/**
 * @file view_15_master_layer.cpp
 * @brief Vista 15 — MASTER LAYER (mock GFB-UI-015)
 *
 * Capa master: flecha de signal flow + grilla 2×2 de FX de bus master.
 * Tinte purpura de modo FX.
 */

#include "views.h"
#include "../../ui_theme.h"
#include "../../widgets/gf_widgets.h"
#include "../../widgets/gf_glyphs.h"

void view_15_create(lv_obj_t* parent) {
    gf_screen_bg(parent);
    gf_tint(parent, GF_COLOR_PURPLE, 46);
    gf_arc_title(parent, "MASTER", GF_COLOR_PURPLE_BRIGHT);

    /* A/B. Flecha de signal flow + label. */
    lv_obj_t* arr = gf_glyph_arrow(parent, 22, GF_COLOR_PURPLE_BRIGHT, GF_ARROW_RIGHT);
    if (arr != nullptr) {
        lv_obj_set_style_opa(arr, 190, 0);
        lv_obj_align(arr, LV_ALIGN_CENTER, 0, -46);
    }
    gf_hero_label(parent, "SIGNAL FLOW", GF_FONT_MICRO, GF_COLOR_PURPLE_BRIGHT, -22);

    /* C. FX master en grilla 2×2 compacta. */
    const char*      names[4] = {"GLUE", "EQ", "LIMIT", "WIDTH"};
    const uint8_t    fxidx[4] = {6, 3, 5, 11};
    const lv_coord_t cx[2]    = {-46, 46};
    const lv_coord_t cy[2]    = {18, 56};

    for (uint8_t i = 0; i < 4; i++) {
        lv_obj_t* gl = gf_glyph_fx(parent, fxidx[i], 16, GF_COLOR_PURPLE_BRIGHT);
        if (gl != nullptr) lv_obj_align(gl, LV_ALIGN_CENTER,
                                        cx[i % 2] - 18, cy[i / 2]);
        lv_obj_t* nm = gf_label(parent, names[i], GF_FONT_MICRO, GF_COLOR_WHITE);
        lv_obj_align(nm, LV_ALIGN_CENTER, cx[i % 2] + 12, cy[i / 2]);
    }

    gf_mode_pill(parent, "FX", GF_COLOR_PURPLE_BRIGHT);
}

void view_15_destroy(void) {}
