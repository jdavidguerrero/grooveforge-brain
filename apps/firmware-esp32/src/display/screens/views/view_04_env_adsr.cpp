/**
 * @file view_04_env_adsr.cpp
 * @brief Vista 04 — ENV ADSR (Sprint 32 — slider por param + curva animada)
 *
 * 4 páginas en gf_pslider, una por parámetro ADSR. Cada página muestra:
 *   - Título del param en GF_FONT_LABEL + GF_COLOR_TEAL_PLUS
 *   - Canvas 165×66 con la curva ADSR completa:
 *       · Segmento activo de esa página: GF_COLOR_TEAL_PLUS, width=3
 *       · Otros segmentos: GF_COLOR_TEAL, width=2, opa=100
 *       · Dot animado blanco (9×9) que recorre el segmento, borde GF_COLOR_TEAL_PLUS
 *       · Vértices PA/PD/PS: puntos de 5×5 en GF_COLOR_TEAL_PLUS, opa=180
 *   - Label de valor en GF_FONT_HERO + GF_COLOR_WHITE en bottom:
 *       · ATTACK/DECAY/RELEASE: "250 MS" o "2.5 S" (si >=1000ms)
 *       · SUSTAIN: "75%"
 *
 * Animación del cursor: s_cursor_t[4] float por página, avanza cada tick (33ms).
 * El período de avance es proporcional al valor normalizado del param.
 *
 * Parámetros del bridge (group=1 para ENV):
 *   param 0: attack  norm [0,1] → ms = 0   + norm*5000
 *   param 1: decay   norm [0,1] → ms = 10  + norm*4990
 *   param 2: sustain norm [0,1] → %
 *   param 3: release norm [0,1] → ms = 10  + norm*4990
 */

#include "views.h"
#include "../../ui_theme.h"
#include "../../widgets/gf_widgets.h"
#include "../../widgets/gf_glyphs.h"
#include "../../widgets/gf_param_slider.h"
#include "../../../bridge/bridge_handlers.h"
#include <stdio.h>
#include <math.h>

/* ── Geometría del canvas ───────────────────────────────────────────────────── */

#define ENV_CANVAS_W  165
#define ENV_CANVAS_H   66

/* Vértices de la curva ADSR en el canvas 165×66.
 * Coordenadas diseñadas para que la forma sea legible en el round display. */
static const lv_point_t P0 = {  0, 64 };   ///< silencio inicial
static const lv_point_t PA = { 38,  2 };   ///< peak de ataque
static const lv_point_t PD = { 78, 34 };   ///< fin decay (nivel sustain)
static const lv_point_t PS = {124, 34 };   ///< fin sustain
static const lv_point_t PR = {164, 64 };   ///< silencio final

/* Extremos de cada segmento indexado por número de página (0=A, 1=D, 2=S, 3=R). */
static const lv_point_t SEGS_FROM[4] = { P0, PA, PD, PS };
static const lv_point_t SEGS_TO[4]   = { PA, PD, PS, PR };

/* ── Tipos de página ─────────────────────────────────────────────────────────── */

typedef struct {
    lv_obj_t* canvas;    ///< canvas de la curva ADSR
    lv_obj_t* val_lbl;   ///< label de valor formateado (bottom)
} env_page_t;

/* ── Estado global de la vista ───────────────────────────────────────────────── */

static lv_obj_t*   s_slider      = nullptr;
static lv_timer_t* s_timer       = nullptr;
static env_page_t  s_pages[4]    = {};
static uint8_t     s_last_param  = 0xFF;
static uint8_t     s_engine_idx  = 0;
static float       s_cursor_t[4] = {};

/* ── Animación del cursor ───────────────────────────────────────────────────── */

/**
 * @brief Devuelve el delta de cursor_t por tick (33ms) para un segmento y valor norm dados.
 *
 * El período de cada segmento se mapea desde el valor normalizado del param:
 *   ATTACK:  100..2000ms
 *   DECAY:   200..2000ms
 *   SUSTAIN: fijo 1200ms (el nivel no varía con tiempo)
 *   RELEASE: 200..2000ms
 */
