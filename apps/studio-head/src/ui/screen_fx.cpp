#include "ui/screen_fx.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

#include "state/fx_catalog.h"
#include "studio_tick.h"
#include "ui/input.h"
#include "ui/theme.h"

using namespace gf;

namespace {

constexpr int METER_X = 16, METER_Y = 48;
constexpr int HEAD_X = 56, HEAD_Y = 48, HEAD_W = 456, HEAD_H = 64;
constexpr int TABS_X = 518, TAB_W = 88, TAB_H = 64;
constexpr int GRAPH_X = 56, GRAPH_Y = 120;
constexpr int LIST_W = 264, ROW_H = 27;
constexpr uint32_t LIST_MS = 1500;
constexpr uint32_t CLIP_LATCH_MS = 2000;
constexpr float PEAK_FALL_PX_PER_MS = 4.0f / 33.3f;  // 4 px por frame a 30 Hz (v3: meter)

const char* const KEYS_FX[4] = {"INSERT", "SEND", "MASTER", "FAV FX"};
const char* const TABS[3] = {"ONDA", "ESPECTRO", "FIRMA"};

lv_obj_t* flex(lv_obj_t* parent, lv_flex_flow_t flow, int gap) {
    lv_obj_t* o = lv_obj_create(parent);
    lv_obj_remove_style_all(o);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(o, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(o, flow);
    lv_obj_set_flex_align(o, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(o, gap, 0);
    lv_obj_set_style_pad_row(o, gap, 0);
    return o;
}

/// Píldora de 32 px de alto con texto T16 sobre fondo sólido.
lv_obj_t* pill(lv_obj_t* parent, uint32_t bg, const char* text, lv_obj_t** lbl) {
    lv_obj_t* p = flex(parent, LV_FLEX_FLOW_ROW, 0);
    lv_obj_set_height(p, 32);
    lv_obj_set_style_pad_hor(p, 12, 0);
    lv_obj_set_style_bg_opa(p, LV_OPA_COVER, 0);
    set_bg(p, bg);
    lv_obj_t* l = label(p, T16, BG, text);
    lv_obj_set_style_text_letter_space(l, 1, 0);
    if (lbl) *lbl = l;
    return p;
}

}  // namespace

lv_obj_t* ScreenFx::create() {
    _scr = screen_root();
    _top.create(_scr, "FX", -1);

    _meter = draw_area(_scr, METER_X, METER_Y, METER_W, METER_H, on_meter_draw, this);

    // ── Cabecera: SLOT / estado, nombre y píldoras ───────────────────────────
    lv_obj_t* head = flex(_scr, LV_FLEX_FLOW_COLUMN, 4);
    lv_obj_set_pos(head, HEAD_X, HEAD_Y);
    lv_obj_set_size(head, HEAD_W, HEAD_H);
    lv_obj_set_flex_align(head, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    _slot_line = label(head, T12, GRAY, "");
    lv_obj_set_style_text_letter_space(_slot_line, 1, 0);
    lv_obj_t* row = flex(head, LV_FLEX_FLOW_ROW, 8);
    _name = label(row, T28, WHITE, "");
    _pill_hold = pill(row, WHITE, "HOLD", nullptr);
    _pill = pill(row, PURPLE_BRIGHT, "ON", &_pill_txt);
    input_attach_tap(_pill, Ctl::NavPush);  // atajo táctil: tocar la píldora = NAV push (on / bypass)

    // ── Pestañas de vista: 3 × 88×64, separación 1 ───────────────────────────
    for (int i = 0; i < 3; ++i) {
        _tab[i] = rect(_scr, TABS_X + i * (TAB_W + 1), HEAD_Y, TAB_W, TAB_H, SURFACE);
        _tab_bar[i] = rect(_tab[i], 0, TAB_H - 4, TAB_W, 4, SURFACE);
        _tab_lbl[i] = label(_tab[i], T16, GRAY, TABS[i]);
        lv_obj_align(_tab_lbl[i], LV_ALIGN_CENTER, 0, 0);
        input_attach_tap(_tab[i], static_cast<Ctl>(int(Ctl::FxWave) + i));
    }

    _graph = draw_area(_scr, GRAPH_X, GRAPH_Y, VIZ_W, VIZ_H, on_graph_draw, this);
    input_attach_tap(_graph, Ctl::FxViewNext);

    // ── Lista de FX (temporal, sobre la gráfica) ─────────────────────────────
    // Sin borde propio: en LVGL el borde corre el contenido en los 4 lados, aunque se dibuje en uno.
    _list = rect(_scr, HEAD_X, HEAD_Y, LIST_W, METER_H, BG);
    rect(_list, LIST_W - 1, 0, 1, METER_H, LINE);
    for (int i = 0; i < FX_SLOTS; ++i) {
        Row& r = _rows[i];
        r.box = rect(_list, 0, i * ROW_H, LIST_W - 1, ROW_H, BG);
        lv_obj_set_style_border_side(r.box, LV_BORDER_SIDE_LEFT, 0);
        lv_obj_set_style_border_width(r.box, 4, 0);
        lv_obj_set_flex_flow(r.box, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(r.box, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_left(r.box, 8, 0);   // + borde de 4 = contenido en x 12, como el diseño
        lv_obj_set_style_pad_right(r.box, 8, 0);
        lv_obj_set_style_pad_column(r.box, 8, 0);
        char t[4];
        std::snprintf(t, sizeof(t), "%02d", i + 1);
        r.idx = label(r.box, T12, MON, t);
        lv_obj_set_width(r.idx, 16);
        r.name = label(r.box, T16R, GRAY, FX_CATALOG[i].name);
        lv_obj_set_flex_grow(r.name, 1);
        lv_label_set_long_mode(r.name, LV_LABEL_LONG_CLIP);
        r.dev = nullptr;
        r.sq = nullptr;
        if (!FX_CATALOG[i].available) {
            r.dev = label(r.box, T12, MON, "IN DEV");
            lv_obj_set_style_border_width(r.dev, 1, 0);
            lv_obj_set_style_border_color(r.dev, lv_color_hex(MON), 0);
            lv_obj_set_style_pad_hor(r.dev, 4, 0);
            lv_obj_set_style_pad_ver(r.dev, 1, 0);
        } else {
            r.sq = rect(r.box, 0, 0, 10, 10, BG);
            lv_obj_set_style_border_width(r.sq, 2, 0);
        }
    }
    lv_obj_add_flag(_list, LV_OBJ_FLAG_HIDDEN);

    _bottom.create(_scr);
    _bottom.set_mode(true, false);
    _bottom.set_nav("SLOT");
    return _scr;
}

void ScreenFx::show_list() {
    _list_until = studio_tick_ms() + LIST_MS;
}

void ScreenFx::on_graph_draw(lv_event_t* e) {
    auto* self = static_cast<ScreenFx*>(lv_event_get_user_data(e));
    if (self->_s) viz_draw(Painter::from(e), *self->_s, self->_st, self->_now);
}

void ScreenFx::on_meter_draw(lv_event_t* e) {
    auto* self = static_cast<ScreenFx*>(lv_event_get_user_data(e));
    meter_draw(Painter::from(e), self->_m);
}

void ScreenFx::paint_rows(const StudioState& s) {
    for (int i = 0; i < FX_SLOTS; ++i) {
        Row& r = _rows[i];
        const bool cur = i == s.fx_slot;
        const bool on = (s.fx_on >> i) & 1u;
        set_bg(r.box, cur ? PURPLE : BG);
        lv_obj_set_style_border_color(r.box, lv_color_hex(cur ? PURPLE_BRIGHT : BG), 0);
        set_text_color(r.idx, cur ? WHITE : MON);
        set_text_color(r.name, !FX_CATALOG[i].available ? MON : cur ? WHITE : GRAY);
        if (r.sq) {
            set_bg(r.sq, on ? (cur ? WHITE : PURPLE_BRIGHT) : (cur ? PURPLE : BG));
            lv_obj_set_style_border_color(r.sq, lv_color_hex(on ? (cur ? WHITE : PURPLE_BRIGHT) : (cur ? WHITE : MON)), 0);
        }
    }
}

void ScreenFx::update_meter(const StudioState& s, uint32_t now) {
    const float dt = _last_ms ? float(now - _last_ms) : 0.0f;
    _last_ms = now;
    MeterView m = _m;
    for (int j = 0; j < 2; ++j) {
        const int h = int(std::lround(272 * std::max(0.1f, s.in_level[j] / 255.0f)));
        _peak[j] = std::max(_peak[j] - PEAK_FALL_PX_PER_MS * dt, float(h));
        if (s.in_clip[j]) _clip_until[j] = now + CLIP_LATCH_MS;
        m.level[j] = uint8_t(h);
        m.peak[j] = uint8_t(std::min(270.0f, _peak[j]));
        m.clip[j] = now < _clip_until[j];
    }
    m.pad_db = s.in_pad_db;
    if (std::memcmp(&m, &_m, sizeof(m)) != 0) {
        _m = m;
        lv_obj_invalidate(_meter);
    }
}

void ScreenFx::update(const StudioState& s) {
    const uint32_t now = studio_tick_ms();
    const FxInfo& fx = FX_CATALOG[s.fx_slot];
    const float* val = s.fx_val[s.fx_slot];
    const bool on = (s.fx_on >> s.fx_slot) & 1u;
    const VizState st = s.fx_hold ? VizState::Hold : on ? VizState::Main : VizState::Bypass;

    _top.update(s);
    _s = &s;
    _now = now;
    _st = st;

    // ── Cabecera ─────────────────────────────────────────────────────────────
    const int on_key = (on ? 1 : 0) | (s.fx_hold ? 2 : 0);
    if (s.fx_slot != _last_slot || on_key != _last_on) {
        char t[48];
        std::snprintf(t, sizeof(t), "SLOT %02d / %d · %s", s.fx_slot + 1, FX_SLOTS,
                      s.fx_hold ? "ON WHILE HELD" : on ? "ACTIVE" : "BYPASSED");
        lv_label_set_text(_slot_line, t);
        lv_label_set_text(_name, fx.name);
        lv_label_set_text(_pill_txt, on ? "ON" : "BYPASS");
        set_bg(_pill, on ? PURPLE_BRIGHT : ORANGE);
        if (s.fx_hold) lv_obj_clear_flag(_pill_hold, LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(_pill_hold, LV_OBJ_FLAG_HIDDEN);
        paint_rows(s);
        _last_slot = s.fx_slot;
        _last_on = on_key;
    }

    if (int(s.fx_view) != _last_view) {
        _last_view = int(s.fx_view);
        for (int i = 0; i < 3; ++i) {
            const bool a = i == _last_view;
            set_bg(_tab[i], a ? PURPLE_DIM : SURFACE);
            set_bg(_tab_bar[i], a ? PURPLE_BRIGHT : SURFACE);
            set_text_color(_tab_lbl[i], a ? WHITE : GRAY);
        }
    }

    // ── Lista temporal ───────────────────────────────────────────────────────
    const bool show = now < _list_until;
    if (show != _list_shown) {
        _list_shown = show;
        if (show) lv_obj_clear_flag(_list, LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(_list, LV_OBJ_FLAG_HIDDEN);
    }

    // ── Barra inferior: ENC L DRY/WET · ENC R / P1 / P2 parámetros del FX ───
    _bottom.set_mode(true, false);
    _bottom.set_hold(s.fx_hold);
    _bottom.set_dim(st == VizState::Bypass);
    _bottom.set_keys(KEYS_FX, int(s.fx_route[s.fx_slot]));
    char v[16];
    std::snprintf(v, sizeof(v), "%d", int(std::lround(val[0])));
    _bottom.set_enc(false, "DRY/WET", v, "%", val[0] / 100.0f);
    fx_format(fx.p[0], val[1], v, sizeof(v));
    _bottom.set_enc(true, fx.p[0].name, v, fx.p[0].unit, fx_frac(fx.p[0], val[1]));
    for (int k = 1; k < 3; ++k) {
        fx_format(fx.p[k], val[k + 1], v, sizeof(v));
        _bottom.set_param(k, fx.p[k].name, v, fx.p[k].unit, fx_frac(fx.p[k], val[k + 1]));
    }

    update_meter(s, now);
    lv_obj_invalidate(_graph);  // ONDA y ESPECTRO cambian en cada frame; LVGL refresca a 30 Hz
}
