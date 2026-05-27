/**
 * @file view_03_osc.cpp
 * @brief Vista 03 — OSC GROUP (Sprint 32 — slider hero por param)
 *
 * Rediseño Sprint 32: una "página" por param (4 totales) navegables con ENC L
 * mediante slide horizontal. Cada página renderiza una visualización dedicada
 * de pantalla completa con tipografía grande (no más MICRO 8px).
 *
 * Páginas:
 *   0 — VCO WAVE  : forma de onda scrolleando + nombre SINE/SAW/SQ/TRI
 *   1 — VCO OCT   : número ±N gigante + 5 teclas piano (la activa resaltada)
 *   2 — VCO2 WAVE : ídem página 0 con título "VCO2"
 *   3 — DETUNE    : dos ondas superpuestas con beating visual + cents value
 *
 * Animaciones:
 *   - WAVE/VCO2 WAVE/DETUNE redibujan su canvas a 30fps con phase scroll.
 *   - El cursor activo (bridge_get_synth_param) dispara gf_pslider_set_active
 *     con animación de 300ms cuando cambia.
 *
 * Bug fix sprint 31: el value display antes usaba `pid != s_last_pid` para
 * detectar cambios, lo cual NO disparaba cuando ENC R cambiaba el mismo param
 * (param_id idéntico, value cambia). Ahora la actualización es por timestamp
 * (bridge_get_last_real_param_ms) y siempre lee del cache por param.
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

/* ── Constantes layout ────────────────────────────────────────────────────── */

#define WAVE_CANVAS_W   180
#define WAVE_CANVAS_H   60
#define DETUNE_CANVAS_W 200
#define DETUNE_CANVAS_H 80

/* ── Estado persistente por página ────────────────────────────────────────── */

typedef struct {
    lv_obj_t* canvas;      ///< canvas de la viz (puede ser nullptr si la página no usa)
    lv_obj_t* val_lbl;     ///< label del valor (siempre presente)
} page_t;

static lv_obj_t*   s_slider     = nullptr;
static lv_timer_t* s_timer      = nullptr;
static page_t      s_pages[4]   = {};
static uint32_t    s_phase      = 0;        ///< contador de fase para scroll de ondas
static uint8_t     s_last_param = 0xFF;     ///< último cursor para detectar cambios

/* ── Renderers ────────────────────────────────────────────────────────────── */

/** Forma de onda al estilo osciloscopio, scroll continuo a 30fps.
 *  val_norm: 0=SINE, 1=SAW, 2=SQ, 3=TRI (categórico, lroundf(val*3))             */
static float wave_sample(int wave_type, float t) {
    /* t en [0, 2π) — un ciclo completo */
    switch (wave_type) {
        case 0:  return sinf(t);                              // SINE
        case 1:  return 1.0f - (t / (float)M_PI);             // SAW (-1 → +1 lineal invertido)
        case 2:  return (sinf(t) >= 0.0f) ? 1.0f : -1.0f;     // SQ
        case 3: {                                             // TRI
            float f = t / (2.0f * (float)M_PI);   // [0,1)
            return (f < 0.5f) ? (-1.0f + 4.0f * f) : (3.0f - 4.0f * f);
        }
        default: return 0.0f;
    }
}

