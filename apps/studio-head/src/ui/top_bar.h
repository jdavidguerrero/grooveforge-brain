#pragma once

#include "lvgl.h"
#include "state/studio_state.h"

/**
 * @brief Barra superior de 800×40 (UI v3, componente TopBar): modo, página, indicador de vista,
 *        bypass, tonalidad, BPM, estado del enlace y volumen.
 */
class TopBar {
public:
    /// @param page_index posición en NAV (PERFORM 0, AI 1, SPECTRUM 2); -1 sin puntos.
    void create(lv_obj_t* parent, const char* page, int page_index);
    void update(const StudioState& s);
    void set_page(const char* page);

private:
    lv_obj_t* _bar = nullptr;
    lv_obj_t* _mode = nullptr;
    lv_obj_t* _mode_txt = nullptr;
    lv_obj_t* _title = nullptr;
    lv_obj_t* _dots[2] = {};   ///< una por vista de la home (PERFORM · PANEL, theory/08)
    lv_obj_t* _bypass = nullptr;
    lv_obj_t* _key_sq = nullptr;
    lv_obj_t* _key = nullptr;
    lv_obj_t* _bpm = nullptr;
    lv_obj_t* _link_sq = nullptr;
    lv_obj_t* _link = nullptr;
    lv_obj_t* _vol = nullptr;
    int _page_index = -1;

    // Último valor dibujado: solo se toca LVGL cuando algo cambia (theory doc §3.2).
    int _last_mode = -1, _last_key = -1, _last_bpm10 = -1, _last_link = -1, _last_vol10 = 9999;
    int _last_bypass = -1;
};
