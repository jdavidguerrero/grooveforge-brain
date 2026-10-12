#include "ui/screen_presets.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "ui/engines.h"
#include "ui/theme.h"

using namespace gf;

namespace {

constexpr int LIST_X = 16, LIST_Y = 104, LIST_W = 456, ROW_H = 40;
constexpr int SCROLL_X = 476, SCROLL_H = 240;
const char* const KEYS_PRESET[4] = {"OSC", "ENV", "LFO / MOD", "PRESET"};

}  // namespace

lv_obj_t* ScreenPresets::create() {
    _scr = screen_root();
    _top.create(_scr, "PRESETS", -1);

    // ── Cabecera: engine activo ──────────────────────────────────────────────
    lv_obj_t* head = rect(_scr, 16, 48, 768, 48, BG);
    lv_obj_set_style_border_side(head, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_width(head, 1, 0);
    lv_obj_set_style_border_color(head, lv_color_hex(LINE), 0);
    lv_obj_set_flex_flow(head, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(head, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(head, 16, 0);
    lv_obj_t* el = label(head, T12, GRAY, "ENGINE");
    lv_obj_set_style_text_letter_space(el, 1, 0);
    _eng_name = label(head, T20, WHITE, "");
    _eng_tag = label(head, T12, GRAY, "");
    lv_obj_set_flex_grow(_eng_tag, 1);
    label(head, T12, MON, "B4 · ENGINE SELECT");

    // ── Lista: 6 filas de 40 ─────────────────────────────────────────────────
    for (int i = 0; i < VISIBLE; ++i) {
        Row& r = _rows[i];
        r.box = rect(_scr, LIST_X, LIST_Y + i * ROW_H, LIST_W, ROW_H, BG);
        // Borde izquierdo de 4 (cursor) y separador inferior de 1 como objetos aparte: en LVGL el
        // borde corre el contenido en los cuatro lados.
        rect(r.box, 0, ROW_H - 1, LIST_W, 1, SURFACE_2);
        lv_obj_t* bar = rect(r.box, 0, 0, 4, ROW_H - 1, BG);
        lv_obj_set_user_data(r.box, bar);
        r.slot = label(r.box, T12, MON, "");
        lv_obj_align(r.slot, LV_ALIGN_LEFT_MID, 16, 0);
        r.name = label(r.box, T16R, GRAY, "");
        lv_obj_align(r.name, LV_ALIGN_LEFT_MID, 16 + 40 + 16, 0);
        r.loaded = label(r.box, T12B, TEAL_PLUS, "LOADED");
        lv_obj_set_style_text_letter_space(r.loaded, 1, 0);
        lv_obj_align(r.loaded, LV_ALIGN_RIGHT_MID, -16, 0);
    }
    _thumb = rect(rect(_scr, SCROLL_X, LIST_Y, 4, SCROLL_H, SURFACE_2), 0, 0, 4, 80, TEAL);

    // ── Detalle del preset bajo el cursor ────────────────────────────────────
    lv_obj_t* d = box(_scr, 496, 104, 288, 272, SURFACE, LINE);
    lv_obj_set_style_pad_all(d, 15, 0);
    lv_obj_set_flex_flow(d, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(d, 8, 0);
    _d_art = engine_art_create(d, 0, 0, 120, 80, 0);
    _d_slot = label(d, T12, MON, "");
    lv_obj_set_style_pad_top(_d_slot, 4, 0);
    _d_name = label(d, T28, WHITE, "");
    lv_obj_set_width(_d_name, 256);
    lv_label_set_long_mode(_d_name, LV_LABEL_LONG_CLIP);
    _d_info = label(d, T12, GRAY, "");
    lv_obj_t* spacer = rect(d, 0, 0, 1, 1, SURFACE);
    lv_obj_set_flex_grow(spacer, 1);
    lv_obj_t* act = box(d, 0, 0, 256, 48, SURFACE, TEAL);
    lv_obj_set_style_pad_hor(act, 15, 0);
    lv_obj_t* np = label(act, T12B, TEAL_PLUS, "NAV PUSH");
    lv_obj_set_style_text_letter_space(np, 1, 0);
    lv_obj_align(np, LV_ALIGN_LEFT_MID, 0, 0);
    _d_action = label(act, T16, WHITE, "LOAD");
    lv_obj_align(_d_action, LV_ALIGN_RIGHT_MID, 0, 0);

    _bottom.create(_scr);
    _bottom.set_params_off(true);
    _bottom.set_nav("LIST");
    _cursor = -1;
    return _scr;
}

void ScreenPresets::move_cursor(int dir, const StudioState& s) {
    const int rows = s.preset_count + 1;
    _cursor = std::clamp(_cursor + dir, 0, rows - 1);
}

void ScreenPresets::update(const StudioState& s) {
    _top.update(s);
    _bottom.set_mode(s.mode == Mode::Fx, s.shift);
    _bottom.set_filter_encs(s.cutoff_hz, s.resonance);
    _bottom.set_keys(KEYS_PRESET, 3);

    if (_cursor < 0) _cursor = s.preset_loaded + 1;
    const int rows = s.preset_count + 1;
    _cursor = std::clamp(_cursor, 0, rows - 1);

    char key[48];
    std::snprintf(key, sizeof(key), "%d|%d|%d|%d|%s", _cursor, s.engine, s.preset_loaded, s.preset_count,
                  s.preset_count ? s.preset_names[s.preset_count - 1] : "");
    if (std::strcmp(key, _last_key) == 0) return;
    std::snprintf(_last_key, sizeof(_last_key), "%s", key);

    const EngineInfo& e = engine_info(s.engine);
    lv_label_set_text(_eng_name, e.name);
    lv_label_set_text(_eng_tag, e.tag);

    // Ventana de 6 filas que sigue al cursor.
    if (_cursor < _top_row) _top_row = _cursor;
    if (_cursor >= _top_row + VISIBLE) _top_row = _cursor - VISIBLE + 1;
    _top_row = std::clamp(_top_row, 0, std::max(0, rows - VISIBLE));

    for (int i = 0; i < VISIBLE; ++i) {
        Row& r = _rows[i];
        const int idx = _top_row + i;
        if (idx >= rows) { lv_obj_add_flag(r.box, LV_OBJ_FLAG_HIDDEN); continue; }
        lv_obj_clear_flag(r.box, LV_OBJ_FLAG_HIDDEN);
        const bool save = idx == 0, cur = idx == _cursor;
        const bool loaded = !save && idx - 1 == s.preset_loaded;
        char slot[16] = "";
        if (!save) std::snprintf(slot, sizeof(slot), "A-%02d", idx);
        lv_label_set_text(r.slot, slot);
        lv_label_set_text(r.name, save ? "+ SAVE CURRENT AS NEW" : s.preset_names[idx - 1]);
        lv_obj_set_style_text_font(r.name, cur || save ? T16 : T16R, 0);
        set_text_color(r.name, save ? TEAL_PLUS : cur ? WHITE : GRAY);
        set_bg(r.box, cur ? TEAL_DIM : BG);
        set_bg(static_cast<lv_obj_t*>(lv_obj_get_user_data(r.box)), cur ? TEAL_PLUS : BG);
        if (loaded) lv_obj_clear_flag(r.loaded, LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(r.loaded, LV_OBJ_FLAG_HIDDEN);
    }

    // Barra de scroll: alto proporcional a lo visible.
    const int th = std::max(16, SCROLL_H * VISIBLE / std::max(rows, VISIBLE));
    const int ty = rows > VISIBLE ? (SCROLL_H - th) * _top_row / (rows - VISIBLE) : 0;
    lv_obj_set_height(_thumb, th);
    lv_obj_set_y(_thumb, ty);

    engine_art_set(_d_art, s.engine);
    char info[48];
    std::snprintf(info, sizeof(info), "%s · %s", e.name, e.tag);
    lv_label_set_text(_d_info, info);
    if (_cursor == 0) {
        char slot[16];
        std::snprintf(slot, sizeof(slot), "A-%02d", s.preset_count + 1);
        lv_label_set_text(_d_slot, slot);
        lv_label_set_text(_d_name, "NEW PRESET");
        lv_label_set_text(_d_action, "SAVE");
    } else {
        char slot[16];
        std::snprintf(slot, sizeof(slot), "A-%02d", _cursor);
        lv_label_set_text(_d_slot, slot);
        lv_label_set_text(_d_name, s.preset_names[_cursor - 1]);
        lv_label_set_text(_d_action, "LOAD");
    }
}
