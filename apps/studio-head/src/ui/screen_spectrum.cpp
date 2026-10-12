#include "ui/screen_spectrum.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

#include "ui/dsp_math.h"
#include "ui/theme.h"

using namespace gf;

namespace {

constexpr int PX = 64, PY = 48, PW = 704, PH = 256;
constexpr float TOP_DB = 12.0f, RANGE_DB = 72.0f;   // eje de +12 a −60 dB
constexpr int GAP = 4;
constexpr float PEAK_FALL = 3.0f;                   // px por frame (studio-data.js)

int db_to_y(float db) { return int(std::lround(PH * (TOP_DB - db) / RANGE_DB)); }

}  // namespace

lv_obj_t* ScreenSpectrum::create() {
    _scr = screen_root();
    _top.create(_scr, "SPECTRUM", 2);

    // Grilla de dB
    for (int d = 0; d >= -60; d -= 12) {
        const int gy = PY + db_to_y(float(d));
        rect(_scr, PX, gy, PW, 1, SURFACE_2);
        char t[8];
        std::snprintf(t, sizeof(t), d == 0 ? "0 dB" : "%d", d);
        lv_obj_t* l = label(_scr, T12, MON, t);
        lv_obj_set_width(l, 40);
        lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_RIGHT, 0);
        lv_obj_set_pos(l, 16, gy - 6);
    }

    // Barras y marcas de pico
    const float slot = float(PW + GAP) / BANDS;
    for (int i = 0; i < BANDS; ++i) {
        const int x0 = PX + int(std::lround(i * slot));
        const int w = int(std::lround((i + 1) * slot)) - int(std::lround(i * slot)) - GAP;
        _bars[i] = rect(_scr, x0, PY + PH, w, 0, TEAL);
        _peaks[i] = rect(_scr, x0, PY + PH - 2, w, 2, WHITE);
        _last_h[i] = -1;
    }

    // Curva del filtro, línea del cutoff y etiqueta de resonancia
    _curve = lv_line_create(_scr);
    lv_obj_set_pos(_curve, PX, PY);
    lv_obj_set_style_line_width(_curve, 2, 0);
    lv_obj_set_style_line_color(_curve, lv_color_hex(TEAL_PLUS), 0);
    _cut_line = rect(_scr, PX, PY, 1, PH, WHITE);
    _res_tag = label(_scr, T12B, TEAL_PLUS, "");

    // Eje de frecuencias
    static const struct { float f; const char* l; } FREQS[] = {
        {20, "20"}, {50, "50"}, {100, "100"}, {200, "200"}, {500, "500"},
        {1000, "1K"}, {2000, "2K"}, {5000, "5K"}, {10000, "10K"}, {20000, "20K"},
    };
    for (const auto& f : FREQS) {
        lv_obj_t* l = label(_scr, T12, MON, f.l);
        lv_obj_update_layout(l);
        lv_obj_set_pos(l, PX + int(std::lround(freq_frac(f.f) * PW)) - lv_obj_get_width(l) / 2, 312);
    }

    // Pie: descripción a la izquierda, FC y RES grandes a la derecha
    lv_obj_t* desc = label(_scr, T12, MON, "32 BANDS \xC2\xB7 LOG \xC2\xB7 dB \xC2\xB7 PEAK HOLD");
    lv_obj_set_pos(desc, 16, 348);
    _res_big = label(_scr, T28, WHITE, "");
    _fc_big = label(_scr, T28, TEAL_PLUS, "");

    _bottom.create(_scr);
    return _scr;
}

