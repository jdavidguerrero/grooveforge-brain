#pragma once

#include <cstdint>

#include "lvgl.h"

/**
 * @brief Dibujo directo dentro de un callback LV_EVENT_DRAW_MAIN (theory doc §5.4).
 *
 * Coordenadas locales al objeto que se dibuja. Los rects se redondean igual que el R() de los
 * diseños de Claude Design (Math.round en x, y, w, h), así la geometría coincide px a px.
 */
struct Painter {
    lv_draw_ctx_t* ctx;
    lv_coord_t ox, oy;

    /// Painter del objeto que recibe el evento.
    static Painter from(lv_event_t* e);

    void fill(float x, float y, float w, float h, uint32_t color) const;
    /// Marco interior de @p t px.
    void frame(int x, int y, int w, int h, int t, uint32_t color) const;
    /// Texto con la línea base en @p baseline (como fillText con textBaseline 'alphabetic').
    void text(float x, float baseline, const char* s, uint32_t color, const lv_font_t* font,
              lv_text_align_t align = LV_TEXT_ALIGN_LEFT) const;
    void circle(float cx, float cy, float r, uint32_t fill, uint32_t border, int border_w) const;
    void line(float x1, float y1, float x2, float y2, int width, uint32_t color) const;
};

/// Ancho en px de @p s con @p font.
int text_width(const char* s, const lv_font_t* font);

/// Crea un objeto sin estilo de @p w×@p h que se dibuja con @p cb (LV_EVENT_DRAW_MAIN).
lv_obj_t* draw_area(lv_obj_t* parent, int x, int y, int w, int h, lv_event_cb_t cb, void* user);
