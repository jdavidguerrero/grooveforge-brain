/**
 * @file view_20_ota_update.cpp
 * @brief Vista 20 — OTA UPDATE (mock GFB-UI-020)
 *
 * Nivel 3 (firmware). Arco de progreso 360°, porcentaje hero, texto de paso,
 * 4 step dots, ETA y aviso ambar "DO NOT POWER OFF".
 */

#include "views.h"
#include "../../ui_theme.h"
#include "../../widgets/gf_widgets.h"

void view_20_create(lv_obj_t* parent) {
    gf_screen_bg(parent);

    /* A. Arco de progreso — circulo completo, 67%. */
    gf_arc(parent, 196, 4, 0, 360, 67, GF_COLOR_TEAL_PLUS, GF_COLOR_TEAL_DIM);

    /* B. Porcentaje hero. */
    gf_hero_label(parent, "67%", GF_FONT_HERO, GF_COLOR_WHITE, -16);

    /* C. Texto del paso actual. */
    gf_hero_label(parent, "DOWNLOADING", GF_FONT_MICRO, GF_COLOR_TEAL_PLUS, 16);

    /* D. 4 step dots — paso 2 activo. */
    for (uint8_t i = 0; i < 4; i++) {
        const bool done = (i <= 1);
        lv_obj_t* dot = gf_dot(parent, done ? 6 : 4,
                               done ? GF_COLOR_TEAL_PLUS : GF_COLOR_TEAL_DIM);
        lv_obj_align(dot, LV_ALIGN_CENTER, -18 + i * 12, 36);
    }

    /* E. ETA. */
    lv_obj_t* eta = gf_label(parent, "ETA 01:24", GF_FONT_MICRO, GF_COLOR_GRAY);
    lv_obj_set_style_opa(eta, 150, 0);
    lv_obj_align(eta, LV_ALIGN_CENTER, 0, 54);

    /* F. Aviso ambar — no apagar durante el flasheo. */
    gf_hero_label(parent, "DO NOT POWER OFF", GF_FONT_MICRO, GF_COLOR_AMBER, 78);
}

void view_20_destroy(void) {}
