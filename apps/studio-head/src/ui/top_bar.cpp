#include "ui/top_bar.h"

#include <cmath>
#include <cstdio>
#include <cstring>

#include "ui/input.h"
#include "ui/theme.h"

using namespace gf;

namespace {

lv_obj_t* flex_row(lv_obj_t* parent, int gap) {
    lv_obj_t* o = lv_obj_create(parent);
    lv_obj_remove_style_all(o);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(o, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(o, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(o, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(o, gap, 0);
    return o;
}

}  // namespace

void TopBar::create(lv_obj_t* parent, const char* page, int page_index) {
    _page_index = page_index;
    _bar = rect(parent, 0, 0, SCREEN_W, TOPBAR_H, BG);
    lv_obj_set_style_border_side(_bar, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_width(_bar, 1, 0);
    lv_obj_set_style_pad_hor(_bar, 16, 0);
    lv_obj_set_flex_flow(_bar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(_bar, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(_bar, 16, 0);

    _mode = rect(_bar, 0, 0, 72, 24, TEAL);
    _mode_txt = label(_mode, T12B, BG, "SYNTH");
    lv_obj_set_style_text_letter_space(_mode_txt, 2, 0);
    lv_obj_center(_mode_txt);

    _title = label(_bar, T16, WHITE, page);
    lv_obj_set_style_text_letter_space(_title, 1, 0);

    lv_obj_t* dots = flex_row(_bar, 4);
    for (auto& d : _dots) {
        d = rect(dots, 0, 0, 8, 8, TEAL_DIM);
        if (_page_index < 0) lv_obj_add_flag(d, LV_OBJ_FLAG_HIDDEN);
    }

    lv_obj_t* spacer = rect(_bar, 0, 0, 1, 1, BG);
    lv_obj_set_flex_grow(spacer, 1);

    _bypass = rect(_bar, 0, 0, 96, 24, ORANGE);
    lv_obj_t* byp = label(_bypass, T12B, BG, "FILTER BYP");
    lv_obj_center(byp);
    lv_obj_add_flag(_bypass, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t* key = flex_row(_bar, 8);
    _key_sq = rect(key, 0, 0, 8, 8, camelot(8));
    _key = label(key, T12, GRAY, "8A");

    _bpm = label(_bar, T12, MON, "124.0 BPM");

    lv_obj_t* link = flex_row(_bar, 8);
    _link_sq = rect(link, 0, 0, 8, 8, MON);
    _link = label(link, T12, MON, "ENGINE OK");

    _vol = label(_bar, T12, MON, "VOL -6.0");

    input_attach_tap(_bar, Ctl::NavNext);  // atajo táctil: tocar la barra = girar NAV
}

void TopBar::set_page(const char* page) {
    if (std::strcmp(lv_label_get_text(_title), page) != 0) lv_label_set_text(_title, page);
}

void TopBar::update(const StudioState& s) {
    const int mode = int(s.mode);
    if (mode != _last_mode) {
        _last_mode = mode;
        const ModeColors mc = mode_colors(s.mode == Mode::Fx);
        set_bg(_mode, mc.mc);
        lv_label_set_text(_mode_txt, s.mode == Mode::Fx ? "FX" : "SYNTH");
        lv_obj_center(_mode_txt);
        lv_obj_set_style_border_color(_bar, lv_color_hex(mc.dim), 0);
        for (int i = 0; i < 3; ++i) set_bg(_dots[i], i == _page_index ? mc.hi : mc.dim);
    }

    if (int(s.filter_bypass) != _last_bypass) {
        _last_bypass = s.filter_bypass;
        if (s.filter_bypass) lv_obj_clear_flag(_bypass, LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(_bypass, LV_OBJ_FLAG_HIDDEN);
    }

    const int key = s.key_camelot * 2 + (s.key_minor ? 0 : 1);
    if (key != _last_key) {
        _last_key = key;
        set_bg(_key_sq, camelot(s.key_camelot));
        lv_label_set_text_fmt(_key, "%u%c", s.key_camelot, s.key_minor ? 'A' : 'B');
    }

    const int bpm10 = int(std::lround(s.bpm * 10.0f));
    if (bpm10 != _last_bpm10) {
        _last_bpm10 = bpm10;
        lv_label_set_text_fmt(_bpm, "%d.%d BPM", bpm10 / 10, bpm10 % 10);
    }

    if (int(s.link) != _last_link) {
        _last_link = int(s.link);
        uint32_t c = MON;
        const char* t = "ENGINE OK";
        if (s.link == LinkState::Lost) { c = RED; t = "ENGINE LOST"; }
        else if (s.link == LinkState::Demo) { c = AMBER; t = "DEMO"; }
        else if (s.link == LinkState::Connecting) { t = "CONNECTING"; }
        set_bg(_link_sq, c);
        set_text_color(_link, c);
        lv_label_set_text(_link, t);
    }

    const int vol10 = int(std::lround(s.volume_db * 10.0f));
    if (vol10 != _last_vol10) {
        _last_vol10 = vol10;
        char buf[16];
        std::snprintf(buf, sizeof(buf), "VOL %.1f", s.volume_db);
        lv_label_set_text(_vol, buf);
    }
}
