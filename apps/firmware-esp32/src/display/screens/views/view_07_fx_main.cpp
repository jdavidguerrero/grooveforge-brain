/**
 * @file view_07_fx_main.cpp
 * @brief Vista 07 — FX MAIN (Sprint 30, RMX-style con arcos concéntricos)
 *
 * Layout de 3 arcos concéntricos centrados en (120,120):
 *
 *   WET arc   — 220px Ø, 10px stroke, 360°, púrpura.
 *               Indica el nivel wet/dry global. Anillo exterior.
 *
 *   Sub-arc L — 196px Ø, 6px stroke, bg_angles 90°→180° (↙).
 *               Pegado al borde interior del WET arc (radio 98px vs WET a 100px).
 *               Cuadrante inferior-izquierdo. Indicator crece desde abajo (90°)
 *               hacia la izquierda (180°). Cuando val=100% llena el cuadrante.
 *
 *   Sub-arc R — 196px Ø, 6px stroke, bg_angles 0°→90° (↘).
 *               Cuadrante inferior-derecho. Indicator crece desde la derecha (0°)
 *               hacia abajo (90°). Cuando val=100% llena el cuadrante.
 *
 *   Ambos sub-arcs en 100% → el semicírculo inferior completo queda iluminado.
 *
 * Spectrum analyzer — 6 barras, centradas (x=[91..149]), dentro de la U, y≤212.
 *
 * Focus mode — sin animaciones: lv_obj_set_style_opa() directo en el timer
 * para respuesta inmediata al movimiento del encoder. Idle 2.5s → vuelve a
 * opacidades por defecto.
 *
 * Timer de 50ms (20fps) — todo in-place, sin carousel_goto ni rebuild.
 *
 * Parámetros categóricos:
 *   Modal Reverb (fx=1) param=0: 0→GLASS, 1→METAL, 2→WOOD, 3→STONE
 *   Spring Plate (fx=7) param=0: 0→SPRING, 1→PLATE, 2→HALL, 3→ROOM
 */

#include "views.h"
#include "../../ui_theme.h"
#include "../../widgets/gf_widgets.h"
#include "../../../bridge/bridge_handlers.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

/* ── Constantes de layout ────────────────────────────────────────────────── */

/* Sub-arcs — pegados al WET arc (radio 98px vs borde interior WET en 100px) */
static constexpr lv_coord_t SUB_SIZE = 196;   ///< 196×196px → radio = 98px
static constexpr lv_coord_t SUB_POS  = 22;    ///< pos (22,22) → centro en (120,120)
static constexpr lv_coord_t SUB_W    = 6;     ///< grosor del stroke

/* Spectrum — 6 barras, centradas en x=[91..148], subidas para dejar espacio
 * debajo de los labels de sub-params (que ahora están en y≈+4..+18 del centro) */
static constexpr uint8_t SPEC_N       = 8;   ///< 8 bandas = todas las del bridge
static constexpr uint8_t SPEC_BAR_W   = 8;
static constexpr uint8_t SPEC_BAR_GAP = 2;
static constexpr uint8_t SPEC_BAR_MAX = 40;  ///< altura máxima visible dentro de la U
static constexpr uint8_t SPEC_BAR_MIN = 5;   ///< mínimo siempre visible
static constexpr uint8_t SPEC_BAR_BOT = 183; ///< subido: 8×8+7×2=78px → x_start=81
/* 8×8 + 7×2 = 78px → x_start = (240-78)/2 = 81 */
static constexpr uint8_t SPEC_X_START = 81;

/* Focus mode — opacidades sin animación para respuesta inmediata */
static constexpr uint32_t FOCUS_IDLE_MS = 2500;
static constexpr lv_opa_t OPA_FULL   = LV_OPA_COVER;  ///< 255  — activo/enfocado
static constexpr lv_opa_t OPA_DIM    = (lv_opa_t)178; ///< ~70% — idle sin bypass
static constexpr lv_opa_t OPA_FADE   = (lv_opa_t)102; ///< ~40% — background
static constexpr lv_opa_t OPA_BYPASS = (lv_opa_t)45;  ///< ~18% — bypass ON (muy tenue)

/* ── Estado persistente de la vista ─────────────────────────────────────── */