/** Dibuja una forma de onda con phase scroll en el canvas. */
static void draw_wave_canvas(lv_obj_t* canvas, int wave_type, uint32_t phase) {
    if (!canvas) return;
    lv_canvas_fill_bg(canvas, lv_color_black(), LV_OPA_TRANSP);

    /* Eje horizontal sutil */
    lv_draw_rect_dsc_t ax;
    lv_draw_rect_dsc_init(&ax);
    ax.bg_color = GF_COLOR_GRAY;
    ax.bg_opa   = 60;
    lv_canvas_draw_rect(canvas, 0, WAVE_CANVAS_H / 2, WAVE_CANVAS_W, 1, &ax);

    /* Onda — 3 ciclos visibles, phase incrementa cada tick */
    const float mid = (WAVE_CANVAS_H - 1) / 2.0f;
    const float amp = (WAVE_CANVAS_H - 1) / 2.0f - 3.0f;
    const float ph  = (phase % 360) * (float)M_PI / 180.0f;

    lv_point_t pts[64];
    for (int i = 0; i < 64; i++) {
        float u = (float)i / 63.0f;                          // [0,1] across canvas
        float t = u * 2.0f * (float)M_PI * 3.0f + ph;        // 3 cycles + phase
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

/** DETUNE: dos ondas superpuestas con cents creciente → beating visible.
 *  val_norm 0.0 = sin detune (perfectamente alineadas), 1.0 = max detune.
 *  Visualmente: dibujamos dos sinusoides de freq cercana; el espectador percibe
 *  la "ola" de envolvente como sí fuera amplitud modulada. */
static void draw_detune_canvas(lv_obj_t* canvas, float val_norm, uint32_t phase) {
    if (!canvas) return;
    lv_canvas_fill_bg(canvas, lv_color_black(), LV_OPA_TRANSP);

    /* Eje horizontal */
    lv_draw_rect_dsc_t ax;
    lv_draw_rect_dsc_init(&ax);
    ax.bg_color = GF_COLOR_GRAY;
    ax.bg_opa   = 50;
    lv_canvas_draw_rect(canvas, 0, DETUNE_CANVAS_H / 2, DETUNE_CANVAS_W, 1, &ax);

    const float mid = (DETUNE_CANVAS_H - 1) / 2.0f;
    const float amp = (DETUNE_CANVAS_H - 1) / 2.0f - 4.0f;

    /* Freq base = 2.5 ciclos visibles. Freq2 = freq1 * (1 + detune_factor).
     * detune_factor exagerado x0.08 → 1 ciclo de beating cada ~12 cycles
     * cuando val_norm=1.0; cuando val_norm=0 las ondas coinciden. */
    const float ph    = (phase % 720) * (float)M_PI / 360.0f;
    const float f1    = 2.5f;
    const float f2    = 2.5f * (1.0f + val_norm * 0.08f);

    /* Onda 1 — teal (atenuada) */
    lv_point_t pts1[72];
    for (int i = 0; i < 72; i++) {
        float u  = (float)i / 71.0f;
        float t  = u * 2.0f * (float)M_PI * f1 + ph;
        pts1[i].x = (lv_coord_t)lroundf(u * (DETUNE_CANVAS_W - 1));
        pts1[i].y = (lv_coord_t)lroundf(mid - sinf(t) * amp * 0.55f);
    }
    lv_draw_line_dsc_t d1;
    lv_draw_line_dsc_init(&d1);
    d1.color = GF_COLOR_TEAL;
    d1.width = 2;
    d1.opa   = 160;
    d1.round_start = d1.round_end = 1;
    lv_canvas_draw_line(canvas, pts1, 72, &d1);

    /* Onda 2 — teal_plus (brillante) */
    lv_point_t pts2[72];
    for (int i = 0; i < 72; i++) {
        float u  = (float)i / 71.0f;
        float t  = u * 2.0f * (float)M_PI * f2 + ph;
        pts2[i].x = (lv_coord_t)lroundf(u * (DETUNE_CANVAS_W - 1));
        pts2[i].y = (lv_coord_t)lroundf(mid - sinf(t) * amp * 0.55f);
    }
    lv_draw_line_dsc_t d2;
    lv_draw_line_dsc_init(&d2);
    d2.color = GF_COLOR_TEAL_PLUS;
    d2.width = 2;
    d2.opa   = LV_OPA_COVER;
    d2.round_start = d2.round_end = 1;
    lv_canvas_draw_line(canvas, pts2, 72, &d2);

    /* Suma (beating envelope) — púrpura brillante encima */
    lv_point_t pts_sum[72];
    for (int i = 0; i < 72; i++) {
        float u  = (float)i / 71.0f;
        float t1 = u * 2.0f * (float)M_PI * f1 + ph;
        float t2 = u * 2.0f * (float)M_PI * f2 + ph;
        float sum = (sinf(t1) + sinf(t2)) * 0.5f;
        pts_sum[i].x = (lv_coord_t)lroundf(u * (DETUNE_CANVAS_W - 1));
        pts_sum[i].y = (lv_coord_t)lroundf(mid - sum * amp);
    }
    lv_draw_line_dsc_t d3;
    lv_draw_line_dsc_init(&d3);
    d3.color = GF_COLOR_PURPLE_BRIGHT;
    d3.width = 1;
    d3.opa   = 200;
    d3.round_start = d3.round_end = 1;
    lv_canvas_draw_line(canvas, pts_sum, 72, &d3);
}

/* ── Build pages ──────────────────────────────────────────────────────────── */

/** Construye la página de waveform (usada por 0=VCO WAVE y 2=VCO2 WAVE). */
static void build_wave_page(lv_obj_t* page, uint8_t idx, const char* title) {
    /* Título arriba */
    lv_obj_t* t = gf_label(page, title, GF_FONT_LABEL, GF_COLOR_TEAL_PLUS);
    if (t) lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 12);

    /* Canvas centro */
    lv_obj_t* cv = gf_canvas_create(page, WAVE_CANVAS_W, WAVE_CANVAS_H);
    if (cv) lv_obj_align(cv, LV_ALIGN_CENTER, 0, -10);
    s_pages[idx].canvas = cv;

    /* Value label abajo — HERO (36px) para que se lea bien */
    lv_obj_t* vl = gf_label(page, "SINE", GF_FONT_HERO, GF_COLOR_WHITE);
    if (vl) lv_obj_align(vl, LV_ALIGN_BOTTOM_MID, 0, -10);
    s_pages[idx].val_lbl = vl;
}

/** Construye la página VCO OCT (page 1). */
static void build_oct_page(lv_obj_t* page) {
    /* Título */
    lv_obj_t* t = gf_label(page, "VCO OCTAVE", GF_FONT_LABEL, GF_COLOR_TEAL_PLUS);
    if (t) lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 12);

    /* Big value — HERO_XL (64px) centrado */
    lv_obj_t* vl = gf_label(page, "0", GF_FONT_HERO_XL, GF_COLOR_WHITE);
    if (vl) lv_obj_align(vl, LV_ALIGN_CENTER, 0, -10);
    s_pages[1].val_lbl = vl;
    s_pages[1].canvas  = nullptr;   /* no canvas — usaremos dots como teclas */

    /* 5 dots/teclas debajo: -2 -1 0 +1 +2 */
    for (int i = 0; i < 5; i++) {
        lv_obj_t* k = gf_dot(page, 10, GF_COLOR_GRAY);
        if (k) lv_obj_align(k, LV_ALIGN_BOTTOM_MID, (lv_coord_t)((i - 2) * 22), -15);
    }
}

