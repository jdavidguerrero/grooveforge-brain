#include "ui/screen_ai.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

#include "ui/dsp_math.h"
#include "ui/theme.h"

using namespace gf;

namespace {

constexpr int WHEEL_X = 16, WHEEL_Y = 48;
constexpr int CX = 160, CY = 160;          // centro de la rueda, relativo a (16, 48)
constexpr int R_OUT = 152, R_MID = 108, R_IN = 64;
constexpr int HALF_DEG = 14;               // cada segmento ocupa ±14° de sus 30°
constexpr uint32_t LABEL_IDLE = 0xB8B8B8;
const char* const NOTE_NAMES[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};

/// Mezcla de colores (studio-data.js: mix): t = peso de @p a.
uint32_t mix(uint32_t a, uint32_t b, float t) {
    auto ch = [&](int sh) {
        const float va = (a >> sh) & 0xFF, vb = (b >> sh) & 0xFF;
        return uint32_t(std::lround(va * t + vb * (1.0f - t))) << sh;
    };
    return ch(16) | ch(8) | ch(0);
}

/// Segmento anular: un lv_arc estático entre r1 y r2 centrado en el ángulo de la posición Camelot.
lv_obj_t* ring_segment(lv_obj_t* parent, int n, int r1, int r2) {
    lv_obj_t* a = lv_arc_create(parent);
    lv_obj_remove_style_all(a);
    lv_obj_clear_flag(a, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(a, 2 * r2, 2 * r2);
    lv_obj_set_pos(a, CX - r2, CY - r2);
    // El diseño mide desde las 12 en sentido horario; LVGL desde las 3.
    const int center = (n % 12) * 30 - 90;
    lv_arc_set_bg_angles(a, (center - HALF_DEG + 360) % 360, (center + HALF_DEG + 360) % 360);
    lv_obj_set_style_arc_width(a, r2 - r1, LV_PART_MAIN);
    lv_obj_set_style_arc_rounded(a, false, LV_PART_MAIN);
    lv_obj_set_style_arc_opa(a, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_arc_opa(a, LV_OPA_TRANSP, LV_PART_INDICATOR);
    return a;
}

}  // namespace

lv_obj_t* ScreenAi::create() {
    _scr = screen_root();
    _top.create(_scr, "AI / HARMONY", 1);

    // ── Rueda Camelot ────────────────────────────────────────────────────────
    lv_obj_t* wheel = lv_obj_create(_scr);
    lv_obj_remove_style_all(wheel);
    lv_obj_clear_flag(wheel, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_pos(wheel, WHEEL_X, WHEEL_Y);
    lv_obj_set_size(wheel, 2 * CX, 2 * CY);
    for (int n = 1; n <= 12; ++n) {
        for (int ring = 0; ring < 2; ++ring) {
            const bool b = ring == 0;
            const int r1 = b ? R_MID : R_IN;
            const int r2 = b ? R_OUT : R_MID - 2;
            Seg& s = b ? _outer[n - 1] : _inner[n - 1];
            s.arc = ring_segment(wheel, n, r1, r2);
            const float ang = float((n % 12) * 30) * float(M_PI) / 180.0f;
            const float rm = (r1 + r2) / 2.0f;
            char txt[4];
            std::snprintf(txt, sizeof(txt), "%d%c", n, b ? 'B' : 'A');
            s.lbl = label(wheel, T12B, LABEL_IDLE, txt);
            lv_obj_update_layout(s.lbl);
            lv_obj_set_pos(s.lbl, int(CX + rm * std::sin(ang)) - lv_obj_get_width(s.lbl) / 2,
                           int(CY - rm * std::cos(ang)) - 6);
        }
    }

    // Centro: tonalidad
    lv_obj_t* center = lv_obj_create(_scr);
    lv_obj_remove_style_all(center);
    lv_obj_set_pos(center, 112, 144);
    lv_obj_set_size(center, 128, 128);
    lv_obj_set_flex_flow(center, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(center, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(center, 4, 0);
    _key_code = label(center, T64, WHITE, "");
    _key_name = label(center, T16, camelot(8), "");
    _key_conf = label(center, T12, GRAY, "");

    // ── Columna derecha ──────────────────────────────────────────────────────
    auto heading = [&](int x, int y, const char* t) {
        lv_obj_t* l = label(_scr, T12, GRAY, t);
        lv_obj_set_style_text_letter_space(l, 1, 0);
        lv_obj_set_pos(l, x, y);
    };
    heading(360, 48, "CHORD");
    _chord = label(_scr, T40, WHITE, "");
    lv_obj_set_pos(_chord, 360, 68);
    _chord_conf = label(_scr, T12, MON, "");
    heading(584, 48, "GENRE");
    _genre = label(_scr, T40, WHITE, "");
    lv_obj_set_pos(_genre, 584, 68);
    _genre_conf = label(_scr, T12, MON, "");

    heading(360, 128, "CHROMA \xC2\xB7 12 BINS");
    rect(_scr, 360, 148, 424, 64, SURFACE);
    const float slot = (424.0f + 4.0f) / 12.0f;
    for (int i = 0; i < 12; ++i) {
        const int x0 = 360 + int(std::lround(i * slot));
        const int w = int(std::lround((i + 1) * slot)) - int(std::lround(i * slot)) - 4;
        _chroma[i] = rect(_scr, x0, 212, w, 0, TEAL_PLUS);
        lv_obj_t* n = label(_scr, T12, MON, NOTE_NAMES[i]);
        lv_obj_set_width(n, w);
        lv_obj_set_style_text_align(n, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_pos(n, x0, 216);
    }

    lv_obj_t* lock = box(_scr, 360, 240, 424, 64, SURFACE, LINE);
    lv_obj_t* lh = label(lock, T12, GRAY, "SCALE LOCK");
    lv_obj_set_style_text_letter_space(lh, 1, 0);
    lv_obj_set_pos(lh, 16, 10);
    _lock_pill = rect(lock, 16, 32, 32, 20, TEAL);
    _lock_txt = label(_lock_pill, T12B, BG, "ON");
    lv_obj_center(_lock_txt);
    lv_obj_t* sh = label(lock, T12, MON, "LAST SNAP");
    lv_obj_set_style_text_letter_space(sh, 1, 0);
    lv_obj_align(sh, LV_ALIGN_TOP_RIGHT, -16, 8);
    _snap = label(lock, T28, WHITE, "");
    lv_obj_align(_snap, LV_ALIGN_TOP_RIGHT, -16, 26);

    heading(360, 312, "BPM \xC2\xB7 BAR PHASE");
    _bpm = label(_scr, T40, WHITE, "");
    lv_obj_set_pos(_bpm, 360, 332);
    for (int i = 0; i < 16; ++i) {
        const int h = i % 4 == 0 ? 24 : 16;
        _cells[i] = rect(_scr, 784 - (16 - i) * 14 + 4, 372 - h, 10, h, LINE);
    }

    _bottom.create(_scr);
    return _scr;
}

void ScreenAi::paint_wheel(uint8_t cam, bool minor) {
    for (int n = 1; n <= 12; ++n) {
        for (int ring = 0; ring < 2; ++ring) {
            const bool b = ring == 0;
            const bool on = n == cam && (b ? !minor : minor);
            Seg& s = b ? _outer[n - 1] : _inner[n - 1];
            const uint32_t col = camelot(uint8_t(n));
            lv_obj_set_style_arc_color(s.arc, lv_color_hex(on ? col : mix(col, BG, 0.3f)), LV_PART_MAIN);
            set_text_color(s.lbl, on ? BG : LABEL_IDLE);
        }
    }
}

void ScreenAi::update(const StudioState& s) {
    _top.update(s);
    const bool fx = s.mode == Mode::Fx;
    const ModeColors mc = mode_colors(fx);

    const int key = s.key_camelot * 2 + (s.key_minor ? 0 : 1);
    if (key != _last_key) {
        _last_key = key;
        paint_wheel(s.key_camelot, s.key_minor);
        lv_label_set_text_fmt(_key_code, "%u%c", s.key_camelot, s.key_minor ? 'A' : 'B');
        lv_label_set_text(_key_name, s.key_name);
        set_text_color(_key_name, camelot(s.key_camelot));
    }

    char t[64];
    std::snprintf(t, sizeof(t), "%u|%s|%u|%s|%u|%d|%s|%s|%.1f|%d", s.key_conf, s.chord, s.chord_conf, s.genre,
                  s.genre_conf, s.scale_lock, s.snap_from, s.snap_to, s.bpm, fx);
    if (std::strcmp(t, _last_text) != 0) {
        std::snprintf(_last_text, sizeof(_last_text), "%s", t);
        lv_label_set_text_fmt(_key_conf, "KEY %u%%", s.key_conf);
        lv_label_set_text(_chord, s.chord);
        lv_label_set_text_fmt(_chord_conf, "%u%%", s.chord_conf);
        lv_obj_align_to(_chord_conf, _chord, LV_ALIGN_OUT_RIGHT_BOTTOM, 8, -6);
        lv_label_set_text(_genre, s.genre);
        lv_label_set_text_fmt(_genre_conf, "%u%%", s.genre_conf);
        lv_obj_align_to(_genre_conf, _genre, LV_ALIGN_OUT_RIGHT_BOTTOM, 8, -6);
        set_bg(_lock_pill, s.scale_lock ? mc.mc : SURFACE_2);
        set_text_color(_lock_txt, s.scale_lock ? (fx ? WHITE : BG) : MON);
        lv_label_set_text(_lock_txt, s.scale_lock ? "ON" : "OFF");
        lv_label_set_text_fmt(_snap, "%s \xE2\x86\x92 %s", s.snap_from, s.snap_to);
        lv_obj_align(_snap, LV_ALIGN_TOP_RIGHT, -16, 26);
        char bpm[12];
        std::snprintf(bpm, sizeof(bpm), "%.1f", s.bpm);  // lv_snprintf no formatea floats
        lv_label_set_text(_bpm, bpm);
        for (lv_obj_t* c : _chroma) set_bg(c, mc.hi);
        _last_step = -1;
    }

    for (int i = 0; i < 12; ++i) {
        const uint8_t h = uint8_t(std::lround(64.0f * std::clamp(s.chroma[i], 0.05f, 1.0f)));
        if (h == _last_chroma_h[i]) continue;
        _last_chroma_h[i] = h;
        lv_obj_set_y(_chroma[i], 212 - h);
        lv_obj_set_height(_chroma[i], h);
    }

    const int step = std::min(15, int(s.bar_phase * 16.0f));
    if (step != _last_step) {
        _last_step = step;
        for (int i = 0; i < 16; ++i) set_bg(_cells[i], i == step ? mc.hi : (i < step ? mc.mc : LINE));
    }

    _bottom.set_mode(fx, s.shift);
    static const char* const KEYS_SYNTH[4] = {"OSC", "ENV", "LFO / MOD", "PRESET"};
    static const char* const KEYS_FX[4] = {"INSERT", "SEND", "MASTER", "FAV FX"};
    _bottom.set_keys(fx ? KEYS_FX : KEYS_SYNTH, fx ? 0 : -1);
    char v[16];
    std::snprintf(v, sizeof(v), "%d", int(std::lround(s.cutoff_hz)));
    _bottom.set_enc(false, "CUTOFF", v, "HZ", freq_frac(s.cutoff_hz));
    std::snprintf(v, sizeof(v), "%d", int(std::lround(s.resonance * 100.0f)));
    _bottom.set_enc(true, "RES", v, "%", s.resonance);
    std::snprintf(v, sizeof(v), "%d", int(std::lround(s.p1 * 16.0f)));
    _bottom.set_param(1, s.shift ? "ARP GATE" : "ARP RATE", v, "/16", s.p1);
    std::snprintf(v, sizeof(v), "%d", int(std::lround(s.p2 * 100.0f)));
    _bottom.set_param(2, s.shift ? "SWING" : "HUMANIZE", v, "%", s.p2);
    _bottom.set_nav(fx ? "SLOT" : "VIEW");
}
