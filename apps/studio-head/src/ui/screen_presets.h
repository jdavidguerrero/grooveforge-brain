#pragma once

#include "ui/bottom_bar.h"
#include "ui/screen.h"
#include "ui/top_bar.h"

/**
 * @brief PRESETS (UI v3, 07): navegador del banco del engine activo.
 *
 * La fila 0 es "+ SAVE CURRENT AS NEW"; las siguientes, los presets A-01… NAV mueve el cursor,
 * NAV push carga (o guarda, en la fila 0). Se ven 6 filas a la vez, con barra de scroll y el
 * detalle del preset bajo el cursor a la derecha. B4 abre ENGINE SELECT.
 */
class ScreenPresets : public Screen {
public:
    lv_obj_t* create() override;
    void update(const StudioState& s) override;

    void move_cursor(int dir, const StudioState& s);
    /// Tap en la fila visible @p i: la primera vez mueve el cursor; devuelve true si ya estaba ahí
    /// (segundo tap = cargar).
    bool tap_row(int i, const StudioState& s);
    static constexpr int ROWS_VISIBLE = 6;
    /// Fila bajo el cursor: 0 = guardar, n ≥ 1 = preset n − 1.
    int cursor() const { return _cursor; }
    void set_cursor(int c) { _cursor = c; }

private:
    static constexpr int VISIBLE = ROWS_VISIBLE;
    struct Row { lv_obj_t *box, *slot, *name, *loaded; };

    lv_obj_t* _scr = nullptr;
    TopBar _top;
    BottomBar _bottom;
    lv_obj_t *_eng_name = nullptr, *_eng_tag = nullptr;
    Row _rows[VISIBLE] = {};
    lv_obj_t* _thumb = nullptr;
    lv_obj_t *_d_art = nullptr, *_d_slot = nullptr, *_d_name = nullptr, *_d_info = nullptr, *_d_action = nullptr;

    int _cursor = 7;     ///< A-07 + 1 por la fila de guardar: arranca sobre el cargado
    int _top_row = 0;
    char _last_key[48] = {};
};
