/**
 * @file view_05_lfo_mod.cpp
 * @brief Vista 05 — LFO / MOD (Sprint 32 — engine-aware, 2 páginas por slider)
 *
 * Diseño engine-aware: el contenido de las 2 páginas varía según el engine activo.
 *
 *   Moog (0x10) — group=3, 2 params:
 *     Página 0 "LFO RATE":  sinusoide animada cuya velocidad = rate norm→Hz.
 *     Página 1 "LFO DEPTH": arc 0-100%.
 *
 *   Juno (0x11) — group=3, 2 params:
 *     Página 0 "CHORUS":      big text "ON"/"OFF" + dot de 40px coloreado.
 *     Página 1 "CHORUS MODE": big text "MODE I" / "MODE II".
 *
 *   Prophet (0x12) — group=3, 2 params:
 *     Página 0 "OSC MIX":   arc 0-100% con labels "A" y "B" a los lados,
 *                             texto "Axx%/Bxx%" en GF_FONT_LABEL debajo.
 *     Página 1 "LFO DEPTH": arc 0-100%.
 *
 * Animación de la sinusoide Moog: s_rate_phase avanza en grados/tick
 * proporcional a hz = 0.1 + rate_norm * 9.9.  Fórmula:
 *   phase_inc = hz * 360 * 0.033  [grados por tick a 33ms]
 *
 * Bridge params: bridge_get_synth_param_cached(engine_idx, 3 /*LFO*\/, param_idx)
 * Valores devueltos: normalizados [0.0, 1.0].
 */

#include "views.h"
#include "../../ui_theme.h"
#include "../../widgets/gf_widgets.h"
#include "../../widgets/gf_glyphs.h"
#include "../../widgets/gf_param_slider.h"
#include "../../../bridge/bridge_handlers.h"
#include <stdio.h>
#include <math.h>

/* ── Tipos de visualización ───────────────────────────────────────────────── */

typedef enum {
    LFOVIZ_RATE,    ///< sinusoide animada — velocidad proporcional al rate
    LFOVIZ_DEPTH,   ///< arc 0-100%
    LFOVIZ_ONOFF,   ///< big text "ON" / "OFF" con dot de color
    LFOVIZ_MODE,    ///< big text "MODE I" / "MODE II"
    LFOVIZ_MIX,     ///< arc 0-100% con labels A/B y texto "Axx%/Bxx%"
} lfo_viz_t;

/* ── Descriptor por engine ────────────────────────────────────────────────── */

typedef struct {
    const char* title;
    lfo_viz_t   viz;
} lfo_param_t;

typedef struct {
    lfo_param_t params[2];
    uint8_t     num;
} lfo_engine_t;

static const lfo_engine_t LFO_ENGINES[3] = {
    /* 0 — Moog Model D */
    { {{ "LFO RATE",    LFOVIZ_RATE  }, { "LFO DEPTH",   LFOVIZ_DEPTH }}, 2 },
    /* 1 — Juno-106    */
    { {{ "CHORUS",      LFOVIZ_ONOFF }, { "CHORUS MODE", LFOVIZ_MODE  }}, 2 },
    /* 2 — Prophet-5   */
    { {{ "OSC MIX",     LFOVIZ_MIX   }, { "LFO DEPTH",   LFOVIZ_DEPTH }}, 2 },
};

/* ── Estado por página ────────────────────────────────────────────────────── */

typedef struct {
    lfo_viz_t  viz;
    lv_obj_t*  canvas;    ///< LFOVIZ_RATE — canvas de la sinusoide
    lv_obj_t*  arc;       ///< LFOVIZ_DEPTH / LFOVIZ_MIX — lv_arc
    lv_obj_t*  val_lbl;   ///< valor principal (Hz, %, ON/OFF, MODE I/II, "Axx%")
    lv_obj_t*  aux_lbl;   ///< auxiliar: "Axx%/Bxx%" para MIX; dot para ONOFF
    lv_obj_t*  dot_obj;   ///< dot de 40px para ONOFF (color cambia según estado)
} lfo_page_t;

/* ── Estado global ────────────────────────────────────────────────────────── */

static lv_obj_t*   s_slider     = nullptr;
static lv_timer_t* s_timer      = nullptr;
static lfo_page_t  s_pages[2]   = {};
static uint8_t     s_last_param = 0xFF;
static uint8_t     s_engine_idx = 0;
static float       s_rate_phase = 0.0f;   ///< fase acumulada de la sinusoide [0..360)