static lv_obj_t*   s_wet_arc    = nullptr;
// s_wet_label eliminado — GUI minimalista: solo arcs, sin porcentajes
static lv_obj_t*   s_fx_label   = nullptr;
static lv_obj_t*   s_sub_arc[2] = {};
static lv_obj_t*   s_sub_name[2]= {};
// s_sub_val eliminado — GUI minimalista: solo arcs + nombre del param
static lv_obj_t*   s_spec_bars[SPEC_N] = {};
static lv_timer_t* s_timer      = nullptr;

/* Focus state */
static uint8_t  s_focus         = 0xFF;
static uint8_t  s_prev_focus    = 0xFF;
static uint32_t s_focus_last_ms = 0;
static uint16_t s_focus_last_id = 0xFFFF;

/* ── Helpers: parámetros categóricos ─────────────────────────────────────── */

static const char* categorical_str(uint8_t fx, uint8_t param, float val) {
    // val llega normalizado [0,1]. Reconstruir índice con roundf(val * N_opciones-1).
    if (fx == 1 && param == 0) {
        static const char* M[] = {"GLASS","METAL","WOOD","STONE"};
        int idx = (int)roundf(val * 3.0f);
        if (idx < 0) idx = 0;
        if (idx > 3) idx = 3;
        return M[idx];
    }
    if (fx == 7 && param == 0) {
        static const char* A[] = {"SPRING","PLATE","HALL","ROOM"};
        int idx = (int)roundf(val * 3.0f);
        if (idx < 0) idx = 0;
        if (idx > 3) idx = 3;
        return A[idx];
    }
    return nullptr;
}

static void update_sub_val_label(lv_obj_t* lbl, uint8_t fx, uint8_t param, float val) {
    if (!lbl) return;
    // Para params int (categóricos), el Teensy envía valor crudo (0,1,2,3) →
    // categorical_str() lo interpreta directamente.
    // Para params no-int, el Teensy normaliza a [0,1] → mostramos como porcentaje.
    const char* cat = categorical_str(fx, param, val);
    static char buf[16];
    if (cat) {
        lv_label_set_text(lbl, cat);
    } else {
        snprintf(buf, sizeof(buf), "%d%%", (int)(val * 100.0f + 0.5f));
        lv_label_set_text(lbl, buf);
    }
}

/* ── Focus mode — opa directa, sin lv_anim ──────────────────────────────── */

static void set_focus_opa(uint8_t focus) {
    lv_opa_t wet_opa  = OPA_FULL;
    lv_opa_t subL_opa = OPA_DIM;
    lv_opa_t subR_opa = OPA_DIM;

    switch (focus) {
        case 0:  wet_opa=OPA_FULL; subL_opa=OPA_FADE; subR_opa=OPA_FADE; break;
        case 1:  wet_opa=OPA_FADE; subL_opa=OPA_FULL; subR_opa=OPA_FADE; break;
        case 2:  wet_opa=OPA_FADE; subL_opa=OPA_FADE; subR_opa=OPA_FULL; break;
        default: /* idle */                                                 break;
    }

    if (s_wet_arc)     lv_obj_set_style_opa(s_wet_arc,     wet_opa,  0);
    if (s_sub_arc[0])  lv_obj_set_style_opa(s_sub_arc[0],  subL_opa, 0);
    if (s_sub_arc[1])  lv_obj_set_style_opa(s_sub_arc[1],  subR_opa, 0);
    if (s_sub_name[0]) lv_obj_set_style_opa(s_sub_name[0], subL_opa, 0);
    if (s_sub_name[1]) lv_obj_set_style_opa(s_sub_name[1], subR_opa, 0);
}

/* ── Timer callback 20fps (50ms) ─────────────────────────────────────────── */

