#include "ui/screen_lfo.h"

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
constexpr int WAVE_X = 24, WAVE_W = 512, WAVE_CY = 124, WAVE_AMP = 56;
constexpr int METER_TOP = 64, METER_H = 116, METER_CY = METER_TOP + METER_H / 2;
constexpr uint32_t TRACK = 0x1C1C1C;
const char* const KEYS_SYNTH[4] = {"OSC", "ENV", "LFO / MOD", "PRESET"};
const char* const DESTS[LFO_DESTS] = {"CUTOFF", "PITCH", "AMP", "PWM"};
const char* const DIVS[4] = {"1/4", "1/8", "1/16", "1/8T"};
constexpr float DIV_BEATS[4] = {1.0f, 0.5f, 0.25f, 1.0f / 3.0f};
const Wave SHAPES[5] = {Wave::Sine, Wave::Tri, Wave::Saw, Wave::Sqr, Wave::SampleHold};

Wave shape_wave(LfoShape s) { return SHAPES[int(s)]; }
float window_cycles(LfoShape s) { return s == LfoShape::SampleHold ? 8.0f : 2.0f; }

/// Profundidad del destino: CUTOFF es el P2 de PERFORM (mismo valor, diseño 12a).
float depth_of(const StudioState& s, int dest) { return dest == 0 ? s.p2 : s.lfo_depth[dest]; }

float rate_hz(const StudioState& s) {
    return s.lfo_sync ? s.bpm / 60.0f / DIV_BEATS[s.lfo_div % 4] : s.lfo_rate_hz;
}

void fmt_depth(const StudioState& s, int dest, char* out, size_t n) {
    if (dest == 1) std::snprintf(out, n, "%d", int(std::lround(depth_of(s, dest))));
    else std::snprintf(out, n, "%d", int(std::lround(depth_of(s, dest) * 100)));
}

}  // namespace

lv_obj_t* ScreenLfo::create() {
    _scr = screen_root();
    _top.create(_scr, "LFO", -1);
    _graph = draw_area(_scr, GX, GY, GW, GH, on_graph_draw, this);
    input_attach_tap(_graph, Ctl::NavPush);  // atajo táctil: tocar la gráfica = NAV push (SYNC)
    _cells.create(_scr);
    _bottom.create(_scr);
    return _scr;
}

ScreenLfo::Meter ScreenLfo::meter(const StudioState& s) const {
    Meter m{};
    const int dest = s.lfo_dest % LFO_DESTS;
    const float d = depth_of(s, dest);
    if (dest == 0) {  // el medidor cubre 20 Hz–20 kHz en escala log
        const float frac = std::clamp(std::log(s.cutoff_hz / 20.0f) / std::log(1000.0f), 0.0f, 1.0f);
        m.by = METER_TOP + METER_H - int(std::lround(frac * METER_H));
        m.bd = int(std::lround(d * METER_H));
        std::snprintf(m.hi, sizeof(m.hi), "%d", int(std::lround(20 * std::pow(1000.0f, std::min(1.0f, frac + d)))));
        std::snprintf(m.base, sizeof(m.base), "%d HZ", int(std::lround(s.cutoff_hz)));
        std::snprintf(m.lo, sizeof(m.lo), "%d", int(std::lround(20 * std::pow(1000.0f, std::max(0.0f, frac - d)))));
        std::snprintf(m.dv, sizeof(m.dv), "%d %%", int(std::lround(d * 100)));
    } else if (dest == 1) {  // ±12 ST en la altura del medidor
        m.by = METER_CY;
        m.bd = int(std::lround(d * (METER_H / 2.0f) / 12.0f));
        std::snprintf(m.hi, sizeof(m.hi), "+%d ST", int(std::lround(d)));
        std::snprintf(m.base, sizeof(m.base), "0 ST");
        std::snprintf(m.lo, sizeof(m.lo), "−%d ST", int(std::lround(d)));
        std::snprintf(m.dv, sizeof(m.dv), "%d ST", int(std::lround(d)));
    } else {
        m.by = METER_CY;
        m.bd = int(std::lround(d * METER_H / 2));
        std::snprintf(m.hi, sizeof(m.hi), "+%d %%", int(std::lround(d * 100)));
        std::snprintf(m.base, sizeof(m.base), dest == 2 ? "LEVEL" : "50 %%");
        std::snprintf(m.lo, sizeof(m.lo), "−%d %%", int(std::lround(d * 100)));
        std::snprintf(m.dv, sizeof(m.dv), "%d %%", int(std::lround(d * 100)));
    }
    return m;
}

void ScreenLfo::on_graph_draw(lv_event_t* e) {
    auto* self = static_cast<ScreenLfo*>(lv_event_get_user_data(e));
    if (self->_s) self->draw(Painter::from(e), *self->_s);
}

