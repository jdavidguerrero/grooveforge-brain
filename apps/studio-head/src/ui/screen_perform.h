#pragma once

#include "ui/bottom_bar.h"
#include "ui/screen.h"
#include "ui/top_bar.h"

/**
 * @brief PERFORM (theory/08 de Studio): única vista de la home junto con PANEL.
 *
 * Card del engine compacta, CUTOFF y RES con barra, y a la derecha una gráfica animada: arriba la onda del
 * oscilador principal pasada por un filtro de 4 polos al cutoff / resonancia actuales; abajo la respuesta del
 * filtro. Se mueven con LFO → CUTOFF y LFO → PWM. Todo se calcula en la cabeza con los PARAM_CHANGED que ya llegan.
 */
class ScreenPerform : public Screen {
public:
    lv_obj_t* create() override;
    void update(const StudioState& s) override;

private:
    struct Readout { lv_obj_t *box, *value, *unit, *bar; char last[16]; int last_w; };

    void build_readout(Readout& r, int y, const char* caption, const char* unit);
    void set_readout(Readout& r, const char* val, float frac, uint32_t color);
    static void on_graph_draw(lv_event_t* e);
    void draw(const struct Painter& p, const StudioState& s) const;

    lv_obj_t* _scr = nullptr;
    TopBar _top;
    BottomBar _bottom;

    lv_obj_t* _art = nullptr;
    lv_obj_t* _engine_name = nullptr;
    lv_obj_t* _engine_tag = nullptr;
    lv_obj_t* _preset = nullptr;
    int _last_engine = -1;

    Readout _cut = {}, _res = {};

    lv_obj_t* _graph = nullptr;
    const StudioState* _s = nullptr;
    uint32_t _last_draw_ms = 0;
    float _scroll = 0.0f;                 ///< fase de la onda dibujada (corre despacio)
};
