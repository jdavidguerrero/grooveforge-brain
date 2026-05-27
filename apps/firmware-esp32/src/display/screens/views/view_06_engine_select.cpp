/**
 * @file view_06_engine_select.cpp
 * @brief Vista 06 — ENGINE SELECT (Sprint 31 Batch C — dinámica)
 *
 * Lista de engines: fila activa con fondo teal, code box por engine, filas
 * "coming soon" atenuadas con tag v1.1, scrollbar y footer hint.
 *
 * Sprint 31: la fila activa se actualiza en tiempo real via un timer de 100ms
 * que lee bridge_get_engine_cursor(). Los highlights de fondo (s_row_bg[]) se
 * muestran/ocultan cambiando bg_opa directamente, sin rebuild de la vista.
 *
 * Solo las primeras 3 filas son seleccionables (engines no "coming soon");
 * el cursor del bridge está limitado al rango 0-2 por el firmware del Teensy.
 */

#include "views.h"
#include "../../ui_theme.h"
#include "../../widgets/gf_widgets.h"
#include "../../../bridge/bridge_handlers.h"

/* ── Tabla de engines ────────────────────────────────────────────────────── */

struct EngineRow { const char* name; const char* code; bool soon; };
static constexpr EngineRow ENG[5] = {
    {"MOOG MODEL D", "M", false},
    {"JUNO-106",     "J", false},
    {"PROPHET-5",    "P", false},
    {"DX UNIT",      "D", true},
    {"OB SYNTH",     "O", true},
};
static constexpr uint8_t  NUM_ENG   = 5;
static constexpr uint8_t  NUM_SEL   = 3;   ///< engines seleccionables (no "coming soon")
static constexpr lv_coord_t DY[5] = {-56, -28, 0, 28, 56};

/* ── Estado persistente ──────────────────────────────────────────────────── */

/* Fondos de fila — solo para las filas seleccionables (índices 0..NUM_SEL-1). */
static lv_obj_t*   s_row_bg[NUM_SEL] = {};
static lv_timer_t* s_timer           = nullptr;
static uint8_t     s_last_cursor     = 0xFF;

/* ── Timer: actualiza el highlight de la fila activa ────────────────────── */

static void engine_timer_cb(lv_timer_t* /*t*/) {
    uint8_t cursor = bridge_get_engine_cursor();
    if (cursor >= NUM_SEL) cursor = 0;
    if (cursor == s_last_cursor) return;
    s_last_cursor = cursor;

    for (uint8_t i = 0; i < NUM_SEL; i++) {
        if (s_row_bg[i]) {
            lv_obj_set_style_bg_opa(s_row_bg[i], (i == cursor) ? 56 : 0, 0);
        }
    }
}

/* ── Entrada principal ───────────────────────────────────────────────────── */

