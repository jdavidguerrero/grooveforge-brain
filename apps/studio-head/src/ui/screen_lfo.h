#pragma once

#include "ui/bottom_bar.h"
#include "ui/edit_cells.h"
#include "ui/painter.h"
#include "ui/screen.h"
#include "ui/top_bar.h"

/**
 * @brief LFO / MOD (diseño v3, 12a / 12b): especificación visual del LFO que todavía no existe en el
 *        firmware.
 *
 * Fichas de forma y destino, lectura de RATE (libre en Hz o sincronizado al BPM), 2 ciclos de la
 * forma (8 escalones en S&H) y el medidor del destino con la banda ±DEPTH. Solo el punto de fase y
 * la marca del medidor se mueven a 30 Hz: se invalida el rect viejo y el nuevo, no la gráfica entera.
 */
class ScreenLfo : public Screen {
public:
    lv_obj_t* create() override;
    void update(const StudioState& s) override;

private:
    struct Meter { int by, bd; char hi[16], base[16], lo[16], dv[16]; };

    static void on_graph_draw(lv_event_t* e);
    void draw(const Painter& p, const StudioState& s) const;
    Meter meter(const StudioState& s) const;
    void invalidate_local(int x, int y, int w, int h);

    lv_obj_t* _scr = nullptr;
    TopBar _top;
    BottomBar _bottom;
    EditCells _cells;
    lv_obj_t* _graph = nullptr;

    const StudioState* _s = nullptr;
    char _last_key[96] = {};
    int _dot_x = -100, _dot_y = 0, _mark_y = 0;
    int _last_engine = -1;
    int _last_cut = -1;
};
