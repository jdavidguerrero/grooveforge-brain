#include "ui/theme.h"

#include "ui/screen.h"

LV_FONT_DECLARE(gf_mono_12_r)
LV_FONT_DECLARE(gf_mono_12_sb)
LV_FONT_DECLARE(gf_mono_16_r)
LV_FONT_DECLARE(gf_mono_16_sb)
LV_FONT_DECLARE(gf_mono_20_sb)
LV_FONT_DECLARE(gf_mono_28_sb)
LV_FONT_DECLARE(gf_mono_40_sb)
LV_FONT_DECLARE(gf_mono_64_sb)
LV_FONT_DECLARE(gf_mono_96_sb)

namespace gf {

const lv_font_t* const T12  = &gf_mono_12_r;
const lv_font_t* const T12B = &gf_mono_12_sb;
const lv_font_t* const T16  = &gf_mono_16_sb;
const lv_font_t* const T16R = &gf_mono_16_r;
const lv_font_t* const T20  = &gf_mono_20_sb;
const lv_font_t* const T28  = &gf_mono_28_sb;
const lv_font_t* const T40  = &gf_mono_40_sb;
const lv_font_t* const T64  = &gf_mono_64_sb;
const lv_font_t* const T96  = &gf_mono_96_sb;

uint32_t camelot(uint8_t n) {
    // studio-data.js: CAM (indexado por número Camelot)
    static const uint32_t CAM[13] = {
        0x000000,
        0xB61F32, 0xDD4D28, 0xDD902C, 0xDDB42A, 0xB0CC30, 0x74C443,
        0x2EBB87, 0x1FB6BB, 0x1D86C4, 0x3358C0, 0x5540B8, 0x8A2DB5,
    };
    return (n >= 1 && n <= 12) ? CAM[n] : MON;
}

ModeColors mode_colors(bool fx) {
    return fx ? ModeColors{PURPLE, PURPLE_BRIGHT, PURPLE_DIM} : ModeColors{TEAL, TEAL_PLUS, TEAL_DIM};
}

static void strip(lv_obj_t* o) {
    lv_obj_remove_style_all(o);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
}

lv_obj_t* rect(lv_obj_t* parent, int x, int y, int w, int h, uint32_t bg) {
    lv_obj_t* o = lv_obj_create(parent);
    strip(o);
    lv_obj_set_pos(o, x, y);
    lv_obj_set_size(o, w, h);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(o, lv_color_hex(bg), 0);
    return o;
}

lv_obj_t* box(lv_obj_t* parent, int x, int y, int w, int h, uint32_t bg, uint32_t border) {
    lv_obj_t* o = rect(parent, x, y, w, h, bg);
    lv_obj_set_style_border_width(o, 1, 0);
    lv_obj_set_style_border_color(o, lv_color_hex(border), 0);
    return o;
}

lv_obj_t* label(lv_obj_t* parent, const lv_font_t* font, uint32_t color, const char* text) {
    lv_obj_t* l = lv_label_create(parent);
    lv_obj_set_style_text_font(l, font, 0);
    lv_obj_set_style_text_color(l, lv_color_hex(color), 0);
    lv_label_set_text(l, text);
    return l;
}

void set_bg(lv_obj_t* obj, uint32_t bg) {
    lv_obj_set_style_bg_color(obj, lv_color_hex(bg), 0);
}

void set_text_color(lv_obj_t* obj, uint32_t color) {
    lv_obj_set_style_text_color(obj, lv_color_hex(color), 0);
}

}  // namespace gf

lv_obj_t* screen_root() {
    lv_obj_t* scr = lv_obj_create(nullptr);
    lv_obj_remove_style_all(scr);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(scr, lv_color_hex(gf::BG), 0);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    return scr;
}
