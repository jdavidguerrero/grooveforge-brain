#include "ui/screen_hybrid.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

#include "ui/edit_draw.h"
#include "ui/engines.h"
#include "ui/input.h"
#include "ui/theme.h"

using namespace gf;

namespace {

constexpr int GX = 16, GY = 48, GW = 768, GH = 208;
constexpr int CARD_Y = 56, CARD_W = 300, CARD_H = 104, CARD_A_X = 16, CARD_B_X = GW - 16 - CARD_W;
constexpr uint32_t TRACK = 0x1C1C1C;
const char* const KEYS_HYBRID[4] = {"OSC", "ENV", "LFO / MOD", "SYNTH FX"};   // las de SYNTH (theory/12 §3d rev. 5)
const char* const SAVE_CHIP = "+ SAVE LAYER";   // filas después de las capas (theory/12 §3e rev. 7)
const char* const CUSTOM_CHIP = "+ CUSTOM";

/// Una tarjeta de capa: letra, engine, preset; la del foco con borde teal y fondo marcado.
void draw_card(const Painter& p, int x, char letter, uint8_t engine, const char* preset, bool focus) {
    draw_box(p, x, CARD_Y, CARD_W, CARD_H, focus ? TEAL_PLUS : LINE, focus ? TEAL_DIM : SURFACE, focus ? 2 : 1);
    char t[8];
    std::snprintf(t, sizeof(t), "%c", letter);
    text_top(p, x + 14, CARD_Y + 10, t, focus ? WHITE : GRAY, T28);
    text_top(p, x + 52, CARD_Y + 14, engine_info(engine).name, focus ? WHITE : GRAY, T20);
    text_top(p, x + 52, CARD_Y + 46, preset[0] ? preset : "\xE2\x80\x94", focus ? GRAY : MON, T12);
    text_top(p, x + 14, CARD_Y + CARD_H - 22, letter == 'A' ? "P1 PUSH: FOCO" : "P2 PUSH: FOCO", MON, T12);
}

}  // namespace

lv_obj_t* ScreenHybrid::create() {
    _scr = screen_root();
    _top.create(_scr, "HYBRID SYNTH", -1);
    _graph = draw_area(_scr, GX, GY, GW, GH, on_graph_draw, this);
    // Tocar una tarjeta = P1 / P2 push (foco en A / B)
    _tap_a = lv_obj_create(_scr);
    lv_obj_remove_style_all(_tap_a);
    lv_obj_set_pos(_tap_a, GX + CARD_A_X, GY + CARD_Y);
    lv_obj_set_size(_tap_a, CARD_W, CARD_H);
    input_attach_tap(_tap_a, Ctl::FocusA);
    _tap_b = lv_obj_create(_scr);
    lv_obj_remove_style_all(_tap_b);
    lv_obj_set_pos(_tap_b, GX + CARD_B_X, GY + CARD_Y);
    lv_obj_set_size(_tap_b, CARD_W, CARD_H);
    input_attach_tap(_tap_b, Ctl::FocusB);
    _cells.create(_scr);
    _bottom.create(_scr);
    return _scr;
}

void ScreenHybrid::on_graph_draw(lv_event_t* e) {
    auto* self = static_cast<ScreenHybrid*>(lv_event_get_user_data(e));
    if (self->_s) self->draw(Painter::from(e), *self->_s);
}

