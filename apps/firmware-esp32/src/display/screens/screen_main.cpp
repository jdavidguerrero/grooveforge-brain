/**
 * @file screen_main.cpp
 * @brief Pantalla principal — GrooveForge Brain
 *
 * Layout de la pantalla circular (240x240px):
 *
 * Coordenadas Y relativas al centro (120,120):
 *   -55px  → label "ENGINE" (categoria, gris 12)
 *   -20px  → label engine name (blanco 24, multilinea)
 *   +20px  → label fx chain (gris 12)
 *   +55px  → row: dot (8x8) + label "READY" (gris 12)
 *
 * El ring arc decorativo (BRAIN_COLOR_PURPLE, 8px stroke, 232x232px)
 * rodea el contenido visualmente — es estatico, no animado en screen_main.
 *
 * Punteros globales (static) para las funciones set_* — permiten actualizar
 * el contenido dinamicamente desde Bridge Protocol en Sprint 3.2.
 */

#include "screen_main.h"
#include "../ui_theme.h"
#include <lvgl.h>

/* ── Punteros a widgets actualizables ────────────────────────────────────── */

static lv_obj_t* s_lbl_engine   = nullptr;
static lv_obj_t* s_lbl_fx       = nullptr;
static lv_obj_t* s_dot_status   = nullptr;
static lv_obj_t* s_lbl_status   = nullptr;

/* ── Implementacion publica ──────────────────────────────────────────────── */

void screen_main_create(void) {
    lv_obj_t* scr = lv_scr_act();

    /* Fondo near-black */
    lv_obj_set_style_bg_color(scr, BRAIN_COLOR_BG, 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

    /* ── Ring arc decorativo — circulo completo purple estatico ─────────── */

    lv_obj_t* ring = lv_arc_create(scr);
    lv_obj_set_size(ring, BRAIN_RING_OBJ_SIZE, BRAIN_RING_OBJ_SIZE);
    lv_obj_center(ring);

    lv_arc_set_bg_angles(ring, 0, 360);
    lv_arc_set_angles(ring, 0, 360);  /* arc activo completo = ring solido */

    lv_obj_set_style_arc_color(ring, BRAIN_COLOR_PURPLE,   LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(ring, BRAIN_COLOR_PURPLE,   LV_PART_MAIN);
    lv_obj_set_style_arc_width(ring, BRAIN_RING_WIDTH, LV_PART_MAIN);
    lv_obj_set_style_arc_width(ring, BRAIN_RING_WIDTH, LV_PART_INDICATOR);

    /* Eliminar knob */
    lv_obj_remove_style(ring, NULL, LV_PART_KNOB);
    lv_obj_set_style_pad_all(ring, 0, LV_PART_KNOB);
    lv_obj_set_style_size(ring, 0, LV_PART_KNOB);

    lv_obj_clear_flag(ring, LV_OBJ_FLAG_CLICKABLE);

    /* ── Label "ENGINE" — categoria, gris, 12 ───────────────────────────── */

    lv_obj_t* lbl_cat = lv_label_create(scr);
    lv_label_set_text(lbl_cat, "ENGINE");
    lv_obj_set_style_text_color(lbl_cat, BRAIN_COLOR_GRAY, 0);
    lv_obj_set_style_text_font(lbl_cat, BRAIN_FONT_SM, 0);
    lv_obj_align(lbl_cat, LV_ALIGN_CENTER, 0, -55);

    /* ── Label engine name — blanco, 24, multilinea ─────────────────────── */

    s_lbl_engine = lv_label_create(scr);
    lv_label_set_text(s_lbl_engine, "MOOG\nMODEL D");
    lv_obj_set_style_text_color(s_lbl_engine, BRAIN_COLOR_WHITE, 0);
    lv_obj_set_style_text_font(s_lbl_engine, BRAIN_FONT_LG, 0);
    /* LV_TEXT_ALIGN_CENTER para alinear las dos lineas entre si */
    lv_obj_set_style_text_align(s_lbl_engine, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(s_lbl_engine, 180); /* ancho maximo dentro del circulo util */
    lv_obj_align(s_lbl_engine, LV_ALIGN_CENTER, 0, -15);

    /* ── Label FX chain — gris, 12 ──────────────────────────────────────── */

    s_lbl_fx = lv_label_create(scr);
    /* \xC2\xB7 = UTF-8 U+00B7 MIDDLE DOT — separador de FX en cadena */
    lv_label_set_text(s_lbl_fx, "TAPE \xC2\xB7 CHORUS");
    lv_obj_set_style_text_color(s_lbl_fx, BRAIN_COLOR_GRAY, 0);
    lv_obj_set_style_text_font(s_lbl_fx, BRAIN_FONT_SM, 0);
    lv_obj_set_style_text_align(s_lbl_fx, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(s_lbl_fx, LV_ALIGN_CENTER, 0, +28);

    /* ── Status row: dot + texto ─────────────────────────────────────────── */

    /*
     * El dot es un lv_obj con border_radius = mitad del tamano -> circulo.
     * Mas eficiente que un lv_label con "●" porque el color es directo,
     * sin depender del glifo de la fuente.
     */
    s_dot_status = lv_obj_create(scr);
    lv_obj_set_size(s_dot_status, BRAIN_STATUS_DOT_SIZE, BRAIN_STATUS_DOT_SIZE);
    lv_obj_set_style_radius(s_dot_status, BRAIN_STATUS_DOT_SIZE / 2, 0);
    lv_obj_set_style_bg_color(s_dot_status, BRAIN_COLOR_GREEN, 0);
    lv_obj_set_style_bg_opa(s_dot_status, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_dot_status, 0, 0);
    lv_obj_align(s_dot_status, LV_ALIGN_CENTER, -22, +55);

    s_lbl_status = lv_label_create(scr);
    lv_label_set_text(s_lbl_status, "READY");
    lv_obj_set_style_text_color(s_lbl_status, BRAIN_COLOR_GRAY, 0);
    lv_obj_set_style_text_font(s_lbl_status, BRAIN_FONT_SM, 0);
    lv_obj_align(s_lbl_status, LV_ALIGN_CENTER, +10, +55);
}

void screen_main_set_engine(const char* name) {
    if (s_lbl_engine != nullptr) {
        lv_label_set_text(s_lbl_engine, name);
        lv_obj_align(s_lbl_engine, LV_ALIGN_CENTER, 0, -15); /* realinear tras cambio de texto */
    }
}

void screen_main_set_fx(const char* fx_chain) {
    if (s_lbl_fx != nullptr) {
        lv_label_set_text(s_lbl_fx, fx_chain);
        lv_obj_align(s_lbl_fx, LV_ALIGN_CENTER, 0, +28);
    }
}

void screen_main_set_status(bool connected) {
    if (s_dot_status == nullptr || s_lbl_status == nullptr) return;

    if (connected) {
        lv_obj_set_style_bg_color(s_dot_status, BRAIN_COLOR_GREEN, 0);
        lv_label_set_text(s_lbl_status, "READY");
    } else {
        lv_obj_set_style_bg_color(s_dot_status, BRAIN_COLOR_RED, 0);
        lv_label_set_text(s_lbl_status, "OFFLINE");
    }
}
