/**
 * @file view_03_osc.cpp
 * @brief Vista 03 — OSC GROUP (Sprint 32 — engine-aware con slider hero)
 *
 * Cada engine tiene un set distinto de params OSC, así que esta vista lee el
 * engine activo (bridge_get_engine_id) y construye sus páginas dinámicamente:
 *
 *   Moog Model D (0x10):
 *     0  VCO1 WAVE   (animated waveform)
 *     1  TRANSPOSE   (vertical VU -2..+2)
 *     2  VCO2 WAVE   (animated waveform)
 *     3  DETUNE      (two-wave beating)
 *
 *   Juno-106 (0x11):
 *     0  VCO WAVE    (animated waveform)
 *     1  SUB LEVEL   (arc 0-100%)
 *     2  NOISE       (arc 0-100%)
 *
 *   Prophet-5 (0x12):
 *     0  OSC A WAVE  (animated waveform)
 *     1  OSC B WAVE  (animated waveform)
 *     2  B INTERVAL  (big number semitones)
 *     3  CROSSMOD    (arc 0-100%)
 *
 * Animaciones: timer @30fps redibuja canvases con phase scroll y actualiza
 * todos los value labels desde el cache del bridge. Cursor del Teensy
 * (bridge_get_synth_param) sincroniza el slider con animación slide 300ms.
 *
 * Naming honesto: "TRANSPOSE" en vez de "OCT" porque en el Moog actual es
 * una transposición global aplicada al MIDI note, no un setOctave por VCO.
 */

#include "views.h"
#include "../../ui_theme.h"
#include "../../widgets/gf_widgets.h"
#include "../../widgets/gf_glyphs.h"
#include "../../widgets/gf_param_slider.h"
#include "../../widgets/gf_anim.h"
#include "../../../bridge/bridge_handlers.h"
#include <stdio.h>
#include <math.h>

/* ── Layout ───────────────────────────────────────────────────────────────── */

#define WAVE_CANVAS_W      180
#define WAVE_CANVAS_H      60
#define DETUNE_CANVAS_W    200
#define DETUNE_CANVAS_H    80
#define VU_TRACK_W         16
#define VU_TRACK_H         110
#define ARC_LEVEL_SZ       130

/* ── Tipos de visualización ───────────────────────────────────────────────── */

typedef enum {
    VIZ_WAVE,        ///< Forma de onda animada — categórico 0=SINE 1=SAW 2=SQ 3=TRI
    VIZ_OCT,         ///< Vertical VU slider — discreto -2..+2 (norm 0.0=−2, 1.0=+2)
    VIZ_DETUNE,      ///< Dos sinusoides con beating — continuo 0..50 cents
    VIZ_LEVEL,       ///< Arc 0-100% — continuo 0..1
    VIZ_INTERVAL,    ///< Número grande semitonos — discreto 0..24
    VIZ_GENERIC,     ///< Fallback: número y label
} viz_type_t;

/* ── Descriptor por engine ────────────────────────────────────────────────── */

typedef struct {
    const char*  title;     ///< Texto del título (no se traduce)
    viz_type_t   viz;       ///< Cuál renderer usar
} osc_param_t;

typedef struct {
    osc_param_t  params[6];
    uint8_t      num;
} osc_engine_t;

static const osc_engine_t ENGINES[3] = {
    /* 0 — Moog Model D */
    {
        {
            { "VCO1 WAVE", VIZ_WAVE   },
            { "TRANSPOSE", VIZ_OCT    },
            { "VCO2 WAVE", VIZ_WAVE   },
            { "DETUNE",    VIZ_DETUNE },
        },
        4
    },
    /* 1 — Juno-106 */
    {
        {
            { "VCO WAVE",  VIZ_WAVE   },
            { "SUB LEVEL", VIZ_LEVEL  },
            { "NOISE",     VIZ_LEVEL  },
        },
        3
    },
    /* 2 — Prophet-5 */
    {
        {
            { "OSC A WAVE",  VIZ_WAVE     },
            { "OSC B WAVE",  VIZ_WAVE     },
            { "B INTERVAL",  VIZ_INTERVAL },
            { "CROSSMOD",    VIZ_LEVEL    },
        },
        4
    },
};

/* ── Estado por página (recursos LVGL) ───────────────────────────────────── */

