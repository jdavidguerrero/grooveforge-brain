#include "ui/screen_perform.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

#include "studio_tick.h"
#include "ui/dsp_math.h"
#include "ui/engines.h"
#include "ui/input.h"
#include "ui/painter.h"
#include "ui/theme.h"

using namespace gf;

namespace {

// Gráfica: 264,48 · 520×328. Onda arriba (0–168), respuesta del filtro abajo (180–328).
constexpr int GX = 264, GY = 48, GW = 520, GH = 328;
constexpr int SCOPE_Y = 24, SCOPE_H = 136, RESP_Y = 196, RESP_H = 120;
constexpr int WAVE_PTS = 256;
constexpr float F0 = 110.0f;                    // nota de referencia de la onda dibujada
constexpr int SPC = 256;                        // muestras por ciclo de la simulación
constexpr float FS = F0 * SPC;                  // 28.2 kHz: el ringing a cutoff alto no sale dentado
constexpr float TOP_DB = 12.0f, RANGE_DB = 60.0f;

/// LFO global en su fase actual (−1..1), misma forma que dibuja la página LFO.
float lfo_value(const StudioState& s) {
    const float ph = s.lfo_phase - std::floor(s.lfo_phase);
    switch (s.lfo_shape) {
        case LfoShape::Sine: return std::sin(6.2831853f * ph);
        case LfoShape::Tri: return 1.0f - 4.0f * std::fabs(ph - 0.5f);
        case LfoShape::Saw: return 2.0f * ph - 1.0f;
        case LfoShape::Sqr: return ph < 0.5f ? 1.0f : -1.0f;
        default: return std::sin(6.2831853f * std::floor(s.lfo_phase) * 1.7f);   // S&H: un valor por ciclo
    }
}

/// Una muestra de la onda principal del engine en la fase `p` (ciclos), sin anti-aliasing (es para dibujar).
float osc_sample(const StudioState& s, float p, float pw) {
    const float ph = p - std::floor(p);
    const float saw = 2.0f * ph - 1.0f;
    const float pulse = ph < pw ? 1.0f : -1.0f;
    const float sqr = ph < 0.5f ? 1.0f : -1.0f;
    const float tri = 1.0f - 4.0f * std::fabs(ph - 0.5f);
    const float (&o)[OSC_STOPS][OSC_SLOTS] = s.osc_val[s.engine % 6];
    switch (s.engine) {
        case 0: {   // MODEL D: SAW · TRI · SQR · PULSE
            const int w = int(std::lround(o[0][0]));
            return w == 1 ? tri : w == 2 ? sqr : w == 3 ? pulse : saw;
        }
        case 1: {   // JUNO-106: SAW · SQR · PULSE · SAW+PULSE, más el SUB una octava abajo
            const int w = int(std::lround(o[0][0]));
            const float main = w == 1 ? sqr : w == 2 ? pulse : w == 3 ? 0.55f * (saw + pulse) : saw;
            const float sub = (p * 0.5f - std::floor(p * 0.5f)) < 0.5f ? 1.0f : -1.0f;
            return main + o[0][1] / 100.0f * 0.6f * sub;
        }
        case 2: {   // PROPHET-5: OSC A con SAW y / o PULSE
            const bool a_saw = o[0][2] > 0.5f, a_pulse = o[0][3] > 0.5f;
            return a_saw && a_pulse ? 0.6f * (saw + pulse) : a_pulse ? pulse : saw;
        }
        case 3: {   // OB-6: SHAPE continua saw → pulse
            const float m = std::clamp(o[0][0] / 100.0f, 0.0f, 1.0f);
            return saw + m * (pulse - saw);
        }
        case 4: {   // DX7: OP2 → OP1 (índice desde el LEVEL, como dx7_level_amp)
            const float idx = s.fm_level[1] > 0 ? 6.0f * std::pow(2.0f, (s.fm_level[1] - 99.0f) / 8.0f) : 0.0f;
            return std::sin(6.2831853f * ph + idx * std::sin(6.2831853f * ph * s.fm_ratio[1]));
        }
        default: {  // ARP 2600: SAW · TRI · SQR · PULSE · SINE
            const int w = int(std::lround(o[0][0]));
            return w == 1 ? tri : w == 2 ? sqr : w == 3 ? pulse : w == 4 ? std::sin(6.2831853f * ph) : saw;
        }
    }
}

/// Onda filtrada: ladder lineal de 4 polos (TPT) con realimentación; arranca 4 ciclos antes para asentarse.
void filtered_wave(const StudioState& s, float fc, float pw, float phase0, float* out) {
    const float f = std::clamp(fc, 20.0f, 0.45f * FS);
    const float g = std::tan(3.14159265f * f / FS), G = g / (1.0f + g);
    const float k = std::clamp(s.resonance, 0.0f, 1.0f) * 3.8f;
    const float comp = 1.0f + 0.5f * k;          // la ganancia del pasabanda cae con la resonancia
    float st[4] = {};
    const int pre = 4 * SPC, n = 2 * SPC;        // se dibujan 2 ciclos
    const float G4 = G * G * G * G, H = 1.0f - G;
    for (int i = 0; i < pre + n; ++i) {
        const float x = osc_sample(s, phase0 + float(i) / SPC, pw) * 0.6f;
        const float sigma = G * G * G * H * st[0] + G * G * H * st[1] + G * H * st[2] + H * st[3];
        const float y4 = (G4 * x + sigma) / (1.0f + k * G4);
        float u = x - k * y4;
        for (float& v : st) {
            const float d = G * (u - v);
            const float y = d + v;
            v = y + d;
            u = y;
        }
        if (i >= pre) {
            const int j = (i - pre) * WAVE_PTS / n;
            if (j < WAVE_PTS) out[j] = std::clamp(u * comp, -1.2f, 1.2f);
        }
    }
}

float filter_db_at(const StudioState& s, float f, float fc) {
    if (s.engine == 3)
        return sem_filter_db(f, fc, s.resonance, s.osc_val[3][0][6] / 100.0f, s.osc_val[3][0][7] > 0.5f);
    return filter_db(f, fc, s.resonance);
}

}  // namespace

