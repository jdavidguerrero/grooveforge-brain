/**
 * @file view_24_filter.cpp
 * @brief Vista 24 — FILTER GROUP (PARAM_LIST, Sprint 31)
 *
 * 2 páginas en gf_pslider, una por parámetro del grupo FILTER:
 *   0  CUTOFF     — arc grande 0-100% + label "800 Hz" / "4.0 k"
 *   1  RESONANCE  — arc grande 0-100% + label "2.5"
 *
 * Llamada cuando el Teensy envía 0x00F6 con group=2 desde PARAM_LIST (nivel 2).
 * Distinta de view_02 (SUBHOME) que muestra ambos arcos juntos en mini-format:
 * aquí cada param ocupa la pantalla completa para edición con mayor precisión visual.
 *
 * Parámetros del bridge (group=2 para FILTER):
 *   param 0: cutoff    norm [0,1] → Hz = 20 + norm×17980 (Moog: 20-18000)
 *   param 1: resonance norm [0,1] → Q  = 0.7 + norm×4.3  (Moog: 0.7-5.0)
 *
 * El cursor llega por bridge_get_synth_param() y sincroniza la página
 * activa del slider con animación 300ms.
 */

#include "views.h"
#include "../../ui_theme.h"
#include "../../widgets/gf_widgets.h"
#include "../../widgets/gf_param_slider.h"
#include "../../../bridge/bridge_handlers.h"
#include <stdio.h>
#include <math.h>

/* ── Constantes de layout (idénticas a LFO depth en view_05) ──────────────────── */

#define FILTER_ARC_SZ  120   ///< diámetro del arc — igual que VIZ_LEVEL / LFO depth
#define FILTER_ARC_W    10   ///< stroke width — igual que VIZ_LEVEL / LFO depth

/* ── Estado global de la vista ──────────────────────────────────────────────── */

static lv_obj_t*   s_slider     = nullptr;
static lv_timer_t* s_timer      = nullptr;
static lv_obj_t*   s_arc[2]     = {};     ///< arc widget por página
static lv_obj_t*   s_val_lbl[2] = {};     ///< label de valor por página
static uint8_t     s_engine_idx = 0;
static uint8_t     s_last_param = 0xFF;

/* ── Formateo de valores ─────────────────────────────────────────────────────── */

/** Cutoff: norm→Hz, luego formateado como "800 Hz" o "4.0 k". */
static void fmt_cutoff(char* buf, size_t sz, float norm) {
    float hz = 20.0f + norm * 17980.0f;
    if (hz < 1000.0f)
        snprintf(buf, sz, "%.0f HZ", hz);
    else
        snprintf(buf, sz, "%.1f K", hz / 1000.0f);
}

/** Resonance: norm→Q (0.7-5.0), formateado como "2.5". */
static void fmt_resonance(char* buf, size_t sz, float norm) {
    float q = 0.7f + norm * 4.3f;
    snprintf(buf, sz, "%.2f", q);
}

/* ── Tick de actualización 30fps ─────────────────────────────────────────────── */

static void filter_timer_cb(lv_timer_t* /*t*/) {
    if (!s_slider) return;

    /* Sincronizar página activa con cursor del Teensy */
    uint8_t cur = bridge_get_synth_param();
    if (cur != s_last_param && cur < 2) {
        gf_pslider_set_active(s_slider, cur, true);
        s_last_param = cur;
    }

    /* Actualizar arc y label de cada página desde cache del bridge */
    for (uint8_t i = 0; i < 2; i++) {
        float norm = bridge_get_synth_param_cached(s_engine_idx, 2, i);

        /* Arc: norm → 0-100 (rango del widget lv_arc) */
        if (s_arc[i]) {
            int16_t v = (int16_t)(norm * 100.0f + 0.5f);
            if (v < 0)   v = 0;
            if (v > 100) v = 100;
            lv_arc_set_value(s_arc[i], v);
        }

        /* Label de valor formateado */
        if (s_val_lbl[i]) {
            char buf[16];
            if (i == 0) fmt_cutoff(buf, sizeof(buf), norm);
            else        fmt_resonance(buf, sizeof(buf), norm);
            lv_label_set_text(s_val_lbl[i], buf);
        }
    }
}