void ScreenLfo::draw(const Painter& p, const StudioState& s) const {
    const bool sh = s.shift;
    const int dest = s.lfo_dest % LFO_DESTS;
    p.fill(0, 0, GW, GH, SURFACE);
    p.frame(0, 0, GW, GH, 1, LINE);

    // Fichas de forma y de destino; con SHIFT, la selección pasa a TEAL_PLUS sólido.
    for (int i = 0; i < 5; ++i) {
        const int x = 16 + i * 52;
        const bool on = i == int(s.lfo_shape);
        if (on) p.fill(x, 8, 48, 24, sh ? TEAL_PLUS : TEAL_DIM);
        else draw_box(p, x, 8, 48, 24, LINE, SURFACE, 1);
        draw_wave_icon(p, SHAPES[i], x + 12, 12, on ? (sh ? BG : TEAL_PLUS) : sh ? GRAY : MON);
    }
    for (int i = 0; i < LFO_DESTS; ++i) {
        const int x = 480 + i * 68;
        const bool on = i == dest;
        if (on) p.fill(x, 8, 64, 24, sh ? TEAL_PLUS : TEAL_DIM);
        else draw_box(p, x, 8, 64, 24, LINE, SURFACE, 1);
        text_top(p, x + 8, 14, DESTS[i], on ? (sh ? BG : WHITE) : sh ? GRAY : MON, T12B);
        if (depth_of(s, i) > 0.0f) p.fill(x + 56, 12, 4, 4, on ? (sh ? BG : TEAL_PLUS) : TEAL);
    }

    char rv[16], rs[32];
    const float hz = rate_hz(s);
    if (s.lfo_sync) {
        std::snprintf(rv, sizeof(rv), "%s", DIVS[s.lfo_div % 4]);
        std::snprintf(rs, sizeof(rs), "SYNC %d BPM", int(std::lround(s.bpm)));
    } else {
        std::snprintf(rv, sizeof(rv), "%.2f HZ", double(hz));
        std::snprintf(rs, sizeof(rs), "FREE · %.2f S", double(1.0f / hz));
    }
    text_top(p, 288, 12, rv, WHITE, T16);
    text_top(p, 288 + text_width(rv, T16) + 8, 14, rs, MON, T12);

    // Forma: 2 ciclos (8 escalones en S&H), con líneas de ciclo y eje en segundos o en beats.
    const Wave w = shape_wave(s.lfo_shape);
    const float cyc = window_cycles(s.lfo_shape);
    const float step = WAVE_W / cyc;
    p.fill(WAVE_X, WAVE_CY, WAVE_W, 1, LINE);
    for (int i = 0; i <= int(cyc); ++i) p.fill(WAVE_X + std::lround(i * step), 44, 1, 148, LINE);
    char t[24];
    if (s.lfo_sync) {
        const int beats = std::max(1, int(cyc * DIV_BEATS[s.lfo_div % 4]));
        for (int b = 0; b < beats; ++b) {
            const int bx = WAVE_X + int(std::lround(b * float(WAVE_W) / beats));
            p.fill(bx, 44, 2, 8, WHITE);
            std::snprintf(t, sizeof(t), "BEAT %d", b + 1);
            text_top(p, bx + 4, 186, t, MON, T12);
        }
    } else {
        text_top(p, 28, 186, "0", MON, T12);
        std::snprintf(t, sizeof(t), "%.2f S", double(cyc / 2 / hz));
        text_top(p, 284, 186, t, MON, T12);
        std::snprintf(t, sizeof(t), "%.2f S", double(cyc / hz));
        text_top(p, 492, 186, t, MON, T12);
    }
    draw_trace(p, w, WAVE_X, WAVE_CY, WAVE_W, WAVE_AMP, cyc, 0, TEAL_PLUS, 2, 2);

    // Medidor del destino: banda ±DEPTH alrededor del valor base.
    const Meter m = meter(s);
    std::snprintf(t, sizeof(t), "→ %s", DESTS[dest]);
    text_top(p, 560, 40, t, TEAL_PLUS, T12B);
    p.fill(568, METER_TOP, 12, METER_H, TRACK);
    const int b0 = std::max(METER_TOP, m.by - m.bd), b1 = std::min(METER_TOP + METER_H, m.by + m.bd);
    p.fill(568, b0, 12, b1 - b0, TEAL_DIM);
    p.fill(564, m.by - 1, 20, 2, GRAY);
    text_top(p, 592, m.by - m.bd - 6, m.hi, GRAY, T12);
    text_top(p, 592, m.by - 6, m.base, WHITE, T12B);
    text_top(p, 592, m.by + m.bd - 6, m.lo, GRAY, T12);
    std::snprintf(t, sizeof(t), "DEPTH %s", m.dv);
    text_top(p, 560, 186, t, MON, T12);

    // Punto de fase y marca viva del medidor (lo único que se anima).
    p.fill(WAVE_X + _dot_x - 4, _dot_y - 4, 8, 8, WHITE);
    p.fill(564, _mark_y - 2, 20, 4, WHITE);
}

