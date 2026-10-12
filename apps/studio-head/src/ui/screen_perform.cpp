#include "ui/screen_perform.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

#include "ui/dsp_math.h"
#include "ui/engines.h"
#include "ui/input.h"
#include "ui/theme.h"

using namespace gf;

namespace {

// Franja de espectro: 16,248 · 768×72 con borde de 1 px → área útil de 766×70.
constexpr int STRIP_X = 16, STRIP_Y = 248, STRIP_W = 768, STRIP_H = 72;
constexpr int PLOT_W = 766, PLOT_H = 70, BAR_GAP = 4;
// Diales de 192×192: ticks entre r 78 y 92 (escala de r 86–100 en 208 px de la v1).
constexpr int DIAL = 192, DIAL_C = 96, TICK_R1 = 78, TICK_R2 = 92;


lv_obj_t* chip_row(lv_obj_t* box) {
    lv_obj_set_style_pad_hor(box, 12, 0);
    lv_obj_set_flex_flow(box, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(box, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(box, 8, 0);
    return box;
}

void format_cutoff(float hz, char* out, size_t n) {
    std::snprintf(out, n, "%d", int(std::lround(hz)));
}

}  // namespace

lv_obj_t* ScreenPerform::create() {
    _scr = screen_root();

    _top.create(_scr, "PERFORM", 0);

    // ── Card del engine ──────────────────────────────────────────────────────
    lv_obj_t* card = box(_scr, 16, 48, 232, 192, SURFACE, TEAL_DIM);
    input_attach_tap(card, Ctl::EngineCard);  // v3: tocar la card abre ENGINE SELECT
    _art = engine_art_create(card, 15, 15, 120, 80, 0);
    lv_obj_t* eng = label(card, T12, GRAY, "ENGINE");
    lv_obj_set_style_text_letter_space(eng, 1, 0);
    lv_obj_set_pos(eng, 143, 15);
    _engine_name = label(card, T20, WHITE, "");
    lv_obj_set_pos(_engine_name, 15, 103);
    _engine_tag = label(card, T12, GRAY, "");
    lv_obj_set_pos(_engine_tag, 15, 127);
    _preset = label(card, T12, WHITE, "");
    lv_obj_set_pos(_preset, 15, 151);

    // ── Diales ───────────────────────────────────────────────────────────────
    build_dial(_cut, 296, "CUTOFF", "HZ");
    input_attach_tap(_cut.root, Ctl::EncLPush);  // atajo táctil del demo: tocar el dial = ENC L push
    build_dial(_res, 560, "RESONANCE", "%");

    // ── Franja de espectro ───────────────────────────────────────────────────
    lv_obj_t* strip = box(_scr, STRIP_X, STRIP_Y, STRIP_W, STRIP_H, SURFACE, LINE);
    const float slot = float(PLOT_W + BAR_GAP) / BANDS;
    for (int i = 0; i < BANDS; ++i) {
        const int x0 = int(std::lround(i * slot));
        const int x1 = int(std::lround((i + 1) * slot)) - BAR_GAP;
        _bars[i] = rect(strip, x0, PLOT_H, x1 - x0, 0, TEAL);
    }
    _curve = lv_line_create(strip);
    lv_obj_set_pos(_curve, 0, 0);
    lv_obj_set_style_line_width(_curve, 2, 0);
    lv_obj_set_style_line_color(_curve, lv_color_hex(TEAL_PLUS), 0);
    _cut_line = rect(strip, 0, 0, 1, PLOT_H, WHITE);
    _cut_tag = label(strip, T12, TEAL_PLUS, "");
    lv_obj_set_y(_cut_tag, 4);

    // ── Chips de AI: 16,328 · 4 × 186×40, separación 8 ──────────────────────
    const int cy = 328;
    _key.box = chip_row(rect(_scr, 16, cy, 186, 40, camelot(8)));
    _key.a = label(_key.box, T20, BG, "");
    _key.b = label(_key.box, T12B, BG, "");
    lv_obj_set_flex_grow(_key.b, 1);
    _key.c = label(_key.box, T12, BG, "");

    _chord.box = chip_row(box(_scr, 16 + 194, cy, 186, 40, SURFACE, LINE));
    label(_chord.box, T12, GRAY, "CHORD");
    _chord.a = label(_chord.box, T20, WHITE, "");
    lv_obj_set_flex_grow(_chord.a, 1);
    _chord.c = label(_chord.box, T12, MON, "");

    _bpm.box = chip_row(box(_scr, 16 + 388, cy, 186, 40, SURFACE, LINE));
    label(_bpm.box, T12, GRAY, "BPM");
    _bpm.a = label(_bpm.box, T20, WHITE, "");
    lv_obj_set_flex_grow(_bpm.a, 1);
    lv_obj_t* beats = lv_obj_create(_bpm.box);
    lv_obj_remove_style_all(beats);
    lv_obj_set_size(beats, 4 * 8 + 3 * 4, 8);
    for (int i = 0; i < 4; ++i) _beats[i] = rect(beats, i * 12, 0, 8, 8, TEAL_DIM);

    _genre.box = chip_row(box(_scr, 16 + 582, cy, 186, 40, SURFACE, LINE));
    label(_genre.box, T12, GRAY, "GENRE");
    _genre.a = label(_genre.box, T20, WHITE, "");
    lv_obj_set_flex_grow(_genre.a, 1);
    _genre.c = label(_genre.box, T12, MON, "");

    _bottom.create(_scr);
    return _scr;
}

void ScreenPerform::build_dial(Dial& d, int x, const char* caption, const char* unit) {
    d.root = lv_obj_create(_scr);
    lv_obj_remove_style_all(d.root);
    lv_obj_clear_flag(d.root, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_pos(d.root, x, 48);
    lv_obj_set_size(d.root, DIAL, DIAL);

    for (int i = 0; i < TICKS; ++i) {
        d.ticks[i] = lv_line_create(d.root);
        lv_obj_set_pos(d.ticks[i], 0, 0);
        lv_obj_set_style_line_width(d.ticks[i], 2, 0);
        lv_obj_set_style_line_color(d.ticks[i], lv_color_hex(TEAL_DIM), 0);
    }
    // Con fondo propio: el tick largo del valor actual pasa por detrás de las cifras.
    d.value = label(d.root, T64, WHITE, "");
    lv_obj_set_style_bg_opa(d.value, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(d.value, lv_color_hex(BG), 0);
    lv_obj_align(d.value, LV_ALIGN_TOP_MID, 0, 56);
    d.unit = label(d.root, T12, GRAY, unit);
    lv_obj_set_style_text_letter_space(d.unit, 2, 0);
    lv_obj_align(d.unit, LV_ALIGN_TOP_MID, 0, 128);
    lv_obj_t* cap = label(d.root, T16, GRAY, caption);
    lv_obj_set_style_text_letter_space(cap, 2, 0);
    lv_obj_align(cap, LV_ALIGN_TOP_MID, 0, 160);
    d.last_idx = -1;
}

void ScreenPerform::update_dial(Dial& d, float frac, const char* val, uint32_t on_color) {
    if (std::strcmp(val, d.last_val) != 0) {
        std::snprintf(d.last_val, sizeof(d.last_val), "%s", val);
        lv_label_set_text(d.value, val);
        lv_obj_align(d.value, LV_ALIGN_TOP_MID, 0, 56);
    }
    const int idx = int(std::lround(std::clamp(frac, 0.0f, 1.0f) * (TICKS - 1)));
    if (idx == d.last_idx) return;
    // Solo se re-dibujan los ticks que cambian de estado.
    const int lo = d.last_idx < 0 ? 0 : std::min(idx, d.last_idx);
    const int hi = d.last_idx < 0 ? TICKS - 1 : std::max(idx, d.last_idx);
    d.last_idx = idx;
    for (int i = lo; i <= hi; ++i) {
        const float a = float((-135.0 + 270.0 * i / (TICKS - 1)) * M_PI / 180.0);
        const float s = std::sin(a), c = -std::cos(a);
        const float r1 = (i == idx) ? TICK_R1 - 10 : TICK_R1;
        d.pts[i][0] = {lv_coord_t(DIAL_C + s * r1), lv_coord_t(DIAL_C + c * r1)};
        d.pts[i][1] = {lv_coord_t(DIAL_C + s * TICK_R2), lv_coord_t(DIAL_C + c * TICK_R2)};
        lv_line_set_points(d.ticks[i], d.pts[i], 2);
        const uint32_t col = i == idx ? WHITE : (i < idx ? on_color : TEAL_DIM);
        lv_obj_set_style_line_color(d.ticks[i], lv_color_hex(col), 0);
        lv_obj_set_style_line_width(d.ticks[i], i <= idx ? 3 : 2, 0);
    }
}

void ScreenPerform::update(const StudioState& s) {
    _top.update(s);

    if (s.engine != _last_engine) {
        _last_engine = s.engine;
        lv_label_set_text(_engine_name, engine_info(s.engine).name);
        lv_label_set_text(_engine_tag, engine_info(s.engine).tag);
        engine_art_set(_art, s.engine);
    }
    if (std::strcmp(lv_label_get_text(_preset), s.preset) != 0) lv_label_set_text(_preset, s.preset);

    // Diales
    char buf[32];
    format_cutoff(s.cutoff_hz, buf, sizeof(buf));
    const float cut_frac = freq_frac(s.cutoff_hz);
    update_dial(_cut, cut_frac, buf, s.filter_bypass ? ORANGE : TEAL);
    char rbuf[8];
    std::snprintf(rbuf, sizeof(rbuf), "%d", int(std::lround(s.resonance * 100.0f)));
    update_dial(_res, s.resonance, rbuf, TEAL);

    // Espectro: alturas de barra
    for (int i = 0; i < BANDS; ++i) {
        const uint8_t h = uint8_t(std::lround(s.spectrum[i] / 255.0f * PLOT_H));
        if (h == _last_bar_h[i]) continue;
        _last_bar_h[i] = h;
        lv_obj_set_y(_bars[i], PLOT_H - h);
        lv_obj_set_height(_bars[i], h);
    }

    std::snprintf(buf, sizeof(buf), "FC %d HZ", int(std::lround(s.cutoff_hz)));
    if (std::strcmp(buf, _last_cut_tag) != 0) {
        std::snprintf(_last_cut_tag, sizeof(_last_cut_tag), "%s", buf);
        lv_label_set_text(_cut_tag, buf);
    }

    // Línea del cutoff y curva del filtro: solo cuando el cutoff se mueve un píxel.
    const int cut_x = int(std::lround(cut_frac * (PLOT_W - 1)));
    if (cut_x != _last_cut_x) {
        _last_cut_x = cut_x;
        lv_obj_set_x(_cut_line, cut_x);
        lv_obj_set_x(_cut_tag, cut_x < PLOT_W - 110 ? cut_x + 6 : cut_x - 104);
        for (int p = 0; p < CURVE_PTS; ++p) {
            const float x = float(p) * (PLOT_W - 1) / (CURVE_PTS - 1);
            const float f = 20.0f * std::pow(1000.0f, x / (PLOT_W - 1));
            const float top = 12.0f;
            float y = PLOT_H * ((top - filter_db(f, s.cutoff_hz, s.resonance)) / (top + 60.0f));
            y = std::clamp(y, 1.0f, float(PLOT_H - 1));
            _curve_pts[p] = {lv_coord_t(x), lv_coord_t(y)};
        }
        lv_line_set_points(_curve, _curve_pts, CURVE_PTS);
    }

    // Chips
    std::snprintf(buf, sizeof(buf), "%u%c|%s|%u", s.key_camelot, s.key_minor ? 'A' : 'B', s.key_name, s.key_conf);
    if (std::strcmp(buf, _key.last) != 0) {
        std::snprintf(_key.last, sizeof(_key.last), "%s", buf);
        set_bg(_key.box, camelot(s.key_camelot));
        lv_label_set_text_fmt(_key.a, "%u%c", s.key_camelot, s.key_minor ? 'A' : 'B');
        lv_label_set_text(_key.b, s.key_name);
        lv_label_set_text_fmt(_key.c, "%u%%", s.key_conf);
    }
    std::snprintf(buf, sizeof(buf), "%s|%u", s.chord, s.chord_conf);
    if (std::strcmp(buf, _chord.last) != 0) {
        std::snprintf(_chord.last, sizeof(_chord.last), "%s", buf);
        lv_label_set_text(_chord.a, s.chord);
        lv_label_set_text_fmt(_chord.c, "%u%%", s.chord_conf);
    }
    std::snprintf(buf, sizeof(buf), "%.1f", s.bpm);
    if (std::strcmp(buf, _bpm.last) != 0) {
        std::snprintf(_bpm.last, sizeof(_bpm.last), "%s", buf);
        lv_label_set_text(_bpm.a, buf);
    }
    const int beat = std::min(3, int(s.bar_phase * 4.0f));
    if (beat != _last_beat) {
        _last_beat = beat;
        for (int i = 0; i < 4; ++i) set_bg(_beats[i], i == beat ? TEAL_PLUS : TEAL_DIM);
    }
    std::snprintf(buf, sizeof(buf), "%s|%u", s.genre, s.genre_conf);
    if (std::strcmp(buf, _genre.last) != 0) {
        std::snprintf(_genre.last, sizeof(_genre.last), "%s", buf);
        lv_label_set_text(_genre.a, s.genre);
        lv_label_set_text_fmt(_genre.c, "%u%%", s.genre_conf);
    }

    // Barra inferior
    _bottom.set_mode(s.mode == Mode::Fx, s.shift);
    format_cutoff(s.cutoff_hz, buf, sizeof(buf));
    _bottom.set_enc(false, "CUTOFF", buf, "HZ", cut_frac);
    _bottom.set_enc(true, "RES", rbuf, "%", s.resonance);
    static const char* const KEYS[4] = {"OSC", "ENV", "LFO / MOD", "PRESET"};
    _bottom.set_keys(KEYS, -1);
    std::snprintf(buf, sizeof(buf), "%d", int(std::lround(s.p1 * 100.0f)));
    _bottom.set_param(1, s.shift ? "GLIDE" : "FLT ENV AMT", buf, "%", s.p1);
    std::snprintf(buf, sizeof(buf), "%d", int(std::lround(s.p2 * 100.0f)));
    _bottom.set_param(2, s.shift ? "DRIVE" : "LFO\xE2\x86\x92" "CUTOFF", buf, "%", s.p2);
    _bottom.set_nav("VIEW");
}
