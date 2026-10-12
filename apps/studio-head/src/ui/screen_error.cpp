#include "ui/screen_error.h"

#include "studio_tick.h"
#include "ui/input.h"
#include "ui/theme.h"

using namespace gf;

lv_obj_t* ScreenError::create() {
    _scr = screen_root();
    _top.create(_scr, "NO AUDIO ENGINE", -1);

    rect(_scr, 64, 120, 8, 160, RED);

    lv_obj_t* col = lv_obj_create(_scr);
    lv_obj_remove_style_all(col);
    lv_obj_clear_flag(col, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_pos(col, 96, 120);
    lv_obj_set_size(col, 640, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(col, 16, 0);

    lv_obj_t* err = label(col, T12B, RED, "ERROR");
    lv_obj_set_style_text_letter_space(err, 2, 0);
    lv_obj_t* title = label(col, T40, WHITE, "AUDIO ENGINE NOT RESPONDING");
    lv_obj_set_width(title, 640);
    lv_label_set_long_mode(title, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_line_space(title, 48 - T40->line_height, 0);

    lv_obj_t* rec = lv_obj_create(col);
    lv_obj_remove_style_all(rec);
    lv_obj_set_size(rec, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(rec, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(rec, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(rec, 12, 0);
    label(rec, T16R, GRAY, "RECONNECTING");
    lv_obj_t* dots = lv_obj_create(rec);
    lv_obj_remove_style_all(dots);
    lv_obj_set_size(dots, 3 * 8 + 2 * 4, 8);
    for (int i = 0; i < 3; ++i) _dots[i] = rect(dots, i * 12, 0, 8, 8, LINE);

    lv_obj_t* retry = box(_scr, 96, 296, 280, 48, BG, GRAY);
    lv_obj_set_style_pad_hor(retry, 15, 0);
    lv_obj_t* np = label(retry, T12B, GRAY, "NAV PUSH");
    lv_obj_set_style_text_letter_space(np, 1, 0);
    lv_obj_align(np, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_t* rn = label(retry, T16, WHITE, "RETRY NOW");
    lv_obj_align(rn, LV_ALIGN_RIGHT_MID, 0, 0);
    input_attach_tap(retry, Ctl::NavPush);

    lv_obj_t* foot = rect(_scr, 0, 384, 800, 96, BG);
    lv_obj_set_style_border_side(foot, LV_BORDER_SIDE_TOP, 0);
    lv_obj_set_style_border_width(foot, 1, 0);
    lv_obj_set_style_border_color(foot, lv_color_hex(LINE), 0);
    lv_obj_t* ft = label(foot, T12, MON, "CONTROLS PAUSED UNTIL THE ENGINE RESPONDS · VOL STAYS ACTIVE");
    lv_obj_set_style_text_letter_space(ft, 1, 0);
    lv_obj_center(ft);
    return _scr;
}

void ScreenError::update(const StudioState& s) {
    _top.update(s);
    const int dot = int(studio_tick_ms() / 500 % 3);  // 2 Hz
    if (dot == _last_dot) return;
    _last_dot = dot;
    for (int i = 0; i < 3; ++i) set_bg(_dots[i], i == dot ? GRAY : LINE);
}