void view_06_create(lv_obj_t* parent) {
    for (uint8_t i = 0; i < NUM_SEL; i++) s_row_bg[i] = nullptr;
    s_timer      = nullptr;
    s_last_cursor = 0xFF;

    gf_screen_bg(parent);
    gf_arc_title(parent, "ENGINE", GF_COLOR_TEAL);

    uint8_t cursor = bridge_get_engine_cursor();
    if (cursor >= NUM_SEL) cursor = 0;

    for (uint8_t i = 0; i < NUM_ENG; i++) {
        const bool is_act = (i == cursor);

        /* A. Fondo de la fila — creado siempre para filas seleccionables,
         *    con opa=0 cuando no está activa para que el timer pueda ajustarla. */
        if (i < NUM_SEL) {
            lv_obj_t* hl = lv_obj_create(parent);
            lv_obj_set_size(hl, 188, 26);
            lv_obj_set_style_radius(hl, 5, 0);
            lv_obj_set_style_bg_color(hl, GF_COLOR_TEAL, 0);
            lv_obj_set_style_bg_opa(hl, is_act ? 56 : 0, 0);
            lv_obj_set_style_border_width(hl, 0, 0);
            lv_obj_clear_flag(hl, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_align(hl, LV_ALIGN_CENTER, 0, DY[i]);
            s_row_bg[i] = hl;
        }

        /* B. Code box — cuadrado teal con la inicial del engine. */
        lv_obj_t* box = lv_obj_create(parent);
        lv_obj_set_size(box, 16, 16);
        lv_obj_set_style_radius(box, 2, 0);
        lv_obj_set_style_bg_color(box, ENG[i].soon ? GF_COLOR_TEAL_DIM : GF_COLOR_TEAL, 0);
        lv_obj_set_style_bg_opa(box, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(box, 0, 0);
        lv_obj_clear_flag(box, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_align(box, LV_ALIGN_CENTER, -76, DY[i]);
        lv_obj_t* code = gf_label(box, ENG[i].code, GF_FONT_MICRO, GF_COLOR_BG);
        lv_obj_center(code);

        /* C. Nombre del engine. */
        lv_color_t name_c = is_act ? GF_COLOR_WHITE : GF_COLOR_GRAY;
        lv_obj_t* nm = gf_label(parent, ENG[i].name, GF_FONT_LABEL, name_c);
        lv_obj_align(nm, LV_ALIGN_CENTER, -52, DY[i]);

        /* D. Tag v1.1 + atenuado para engines "coming soon". */
        if (ENG[i].soon) {
            lv_obj_set_style_opa(nm, 82, 0);
            lv_obj_t* tag = gf_label(parent, "v1.1", GF_FONT_MICRO, GF_COLOR_GRAY);
            lv_obj_set_style_opa(tag, 82, 0);
            lv_obj_align(tag, LV_ALIGN_CENTER, 76, DY[i]);
        }
    }

    /* E. Scrollbar — posición del thumb refleja cursor actual. */
    lv_obj_t* track = lv_obj_create(parent);
    lv_obj_set_size(track, 2, 120);
    lv_obj_set_style_bg_color(track, GF_COLOR_TEAL_DIM, 0);
    lv_obj_set_style_bg_opa(track, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(track, 0, 0);
    lv_obj_set_style_radius(track, 1, 0);
    lv_obj_clear_flag(track, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(track, LV_ALIGN_RIGHT_MID, -14, 0);

    /* Thumb estático en la posición inicial — el timer no lo actualiza por simplicidad;
     * el cursor cambia rápido y el thumb es solo feedback visual secundario. */
    lv_obj_t* thumb = lv_obj_create(parent);
    lv_obj_set_size(thumb, 2, 44);
    lv_obj_set_style_bg_color(thumb, GF_COLOR_TEAL, 0);
    lv_obj_set_style_bg_opa(thumb, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(thumb, 0, 0);
    lv_obj_set_style_radius(thumb, 1, 0);
    lv_obj_clear_flag(thumb, LV_OBJ_FLAG_SCROLLABLE);
    /* Desplazar thumb proporcionalmente al cursor (rango 0..NUM_SEL-1 → -38..+38). */
    lv_coord_t thumb_dy = (lv_coord_t)(-38 + cursor * 38);
    lv_obj_align(thumb, LV_ALIGN_RIGHT_MID, -14, thumb_dy);

    /* F. Footer hint. */
    lv_obj_t* hint = gf_label(parent, "NAV TO SELECT", GF_FONT_MICRO, GF_COLOR_GRAY);
    lv_obj_set_style_opa(hint, 150, 0);
    lv_obj_align(hint, LV_ALIGN_CENTER, 0, 88);

    gf_mode_pill(parent, "SYNTH", GF_COLOR_TEAL);

    /* ── Timer 100ms — actualiza highlight de fila activa ───────────────── */
    s_last_cursor = cursor;
    s_timer = lv_timer_create(engine_timer_cb, 100, nullptr);
}

void view_06_destroy(void) {
    if (s_timer) { lv_timer_del(s_timer); s_timer = nullptr; }
    for (uint8_t i = 0; i < NUM_SEL; i++) s_row_bg[i] = nullptr;
}