void ScreenHybrid::draw(const Painter& p, const StudioState& s) const {
    p.fill(0, 0, GW, GH, SURFACE);
    p.frame(0, 0, GW, GH, 1, LINE);
    const auto& h = s.hybrid;

    // Presets de capa (hasta 32): ventana que sigue al cursor de NAV (ámbar); la que suena, con borde.
    // Al final, + SAVE LAYER (fila n) y + CUSTOM (fila n + 1).
    const int n = s.layer_count, cur = std::min<int>(s.ui_cursor, n + 1);
    auto chip_name = [&](int i) { return i < n ? s.layer_names[i] : i == n ? SAVE_CHIP : CUSTOM_CHIP; };
    auto chip_w = [&](int i) { return text_width(chip_name(i), T12B) + 20 + 4; };
    int first = cur, used = chip_w(cur);
    while (first > 0 && used + chip_w(first - 1) < GW / 3) used += chip_w(--first);   // un tercio de contexto atrás
    int x = 12;
    for (int i = first; i <= n + 1; ++i) {
        const int w = chip_w(i) - 4;
        if (x + w > GW - 12) break;
        const bool c = i == cur, loaded = i == h.layer;
        if (c) p.fill(x, 10, w, 24, HYB);
        else draw_box(p, x, 10, w, 24, loaded ? HYB : LINE, loaded ? HYB_DIM : SURFACE, 1);
        text_top(p, x + 10, 16, chip_name(i), c ? BG : loaded ? WHITE : i >= n ? HYB_HI : GRAY, T12B);
        x += w + 4;
    }

    if (h.layer < 0) {
        text_top(p, 24, 90, "SIN CAPA \xC2\xB7 NAV PUSH CARGA LA RESALTADA O ARMA UNA CUSTOM", GRAY, T16);
        return;
    }
    draw_card(p, CARD_A_X, 'A', h.engine_a, h.preset_a, h.focus == 0);
    draw_card(p, CARD_B_X, 'B', h.engine_b, h.preset_b, h.focus == 1);

    // Centro: nombre de la capa, MORPH y B
    const int cx = CARD_A_X + CARD_W + 16, cw = CARD_B_X - 16 - cx;
    text_top(p, cx + (cw - text_width(h.name, T16)) / 2, CARD_Y + 4, h.name, WHITE, T16);
    text_top(p, cx, CARD_Y + 32, "MORPH", MON, T12B);
    char t[24];
    std::snprintf(t, sizeof(t), "%d %%", int(h.morph));
    text_top(p, cx + cw - text_width(t, T12), CARD_Y + 32, t, WHITE, T12);
    p.fill(cx, CARD_Y + 50, cw, 10, TRACK);
    p.fill(cx, CARD_Y + 50, std::lround(cw * std::clamp(h.morph / 100.0f, 0.0f, 1.0f)), 10, TEAL_PLUS);
    text_top(p, cx, CARD_Y + 64, "A", MON, T12);
    text_top(p, cx + cw - 8, CARD_Y + 64, "B", MON, T12);
    std::snprintf(t, sizeof(t), "B %+d ST \xC2\xB7 %d MS", int(h.transpose_b), int(h.delay_b_ms));
    text_top(p, cx + (cw - text_width(t, T12)) / 2, CARD_Y + 86, t, GRAY, T12);

    text_top(p, 24, CARD_Y + CARD_H + 18,
             "B1-B3 OTRA VEZ: A \xE2\x86\x94 B \xC2\xB7 SHIFT+B4: PRESETS DE LA MITAD \xC2\xB7 P1/P2 PUSH: FOCO \xC2\xB7 MODE: SALIR", MON, T12);
}

void ScreenHybrid::update(const StudioState& s) {
    _s = &s;
    _top.update(s);
    const auto& h = s.hybrid;
    const bool sh = s.shift;

    char morph[8], delay[8], transp[8], cut[12], res[8];
    std::snprintf(morph, sizeof(morph), "%d", int(h.morph));
    std::snprintf(delay, sizeof(delay), "%d", int(h.delay_b_ms));
    std::snprintf(transp, sizeof(transp), "%+d", int(h.transpose_b));
    std::snprintf(cut, sizeof(cut), "%d", int(std::lround(s.cutoff_hz)));
    std::snprintf(res, sizeof(res), "%d", int(std::lround(s.resonance * 100)));
    _cells.set(0, true, "MORPH", morph, "%");
    if (sh) _cells.set(1, true, "B TRANSP", transp, "ST");
    else _cells.set(1, true, "B DELAY", delay, "MS");
    _cells.set(2, true, "CUTOFF A+B", cut, "HZ");
    _cells.set(3, true, "RES A+B", res, "%");
    _bottom.set_page_control(0, "MORPH", morph, "%", h.morph / 100.0f);
    if (sh) _bottom.set_page_control(1, "B TRANSP", transp, "ST", (h.transpose_b + 24) / 48.0f);
    else _bottom.set_page_control(1, "B DELAY", delay, "MS", h.delay_b_ms / 35.0f);
    _bottom.set_page_control(2, "CUTOFF", cut, "HZ", 0.0f);
    _bottom.set_page_control(3, "RES", res, "%", s.resonance);
    _bottom.set_mode(s.mode == Mode::Fx, sh, s.mode == Mode::Hybrid);
    _bottom.set_keys(KEYS_HYBRID, -1);
    _bottom.set_nav("CAPA \xC2\xB7 PUSH CARGA");

    char key[192];
    std::snprintf(key, sizeof(key), "%d|%d|%d|%d|%d|%d|%d|%d|%d|%s|%s|%s|%s", h.layer, s.ui_cursor, s.layer_count,
                  h.engine_a, h.engine_b, h.focus, h.morph, h.delay_b_ms, h.transpose_b, h.name, h.preset_a, h.preset_b,
                  s.layer_count ? s.layer_names[s.layer_count - 1] : "");
    if (std::strcmp(key, _last_key) != 0) {
        std::snprintf(_last_key, sizeof(_last_key), "%s", key);
        char t[40];
        std::snprintf(t, sizeof(t), "%s", h.layer >= 0 ? h.name : "SIN CAPA");
        _top.set_page(t);
        lv_obj_invalidate(_graph);
    }
}
