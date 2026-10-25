#include "ui/screen_groove.h"

#include <cmath>
#include <cstdio>
#include <cstring>

#include "state/osc_catalog.h"
#include "ui/edit_draw.h"
#include "ui/engines.h"
#include "ui/input.h"
#include "ui/theme.h"

using namespace gf;

namespace {

constexpr int GX = 16, GY = 48, GW = 768, GH = 208;
constexpr int BPM_X = 568;                       // columna del reloj, a la derecha de los slots
constexpr uint32_t TRACK = 0x1C1C1C;
const char* const KEYS_GROOVE[4] = {"OSC", "ENV", "LFO / MOD", "TAP"};
const char* const SRC_NAME[4] = {"AUTO", "MIDI", "AI", "INT"};

bool has_toggle(int stop) { return stop < GROOVE_TOGGLES; }

}  // namespace

lv_obj_t* ScreenGroove::create() {
    _scr = screen_root();
    _top.create(_scr, "GROOVE", -1);
    _graph = draw_area(_scr, GX, GY, GW, GH, on_graph_draw, this);
    input_attach_tap(_graph, Ctl::NavPush);   // tocar la gráfica = NAV push (enciende / apaga la parada)
    _cells.create(_scr);
    _bottom.create(_scr);
    return _scr;
}

void ScreenGroove::on_graph_draw(lv_event_t* e) {
    auto* self = static_cast<ScreenGroove*>(lv_event_get_user_data(e));
    if (self->_s) self->draw(Painter::from(e), *self->_s);
}

void ScreenGroove::draw(const Painter& p, const StudioState& s) const {
    p.fill(0, 0, GW, GH, SURFACE);
    p.frame(0, 0, GW, GH, 1, LINE);

    // Paradas: la elegida en TEAL_PLUS; las encendidas con un punto
    int x = 16;
    for (int i = 0; i < GROOVE_STOPS; ++i) {
        const char* name = groove_stop(i).name;
        const bool on = has_toggle(i) && s.groove_on[i];
        const int w = text_width(name, T12B) + (has_toggle(i) ? 36 : 24);
        if (i == _stop) p.fill(x, 8, w, 24, TEAL_PLUS);
        else draw_box(p, x, 8, w, 24, on ? TEAL_PLUS : LINE, on ? TEAL_DIM : SURFACE, 1);
        text_top(p, x + 12, 14, name, i == _stop ? BG : on ? WHITE : GRAY, T12B);
        if (has_toggle(i)) p.fill(x + w - 18, 16, 8, 8, on ? (i == _stop ? BG : TEAL_PLUS) : LINE);
        x += w + 8;
    }

    // Slots de la parada: dos filas (P1 · P2 · ENC L · ENC R | SHIFT P1 · SHIFT P2); la capa visible, en blanco
    const ModStop& m = groove_stop(_stop);
    const OscEngine& oe = osc_engine(s.engine);
    const float* v = s.groove_val[_stop];
    constexpr int CELL_W = 128, BAR_W = 112;
    char t[32], val[20];
    for (int k = 0; k < OSC_SLOTS; ++k) {
        const OscSlot* sl = m.slot[k];
        if (!sl) continue;
        const bool layer = (k >= 4) == s.shift;
        const int cx = 24 + (k % 4) * CELL_W, cy = 56 + (k / 4) * 64;
        text_top(p, cx, cy, sl->name, layer ? GRAY : MON, T12B);
        osc_format(oe, *sl, v[k], val, sizeof(val));
        std::snprintf(t, sizeof(t), sl->unit[0] ? "%s %s" : "%s", val, sl->unit);
        text_top(p, cx, cy + 18, t, layer ? WHITE : MON, T16);
        p.fill(cx, cy + 42, BAR_W, 8, TRACK);
        p.fill(cx, cy + 42, std::lround(BAR_W * osc_frac(oe, *sl, v[k])), 8, layer ? TEAL_PLUS : TEAL_DIM);
    }

    // Reloj común: BPM grande y la fuente elegida en PUMP
    p.fill(BPM_X - 16, 44, 1, GH - 56, LINE);
    text_top(p, BPM_X, 52, "CLOCK", MON, T12B);
    std::snprintf(t, sizeof(t), "%.1f", double(s.bpm));
    text_top(p, BPM_X, 70, t, WHITE, T28);
    text_top(p, BPM_X + text_width(t, T28) + 8, 82, "BPM", MON, T12);
    const int src = int(std::lround(s.groove_val[0][5])) & 3;
    std::snprintf(t, sizeof(t), "SOURCE %s", SRC_NAME[src]);
    text_top(p, BPM_X, 112, t, GRAY, T12);
    text_top(p, BPM_X, 136, "B4: TAP", MON, T12);
    const char* hint = _stop == 2 ? "PUSH + KEYS: LEARN" : has_toggle(_stop) ? "PUSH: ON / OFF" : "CC 14-17";
    text_top(p, BPM_X, 156, hint, MON, T12);
}

void ScreenGroove::update(const StudioState& s) {
    _s = &s;
    _top.update(s);
    _stop = s.ui_cursor < GROOVE_STOPS ? s.ui_cursor : 0;
    const bool sh = s.shift;
    const ModStop& m = groove_stop(_stop);
    const OscEngine& e = osc_engine(s.engine);
    const float* v = s.groove_val[_stop];

    char val[16];
    for (int k = 0; k < 4; ++k) {
        const int slot = (sh ? 4 : 0) + k;
        const OscSlot* sl = m.slot[slot];
        if (!sl) {
            _cells.set_unused(k);
            _bottom.set_page_control(k, "NO FUNCTION", "\xE2\x80\x94", "", 0.0f);
            continue;
        }
        osc_format(e, *sl, v[slot], val, sizeof(val));
        _cells.set(k, true, sl->name, val, sl->unit);
        _bottom.set_page_control(k, sl->name, val, sl->unit, osc_frac(e, *sl, v[slot]));
    }
    _bottom.set_mode(s.mode == Mode::Fx, sh);
    _bottom.set_keys(KEYS_GROOVE, 2);
    _bottom.set_nav("STOP · ON");

    char key[160];
    int n = std::snprintf(key, sizeof(key), "G%d|%d|%d|%.1f|", _stop, sh, s.engine, double(s.bpm));
    for (int i = 0; i < GROOVE_TOGGLES; ++i) n += std::snprintf(key + n, sizeof(key) - n, "%d", s.groove_on[i]);
    for (int k = 0; k < OSC_SLOTS && n < int(sizeof(key)) - 8; ++k)
        n += std::snprintf(key + n, sizeof(key) - n, ",%.0f", double(v[k]));
    if (std::strcmp(key, _last_key) != 0) {
        std::snprintf(_last_key, sizeof(_last_key), "%s", key);
        char t[40];
        std::snprintf(t, sizeof(t), "%s · %s", m.name, engine_info(s.engine).name);
        _top.set_page(t);
        lv_obj_invalidate(_graph);
    }
}