typedef struct {
    viz_type_t viz;
    lv_obj_t*  canvas;     ///< canvas para wave/detune (nullptr si no aplica)
    lv_obj_t*  arc;        ///< arc para level (nullptr si no aplica)
    lv_obj_t*  vu_thumb;   ///< thumb del VU para oct (nullptr si no aplica)
    lv_obj_t*  val_lbl;    ///< siempre presente — muestra el valor formateado
    lv_obj_t*  aux_lbl;    ///< sufijo/auxiliar (ej: "ST" para INTERVAL)
} page_t;

/* ── Estado global de la vista ────────────────────────────────────────────── */

static lv_obj_t*   s_slider      = nullptr;
static lv_timer_t* s_timer       = nullptr;
static page_t      s_pages[6]    = {};
static uint8_t     s_num_pages   = 0;
static uint8_t     s_engine_idx  = 0;
static uint32_t    s_phase       = 0;
static uint8_t     s_last_param  = 0xFF;

/* ── Helpers de waveform ──────────────────────────────────────────────────── */

static float wave_sample(int wave_type, float t) {
    switch (wave_type) {
        case 0:  return sinf(t);
        case 1:  return 1.0f - (t / (float)M_PI);
        case 2:  return (sinf(t) >= 0.0f) ? 1.0f : -1.0f;
        case 3: {
            float f = t / (2.0f * (float)M_PI);
            return (f < 0.5f) ? (-1.0f + 4.0f * f) : (3.0f - 4.0f * f);
        }
        default: return 0.0f;
    }
}

static void draw_wave_canvas(lv_obj_t* canvas, int wave_type, uint32_t phase) {
    if (!canvas) return;
    lv_canvas_fill_bg(canvas, lv_color_black(), LV_OPA_TRANSP);

    /* Eje */
    lv_draw_rect_dsc_t ax;
    lv_draw_rect_dsc_init(&ax);
    ax.bg_color = GF_COLOR_GRAY;
    ax.bg_opa   = 60;
    lv_canvas_draw_rect(canvas, 0, WAVE_CANVAS_H / 2, WAVE_CANVAS_W, 1, &ax);

    const float mid = (WAVE_CANVAS_H - 1) / 2.0f;
    const float amp = (WAVE_CANVAS_H - 1) / 2.0f - 3.0f;
    const float ph  = (phase % 360) * (float)M_PI / 180.0f;

    lv_point_t pts[64];
    for (int i = 0; i < 64; i++) {
        float u = (float)i / 63.0f;
        float t = u * 2.0f * (float)M_PI * 3.0f + ph;
        t = fmodf(t, 2.0f * (float)M_PI);
        if (t < 0.0f) t += 2.0f * (float)M_PI;
        pts[i].x = (lv_coord_t)lroundf(u * (WAVE_CANVAS_W - 1));
        pts[i].y = (lv_coord_t)lroundf(mid - wave_sample(wave_type, t) * amp);
    }
    lv_draw_line_dsc_t d;
    lv_draw_line_dsc_init(&d);
    d.color = GF_COLOR_TEAL_PLUS;
    d.width = 2;
    d.opa   = LV_OPA_COVER;
    d.round_start = d.round_end = 1;
    lv_canvas_draw_line(canvas, pts, 64, &d);
}

