#pragma once

#include "ui/screen.h"
#include "ui/top_bar.h"

/**
 * @brief CONNECTION ERROR (UI v3, 08d): reemplaza cualquier pantalla cuando se pierde el enlace
 *        con el motor de audio. Solo NAV push (reintentar) y VOL siguen activos.
 */
class ScreenError : public Screen {
public:
    lv_obj_t* create() override;
    void update(const StudioState& s) override;

private:
    lv_obj_t* _scr = nullptr;
    TopBar _top;
    lv_obj_t* _dots[3] = {};
    int _last_dot = -1;
};
