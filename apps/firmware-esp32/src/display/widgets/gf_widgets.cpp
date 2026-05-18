/**
 * @file gf_widgets.cpp
 * @brief Implementacion de los helpers de chrome — GrooveForge Brain
 */

#include "gf_widgets.h"
#include "../ui_theme.h"
#include <math.h>
#include <string.h>

/* ── Fondo y tint ────────────────────────────────────────────────────────── */

void gf_screen_bg(lv_obj_t* scr) {
    lv_obj_set_style_bg_color(scr, GF_COLOR_BG, 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    /* Limpiar cualquier gradiente que una vista previa (p.ej. Mode Switch)
     * haya dejado en la pantalla — el estilo de la pantalla persiste. */
    lv_obj_set_style_bg_grad_dir(scr, LV_GRAD_DIR_NONE, 0);
    lv_obj_set_style_pad_all(scr, 0, 0);
    lv_obj_set_style_border_width(scr, 0, 0);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
}

lv_obj_t* gf_tint(lv_obj_t* scr, lv_color_t c, lv_opa_t opa) {
    lv_obj_t* t = lv_obj_create(scr);
    lv_obj_set_size(t, GF_DISP_W, GF_DISP_H);
    lv_obj_center(t);
    lv_obj_set_style_bg_color(t, c, 0);
    lv_obj_set_style_bg_opa(t, opa, 0);
    lv_obj_set_style_border_width(t, 0, 0);
    lv_obj_set_style_radius(t, 0, 0);
    lv_obj_set_style_pad_all(t, 0, 0);
    lv_obj_clear_flag(t, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(t, LV_OBJ_FLAG_CLICKABLE);
    return t;
}

/* ── Labels ──────────────────────────────────────────────────────────────── */

lv_obj_t* gf_label(lv_obj_t* parent, const char* txt, const lv_font_t* font, lv_color_t color) {
    lv_obj_t* l = lv_label_create(parent);
    lv_label_set_text(l, txt);
    lv_obj_set_style_text_font(l, font, 0);
    lv_obj_set_style_text_color(l, color, 0);
    return l;
}

lv_obj_t* gf_hero_label(lv_obj_t* parent, const char* txt, const lv_font_t* font,
                        lv_color_t color, lv_coord_t dy) {
    lv_obj_t* l = gf_label(parent, txt, font, color);
    lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(l, LV_ALIGN_CENTER, 0, dy);
    return l;
}

/* ── Dot circular ────────────────────────────────────────────────────────── */

lv_obj_t* gf_dot(lv_obj_t* parent, lv_coord_t d, lv_color_t color) {
    lv_obj_t* dot = lv_obj_create(parent);
    lv_obj_set_size(dot, d, d);
    lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(dot, color, 0);
    lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(dot, 0, 0);
    lv_obj_set_style_pad_all(dot, 0, 0);
    lv_obj_clear_flag(dot, LV_OBJ_FLAG_SCROLLABLE);
    return dot;
}

/* ── Barra horizontal ────────────────────────────────────────────────────── */

lv_obj_t* gf_bar(lv_obj_t* parent, lv_coord_t w, lv_coord_t h, uint8_t pct, lv_color_t color) {
    lv_obj_t* bar = lv_bar_create(parent);
    lv_obj_set_size(bar, w, h);
    /* Pista de fondo: teal apagado. */
    lv_obj_set_style_bg_color(bar, GF_COLOR_TEAL_DIM, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(bar, h / 2, LV_PART_MAIN);
    /* Indicador: color dado. */
    lv_obj_set_style_bg_color(bar, color, LV_PART_INDICATOR);
    lv_obj_set_style_radius(bar, h / 2, LV_PART_INDICATOR);
    lv_bar_set_range(bar, 0, 100);
    lv_bar_set_value(bar, pct, LV_ANIM_OFF);
    return bar;
}

/* ── Arco de parametro ───────────────────────────────────────────────────── */

lv_obj_t* gf_arc(lv_obj_t* parent, lv_coord_t size, lv_coord_t width,
                 uint16_t bg_start, uint16_t bg_end, uint8_t value,
                 lv_color_t ind_color, lv_color_t track_color) {
    lv_obj_t* arc = lv_arc_create(parent);
    lv_obj_set_size(arc, size, size);
    lv_obj_center(arc);
    lv_arc_set_bg_angles(arc, bg_start, bg_end);
    lv_arc_set_range(arc, 0, 100);
    lv_arc_set_value(arc, value);
    lv_obj_set_style_arc_color(arc, track_color, LV_PART_MAIN);
    lv_obj_set_style_arc_color(arc, ind_color, LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(arc, width, LV_PART_MAIN);
    lv_obj_set_style_arc_width(arc, width, LV_PART_INDICATOR);
    /* Decorativo: sin knob, sin interaccion. */
    lv_obj_remove_style(arc, NULL, LV_PART_KNOB);
    lv_obj_set_style_pad_all(arc, 0, LV_PART_KNOB);
    lv_obj_clear_flag(arc, LV_OBJ_FLAG_CLICKABLE);
    return arc;
}

/* ── Mode pill ───────────────────────────────────────────────────────────── */

lv_obj_t* gf_mode_pill(lv_obj_t* parent, const char* txt, lv_color_t color) {
    lv_obj_t* pill = lv_obj_create(parent);
    /* Ancho automatico: la capsula se ajusta al texto + padding horizontal,
     * asi cabe tanto "FX" como "MIX-AWARE". Altura fija. */
    lv_obj_set_height(pill, GF_MODE_PILL_H);
    lv_obj_set_width(pill, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_hor(pill, 7, 0);
    lv_obj_set_style_pad_ver(pill, 0, 0);
    lv_obj_set_style_radius(pill, GF_MODE_PILL_H / 2, 0);
    lv_obj_set_style_bg_opa(pill, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_color(pill, color, 0);
    lv_obj_set_style_border_width(pill, 1, 0);
    lv_obj_clear_flag(pill, LV_OBJ_FLAG_SCROLLABLE);
    /* y=224 en coordenadas de pantalla → offset desde el centro (120). */
    lv_obj_align(pill, LV_ALIGN_CENTER, 0, GF_MODE_PILL_Y - GF_DISP_CY);

    lv_obj_t* l = gf_label(pill, txt, GF_FONT_MICRO, color);
    lv_obj_center(l);
    return pill;
}

/* ── Page dots ───────────────────────────────────────────────────────────── */

void gf_page_dots(lv_obj_t* parent, uint8_t count, uint8_t active, lv_color_t color) {
    if (count == 0) return;
    const lv_coord_t dy = GF_PAGE_DOTS_Y - GF_DISP_CY;       /* offset y del centro */
    const lv_coord_t total_w = (count - 1) * GF_PAGE_DOTS_GAP;

    for (uint8_t i = 0; i < count; i++) {
        const bool is_active = (i == active);
        const lv_coord_t d = is_active ? (GF_DOT_R_ACTIVE * 2) : (GF_DOT_R_IDLE * 2);
        lv_obj_t* dot = gf_dot(parent, d, color);
        /* Los inactivos se atenuan con opacidad. */
        lv_obj_set_style_bg_opa(dot, is_active ? LV_OPA_COVER : LV_OPA_40, 0);
        const lv_coord_t x = -total_w / 2 + i * GF_PAGE_DOTS_GAP;
        lv_obj_align(dot, LV_ALIGN_CENTER, x, dy);
    }
}

/* ── Arc title ───────────────────────────────────────────────────────────── */

void gf_arc_title(lv_obj_t* parent, const char* txt, lv_color_t color) {
    const size_t n = strlen(txt);
    if (n == 0) return;

    /* Paso angular por glifo: ~5° da el avance de IBM Plex Mono 11px sobre el
     * arco de radio 90 mas un poco de letter-spacing (los mocks lo piden). */
    const float step_deg = 5.0f;
    const float total_deg = n * step_deg;
    /* Centrado en 12 o'clock: en convencion matematica con y hacia abajo,
     * el top del circulo es -90°. */
    const float start_deg = -90.0f - total_deg / 2.0f;

    for (size_t i = 0; i < n; i++) {
        const float a_deg = start_deg + (i + 0.5f) * step_deg;
        const float a_rad = a_deg * (float)M_PI / 180.0f;
        const lv_coord_t x = (lv_coord_t)lroundf(GF_ARC_TITLE_R * cosf(a_rad));
        const lv_coord_t y = (lv_coord_t)lroundf(GF_ARC_TITLE_R * sinf(a_rad));

        char ch[2] = { txt[i], '\0' };
        lv_obj_t* gl = gf_label(parent, ch, GF_FONT_LABEL, color);
        /* x/y ya son offsets desde el centro del display. */
        lv_obj_align(gl, LV_ALIGN_CENTER, x, y);
    }
}
