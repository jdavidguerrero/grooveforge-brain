/**
 * @file view_06_engine_select.cpp
 * @brief Vista 06 — ENGINE SELECT (mock GFB-UI-006)
 *
 * Lista de engines: fila activa con fondo teal, code box por engine, filas
 * "coming soon" atenuadas con tag v1.1, scrollbar y footer hint.
 */

#include "views.h"
#include "../../ui_theme.h"
#include "../../widgets/gf_widgets.h"

void view_06_create(lv_obj_t* parent) {
    gf_screen_bg(parent);
    gf_arc_title(parent, "ENGINE", GF_COLOR_TEAL);

    struct EngineRow { const char* name; const char* code; bool soon; };
    const EngineRow eng[5] = {
        {"MOOG MODEL D", "M", false},
        {"JUNO-106",     "J", false},
        {"PROPHET-5",    "P", false},
        {"DX UNIT",      "D", true},
        {"OB SYNTH",     "O", true},
    };
    const lv_coord_t dy[5] = {-56, -28, 0, 28, 56};
    const uint8_t    active = 0;

    for (uint8_t i = 0; i < 5; i++) {
        const bool is_act = (i == active);

        /* A. Fondo de la fila activa (teal ~22%). */
        if (is_act) {
            lv_obj_t* hl = lv_obj_create(parent);
            lv_obj_set_size(hl, 188, 26);
            lv_obj_set_style_radius(hl, 5, 0);
            lv_obj_set_style_bg_color(hl, GF_COLOR_TEAL, 0);
            lv_obj_set_style_bg_opa(hl, 56, 0);
            lv_obj_set_style_border_width(hl, 0, 0);
            lv_obj_clear_flag(hl, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_align(hl, LV_ALIGN_CENTER, 0, dy[i]);
        }

        /* B. Code box — cuadrado teal con la inicial del engine. */
        lv_obj_t* box = lv_obj_create(parent);
        lv_obj_set_size(box, 16, 16);
        lv_obj_set_style_radius(box, 2, 0);
        lv_obj_set_style_bg_color(box, eng[i].soon ? GF_COLOR_TEAL_DIM : GF_COLOR_TEAL, 0);
        lv_obj_set_style_bg_opa(box, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(box, 0, 0);
        lv_obj_clear_flag(box, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_align(box, LV_ALIGN_CENTER, -76, dy[i]);
        lv_obj_t* code = gf_label(box, eng[i].code, GF_FONT_MICRO, GF_COLOR_BG);
        lv_obj_center(code);

        /* C. Nombre del engine. */
        lv_color_t name_c = is_act ? GF_COLOR_WHITE : GF_COLOR_GRAY;
        lv_obj_t* nm = gf_label(parent, eng[i].name, GF_FONT_LABEL, name_c);
        lv_obj_align(nm, LV_ALIGN_CENTER, -52, dy[i]);

        /* D. Tag v1.1 + atenuado para engines "coming soon". */
        if (eng[i].soon) {
            lv_obj_set_style_opa(nm, 82, 0);   /* ~0.32 */
            lv_obj_t* tag = gf_label(parent, "v1.1", GF_FONT_MICRO, GF_COLOR_GRAY);
            lv_obj_set_style_opa(tag, 82, 0);
            lv_obj_align(tag, LV_ALIGN_CENTER, 76, dy[i]);
        }
    }

    /* E. Scrollbar — barra fina a la derecha reflejando la posicion. */
    lv_obj_t* track = lv_obj_create(parent);
    lv_obj_set_size(track, 2, 120);
    lv_obj_set_style_bg_color(track, GF_COLOR_TEAL_DIM, 0);
    lv_obj_set_style_bg_opa(track, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(track, 0, 0);
    lv_obj_set_style_radius(track, 1, 0);
    lv_obj_clear_flag(track, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(track, LV_ALIGN_RIGHT_MID, -14, 0);
    lv_obj_t* thumb = lv_obj_create(parent);
    lv_obj_set_size(thumb, 2, 44);
    lv_obj_set_style_bg_color(thumb, GF_COLOR_TEAL, 0);
    lv_obj_set_style_bg_opa(thumb, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(thumb, 0, 0);
    lv_obj_set_style_radius(thumb, 1, 0);
    lv_obj_clear_flag(thumb, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(thumb, LV_ALIGN_RIGHT_MID, -14, -38);

    /* F. Footer hint. */
    lv_obj_t* hint = gf_label(parent, "NAV TO SELECT", GF_FONT_MICRO, GF_COLOR_GRAY);
    lv_obj_set_style_opa(hint, 150, 0);
    lv_obj_align(hint, LV_ALIGN_CENTER, 0, 88);

    gf_mode_pill(parent, "SYNTH", GF_COLOR_TEAL);
}

void view_06_destroy(void) {}
