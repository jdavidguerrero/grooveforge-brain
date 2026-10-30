#include "juno_preview.h"
#include "juno_preset.h"
#include "ui/page_params.h"
#include <cmath>
#include <cstdio>

static bool run(double rate) {
    gf::JunoPreview a, b;
    a.prepare(rate); b.prepare(rate);
    a.note_on(60, 0.8f);
    float energy = 0, difference = 0;
    for (int i = 0; i < int(rate / 2); ++i) {
        float l, r, silent_l, silent_r;
        a.process_frame(l, r); b.process_frame(silent_l, silent_r);
        if (!std::isfinite(l) || !std::isfinite(r) || silent_l != 0 || silent_r != 0) return false;
        energy += l * l + r * r;
        difference += std::abs(l - r);
    }
    if (energy < 0.01f || difference < 0.01f) return false;
    a.note_off(60);
    for (int i = 0; i < int(rate * 4); ++i) { float l, r; a.process_frame(l, r); if (!std::isfinite(l + r)) return false; }
    float l, r; a.process_frame(l, r);
    return std::abs(l) < 0.0001f && std::abs(r) < 0.0001f;
}
static bool voice_modulation() {
    gf::JunoPreview a, b, c;
    a.prepare(44100); b.prepare(44100); c.prepare(44100);
    a.set_voice_modulation(0, 3, 30, 0.8f);
    b.set_voice_modulation(0, 3, 30, 0.8f);
    c.set_voice_modulation(0, 1, 0, 0);
    a.set_sustain(true); b.set_sustain(true);
    a.note_on(60, .8f); b.note_on(60, .8f); c.note_on(60, .8f);
    a.note_off(60); b.note_off(60);
    float diff = 0, energy = 0;
    for (int i = 0; i < 22050; ++i) {
        float al, ar, bl, br, cl, cr;
        a.process_frame(al, ar); b.process_frame(bl, br); c.process_frame(cl, cr);
        if (std::abs(al - bl) > 1e-7f || std::abs(ar - br) > 1e-7f) return false;
        diff += std::abs(al - cl); energy += al * al;
    }
    if (diff < 0.01f || energy < 0.001f) return false;
    a.set_sustain(false);
    for (int i = 0; i < 180000; ++i) { float l, r; a.process_frame(l, r); }
    float l, r; a.process_frame(l, r);
    if (std::abs(l) > .0001f || std::abs(r) > .0001f) return false;
    gf::JunoPreview glide, instant;
    glide.prepare(44100); instant.prepare(44100);
    glide.set_voice_modulation(400, 1, 0, 0);
    instant.set_voice_modulation(0, 1, 0, 0);
    for (int note = 48; note < 54; ++note) { glide.note_on(uint8_t(note), 0.8f); instant.note_on(uint8_t(note), 0.8f); }
    for (int i = 0; i < 2000; ++i) { float x, y; glide.process_frame(x,y); instant.process_frame(x,y); }
    glide.note_on(72, .8f); instant.note_on(72, .8f);
    float delta = 0;
    for (int i = 0; i < 4000; ++i) { float al, ar, bl, br; glide.process_frame(al, ar); instant.process_frame(bl, br); delta += std::abs(al - bl); }
    return delta > .01f;
}
static bool preset_unison() {
    studio::Preset single, multiple;
    studio::patch_init(single.patch, 1);
    multiple = single;
    multiple.patch.groove_on[GF_GROOVE_UNISON] = true;
    multiple.patch.groove[GF_GROOVE_UNISON][GF_UNISON_VOICES] = 2;
    multiple.patch.groove[GF_GROOVE_UNISON][GF_UNISON_DETUNE] = 30;
    gf::JunoPreview a, b;
    a.prepare(44100); b.prepare(44100);
    gf::apply_juno_preset_preview(a, single);
    gf::apply_juno_preset_preview(b, multiple);
    a.note_on(60, 0.8f); b.note_on(60, 0.8f);
    float difference = 0;
    for (int i = 0; i < 12000; ++i) {
        float al, ar, bl, br;
        a.process_frame(al, ar); b.process_frame(bl, br);
        difference += std::abs(al - bl);
    }
    return difference > 0.01f;
}
static bool lfo_modulation() {
    studio::Preset plain, modulated;
    studio::patch_init(plain.patch, 1);
    plain.patch.osc[0][0] = 2; plain.patch.osc[0][2] = 0; plain.patch.mod[0][1] = 0;
    modulated = plain;
    modulated.patch.lfo_rate_hz = 7; modulated.patch.lfo_amt[GF_DEST_PWM] = 0.9f;
    modulated.patch.lfo_amt[GF_DEST_PITCH] = 0.7f; modulated.patch.lfo_amt[GF_DEST_CUTOFF] = 0.4f;
    modulated.patch.mod[0][0] = 50;
    gf::JunoPreview a, b, c;
    a.prepare(48000); b.prepare(48000); c.prepare(48000);
    gf::apply_juno_preset_preview(a, plain);
    gf::apply_juno_preset_preview(b, modulated);
    gf::apply_juno_preset_preview(c, modulated);
    b.set_lfo_control(0, 120, true); c.set_lfo_control(0, 120, true);
    a.note_on(60, .8f); b.note_on(60, .8f); c.note_on(60, .8f);
    float difference = 0;
    for (int i = 0; i < 20000; ++i) {
        float al, ar, bl, br, cl, cr;
        a.process_frame(al, ar); b.process_frame(bl, br); c.process_frame(cl, cr);
        if (std::abs(bl - cl) > 1e-7f || !std::isfinite(bl + br)) return false;
        difference += std::abs(al - bl);
    }
    return difference > 0.01f;
}
int main() {
    for (double rate : {44100.0, 48000.0, 96000.0, 192000.0, 384000.0})
        if (!run(rate)) { std::fprintf(stderr, "Juno preview failed at %.0f Hz\n", rate); return 1; }
    if (!lfo_modulation()) { std::fprintf(stderr, "Juno LFO mapping failed\n"); return 1; }
    if (!preset_unison()) { std::fprintf(stderr, "Juno preset unison mapping failed\n"); return 1; }
    if (!voice_modulation()) { std::fprintf(stderr, "Juno voice modulation failed\n"); return 1; }
    return 0;
}
