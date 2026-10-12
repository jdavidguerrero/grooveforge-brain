#include "ui/overlays.h"

#include <algorithm>
#include <climits>
#include <cmath>
#include <cstdio>
#include <cstring>

#include "ui/theme.h"

using namespace gf;

namespace {

constexpr uint32_t POT_MS = 1500, TOAST_MS = 1500, BANNER_MS = 600, CLIP_MS = 2000;
constexpr float VOL_LO = -42.0f, VOL_HI = 6.0f;   // escala de los 32 segmentos de VOL
constexpr float IN_PEAK_FALL_PER_MS = 10.0f / 1000.0f;  // segmentos por ms

/// Panel con fondo SURFACE y marco interior de 2 px (outline: no corre el contenido).
lv_obj_t* panel(lv_obj_t* parent, int x, int y, int w, int h, uint32_t frame) {
    lv_obj_t* p = rect(parent, x, y, w, h, SURFACE);
    lv_obj_set_style_outline_width(p, 2, 0);
    lv_obj_set_style_outline_pad(p, -2, 0);
    lv_obj_set_style_outline_color(p, lv_color_hex(frame), 0);
    lv_obj_add_flag(p, LV_OBJ_FLAG_HIDDEN);
    return p;
}

/// Chip de 40×24 (VOL / IN): la etiqueta del pot físico junto a la pantalla.
void chip(lv_obj_t* parent, int x, const char* text) {
    lv_obj_t* c = rect(parent, x, 0, 40, 24, GRAY);
    lv_obj_t* l = label(c, T12B, BG, text);
    lv_obj_set_style_text_letter_space(l, 1, 0);
    lv_obj_center(l);
}

/// Valor hero T64 con la caja de 64 px del diseño (la line_height de la fuente es mayor).
lv_obj_t* hero(lv_obj_t* parent, int x, int y) {
    lv_obj_t* v = label(parent, T64, WHITE, "");
    lv_obj_set_pos(v, x, y - (T64->line_height - 64) / 2);
    return v;
}

/// Unidad al costado del valor, con las líneas base alineadas.
void unit_after(lv_obj_t* unit, lv_obj_t* value) {
    lv_obj_align_to(unit, value, LV_ALIGN_OUT_RIGHT_BOTTOM, 8, -(T64->base_line - T16R->base_line));
}

void fmt_db(float db, bool plus, char* out, size_t n) {
    std::snprintf(out, n, plus ? "%+.1f" : "%.1f", double(db));
}

}  // namespace

void Overlays::create() {
    lv_obj_t* top = lv_layer_top();
    build_volume(top);
    build_input(top);
    build_toast(top);
    build_banner(top);
    build_clip(top);
}

void Overlays::build_volume(lv_obj_t* top) {
    _vol = panel(top, 384, 56, 416, 144, GRAY);
    chip(_vol, 416 - 40, "VOL");
    lv_obj_t* cap = label(_vol, T12, GRAY, "OUTPUT VOLUME");
    lv_obj_set_style_text_letter_space(cap, 1, 0);
    lv_obj_set_pos(cap, 24, 16);
    _vol_val = hero(_vol, 24, 40);
    lv_obj_t* u = label(_vol, T16R, GRAY, "DB");
    lv_obj_set_user_data(_vol_val, u);
    for (int i = 0; i < 32; ++i) _vol_seg[i] = rect(_vol, 24 + i * 11, 112, 8, 12, LINE);
}

void Overlays::build_input(lv_obj_t* top) {
    _in = panel(top, 0, 56, 416, 144, GRAY);
    chip(_in, 0, "IN");
    lv_obj_t* cap = label(_in, T12, GRAY, "INPUT GAIN");
    lv_obj_set_style_text_letter_space(cap, 1, 0);
    lv_obj_set_pos(cap, 24 + 28, 16);
    _in_pad = label(_in, T12B, WHITE, "");
    lv_obj_set_style_border_width(_in_pad, 1, 0);
    lv_obj_set_style_border_color(_in_pad, lv_color_hex(GRAY), 0);
    lv_obj_set_style_pad_hor(_in_pad, 6, 0);
    lv_obj_align(_in_pad, LV_ALIGN_TOP_RIGHT, -24, 16);
    _in_val = hero(_in, 24, 38);
    lv_obj_t* u = label(_in, T16R, GRAY, "DB");
    lv_obj_set_user_data(_in_val, u);
    for (int r = 0; r < 2; ++r)
        for (int i = 0; i < 31; ++i) {
            _in_seg[r][i] = rect(_in, 24 + i * 11, 108 + r * 12, 8, 8, LINE);
            _in_seg_c[r][i] = LINE;
        }
}

