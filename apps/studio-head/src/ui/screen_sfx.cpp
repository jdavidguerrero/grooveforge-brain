#include "ui/screen_sfx.h"

#include <cmath>
#include <cstdio>
#include <cstring>

#include <gf_params.h>

#include "ui/edit_draw.h"
#include "ui/engines.h"
#include "ui/theme.h"

using namespace gf;

namespace {

constexpr int GX = 16, GY = 48, GW = 768, GH = 208;
constexpr int CARD_X = 16, CARD_Y = 20, CARD_W = 166, CARD_H = 168, CARD_STEP = 188;
constexpr uint32_t TRACK = 0x1C1C1C;
const char* const KEYS_SYNTH[4] = {"OSC", "ENV", "LFO / MOD", "SYNTH FX"};
const char* const NAME[GF_SFX_COUNT] = {"BIT", "TAPE", "CHORUS", "REVERB"};   // caben junto al ON/OFF
const char* const MATERIAL[4] = {"BELL", "GUADUA", "GLASS", "WOOD"};
const char* const SIZE[3] = {"S", "M", "L"};

}  // namespace

void sfx_format(int fx, int k, float n, char* val, size_t sz, const char** label, const char** unit) {
    static const char* const LBL[GF_SFX_COUNT][GF_SFX_PARAMS] = {
        {"BITS", "SR", "SCULPT"}, {"DRIVE", "WOW", "FLUTTER"}, {"DEPTH", "RATE", "DRIFT"}, {"DECAY", "MATERIAL", "SIZE"}};
    *label = LBL[fx][k];
    *unit = "%";
    const int pct = int(std::lround(n * 100));
    switch (fx * 3 + k) {
        case 0: std::snprintf(val, sz, "%d", gf_sfx_bits(n)); *unit = ""; break;
        case 1: std::snprintf(val, sz, "%.1f", double(gf_sfx_sample_rate(n) / 1000.0f)); *unit = "KHZ"; break;
        case 3: std::snprintf(val, sz, "%.1f", double(gf_sfx_drive(n))); *unit = ""; break;
        case 7: std::snprintf(val, sz, "%.2f", double(gf_sfx_chorus_rate(n))); *unit = "HZ"; break;
        case 9: std::snprintf(val, sz, "x%.1f", double(gf_sfx_decay(n))); *unit = ""; break;
        case 10: std::snprintf(val, sz, "%s", MATERIAL[gf_index_from_norm(n, 4)]); *unit = ""; break;
        case 11: std::snprintf(val, sz, "%s", SIZE[gf_index_from_norm(n, 3)]); *unit = ""; break;
        default: std::snprintf(val, sz, "%d", pct); break;
    }
}

lv_obj_t* ScreenSfx::create() {
    _scr = screen_root();
    _top.create(_scr, "SYNTH FX", -1);
    _graph = draw_area(_scr, GX, GY, GW, GH, on_graph_draw, this);
    _cells.create(_scr);
    _bottom.create(_scr);
    return _scr;
}

void ScreenSfx::on_graph_draw(lv_event_t* e) {
    auto* self = static_cast<ScreenSfx*>(lv_event_get_user_data(e));
    if (self->_s) self->draw(Painter::from(e), *self->_s);
}

