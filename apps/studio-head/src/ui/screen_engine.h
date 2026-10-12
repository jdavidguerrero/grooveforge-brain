#pragma once

#include "ui/bottom_bar.h"
#include "ui/engines.h"
#include "ui/screen.h"
#include "ui/top_bar.h"

/**
 * @brief ENGINE SELECT (UI v3, 02): card grande del engine bajo el cursor y grilla de 6 tiles.
 *
 * NAV mueve el cursor (fila por fila, da la vuelta) y NAV push carga el engine; también se carga
 * tocando una tile. B4 vuelve a PERFORM. P1/P2 no tienen función.
 */
class ScreenEngine : public Screen {
public:
    lv_obj_t* create() override;
    void update(const StudioState& s) override;

    void move_cursor(int dir);
    void set_cursor(int i);
    int cursor() const { return _cursor; }

private:
    struct Tile { lv_obj_t *box, *art, *name, *active, *tag; };

    lv_obj_t* _scr = nullptr;
    TopBar _top;
    BottomBar _bottom;
    lv_obj_t *_focus_art = nullptr, *_name = nullptr, *_tag = nullptr, *_arch = nullptr;
    Tile _tiles[ENGINE_COUNT] = {};

    int _cursor = 0;
    int _last_cursor = -1, _last_active = -1;
};