void Overlays::build_toast(lv_obj_t* top) {
    _toast = rect(top, 200, 252, 400, 64, ORANGE);
    lv_obj_t* t1 = label(_toast, T28, BG, "FILTER BYPASS · ON");
    lv_obj_align(t1, LV_ALIGN_TOP_MID, 0, 8);
    lv_obj_t* t2 = label(_toast, T12, BG, "ENC L PUSH TO RESTORE");
    lv_obj_set_style_text_letter_space(t2, 1, 0);
    lv_obj_align(t2, LV_ALIGN_BOTTOM_MID, 0, -6);
    lv_obj_add_flag(_toast, LV_OBJ_FLAG_HIDDEN);
}

void Overlays::build_banner(lv_obj_t* top) {
    _banner = rect(top, 0, 152, 800, 128, PURPLE);
    lv_obj_set_style_pad_hor(_banner, 32, 0);
    lv_obj_set_flex_flow(_banner, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(_banner, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(_banner, 32, 0);
    _banner_big = label(_banner, T96, WHITE, "FX");
    lv_obj_t* col = lv_obj_create(_banner);
    lv_obj_remove_style_all(col);
    lv_obj_set_size(col, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(col, 8, 0);
    label(col, T28, WHITE, "MODE");
    _banner_hint = label(col, T12, WHITE, "");
    lv_obj_set_style_text_letter_space(_banner_hint, 1, 0);
    lv_obj_add_flag(_banner, LV_OBJ_FLAG_HIDDEN);
}

void Overlays::build_clip(lv_obj_t* top) {
    _scrim = rect(top, 0, 0, 800, 480, 0x000000);
    lv_obj_set_style_bg_opa(_scrim, LV_OPA_COVER * 55 / 100, 0);
    lv_obj_add_flag(_scrim, LV_OBJ_FLAG_HIDDEN);

    _clip = panel(top, 200, 168, 400, 144, AMBER);
    lv_obj_t* w = label(_clip, T12B, AMBER, "WARNING");
    lv_obj_set_style_text_letter_space(w, 1, 0);
    lv_obj_set_pos(w, 24, 18);
    lv_obj_t* in = label(_clip, T12B, MON, "IN");
    lv_obj_align(in, LV_ALIGN_TOP_RIGHT, -24, 18);
    lv_obj_t* title = label(_clip, T28, WHITE, "INPUT CLIPPING");
    lv_obj_set_pos(title, 24, 42 - (T28->line_height - 32) / 2);
    lv_obj_t* act = label(_clip, T16, AMBER, "BAJA IN O SUBE EL PAD");
    lv_obj_set_pos(act, 24, 82 - (T16->line_height - 16) / 2);
    _clip_detail = label(_clip, T12, GRAY, "");
    lv_obj_set_pos(_clip_detail, 24, 106);
}

void Overlays::show(lv_obj_t* o, bool on) {
    if (on == !lv_obj_has_flag(o, LV_OBJ_FLAG_HIDDEN)) return;
    if (on) lv_obj_clear_flag(o, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(o, LV_OBJ_FLAG_HIDDEN);
}

void Overlays::paint_volume(const StudioState& s) {
    char t[12];
    fmt_db(s.volume_db, false, t, sizeof(t));
    lv_label_set_text(_vol_val, t);
    unit_after(static_cast<lv_obj_t*>(lv_obj_get_user_data(_vol_val)), _vol_val);
    const int lit = int(std::lround(32 * std::clamp((s.volume_db - VOL_LO) / (VOL_HI - VOL_LO), 0.0f, 1.0f)));
    if (lit == _vol_lit) return;
    _vol_lit = lit;
    for (int i = 0; i < 32; ++i) set_bg(_vol_seg[i], i < lit ? GRAY : LINE);
}

void Overlays::paint_input(const StudioState& s, uint32_t now) {
    char key[32];
    std::snprintf(key, sizeof(key), "%.1f|%d", double(s.in_gain_db), s.in_pad_db);
    if (std::strcmp(key, _last_in_key) != 0) {
        std::snprintf(_last_in_key, sizeof(_last_in_key), "%s", key);
        char t[16];
        fmt_db(s.in_gain_db, true, t, sizeof(t));
        lv_label_set_text(_in_val, t);
        unit_after(static_cast<lv_obj_t*>(lv_obj_get_user_data(_in_val)), _in_val);
        if (s.in_pad_db) std::snprintf(t, sizeof(t), "PAD −%d dB", -s.in_pad_db);
        else std::snprintf(t, sizeof(t), "PAD 0 dB");
        lv_label_set_text(_in_pad, t);
    }
    // Medidor: 30 segmentos + 1 de clip; los últimos 4 en ámbar y el pico en blanco.
    const float dt = _last_ms ? float(now - _last_ms) : 0.0f;
    _last_ms = now;
    for (int r = 0; r < 2; ++r) {
        const int lit = int(std::lround(s.in_level[r] / 255.0f * 30));
        _in_peak[r] = std::max(_in_peak[r] - IN_PEAK_FALL_PER_MS * dt, float(lit));
        const int pk = std::min(29, int(_in_peak[r]));
        const bool clip = now < _clip_ch_until[r];
        for (int i = 0; i < 31; ++i) {
            uint32_t c = LINE;
            if (i == 30) c = clip ? RED : LINE;
            else if (i == pk && pk > 0) c = WHITE;
            else if (i < lit) c = i >= 26 ? AMBER : GRAY;
            if (c != _in_seg_c[r][i]) {
                _in_seg_c[r][i] = c;
                set_bg(_in_seg[r][i], c);
            }
        }
    }
}

void Overlays::paint_banner(bool fx) {
    set_bg(_banner, fx ? PURPLE : TEAL);
    lv_label_set_text(_banner_big, fx ? "FX" : "SYNTH");
    lv_label_set_text(_banner_hint, fx ? "ENC L DRY/WET · ENC R · P1 · P2 FX PARAMS · B6 HOLD · MODE BACK"
                                       : "ENC L CUTOFF · ENC R RES · B6 SHIFT · MODE BACK");
}

void Overlays::force(Overlay o) {
    switch (o) {
        case Overlay::Volume:     _vol_until = UINT32_MAX; break;
        case Overlay::Input:      _in_until = UINT32_MAX; break;
        case Overlay::Bypass:     _toast_until = UINT32_MAX; break;
        case Overlay::ModeBanner: _banner_until = UINT32_MAX; break;
        case Overlay::Clip:       _clip_ch_until[0] = _clip_ch_until[1] = UINT32_MAX; break;
    }
}

void Overlays::update(const StudioState& s, uint32_t now) {
    if (!_init) {
        _init = true;
        _last_vol = s.volume_db;
        _last_gain = s.in_gain_db;
        _last_pad = s.in_pad_db;
        _last_bypass = s.filter_bypass;
        _last_mode = s.mode;
        paint_volume(s);
        paint_banner(s.mode == Mode::Fx);
    }
    if (s.volume_db != _last_vol) {
        _last_vol = s.volume_db;
        _vol_until = now + POT_MS;
        paint_volume(s);
    }
    if (s.in_gain_db != _last_gain || s.in_pad_db != _last_pad) {
        _last_gain = s.in_gain_db;
        _last_pad = s.in_pad_db;
        _in_until = now + POT_MS;
    }
    if (s.filter_bypass != _last_bypass) {
        _last_bypass = s.filter_bypass;
        _toast_until = s.filter_bypass ? now + TOAST_MS : 0;
    }
    if (s.mode != _last_mode) {
        _last_mode = s.mode;
        paint_banner(s.mode == Mode::Fx);
        _banner_until = now + BANNER_MS;
    }
    for (int j = 0; j < 2; ++j)
        if (s.in_clip[j]) _clip_ch_until[j] = now + CLIP_MS;

    const bool err = s.link == LinkState::Lost;
    show(_vol, now < _vol_until);
    const bool in_vis = !err && now < _in_until;
    show(_in, in_vis);
    if (in_vis) paint_input(s, now);
    show(_toast, !err && now < _toast_until);
    show(_banner, !err && now < _banner_until);

    const bool cl = now < _clip_ch_until[0], cr = now < _clip_ch_until[1];
    const bool clip_vis = !err && (cl || cr);
    show(_scrim, clip_vis);
    show(_clip, clip_vis);
    if (clip_vis) {
        char t[64], pad[12];
        if (s.in_pad_db) std::snprintf(pad, sizeof(pad), "−%d dB", -s.in_pad_db);
        else std::snprintf(pad, sizeof(pad), "0 dB");
        std::snprintf(t, sizeof(t), "IN %+.1f DB · PAD %s · CLIP%s%s", double(s.in_gain_db), pad, cl ? " L" : "", cr ? " R" : "");
        if (std::strcmp(lv_label_get_text(_clip_detail), t) != 0) lv_label_set_text(_clip_detail, t);
    }
}
