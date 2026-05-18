/**
 * @file view_13_insert_layer.cpp
 * @brief Vista 13 — INSERT LAYER (mock GFB-UI-013)
 *
 * Capa de FX en serie: grilla 4×2 de glifos FX. Los activos van brillantes,
 * los inactivos atenuados. Contador de FX activos. Tinte purpura de modo FX.
 */

#include "views.h"
#include "../../ui_theme.h"
#include "../../widgets/gf_widgets.h"
#include "../../widgets/gf_glyphs.h"

void view_13_create(lv_obj_t* parent) {
    gf_screen_bg(parent);
    gf_tint(parent, GF_COLOR_PURPLE, 46);
    gf_arc_title(parent, "INSERT", GF_COLOR_PURPLE_BRIGHT);

    /* Grilla 4×2 de glifos FX. */
    const bool       on[8]  = {true, false, true, true, false, false, false, false};
    const lv_coord_t cx[4]  = {-66, -22, 22, 66};
    const lv_coord_t cy[2]  = {-8, 38};

    for (uint8_t i = 0; i < 8; i++) {
        const bool act = on[i];
        lv_obj_t* gl = gf_glyph_fx(parent, i, act ? 22 : 16, GF_COLOR_PURPLE_BRIGHT);
        if (gl != nullptr) {
            if (!act) lv_obj_set_style_opa(gl, 102, 0);   /* ~0.4 inactivo */
            lv_obj_align(gl, LV_ALIGN_CENTER, cx[i % 4], cy[i / 4]);
        }
    }

    /* D. Contador de FX activos. */
    gf_hero_label(parent, "3 / 8 ACTIVE", GF_FONT_MICRO, GF_COLOR_PURPLE_BRIGHT, 78);

    gf_mode_pill(parent, "FX", GF_COLOR_PURPLE_BRIGHT);
}

void view_13_destroy(void) {}
