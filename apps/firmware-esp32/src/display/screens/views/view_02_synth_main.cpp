/**
 * @file view_02_synth_main.cpp
 * @brief Vista 02 — ENGINE SUB-HOME (Sprint 31 Batch C)
 *
 * Arc-view de sub-home para el engine activo en modo SYNTH. Muestra los dos
 * parámetros de filtro (cutoff/resonance) como arcos concéntricos, idénticos
 * en geometría a los sub-arcs de view_07 (FX MAIN) pero en color teal.
 *
 * Layout:
 *   A. Nombre del engine — título, teal, y=-38
 *   B. Arc izquierdo  — Cutoff, sub-arc ↙ (bg 90→180°), teal
 *   C. Arc derecho    — Resonance, sub-arc ↘ (bg 0→90°), teal
 *   D. Hint inferior  — "NAV → GROUPS", micro, gris
 *   E. Mode pill      — "SYNTH", teal
 *
 * El timer de 50ms lee bridge_get_synth_cutoff() / bridge_get_synth_resonance()
 * directamente — no usa s_param_cache (que es solo para FX) ni bridge_get_param_cached().
 *
 * Misma geometría que view_07 sub-arcs: 196×196px en pos (22,22), 6px stroke.
 */

#include "views.h"
#include "../../ui_theme.h"
#include "../../widgets/gf_widgets.h"
#include "../../../bridge/bridge_handlers.h"
#include <stdio.h>
#include <string.h>

/* ── Constantes de layout — idénticas a view_07 sub-arcs ─────────────────── */

static constexpr lv_coord_t SUB_SIZE = 196;
static constexpr lv_coord_t SUB_POS  = 22;    ///< pos (22,22) → centro en (120,120)
static constexpr lv_coord_t SUB_W    = 6;

/* ── Estado persistente ──────────────────────────────────────────────────── */

static lv_obj_t*   s_arc_cut  = nullptr;   ///< arc izquierdo — cutoff
static lv_obj_t*   s_arc_res  = nullptr;   ///< arc derecho — resonance
static lv_obj_t*   s_eng_label= nullptr;   ///< nombre del engine
static lv_timer_t* s_timer    = nullptr;

/* ── Helper: crear sub-arco teal ─────────────────────────────────────────── */

static lv_obj_t* make_arc(lv_obj_t* parent, uint16_t bg_start, uint16_t bg_end) {
    lv_obj_t* arc = lv_arc_create(parent);
    lv_obj_set_size(arc, SUB_SIZE, SUB_SIZE);
    lv_obj_set_pos(arc, SUB_POS, SUB_POS);
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
    return arc;
}

/* ── Timer callback 20fps ────────────────────────────────────────────────── */

static void subhome_timer_cb(lv_timer_t* /*t*/) {
    /* Nombre del engine — puede cambiar si llega ENGINE_CHANGED */
    if (s_eng_label) lv_label_set_text(s_eng_label, bridge_get_engine_name());

    /* Arcos leídos directamente de los getters de cutoff/resonance — independientes
     * del FX param_cache que es exclusivo del modo FX. */
    if (s_arc_cut) {
        int16_t v = (int16_t)(bridge_get_synth_cutoff() * 100.0f + 0.5f);
        lv_arc_set_value(s_arc_cut, v);
    }
    if (s_arc_res) {
        int16_t v = (int16_t)(bridge_get_synth_resonance() * 100.0f + 0.5f);
        lv_arc_set_value(s_arc_res, v);
    }
}

/* ── Entrada principal ───────────────────────────────────────────────────── */

void view_02_create(lv_obj_t* parent) {
    s_arc_cut = s_arc_res = s_eng_label = nullptr;
    s_timer = nullptr;

    gf_screen_bg(parent);
    gf_tint(parent, GF_COLOR_TEAL, 20);

    /* ── A. Nombre del engine — título, teal ───────────────────────────── */
    lv_obj_t* eng = gf_label(parent, bridge_get_engine_name(),
                             GF_FONT_TITLE, GF_COLOR_TEAL_PLUS);
    if (eng) {
        lv_obj_set_width(eng, 160);
        lv_label_set_long_mode(eng, LV_LABEL_LONG_WRAP);
        lv_obj_set_style_text_align(eng, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_align(eng, LV_ALIGN_CENTER, 0, -38);
    }
    s_eng_label = eng;

    /* ── B. Arc izquierdo — Cutoff (cuadrante ↙, bg 90→180°) ──────────── */
    s_arc_cut = make_arc(parent, 90, 180);
    lv_arc_set_value(s_arc_cut, (int16_t)(bridge_get_synth_cutoff() * 100.0f + 0.5f));

    /* Label "CUTOFF" en el lado izquierdo (tip en 180° = 9 o'clock) */
    lv_obj_t* lc = gf_label(parent, "CUTOFF", GF_FONT_LABEL, GF_COLOR_GRAY);
    if (lc) lv_obj_align(lc, LV_ALIGN_CENTER, -55, +3);

    /* ── C. Arc derecho — Resonance (cuadrante ↘, bg 0→90°) ───────────── */
    s_arc_res = make_arc(parent, 0, 90);
    lv_arc_set_value(s_arc_res, (int16_t)(bridge_get_synth_resonance() * 100.0f + 0.5f));

    /* Label "RESO" en el lado derecho (tip en 0° = 3 o'clock) */
    lv_obj_t* lr = gf_label(parent, "RESO", GF_FONT_LABEL, GF_COLOR_GRAY);
    if (lr) lv_obj_align(lr, LV_ALIGN_CENTER, +55, +3);

    /* ── D. Hint inferior ────────────────────────────────────────────────── */
    lv_obj_t* hint = gf_label(parent, "NAV \xE2\x86\x92 GROUPS", GF_FONT_MICRO, GF_COLOR_GRAY);
    if (hint) {
        lv_obj_set_style_opa(hint, 140, 0);
        lv_obj_align(hint, LV_ALIGN_CENTER, 0, 70);
    }

    /* ── E. Mode pill ────────────────────────────────────────────────────── */
    gf_mode_pill(parent, "SYNTH", GF_COLOR_TEAL);

    /* ── Timer 50ms ──────────────────────────────────────────────────────── */
    s_timer = lv_timer_create(subhome_timer_cb, 50, nullptr);
}

void view_02_destroy(void) {
    if (s_timer) { lv_timer_del(s_timer); s_timer = nullptr; }
    s_arc_cut = s_arc_res = s_eng_label = nullptr;
}