/* ── Dimensiones del canvas de sinusoide ──────────────────────────────────── */

static const lv_coord_t LFO_W = 180;
static const lv_coord_t LFO_H = 60;

/* ── Dibujo de la sinusoide animada ──────────────────────────────────────── */

/**
 * @brief Redibuja el canvas de la onda LFO con la fase actual.
 *
 * La fase en grados se convierte a radianes para sinf().
 * Se dibujan 2 ciclos visibles con scroll a la velocidad del rate.
 */
static void draw_rate_canvas(lv_obj_t* canvas, float phase_deg) {
    if (!canvas) return;
    lv_canvas_fill_bg(canvas, lv_color_black(), LV_OPA_TRANSP);

    /* Eje central atenuado */
    lv_draw_rect_dsc_t ax;
    lv_draw_rect_dsc_init(&ax);
    ax.bg_color = GF_COLOR_GRAY; ax.bg_opa = 55;
    lv_canvas_draw_rect(canvas, 0, LFO_H / 2, LFO_W, 1, &ax);

    const float mid = (LFO_H - 1) / 2.0f;
    const float amp = (LFO_H - 1) / 2.0f - 3.0f;
    const float ph  = phase_deg * (float)M_PI / 180.0f;

    lv_point_t pts[64];
    for (int i = 0; i < 64; i++) {
        float u = (float)i / 63.0f;
        /* 2 ciclos visibles con scroll de fase */
        float t = u * 2.0f * 2.0f * (float)M_PI + ph;
        pts[i].x = (lv_coord_t)lroundf(u * (LFO_W - 1));
        pts[i].y = (lv_coord_t)lroundf(mid - sinf(t) * amp);
    }

    lv_draw_line_dsc_t d;
    lv_draw_line_dsc_init(&d);
    d.color = GF_COLOR_TEAL_PLUS; d.width = 2; d.opa = LV_OPA_COVER;
    d.round_start = d.round_end = 1;
    lv_canvas_draw_line(canvas, pts, 64, &d);
}

/* ── Creación de arc reutilizable ─────────────────────────────────────────── */

/**
 * @brief Crea un lv_arc 120×120 estilo GF centrado en la página.
 *
 * La misma configuración se usa para DEPTH y MIX. No tiene knob ni click.
 */
static lv_obj_t* create_level_arc(lv_obj_t* page) {
    lv_obj_t* arc = lv_arc_create(page);
    lv_obj_set_size(arc, 120, 120);
    lv_obj_align(arc, LV_ALIGN_CENTER, 0, -5);
    lv_arc_set_rotation(arc, 135);
    lv_arc_set_bg_angles(arc, 0, 270);
    lv_arc_set_range(arc, 0, 100);
    lv_arc_set_value(arc, 0);
    lv_obj_set_style_arc_color(arc, GF_COLOR_TEAL_DIM,  LV_PART_MAIN);
    lv_obj_set_style_arc_color(arc, GF_COLOR_TEAL_PLUS, LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(arc, 10, LV_PART_MAIN);
    lv_obj_set_style_arc_width(arc, 10, LV_PART_INDICATOR);
    lv_obj_remove_style(arc, NULL, LV_PART_KNOB);
    lv_obj_clear_flag(arc, LV_OBJ_FLAG_CLICKABLE);
    return arc;
}

/* ── Page builders ────────────────────────────────────────────────────────── */

static void build_rate_page(lv_obj_t* page, uint8_t idx, const char* title) {
    lv_obj_t* t = gf_label(page, title, GF_FONT_LABEL, GF_COLOR_TEAL_PLUS);
    if (t) lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 12);

    lv_obj_t* cv = gf_canvas_create(page, LFO_W, LFO_H);
    if (cv) lv_obj_align(cv, LV_ALIGN_CENTER, 0, -12);
    s_pages[idx].canvas = cv;

    lv_obj_t* vl = gf_label(page, "0.1 HZ", GF_FONT_HERO, GF_COLOR_WHITE);
    if (vl) lv_obj_align(vl, LV_ALIGN_BOTTOM_MID, 0, -10);
    s_pages[idx].val_lbl = vl;
    s_pages[idx].viz     = LFOVIZ_RATE;
}