void ScreenLfo::invalidate_local(int x, int y, int w, int h) {
    lv_area_t a;
    lv_obj_get_coords(_graph, &a);
    const lv_area_t r{lv_coord_t(a.x1 + x), lv_coord_t(a.y1 + y), lv_coord_t(a.x1 + x + w - 1), lv_coord_t(a.y1 + y + h - 1)};
    lv_obj_invalidate_area(_graph, &r);
}

void ScreenLfo::update(const StudioState& s) {
    _s = &s;
    _top.update(s);
    const bool sh = s.shift;
    const int dest = s.lfo_dest % LFO_DESTS;

    if (s.engine != _last_engine) {
        _last_engine = s.engine;
        char t[32];
        std::snprintf(t, sizeof(t), "LFO · %s", engine_info(s.engine).name);
        _top.set_page(t);
    }

    // Celdas y fila 2: RATE / DEPTH, con SHIFT SHAPE / DEST.
    char rate[16], depth[16];
    if (s.lfo_sync) std::snprintf(rate, sizeof(rate), "%s", DIVS[s.lfo_div % 4]);
    else std::snprintf(rate, sizeof(rate), "%.2f", double(s.lfo_rate_hz));
    fmt_depth(s, dest, depth, sizeof(depth));
    const char* rate_u = s.lfo_sync ? "SYNC" : "HZ";
    const char* depth_u = dest == 1 ? "ST" : "%";
    const Wave w = shape_wave(s.lfo_shape);
    _cells.set(0, !sh, "RATE", rate, rate_u);
    _cells.set(1, !sh, "DEPTH", depth, depth_u);
    _cells.set(2, sh, "SHAPE", wave_name(w), "", w);
    _cells.set(3, sh, "DEST", DESTS[dest], "");

    _bottom.set_mode(s.mode == Mode::Fx, sh);
    _bottom.set_filter_encs(s.cutoff_hz, s.resonance);
    _bottom.set_keys(KEYS_SYNTH, 2);
    _bottom.set_nav("PUSH SYNC");
    if (sh) {
        _bottom.set_param(1, "SHAPE", wave_name(w), "", int(s.lfo_shape) / 4.0f);
        _bottom.set_param(2, "DEST", DESTS[dest], "", dest / 3.0f);
    } else {
        const float rf = s.lfo_sync ? (s.lfo_div % 4) / 3.0f : std::log(s.lfo_rate_hz / 0.05f) / std::log(400.0f);
        _bottom.set_param(1, "RATE", rate, rate_u, rf);
        _bottom.set_param(2, "DEPTH", depth, depth_u, dest == 1 ? depth_of(s, 1) / 12 : depth_of(s, dest));
    }

    // Lo estático se redibuja solo si cambia; el punto y la marca, invalidando su rect viejo y nuevo.
    char key[96];
    std::snprintf(key, sizeof(key), "%d|%d|%d|%d|%d|%.2f|%d|%ld|%.1f|%.2f|%.2f", sh, int(s.lfo_shape), dest, s.lfo_sync,
                  s.lfo_div, double(s.lfo_rate_hz), int(std::lround(s.bpm)), std::lround(depth_of(s, dest) * 100),
                  double(s.lfo_depth[1]), double(s.lfo_depth[2]), double(s.lfo_depth[3]));
    const bool full = std::strcmp(key, _last_key) != 0;
    if (full) std::snprintf(_last_key, sizeof(_last_key), "%s", key);
    // El cutoff (base del medidor) cambia seguido: solo se repinta la zona del medidor.
    const int cut = dest == 0 ? int(std::lround(s.cutoff_hz)) : 0;
    const bool meter_changed = cut != _last_cut;
    _last_cut = cut;

    const float cyc = window_cycles(s.lfo_shape);
    const float ph = std::fmod(s.lfo_phase, cyc);
    const float v = wave_value(w, ph);
    const Meter m = meter(s);
    const int dot_x = int(std::floor(ph / cyc * WAVE_W / 2)) * 2;
    const int dot_y = int(std::lround(WAVE_CY - v * WAVE_AMP));
    const int mark_y = int(std::lround(m.by - v * m.bd));

    if (full) {
        _dot_x = dot_x; _dot_y = dot_y; _mark_y = mark_y;
        lv_obj_invalidate(_graph);
        return;
    }
    if (meter_changed) invalidate_local(556, 40, GW - 556 - 1, GH - 41);
    if (dot_x != _dot_x || dot_y != _dot_y) {
        invalidate_local(WAVE_X + _dot_x - 4, _dot_y - 4, 8, 8);
        _dot_x = dot_x; _dot_y = dot_y;
        invalidate_local(WAVE_X + _dot_x - 4, _dot_y - 4, 8, 8);
    }
    if (mark_y != _mark_y) {
        invalidate_local(564, _mark_y - 2, 20, 4);
        _mark_y = mark_y;
        invalidate_local(564, _mark_y - 2, 20, 4);
    }
}
