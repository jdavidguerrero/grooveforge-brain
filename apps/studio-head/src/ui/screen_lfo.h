#pragma once

#include "ui/bottom_bar.h"
#include "ui/edit_cells.h"
#include "ui/painter.h"
#include "ui/screen.h"
#include "ui/top_bar.h"

/**
 * @brief LFO / MOD: un LFO y una cantidad por destino (theory/lfo-modulacion.md §8).
 *
 * Fichas de forma (NAV), lectura de RATE (libre en Hz o sincronizado al BPM), 2 ciclos de la forma
 * (8 escalones en S&H) y tres medidores: CUTOFF, PITCH y el tercer destino del engine (PWM o AMP), cada
 * uno con su banda ±cantidad. Un destino que el engine no tiene va en gris. Solo el punto de fase y las
 * marcas de los medidores se mueven a 30 Hz: se invalida su zona, no la gráfica entera.
 */
class ScreenLfo : public Screen {
public:
    lv_obj_t* create() override;
    void update(const StudioState& s) override;

private:
    static void on_graph_draw(lv_event_t* e);
    void draw(const Painter& p, const StudioState& s) const;
    void draw_mod(const Painter& p, const StudioState& s) const;
    void update_mod(const StudioState& s);
    void invalidate_local(int x, int y, int w, int h);

    lv_obj_t* _scr = nullptr;
    TopBar _top;
    BottomBar _bottom;
    EditCells _cells;
    lv_obj_t* _graph = nullptr;

    const StudioState* _s = nullptr;
    char _last_key[160] = {};
    int _stop = 0;                ///< 0 = LFO; 1… = paradas de MOD del engine (POLY-MOD, WHEEL-MOD)
    int _last_stop = -1;
    int _dot_x = -100, _dot_y = 0;
    int _mark_y[3] = {};
    int _last_engine = -1;
};