static void draw_detune_canvas(lv_obj_t* canvas, float val_norm, uint32_t phase) {
    if (!canvas) return;
    lv_canvas_fill_bg(canvas, lv_color_black(), LV_OPA_TRANSP);

    lv_draw_rect_dsc_t ax;
    lv_draw_rect_dsc_init(&ax);
    ax.bg_color = GF_COLOR_GRAY;
    ax.bg_opa   = 50;
    lv_canvas_draw_rect(canvas, 0, DETUNE_CANVAS_H / 2, DETUNE_CANVAS_W, 1, &ax);

    const float mid = (DETUNE_CANVAS_H - 1) / 2.0f;
    const float amp = (DETUNE_CANVAS_H - 1) / 2.0f - 4.0f;
    const float ph  = (phase % 720) * (float)M_PI / 360.0f;
    const float f1  = 2.5f;
    const float f2  = 2.5f * (1.0f + val_norm * 0.08f);

    /* Onda 1 — teal atenuado */
    lv_point_t pts1[72], pts2[72], psum[72];
    for (int i = 0; i < 72; i++) {
        float u  = (float)i / 71.0f;
        float t1 = u * 2.0f * (float)M_PI * f1 + ph;
        float t2 = u * 2.0f * (float)M_PI * f2 + ph;
        lv_coord_t x = (lv_coord_t)lroundf(u * (DETUNE_CANVAS_W - 1));
        pts1[i].x = x;
        pts1[i].y = (lv_coord_t)lroundf(mid - sinf(t1) * amp * 0.55f);
        pts2[i].x = x;
        pts2[i].y = (lv_coord_t)lroundf(mid - sinf(t2) * amp * 0.55f);
        psum[i].x = x;
        psum[i].y = (lv_coord_t)lroundf(mid - (sinf(t1) + sinf(t2)) * 0.5f * amp);
    }
    lv_draw_line_dsc_t d1;
    lv_draw_line_dsc_init(&d1);
    d1.color = GF_COLOR_TEAL;
    d1.width = 2; d1.opa = 160; d1.round_start = d1.round_end = 1;
    lv_canvas_draw_line(canvas, pts1, 72, &d1);

    lv_draw_line_dsc_t d2;
    lv_draw_line_dsc_init(&d2);
    d2.color = GF_COLOR_TEAL_PLUS;
    d2.width = 2; d2.opa = LV_OPA_COVER; d2.round_start = d2.round_end = 1;
    lv_canvas_draw_line(canvas, pts2, 72, &d2);

    lv_draw_line_dsc_t d3;
    lv_draw_line_dsc_init(&d3);
    d3.color = GF_COLOR_PURPLE_BRIGHT;
    d3.width = 1; d3.opa = 200; d3.round_start = d3.round_end = 1;
    lv_canvas_draw_line(canvas, psum, 72, &d3);
}

/* ── VU vertical: actualiza height/posición del thumb según value ─────────── */

static void update_vu_thumb(lv_obj_t* thumb, float val_norm, lv_coord_t track_y_top) {
    if (!thumb) return;
    /* val_norm 0..1, centro = 0.5. Offset desde el centro en píxeles. */
    int offset_px = (int)lroundf((val_norm - 0.5f) * (float)VU_TRACK_H);
    lv_coord_t center_y = track_y_top + VU_TRACK_H / 2;

    if (offset_px > 0) {
        /* Fill desde el centro hacia arriba */
        lv_obj_set_y(thumb, center_y - offset_px);
        lv_obj_set_height(thumb, (lv_coord_t)offset_px);
    } else if (offset_px < 0) {
        /* Fill desde el centro hacia abajo */
        lv_obj_set_y(thumb, center_y);
        lv_obj_set_height(thumb, (lv_coord_t)(-offset_px));
    } else {
        /* Sin fill — línea de 1px en el centro */
        lv_obj_set_y(thumb, center_y - 1);
        lv_obj_set_height(thumb, 2);
    }
}

/* ── Page builders ────────────────────────────────────────────────────────── */

static void build_wave_page(lv_obj_t* page, uint8_t idx, const char* title) {
    lv_obj_t* t = gf_label(page, title, GF_FONT_LABEL, GF_COLOR_TEAL_PLUS);
    if (t) lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 12);

    lv_obj_t* cv = gf_canvas_create(page, WAVE_CANVAS_W, WAVE_CANVAS_H);
    if (cv) lv_obj_align(cv, LV_ALIGN_CENTER, 0, -10);
    s_pages[idx].canvas = cv;

    lv_obj_t* vl = gf_label(page, "SINE", GF_FONT_HERO, GF_COLOR_WHITE);
    if (vl) lv_obj_align(vl, LV_ALIGN_BOTTOM_MID, 0, -10);
    s_pages[idx].val_lbl = vl;
    s_pages[idx].viz     = VIZ_WAVE;
}

