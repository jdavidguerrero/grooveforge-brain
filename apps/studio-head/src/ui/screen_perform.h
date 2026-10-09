#pragma once

#include "lvgl.h"
#include "state/studio_state.h"
#include "ui/bottom_bar.h"
#include "ui/top_bar.h"

/**
 * @brief Pantalla PERFORM (UI v3, 01): card del engine, diales de cutoff y resonance, franja de
 *        espectro con la curva y la línea del cutoff, chips de AI y barra inferior.
 *
 * Geometría tomada de claude-design-2026-10-09/Perform v3.dc.html.
 */
class ScreenPerform {
public:
    lv_obj_t* create();
    void update(const StudioState& s);

private:
    static constexpr int TICKS = 31;
    struct Dial {
        lv_obj_t* root;
        lv_obj_t* ticks[TICKS];
        lv_point_t pts[TICKS][2];
        lv_obj_t* value;
        lv_obj_t* unit;
        int last_idx;
        char last_val[12];
    };
    struct Chip { lv_obj_t *box, *a, *b, *c; char last[32]; };

    void build_dial(Dial& d, int x, const char* caption, const char* unit);
    void update_dial(Dial& d, float frac, const char* val, uint32_t on_color);

    lv_obj_t* _scr = nullptr;
    TopBar _top;
    BottomBar _bottom;

    lv_obj_t* _engine_name = nullptr;
    lv_obj_t* _engine_tag = nullptr;
    lv_obj_t* _preset = nullptr;
    int _last_engine = -1;

    Dial _cut = {};
    Dial _res = {};

    static constexpr int BANDS = 32;
    static constexpr int CURVE_PTS = 97;
    lv_obj_t* _bars[BANDS] = {};
    uint8_t _last_bar_h[BANDS] = {};
    lv_obj_t* _cut_line = nullptr;
    lv_obj_t* _cut_tag = nullptr;
    lv_obj_t* _curve = nullptr;
    lv_point_t _curve_pts[CURVE_PTS] = {};
    int _last_cut_x = -1;
    char _last_cut_tag[32] = {};

    Chip _key = {}, _chord = {}, _bpm = {}, _genre = {};
    lv_obj_t* _beats[4] = {};
    int _last_beat = -1;
};
