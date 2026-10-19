#include "ui/screen_lfo.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

#include <gf_params.h>

#include "state/osc_catalog.h"
#include "ui/edit_draw.h"
#include "ui/engines.h"
#include "ui/input.h"
#include "ui/theme.h"

using namespace gf;

namespace {

constexpr int GX = 16, GY = 48, GW = 768, GH = 208;
constexpr int WAVE_X = 24, WAVE_W = 512, WAVE_CY = 124, WAVE_AMP = 56;
constexpr int METER_TOP = 64, METER_H = 112, METER_CY = METER_TOP + METER_H / 2;
constexpr int METER_X0 = 560, METER_STEP = 70, TRACK_DX = 26, TRACK_W = 12;
constexpr uint32_t TRACK = 0x1C1C1C;
const char* const KEYS_SYNTH[4] = {"OSC", "ENV", "LFO / MOD", "SYNTH FX"};
const char* const DEST_NAME[LFO_DESTS] = {"CUTOFF", "PITCH", "AMP", "PWM"};
const char* const DIVS[4] = {"1/4", "1/8", "1/16", "1/8T"};
const Wave SHAPES[5] = {Wave::Sine, Wave::Tri, Wave::Saw, Wave::Sqr, Wave::SampleHold};

Wave shape_wave(LfoShape s) { return SHAPES[int(s)]; }
float window_cycles(LfoShape s) { return s == LfoShape::SampleHold ? 8.0f : 2.0f; }

/// Los tres medidores: CUTOFF, PITCH y el tercer destino del engine (GF_DEST_NONE si no tiene).
uint8_t meter_dest(const StudioState& s, int i) {
    if (i == 0) return GF_DEST_CUTOFF;
    if (i == 1) return GF_DEST_PITCH;
    return GF_LFO_THIRD_DEST[s.engine % GF_ENGINE_COUNT];
}

bool dest_on(const StudioState& s, uint8_t d) { return d != GF_DEST_NONE && gf_lfo_dest_available(s.engine, d); }

/// Cantidad del destino: CUTOFF es el P2 de PERFORM (un solo valor por destino).
float amt_of(const StudioState& s, uint8_t d) { return d == GF_DEST_CUTOFF ? s.p2 : s.lfo_amt[d]; }

void fmt_amt(const StudioState& s, uint8_t d, char* val, size_t n, const char** unit) {
    if (d == GF_DEST_PITCH) {
        std::snprintf(val, n, "%.1f", double(gf_lfo_pitch_st(amt_of(s, d))));
        *unit = "ST";
    } else {
        std::snprintf(val, n, "%d", int(std::lround(amt_of(s, d) * 100)));
        *unit = "%";
    }
}

float rate_hz(const StudioState& s) {
    return s.lfo_sync ? s.bpm / 60.0f / GF_LFO_DIV_BEATS[s.lfo_div % 4] : s.lfo_rate_hz;
}

int band_px(const StudioState& s, uint8_t d) { return dest_on(s, d) ? int(std::lround(amt_of(s, d) * METER_H / 2)) : 0; }

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

void ScreenLfo::on_graph_draw(lv_event_t* e) {
    auto* self = static_cast<ScreenLfo*>(lv_event_get_user_data(e));
    if (self->_s) self->draw(Painter::from(e), *self->_s);
}

/// Parada de MOD: fila de paradas, fuentes (cantidades) como barras y destinos como fichas encendidas o apagadas.
void ScreenLfo::draw_mod(const Painter& p, const StudioState& s) const {
    p.fill(0, 0, GW, GH, SURFACE);
    p.frame(0, 0, GW, GH, 1, LINE);
    const int n = 1 + mod_stop_count(s.engine);
    int x = 16;
    for (int i = 0; i < n; ++i) {
        const char* name = i == 0 ? "LFO" : mod_stop(s.engine, i - 1).name;
        const int w = text_width(name, T12B) + 24;
        if (i == _stop) p.fill(x, 8, w, 24, TEAL_PLUS);
        else draw_box(p, x, 8, w, 24, LINE, SURFACE, 1);
        text_top(p, x + 12, 14, name, i == _stop ? BG : GRAY, T12B);
        x += w + 8;
    }
    text_top(p, x + 16, 14, "NAV SELECTS", MON, T12);

    const ModStop& m = mod_stop(s.engine, _stop - 1);
    const float* v = s.mod_val[s.engine % 6][(_stop - 1) % MOD_STOPS];
    text_top(p, 24, 52, "SOURCES", MON, T12B);
    text_top(p, 420, 52, "DESTINATIONS", MON, T12B);
    int ys = 76, dx = 420, dy = 76;
    char t[24];
    for (int k = 0; k < OSC_SLOTS; ++k) {
        const OscSlot* sl = m.slot[k];
        if (!sl) continue;
        if (sl->kind == SlotKind::Toggle) {                    // destino: ficha
            const bool on = v[k] > 0.5f;
            const int w = 110;
            if (on) p.fill(dx, dy, w, 32, TEAL_DIM);
            draw_box(p, dx, dy, w, 32, on ? TEAL_PLUS : LINE, on ? TEAL_DIM : SURFACE, on ? 2 : 1);
            text_top(p, dx + 12, dy + 9, sl->name, on ? WHITE : MON, T12B);
            dx += w + 12;
            if (dx > GW - 120) { dx = 420; dy += 44; }
        } else {                                              // fuente: barra con su cantidad
            text_top(p, 24, ys, sl->name, GRAY, T12B);
            std::snprintf(t, sizeof(t), "%d %%", int(std::lround(v[k])));
            text_top(p, 360 - text_width(t, T12), ys, t, WHITE, T12);
            p.fill(24, ys + 20, 336, 10, TRACK);
            p.fill(24, ys + 20, std::lround(336 * std::clamp(v[k] / 100.0f, 0.0f, 1.0f)), 10, TEAL_PLUS);
            ys += 52;
        }
    }
}

void ScreenLfo::draw(const Painter& p, const StudioState& s) const {
    if (_stop > 0) {
        draw_mod(p, s);
        return;
    }
    p.fill(0, 0, GW, GH, SURFACE);
    p.frame(0, 0, GW, GH, 1, LINE);

    // Fichas de forma: la elige NAV.
    for (int i = 0; i < 5; ++i) {
        const int x = 16 + i * 52;
        const bool on = i == int(s.lfo_shape);
        if (on) p.fill(x, 8, 48, 24, TEAL_DIM);
        else draw_box(p, x, 8, 48, 24, LINE, SURFACE, 1);
        draw_wave_icon(p, SHAPES[i], x + 12, 12, on ? TEAL_PLUS : MON);
    }

    char rv[16], rs[32], t[24];
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
    if (s.lfo_sync) {
        const int beats = std::max(1, int(cyc * GF_LFO_DIV_BEATS[s.lfo_div % 4]));
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

    // Tres medidores, uno por cantidad, con su banda ±cantidad; el destino que el engine no tiene, apagado (MON).
    p.fill(METER_X0 - 12, 8, 1, GH - 16, LINE);
    for (int i = 0; i < 3; ++i) {
        const uint8_t d = meter_dest(s, i);
        const bool on = dest_on(s, d);
        const int x = METER_X0 + i * METER_STEP, tx = x + TRACK_DX;
        const char* name = d == GF_DEST_NONE ? "—" : DEST_NAME[d];
        text_top(p, x + (METER_STEP - text_width(name, T12B)) / 2 - 4, 14, name, on ? TEAL_PLUS : MON, T12B);
        p.fill(tx, METER_TOP, TRACK_W, METER_H, TRACK);
        const int bd = band_px(s, d);
        if (bd > 0) p.fill(tx, METER_CY - bd, TRACK_W, 2 * bd, TEAL_DIM);
        p.fill(tx - 4, METER_CY - 1, TRACK_W + 8, 2, GRAY);
        char v[16];
        const char* unit = "";
        if (on) {
            fmt_amt(s, d, v, sizeof(v), &unit);
            std::snprintf(t, sizeof(t), "%s %s", v, unit);
        } else {
            std::snprintf(t, sizeof(t), "—");
        }
        text_top(p, x + (METER_STEP - text_width(t, T12)) / 2 - 4, 186, t, on ? WHITE : MON, T12);
        if (on) p.fill(tx - 4, _mark_y[i] - 2, TRACK_W + 8, 4, WHITE);   // marca viva
    }

    p.fill(WAVE_X + _dot_x - 4, _dot_y - 4, 8, 8, WHITE);                 // punto de fase
}

void ScreenLfo::invalidate_local(int x, int y, int w, int h) {
    lv_area_t a;
    lv_obj_get_coords(_graph, &a);
    const lv_area_t r{lv_coord_t(a.x1 + x), lv_coord_t(a.y1 + y), lv_coord_t(a.x1 + x + w - 1), lv_coord_t(a.y1 + y + h - 1)};
    lv_obj_invalidate_area(_graph, &r);
}

/// Parada de MOD: celdas y barra inferior con los 4 slots de la capa visible (SHIFT = slots 4–7).
void ScreenLfo::update_mod(const StudioState& s) {
    const bool sh = s.shift;
    const ModStop& m = mod_stop(s.engine, _stop - 1);
    const float* v = s.mod_val[s.engine % 6][(_stop - 1) % MOD_STOPS];
    const OscEngine& e = osc_engine(s.engine);
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
    _bottom.set_keys(KEYS_SYNTH, 2);
    _bottom.set_nav("STOP");
    char key[160];
    int n = std::snprintf(key, sizeof(key), "M%d|%d|%d|", _stop, sh, s.engine);
    for (int k = 0; k < OSC_SLOTS && n < int(sizeof(key)) - 8; ++k) n += std::snprintf(key + n, sizeof(key) - n, "%.0f,", double(v[k]));
    if (std::strcmp(key, _last_key) != 0) {
        std::snprintf(_last_key, sizeof(_last_key), "%s", key);
        lv_obj_invalidate(_graph);
    }
}

void ScreenLfo::update(const StudioState& s) {
    _s = &s;
    _top.update(s);
    const bool sh = s.shift;
    const int nmod = mod_stop_count(s.engine);
    _stop = s.ui_cursor <= nmod ? s.ui_cursor : 0;

    if (s.engine != _last_engine || _stop != _last_stop) {
        _last_engine = s.engine;
        _last_stop = _stop;
        char t[40];
        std::snprintf(t, sizeof(t), "%s · %s", _stop ? mod_stop(s.engine, _stop - 1).name : "LFO", engine_info(s.engine).name);
        _top.set_page(t);
    }
    if (_stop > 0) {
        update_mod(s);
        return;
    }

    // Celdas: P1 RATE · P2 →CUTOFF · ENC L →PITCH · ENC R →tercer destino · SHIFT + P1 = SHAPE.
    char rate[16];
    if (s.lfo_sync) std::snprintf(rate, sizeof(rate), "%s", DIVS[s.lfo_div % 4]);
    else std::snprintf(rate, sizeof(rate), "%.2f", double(s.lfo_rate_hz));
    const char* rate_u = s.lfo_sync ? "SYNC" : "HZ";
    char lbl[16], val[16];
    const char* unit = "";
    if (sh) {
        const Wave w = shape_wave(s.lfo_shape);
        _cells.set(0, true, "SHAPE", wave_name(w), "", w);
        for (int k = 1; k < 4; ++k) _cells.set_unused(k);
    } else {
        _cells.set(0, true, "RATE", rate, rate_u, shape_wave(s.lfo_shape));
        for (int i = 0; i < 3; ++i) {
            const uint8_t d = meter_dest(s, i);
            const int cell = i + 1;
            if (!dest_on(s, d)) {
                _cells.set_unused(cell);
                continue;
            }
            std::snprintf(lbl, sizeof(lbl), "\xE2\x86\x92%s", DEST_NAME[d]);
            fmt_amt(s, d, val, sizeof(val), &unit);
            _cells.set(cell, true, lbl, val, unit);
        }
    }

    _bottom.set_mode(s.mode == Mode::Fx, sh);
    _bottom.set_keys(KEYS_SYNTH, 2);
    _bottom.set_nav(nmod ? "STOP · SYNC" : "PUSH SYNC");
    auto bottom_amt = [&](int k, uint8_t d) {
        if (!dest_on(s, d)) {
            _bottom.set_page_control(k, "\xE2\x80\x94", "", "", 0.0f);
            return;
        }
        std::snprintf(lbl, sizeof(lbl), "\xE2\x86\x92%s", DEST_NAME[d]);
        fmt_amt(s, d, val, sizeof(val), &unit);
        _bottom.set_page_control(k, lbl, val, unit, amt_of(s, d));
    };
    if (sh) {
        const Wave w = shape_wave(s.lfo_shape);
        _bottom.set_page_control(0, "SHAPE", wave_name(w), "", int(s.lfo_shape) / 4.0f);
        for (int k = 1; k < 4; ++k) _bottom.set_page_control(k, "NO FUNCTION", "\xE2\x80\x94", "", 0.0f);
    } else {
        const float rf = s.lfo_sync ? (s.lfo_div % 4) / 3.0f : gf_lfo_rate_norm(s.lfo_rate_hz);
        _bottom.set_page_control(0, "RATE", rate, rate_u, rf);
        bottom_amt(1, GF_DEST_CUTOFF);
        bottom_amt(2, GF_DEST_PITCH);
        bottom_amt(3, meter_dest(s, 2));
    }

    // Lo estático se redibuja solo si cambia; el punto y las marcas, invalidando su zona.
    char key[96];
    std::snprintf(key, sizeof(key), "%d|%d|%d|%d|%.2f|%d|%d|%ld|%ld|%ld|%ld", int(s.lfo_shape), s.lfo_sync, s.lfo_div,
                  s.engine, double(s.lfo_rate_hz), int(std::lround(s.bpm)), sh, std::lround(s.p2 * 100),
                  std::lround(s.lfo_amt[1] * 100), std::lround(s.lfo_amt[2] * 100), std::lround(s.lfo_amt[3] * 100));
    const bool full = std::strcmp(key, _last_key) != 0;
    if (full) std::snprintf(_last_key, sizeof(_last_key), "%s", key);

    const float cyc = window_cycles(s.lfo_shape);
    const float ph = std::fmod(s.lfo_phase, cyc);
    const Wave w = shape_wave(s.lfo_shape);
    const float v = wave_value(w, ph);
    const int dot_x = int(std::floor(ph / cyc * WAVE_W / 2)) * 2;
    const int dot_y = int(std::lround(WAVE_CY - v * WAVE_AMP));
    int mark_y[3];
    for (int i = 0; i < 3; ++i) mark_y[i] = int(std::lround(METER_CY - v * band_px(s, meter_dest(s, i))));

    if (full) {
        _dot_x = dot_x; _dot_y = dot_y;
        std::memcpy(_mark_y, mark_y, sizeof(_mark_y));
        lv_obj_invalidate(_graph);
        return;
    }
    if (dot_x != _dot_x || dot_y != _dot_y) {
        invalidate_local(WAVE_X + _dot_x - 4, _dot_y - 4, 8, 8);
        _dot_x = dot_x; _dot_y = dot_y;
        invalidate_local(WAVE_X + _dot_x - 4, _dot_y - 4, 8, 8);
    }
    for (int i = 0; i < 3; ++i) {
        if (mark_y[i] == _mark_y[i]) continue;
        const int tx = METER_X0 + i * METER_STEP + TRACK_DX - 4;
        invalidate_local(tx, _mark_y[i] - 2, TRACK_W + 8, 4);
        _mark_y[i] = mark_y[i];
        invalidate_local(tx, _mark_y[i] - 2, TRACK_W + 8, 4);
    }
}
