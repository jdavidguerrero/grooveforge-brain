/**
 * @file view_08_fx_select.cpp
 * @brief Vista 08 — FX SELECT (mock GFB-UI-008)
 *
 * Lista vertical de 12 FX, 5 visibles. La fila central es la activa: glifo
 * mas grande, texto blanco, enmarcada por brackets de seleccion. Las demas
 * van atenuadas. Contador de posicion N/12.
 */

#include "views.h"
#include "../../ui_theme.h"
#include "../../widgets/gf_widgets.h"
#include "../../widgets/gf_glyphs.h"

void view_08_create(lv_obj_t* parent) {
    gf_screen_bg(parent);
    gf_tint(parent, GF_COLOR_PURPLE, 30);
    gf_arc_title(parent, "FX SELECT", GF_COLOR_PURPLE_BRIGHT);

    /* 5 filas visibles: indices de FX 2..6, activa la central (FX 4). */
    const char*      names[5]  = {"GHOST ECHO", "SPECTRAL", "TAPE SAT",
                                  "BIT SCULPT", "MODAL REV"};
    const uint8_t    fxidx[5]  = {2, 3, 4, 5, 6};
    const lv_coord_t dy[5]     = {-56, -28, 2, 32, 60};
    const uint8_t    active    = 2;

    for (uint8_t i = 0; i < 5; i++) {
        const bool is_act = (i == active);

        /* C. Brackets de seleccion de la fila activa. */
        if (is_act) {
            lv_obj_t* fr = lv_obj_create(parent);
            lv_obj_set_size(fr, 156, 30);
            lv_obj_set_style_radius(fr, 3, 0);
            lv_obj_set_style_bg_opa(fr, LV_OPA_TRANSP, 0);
            lv_obj_set_style_border_color(fr, GF_COLOR_PURPLE_BRIGHT, 0);
            lv_obj_set_style_border_width(fr, 1, 0);
            lv_obj_clear_flag(fr, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_align(fr, LV_ALIGN_CENTER, 0, dy[i]);
        }

        /* D. Glifo line-art del FX. */
        lv_obj_t* gl = gf_glyph_fx(parent, fxidx[i], is_act ? 22 : 15,
                                   GF_COLOR_PURPLE_BRIGHT);
        if (gl != nullptr) lv_obj_align(gl, LV_ALIGN_CENTER, -52, dy[i]);

        /* A/B. Nombre — activo blanco grande, inactivo gris atenuado. */
        lv_obj_t* nm = gf_label(parent, names[i],
                                is_act ? GF_FONT_LABEL : GF_FONT_MICRO,
                                is_act ? GF_COLOR_WHITE : GF_COLOR_GRAY);
        if (!is_act) lv_obj_set_style_opa(nm, 90, 0);   /* ~0.35 */
        lv_obj_align(nm, LV_ALIGN_CENTER, 12, dy[i]);
    }

    /* E. Contador de posicion. */
    lv_obj_t* pos = gf_label(parent, "5 / 12", GF_FONT_MICRO, GF_COLOR_PURPLE_BRIGHT);
    lv_obj_align(pos, LV_ALIGN_CENTER, 0, 96);

    gf_mode_pill(parent, "FX", GF_COLOR_PURPLE_BRIGHT);
}

void view_08_destroy(void) {}
