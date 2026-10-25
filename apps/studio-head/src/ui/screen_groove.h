#pragma once

#include "ui/bottom_bar.h"
#include "ui/edit_cells.h"
#include "ui/painter.h"
#include "ui/screen.h"
#include "ui/top_bar.h"

/**
 * @brief GROOVE (SHIFT + B3): reloj común, PUMP, ARP, CHORD, UNISON y macros (variants/studio/theory/09 §7).
 *
 * Fila de paradas (NAV) con su encendido, los slots de la parada como barras (la capa de SHIFT resaltada si está
 * apretado) y el BPM del reloj. Las celdas y la barra inferior muestran los 4 slots de la capa visible, igual que
 * las paradas de MOD.
 */
class ScreenGroove : public Screen {
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
    int _stop = 0;
};
