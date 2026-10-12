#pragma once

#include <cstdint>

#include "lvgl.h"
#include "ui/engines.h"

/**
 * @brief Splash de arranque animado (diseño v3, 08g, "Boot Splash"): línea base, 32 barras con un
 *        barrido resonante, la marca tipeada, los 6 engines que se encienden uno por uno, la barra de
 *        carga y READY. Termina con un fundido a BG y la app pasa a PERFORM.
 *
 * En el demo la línea de tiempo es fija (6.1 s). Con el motor conectado, el avance tiene que seguir
 * a boot.step (link · engine n/6 · modelos AI · rack de FX · listo), no rellenar tiempo.
 */
class BootSplash {
public:
    static constexpr uint32_t DURATION_MS = 6100;

    lv_obj_t* create();
    /// @param ms tiempo desde que arrancó el splash.
    void update(uint32_t ms);

private:
    struct Thumb { lv_obj_t *art, *veil, *bar, *name; };

    lv_obj_t* _scr = nullptr;
    lv_obj_t* _line = nullptr;
    lv_obj_t* _bars[32] = {};
    lv_obj_t* _letters[11] = {};
    lv_obj_t* _cursor = nullptr;
    lv_obj_t* _sub = nullptr;
    lv_obj_t* _row = nullptr;
    Thumb _thumbs[ENGINE_COUNT] = {};
    lv_obj_t* _segs[32] = {};
    lv_obj_t* _status = nullptr;
    lv_obj_t* _pct = nullptr;
    lv_obj_t* _fade = nullptr;

    // Último estado dibujado de cada elemento animado (solo se toca LVGL si cambia).
    int16_t _bar_h[32] = {};
    uint32_t _bar_c[32] = {};
    uint32_t _seg_c[32] = {};
    int _line_w = -1, _letters_n = -1, _cursor_at = -2, _sub_opa = -1, _fade_opa = -1;
    int _thumb_state[ENGINE_COUNT] = {};
    char _last_status[48] = {};
    char _last_pct[16] = {};
};
