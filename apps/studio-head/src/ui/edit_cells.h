#pragma once

#include "lvgl.h"
#include "state/osc_catalog.h"

/**
 * @brief Las 4 celdas de valor de las páginas de edición (ENV / OSC / LFO, diseño v3):
 *        16,264 · 4 × 186×112, separación 8, tags P1 · P2 · SHIFT P1 · SHIFT P2.
 *
 * El par activo (P1/P2 sin SHIFT, o SHIFT P1/P2) va en TEAL_DIM con borde superior TEAL_PLUS. Una
 * celda puede llevar el ícono de la forma de onda o quedar NOT USED. Solo toca LVGL si algo cambió.
 */
class EditCells {
public:
    void create(lv_obj_t* parent);

    /// @param icon  forma de onda a la izquierda del valor; Wave::Noise = sin ícono.
    void set(int i, bool on, const char* lbl, const char* val, const char* unit, Wave icon = Wave::Noise);
    void set_unused(int i);

private:
    struct Cell {
        lv_obj_t *box, *top, *lbl, *tag_box, *tag, *val, *unit, *icon;
        char last[64];
        Wave wave;
        bool on;
    };
    static void on_icon_draw(lv_event_t* e);

    Cell _c[4] = {};
};
