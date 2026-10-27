#pragma once

#include "ui/bottom_bar.h"
#include "ui/edit_cells.h"
#include "ui/painter.h"
#include "ui/screen.h"
#include "ui/top_bar.h"

/**
 * @brief Home de HYBRID SYNTH (tercer modo de MODE, variants/studio/theory/12 §3d).
 *
 * Presets de capa (NAV elige, push carga), las tarjetas A y B con su engine y preset (la capa en foco resaltada;
 * tocar una tarjeta cambia el foco), la barra de MORPH y el retraso / transposición de B.
 */
class ScreenHybrid : public Screen {
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
    lv_obj_t* _tap_a = nullptr;
    lv_obj_t* _tap_b = nullptr;

    const StudioState* _s = nullptr;
    char _last_key[192] = {};
};
