/**
 * @file view_03_osc.cpp
 * @brief Vista 03 — OSC PAGE (mock GFB-UI-003)
 *
 * Tres filas de oscilador (glifo de onda + barra de nivel + detune). La fila
 * activa se resalta con fondo teal y un indicador ▶. Edge params L/R visibles.
 */

#include "views.h"
#include "../../ui_theme.h"
#include "../../widgets/gf_widgets.h"
#include "../../widgets/gf_glyphs.h"

void view_03_create(lv_obj_t* parent) {
    gf_screen_bg(parent);
    gf_arc_title(parent, "OSC", GF_COLOR_TEAL);

    const uint8_t   active     = 1;                 /* OSC 2 en edicion */
    const gf_wave_t waves[3]   = { GF_WAVE_SINE, GF_WAVE_SAW, GF_WAVE_SQUARE };
    const uint8_t   levels[3]  = { 55, 80, 30 };
    const char*     detune[3]  = { "+0", "-7", "+12" };
    const lv_coord_t row_dy[3] = { -36, 4, 44 };

    for (uint8_t i = 0; i < 3; i++) {
        const bool      is_act = (i == active);
        const lv_color_t col   = is_act ? GF_COLOR_TEAL_PLUS : GF_COLOR_TEAL;

        /* Fondo de resaltado de la fila activa (se crea primero → queda detras). */
        if (is_act) {
            lv_obj_t* hl = lv_obj_create(parent);
            lv_obj_set_size(hl, 190, 34);
            lv_obj_set_style_radius(hl, 8, 0);
            lv_obj_set_style_bg_color(hl, GF_COLOR_TEAL, 0);
            lv_obj_set_style_bg_opa(hl, 38, 0);            /* ~8% segun mock */
            lv_obj_set_style_border_width(hl, 0, 0);
            lv_obj_clear_flag(hl, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_align(hl, LV_ALIGN_CENTER, 0, row_dy[i]);

            /* Indicador de edicion ▶ */
            lv_obj_t* arr = gf_glyph_arrow(parent, 9, GF_COLOR_TEAL_PLUS, GF_ARROW_RIGHT);
            if (arr != nullptr) lv_obj_align(arr, LV_ALIGN_CENTER, -90, row_dy[i]);
        }

        /* Glifo de forma de onda. */
        lv_obj_t* wv = gf_glyph_wave(parent, waves[i], 30, 16, col);
        if (wv != nullptr) lv_obj_align(wv, LV_ALIGN_CENTER, -62, row_dy[i]);

        /* Barra de nivel. */
        lv_obj_t* bar = gf_bar(parent, 70, 6, levels[i], col);
        lv_obj_align(bar, LV_ALIGN_CENTER, 14, row_dy[i]);

        /* Detune en cents. */
        lv_obj_t* dt = gf_label(parent, detune[i], GF_FONT_MICRO, GF_COLOR_WHITE);
        lv_obj_align(dt, LV_ALIGN_CENTER, 78, row_dy[i]);
    }

    /* Edge params L/R — ENC L = cutoff, ENC R = resonance (01-architecture §3.4). */
    lv_obj_t* el = gf_label(parent, "CUT", GF_FONT_MICRO, GF_COLOR_GRAY);
    lv_obj_align(el, LV_ALIGN_LEFT_MID, 4, 0);
    lv_obj_t* er = gf_label(parent, "RES", GF_FONT_MICRO, GF_COLOR_GRAY);
    lv_obj_align(er, LV_ALIGN_RIGHT_MID, -4, 0);

    gf_page_dots(parent, 5, 1, GF_COLOR_TEAL);
    gf_mode_pill(parent, "SYNTH", GF_COLOR_TEAL);
}

void view_03_destroy(void) {}
