#pragma once

#include "ui/bottom_bar.h"
#include "ui/edit_cells.h"
#include "ui/painter.h"
#include "ui/screen.h"
#include "ui/top_bar.h"

/**
 * @brief SYNTH FX (B4): la cadena fija de 4 FX del synth (variants/studio/theory/05-studio-synth-fx.md).
 *
 * Cuatro tarjetas en fila con flechas (BIT → TAPE → CHORUS → MODAL): nombre, ON/OFF, el parámetro principal
 * y la barra de MIX. NAV elige la tarjeta (borde TEAL_PLUS) y NAV push la enciende. Las celdas muestran MIX y
 * los tres parámetros de la tarjeta elegida (P1 / P2, SHIFT P1 / P2).
 */
class ScreenSfx : public Screen {
public:
    lv_obj_t* create() override;
    void update(const StudioState& s) override;

private:
    static void on_graph_draw(lv_event_t* e);
    void draw(const Painter& p, const StudioState& s) const;

    lv_obj_t* _scr = nullptr;
    TopBar _top;
    BottomBar _bottom;
    EditCells _cells;
    lv_obj_t* _graph = nullptr;
    const StudioState* _s = nullptr;
    char _last_key[160] = {};
    int _last_engine = -1;
};

/// Texto de un parámetro de la cadena (k = 0 principal, 1–2 de usuario) para celdas y barra inferior.
void sfx_format(int fx, int k, float norm, char* val, size_t n, const char** label, const char** unit);
