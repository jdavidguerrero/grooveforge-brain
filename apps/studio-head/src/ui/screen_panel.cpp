#include "ui/screen_panel.h"

#include <cmath>
#include <cstdio>

#include <gf_params.h>

#include "state/osc_catalog.h"
#include "ui/engines.h"
#include "ui/theme.h"

using namespace gf;

namespace {

constexpr int CW = 192, CH = 104, GAP_X = 5, GAP_Y = 6, X0 = 8, Y0 = 50;

void ms(char* out, size_t n, float v) {
    if (v >= 1000.0f) std::snprintf(out, n, "%.1fs", v / 1000.0f);
    else std::snprintf(out, n, "%dms", int(std::lround(v)));
}

/// Dos slots por línea: "NOMBRE valor  NOMBRE valor".
void slot_lines(std::string& out, const OscEngine& e, const OscSlot* const* slots, const float* vals) {
    int col = 0;
    for (int k = 0; k < OSC_SLOTS; ++k) {
        if (!slots[k]) continue;
        char v[16], cell[40];
        osc_format(e, *slots[k], vals[k], v, sizeof(v));
        std::snprintf(cell, sizeof(cell), "%s %s", slots[k]->name, v);
        out += cell;
        out += (++col % 2) ? "  " : "\n";
    }
}

}  // namespace

lv_obj_t* ScreenPanel::create() {
    _scr = screen_root();
    _top.create(_scr, "PANEL", 3);
    for (int i = 0; i < CARDS; ++i) {
        Card& c = _cards[i];
        c.box = rect(_scr, X0 + (i % 4) * (CW + GAP_X), Y0 + (i / 4) * (CH + GAP_Y), CW, CH, SURFACE);
        c.title = label(c.box, T12B, TEAL, "");
        lv_obj_set_style_text_letter_space(c.title, 1, 0);
        lv_obj_set_pos(c.title, 8, 6);
        c.body = label(c.box, T12, GRAY, "");
        lv_obj_set_width(c.body, CW - 12);
        lv_label_set_long_mode(c.body, LV_LABEL_LONG_CLIP);
        lv_obj_set_pos(c.body, 8, 24);
    }
    _bottom.create(_scr);
    return _scr;
}