static void build_oct_page(lv_obj_t* page, uint8_t idx, const char* title) {
    lv_obj_t* t = gf_label(page, title, GF_FONT_LABEL, GF_COLOR_TEAL_PLUS);
    if (t) lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 12);

    /* VU vertical track — bg dim */
    const lv_coord_t track_x = 120 - VU_TRACK_W / 2;  /* centro horizontal */
    const lv_coord_t track_y = 36;                     /* debajo del título */

    lv_obj_t* track = lv_obj_create(page);
    lv_obj_set_size(track, VU_TRACK_W, VU_TRACK_H);
    lv_obj_set_pos(track, track_x, track_y);
    lv_obj_set_style_radius(track, VU_TRACK_W / 2, 0);
    lv_obj_set_style_bg_color(track, GF_COLOR_TEAL_DIM, 0);
    lv_obj_set_style_bg_opa(track, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(track, 0, 0);
    lv_obj_clear_flag(track, LV_OBJ_FLAG_SCROLLABLE);

    /* Marca de centro (línea horizontal blanca) */
    lv_obj_t* zero_mark = lv_obj_create(page);
    lv_obj_set_size(zero_mark, VU_TRACK_W + 14, 2);
    lv_obj_set_pos(zero_mark, track_x - 7, track_y + VU_TRACK_H / 2 - 1);
    lv_obj_set_style_radius(zero_mark, 0, 0);
    lv_obj_set_style_bg_color(zero_mark, GF_COLOR_GRAY, 0);
    lv_obj_set_style_bg_opa(zero_mark, 200, 0);
    lv_obj_set_style_border_width(zero_mark, 0, 0);
    lv_obj_clear_flag(zero_mark, LV_OBJ_FLAG_SCROLLABLE);

    /* Tick marks a la izquierda (-2,-1,0,+1,+2) */
    for (int i = -2; i <= 2; i++) {
        if (i == 0) continue;   /* el centro ya tiene zero_mark */
        lv_coord_t ty = track_y + VU_TRACK_H / 2 - (lv_coord_t)(i * VU_TRACK_H / 4) - 1;
        lv_obj_t* tk = lv_obj_create(page);
        lv_obj_set_size(tk, 6, 2);
        lv_obj_set_pos(tk, track_x - 10, ty);
        lv_obj_set_style_radius(tk, 0, 0);
        lv_obj_set_style_bg_color(tk, GF_COLOR_GRAY, 0);
        lv_obj_set_style_bg_opa(tk, 130, 0);
        lv_obj_set_style_border_width(tk, 0, 0);
        lv_obj_clear_flag(tk, LV_OBJ_FLAG_SCROLLABLE);
    }

    /* Thumb — fill brillante (se redimensiona en cada tick) */
    lv_obj_t* thumb = lv_obj_create(page);
    lv_obj_set_size(thumb, VU_TRACK_W, 1);  /* h se actualiza en update_vu_thumb */
    lv_obj_set_pos(thumb, track_x, track_y + VU_TRACK_H / 2);
    lv_obj_set_style_radius(thumb, VU_TRACK_W / 2, 0);
    lv_obj_set_style_bg_color(thumb, GF_COLOR_TEAL_PLUS, 0);
    lv_obj_set_style_bg_opa(thumb, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(thumb, 0, 0);
    lv_obj_clear_flag(thumb, LV_OBJ_FLAG_SCROLLABLE);
    s_pages[idx].vu_thumb = thumb;
    /* Guardamos el offset y del track en .canvas para reusar el ptr */
    s_pages[idx].canvas = (lv_obj_t*)(uintptr_t)track_y;  /* hack: reusar campo */

    /* Número ±N a la derecha del VU */
    lv_obj_t* vl = gf_label(page, "0", GF_FONT_HERO, GF_COLOR_WHITE);
    if (vl) lv_obj_align(vl, LV_ALIGN_LEFT_MID, 145, 0);
    s_pages[idx].val_lbl = vl;
    s_pages[idx].viz     = VIZ_OCT;
}

static void build_detune_page(lv_obj_t* page, uint8_t idx, const char* title) {
    lv_obj_t* t = gf_label(page, title, GF_FONT_LABEL, GF_COLOR_TEAL_PLUS);
    if (t) lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 12);

    lv_obj_t* cv = gf_canvas_create(page, DETUNE_CANVAS_W, DETUNE_CANVAS_H);
    if (cv) lv_obj_align(cv, LV_ALIGN_CENTER, 0, -5);
    s_pages[idx].canvas = cv;

    lv_obj_t* vl = gf_label(page, "+0 ct", GF_FONT_HERO, GF_COLOR_WHITE);
    if (vl) lv_obj_align(vl, LV_ALIGN_BOTTOM_MID, 0, -10);
    s_pages[idx].val_lbl = vl;
    s_pages[idx].viz     = VIZ_DETUNE;
}