/* ── Helpers de construcción de páginas ──────────────────────────────────────── */

static const char* TITLES[2] = { "CUTOFF", "RESONANCE" };

static void build_page(uint8_t idx, lv_obj_t* page) {
    /* Título — copia exacta de build_depth_page (view_05): dy=12 */
    lv_obj_t* t = gf_label(page, TITLES[idx], GF_FONT_LABEL, GF_COLOR_TEAL_PLUS);
    if (t) lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 12);

    /* Arc — copia exacta de create_level_arc() en view_05:
     *   120×120, width=10, CENTER,-5, sin bg_opa explícito, sin knob.
     *   El valor se dibuja DENTRO del anillo (fondo transparente por defecto en LVGL). */
    lv_obj_t* arc = lv_arc_create(page);
    lv_obj_set_size(arc, FILTER_ARC_SZ, FILTER_ARC_SZ);
    lv_obj_align(arc, LV_ALIGN_CENTER, 0, -5);
    lv_arc_set_rotation(arc, 135);
    lv_arc_set_bg_angles(arc, 0, 270);
    lv_arc_set_range(arc, 0, 100);
    lv_arc_set_value(arc, 0);
    lv_obj_set_style_arc_color(arc, GF_COLOR_TEAL_DIM,  LV_PART_MAIN);
    lv_obj_set_style_arc_color(arc, GF_COLOR_TEAL_PLUS, LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(arc, FILTER_ARC_W, LV_PART_MAIN);
    lv_obj_set_style_arc_width(arc, FILTER_ARC_W, LV_PART_INDICATOR);
    lv_obj_remove_style(arc, NULL, LV_PART_KNOB);
    lv_obj_clear_flag(arc, LV_OBJ_FLAG_CLICKABLE);
    s_arc[idx] = arc;

    /* Label — GF_FONT_TITLE (22px) en lugar de HERO (36px): el valor cabe mejor
     * dentro del anillo de 120px con strings tipo "18.0 K" y "5.00". */
    lv_obj_t* vl = gf_label(page, "---", GF_FONT_TITLE, GF_COLOR_WHITE);
    if (vl) lv_obj_align(vl, LV_ALIGN_CENTER, 0, -5);
    s_val_lbl[idx] = vl;
}

/* ── API pública — create / destroy ─────────────────────────────────────────── */

void view_24_create(lv_obj_t* parent) {
    gf_screen_bg(parent);

    /* Engine activo — determina rangos de params (todos tienen grupo FILTER en idx 2) */
    uint8_t eid = bridge_get_engine_id();
    s_engine_idx = (eid >= 0x10) ? (uint8_t)(eid - 0x10) : 0;
    s_last_param = 0xFF;

    /* Slider con 2 páginas horizontales — geometría estándar 240×210 */
    s_slider = gf_pslider_create(parent, 2);

    for (uint8_t i = 0; i < 2; i++) {
        lv_obj_t* page = gf_pslider_get_page(s_slider, i);
        if (page) build_page(i, page);
    }

    /* Ir a la página del cursor actual sin animación (la vista acaba de abrirse) */
    uint8_t cur = bridge_get_synth_param();
    if (cur < 2) {
        gf_pslider_set_active(s_slider, cur, false);
        s_last_param = cur;
    }

    /* Page dots arriba del slider (y=8) — patrón consistente con OSC / ENV / LFO */
    gf_page_dots(parent, 2, (cur < 2) ? cur : 0, GF_COLOR_TEAL, 8);

    /* Timer 30fps para actualizar arcs y labels desde bridge cache */
    s_timer = lv_timer_create(filter_timer_cb, 33, nullptr);
}

void view_24_destroy(void) {
    if (s_timer) { lv_timer_del(s_timer); s_timer = nullptr; }
    s_slider = nullptr;
    for (uint8_t i = 0; i < 2; i++) { s_arc[i] = nullptr; s_val_lbl[i] = nullptr; }
}