lv_obj_t* ScreenPerform::create() {
    _scr = screen_root();
    _top.create(_scr, "PERFORM", 0);

    // Card del engine compacta (theory/08): tocarla abre ENGINE SELECT
    lv_obj_t* card = box(_scr, 16, 48, 232, 112, SURFACE, TEAL_DIM);
    input_attach_tap(card, Ctl::EngineCard);
    _art = engine_art_create(card, 12, 12, 90, 60, 0);
    _engine_name = label(card, T20, WHITE, "");
    lv_obj_set_pos(_engine_name, 112, 14);
    _engine_tag = label(card, T12, GRAY, "");
    lv_obj_set_width(_engine_tag, 112);
    lv_label_set_long_mode(_engine_tag, LV_LABEL_LONG_CLIP);
    lv_obj_set_pos(_engine_tag, 112, 42);
    _preset = label(card, T12, WHITE, "");
    lv_obj_set_width(_preset, 208);
    lv_label_set_long_mode(_preset, LV_LABEL_LONG_CLIP);
    lv_obj_set_pos(_preset, 12, 84);

    build_readout(_cut, 168, "CUTOFF", "HZ");
    input_attach_tap(_cut.box, Ctl::EncLPush);   // atajo táctil: tocar CUTOFF = ENC L push (bypass)
    build_readout(_res, 272, "RESONANCE", "%");

    _graph = draw_area(_scr, GX, GY, GW, GH, on_graph_draw, this);
    _bottom.create(_scr);
    return _scr;
}

void ScreenPerform::build_readout(Readout& r, int y, const char* caption, const char* unit) {
    r.box = box(_scr, 16, y, 232, 96, SURFACE, LINE);
    lv_obj_t* cap = label(r.box, T12B, GRAY, caption);
    lv_obj_set_style_text_letter_space(cap, 2, 0);
    lv_obj_set_pos(cap, 12, 10);
    r.value = label(r.box, T40, WHITE, "");
    lv_obj_set_pos(r.value, 12, 28);
    r.unit = label(r.box, T12, GRAY, unit);
    rect(r.box, 12, 80, 208, 6, LINE);
    r.bar = rect(r.box, 12, 80, 0, 6, TEAL);
    r.last_w = -1;
}

void ScreenPerform::set_readout(Readout& r, const char* val, float frac, uint32_t color) {
    if (std::strcmp(val, r.last) != 0) {
        std::snprintf(r.last, sizeof(r.last), "%s", val);
        lv_label_set_text(r.value, val);
        lv_obj_update_layout(r.value);
        lv_obj_set_pos(r.unit, 12 + lv_obj_get_width(r.value) + 6, 52);
    }
    const int w = int(std::lround(std::clamp(frac, 0.0f, 1.0f) * 208));
    if (w != r.last_w) {
        r.last_w = w;
        lv_obj_set_width(r.bar, w);
    }
    set_bg(r.bar, color);
}

void ScreenPerform::on_graph_draw(lv_event_t* e) {
    auto* self = static_cast<ScreenPerform*>(lv_event_get_user_data(e));
    if (self->_s) self->draw(Painter::from(e), *self->_s);
}