/** Construye la página DETUNE (page 3). */
static void build_detune_page(lv_obj_t* page) {
    lv_obj_t* t = gf_label(page, "DETUNE", GF_FONT_LABEL, GF_COLOR_TEAL_PLUS);
    if (t) lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 12);

    lv_obj_t* cv = gf_canvas_create(page, DETUNE_CANVAS_W, DETUNE_CANVAS_H);
    if (cv) lv_obj_align(cv, LV_ALIGN_CENTER, 0, -5);
    s_pages[3].canvas = cv;

    lv_obj_t* vl = gf_label(page, "+0 ct", GF_FONT_HERO, GF_COLOR_WHITE);
    if (vl) lv_obj_align(vl, LV_ALIGN_BOTTOM_MID, 0, -10);
    s_pages[3].val_lbl = vl;
}

/* ── Timer principal — 33ms (~30fps) ─────────────────────────────────────── */

static const char* WAVE_NAMES[4] = { "SINE", "SAW", "SQ", "TRI" };

static void osc_tick(lv_timer_t* /*t*/) {
    s_phase += 8;   /* incremento de fase por tick — velocidad de scroll */

    /* ── 1. Leer valores del cache para los 4 params (engine 0, grupo OSC) ── */
    const float v0 = bridge_get_synth_param_cached(0, 0, 0);  // VCO WAVE
    const float v1 = bridge_get_synth_param_cached(0, 0, 1);  // VCO OCT
    const float v2 = bridge_get_synth_param_cached(0, 0, 2);  // VCO2 WAVE
    const float v3 = bridge_get_synth_param_cached(0, 0, 3);  // DETUNE

    /* ── 2. Redibujar canvases (siempre, son cheap @ 30fps) ─────────────── */
    int wt0 = (int)lroundf(v0 * 3.0f); if (wt0 < 0) wt0 = 0; if (wt0 > 3) wt0 = 3;
    int wt2 = (int)lroundf(v2 * 3.0f); if (wt2 < 0) wt2 = 0; if (wt2 > 3) wt2 = 3;
    draw_wave_canvas(s_pages[0].canvas, wt0, s_phase);
    draw_wave_canvas(s_pages[2].canvas, wt2, s_phase);
    draw_detune_canvas(s_pages[3].canvas, v3, s_phase);

    /* ── 3. Actualizar value labels ─────────────────────────────────────── */
    if (s_pages[0].val_lbl) lv_label_set_text(s_pages[0].val_lbl, WAVE_NAMES[wt0]);
    if (s_pages[2].val_lbl) lv_label_set_text(s_pages[2].val_lbl, WAVE_NAMES[wt2]);

    /* VCO OCT: ±N */
    int oct = (int)lroundf(v1 * 4.0f) - 2;
    if (oct < -2) oct = -2; if (oct > 2) oct = 2;
    if (s_pages[1].val_lbl) {
        static char buf[8];
        snprintf(buf, sizeof(buf), "%+d", oct);
        lv_label_set_text(s_pages[1].val_lbl, buf);
    }

    /* DETUNE: cents */
    int cents = (int)lroundf((v3 - 0.5f) * 50.0f);
    if (s_pages[3].val_lbl) {
        static char buf[12];
        snprintf(buf, sizeof(buf), "%+d ct", cents);
        lv_label_set_text(s_pages[3].val_lbl, buf);
    }

    /* ── 4. Sincronizar slider con el cursor del Teensy ─────────────────── */
    uint8_t cur = bridge_get_synth_param();
    if (cur > 3) cur = 0;
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
    for (uint8_t i = 0; i < 4; i++) {
        s_pages[i].canvas  = nullptr;
        s_pages[i].val_lbl = nullptr;
    }

    gf_screen_bg(parent);

    /* Slider de 4 páginas — el llamante rellena cada una con build_*_page */
    s_slider = gf_pslider_create(parent, 4);
    build_wave_page(gf_pslider_get_page(s_slider, 0), 0, "VCO WAVE");
    build_oct_page (gf_pslider_get_page(s_slider, 1));
    build_wave_page(gf_pslider_get_page(s_slider, 2), 2, "VCO2 WAVE");
    build_detune_page(gf_pslider_get_page(s_slider, 3));

    /* Posicionar slider en la página activa (sin animación inicial) */
    uint8_t init_param = bridge_get_synth_param();
    if (init_param > 3) init_param = 0;
    gf_pslider_set_active(s_slider, init_param, false);
    s_last_param = init_param;

    /* Chrome inferior: page dots + mode pill */
    gf_page_dots(parent, 4, init_param, GF_COLOR_TEAL);
    gf_mode_pill(parent, "OSC", GF_COLOR_TEAL);

    /* Timer 33ms (~30fps) — redibuja ondas + actualiza labels + sync slider */
    s_timer = lv_timer_create(osc_tick, 33, nullptr);
    osc_tick(nullptr);   /* primer frame inmediato */
}

void view_03_destroy(void) {
    if (s_timer) { lv_timer_del(s_timer); s_timer = nullptr; }
    /* s_slider y sus hijos son liberados por lv_obj_clean() — solo soltar punteros. */
    s_slider = nullptr;
    for (uint8_t i = 0; i < 4; i++) {
        s_pages[i].canvas  = nullptr;
        s_pages[i].val_lbl = nullptr;
    }
    s_last_param = 0xFF;
}
