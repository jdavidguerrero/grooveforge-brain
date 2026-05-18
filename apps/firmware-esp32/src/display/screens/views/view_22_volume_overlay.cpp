/**
 * @file view_22_volume_overlay.cpp
 * @brief Vista 22 — VOLUME OVERLAY (mock GFB-UI-022)
 *
 * Overlay de volumen: arco de 270°, dB hero, salida %, y medidores de pico
 * L/R animados (~30fps via lv_timer registrado en gf_anim).
 */

#include "views.h"
#include "../../ui_theme.h"
#include "../../widgets/gf_widgets.h"
#include "../../widgets/gf_anim.h"
#include <math.h>

static lv_obj_t* s_peak_l = nullptr;
static lv_obj_t* s_peak_r = nullptr;
static uint32_t  s_vphase = 0;

/* Animacion de los medidores de pico — simula audio (no hay senal real). */
static void peak_tick(lv_timer_t* /*t*/) {
    if (s_peak_l == nullptr || s_peak_r == nullptr) return;
    s_vphase += 11;
    const float t = s_vphase * 0.06f;
    float pl = 0.52f + 0.40f * sinf(t)        + 0.06f * sinf(t * 3.7f);
    float pr = 0.52f + 0.40f * sinf(t + 1.1f) + 0.06f * sinf(t * 3.1f);
    if (pl < 0) pl = 0; if (pl > 1) pl = 1;
    if (pr < 0) pr = 0; if (pr > 1) pr = 1;
    lv_bar_set_value(s_peak_l, (int32_t)(pl * 100), LV_ANIM_OFF);
    lv_bar_set_value(s_peak_r, (int32_t)(pr * 100), LV_ANIM_OFF);
}

void view_22_create(lv_obj_t* parent) {
    gf_screen_bg(parent);
    s_vphase = 0;

    /* B. Arco de volumen — span de 270° (hueco abajo). */
    gf_arc(parent, 200, 5, 135, 45, 70, GF_COLOR_TEAL_PLUS, GF_COLOR_TEAL_DIM);

    /* C. dB hero. */
    gf_hero_label(parent, "-12", GF_FONT_HERO, GF_COLOR_WHITE, -14);
    gf_hero_label(parent, "dB",  GF_FONT_MICRO, GF_COLOR_GRAY, 14);

    /* D. Salida %. */
    gf_hero_label(parent, "OUT 68%", GF_FONT_MICRO, GF_COLOR_GRAY, 30);

    /* E. Medidores de pico L / R. */
    lv_obj_t* ll = gf_label(parent, "L", GF_FONT_MICRO, GF_COLOR_GRAY);
    lv_obj_align(ll, LV_ALIGN_CENTER, -42, 52);
    s_peak_l = gf_bar(parent, 60, 4, 50, GF_COLOR_TEAL_PLUS);
    lv_obj_align(s_peak_l, LV_ALIGN_CENTER, 8, 52);

    lv_obj_t* rl = gf_label(parent, "R", GF_FONT_MICRO, GF_COLOR_GRAY);
    lv_obj_align(rl, LV_ALIGN_CENTER, -42, 64);
    s_peak_r = gf_bar(parent, 60, 4, 50, GF_COLOR_TEAL_PLUS);
    lv_obj_align(s_peak_r, LV_ALIGN_CENTER, 8, 64);

    /* Timer de animacion ~30fps. */
    lv_timer_t* t = lv_timer_create(peak_tick, 33, nullptr);
    gf_anim_register_timer(t);
}

void view_22_destroy(void) {
    s_peak_l = nullptr;
    s_peak_r = nullptr;
}