void ScreenPerform::draw(const Painter& p, const StudioState& s) const {
    p.fill(0, 0, GW, GH, SURFACE);
    p.frame(0, 0, GW, GH, 1, LINE);

    // Modulación del LFO (la misma escala que el firmware: ±4 oct a CUTOFF, ±0.45 de PW)
    const float lv = lfo_value(s);
    const float fc = s.filter_bypass ? 20000.0f
                                     : std::clamp(s.cutoff_hz * std::exp2(s.lfo_amt[0] * 4.0f * lv), 20.0f, 20000.0f);
    float pw = 0.5f;
    if (s.engine == 0) pw = s.osc_val[0][0][4] / 100.0f;
    else if (s.engine == 1) pw = s.osc_val[1][0][3] / 100.0f;
    else if (s.engine == 2) pw = s.osc_val[2][0][1] / 100.0f;
    else if (s.engine == 3) pw = s.osc_val[3][0][4] / 100.0f;
    else if (s.engine == 5) pw = s.osc_val[5][0][1] / 100.0f;
    pw = std::clamp(pw + 0.45f * s.lfo_amt[3] * lv, 0.05f, 0.95f);

    // ── Onda filtrada ──────────────────────────────────────────────────────
    p.text(12, 16, "OUT \xC2\xB7 OSC \xE2\x86\x92 FILTER", MON, T12);
    const int cy = SCOPE_Y + SCOPE_H / 2;
    p.fill(12, cy, GW - 24, 1, LINE);
    float wv[WAVE_PTS];
    filtered_wave(s, fc, pw, _scroll, wv);
    const float sx = float(GW - 24) / (WAVE_PTS - 1), amp = SCOPE_H * 0.42f;
    for (int i = 1; i < WAVE_PTS; ++i)
        p.line(12 + (i - 1) * sx, cy - wv[i - 1] * amp, 12 + i * sx, cy - wv[i] * amp, 2, TEAL_PLUS);

    // ── Respuesta del filtro ───────────────────────────────────────────────
    p.text(12, RESP_Y - 8, "FILTER", MON, T12);
    auto db_y = [](float db) { return RESP_Y + RESP_H * (TOP_DB - std::clamp(db, -60.0f, TOP_DB)) / (TOP_DB + RANGE_DB); };
    p.fill(12, db_y(0.0f), GW - 24, 1, LINE);
    const int n = 128;
    float px = 12, py = db_y(filter_db_at(s, 20.0f, fc));
    for (int i = 1; i < n; ++i) {
        const float x = 12 + float(i) * (GW - 24) / (n - 1);
        const float y = db_y(filter_db_at(s, frac_freq(float(i) / (n - 1)), fc));
        p.line(px, py, x, y, 2, s.filter_bypass ? ORANGE : TEAL_PLUS);
        px = x;
        py = y;
    }
    const float cut_x = 12 + freq_frac(fc) * (GW - 24);
    p.fill(cut_x, RESP_Y, 1, RESP_H, WHITE);
    char t[32];
    std::snprintf(t, sizeof(t), "FC %d HZ \xC2\xB7 RES %d%%", int(std::lround(fc)), int(std::lround(s.resonance * 100)));
    const int tw = text_width(t, T12);
    const float tx = cut_x + 8 + tw <= GW - 12 ? cut_x + 8 : cut_x - 8 - tw;   // del lado donde quepa
    p.text(tx, RESP_Y + 14, t, s.filter_bypass ? ORANGE : WHITE, T12);
}

void ScreenPerform::update(const StudioState& s) {
    _s = &s;
    _top.update(s);

    if (s.engine != _last_engine) {
        _last_engine = s.engine;
        lv_label_set_text(_engine_name, engine_info(s.engine).name);
        lv_label_set_text(_engine_tag, engine_info(s.engine).tag);
        engine_art_set(_art, s.engine);
    }
    if (std::strcmp(lv_label_get_text(_preset), s.preset) != 0) lv_label_set_text(_preset, s.preset);

    char buf[16], rbuf[8];
    std::snprintf(buf, sizeof(buf), "%d", int(std::lround(s.cutoff_hz)));
    const float cut_frac = freq_frac(s.cutoff_hz);
    set_readout(_cut, buf, cut_frac, s.filter_bypass ? ORANGE : TEAL);
    std::snprintf(rbuf, sizeof(rbuf), "%d", int(std::lround(s.resonance * 100.0f)));
    set_readout(_res, rbuf, s.resonance, TEAL);

    // Gráfica a ~30 Hz: la onda corre despacio (0.35 ciclos / s) y sigue al LFO
    const uint32_t now = studio_tick_ms();
    if (now - _last_draw_ms >= 33) {
        _scroll += 0.35f * float(now - _last_draw_ms) / 1000.0f;
        _scroll -= std::floor(_scroll);
        _last_draw_ms = now;
        lv_obj_invalidate(_graph);
    }

    _bottom.set_mode(s.mode == Mode::Fx, s.shift);
    _bottom.set_enc(false, "CUTOFF", buf, "HZ", cut_frac);
    _bottom.set_enc(true, "RES", rbuf, "%", s.resonance);
    static const char* const KEYS[4] = {"OSC", "ENV", "LFO / MOD", "SYNTH FX"};
    _bottom.set_keys(KEYS, -1);
    char v[16];
    const float p1_val = s.shift ? s.glide : s.p1;
    std::snprintf(v, sizeof(v), "%d", int(std::lround(p1_val * 100.0f)));
    _bottom.set_param(1, s.shift ? "GLIDE" : "FLT ENV AMT", v, "%", p1_val);
    const float p2_val = s.shift ? s.drive : s.p2;
    std::snprintf(v, sizeof(v), "%d", int(std::lround(p2_val * 100.0f)));
    _bottom.set_param(2, s.shift ? "DRIVE" : "LFO\xE2\x86\x92" "CUTOFF", v, "%", p2_val);
    _bottom.set_nav("VIEW");
}
