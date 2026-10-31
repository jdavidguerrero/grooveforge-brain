#include "ob6_preview.h"
#include "ob6_preset.h"
#include <cmath>
#include <cstdio>

static bool run(double fs) {
    gf::Ob6Preview a, b;
    a.prepare(fs); b.prepare(fs);
    a.set_sustain(true);
    for (uint8_t n : {48, 55, 60, 64, 67, 72, 76}) a.note_on(n, .8f);
    for (uint8_t n : {48, 55, 60, 64, 67, 72, 76}) a.note_off(n);
    double energy = 0;
    for (int i = 0; i < int(fs * .25); ++i) {
        const float x = a.process_sample(), silent = b.process_sample();
        if (!std::isfinite(x) || silent != 0) return false;
        energy += x*x;
    }
    if (energy < .01) return false;
    a.set_sustain(false);
    for (int i = 0; i < int(fs * 5); ++i) if (!std::isfinite(a.process_sample())) return false;
    return std::abs(a.process_sample()) < 1e-5f;
}
static bool filter_modes_change_audio() {
    gf::Ob6Preview low, high, bp;
    low.prepare(44100); high.prepare(44100); bp.prepare(44100);
    gf::Ob6SoundPatch p;
    low.set_sound_patch(p); p.morph = 1; high.set_sound_patch(p); p.bandpass = true; bp.set_sound_patch(p);
    low.note_on(60, .8f); high.note_on(60, .8f); bp.note_on(60, .8f);
    float high_delta = 0, bp_delta = 0;
    for (int i = 0; i < 8192; ++i) {
        const float a = low.process_sample(), b = high.process_sample(), c = bp.process_sample();
        if (!std::isfinite(a + b + c)) return false;
        high_delta += std::abs(a - b); bp_delta += std::abs(a - c);
    }
    return high_delta > .01f && bp_delta > .01f;
}
static bool sync_and_xmod() {
    gf::Ob6Preview master_sync, master_free;
    master_sync.prepare(44100); master_free.prepare(44100);
    gf::Ob6SoundPatch p;
    p.level1 = 0; p.level2 = .8f; p.sub = 0; p.interval = 7; p.detune = 0;
    master_free.set_sound_patch(p); p.sync = true; master_sync.set_sound_patch(p);
    master_sync.set_parameters(15000, .707f, 5, 300); master_free.set_parameters(15000, .707f, 5, 300);
    master_sync.note_on(69, .8f); master_free.note_on(69, .8f);
    int crossings_sync = 0, crossings_free = 0;
    float old_sync = 0, old_free = 0;
    for (int i = 0; i < 30000; ++i) {
        const float a = master_sync.process_sample(), b = master_free.process_sample();
        if (!std::isfinite(a + b)) return false;
        if (i >= 5000) { if (old_sync < 0 && a >= 0) ++crossings_sync; if (old_free < 0 && b >= 0) ++crossings_free; }
        old_sync = a; old_free = b;
    }
    if (crossings_sync < 220 || crossings_sync > 280 || crossings_free < 340 || crossings_free > 410) {
        std::fprintf(stderr, "OB-6 sync master crossings %d %d\n", crossings_sync, crossings_free);
        return false;
    }
    gf::Ob6Preview plain, mod_freq, mod_pw, mod_morph;
    plain.prepare(44100); mod_freq.prepare(44100); mod_pw.prepare(44100); mod_morph.prepare(44100);
    p = gf::Ob6SoundPatch{}; p.shape1 = .6f; p.shape2 = .3f;
    plain.set_sound_patch(p);
    p.xmod_vco2 = .8f; p.xmod_env = .5f; p.xmod_freq = true; mod_freq.set_sound_patch(p);
    p.xmod_freq = false; p.xmod_pw = true; mod_pw.set_sound_patch(p);
    p.xmod_pw = false; p.xmod_morph = true; mod_morph.set_sound_patch(p);
    for (auto* engine : {&plain, &mod_freq, &mod_pw, &mod_morph}) engine->note_on(60, .8f);
    double freq_diff = 0, pw_diff = 0, morph_diff = 0;
    for (int i = 0; i < 12000; ++i) {
        const float a = plain.process_sample(), b = mod_freq.process_sample(), c = mod_pw.process_sample(), d = mod_morph.process_sample();
        if (!std::isfinite(a + b + c + d)) return false;
        freq_diff += std::abs(a - b); pw_diff += std::abs(a - c); morph_diff += std::abs(a - d);
    }
    return freq_diff > .01 && pw_diff > .01 && morph_diff > .01;
}

