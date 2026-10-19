#pragma once

#include "ui/bottom_bar.h"
#include "ui/edit_cells.h"
#include "ui/painter.h"
#include "ui/screen.h"
#include "ui/top_bar.h"

/**
 * @brief OSC (diseño v3, 11a–11e y 11c2): plantilla alimentada por la tabla de osciladores de cada
 *        engine (state/osc_catalog.h).
 *
 * Arriba, la fila de selección y una pista por fuente (forma de onda, corrimiento por detune o
 * intervalo y nivel). En el DX7, el diagrama del algoritmo con los 4 operadores; con SHIFT, el
 * selector de los 8 algoritmos. NAV elige el oscilador u operador que editan P1/P2.
 */
class ScreenOsc : public Screen {
public:
    lv_obj_t* create() override;
    void update(const StudioState& s) override;

    /// NAV giró: siguiente oscilador u operador del engine activo (sin efecto con uno solo).
    void move_selection(int dir, const StudioState& s);
    void set_selection(uint8_t engine, uint8_t sel) { if (engine < 6) _sel[engine] = sel; }

private:
    static void on_graph_draw(lv_event_t* e);
    void draw_template(const Painter& p, const StudioState& s) const;
    void draw_fm(const Painter& p, const StudioState& s) const;

    lv_obj_t* _scr = nullptr;
    TopBar _top;
    BottomBar _bottom;
    EditCells _cells;
    lv_obj_t* _graph = nullptr;

    const StudioState* _s = nullptr;
    uint8_t _sel[6] = {1, 0, 1, 0, 1, 1};  ///< selección por engine (los ejemplos del diseño)
    char _last_key[400] = {};
    int _last_engine = -1;
};