static float cursor_delta(uint8_t seg, float norm) {
    float period_ms;
    switch (seg) {
        case 0:  period_ms = 100.0f + norm * 1900.0f; break;
        case 1:  period_ms = 200.0f + norm * 1800.0f; break;
        case 2:  period_ms = 1200.0f;                 break;
        default: period_ms = 200.0f + norm * 1800.0f; break;
    }
    return 33.0f / period_ms;
}

/* ── Redibujado del canvas ───────────────────────────────────────────────────── */

/**
 * @brief Redibuja el canvas de la curva ADSR para una página/segmento dado.
 *
 * @param canvas   Canvas LVGL a redibujar.
 * @param page_seg Índice del segmento activo (0=A, 1=D, 2=S, 3=R).
 * @param cur_t    Posición del cursor animado [0.0, 1.0] en el segmento activo.
 */
static void draw_env_canvas(lv_obj_t* canvas, uint8_t page_seg, float cur_t) {
    if (!canvas) return;
    lv_canvas_fill_bg(canvas, lv_color_black(), LV_OPA_TRANSP);

    lv_draw_line_dsc_t dim, bright;
    lv_draw_line_dsc_init(&dim);
    dim.color = GF_COLOR_TEAL; dim.width = 2; dim.opa = 100;
    dim.round_start = dim.round_end = 1;

    lv_draw_line_dsc_init(&bright);
    bright.color = GF_COLOR_TEAL_PLUS; bright.width = 3; bright.opa = LV_OPA_COVER;
    bright.round_start = bright.round_end = 1;

    for (uint8_t s = 0; s < 4; s++) {
        lv_point_t pair[2] = { SEGS_FROM[s], SEGS_TO[s] };
        lv_canvas_draw_line(canvas, pair, 2, (s == page_seg) ? &bright : &dim);
    }

    /* Vértices de transición: PA, PD, PS — dots de 5×5 en teal+ */
    lv_draw_rect_dsc_t vd;
    lv_draw_rect_dsc_init(&vd);
    vd.bg_color = GF_COLOR_TEAL_PLUS; vd.bg_opa = 180; vd.radius = LV_RADIUS_CIRCLE;
    const lv_point_t verts[3] = { PA, PD, PS };
    for (uint8_t i = 0; i < 3; i++) {
        lv_canvas_draw_rect(canvas, verts[i].x - 2, verts[i].y - 2, 5, 5, &vd);
    }

    /* Cursor animado: dot blanco 9×9 con borde teal+ en el segmento activo */
    float t = cur_t;
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    const lv_point_t& pf = SEGS_FROM[page_seg];
    const lv_point_t& pt = SEGS_TO[page_seg];
    lv_coord_t cx = (lv_coord_t)lroundf(pf.x + (pt.x - pf.x) * t);
    lv_coord_t cy = (lv_coord_t)lroundf(pf.y + (pt.y - pf.y) * t);

    lv_draw_rect_dsc_t cd;
    lv_draw_rect_dsc_init(&cd);
    cd.bg_color    = GF_COLOR_WHITE;      cd.bg_opa    = LV_OPA_COVER;
    cd.radius      = LV_RADIUS_CIRCLE;
    cd.border_color = GF_COLOR_TEAL_PLUS; cd.border_width = 2; cd.border_opa = LV_OPA_COVER;
    lv_canvas_draw_rect(canvas, cx - 4, cy - 4, 9, 9, &cd);
}

/* ── Títulos y rangos de param ───────────────────────────────────────────────── */

static const char* const ENV_TITLES[4] = { "ATTACK", "DECAY", "SUSTAIN", "RELEASE" };
static const float       ENV_MIN[4]    = {    0.0f,   10.0f,    0.0f,     10.0f };
static const float       ENV_MAX[4]    = { 5000.0f, 5000.0f,    1.0f,   5000.0f };

/* ── Page builder ────────────────────────────────────────────────────────────── */