static void fx_main_timer_cb(lv_timer_t* /*t*/) {
    uint8_t fx_id = bridge_get_engine_id();
    if (fx_id >= 9) fx_id = 0;
    uint8_t sub1 = bridge_get_sub1(fx_id);
    uint8_t sub2 = bridge_get_sub2(fx_id);

    /* 1. WET arc (solo arco, sin label de porcentaje) */
    float wet    = bridge_get_wet_dry();
    int   wet_pct = (int)(wet * 100.0f + 0.5f);
    if (s_wet_arc) lv_arc_set_value(s_wet_arc, wet_pct);

    /* 2. FX name */
    if (s_fx_label) lv_label_set_text(s_fx_label, bridge_get_engine_name());

    /* 3. Sub-arcs + nombre del param (sin label de porcentaje) */
    float val1 = bridge_get_param_cached(fx_id, sub1);
    float val2 = bridge_get_param_cached(fx_id, sub2);
    if (s_sub_arc[0]) lv_arc_set_value(s_sub_arc[0], (int16_t)(val1*100.0f+0.5f));
    if (s_sub_arc[1]) lv_arc_set_value(s_sub_arc[1], (int16_t)(val2*100.0f+0.5f));
    if (s_sub_name[0]) lv_label_set_text(s_sub_name[0], bridge_get_param_name(fx_id,sub1));
    if (s_sub_name[1]) lv_label_set_text(s_sub_name[1], bridge_get_param_name(fx_id,sub2));

    /* 4. Focus mode — opa inmediata, sin animación */
    uint16_t last_id = bridge_get_last_real_param_id();
    uint32_t now     = lv_tick_get();
    uint8_t  new_focus = s_focus;

    if (last_id != s_focus_last_id) {
        s_focus_last_id = last_id;
        s_focus_last_ms = now;
        if (last_id == 0x00FF) {
            new_focus = 0;
        } else {
            uint8_t lf = (uint8_t)(last_id >> 8);
            uint8_t lp = (uint8_t)(last_id & 0xFF);
            if      (lf == fx_id && lp == sub1) new_focus = 1;
            else if (lf == fx_id && lp == sub2) new_focus = 2;
            else                                 new_focus = 0xFF;
        }
    } else if (s_focus != 0xFF && (now - s_focus_last_ms) > FOCUS_IDLE_MS) {
        new_focus = 0xFF;
    }

    if (new_focus != s_prev_focus) {
        s_focus      = new_focus;
        s_prev_focus = new_focus;
        set_focus_opa(new_focus);   /* inmediato — sin lv_anim */
    }

    /* 5. Bypass visual — dim pronunciado cuando wet≈0 e idle.
     * wet arc principal: OPA_BYPASS (~18%) — contraste claro vs estado activo.
     * sub-arcs + labels: OPA_BYPASS también — todo apagado en bypass idle.
     * Solo aplica en s_focus==0xFF: si el usuario gira ENC L/R el arc que toca
     * se ilumina para feedback visual, vuelve a bypass-dim al soltar (2.5s). */
    if (wet < 0.01f && s_focus == 0xFF) {
        if (s_wet_arc) lv_obj_set_style_opa(s_wet_arc, OPA_BYPASS, 0);
        for (int i = 0; i < 2; i++) {
            if (s_sub_arc[i])  lv_obj_set_style_opa(s_sub_arc[i],  OPA_BYPASS, 0);
            if (s_sub_name[i]) lv_obj_set_style_opa(s_sub_name[i], OPA_BYPASS, 0);
        }
    } else if (wet >= 0.01f) {
        /* Al salir de bypass (wet sube), restaurar wet arc a opacidad normal.
         * Los sub-arcs los maneja set_focus_opa() — no tocarlos aquí. */
        if (s_wet_arc) lv_obj_set_style_opa(s_wet_arc, OPA_FULL, 0);
    }

    /* 6. Spectrum bars */
    for (uint8_t b = 0; b < SPEC_N; b++) {
        if (!s_spec_bars[b]) continue;
        float level = bridge_get_spectrum(b);
        uint8_t h = (uint8_t)(level*(SPEC_BAR_MAX-SPEC_BAR_MIN)+SPEC_BAR_MIN+0.5f);
        if (h > SPEC_BAR_MAX) h = SPEC_BAR_MAX;
        if (h < SPEC_BAR_MIN) h = SPEC_BAR_MIN;
        lv_obj_set_size(s_spec_bars[b], SPEC_BAR_W, h);
        lv_obj_set_pos(s_spec_bars[b],
                       SPEC_X_START + b*(SPEC_BAR_W+SPEC_BAR_GAP),
                       SPEC_BAR_BOT - h);
        uint8_t mix = (uint8_t)((b*40)+(uint8_t)(level*60));
        lv_obj_set_style_bg_color(s_spec_bars[b],
            lv_color_mix(GF_COLOR_PURPLE_BRIGHT, GF_COLOR_TEAL_PLUS, mix),
            LV_PART_MAIN);
    }
}

