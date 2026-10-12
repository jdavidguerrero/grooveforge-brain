#include "ui/screen_osc.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

#include "ui/edit_draw.h"
#include "ui/engines.h"
#include "ui/theme.h"

using namespace gf;

namespace {

constexpr int GX = 16, GY = 48, GW = 768, GH = 208;
constexpr uint32_t ROW_LINE = 0x1C1C1C;
const char* const KEYS_SYNTH[4] = {"OSC", "ENV", "LFO / MOD", "PRESET"};

// ── Plantilla: fila de selección + pistas (diseño: selRow, lanes) ──────────────

struct Lane {
    const char* name;
    char sub[16];
    Wave wave;
    float level;   ///< 0–1
    float cycles;  ///< ciclos en la ventana de 464 px (VCO 1 = 2)
    int off;       ///< corrimiento en px (detune, 2 px por cent)
};

float interval_cycles(float st) { return 2.0f * std::pow(2.0f, st / 12.0f); }

void fmt_signed(float v, const char* unit, char* out, size_t n) {
    const int iv = int(std::lround(v));
    if (iv > 0) std::snprintf(out, n, "+%d %s", iv, unit);
    else if (iv < 0) std::snprintf(out, n, "−%d %s", -iv, unit);
    else std::snprintf(out, n, "0 %s", unit);
}

/// Pistas que dibuja cada engine, en el orden de la fila de selección.
int build_lanes(const StudioState& s, uint8_t eng, Lane* L) {
    const OscEngine& e = osc_engine(eng);
    auto val = [&](int stop, int slot) { return osc_value(s, eng, stop, slot); };
    auto wave = [&](int stop) { return e.waves[std::clamp(int(val(stop, 0)), 0, e.wave_count - 1)]; };
    auto lane = [](Lane& l, const char* name, Wave w, float lv, float cyc, int off = 0) {
        l.name = name; l.wave = w; l.level = std::clamp(lv, 0.0f, 1.0f); l.cycles = cyc; l.off = off; l.sub[0] = '\0';
    };
    switch (eng) {
        case 0:  // MODEL D
            lane(L[0], "VCO 1", wave(0), val(0, 2) / 100, 2);
            std::snprintf(L[0].sub, sizeof(L[0].sub), "REF");
            lane(L[1], "VCO 2", wave(1), val(1, 2) / 100, 2, int(std::lround(val(1, 1) * 2)));
            fmt_signed(val(1, 1), "CT", L[1].sub, sizeof(L[1].sub));
            lane(L[2], "VCO 3", wave(2), val(2, 2) / 100, interval_cycles(val(2, 1)));
            fmt_signed(val(2, 1), "ST", L[2].sub, sizeof(L[2].sub));
            lane(L[3], "NOISE", Wave::Noise, val(0, 3) / 100, 1);
            std::snprintf(L[3].sub, sizeof(L[3].sub), "WHITE");
            return 4;
        case 1:  // JUNO-106
            lane(L[0], "DCO", wave(0), 1.0f, 2);
            std::snprintf(L[0].sub, sizeof(L[0].sub), "1 OSC");
            lane(L[1], "SUB", Wave::Sqr, val(0, 1) / 100, 1);
            std::snprintf(L[1].sub, sizeof(L[1].sub), "−1 OCT");
            lane(L[2], "NOISE", Wave::Noise, val(0, 2) / 100, 1);
            std::snprintf(L[2].sub, sizeof(L[2].sub), "WHITE");
            return 3;
        case 2: {  // PROPHET-5: MIX A/B reparte el nivel
            const float mix = val(0, 3) / 100;
            lane(L[0], "OSC A", wave(0), mix, 2);
            std::snprintf(L[0].sub, sizeof(L[0].sub), "XMOD %d%%", int(std::lround(val(0, 2))));
            lane(L[1], "OSC B", wave(1), 1 - mix, interval_cycles(val(1, 1)));
            fmt_signed(val(1, 1), "ST", L[1].sub, sizeof(L[1].sub));
            return 2;
        }
        case 3:  // OB-6
            lane(L[0], "VCO 1", wave(0), val(0, 2) / 100, 2);
            std::snprintf(L[0].sub, sizeof(L[0].sub), "REF");
            lane(L[1], "VCO 2", wave(1), val(1, 2) / 100, interval_cycles(val(1, 1)));
            fmt_signed(val(1, 1), "ST", L[1].sub, sizeof(L[1].sub));
            lane(L[2], "SUB", Wave::Sqr, val(0, 1) / 100, 1);
            std::snprintf(L[2].sub, sizeof(L[2].sub), "−1 OCT");
            lane(L[3], "NOISE", Wave::Noise, val(0, 3) / 100, 1);
            std::snprintf(L[3].sub, sizeof(L[3].sub), "WHITE");
            return 4;
        case 5: {  // ARP 2600: el ring mod es el producto de los dos VCO
            const float ratio = val(1, 1);
            lane(L[0], "VCO 1", wave(0), 0.8f, 2);
            std::snprintf(L[0].sub, sizeof(L[0].sub), "REF");
            lane(L[1], "VCO 2", wave(1), 0.7f, 2 * ratio);
            std::snprintf(L[1].sub, sizeof(L[1].sub), "×%.2f", double(ratio));
            lane(L[2], "RING", Wave::Sine, val(0, 2) / 100, 2 * (1 + ratio));
            std::snprintf(L[2].sub, sizeof(L[2].sub), "VCO1×VCO2");
            lane(L[3], "NOISE", Wave::Noise, val(0, 3) / 100, 1);
            std::snprintf(L[3].sub, sizeof(L[3].sub), "WHITE");
            return 4;
        }
        default:
            return 0;
    }
}

struct Chip { const char* name; bool stop; bool has_wave; Wave wave; };

void sel_row(const Painter& p, const Chip* items, int n, int sel, const char* hint) {
    int x = 16;
    for (int i = 0; i < n; ++i) {
        const bool on = i == sel, stop = items[i].stop;
        const int w = stop ? 104 : 72;
        if (stop) {
            if (on) p.fill(x, 8, w, 24, TEAL_PLUS);
            else draw_box(p, x, 8, w, 24, LINE, SURFACE, 1);
        }
        text_top(p, x + 10, 14, items[i].name, on ? BG : stop ? GRAY : MON, T12B);
        if (items[i].has_wave) draw_wave_icon(p, items[i].wave, x + w - 34, 12, on ? BG : GRAY);
        x += w + 8;
    }
    if (hint) text_top(p, x + 24, 14, hint, MON, T12);
}

void lanes(const Painter& p, const Lane* L, int n, int sel) {
    const int lh = std::min(64, 160 / n);
    for (int x : {120, 352, 584}) p.fill(x, 40, 1, n * lh, LINE);
    for (int i = 0; i < n; ++i) {
        const Lane& o = L[i];
        const int top = 40 + i * lh, cy = top + (lh >> 1);
        const bool on = i == sel;
        const uint32_t c = on ? TEAL_PLUS : TEAL;
        if (i) p.fill(16, top, 736, 1, ROW_LINE);
        if (on) p.fill(16, top + 4, 4, lh - 8, TEAL_PLUS);
        text_top(p, 28, cy - 13, o.name, on ? TEAL_PLUS : GRAY, T12B);
        text_top(p, 28, cy + 3, o.sub, on ? WHITE : MON, T12);
        const int amp = std::max(3, int(std::lround((lh / 2.0f - 6) * std::max(0.25f, o.level))));
        draw_trace(p, o.wave, 120, cy, 464, amp, o.cycles, o.off, c, 2, 2);
        if (o.off) p.fill(120, top + 3, std::abs(o.off), 2, WHITE);
        p.fill(600, cy - 4, 112, 8, ROW_LINE);
        p.fill(600, cy - 4, std::lround(112 * o.level), 8, c);
        char t[8];
        std::snprintf(t, sizeof(t), "%d", int(std::lround(o.level * 100)));
        text_top(p, 720, cy - 6, t, on ? WHITE : GRAY, T12B);
    }
    text_top(p, 600, 14, "LEVEL", MON, T12);
}

// ── DX7: algoritmos de 4 operadores (diseño: ALG, diag, tile) ──────────────────

struct Alg {
    float pos[4][2];   ///< columna, fila (fila 0 = portadoras)
    int edges[3][2];   ///< modulador → destino (1-based)
    int n_edges;
};

const Alg ALG[8] = {
    {{{0, 0}, {0, 1}, {0, 2}, {0, 3}}, {{4, 3}, {3, 2}, {2, 1}}, 3},
    {{{.5f, 0}, {.5f, 1}, {0, 2}, {1, 2}}, {{3, 2}, {4, 2}, {2, 1}}, 3},
    {{{.5f, 0}, {0, 1}, {0, 2}, {1, 1}}, {{3, 2}, {2, 1}, {4, 1}}, 3},
    {{{.5f, 0}, {0, 1}, {1, 1}, {1, 2}}, {{4, 3}, {3, 1}, {2, 1}}, 3},
    {{{0, 0}, {0, 1}, {1, 0}, {1, 1}}, {{2, 1}, {4, 3}}, 2},
    {{{0, 0}, {1, 0}, {2, 0}, {1, 1}}, {{4, 1}, {4, 2}, {4, 3}}, 3},
    {{{0, 0}, {1, 0}, {2, 0}, {2, 1}}, {{4, 3}}, 1},
    {{{0, 0}, {1, 0}, {2, 0}, {3, 0}}, {}, 0},
};

struct DiagStyle {
    int ox, by, bw, bh, cp, rp, lt, fo, bus, bus_end;
    uint32_t lc, car, mod_fill, sel_fill;
    int sel, b;
    bool txt;
};

struct DiagOut { int carriers, fb_x, fb_y; };

DiagOut diagram(const Painter& p, int alg, const DiagStyle& o) {
    const Alg& A = ALG[std::clamp(alg, 1, 8) - 1];
    const int hl = o.lt >> 1;
    int px[4], py[4], pr[4];
    for (int k = 0; k < 4; ++k) {
        px[k] = int(std::lround(o.ox + A.pos[k][0] * o.cp));
        pr[k] = int(A.pos[k][1]);
        py[k] = o.by - pr[k] * o.rp;
    }
    auto cx = [&](int k) { return px[k - 1] + (o.bw >> 1); };
    for (int i = 0; i < A.n_edges; ++i) {  // abajo, al costado, abajo
        const int m = A.edges[i][0], d = A.edges[i][1];
        const int mx = cx(m), mb = py[m - 1] + o.bh, dx = cx(d), tt = py[d - 1], my = (mb + tt + 1) / 2;
        p.fill(mx - hl, mb, o.lt, my - mb, o.lc);
        p.fill(std::min(mx, dx) - hl, my - hl, std::abs(dx - mx) + o.lt, o.lt, o.lc);
        p.fill(dx - hl, my - hl, o.lt, tt - my + hl, o.lc);
    }
    // Lazo de feedback del OP 4.
    const int x1 = px[3] + o.bw, ym = py[3] + (o.bh >> 1), c4 = cx(4), top = py[3] - o.fo;
    p.fill(x1, ym - hl, o.fo, o.lt, o.lc);
    p.fill(x1 + o.fo - hl, top - hl, o.lt, ym - top + o.lt, o.lc);
    p.fill(c4 - hl, top - hl, x1 + o.fo - c4 + hl, o.lt, o.lc);
    p.fill(c4 - hl, top, o.lt, o.fo, o.lc);
    // Bus de salida de las portadoras.
    int carriers = 0, bx0 = 1 << 30, bx1 = -(1 << 30);
    const int bus_y = o.by + o.bh + o.bus;
    for (int k = 1; k <= 4; ++k) {
        if (pr[k - 1] != 0) continue;
        ++carriers;
        p.fill(cx(k) - hl, o.by + o.bh, o.lt, o.bus, o.lc);
        bx0 = std::min(bx0, cx(k));
        bx1 = std::max(bx1, cx(k));
    }
    if (o.bus_end) bx1 = o.bus_end;
    p.fill(bx0 - hl, bus_y - hl, bx1 - bx0 + o.lt, o.lt, o.lc);
    for (int k = 1; k <= 4; ++k) {
        const bool s = k == o.sel, carrier = pr[k - 1] == 0;
        draw_box(p, px[k - 1], py[k - 1], o.bw, o.bh, s ? o.sel_fill : o.lc, s ? o.sel_fill : carrier ? o.car : o.mod_fill, o.b);
        if (o.txt) {
            char t[8];
            std::snprintf(t, sizeof(t), "OP %d", k);
            text_top(p, px[k - 1] + 22, py[k - 1] + 8, t, s ? BG : carrier ? WHITE : GRAY, T12B);
        }
    }
    return {carriers, x1 + o.fo + 6, ym - 6};
}

void alg_tile(const Painter& p, int alg, bool sel, int x, int y) {
    const Alg& A = ALG[alg - 1];
    float maxc = 0;
    for (const auto& q : A.pos) maxc = std::max(maxc, q[0]);
    const int span = int(maxc * 20) + 16;
    const uint32_t lc = sel ? TEAL_PLUS : TEAL, bg = sel ? TEAL_DIM : BG;
    if (sel) {
        p.fill(x, y, 88, 192, bg);
        p.fill(x, y, 88, 4, TEAL_PLUS);
    } else {
        draw_box(p, x, y, 88, 192, LINE, bg, 1);
    }
    const DiagOut d = diagram(p, alg, {x + (88 - span + 1) / 2, y + 130, 16, 12, 20, 22, 1, 4, 8, 0, lc, lc, bg, lc, 0, 1, false});
    char t[12];
    std::snprintf(t, sizeof(t), "%d", alg);
    text_top(p, x + 8, y + 12, t, sel ? WHITE : GRAY, T16);
    std::snprintf(t, sizeof(t), "%d OUT", d.carriers);
    text_top(p, x + 8, y + 164, t, sel ? TEAL_PLUS : MON, T12);
}

void fmt_ratio(float r, char* out, size_t n) { std::snprintf(out, n, "%.2f", double(r)); }

}  // namespace