void ScreenPanel::set_card(int i, const char* title, const std::string& body, bool accent) {
    Card& c = _cards[i];
    if (c.t != title) {
        c.t = title;
        lv_label_set_text(c.title, title);
    }
    if (c.b != body) {
        c.b = body;
        lv_label_set_text(c.body, body.c_str());
    }
    lv_obj_set_style_text_color(c.title, lv_color_hex(accent ? PURPLE_BRIGHT : TEAL), 0);
    if (title[0]) lv_obj_clear_flag(c.box, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(c.box, LV_OBJ_FLAG_HIDDEN);
}

void ScreenPanel::update(const StudioState& s) {
    _top.update(s);
    const uint8_t e = s.engine < GF_ENGINE_COUNT ? s.engine : 0;
    const OscEngine& oe = osc_engine(e);
    int n = 0;
    char t[64];

    // OSC: una tarjeta por parada (el DX7, por operador)
    for (int st = 0; st < oe.stops && n < CARDS; ++st) {
        std::string b;
        if (oe.fm) {
            const float* o = s.osc_val[4][st];
            std::snprintf(t, sizeof(t), "x%.2f  LVL %d\nA %d  D %d\nS %d%%  R %d", double(s.fm_ratio[st]),
                          int(s.fm_level[st]), int(o[4]), int(o[5]), int(o[6]), int(o[7]));
            b = t;
            if (st == 0) {
                std::snprintf(t, sizeof(t), "\nALG %d  FB %d", s.fm_algorithm, s.fm_feedback);
                b += t;
            }
        } else {
            float vals[OSC_SLOTS];
            for (int k = 0; k < OSC_SLOTS; ++k) vals[k] = osc_value(s, e, st, k);
            slot_lines(b, oe, oe.slot[st], vals);
        }
        set_card(n++, oe.src[st].name, b, false);
    }
    // MOD
    for (int st = 0; st < mod_stop_count(e) && n < CARDS; ++st) {
        const ModStop& m = mod_stop(e, st);
        std::string b;
        slot_lines(b, oe, m.slot, s.mod_val[e][st]);
        set_card(n++, m.name, b, true);
    }
    // ENV AMP y FILTER
    char a[12], d[12], r[12];
    ms(a, sizeof(a), s.amp_attack_ms); ms(d, sizeof(d), s.amp_decay_ms); ms(r, sizeof(r), s.amp_release_ms);
    std::snprintf(t, sizeof(t), "A %s  D %s\nS %d%%  R %s", a, d, int(std::lround(s.amp_sustain * 100)), r);
    set_card(n++, "ENV AMP", t, false);
    ms(a, sizeof(a), s.env_attack_ms); ms(d, sizeof(d), s.env_decay_ms); ms(r, sizeof(r), s.env_release_ms);
    std::snprintf(t, sizeof(t), "A %s  D %s\nS %d%%  R %s", a, d, int(std::lround(s.env_sustain * 100)), r);
    set_card(n++, "ENV FILTER", t, false);
    // FILTER
    std::snprintf(t, sizeof(t), "CUTOFF %d HZ\nRES %d%%  ENV %d%%\nGLIDE %d%%  DRIVE %d%%", int(std::lround(s.cutoff_hz)),
                  int(std::lround(s.resonance * 100)), int(std::lround(s.p1 * 100)), int(std::lround(s.glide * 100)),
                  int(std::lround(s.drive * 100)));
    set_card(n++, s.filter_bypass ? "FILTER (BYPASS)" : "FILTER", t, false);
    // LFO
    static const char* const SHAPES[5] = {"SINE", "TRI", "SAW", "SQR", "S&H"};
    static const char* const DIVS[4] = {"1/4", "1/8", "1/16", "1/8T"};
    char rate[16];
    if (s.lfo_sync) std::snprintf(rate, sizeof(rate), "%s", DIVS[s.lfo_div % 4]);
    else std::snprintf(rate, sizeof(rate), "%.2f HZ", double(s.lfo_rate_hz));
    std::snprintf(t, sizeof(t), "%s  %s\nCUT %d%%  PITCH %d%%\nAMP %d%%  PWM %d%%", SHAPES[int(s.lfo_shape) % 5], rate,
                  int(std::lround(s.lfo_amt[0] * 100)), int(std::lround(s.lfo_amt[1] * 100)),
                  int(std::lround(s.lfo_amt[2] * 100)), int(std::lround(s.lfo_amt[3] * 100)));
    if (n < CARDS) set_card(n++, "LFO", t, false);
    // SYNTH FX
    static const char* const SFX[4] = {"BIT", "TAPE", "CHORUS", "MODAL"};
    std::string fx;
    for (int i = 0; i < 4; ++i) {
        std::snprintf(t, sizeof(t), "%-7s%s  MIX %d%%\n", SFX[i], s.sfx_on[i] ? "ON " : "OFF",
                      int(std::lround(s.sfx_mix[i] * 100)));
        fx += t;
    }
    if (n < CARDS) set_card(n++, "SYNTH FX", fx, false);
    for (; n < CARDS; ++n) set_card(n, "", "", false);

    _bottom.set_mode(s.mode == Mode::Fx, s.shift);
    static const char* const KEYS[4] = {"OSC", "ENV", "LFO / MOD", "SYNTH FX"};
    _bottom.set_keys(KEYS, -1);
    _bottom.set_filter_encs(s.cutoff_hz, s.resonance);
    char v[16];
    std::snprintf(v, sizeof(v), "%d", int(std::lround(s.p1 * 100.0f)));
    _bottom.set_param(1, "FLT ENV AMT", v, "%", s.p1);
    std::snprintf(v, sizeof(v), "%d", int(std::lround(s.p2 * 100.0f)));
    _bottom.set_param(2, "LFO\xE2\x86\x92" "CUTOFF", v, "%", s.p2);
    _bottom.set_nav("VIEW");
}