static void build_level_page(lv_obj_t* page, uint8_t idx, const char* title) {
    lv_obj_t* t = gf_label(page, title, GF_FONT_LABEL, GF_COLOR_TEAL_PLUS);
    if (t) lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 12);

    /* Arc 0-100% como visualización principal */
    lv_obj_t* arc = lv_arc_create(page);
    lv_obj_set_size(arc, ARC_LEVEL_SZ, ARC_LEVEL_SZ);
    lv_obj_align(arc, LV_ALIGN_CENTER, 0, -5);
    lv_arc_set_rotation(arc, 135);
    lv_arc_set_bg_angles(arc, 0, 270);
    lv_arc_set_range(arc, 0, 100);
    lv_arc_set_value(arc, 50);
    lv_obj_set_style_arc_color(arc, GF_COLOR_TEAL_DIM, LV_PART_MAIN);
    lv_obj_set_style_arc_color(arc, GF_COLOR_TEAL_PLUS, LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(arc, 10, LV_PART_MAIN);
    lv_obj_set_style_arc_width(arc, 10, LV_PART_INDICATOR);
    lv_obj_remove_style(arc, NULL, LV_PART_KNOB);
    lv_obj_clear_flag(arc, LV_OBJ_FLAG_CLICKABLE);
    s_pages[idx].arc = arc;

    /* Porcentaje grande en el centro del arc */
    lv_obj_t* vl = gf_label(page, "0%", GF_FONT_HERO, GF_COLOR_WHITE);
    if (vl) lv_obj_align(vl, LV_ALIGN_CENTER, 0, -5);
    s_pages[idx].val_lbl = vl;
    s_pages[idx].viz     = VIZ_LEVEL;
}

static void build_interval_page(lv_obj_t* page, uint8_t idx, const char* title) {
    lv_obj_t* t = gf_label(page, title, GF_FONT_LABEL, GF_COLOR_TEAL_PLUS);
    if (t) lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 12);

    // Número grande — GF_FONT_HERO_XL (64px) tiene solo dígitos 0-9.
    // El sufijo "ST" va separado en GF_FONT_HERO (36px, ASCII completo).
    lv_obj_t* vl = gf_label(page, "0", GF_FONT_HERO_XL, GF_COLOR_WHITE);
    if (vl) lv_obj_align(vl, LV_ALIGN_CENTER, -20, -5);
    s_pages[idx].val_lbl = vl;

    lv_obj_t* su = gf_label(page, "ST", GF_FONT_HERO, GF_COLOR_TEAL_PLUS);
    if (su) lv_obj_align(su, LV_ALIGN_CENTER, 52, 22);
    s_pages[idx].aux_lbl = su;

    s_pages[idx].viz = VIZ_INTERVAL;
}

static void build_generic_page(lv_obj_t* page, uint8_t idx, const char* title) {
    lv_obj_t* t = gf_label(page, title, GF_FONT_LABEL, GF_COLOR_TEAL_PLUS);
    if (t) lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 12);

    lv_obj_t* vl = gf_label(page, "--", GF_FONT_HERO, GF_COLOR_WHITE);
    if (vl) lv_obj_align(vl, LV_ALIGN_CENTER, 0, 0);
    s_pages[idx].val_lbl = vl;
    s_pages[idx].viz     = VIZ_GENERIC;
}

/* ── Timer principal — 33ms ────────────────────────────────────────────── */

static const char* WAVE_NAMES[4] = { "SINE", "SAW", "SQ", "TRI" };