void ScreenSpectrum::update(const StudioState& s) {
    _top.update(s);
    const float fc = s.cutoff_hz;
    const float cut_frac = freq_frac(fc);

    for (int i = 0; i < BANDS; ++i) {
        const float db = s.spectrum[i] / 255.0f * 60.0f - 60.0f;
        const int h = std::clamp(PH - db_to_y(db), 0, PH);
        _peak_h[i] = std::max(_peak_h[i] - PEAK_FALL, float(h));
        const int pk = std::min(PH - 2, std::max(h + 2, int(std::lround(_peak_h[i]))));
        const bool lit = frac_freq((i + 0.5f) / BANDS) <= fc;
        if (h != _last_h[i]) {
            _last_h[i] = int16_t(h);
            lv_obj_set_y(_bars[i], PY + PH - h);
            lv_obj_set_height(_bars[i], h);
        }
        if (pk != _last_pk[i]) {
            _last_pk[i] = int16_t(pk);
            lv_obj_set_y(_peaks[i], PY + PH - pk - 2);
        }
        if (lit != _last_lit[i]) {
            _last_lit[i] = lit;
            set_bg(_bars[i], lit ? TEAL_PLUS : TEAL);
        }
    }

    const int cut_x = int(std::lround(cut_frac * (PW - 1)));
    const int res = int(std::lround(s.resonance * 100.0f));
    if (cut_x != _last_cut_x || res != _last_res) {
        _last_cut_x = cut_x;
        _last_res = res;
        lv_obj_set_x(_cut_line, PX + cut_x);
        for (int p = 0; p < CURVE_PTS; ++p) {
            const float x = float(p) * (PW - 1) / (CURVE_PTS - 1);
            const float y = std::clamp(float(db_to_y(filter_db(frac_freq(x / (PW - 1)), fc, s.resonance))), 1.0f,
                                       float(PH - 1));
            _curve_pts[p] = {lv_coord_t(x), lv_coord_t(y)};
        }
        lv_line_set_points(_curve, _curve_pts, CURVE_PTS);
        lv_label_set_text_fmt(_res_tag, "RES %d%%", res);
        const int res_y = std::clamp(db_to_y(filter_db(fc, fc, s.resonance)), 0, PH - 16);
        lv_obj_set_pos(_res_tag, PX + std::min(cut_x + 8, PW - 64), PY + res_y + 2);
    }

    char fcs[16];
    std::snprintf(fcs, sizeof(fcs), "FC %d HZ", int(std::lround(fc)));
    if (std::strcmp(fcs, _last_fc) != 0) {
        std::snprintf(_last_fc, sizeof(_last_fc), "%s", fcs);
        lv_label_set_text(_fc_big, fcs);
        lv_label_set_text_fmt(_res_big, "RES %d%%", res);
        lv_obj_update_layout(_res_big);
        lv_obj_set_pos(_res_big, 784 - lv_obj_get_width(_res_big), 342);
        lv_obj_update_layout(_fc_big);
        lv_obj_set_pos(_fc_big, 784 - lv_obj_get_width(_res_big) - 24 - lv_obj_get_width(_fc_big), 342);
    }

    const bool fx = s.mode == Mode::Fx;
    _bottom.set_mode(fx, s.shift);
    static const char* const KEYS[4] = {"OSC", "ENV", "LFO / MOD", "PRESET"};
    _bottom.set_keys(KEYS, -1);
    char v[16];
    std::snprintf(v, sizeof(v), "%d", int(std::lround(fc)));
    _bottom.set_enc(false, "CUTOFF", v, "HZ", cut_frac);
    std::snprintf(v, sizeof(v), "%d", res);
    _bottom.set_enc(true, "RES", v, "%", s.resonance);
    std::snprintf(v, sizeof(v), "%d", int(std::lround(s.p1 * 100.0f)));
    _bottom.set_param(1, s.shift ? "GLIDE" : "FLT ENV AMT", v, "%", s.p1);
    std::snprintf(v, sizeof(v), "%d", int(std::lround(s.p2 * 100.0f)));
    _bottom.set_param(2, s.shift ? "DRIVE" : "LFO\xE2\x86\x92" "CUTOFF", v, "%", s.p2);
    _bottom.set_nav("VIEW");
}
