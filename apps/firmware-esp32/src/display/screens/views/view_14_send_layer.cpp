/**
 * @file view_14_send_layer.cpp
 * @brief Vista 14 — SEND LAYER (mock GFB-UI-014)
 *
 * Capa de envios FX: filas con glifo + nombre + barra de send-level.
 * Tinte purpura de modo FX.
 */

#include "views.h"
#include "../../ui_theme.h"
#include "../../widgets/gf_widgets.h"
#include "../../widgets/gf_glyphs.h"

void view_14_create(lv_obj_t* parent) {
    gf_screen_bg(parent);
    gf_tint(parent, GF_COLOR_PURPLE, 46);
    gf_arc_title(parent, "SEND", GF_COLOR_PURPLE_BRIGHT);

    const char*      names[4]  = {"MODAL REV", "GHOST ECHO", "SPRING", "CHORUS"};
    const uint8_t    fxidx[4]  = {6, 2, 9, 7};
    const uint8_t    send[4]   = {72, 40, 0, 55};
    const lv_coord_t dy[4]     = {-48, -16, 16, 48};

    for (uint8_t i = 0; i < 4; i++) {
        /* C. Glifo del FX. */
        lv_obj_t* gl = gf_glyph_fx(parent, fxidx[i], 16, GF_COLOR_PURPLE_BRIGHT);
        if (gl != nullptr) lv_obj_align(gl, LV_ALIGN_CENTER, -76, dy[i]);

        /* C. Nombre del FX. */
        lv_obj_t* nm = gf_label(parent, names[i], GF_FONT_MICRO,
                                send[i] > 0 ? GF_COLOR_WHITE : GF_COLOR_GRAY);
        lv_obj_align(nm, LV_ALIGN_CENTER, -28, dy[i]);

        /* A/B. Barra de send-level. */
        lv_obj_t* bar = gf_bar(parent, 66, 4, send[i], GF_COLOR_PURPLE_BRIGHT);
        lv_obj_align(bar, LV_ALIGN_CENTER, 56, dy[i]);
    }

    /* D. Contador. */
    gf_hero_label(parent, "3 SENDS", GF_FONT_MICRO, GF_COLOR_PURPLE_BRIGHT, 78);

    gf_mode_pill(parent, "FX", GF_COLOR_PURPLE_BRIGHT);
}

void view_14_destroy(void) {}