static void osc_tick(lv_timer_t* /*t*/) {
    s_phase += 8;

    for (uint8_t i = 0; i < s_num_pages; i++) {
        const float v = bridge_get_synth_param_cached(s_engine_idx, 0 /* OSC */, i);
        page_t&     p = s_pages[i];

        switch (p.viz) {
            case VIZ_WAVE: {
                int wt = (int)lroundf(v * 3.0f);
                if (wt < 0) wt = 0; if (wt > 3) wt = 3;
                draw_wave_canvas(p.canvas, wt, s_phase);
                if (p.val_lbl) lv_label_set_text(p.val_lbl, WAVE_NAMES[wt]);
                break;
            }
            case VIZ_OCT: {
                int oct = (int)lroundf(v * 4.0f) - 2;
                if (oct < -2) oct = -2; if (oct > 2) oct = 2;
                if (p.val_lbl) {
                    static char buf[8];
                    snprintf(buf, sizeof(buf), "%+d", oct);
                    lv_label_set_text(p.val_lbl, buf);
                }
                /* track_y guardado como hack en p.canvas */
                lv_coord_t track_y = (lv_coord_t)(uintptr_t)p.canvas;
                update_vu_thumb(p.vu_thumb, v, track_y);
                break;
            }
            case VIZ_DETUNE: {
                draw_detune_canvas(p.canvas, v, s_phase);
                int cents = (int)lroundf((v - 0.5f) * 50.0f);
                if (p.val_lbl) {
                    static char buf[12];
                    snprintf(buf, sizeof(buf), "%+d ct", cents);
                    lv_label_set_text(p.val_lbl, buf);
                }
                break;
            }
            case VIZ_LEVEL: {
                int pct = (int)lroundf(v * 100.0f);
                if (pct < 0) pct = 0; if (pct > 100) pct = 100;
                if (p.arc) lv_arc_set_value(p.arc, pct);
                if (p.val_lbl) {
                    static char buf[8];
                    snprintf(buf, sizeof(buf), "%d%%", pct);
                    lv_label_set_text(p.val_lbl, buf);
                }
                break;
            }
            case VIZ_INTERVAL: {
                int st = (int)lroundf(v * 24.0f);
                if (st < 0) st = 0; if (st > 24) st = 24;
                if (p.val_lbl) {
                    static char buf[8];
                    snprintf(buf, sizeof(buf), "%d", st);
                    lv_label_set_text(p.val_lbl, buf);
                }
                // p.aux_lbl ("ST") es estático, no necesita actualización
                break;
            }
            case VIZ_GENERIC:
            default: {
                if (p.val_lbl) {
                    static char buf[12];
                    snprintf(buf, sizeof(buf), "%.2f", (double)v);
                    lv_label_set_text(p.val_lbl, buf);
                }
                break;
            }
        }
    }

    /* Sincronizar slider con cursor del Teensy */
    uint8_t cur = bridge_get_synth_param();
    if (cur >= s_num_pages) cur = 0;
    if (cur != s_last_param) {
        s_last_param = cur;
        gf_pslider_set_active(s_slider, cur, true);
    }
}

/* ── Create / Destroy ─────────────────────────────────────────────────────── */

void view_03_create(lv_obj_t* parent) {
    s_slider     = nullptr;
    s_timer      = nullptr;
    s_phase      = 0;
    s_last_param = 0xFF;
    s_num_pages  = 0;
    for (uint8_t i = 0; i < 6; i++) s_pages[i] = page_t{};

    /* Engine activo: el ID llega como 0x10-0x12. Mapear a índice 0-2. */
    uint8_t eng_id  = bridge_get_engine_id();
    s_engine_idx    = (eng_id >= 0x10 && eng_id <= 0x12) ? (eng_id - 0x10) : 0;
    s_num_pages     = ENGINES[s_engine_idx].num;
    if (s_num_pages == 0 || s_num_pages > 6) s_num_pages = 1;

    gf_screen_bg(parent);

    /* Crear slider con el número de páginas del engine activo */
    s_slider = gf_pslider_create(parent, s_num_pages);

    for (uint8_t i = 0; i < s_num_pages; i++) {
        lv_obj_t* page = gf_pslider_get_page(s_slider, i);
        if (!page) continue;
        const osc_param_t& pd = ENGINES[s_engine_idx].params[i];
        switch (pd.viz) {
            case VIZ_WAVE:     build_wave_page    (page, i, pd.title); break;
            case VIZ_OCT:      build_oct_page     (page, i, pd.title); break;
            case VIZ_DETUNE:   build_detune_page  (page, i, pd.title); break;
            case VIZ_LEVEL:    build_level_page   (page, i, pd.title); break;
            case VIZ_INTERVAL: build_interval_page(page, i, pd.title); break;
            default:           build_generic_page (page, i, pd.title); break;
        }
    }

    uint8_t init_param = bridge_get_synth_param();
    if (init_param >= s_num_pages) init_param = 0;
    gf_pslider_set_active(s_slider, init_param, false);
    s_last_param = init_param;

    gf_page_dots(parent, s_num_pages, init_param, GF_COLOR_TEAL);
    gf_mode_pill(parent, "OSC", GF_COLOR_TEAL);

    s_timer = lv_timer_create(osc_tick, 33, nullptr);
    osc_tick(nullptr);
}

void view_03_destroy(void) {
    if (s_timer) { lv_timer_del(s_timer); s_timer = nullptr; }
    s_slider = nullptr;
    for (uint8_t i = 0; i < 6; i++) s_pages[i] = page_t{};
    s_last_param = 0xFF;
    s_num_pages  = 0;
}