lv_obj_t* ScreenOsc::create() {
    _scr = screen_root();
    _top.create(_scr, "OSC", -1);
    _graph = draw_area(_scr, GX, GY, GW, GH, on_graph_draw, this);
    _cells.create(_scr);
    _bottom.create(_scr);
    return _scr;
}

void ScreenOsc::move_selection(int dir, const StudioState& s) {
    const int n = osc_engine(s.engine).stops;
    if (n <= 1) return;
    uint8_t& sel = _sel[s.engine < 6 ? s.engine : 0];
    sel = uint8_t((sel + dir + n) % n);
}

void ScreenOsc::on_graph_draw(lv_event_t* e) {
    auto* self = static_cast<ScreenOsc*>(lv_event_get_user_data(e));
    if (!self->_s) return;
    const Painter p = Painter::from(e);
    p.fill(0, 0, GW, GH, SURFACE);
    p.frame(0, 0, GW, GH, 1, LINE);
    if (osc_engine(self->_s->engine).fm) self->draw_fm(p, *self->_s);
    else self->draw_template(p, *self->_s);
}

void ScreenOsc::draw_template(const Painter& p, const StudioState& s) const {
    const OscEngine& e = osc_engine(s.engine);
    Chip chips[5];
    for (int i = 0; i < e.sources; ++i) {
        const bool stop = e.src[i].stop;
        Wave w = Wave::Noise;
        if (stop) w = e.waves[std::clamp(int(osc_value(s, s.engine, i, 0)), 0, e.wave_count - 1)];
        chips[i] = {e.src[i].name, stop, stop, w};
    }
    const int sel = _sel[s.engine];
    sel_row(p, chips, e.sources, sel, e.stops > 1 ? "NAV SELECTS" : "NAV · NO FUNCTION · 1 OSCILLATOR");
    Lane L[5];
    const int n = build_lanes(s, s.engine, L);
    if (n) lanes(p, L, n, sel);
}

