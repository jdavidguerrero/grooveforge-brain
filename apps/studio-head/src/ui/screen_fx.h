#pragma once

#include "ui/bottom_bar.h"
#include "ui/fx_viz.h"
#include "ui/screen.h"
#include "ui/top_bar.h"

/**
 * @brief FX MAIN con visualización (diseño FX Viz, reemplaza 04a–04c de la v3).
 *
 * Medidor IN a la izquierda, cabecera con slot y estado, 3 pestañas (ONDA · ESPECTRO · FIRMA) y
 * la gráfica de 728×256. La lista de los 12 FX aparece sobre la gráfica al girar NAV y se oculta
 * 1.5 s después del último paso. Los cuatro valores (DRY/WET + 3 parámetros) van en la barra
 * inferior, en los controles que los mueven.
 */
class ScreenFx : public Screen {
public:
    lv_obj_t* create() override;
    void update(const StudioState& s) override;

    /// NAV giró: muestra la lista de FX durante 1.5 s.
    void show_list();

private:
    struct Row { lv_obj_t *box, *idx, *name, *dev, *sq; };

    static void on_graph_draw(lv_event_t* e);
    static void on_meter_draw(lv_event_t* e);
    void paint_rows(const StudioState& s);
    void update_meter(const StudioState& s, uint32_t now);

    lv_obj_t* _scr = nullptr;
    TopBar _top;
    BottomBar _bottom;

    lv_obj_t* _meter = nullptr;
    lv_obj_t* _graph = nullptr;
    lv_obj_t *_slot_line = nullptr, *_name = nullptr;
    lv_obj_t *_pill_hold = nullptr, *_pill = nullptr, *_pill_txt = nullptr;
    lv_obj_t *_tab[3] = {}, *_tab_bar[3] = {}, *_tab_lbl[3] = {};
    lv_obj_t* _list = nullptr;
    Row _rows[FX_SLOTS] = {};

    // Lo que leen los callbacks de dibujo (se llaman desde lv_timer_handler, después de update).
    const StudioState* _s = nullptr;
    VizState _st = VizState::Main;
    uint32_t _now = 0;

    MeterView _m = {};
    float _peak[2] = {};
    uint32_t _clip_until[2] = {};
    uint32_t _last_ms = 0;
    uint32_t _list_until = 0;
    bool _list_shown = false;

    int _last_slot = -1, _last_on = -1, _last_view = -1;
};