static bool gfp_xmod_mapping() {
    studio::Preset neutral, modulated;
    studio::patch_init(neutral.patch, 3);
    neutral.patch.mod[0][0] = neutral.patch.mod[0][1] = 0;
    for (int i = 2; i < 6; ++i) neutral.patch.mod[0][i] = 0;
    modulated = neutral;
    modulated.patch.mod[0][0] = 50; modulated.patch.mod[0][1] = 80;
    modulated.patch.mod[0][2] = 1; modulated.patch.mod[0][4] = 1; modulated.patch.mod[0][5] = 1;
    gf::Ob6Preview a, b; a.prepare(44100); b.prepare(44100);
    gf::apply_ob6_preset_preview(a, neutral); gf::apply_ob6_preset_preview(b, modulated);
    a.note_on(60, .8f); b.note_on(60, .8f);
    double difference = 0;
    for (int i = 0; i < 12000; ++i) difference += std::abs(a.process_sample() - b.process_sample());
    return difference > .01;
}

static bool voice_controls() {
    gf::Ob6Preview mono, unison, duplicate, glide, instant, lfo;
    for (auto* e : {&mono, &unison, &duplicate, &glide, &instant, &lfo}) e->prepare(44100);
    gf::Ob6SoundPatch p;
    mono.set_sound_patch(p); instant.set_sound_patch(p);
    p.unison = 3; p.unison_cents = 24; p.drift = .7f;
    unison.set_sound_patch(p); duplicate.set_sound_patch(p);
    p = gf::Ob6SoundPatch{}; p.glide_ms = 900; glide.set_sound_patch(p);
    p.glide_ms = 0; p.lfo_pitch = .8f; p.lfo_cutoff = .6f; p.lfo_pwm = .7f;
    lfo.set_sound_patch(p); lfo.set_lfo_control(.5f, 120, true);
    for (auto* e : {&mono, &unison, &duplicate, &lfo}) e->note_on(60, .8f);
    for (uint8_t note : {48, 50, 52, 54, 56, 58}) { glide.note_on(note, .8f); instant.note_on(note, .8f); }
    for (int i = 0; i < 3000; ++i) { glide.process_sample(); instant.process_sample(); }
    glide.note_on(72, .8f); instant.note_on(72, .8f);
    double unison_diff = 0, glide_diff = 0, lfo_diff = 0;
    for (int i = 0; i < 12000; ++i) {
        const float a = mono.process_sample(), b = unison.process_sample(), c = duplicate.process_sample();
        const float d = glide.process_sample(), e = instant.process_sample(), f = lfo.process_sample();
        if (!std::isfinite(a + b + c + d + e + f) || b != c) return false;
        unison_diff += std::abs(a - b); glide_diff += std::abs(d - e); lfo_diff += std::abs(a - f);
    }
    return unison_diff > .01 && glide_diff > .01 && lfo_diff > .01;
}

int main() {
    if (!voice_controls()) { std::fprintf(stderr, "OB-6 unison / drift / glide / LFO failed\n"); return 1; }
    if (!gfp_xmod_mapping()) { std::fprintf(stderr, "OB-6 GFP X-MOD mapping failed\n"); return 1; }
    if (!sync_and_xmod()) { std::fprintf(stderr, "OB-6 SYNC / X-MOD failed\n"); return 1; }
    if (!filter_modes_change_audio()) { std::fprintf(stderr, "OB-6 SEM modes failed\n"); return 1; }
    for (double fs : {44100.0, 48000.0, 96000.0})
        if (!run(fs)) { std::fprintf(stderr, "OB-6 pilot failed at %.0f Hz\n", fs); return 1; }
}