/* ── Helper: crear sub-arco concéntrico ─────────────────────────────────── */

static lv_obj_t* make_sub_arc(lv_obj_t* parent, uint16_t bg_start, uint16_t bg_end) {
    lv_obj_t* arc = lv_arc_create(parent);
    lv_obj_set_size(arc, SUB_SIZE, SUB_SIZE);
    lv_obj_set_pos(arc, SUB_POS, SUB_POS);   /* (22,22) → centro (120,120) */
    lv_arc_set_rotation(arc, 0);
    lv_arc_set_bg_angles(arc, bg_start, bg_end);
    lv_arc_set_range(arc, 0, 100);
    lv_arc_set_value(arc, 0);
    lv_obj_remove_style(arc, nullptr, LV_PART_KNOB);
    lv_obj_set_style_arc_color(arc, GF_COLOR_TEAL_DIM,  LV_PART_MAIN);
    lv_obj_set_style_arc_color(arc, GF_COLOR_TEAL_PLUS, LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(arc, SUB_W, LV_PART_MAIN);
    lv_obj_set_style_arc_width(arc, SUB_W, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(arc, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_clear_flag(arc, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_opa(arc, OPA_DIM, 0);
    return arc;
}

/* ── Entrada principal ───────────────────────────────────────────────────── */

void view_07_create(lv_obj_t* parent) {
    s_wet_arc = s_fx_label = nullptr;
    for (int i = 0; i < 2; i++) s_sub_arc[i] = s_sub_name[i] = nullptr;
    for (int i = 0; i < SPEC_N; i++) s_spec_bars[i] = nullptr;
    s_timer        = nullptr;
    s_focus        = 0xFF;
    s_prev_focus   = 0xFF;
    s_focus_last_id= 0xFFFF;

    gf_screen_bg(parent);
    gf_tint(parent, GF_COLOR_PURPLE, 46);

    uint8_t fx_id = bridge_get_engine_id();
    if (fx_id >= 9) fx_id = 0;
    uint8_t sub1 = bridge_get_sub1(fx_id);
    uint8_t sub2 = bridge_get_sub2(fx_id);
    float   wet  = bridge_get_wet_dry();

    /* ── A. Outer WET arc (220×220, 10px, 360°) ─────────────────────────── */
    lv_obj_t* warc = lv_arc_create(parent);
    lv_obj_set_size(warc, 220, 220);
    lv_obj_center(warc);
    lv_arc_set_rotation(warc, 270);
    lv_arc_set_bg_angles(warc, 0, 360);
    lv_arc_set_range(warc, 0, 100);
    lv_arc_set_value(warc, (int16_t)(wet*100.0f+0.5f));
    lv_obj_remove_style(warc, nullptr, LV_PART_KNOB);
    lv_obj_set_style_arc_color(warc, GF_COLOR_PURPLE,        LV_PART_MAIN);
    lv_obj_set_style_arc_color(warc, GF_COLOR_PURPLE_BRIGHT, LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(warc, 10, LV_PART_MAIN);
    lv_obj_set_style_arc_width(warc, 10, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(warc, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_clear_flag(warc, LV_OBJ_FLAG_CLICKABLE);
    s_wet_arc = warc;

    /* ── B. FX name — grande, blanco, protagonismo visual ───────────────── */
    /* ancho=160px + wrap evita que nombres largos ("Cymatic Resonator") cubran los arcs */
    lv_obj_t* fnm = gf_label(parent, bridge_get_engine_name(),
                              GF_FONT_TITLE, GF_COLOR_WHITE);
    if (fnm) {
        lv_obj_set_width(fnm, 160);
        lv_label_set_long_mode(fnm, LV_LABEL_LONG_WRAP);
        lv_obj_set_style_text_align(fnm, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_align(fnm, LV_ALIGN_CENTER, 0, -38);
    }
    s_fx_label = fnm;

    /* WET % label eliminado — GUI minimalista, solo el arco comunica el nivel. */

    /* ── D. Sub-arc L (196px, bg_angles 90→180, cuadrante ↙) ───────────── */
    s_sub_arc[0] = make_sub_arc(parent, 90, 180);
    lv_arc_set_value(s_sub_arc[0],
                     (int16_t)(bridge_get_param_cached(fx_id,sub1)*100.0f+0.5f));

    /* Labels al inicio del sub-arc izquierdo (tip en 180° = 9 o'clock = lado izq).
     * Posición: x=-63 (dentro del safe zone Ø220), y=+4/+16 (ligeramente bajo centro).
     * dist_centro = √(63²+4²) ≈ 63 < 110 ✓
     * Font subido: nombre MICRO→BODY, valor LABEL (igual pero más legible con posición). */
    /* Sub-param labels: mismo font IBM Plex Mono a 16px (GF_FONT_BODY = 73% de
     * GF_FONT_TITLE 22px). Nombre gris (secundario), valor teal+ (activo). */
    lv_obj_t* nm0 = gf_label(parent, bridge_get_param_name(fx_id,sub1),
                              GF_FONT_LABEL, GF_COLOR_GRAY);
    if (nm0) {
        lv_obj_align(nm0, LV_ALIGN_CENTER, -55, +3);
        lv_obj_set_style_opa(nm0, OPA_DIM, 0);
    }
    s_sub_name[0] = nm0;

    /* Sub-param value label eliminado — solo el arc indica el nivel. */

    /* ── E. Sub-arc R (196px, bg_angles 0→90, cuadrante ↘) ─────────────── */
    s_sub_arc[1] = make_sub_arc(parent, 0, 90);
    lv_arc_set_value(s_sub_arc[1],
                     (int16_t)(bridge_get_param_cached(fx_id,sub2)*100.0f+0.5f));

    /* Labels al inicio del sub-arc derecho (tip en 0° = 3 o'clock = lado der, simétrico). */
    lv_obj_t* nm1 = gf_label(parent, bridge_get_param_name(fx_id,sub2),
                              GF_FONT_LABEL, GF_COLOR_GRAY);
    if (nm1) {
        lv_obj_align(nm1, LV_ALIGN_CENTER, +55, +3);
        lv_obj_set_style_opa(nm1, OPA_DIM, 0);
    }
    s_sub_name[1] = nm1;

    /* Sub-param value label eliminado — solo el arc indica el nivel. */

    /* ── F. Spectrum (6 barras, centradas, dentro de la U, y≤212) ──────── */
    for (uint8_t b = 0; b < SPEC_N; b++) {
        float level = bridge_get_spectrum(b);
        uint8_t h = (uint8_t)(level*(SPEC_BAR_MAX-SPEC_BAR_MIN)+SPEC_BAR_MIN+0.5f);
        if (h > SPEC_BAR_MAX) h = SPEC_BAR_MAX;
        if (h < SPEC_BAR_MIN) h = SPEC_BAR_MIN;

        lv_obj_t* bar = lv_obj_create(parent);
        lv_obj_set_style_bg_color(bar, GF_COLOR_TEAL_PLUS, LV_PART_MAIN);
        lv_obj_set_style_bg_opa(bar, LV_OPA_80, LV_PART_MAIN);
        lv_obj_set_style_border_width(bar, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(bar, 0, LV_PART_MAIN);
        lv_obj_set_style_radius(bar, 2, LV_PART_MAIN);
        lv_obj_set_size(bar, SPEC_BAR_W, h);
        lv_obj_set_pos(bar,
                       SPEC_X_START + b*(SPEC_BAR_W+SPEC_BAR_GAP),
                       SPEC_BAR_BOT - h);
        s_spec_bars[b] = bar;
    }

    /* ── G. Timer 50ms ───────────────────────────────────────────────────── */
    s_timer = lv_timer_create(fx_main_timer_cb, 50, nullptr);
}

void view_07_destroy(void) {
    if (s_timer) { lv_timer_del(s_timer); s_timer = nullptr; }
    s_wet_arc = s_fx_label = nullptr;
    for (int i = 0; i < 2; i++) s_sub_arc[i] = s_sub_name[i] = nullptr;
    for (int i = 0; i < SPEC_N; i++) s_spec_bars[i] = nullptr;
}
