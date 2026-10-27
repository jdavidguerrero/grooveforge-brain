#include "ui/screen_presets.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "ui/engines.h"
#include "ui/input.h"
#include "ui/theme.h"

using namespace gf;

namespace {

constexpr int LIST_X = 16, LIST_Y = 104, LIST_W = 456, ROW_H = 40;
constexpr int SCROLL_X = 476, SCROLL_H = 240;
const char* const KEYS_PRESET[4] = {"BANK A", "BANK B", "BANK C", "BANK D"};  // B1–B4 saltan de banco (theory/13)
const char* const KEYS_LAYER[4] = {"BANK A", "BANK B", "LFO / MOD", "SYNTH FX"};  // HYBRID SYNTH (theory/12 §3d)

}  // namespace

lv_obj_t* ScreenPresets::create() {
    _scr = screen_root();
    _top.create(_scr, _layers ? "LAYER PRESETS" : "PRESETS", -1);

    // ── Cabecera: engine activo ──────────────────────────────────────────────
    lv_obj_t* head = rect(_scr, 16, 48, 768, 48, BG);
    lv_obj_set_style_border_side(head, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_width(head, 1, 0);
    lv_obj_set_style_border_color(head, lv_color_hex(LINE), 0);
    lv_obj_set_flex_flow(head, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(head, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(head, 16, 0);
    lv_obj_t* el = label(head, T12, GRAY, _layers ? "LAYER" : "ENGINE");
    lv_obj_set_style_text_letter_space(el, 1, 0);
    _eng_name = label(head, T20, WHITE, "");
    _eng_tag = label(head, T12, GRAY, "");
    lv_obj_set_flex_grow(_eng_tag, 1);
    label(head, T12, MON, _layers ? "A + B · LAS DOS MITADES" : "SHIFT + NAV PUSH · ENGINE");

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
        input_attach_tap(r.box, static_cast<Ctl>(int(Ctl::Pick0) + i));
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
    input_attach_tap(act, Ctl::NavPush);  // atajo táctil: tocar NAV PUSH · LOAD
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
    const int rows = count(s) + 1;
    _cursor = std::clamp(_cursor + dir, 0, rows - 1);
}

bool ScreenPresets::tap_row(int i, const StudioState& s) {
    const int row = std::clamp(_top_row + i, 0, count(s));
    if (row == _cursor) return true;
    _cursor = row;
    return false;
}

void ScreenPresets::update(const StudioState& s) {
    _top.update(s);
    _bottom.set_mode(s.mode == Mode::Fx, s.shift, s.mode == Mode::Hybrid);
    _bottom.set_filter_encs(s.cutoff_hz, s.resonance);
    const int n = count(s), ld = loaded(s);
    if (_layers) {                                   // como en la home: P1 MORPH · P2 retraso de B (theory/12 §3d)
        char m[8], d[8];
        std::snprintf(m, sizeof(m), "%d", int(s.hybrid.morph));
        std::snprintf(d, sizeof(d), "%d", int(s.hybrid.delay_b_ms));
        _bottom.set_params_off(false);
        _bottom.set_page_control(0, "MORPH", m, "%", s.hybrid.morph / 100.0f);
        _bottom.set_page_control(1, "B DELAY", d, "MS", s.hybrid.delay_b_ms / 35.0f);
    }
    _bottom.set_keys(_layers ? KEYS_LAYER : KEYS_PRESET, _cursor > 0 ? std::min(_layers ? 1 : 3, (_cursor - 1) / 16) : 0);

    if (_cursor < 0) _cursor = ld + 1;
    const int rows = n + 1;
    _cursor = std::clamp(_cursor, 0, rows - 1);

    char key[64];
    std::snprintf(key, sizeof(key), "%d|%d|%d|%d|%d|%s", _cursor, s.engine, ld, n, s.hybrid.engine_a,
                  n ? name(s, n - 1) : "");
    if (std::strcmp(key, _last_key) == 0) return;
    std::snprintf(_last_key, sizeof(_last_key), "%s", key);

    const EngineInfo& e = engine_info(_layers ? s.hybrid.engine_a : s.engine);
    if (_layers) {
        char t[48];
        std::snprintf(t, sizeof(t), "%s + %s", engine_info(s.hybrid.engine_a).name, engine_info(s.hybrid.engine_b).name);
        lv_label_set_text(_eng_name, ld >= 0 ? name(s, ld) : "SIN CAPA");
        lv_label_set_text(_eng_tag, ld >= 0 ? t : "");
    } else {
        lv_label_set_text(_eng_name, e.name);
        lv_label_set_text(_eng_tag, e.tag);
    }

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
        const bool is_loaded = !save && idx - 1 == ld;
        char slot[16] = "";
        if (!save) preset_slot(slot, sizeof(slot), idx - 1);
        lv_label_set_text(r.slot, slot);
        lv_label_set_text(r.name, save ? (_layers ? "+ SAVE LAYER AS NEW" : "+ SAVE CURRENT AS NEW") : name(s, idx - 1));
        lv_obj_set_style_text_font(r.name, cur || save ? T16 : T16R, 0);
        set_text_color(r.name, save ? TEAL_PLUS : cur ? WHITE : GRAY);
        set_bg(r.box, cur ? TEAL_DIM : BG);
        set_bg(static_cast<lv_obj_t*>(lv_obj_get_user_data(r.box)), cur ? TEAL_PLUS : BG);
        if (is_loaded) lv_obj_clear_flag(r.loaded, LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(r.loaded, LV_OBJ_FLAG_HIDDEN);
    }

    // Barra de scroll: alto proporcional a lo visible.
    const int th = std::max(16, SCROLL_H * VISIBLE / std::max(rows, VISIBLE));
    const int ty = rows > VISIBLE ? (SCROLL_H - th) * _top_row / (rows - VISIBLE) : 0;
    lv_obj_set_height(_thumb, th);
    lv_obj_set_y(_thumb, ty);

    engine_art_set(_d_art, _layers ? s.hybrid.engine_a : s.engine);
    char info[48];
    if (_layers) std::snprintf(info, sizeof(info), "HYBRID SYNTH · MITADES A + B");
    else std::snprintf(info, sizeof(info), "%s · %s", e.name, e.tag);
    lv_label_set_text(_d_info, info);
    if (_cursor == 0) {
        char slot[16];
        preset_slot(slot, sizeof(slot), n);
        lv_label_set_text(_d_slot, slot);
        lv_label_set_text(_d_name, _layers ? "NEW LAYER" : "NEW PRESET");
        lv_label_set_text(_d_action, "SAVE");
    } else {
        char slot[16];
        preset_slot(slot, sizeof(slot), _cursor - 1);
        lv_label_set_text(_d_slot, slot);
        lv_label_set_text(_d_name, name(s, _cursor - 1));
        lv_label_set_text(_d_action, "LOAD");
    }
}
