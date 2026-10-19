#include "ui/edit_cells.h"

#include <cstdio>
#include <cstring>

#include "ui/edit_draw.h"
#include "ui/theme.h"

using namespace gf;

namespace {

const char* const TAGS[4] = {"P1", "P2", "ENC L", "ENC R"};   // los 4 encoders de la página (theory/04 §3.1)
constexpr uint32_t UNUSED_FRAME = 0x1C1C1C;

}  // namespace

void EditCells::create(lv_obj_t* parent) {
    for (int i = 0; i < 4; ++i) {
        Cell& c = _c[i];
        c.box = rect(parent, 16 + i * 194, 264, 186, 112, SURFACE);
        lv_obj_set_style_outline_width(c.box, 2, 0);
        lv_obj_set_style_outline_pad(c.box, -2, 0);
        lv_obj_set_style_outline_opa(c.box, LV_OPA_TRANSP, 0);
        c.top = rect(c.box, 0, 0, 186, 4, LINE);
        c.lbl = label(c.box, T12B, MON, "");
        lv_obj_set_style_text_letter_space(c.lbl, 1, 0);
        lv_obj_set_pos(c.lbl, 16, 18);
        c.tag_box = lv_obj_create(c.box);
        lv_obj_remove_style_all(c.tag_box);
        lv_obj_set_size(c.tag_box, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
        lv_obj_set_style_pad_hor(c.tag_box, 6, 0);
        lv_obj_set_style_pad_ver(c.tag_box, 2, 0);
        lv_obj_set_style_border_width(c.tag_box, 1, 0);
        lv_obj_set_style_bg_opa(c.tag_box, LV_OPA_COVER, 0);
        c.tag = label(c.tag_box, T12B, MON, TAGS[i]);
        lv_obj_align(c.tag_box, LV_ALIGN_TOP_RIGHT, -16, 16);
        c.icon = draw_area(c.box, 16, 52, 24, 16, on_icon_draw, &c);
        c.val = label(c.box, T40, GRAY, "");
        lv_obj_set_pos(c.val, 16, 50);
        c.unit = label(c.box, T12, MON, "");
        c.wave = Wave::Noise;
        c.last[0] = '\0';
    }
}

void EditCells::on_icon_draw(lv_event_t* e) {
    const Cell* c = static_cast<const Cell*>(lv_event_get_user_data(e));
    if (c->wave == Wave::Noise) return;
    draw_wave_icon(Painter::from(e), c->wave, 0, 0, c->on ? TEAL_PLUS : GRAY);
}

void EditCells::set(int i, bool on, const char* lbl, const char* val, const char* unit, Wave icon) {
    Cell& c = _c[i];
    char key[64];
    std::snprintf(key, sizeof(key), "%d|%s|%s|%s|%d", on, lbl, val, unit, int(icon));
    if (std::strcmp(key, c.last) == 0) return;
    std::snprintf(c.last, sizeof(c.last), "%s", key);
    c.on = on;
    c.wave = icon;

    set_bg(c.box, on ? TEAL_DIM : SURFACE);
    set_bg(c.top, on ? TEAL_PLUS : LINE);
    lv_obj_set_style_outline_opa(c.box, LV_OPA_TRANSP, 0);
    lv_label_set_text(c.lbl, lbl);
    set_text_color(c.lbl, on ? WHITE : MON);
    lv_obj_set_style_bg_opa(c.tag_box, on ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
    set_bg(c.tag_box, TEAL_PLUS);
    lv_obj_set_style_border_color(c.tag_box, lv_color_hex(on ? TEAL_PLUS : MON), 0);
    set_text_color(c.tag, on ? BG : MON);
    lv_label_set_text(c.val, val);
    set_text_color(c.val, on ? WHITE : GRAY);
    lv_label_set_text(c.unit, unit);
    set_text_color(c.unit, on ? WHITE : MON);

    const bool has_icon = icon != Wave::Noise;
    if (has_icon) lv_obj_clear_flag(c.icon, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(c.icon, LV_OBJ_FLAG_HIDDEN);
    lv_obj_invalidate(c.icon);
    lv_obj_set_x(c.val, has_icon ? 16 + 24 + 8 : 16);
    lv_obj_align_to(c.unit, c.val, LV_ALIGN_OUT_RIGHT_BOTTOM, 8, -6);
}

void EditCells::set_unused(int i) {
    Cell& c = _c[i];
    if (std::strcmp(c.last, "unused") == 0) return;
    std::snprintf(c.last, sizeof(c.last), "unused");
    c.on = false;
    c.wave = Wave::Noise;
    set_bg(c.box, BG);
    set_bg(c.top, UNUSED_FRAME);
    lv_obj_set_style_outline_color(c.box, lv_color_hex(UNUSED_FRAME), 0);
    lv_obj_set_style_outline_opa(c.box, LV_OPA_COVER, 0);
    lv_label_set_text(c.lbl, "NOT USED");
    set_text_color(c.lbl, MON);
    lv_obj_set_style_bg_opa(c.tag_box, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_color(c.tag_box, lv_color_hex(LINE), 0);
    set_text_color(c.tag, MON);
    lv_label_set_text(c.val, "—");
    set_text_color(c.val, MON);
    lv_label_set_text(c.unit, "");
    lv_obj_add_flag(c.icon, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_x(c.val, 16);
}
