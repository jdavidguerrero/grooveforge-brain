#pragma once

#include "lvgl.h"
#include "state/studio_state.h"

/**
 * @brief Interfaz común de las pantallas: se crean una vez y la UI solo actualiza la activa.
 */
class Screen {
public:
    virtual ~Screen() = default;
    virtual lv_obj_t* create() = 0;
    virtual void update(const StudioState& s) = 0;
};

/// Pantalla raíz sin estilos, fondo antracita y sin scroll.
lv_obj_t* screen_root();