void ScreenOsc::draw_fm(const Painter& p, const StudioState& s) const {
    if (s.shift) {  // 11c2: selector de los 8 algoritmos
        for (int i = 0; i < 8; ++i) alg_tile(p, i + 1, i + 1 == s.fm_algorithm, 11 + i * 94, 8);
        return;
    }
    const int sel = _sel[4];
    const Chip chips[4] = {{"OP 1", true, false, Wave::Sine}, {"OP 2", true, false, Wave::Sine},
                           {"OP 3", true, false, Wave::Sine}, {"OP 4", true, false, Wave::Sine}};
    char hint[40];
    std::snprintf(hint, sizeof(hint), "NAV SELECTS · ALG %d · FB %d", s.fm_algorithm, s.fm_feedback);
    sel_row(p, chips, 4, sel, hint);
    const DiagOut d = diagram(p, s.fm_algorithm, {40, 148, 72, 28, 104, 44, 2, 10, 12, 400, TEAL, TEAL_DIM, SURFACE,
                                                  TEAL_PLUS, sel + 1, 2, true});
    char t[16];
    std::snprintf(t, sizeof(t), "FB %d", s.fm_feedback);
    text_top(p, d.fb_x, d.fb_y, t, GRAY, T12B);
    text_top(p, 408, 182, "OUT", MON, T12);
    text_top(p, 456, 40, "LEVEL", MON, T12);
    text_top(p, 704, 40, "RATIO", MON, T12);
    for (int i = 0; i < 4; ++i) {
        const bool on = i == sel;
        const int y = 60 + i * 30;
        const uint32_t c = on ? TEAL_PLUS : TEAL;
        std::snprintf(t, sizeof(t), "OP %d", i + 1);
        text_top(p, 456, y, t, on ? TEAL_PLUS : GRAY, T12B);
        std::snprintf(t, sizeof(t), "%d", s.fm_level[i]);
        text_top(p, 672, y, t, on ? WHITE : GRAY, T12B);
        std::snprintf(t, sizeof(t), "×%.2f", double(s.fm_ratio[i]));
        text_top(p, 704, y, t, on ? WHITE : MON, T12);
        p.fill(504, y + 2, 160, 8, ROW_LINE);
        p.fill(504, y + 2, std::lround(160.0f * s.fm_level[i] / 99), 8, c);
    }
    draw_box(p, 456, 182, 12, 12, TEAL, TEAL_DIM, 2);
    draw_box(p, 560, 182, 12, 12, TEAL, SURFACE, 2);
    text_top(p, 476, 182, "CARRIER", MON, T12);
    text_top(p, 580, 182, "MODULATOR", MON, T12);
}

