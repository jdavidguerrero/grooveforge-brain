#include "ui/screen_engine.h"

#include "ui/input.h"
#include "ui/theme.h"

using namespace gf;

namespace {

constexpr int GRID_X = 312, GRID_Y = 48, TILE_W = 152, TILE_H = 160, GAP = 8;
const char* const KEYS_ENGINE[4] = {"OSC", "ENV", "LFO / MOD", "ENGINE"};

/// Caja de acción "NAV PUSH · <acción>" de 280×48.
void action_box(lv_obj_t* parent, int x, int y, uint32_t border, const char* action) {
    lv_obj_t* b = box(parent, x, y, 280, 48, BG, border);
    lv_obj_set_style_pad_hor(b, 15, 0);
    lv_obj_t* l = label(b, T12B, TEAL_PLUS, "NAV PUSH");
    lv_obj_set_style_text_letter_space(l, 1, 0);
    lv_obj_align(l, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_t* a = label(b, T16, WHITE, action);
    lv_obj_align(a, LV_ALIGN_RIGHT_MID, 0, 0);
}

}  // namespace

lv_obj_t* ScreenEngine::create() {
    _scr = screen_root();
    _top.create(_scr, "ENGINE SELECT", -1);

    // ── Card del engine bajo el cursor ───────────────────────────────────────
    _focus_art = engine_art_create(_scr, 16, 48, 280, 180, 0);
    lv_obj_t* frame = lv_obj_create(_scr);
    lv_obj_remove_style_all(frame);
    lv_obj_clear_flag(frame, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_pos(frame, 16, 48);
    lv_obj_set_size(frame, 280, 180);
    lv_obj_set_style_border_width(frame, 2, 0);
    lv_obj_set_style_border_color(frame, lv_color_hex(TEAL_PLUS), 0);

    _name = label(_scr, T28, WHITE, "");
    lv_obj_set_pos(_name, 16, 236);
    _tag = label(_scr, T16, TEAL_PLUS, "");
    lv_obj_set_style_text_letter_space(_tag, 1, 0);
    lv_obj_set_pos(_tag, 16, 276);
    _arch = label(_scr, T12, GRAY, "");
    lv_obj_set_pos(_arch, 16, 300);
    action_box(_scr, 16, 328, TEAL_DIM, "LOAD ENGINE");

    // ── Grilla: 3 × 152, filas de 160, separación 8 ──────────────────────────
    for (int i = 0; i < ENGINE_COUNT; ++i) {
        Tile& t = _tiles[i];
        const EngineInfo& e = engine_info(uint8_t(i));
        t.box = box(_scr, GRID_X + (i % 3) * (TILE_W + GAP), GRID_Y + (i / 3) * (TILE_H + GAP), TILE_W, TILE_H, SURFACE, LINE);
        lv_obj_set_style_border_width(t.box, 2, 0);
        t.art = engine_art_create(t.box, 14, 12, 120, 80, uint8_t(i));
        t.name = label(t.box, T16, WHITE, e.name);
        lv_obj_set_pos(t.name, 14, 100);
        t.active = rect(t.box, 14 + 120 - 8, 104, 8, 8, TEAL_PLUS);
        t.tag = label(t.box, T12, GRAY, e.tag);
        lv_obj_set_pos(t.tag, 14, 120);
        input_attach_tap(t.box, static_cast<Ctl>(int(Ctl::Pick0) + i));
    }

    _bottom.create(_scr);
    _bottom.set_params_off(true);
    _bottom.set_nav("ENGINE");
    return _scr;
}

void ScreenEngine::move_cursor(int dir) {
    _cursor = (_cursor + dir + ENGINE_COUNT) % ENGINE_COUNT;
}

void ScreenEngine::set_cursor(int i) {
    _cursor = (i % ENGINE_COUNT + ENGINE_COUNT) % ENGINE_COUNT;
}

void ScreenEngine::update(const StudioState& s) {
    _top.update(s);
    _bottom.set_mode(s.mode == Mode::Fx, s.shift);
    _bottom.set_filter_encs(s.cutoff_hz, s.resonance);
    _bottom.set_keys(KEYS_ENGINE, 3);

    if (_cursor == _last_cursor && s.engine == _last_active) return;
    const EngineInfo& e = engine_info(uint8_t(_cursor));
    engine_art_set(_focus_art, uint8_t(_cursor));
    lv_label_set_text(_name, e.name);
    lv_label_set_text(_tag, e.tag);
    lv_label_set_text(_arch, e.arch);
    for (int i = 0; i < ENGINE_COUNT; ++i) {
        const bool cur = i == _cursor;
        set_bg(_tiles[i].box, cur ? TEAL_DIM : SURFACE);
        lv_obj_set_style_border_color(_tiles[i].box, lv_color_hex(cur ? TEAL_PLUS : LINE), 0);
        if (i == s.engine) lv_obj_clear_flag(_tiles[i].active, LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(_tiles[i].active, LV_OBJ_FLAG_HIDDEN);
    }
    _last_cursor = _cursor;
    _last_active = s.engine;
}