static void build_depth_page(lv_obj_t* page, uint8_t idx, const char* title) {
    lv_obj_t* t = gf_label(page, title, GF_FONT_LABEL, GF_COLOR_TEAL_PLUS);
    if (t) lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 12);

    s_pages[idx].arc = create_level_arc(page);

    lv_obj_t* vl = gf_label(page, "0%", GF_FONT_HERO, GF_COLOR_WHITE);
    if (vl) lv_obj_align(vl, LV_ALIGN_CENTER, 0, -5);
    s_pages[idx].val_lbl = vl;
    s_pages[idx].viz     = LFOVIZ_DEPTH;
}

static void build_onoff_page(lv_obj_t* page, uint8_t idx, const char* title) {
    lv_obj_t* t = gf_label(page, title, GF_FONT_LABEL, GF_COLOR_TEAL_PLUS);
    if (t) lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 12);

    /* Dot de 40px centrado arriba del texto */
    lv_obj_t* dot = gf_dot(page, 40, GF_COLOR_GRAY);
    if (dot) lv_obj_align(dot, LV_ALIGN_CENTER, 0, -22);
    s_pages[idx].dot_obj = dot;

    lv_obj_t* vl = gf_label(page, "OFF", GF_FONT_HERO, GF_COLOR_GRAY);
    if (vl) lv_obj_align(vl, LV_ALIGN_CENTER, 0, 22);
    s_pages[idx].val_lbl = vl;
    s_pages[idx].viz     = LFOVIZ_ONOFF;
}

static void build_mode_page(lv_obj_t* page, uint8_t idx, const char* title) {
    lv_obj_t* t = gf_label(page, title, GF_FONT_LABEL, GF_COLOR_TEAL_PLUS);
    if (t) lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 12);

    lv_obj_t* vl = gf_label(page, "MODE I", GF_FONT_HERO, GF_COLOR_WHITE);
    if (vl) lv_obj_align(vl, LV_ALIGN_CENTER, 0, 0);
    s_pages[idx].val_lbl = vl;
    s_pages[idx].viz     = LFOVIZ_MODE;
}

static void build_mix_page(lv_obj_t* page, uint8_t idx, const char* title) {
    lv_obj_t* t = gf_label(page, title, GF_FONT_LABEL, GF_COLOR_TEAL_PLUS);
    if (t) lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 12);

    s_pages[idx].arc = create_level_arc(page);

    /* Labels "A" y "B" a los lados del arc */
    lv_obj_t* la = gf_label(page, "A", GF_FONT_LABEL, GF_COLOR_TEAL_PLUS);
    if (la) lv_obj_align(la, LV_ALIGN_CENTER, -68, -5);

    lv_obj_t* lb = gf_label(page, "B", GF_FONT_LABEL, GF_COLOR_TEAL_PLUS);
    if (lb) lv_obj_align(lb, LV_ALIGN_CENTER, 68, -5);

    /* Texto compuesto "Axx%/Bxx%" en GF_FONT_LABEL debajo del arc */
    lv_obj_t* vl = gf_label(page, "A100%/B0%", GF_FONT_LABEL, GF_COLOR_WHITE);
    if (vl) lv_obj_align(vl, LV_ALIGN_BOTTOM_MID, 0, -10);
    s_pages[idx].val_lbl = vl;
    s_pages[idx].viz     = LFOVIZ_MIX;
}

/* ── Timer principal — 33ms ────────────────────────────────────────────────── */