static void build_env_page(lv_obj_t* page, uint8_t idx) {
    lv_obj_t* t = gf_label(page, ENV_TITLES[idx], GF_FONT_LABEL, GF_COLOR_TEAL_PLUS);
    if (t) lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 12);

    lv_obj_t* cv = gf_canvas_create(page, ENV_CANVAS_W, ENV_CANVAS_H);
    if (cv) lv_obj_align(cv, LV_ALIGN_CENTER, 0, -8);
    s_pages[idx].canvas = cv;

    const char* init_str = (idx == 2) ? "0%" : "0 MS";
    lv_obj_t* vl = gf_label(page, init_str, GF_FONT_HERO, GF_COLOR_WHITE);
    if (vl) lv_obj_align(vl, LV_ALIGN_BOTTOM_MID, 0, -30);
    s_pages[idx].val_lbl = vl;

    draw_env_canvas(cv, idx, 0.0f);
}

/* ── Timer principal — 33ms ────────────────────────────────────────────────── */

static void env_tick(lv_timer_t* /*t*/) {
    /* Sincronizar slider con cursor del Teensy */
    uint8_t cur = bridge_get_synth_param();
    if (cur >= 4) cur = 0;
    if (cur != s_last_param) {
        s_last_param = cur;
        gf_pslider_set_active(s_slider, cur, true);
    }

    for (uint8_t i = 0; i < 4; i++) {
        float norm = bridge_get_synth_param_cached(s_engine_idx, 1 /* ENV */, i);
        env_page_t& p = s_pages[i];

        /* Avanzar cursor animado */
        s_cursor_t[i] += cursor_delta(i, norm);
        if (s_cursor_t[i] > 1.0f) s_cursor_t[i] = 0.0f;

        draw_env_canvas(p.canvas, i, s_cursor_t[i]);

        /* Actualizar label de valor */
        if (p.val_lbl) {
            static char buf[16];
            if (i == 2) {
                /* SUSTAIN — porcentaje */
                int pct = (int)lroundf(norm * 100.0f);
                if (pct < 0) pct = 0;
                if (pct > 100) pct = 100;
                snprintf(buf, sizeof(buf), "%d%%", pct);
            } else {
                /* ATTACK / DECAY / RELEASE — milisegundos o segundos */
                float ms = ENV_MIN[i] + norm * (ENV_MAX[i] - ENV_MIN[i]);
                if (ms < 1000.0f) {
                    snprintf(buf, sizeof(buf), "%d MS", (int)lroundf(ms));
                } else {
                    snprintf(buf, sizeof(buf), "%.1f S", (double)(ms / 1000.0f));
                }
            }
            lv_label_set_text(p.val_lbl, buf);
        }
    }
}

/* ── Create / Destroy ─────────────────────────────────────────────────────────── */

void view_04_create(lv_obj_t* parent) {
    s_slider     = nullptr;
    s_timer      = nullptr;
    s_last_param = 0xFF;
    for (uint8_t i = 0; i < 4; i++) {
        s_pages[i]    = env_page_t{};
        s_cursor_t[i] = 0.0f;
    }

    uint8_t eng_id = bridge_get_engine_id();
    s_engine_idx = (eng_id >= 0x10) ? (uint8_t)(eng_id - 0x10) : 0;

    gf_screen_bg(parent);
    s_slider = gf_pslider_create(parent, 4);

    for (uint8_t i = 0; i < 4; i++) {
        lv_obj_t* page = gf_pslider_get_page(s_slider, i);
        if (page) build_env_page(page, i);
    }

    uint8_t init_param = bridge_get_synth_param();
    if (init_param >= 4) init_param = 0;
    gf_pslider_set_active(s_slider, init_param, false);
    s_last_param = init_param;

    gf_page_dots(parent, 4, init_param, GF_COLOR_TEAL, 8);
    gf_mode_pill(parent, "ENV", GF_COLOR_TEAL);

    s_timer = lv_timer_create(env_tick, 33, nullptr);
    env_tick(nullptr);
}

void view_04_destroy(void) {
    if (s_timer) { lv_timer_del(s_timer); s_timer = nullptr; }
    s_slider     = nullptr;
    s_last_param = 0xFF;
    for (uint8_t i = 0; i < 4; i++) s_pages[i] = env_page_t{};
}
