#pragma once

#include "ui/bottom_bar.h"
#include "ui/screen.h"
#include "ui/top_bar.h"

/**
 * @brief Página ENV (UI v3, 03a/03b): gráfica ADSR y 4 celdas de valor. P1/P2 editan ATTACK y
 *        DECAY; con SHIFT, SUSTAIN y RELEASE.
 *
 * La gráfica se dibuja en un lv_canvas y solo se regenera cuando cambian los valores o SHIFT
 * (el contenido es estático entre ediciones; theory doc §5.1).
 */
class ScreenEnv : public Screen {
public:
    lv_obj_t* create() override;
    void update(const StudioState& s) override;

private:
    struct Cell { lv_obj_t *box, *top, *lbl, *tag_box, *tag, *val, *unit; };

    void draw_graph(const StudioState& s);
    void paint_cell(Cell& c, bool on, bool nav, const char* val, const char* unit);

    lv_obj_t* _scr = nullptr;
    TopBar _top;
    BottomBar _bottom;
    lv_obj_t* _canvas = nullptr;
    lv_obj_t* _seg_lbl[4] = {};
    Cell _cells[4] = {};

    char _last_key[64] = {};
    int _last_engine = -1;
};
