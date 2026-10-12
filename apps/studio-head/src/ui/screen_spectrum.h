#pragma once

#include "ui/bottom_bar.h"
#include "ui/screen.h"
#include "ui/top_bar.h"

/**
 * @brief SPECTRUM (UI v3, 06): analizador de 32 bandas a pantalla completa con peak hold, escala
 *        en dB y Hz, curva del filtro, línea del cutoff y lectura grande de FC y RES.
 *
 * Gráfica de 704×256 en (64, 48); eje vertical de +12 a −60 dB (studio-data.js: dbs, spBars).
 */
class ScreenSpectrum : public Screen {
public:
    lv_obj_t* create() override;
    void update(const StudioState& s) override;

private:
    static constexpr int BANDS = 32;
    static constexpr int CURVE_PTS = 89;

    lv_obj_t* _scr = nullptr;
    TopBar _top;
    BottomBar _bottom;

    lv_obj_t* _bars[BANDS] = {};
    lv_obj_t* _peaks[BANDS] = {};
    float _peak_h[BANDS] = {};
    int16_t _last_h[BANDS] = {};
    int16_t _last_pk[BANDS] = {};
    bool _last_lit[BANDS] = {};

    lv_obj_t* _curve = nullptr;
    lv_point_t _curve_pts[CURVE_PTS] = {};
    lv_obj_t* _cut_line = nullptr;
    lv_obj_t* _res_tag = nullptr;
    lv_obj_t* _fc_big = nullptr;
    lv_obj_t* _res_big = nullptr;
    int _last_cut_x = -1;
    int _last_res = -1;
    char _last_fc[16] = {};
};
