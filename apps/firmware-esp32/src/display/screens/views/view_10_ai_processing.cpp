/**
 * @file view_10_ai_processing.cpp
 * @brief Vista 10 — AI PROCESSING (mock GFB-UI-010)
 *
 * Core que "respira" (animacion de escala) rodeado de 8 orbit dots con un
 * chase secuencial (opacidad escalonada por delay). Animaciones lv_anim
 * infinitas — gf_anim_kill_all() las elimina al cambiar de vista.
 */

#include "views.h"
#include "../../ui_theme.h"
#include "../../widgets/gf_widgets.h"
#include <math.h>

/* exec_cb: opacidad de un objeto. */
static void opa_cb(void* obj, int32_t v) {
    lv_obj_set_style_opa(static_cast<lv_obj_t*>(obj), static_cast<lv_opa_t>(v), 0);
}

/* exec_cb: tamano del core (respiracion) — recentra al cambiar de tamano. */
static void core_size_cb(void* obj, int32_t v) {
    lv_obj_t* d = static_cast<lv_obj_t*>(obj);
    lv_obj_set_size(d, v, v);
    lv_obj_set_style_radius(d, v / 2, 0);
    lv_obj_center(d);
}

void view_10_create(lv_obj_t* parent) {
    gf_screen_bg(parent);
    gf_arc_title(parent, "GROOVEFORGE AI", GF_COLOR_TEAL);

    /* Glow detras del core. */
    lv_obj_t* glow = gf_dot(parent, 32, GF_COLOR_TEAL_PLUS);
    lv_obj_center(glow);
    lv_obj_set_style_opa(glow, 45, 0);

    /* B. 8 orbit dots con chase escalonado. */
    for (int i = 0; i < 8; i++) {
        const float a = i * 45.0f * (float)M_PI / 180.0f;
        lv_obj_t* d = gf_dot(parent, 7, GF_COLOR_TEAL_PLUS);
        lv_obj_align(d, LV_ALIGN_CENTER,
                     (lv_coord_t)lroundf(50.0f * cosf(a)),
                     (lv_coord_t)lroundf(50.0f * sinf(a)));

        lv_anim_t an;
        lv_anim_init(&an);
        lv_anim_set_var(&an, d);
        lv_anim_set_exec_cb(&an, opa_cb);
        lv_anim_set_values(&an, 50, 255);
        lv_anim_set_time(&an, 400);
        lv_anim_set_playback_time(&an, 400);
        lv_anim_set_delay(&an, i * 100);                 /* chase secuencial */
        lv_anim_set_repeat_count(&an, LV_ANIM_REPEAT_INFINITE);
        lv_anim_start(&an);
    }

    /* A. Core que respira. */
    lv_obj_t* core = gf_dot(parent, 14, GF_COLOR_TEAL_PLUS);
    lv_obj_center(core);
    lv_anim_t ca;
    lv_anim_init(&ca);
    lv_anim_set_var(&ca, core);
    lv_anim_set_exec_cb(&ca, core_size_cb);
    lv_anim_set_values(&ca, 10, 18);
    lv_anim_set_time(&ca, 700);
    lv_anim_set_playback_time(&ca, 700);
    lv_anim_set_repeat_count(&ca, LV_ANIM_REPEAT_INFINITE);
    lv_anim_start(&ca);

    /* C/D. Textos. */
    gf_hero_label(parent, "AI THINKING", GF_FONT_LABEL, GF_COLOR_TEAL_PLUS, 46);
    gf_hero_label(parent, "ANALYZING GROOVE", GF_FONT_MICRO, GF_COLOR_GRAY, 64);
}

void view_10_destroy(void) {}
