#pragma once

#include <cstdint>

#include "state/osc_catalog.h"
#include "ui/painter.h"

/**
 * @file edit_draw.h
 * @brief Primitivas de las páginas de edición OSC y LFO (diseño v3, 11–12): trazas de 2 px por
 *        columnas, íconos de onda de 24×16, cajas con marco y texto posicionado por su caja de
 *        línea (como el CSS del diseño), todo con rects sólidos.
 */

/// Valor de la forma @p w en la fase @p p (en ciclos). S&H usa una secuencia fija de 8 escalones.
float wave_value(Wave w, float p);

/// Traza de @p cyc ciclos en @p w px: columnas de @p st px y trazo de @p th px (diseño: trace()).
void draw_trace(const Painter& p, Wave w, int x0, int cy, int width, int amp, float cyc, int off,
                uint32_t color, int th, int st);

/// Ícono de onda de 24×16 con la esquina en (x, y).
void draw_wave_icon(const Painter& p, Wave w, int x, int y, uint32_t color);

/// Caja con marco de @p b px: fondo @p fc, marco @p bc.
void draw_box(const Painter& p, int x, int y, int w, int h, uint32_t bc, uint32_t fc, int b);

/// Texto con la caja de línea arriba en @p y_top (line-height = tamaño de fuente, como el diseño).
void text_top(const Painter& p, int x, int y_top, const char* s, uint32_t color, const lv_font_t* font,
              lv_text_align_t align = LV_TEXT_ALIGN_LEFT);
