#include "preset_bank.h"

#include <stdio.h>
#include <string.h>

namespace studio {

namespace {

constexpr uint8_t MAGIC[3] = {'G', 'F', 'P'};

/// Recorre los campos del preset en el orden del archivo; el mismo código escribe y lee (sin desfasajes).
template <typename F, typename B>
void visit(Preset& p, F&& f32, B&& u8) {
    Patch& t = p.patch;
    float* const scalars[12] = {&t.attack_ms, &t.decay_ms,   &t.sustain,   &t.release_ms, &t.f_attack_ms,
                                &t.f_decay_ms, &t.f_sustain, &t.f_release_ms, &t.env_amt,   &t.glide,
                                &t.drive,     &t.lfo_rate_hz};
    for (float* s : scalars) f32(*s);
    for (float& a : t.lfo_amt) f32(a);
    for (auto& st : t.osc)
        for (float& v : st) f32(v);
    for (auto& st : t.mod)
        for (float& v : st) f32(v);
    for (float& m : t.sfx_mix) f32(m);
    for (auto& fx : t.sfx_p)
        for (float& v : fx) f32(v);
    f32(p.cutoff_hz);
    f32(p.reso_norm);
    u8(t.lfo_shape);
    u8(t.lfo_div);
    for (bool& on : t.sfx_on) {
        uint8_t b = on;
        u8(b);
        on = b != 0;
    }
}

void set_name(char* dst, const char* src) {
    strncpy(dst, src, PRESET_NAME_LEN);
    dst[PRESET_NAME_LEN] = '\0';
}

/// Un patch de fábrica: lo que cambia respecto de patch_init() del engine.
struct Factory {
    const char* name;
    float a, d, s, r;            ///< ENV AMP (ms, ms, 0–1, ms)
    float fa, fd, fs, fr;        ///< ENV FILTER
    float env_amt, cutoff_hz, reso, glide;
    float lfo_hz;
    uint8_t lfo_shape;
    float amt_cut, amt_pitch, amt_third;
    void (*osc)(Patch&);         ///< página OSC / MOD (nullptr = la de patch_init)
};

using O = float (&)[GF_OSC_STOPS][GF_OSC_SLOTS];

// theory/06 §8. Waves: MODEL D / OB-6 0 SAW 1 TRI 2 SQR 3 PULSE · JUNO 0 SAW 1 SQR 2 PULSE 3 SAW+PLS ·
// ARP 0 SAW 1 TRI 2 SQR 3 PULSE 4 SINE. DX7 por operador: RATIO · LEVEL · ALG · FB | A · D · S % · R.
const Factory MODEL_D[] = {
    {"INIT", 5, 300, 0.7f, 300, 5, 400, 0.3f, 300, 0.3f, 800, 0.15f, 0, 2, 1, 0, 0, 0, nullptr},
    {"FAT BASS", 2, 400, 0.6f, 120, 2, 350, 0.15f, 150, 0.45f, 300, 0.25f, 0, 2, 1, 0, 0, 0,
     [](Patch& p) { O o = p.osc; o[0][0] = 0; o[1][0] = 0; o[2][0] = 2; o[1][1] = 6; o[2][1] = -12;
                    o[0][2] = 90; o[1][2] = 80; o[2][2] = 70; o[0][3] = 0; }},
    {"70S LEAD", 3, 500, 0.8f, 250, 3, 600, 0.5f, 300, 0.35f, 1200, 0.4f, 0.35f, 5.5f, 0, 0, 0, 0,
     [](Patch& p) { O o = p.osc; o[0][0] = 0; o[1][0] = 0; o[2][0] = 2; o[1][1] = 9; o[2][1] = 12;
                    o[0][2] = 80; o[1][2] = 75; o[2][2] = 40; o[0][3] = 0; }},
    {"BRASS", 60, 600, 0.7f, 250, 80, 700, 0.4f, 250, 0.5f, 500, 0.1f, 0, 2, 1, 0, 0, 0,
     [](Patch& p) { O o = p.osc; o[0][0] = 0; o[1][0] = 0; o[2][0] = 0; o[1][1] = 7; o[2][1] = 12;
                    o[0][2] = 80; o[1][2] = 80; o[2][2] = 50; o[0][3] = 0; }},
    {"FLUTE", 80, 200, 0.9f, 200, 60, 300, 0.7f, 200, 0.1f, 1500, 0.1f, 0, 5, 0, 0, 0.12f, 0,
     [](Patch& p) { O o = p.osc; o[0][0] = 1; o[0][2] = 80; o[1][2] = 0; o[2][2] = 0; o[0][3] = 15; }},
    {"PLUCK", 1, 250, 0.0f, 200, 1, 200, 0.0f, 200, 0.6f, 250, 0.35f, 0, 2, 1, 0, 0, 0,
     [](Patch& p) { O o = p.osc; o[0][0] = 2; o[1][0] = 0; o[1][1] = 4; o[0][2] = 80; o[1][2] = 70; o[2][2] = 0;
                    o[0][3] = 0; }},
};

const Factory JUNO[] = {
    {"INIT", 5, 300, 0.7f, 300, 5, 400, 0.3f, 300, 0.3f, 1000, 0.15f, 0, 2, 1, 0, 0, 0, nullptr},
    {"STRINGS", 300, 800, 0.85f, 900, 300, 800, 0.6f, 900, 0.1f, 2500, 0.05f, 0, 2, 1, 0, 0, 0,
     [](Patch& p) { O o = p.osc; o[0][0] = 0; o[0][1] = 0; o[0][2] = 0; o[0][4] = 2; o[0][5] = 2; }},
    {"PWM PAD", 400, 1000, 0.8f, 1200, 400, 1000, 0.5f, 1200, 0.15f, 1800, 0.15f, 0, 0.6f, 1, 0, 0, 0.6f,
     [](Patch& p) { O o = p.osc; o[0][0] = 2; o[0][1] = 20; o[0][2] = 0; o[0][3] = 50; o[0][4] = 1; o[0][5] = 1; }},
    {"SUB BASS", 1, 300, 0.7f, 80, 1, 250, 0.2f, 100, 0.35f, 400, 0.2f, 0, 2, 1, 0, 0, 0,
     [](Patch& p) { O o = p.osc; o[0][0] = 1; o[0][1] = 100; o[0][2] = 0; o[0][4] = 0; o[0][5] = 0; }},
    {"BRASS STAB", 15, 300, 0.5f, 200, 20, 300, 0.3f, 200, 0.55f, 600, 0.15f, 0, 2, 1, 0, 0, 0,
     [](Patch& p) { O o = p.osc; o[0][0] = 3; o[0][1] = 30; o[0][2] = 0; o[0][3] = 40; o[0][4] = 1; o[0][5] = 1; }},
    {"PLUCK KEYS", 1, 600, 0.0f, 300, 1, 400, 0.0f, 300, 0.55f, 500, 0.2f, 0, 2, 1, 0, 0, 0,
     [](Patch& p) { O o = p.osc; o[0][0] = 0; o[0][1] = 30; o[0][2] = 0; o[0][4] = 1; o[0][5] = 1; }},
};

const Factory PROPHET[] = {
    {"INIT", 5, 300, 0.7f, 300, 5, 400, 0.3f, 300, 0.3f, 1000, 0.15f, 0, 2, 1, 0, 0, 0, nullptr},
    {"SYNC LEAD", 2, 500, 0.8f, 300, 2, 600, 0.5f, 300, 0.35f, 2000, 0.2f, 0, 2, 1, 0, 0, 0,
     [](Patch& p) { O o = p.osc; o[0][0] = 12; o[0][2] = 1; o[0][3] = 0; o[0][4] = 1;
                    o[1][0] = 0; o[1][4] = 1; o[1][6] = 0; o[2][0] = 80; o[2][1] = 20;
                    p.mod[0][0] = 40; p.mod[0][2] = 1; }},
    {"POLY BRASS", 40, 700, 0.75f, 300, 50, 800, 0.4f, 300, 0.45f, 450, 0.1f, 0, 2, 1, 0, 0, 0,
     [](Patch& p) { O o = p.osc; o[0][2] = 1; o[0][3] = 0; o[1][0] = 0; o[1][1] = 6; o[1][4] = 1; o[1][6] = 0;
                    o[2][0] = 70; o[2][1] = 70; o[2][4] = 1; }},
    {"POLYMOD BELL", 1, 1500, 0.0f, 1200, 1, 1200, 0.0f, 1000, 0.3f, 3000, 0.05f, 0, 2, 1, 0, 0, 0,
     [](Patch& p) { O o = p.osc; o[0][2] = 0; o[0][3] = 1; o[0][1] = 50; o[1][0] = 19; o[1][4] = 0; o[1][5] = 1;
                    o[1][6] = 0; o[2][0] = 80; o[2][1] = 0; o[2][4] = 2; p.mod[0][1] = 60; p.mod[0][2] = 1; }},
    {"STRING PAD", 500, 1000, 0.85f, 1200, 500, 1000, 0.6f, 1200, 0.1f, 2200, 0.05f, 0, 2, 1, 0, 0, 0,
     [](Patch& p) { O o = p.osc; o[0][2] = 1; o[0][3] = 0; o[1][0] = 0; o[1][1] = 8; o[1][4] = 1; o[1][6] = 0;
                    o[2][0] = 60; o[2][1] = 60; }},
    {"WARM KEYS", 2, 900, 0.4f, 400, 2, 600, 0.2f, 400, 0.4f, 700, 0.15f, 0, 2, 1, 0, 0, 0,
     [](Patch& p) { O o = p.osc; o[0][1] = 40; o[0][2] = 0; o[0][3] = 1; o[1][0] = -12; o[1][4] = 1; o[1][6] = 0;
                    o[2][0] = 60; o[2][1] = 40; o[2][4] = 1; }},
};

const Factory OB6[] = {
    {"INIT", 5, 300, 0.7f, 300, 5, 400, 0.3f, 300, 0.3f, 2000, 0.15f, 0, 2, 1, 0, 0, 0, nullptr},
    {"JUMP BRASS", 10, 400, 0.8f, 300, 15, 500, 0.5f, 300, 0.4f, 900, 0.1f, 0, 2, 1, 0, 0, 0,
     [](Patch& p) { O o = p.osc; o[0][0] = 0; o[1][0] = 0; o[1][1] = 0; o[1][5] = 6; o[0][1] = 0;
                    o[0][2] = 80; o[1][2] = 80; }},
    {"BP PAD", 600, 1200, 0.9f, 1500, 600, 1200, 0.6f, 1500, 0.15f, 1500, 0.3f, 0, 0.4f, 0, 0, 0, 0,
     [](Patch& p) { O o = p.osc; o[0][0] = 0; o[1][0] = 3; o[1][1] = 0; o[1][5] = 7; o[0][1] = 0; o[0][7] = 1; }},
    {"XMOD LEAD", 2, 500, 0.8f, 250, 2, 500, 0.5f, 250, 0.3f, 2500, 0.25f, 0, 2, 1, 0, 0, 0,
     [](Patch& p) { O o = p.osc; o[0][0] = 0; o[1][0] = 1; o[1][1] = 7; o[0][1] = 0; o[0][2] = 80; o[1][2] = 40;
                    p.mod[0][1] = 30; p.mod[0][2] = 1; }},
    {"SAW STRINGS", 400, 800, 0.85f, 1000, 400, 800, 0.7f, 1000, 0.1f, 3000, 0.05f, 0, 2, 1, 0, 0, 0,
     [](Patch& p) { O o = p.osc; o[0][0] = 0; o[1][0] = 0; o[1][1] = 0; o[1][5] = 9; o[0][1] = 0; }},
    {"PULSE BASS", 1, 300, 0.6f, 100, 1, 250, 0.1f, 100, 0.5f, 300, 0.2f, 0, 2, 1, 0, 0, 0,
     [](Patch& p) { O o = p.osc; o[0][0] = 3; o[1][0] = 3; o[1][1] = -12; o[0][4] = 30; o[1][4] = 30;
                    o[0][1] = 60; }},
};

void dx7_ops(Patch& p, const float (&op)[4][8]) {
    for (int k = 0; k < 4; ++k)
        for (int s = 0; s < 8; ++s) p.osc[k][s] = op[k][s];
}

const Factory DX7[] = {
    {"E.PIANO", 5, 300, 0.7f, 300, 5, 400, 0.3f, 300, 0.3f, 15000, 0.15f, 0, 2, 1, 0, 0, 0, nullptr},
    {"FM BASS", 0, 600, 0.5f, 100, 5, 400, 0.3f, 300, 0.3f, 15000, 0.15f, 0, 2, 1, 0, 0, 0,
     [](Patch& p) { dx7_ops(p, {{1, 99, 1, 5, 0, 800, 40, 100}, {1, 80, 0, 0, 0, 400, 20, 80},
                                {1, 60, 0, 0, 0, 300, 0, 80}, {1, 50, 0, 0, 0, 200, 0, 60}}); }},
    {"BELLS", 0, 3000, 0.0f, 2500, 5, 400, 0.3f, 300, 0.3f, 15000, 0.15f, 0, 2, 1, 0, 0, 0,
     [](Patch& p) { dx7_ops(p, {{1, 99, 5, 0, 0, 3000, 0, 2500}, {3.5f, 70, 0, 0, 0, 2000, 0, 2000},
                                {1, 90, 0, 0, 0, 2500, 0, 2000}, {14, 50, 0, 0, 0, 600, 0, 500}}); }},
    {"BRASS FM", 60, 800, 0.85f, 300, 5, 400, 0.3f, 300, 0.3f, 15000, 0.15f, 0, 2, 1, 0, 0, 0,
     [](Patch& p) { dx7_ops(p, {{1, 99, 3, 6, 60, 800, 85, 300}, {1, 70, 0, 0, 80, 900, 70, 300},
                                {1, 55, 0, 0, 60, 600, 60, 300}, {1, 60, 0, 0, 50, 400, 50, 300}}); }},
    {"MARIMBA", 0, 500, 0.0f, 300, 5, 400, 0.3f, 300, 0.3f, 15000, 0.15f, 0, 2, 1, 0, 0, 0,
     [](Patch& p) { dx7_ops(p, {{1, 99, 5, 0, 0, 500, 0, 300}, {4, 75, 0, 0, 0, 80, 0, 80},
                                {4, 60, 0, 0, 0, 300, 0, 200}, {10, 50, 0, 0, 0, 40, 0, 40}}); }},
    {"ORGAN", 5, 100, 1.0f, 80, 5, 400, 0.3f, 300, 0.3f, 15000, 0.15f, 0, 6, 0, 0, 0.06f, 0,
     [](Patch& p) { dx7_ops(p, {{0.5f, 85, 8, 0, 5, 100, 100, 80}, {1, 99, 0, 0, 5, 100, 100, 80},
                                {2, 80, 0, 0, 5, 100, 100, 80}, {3, 70, 0, 0, 5, 100, 100, 80}}); }},
};

const Factory ARP[] = {
    {"INIT", 5, 300, 0.7f, 300, 5, 400, 0.3f, 300, 0.3f, 2000, 0.15f, 0, 2, 1, 0, 0, 0, nullptr},
    {"RING BELL", 1, 1200, 0.0f, 900, 1, 1000, 0.0f, 900, 0.2f, 6000, 0.05f, 0, 2, 1, 0, 0, 0,
     [](Patch& p) { O o = p.osc; o[0][0] = 4; o[1][0] = 4; o[1][1] = 2.75f; o[0][2] = 90; o[0][3] = 0;
                    o[0][4] = 20; o[1][4] = 10; }},
    {"SQUARE BASS", 1, 300, 0.6f, 100, 1, 200, 0.1f, 100, 0.5f, 350, 0.3f, 0, 2, 1, 0, 0, 0,
     [](Patch& p) { O o = p.osc; o[0][0] = 2; o[1][0] = 0; o[1][1] = 0.5f; o[0][2] = 0; o[0][3] = 0;
                    o[0][4] = 80; o[1][4] = 60; }},
    {"SEQ LEAD", 2, 400, 0.7f, 200, 2, 400, 0.4f, 200, 0.35f, 1500, 0.45f, 0.25f, 2, 1, 0, 0, 0,
     [](Patch& p) { O o = p.osc; o[0][0] = 0; o[1][0] = 3; o[1][1] = 2; o[0][2] = 0; o[0][3] = 0;
                    o[0][4] = 80; o[1][4] = 50; }},
    {"S&H BURBLE", 2, 300, 0.8f, 300, 2, 300, 0.5f, 300, 0.2f, 800, 0.55f, 0, 6, 4, 0.35f, 0, 0,
     [](Patch& p) { O o = p.osc; o[0][0] = 0; o[1][0] = 0; o[1][1] = 1; o[0][2] = 0; o[0][3] = 0;
                    o[0][4] = 80; o[1][4] = 0; }},
    {"PERC BLIP", 0, 120, 0.0f, 120, 0, 100, 0.0f, 100, 0.7f, 300, 0.4f, 0, 2, 1, 0, 0, 0,
     [](Patch& p) { O o = p.osc; o[0][0] = 1; o[1][0] = 4; o[1][1] = 1.5f; o[0][2] = 30; o[0][3] = 0;
                    o[0][4] = 70; o[1][4] = 50; }},
};

struct Bank {
    const Factory* v;
    uint8_t n;
};
const Bank BANKS[GF_ENGINE_COUNT] = {{MODEL_D, 6}, {JUNO, 6}, {PROPHET, 6}, {OB6, 6}, {DX7, 6}, {ARP, 6}};

}  // namespace

size_t preset_encode(const Preset& src, uint8_t engine, uint8_t* buf, size_t cap) {
    if (cap < PRESET_BYTES) return 0;
    Preset p = src;
    size_t n = 0;
    memcpy(buf, MAGIC, 3);
    buf[3] = PRESET_FORMAT;
    buf[4] = engine;
    memset(buf + 5, 0, PRESET_NAME_LEN);
    memcpy(buf + 5, p.name, strnlen(p.name, PRESET_NAME_LEN));
    n = 5 + PRESET_NAME_LEN;
    visit(p, [&](float& v) { memcpy(buf + n, &v, 4); n += 4; }, [&](uint8_t& b) { buf[n++] = b; });
    return n;
}

bool preset_decode(const uint8_t* buf, size_t len, uint8_t engine, Preset* out) {
    if (len != PRESET_BYTES || memcmp(buf, MAGIC, 3) != 0 || buf[3] != PRESET_FORMAT || buf[4] != engine) return false;
    Preset p;
    memcpy(p.name, buf + 5, PRESET_NAME_LEN);
    p.name[PRESET_NAME_LEN] = '\0';
    size_t n = 5 + PRESET_NAME_LEN;
    visit(p, [&](float& v) { memcpy(&v, buf + n, 4); n += 4; }, [&](uint8_t& b) { b = buf[n++]; });
    p.patch.lfo_shape = p.patch.lfo_shape < GF_LFO_SHAPES ? p.patch.lfo_shape : 0;
    p.patch.lfo_div = p.patch.lfo_div < GF_LFO_DIVS ? p.patch.lfo_div : 0;
    *out = p;
    return true;
}

uint8_t factory_presets(uint8_t e, Preset* out, uint8_t cap) {
    if (e >= GF_ENGINE_COUNT) return 0;
    const Bank& b = BANKS[e];
    uint8_t n = 0;
    for (; n < b.n && n < cap; ++n) {
        const Factory& f = b.v[n];
        Preset& p = out[n];
        p = Preset{};
        patch_init(p.patch, e);
        set_name(p.name, f.name);
        Patch& t = p.patch;
        t.attack_ms = f.a; t.decay_ms = f.d; t.sustain = f.s; t.release_ms = f.r;
        t.f_attack_ms = f.fa; t.f_decay_ms = f.fd; t.f_sustain = f.fs; t.f_release_ms = f.fr;
        t.env_amt = f.env_amt;
        t.glide = f.glide;
        t.lfo_rate_hz = f.lfo_hz;
        t.lfo_shape = f.lfo_shape;
        t.lfo_amt[GF_DEST_CUTOFF] = f.amt_cut;
        t.lfo_amt[GF_DEST_PITCH] = f.amt_pitch;
        if (GF_LFO_THIRD_DEST[e] != GF_DEST_NONE) t.lfo_amt[GF_LFO_THIRD_DEST[e]] = f.amt_third;
        p.cutoff_hz = f.cutoff_hz;
        p.reso_norm = f.reso;
        if (f.osc) f.osc(t);
    }
    return n;
}

const char* PresetBank::name(uint8_t e, uint8_t i) const {
    return e < GF_ENGINE_COUNT && i < count_[e] ? names_[e][i] : "";
}

void PresetBank::scan(uint8_t e) {
    if (e >= GF_ENGINE_COUNT) return;
    uint8_t buf[PRESET_BYTES];
    Preset p;
    count_[e] = 0;
    // El banco es contiguo (save_new agrega al final): el primer hueco o archivo inválido lo termina
    for (uint8_t i = 0; i < PRESET_MAX; ++i) {
        const size_t len = storage_.read(e, i, buf, sizeof(buf));
        if (!preset_decode(buf, len, e, &p)) break;
        set_name(names_[e][i], p.name);
        count_[e] = uint8_t(i + 1);
    }
    if (count_[e]) return;
    Preset fac[PRESET_MAX];
    const uint8_t n = factory_presets(e, fac, PRESET_MAX);
    for (uint8_t i = 0; i < n; ++i) {
        if (preset_encode(fac[i], e, buf, sizeof(buf)) == 0 || !storage_.write(e, i, buf, PRESET_BYTES)) break;
        set_name(names_[e][i], fac[i].name);
        count_[e] = uint8_t(i + 1);
    }
}

bool PresetBank::load(uint8_t e, uint8_t i, Preset* out) {
    if (e >= GF_ENGINE_COUNT || i >= count_[e]) return false;
    uint8_t buf[PRESET_BYTES];
    return preset_decode(buf, storage_.read(e, i, buf, sizeof(buf)), e, out);
}

int PresetBank::save_new(uint8_t e, Preset p) {
    if (e >= GF_ENGINE_COUNT || count_[e] >= PRESET_MAX) return -1;
    const uint8_t i = count_[e];
    snprintf(p.name, sizeof(p.name), "USER %02u", unsigned(i + 1));
    uint8_t buf[PRESET_BYTES];
    if (preset_encode(p, e, buf, sizeof(buf)) == 0 || !storage_.write(e, i, buf, PRESET_BYTES)) return -1;
    set_name(names_[e][i], p.name);
    count_[e] = uint8_t(i + 1);
    return i;
}

}  // namespace studio
