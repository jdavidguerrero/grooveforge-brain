/**
 * @file view_05_lfo_mod.cpp
 * @brief Vista 05 — LFO · MOD (mock GFB-UI-005)
 *
 * Onda LFO animada: un lv_timer (~30fps) redibuja la senoidal sobre un canvas
 * con un desfase creciente (scroll) y reposiciona el playhead que sigue la
 * onda. El timer se registra con gf_anim — el carrusel lo destruye en el
 * cambio de vista (gf_anim_kill_all).
 */

#include "views.h"
#include "../../ui_theme.h"
#include "../../widgets/gf_widgets.h"
#include "../../widgets/gf_glyphs.h"
#include "../../widgets/gf_anim.h"
#include <math.h>

static constexpr lv_coord_t LFO_W  = 184;
static constexpr lv_coord_t LFO_H  = 60;
static constexpr lv_coord_t LFO_DY = -8;     /* offset y del canvas respecto al centro */

static lv_obj_t* s_wave_cv  = nullptr;
static lv_obj_t* s_playhead = nullptr;
static uint32_t  s_phase    = 0;

/* Evalua la onda LFO en t∈[0,1] dado el desfase actual → valor [-1,1]. */
static float lfo_value(float t) {
    const float ph = s_phase * (float)M_PI / 180.0f;
    return sinf(t * 2.0f * 2.0f * (float)M_PI + ph);   /* 2 ciclos visibles */
}

/* Redibuja la onda + reposiciona el playhead. Llamado por el lv_timer. */
static void lfo_tick(lv_timer_t* /*t*/) {
    if (s_wave_cv == nullptr) return;
    s_phase = (s_phase + 8) % 360;

    lv_canvas_fill_bg(s_wave_cv, lv_color_black(), LV_OPA_TRANSP);

    const float mid = (LFO_H - 1) / 2.0f;
    const float amp = (LFO_H - 1) / 2.0f - 3.0f;

    lv_point_t pts[48];
    for (int i = 0; i < 48; i++) {
        const float t = (float)i / 47.0f;
        pts[i].x = (lv_coord_t)lroundf(t * (LFO_W - 1));
        pts[i].y = (lv_coord_t)lroundf(mid - lfo_value(t) * amp);
    }
    lv_draw_line_dsc_t d;
    lv_draw_line_dsc_init(&d);
    d.color = GF_COLOR_TEAL_PLUS; d.width = 2; d.opa = LV_OPA_COVER;
    d.round_start = d.round_end = 1;
    lv_canvas_draw_line(s_wave_cv, pts, 48, &d);

    /* Playhead fijo en x=centro, su y sigue la onda. */
    if (s_playhead != nullptr) {
        const float py = mid - lfo_value(0.5f) * amp;
        lv_obj_align(s_playhead, LV_ALIGN_CENTER, 0,
                     LFO_DY - LFO_H / 2 + (lv_coord_t)lroundf(py));
    }
}

void view_05_create(lv_obj_t* parent) {
    gf_screen_bg(parent);
    gf_arc_title(parent, "LFO MOD", GF_COLOR_TEAL);
    s_phase = 0;

    /* E. Eje central (linea fina atenuada) — detras del canvas. */
    lv_obj_t* axis = lv_obj_create(parent);
    lv_obj_set_size(axis, LFO_W, 1);
    lv_obj_set_style_bg_color(axis, GF_COLOR_GRAY, 0);
    lv_obj_set_style_bg_opa(axis, 55, 0);
    lv_obj_set_style_border_width(axis, 0, 0);
    lv_obj_clear_flag(axis, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(axis, LV_ALIGN_CENTER, 0, LFO_DY);

    /* A. Canvas de la onda LFO. */
    s_wave_cv = gf_canvas_create(parent, LFO_W, LFO_H);
    if (s_wave_cv != nullptr) lv_obj_align(s_wave_cv, LV_ALIGN_CENTER, 0, LFO_DY);

    /* B. Playhead — dot blanco. */
    s_playhead = gf_dot(parent, 5, GF_COLOR_WHITE);

    /* C. RATE / DEPTH. */
    gf_hero_label(parent, "RATE  2.4 HZ", GF_FONT_LABEL, GF_COLOR_WHITE, 40);
    gf_hero_label(parent, "DEPTH  60%",   GF_FONT_LABEL, GF_COLOR_WHITE, 58);

    /* D. TARGET / SHAPE. */
    gf_hero_label(parent, "TARGET CUTOFF", GF_FONT_MICRO, GF_COLOR_TEAL_PLUS, -52);

    /* Timer de animacion ~30fps, registrado para limpieza automatica. */
    lv_timer_t* t = lv_timer_create(lfo_tick, 33, nullptr);
    gf_anim_register_timer(t);
    lfo_tick(nullptr);   /* primer frame inmediato */

    gf_page_dots(parent, 5, 3, GF_COLOR_TEAL);
    gf_mode_pill(parent, "SYNTH", GF_COLOR_TEAL);
}

void view_05_destroy(void) {
    /* El timer lo destruye gf_anim_kill_all(); aqui solo se sueltan punteros. */
    s_wave_cv  = nullptr;
    s_playhead = nullptr;
}