static void lfo_tick(lv_timer_t* /*t*/) {
    /* Sincronizar slider con cursor del Teensy */
    uint8_t cur = bridge_get_synth_param();
    if (cur >= 2) cur = 0;
    if (cur != s_last_param) {
        s_last_param = cur;
        gf_pslider_set_active(s_slider, cur, true);
    }

    for (uint8_t i = 0; i < 2; i++) {
        float norm = bridge_get_synth_param_cached(s_engine_idx, 3 /* LFO */, i);
        lfo_page_t& p = s_pages[i];

        switch (p.viz) {

            case LFOVIZ_RATE: {
                /* Avanzar fase según rate actual */
                float hz = 0.1f + norm * 9.9f;
                float phase_inc = hz * 360.0f * 0.033f;
                s_rate_phase = fmodf(s_rate_phase + phase_inc, 360.0f);
                draw_rate_canvas(p.canvas, s_rate_phase);

                if (p.val_lbl) {
                    static char buf[16];
                    snprintf(buf, sizeof(buf), "%.1f HZ", (double)hz);
                    lv_label_set_text(p.val_lbl, buf);
                }
                break;
            }

            case LFOVIZ_DEPTH: {
                int pct = (int)lroundf(norm * 100.0f);
                if (pct < 0) pct = 0;
                if (pct > 100) pct = 100;
                if (p.arc) lv_arc_set_value(p.arc, pct);
                if (p.val_lbl) {
                    static char buf[8];
                    snprintf(buf, sizeof(buf), "%d%%", pct);
                    lv_label_set_text(p.val_lbl, buf);
                }
                break;
            }

            case LFOVIZ_ONOFF: {
                bool on = (norm > 0.5f);
                lv_color_t c = on ? GF_COLOR_TEAL_PLUS : GF_COLOR_GRAY;
                if (p.val_lbl) {
                    lv_label_set_text(p.val_lbl, on ? "ON" : "OFF");
                    lv_obj_set_style_text_color(p.val_lbl, c, 0);
                }
                if (p.dot_obj) {
                    lv_obj_set_style_bg_color(p.dot_obj, c, 0);
                }
                break;
            }

            case LFOVIZ_MODE: {
                bool mode2 = (norm > 0.5f);
                if (p.val_lbl) {
                    lv_label_set_text(p.val_lbl, mode2 ? "MODE II" : "MODE I");
                }
                break;
            }

            case LFOVIZ_MIX: {
                /* norm=0 → 100% OSC A, norm=1 → 100% OSC B */
                int pct_b = (int)lroundf(norm * 100.0f);
                if (pct_b < 0) pct_b = 0;
                if (pct_b > 100) pct_b = 100;
                int pct_a = 100 - pct_b;
                if (p.arc) lv_arc_set_value(p.arc, pct_b);
                if (p.val_lbl) {
                    static char buf[20];
                    snprintf(buf, sizeof(buf), "A%d%%/B%d%%", pct_a, pct_b);
                    lv_label_set_text(p.val_lbl, buf);
                }
                break;
            }

            default:
                break;
        }
    }
}

/* ── Create / Destroy ─────────────────────────────────────────────────────────── */

void view_05_create(lv_obj_t* parent) {
    s_slider     = nullptr;
    s_timer      = nullptr;
    s_last_param = 0xFF;
    s_rate_phase = 0.0f;
    for (uint8_t i = 0; i < 2; i++) s_pages[i] = lfo_page_t{};

    uint8_t eng_id = bridge_get_engine_id();
    s_engine_idx = (eng_id >= 0x10 && eng_id <= 0x12) ? (eng_id - 0x10) : 0;

    const lfo_engine_t& eng = LFO_ENGINES[s_engine_idx];

    gf_screen_bg(parent);
    s_slider = gf_pslider_create(parent, eng.num);

    for (uint8_t i = 0; i < eng.num; i++) {
        lv_obj_t* page = gf_pslider_get_page(s_slider, i);
        if (!page) continue;
        const lfo_param_t& pd = eng.params[i];
        switch (pd.viz) {
            case LFOVIZ_RATE:  build_rate_page (page, i, pd.title); break;
            case LFOVIZ_DEPTH: build_depth_page(page, i, pd.title); break;
            case LFOVIZ_ONOFF: build_onoff_page(page, i, pd.title); break;
            case LFOVIZ_MODE:  build_mode_page (page, i, pd.title); break;
            case LFOVIZ_MIX:   build_mix_page  (page, i, pd.title); break;
            default: break;
        }
    }

    uint8_t init_param = bridge_get_synth_param();
    if (init_param >= eng.num) init_param = 0;
    gf_pslider_set_active(s_slider, init_param, false);
    s_last_param = init_param;

    gf_page_dots(parent, eng.num, init_param, GF_COLOR_TEAL);
    gf_mode_pill(parent, "LFO", GF_COLOR_TEAL);

    s_timer = lv_timer_create(lfo_tick, 33, nullptr);
    lfo_tick(nullptr);
}

void view_05_destroy(void) {
    if (s_timer) { lv_timer_del(s_timer); s_timer = nullptr; }
    s_slider     = nullptr;
    s_last_param = 0xFF;
    for (uint8_t i = 0; i < 2; i++) s_pages[i] = lfo_page_t{};
}
