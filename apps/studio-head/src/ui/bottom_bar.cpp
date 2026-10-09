#include "ui/bottom_bar.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

#include "ui/theme.h"

using namespace gf;

namespace {

constexpr int ROW1_H = 63;   // 64 menos el borde superior de 1 px
constexpr int ROW2_Y = 64;
constexpr int ROW2_H = 32;
constexpr int ENC_BAR_W = 80;
constexpr int P_TRACK_W = 168;

lv_obj_t* group(lv_obj_t* parent, lv_flex_flow_t flow, int gap) {
    lv_obj_t* o = lv_obj_create(parent);
    lv_obj_remove_style_all(o);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(o, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(o, flow);
    lv_obj_set_flex_align(o, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(o, gap, 0);
    lv_obj_set_style_pad_row(o, gap, 0);
    return o;
}

lv_obj_t* cell(lv_obj_t* parent, int x, int y, int w, int h, bool border_left) {
    lv_obj_t* c = rect(parent, x, y, w, h, BG);
    if (border_left) {
        lv_obj_set_style_border_side(c, LV_BORDER_SIDE_LEFT, 0);
        lv_obj_set_style_border_width(c, 1, 0);
        lv_obj_set_style_border_color(c, lv_color_hex(LINE), 0);
    }
    return c;
}

int frac_px(float f, int w) { return int(std::lround(std::clamp(f, 0.0f, 1.0f) * w)); }

}  // namespace

void BottomBar::create(lv_obj_t* parent) {
    _bar = rect(parent, 0, BOTTOM_Y, SCREEN_W, BOTTOM_H, BG);
    lv_obj_set_style_border_side(_bar, LV_BORDER_SIDE_TOP, 0);
    lv_obj_set_style_border_width(_bar, 1, 0);

    // ── Fila 1 ───────────────────────────────────────────────────────────────
    build_enc(_enc[0], cell(_bar, 0, 0, 104, ROW1_H, false), false);
    for (int i = 0; i < 4; ++i) {
        Key& k = _keys[i];
        k.cell = cell(_bar, 104 + i * 148, 0, 148, ROW1_H, true);
        k.bar = rect(k.cell, 0, 0, 148, 4, BG);
        k.lbl = label(k.cell, T16, GRAY, "");
        lv_obj_set_style_text_letter_space(k.lbl, 1, 0);
        lv_obj_align(k.lbl, LV_ALIGN_TOP_MID, 0, 14);
        char b[4];
        std::snprintf(b, sizeof(b), "B%d", i + 1);
        lv_obj_t* n = label(k.cell, T12, MON, b);
        lv_obj_align(n, LV_ALIGN_TOP_MID, 0, 38);
    }
    build_enc(_enc[1], cell(_bar, 696, 0, 104, ROW1_H, true), true);

    // ── Fila 2 (solo display) ────────────────────────────────────────────────
    lv_obj_t* row2 = rect(_bar, 0, ROW2_Y - 1, SCREEN_W, ROW2_H, BG);
    lv_obj_set_style_border_side(row2, LV_BORDER_SIDE_TOP, 0);
    lv_obj_set_style_border_width(row2, 1, 0);
    lv_obj_set_style_border_color(row2, lv_color_hex(LINE), 0);

    build_param(_p[0], cell(row2, 0, 0, 200, ROW2_H, false), 1);

    lv_obj_t* mode = cell(row2, 200, 0, 96, ROW2_H, true);
    lv_obj_t* mg = group(mode, LV_FLEX_FLOW_ROW, 6);
    lv_obj_center(mg);
    lv_obj_t* ml = label(mg, T12B, GRAY, "MODE");
    lv_obj_set_style_text_letter_space(ml, 1, 0);
    _mode_pill = lv_obj_create(mg);
    lv_obj_remove_style_all(_mode_pill);
    lv_obj_set_size(_mode_pill, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(_mode_pill, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_hor(_mode_pill, 6, 0);
    lv_obj_set_style_pad_ver(_mode_pill, 2, 0);
    _mode_next = label(_mode_pill, T12B, WHITE, "FX");

    lv_obj_t* nav = cell(row2, 296, 0, 208, ROW2_H, true);
    lv_obj_t* ng = group(nav, LV_FLEX_FLOW_ROW, 8);
    lv_obj_center(ng);
    label(ng, T12B, MON, "NAV");
    _nav_val = label(ng, T12, GRAY, "VIEW");
    lv_obj_set_style_text_letter_space(_nav_val, 1, 0);

    _b6_cell = cell(row2, 504, 0, 96, ROW2_H, true);
    lv_obj_t* bg6 = group(_b6_cell, LV_FLEX_FLOW_ROW, 6);
    lv_obj_center(bg6);
    _b6_lbl = label(bg6, T12B, GRAY, "SHIFT");
    _b6_state = label(bg6, T12, MON, "OFF");

    build_param(_p[1], cell(row2, 600, 0, 200, ROW2_H, true), 2);

    set_mode(false, false);
}

void BottomBar::build_enc(Enc& e, lv_obj_t* c, bool right) {
    lv_obj_set_style_pad_hor(c, 12, 0);
    lv_obj_t* col = group(c, LV_FLEX_FLOW_COLUMN, 4);
    lv_obj_set_flex_align(col, LV_FLEX_ALIGN_CENTER, right ? LV_FLEX_ALIGN_END : LV_FLEX_ALIGN_START,
                          right ? LV_FLEX_ALIGN_END : LV_FLEX_ALIGN_START);
    lv_obj_align(col, right ? LV_ALIGN_RIGHT_MID : LV_ALIGN_LEFT_MID, 0, 0);

    lv_obj_t* head = group(col, LV_FLEX_FLOW_ROW, 8);
    if (right) {
        e.lbl = label(head, T12, GRAY, "");
        e.tag = label(head, T12B, TEAL, "R");
    } else {
        e.tag = label(head, T12B, TEAL, "L");
        e.lbl = label(head, T12, GRAY, "");
    }
    lv_obj_t* vrow = group(col, LV_FLEX_FLOW_ROW, 4);
    e.val = label(vrow, T20, WHITE, "");
    e.unit = label(vrow, T12, GRAY, "");

    e.track = rect(col, 0, 0, ENC_BAR_W, 4, TEAL_DIM);
    e.fill = rect(e.track, 0, 0, 0, 4, TEAL_PLUS);
    e.last_fw = -1;
}

void BottomBar::build_param(Param& p, lv_obj_t* c, int n) {
    p.cell = c;
    char t[4];
    std::snprintf(t, sizeof(t), "P%d", n);
    p.tag = label(c, T12B, TEAL, t);
    lv_obj_align(p.tag, LV_ALIGN_LEFT_MID, 16, -1);
    p.lbl = label(c, T12, GRAY, "");
    lv_obj_set_width(p.lbl, 96);
    lv_label_set_long_mode(p.lbl, LV_LABEL_LONG_CLIP);
    lv_obj_align(p.lbl, LV_ALIGN_LEFT_MID, 40, -1);
    p.unit = label(c, T12, GRAY, "");
    lv_obj_align(p.unit, LV_ALIGN_RIGHT_MID, -16, -1);
    p.val = label(c, T16, WHITE, "");
    lv_obj_align_to(p.val, p.unit, LV_ALIGN_OUT_LEFT_MID, -4, 0);
    p.track = rect(c, 16, ROW2_H - 2, P_TRACK_W, 2, TEAL_DIM);
    p.fill = rect(p.track, 0, 0, 0, 2, TEAL_PLUS);
    p.last_fw = -1;
}

void BottomBar::set_mode(bool fx, bool shift) {
    const int m = (fx ? 1 : 0) | (shift ? 2 : 0);
    if (m == _last_mode) return;
    _last_mode = m;
    _fx = fx;
    _shift = !fx && shift;
    const ModeColors mc = mode_colors(fx);

    lv_obj_set_style_border_color(_bar, lv_color_hex(mc.dim), 0);
    for (Enc& e : _enc) {
        set_text_color(e.tag, mc.mc);
        set_bg(e.track, mc.dim);
        set_bg(e.fill, mc.hi);
    }
    for (Key& k : _keys) set_bg(k.bar, BG);
    _last_active = -2;

    // MODE muestra el modo al que se pasa.
    set_bg(_mode_pill, fx ? TEAL : PURPLE);
    set_text_color(_mode_next, fx ? BG : WHITE);
    lv_label_set_text(_mode_next, fx ? "SYNTH" : "FX");

    // B6: HOLD en FX, SHIFT en SYNTH.
    lv_label_set_text(_b6_lbl, fx ? "HOLD" : "SHIFT");
    set_bg(_b6_cell, _shift ? mc.hi : BG);
    set_text_color(_b6_lbl, _shift ? BG : GRAY);
    set_text_color(_b6_state, _shift ? BG : MON);
    lv_label_set_text(_b6_state, _shift ? "ON" : "OFF");

    paint_params();
}

void BottomBar::paint_params() {
    const ModeColors mc = mode_colors(_fx);
    for (int i = 0; i < 2; ++i) {
        Param& p = _p[i];
        const uint32_t fg = _shift ? BG : WHITE;
        const uint32_t lc = _shift ? BG : GRAY;
        set_bg(p.cell, _shift ? mc.hi : BG);
        lv_label_set_text(p.tag, _shift ? (i == 0 ? "SHIFT P1" : "P2 SHIFT") : (i == 0 ? "P1" : "P2"));
        set_text_color(p.tag, _shift ? BG : (_fx ? mc.hi : mc.mc));
        set_text_color(p.lbl, lc);
        set_text_color(p.unit, lc);
        set_text_color(p.val, fg);
        set_bg(p.track, mc.dim);
        set_bg(p.fill, _shift ? BG : mc.hi);
        lv_obj_align(p.lbl, LV_ALIGN_LEFT_MID, _shift ? 88 : 40, -1);
    }
}

void BottomBar::set_enc(bool right, const char* lbl, const char* val, const char* unit, float frac) {
    Enc& e = _enc[right ? 1 : 0];
    char key[24];
    std::snprintf(key, sizeof(key), "%s|%s|%s", lbl, val, unit);
    if (std::strcmp(key, e.last) != 0) {
        std::snprintf(e.last, sizeof(e.last), "%s", key);
        lv_label_set_text(e.lbl, lbl);
        lv_label_set_text(e.val, val);
        lv_label_set_text(e.unit, unit);
    }
    const int fw = frac_px(frac, ENC_BAR_W);
    if (fw != e.last_fw) {
        e.last_fw = fw;
        lv_obj_set_width(e.fill, fw);
    }
}

void BottomBar::set_keys(const char* const labels[4], int active) {
    char key[64];
    std::snprintf(key, sizeof(key), "%s|%s|%s|%s", labels[0], labels[1], labels[2], labels[3]);
    if (std::strcmp(key, _last_keys) != 0) {
        std::snprintf(_last_keys, sizeof(_last_keys), "%s", key);
        for (int i = 0; i < 4; ++i) lv_label_set_text(_keys[i].lbl, labels[i]);
    }
    if (active == _last_active) return;
    _last_active = active;
    const ModeColors mc = mode_colors(_fx);
    for (int i = 0; i < 4; ++i) {
        const bool on = i == active;
        set_bg(_keys[i].cell, on ? mc.dim : BG);
        set_bg(_keys[i].bar, on ? mc.hi : BG);
        set_text_color(_keys[i].lbl, on ? WHITE : GRAY);
    }
}

void BottomBar::set_param(int n, const char* lbl, const char* val, const char* unit, float frac) {
    Param& p = _p[n == 2 ? 1 : 0];
    char key[32];
    std::snprintf(key, sizeof(key), "%s|%s|%s", lbl, val, unit);
    if (std::strcmp(key, p.last) != 0) {
        std::snprintf(p.last, sizeof(p.last), "%s", key);
        lv_label_set_text(p.lbl, lbl);
        lv_label_set_text(p.val, val);
        lv_label_set_text(p.unit, unit);
        lv_obj_align_to(p.val, p.unit, LV_ALIGN_OUT_LEFT_MID, -4, 0);
    }
    const int fw = frac_px(frac, P_TRACK_W);
    if (fw != p.last_fw) {
        p.last_fw = fw;
        lv_obj_set_width(p.fill, fw);
    }
}

void BottomBar::set_nav(const char* text) {
    if (std::strcmp(text, _last_nav) == 0) return;
    std::snprintf(_last_nav, sizeof(_last_nav), "%s", text);
    lv_label_set_text(_nav_val, text);
}