void ScreenOsc::update(const StudioState& s) {
    _s = &s;
    _top.update(s);
    const OscEngine& e = osc_engine(s.engine);
    const int sel = _sel[s.engine];
    const bool sh = s.shift;

    if (s.engine != _last_engine) {
        _last_engine = s.engine;
        char t[32];
        std::snprintf(t, sizeof(t), "OSC · %s", engine_info(s.engine).name);
        _top.set_page(t);
    }

    _bottom.set_mode(s.mode == Mode::Fx, sh);
    _bottom.set_filter_encs(s.cutoff_hz, s.resonance);
    _bottom.set_keys(KEYS_SYNTH, 0);
    _bottom.set_nav(e.fm ? "OP" : e.stops > 1 ? "OSC" : "NONE");

    char v[4][16];
    if (e.fm) {
        fmt_ratio(s.fm_ratio[sel], v[0], sizeof(v[0]));
        std::snprintf(v[1], sizeof(v[1]), "%d", s.fm_level[sel]);
        std::snprintf(v[2], sizeof(v[2]), "%d", s.fm_algorithm);
        std::snprintf(v[3], sizeof(v[3]), "%d", s.fm_feedback);
        _cells.set(0, !sh, "RATIO", v[0], "×");
        _cells.set(1, !sh, "LEVEL", v[1], "");
        _cells.set(2, sh, "ALGORITHM", v[2], "OF 8");
        _cells.set(3, sh, "FEEDBACK", v[3], "OF 7");
        if (sh) {
            _bottom.set_param(1, "ALGORITHM", v[2], "", (s.fm_algorithm - 1) / 7.0f);
            _bottom.set_param(2, "FEEDBACK", v[3], "", s.fm_feedback / 7.0f);
        } else {
            _bottom.set_param(1, "RATIO", v[0], "×", (s.fm_ratio[sel] - 0.5f) / 30.5f);
            _bottom.set_param(2, "LEVEL", v[1], "", s.fm_level[sel] / 99.0f);
        }
    } else {
        for (int k = 0; k < OSC_SLOTS; ++k) {
            const OscSlot* sl = e.slot[sel][k];
            const bool on = sh ? k >= 2 : k < 2;
            if (!sl) {
                _cells.set_unused(k);
                if (on) _bottom.set_param(k % 2 + 1, "NO FUNCTION", "—", "", 0);
                continue;
            }
            const float val = osc_value(s, s.engine, sel, k);
            osc_format(e, *sl, val, v[k], sizeof(v[k]));
            const Wave icon = sl->kind == SlotKind::WaveSel ? e.waves[std::clamp(int(val), 0, e.wave_count - 1)] : Wave::Noise;
            _cells.set(k, on, sl->name, v[k], sl->unit, icon);
            if (on) _bottom.set_param(k % 2 + 1, sl->name, v[k], sl->unit, osc_frac(e, *sl, val));
        }
    }

    // La gráfica se redibuja solo cuando cambia algo de lo que muestra (diseño: "on parameter change").
    char key[160];
    int n = std::snprintf(key, sizeof(key), "%d|%d|%d|", s.engine, sel, sh && e.fm);
    if (e.fm) {
        n += std::snprintf(key + n, sizeof(key) - n, "%d|%d|%d%d%d%d|%.2f%.2f%.2f%.2f", s.fm_algorithm, s.fm_feedback,
                           s.fm_level[0], s.fm_level[1], s.fm_level[2], s.fm_level[3], double(s.fm_ratio[0]),
                           double(s.fm_ratio[1]), double(s.fm_ratio[2]), double(s.fm_ratio[3]));
    } else {
        for (int st = 0; st < OSC_STOPS && n < int(sizeof(key)) - 8; ++st)
            for (int k = 0; k < OSC_SLOTS; ++k) n += std::snprintf(key + n, sizeof(key) - n, "%.0f,", double(s.osc_val[s.engine][st][k] * 4));
    }
    if (std::strcmp(key, _last_key) != 0) {
        std::snprintf(_last_key, sizeof(_last_key), "%s", key);
        lv_obj_invalidate(_graph);
    }
}