void ScreenSfx::draw(const Painter& p, const StudioState& s) const {
    p.fill(0, 0, GW, GH, SURFACE);
    p.frame(0, 0, GW, GH, 1, LINE);
    const int sel = s.ui_cursor % GF_SFX_COUNT;
    char t[24], v[16];
    for (int i = 0; i < int(GF_SFX_COUNT); ++i) {
        const int x = CARD_X + i * CARD_STEP;
        const bool on = s.sfx_on[i];
        const bool here = i == sel;
        draw_box(p, x, CARD_Y, CARD_W, CARD_H, here ? TEAL_PLUS : LINE, on ? TEAL_DIM : SURFACE_2, here ? 2 : 1);
        if (i < int(GF_SFX_COUNT) - 1) {                                    // flecha hacia el siguiente FX
            const int ax = x + CARD_W + 4, ay = CARD_Y + CARD_H / 2;
            p.fill(ax, ay - 1, CARD_STEP - CARD_W - 10, 2, on ? TEAL : MON);
            p.line(ax + CARD_STEP - CARD_W - 14, ay - 4, ax + CARD_STEP - CARD_W - 10, ay, 2, on ? TEAL : MON);
            p.line(ax + CARD_STEP - CARD_W - 14, ay + 4, ax + CARD_STEP - CARD_W - 10, ay, 2, on ? TEAL : MON);
        }
        std::snprintf(t, sizeof(t), "%d", i + 1);
        text_top(p, x + 12, CARD_Y + 12, t, on ? TEAL_PLUS : MON, T12B);
        text_top(p, x + 28, CARD_Y + 10, NAME[i], on ? WHITE : GRAY, T16);

        // ON / OFF
        const char* st = on ? "ON" : "OFF";
        const int sw = text_width(st, T12B) + 12;
        if (on) p.fill(x + CARD_W - sw - 10, CARD_Y + 10, sw, 20, TEAL_PLUS);
        else draw_box(p, x + CARD_W - sw - 10, CARD_Y + 10, sw, 20, MON, SURFACE_2, 1);
        text_top(p, x + CARD_W - sw - 4, CARD_Y + 14, st, on ? BG : MON, T12B);

        // Parámetro principal en grande
        const char *lbl, *unit;
        sfx_format(i, 0, s.sfx_p[i][0], v, sizeof(v), &lbl, &unit);
        text_top(p, x + 12, CARD_Y + 48, lbl, MON, T12);
        text_top(p, x + 12, CARD_Y + 66, v, on ? WHITE : MON, T28);
        if (*unit) text_top(p, x + 16 + text_width(v, T28), CARD_Y + 84, unit, MON, T12);

        // MIX
        text_top(p, x + 12, CARD_Y + 122, "MIX", MON, T12);
        std::snprintf(t, sizeof(t), "%d %%", int(std::lround(s.sfx_mix[i] * 100)));
        text_top(p, x + CARD_W - 12 - text_width(t, T12), CARD_Y + 122, t, on ? WHITE : MON, T12);
        p.fill(x + 12, CARD_Y + 142, CARD_W - 24, 8, TRACK);
        p.fill(x + 12, CARD_Y + 142, std::lround((CARD_W - 24) * s.sfx_mix[i]), 8, on ? TEAL_PLUS : MON);
    }
}

void ScreenSfx::update(const StudioState& s) {
    _s = &s;
    _top.update(s);
    const bool sh = s.shift;
    const int fx = s.ui_cursor % GF_SFX_COUNT;

    if (s.engine != _last_engine) {
        _last_engine = s.engine;
        char t[32];
        std::snprintf(t, sizeof(t), "SYNTH FX · %s", engine_info(s.engine).name);
        _top.set_page(t);
    }

    char v[16], mix[16];
    const char *lbl, *unit;
    std::snprintf(mix, sizeof(mix), "%d", int(std::lround(s.sfx_mix[fx] * 100)));
    // Igual que el modo FX (05 §1.13): P1 / P2 usuario · ENC L MIX · ENC R principal
    static const int CELL_K[4] = {1, 2, -1, 0};          // -1 = MIX
    for (int c = 0; c < 4; ++c) {
        const int k = CELL_K[c];
        if (k < 0) {
            _cells.set(c, !sh, "MIX", mix, "%");
            _bottom.set_page_control(c, "MIX", mix, "%", s.sfx_mix[fx]);
            continue;
        }
        sfx_format(fx, k, s.sfx_p[fx][k], v, sizeof(v), &lbl, &unit);
        _cells.set(c, !sh, lbl, v, unit);
        _bottom.set_page_control(c, lbl, v, unit, s.sfx_p[fx][k]);
    }

    _bottom.set_mode(s.mode == Mode::Fx, sh);
    _bottom.set_keys(KEYS_SYNTH, 3);
    _bottom.set_nav(s.sfx_on[fx] ? "SELECT · OFF" : "SELECT · ON");

    char key[160];
    int n = std::snprintf(key, sizeof(key), "%d|%d|%d", fx, sh, s.engine);
    for (int i = 0; i < int(GF_SFX_COUNT) && n < int(sizeof(key)) - 24; ++i)
        n += std::snprintf(key + n, sizeof(key) - n, "|%d,%d,%d,%d,%d", s.sfx_on[i], int(s.sfx_mix[i] * 100),
                           int(s.sfx_p[i][0] * 100), int(s.sfx_p[i][1] * 100), int(s.sfx_p[i][2] * 100));
    if (std::strcmp(key, _last_key) != 0) {
        std::snprintf(_last_key, sizeof(_last_key), "%s", key);
        lv_obj_invalidate(_graph);
    }
}
