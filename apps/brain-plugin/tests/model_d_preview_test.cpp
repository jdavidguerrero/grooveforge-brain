#include "model_d_preview.h"
#include "model_d_preset.h"
#include <cmath>
#include <cstdio>

static bool run(double fs) {
    gf::ModelDPreview synth, silent;
    synth.prepare(fs); silent.prepare(fs);
    synth.set_parameters(1400, .8f, 10, 200);
    synth.note_on(57, 1);
    double energy = 0;
    for (int i = 0; i < int(fs * .2); ++i) {
        const float value = synth.process_sample();
        if (!std::isfinite(value) || silent.process_sample() != 0) return false;
        energy += value * value;
    }
    if (energy < .01) return false;
    auto spectral = [&]() {
        double low_re = 0, low_im = 0, high_re = 0, high_im = 0;
        const int frames = int(fs * .3);
        for (int i = 0; i < frames; ++i) {
            const float value = synth.process_sample();
            if (!std::isfinite(value)) return std::array<double, 2>{-1, -1};
            const double phase = 6.283185307179586 * i / fs;
            low_re += value * std::cos(110 * phase); low_im += value * std::sin(110 * phase);
            high_re += value * std::cos(440 * phase); high_im += value * std::sin(440 * phase);
        }
        return std::array<double, 2>{std::hypot(low_re, low_im), std::hypot(high_re, high_im)};
    };
    const auto low = spectral();
    synth.note_on(69, 1);
    const auto high = spectral();
    synth.note_off(69);
    const auto back = spectral();
    if (low[0] <= 0 || high[1] <= 0 || back[0] <= 0 || low[0] < high[0] * 3 || back[0] < high[0] * 3) {
        std::fprintf(stderr, "spectral low=%.3f high-low=%.3f high=%.3f back=%.3f\n", low[0], high[0], high[1], back[0]); return false;
    }
    synth.set_sustain(true); synth.note_off(57);
    double held = 0;
    for (int i = 0; i < int(fs * .1); ++i) { const auto value = synth.process_sample(); held += value * value; }
    if (held < .01) return false;
    synth.set_sustain(false);
    for (int i = 0; i < int(fs * 2); ++i) if (!std::isfinite(synth.process_sample())) return false;
    return std::abs(synth.process_sample()) < 1e-5f;
}
int main() {
    for (double fs : {44100.0, 48000.0, 96000.0})
        if (!run(fs)) { std::fprintf(stderr, "Model D mono pilot failed at %.0f Hz\n", fs); return 1; }
    studio::Preset first, second;
    if (!studio::factory_preset(0, 0, &first)) return 1;
    second = first;
    second.patch.osc[0][0] = 4; second.patch.osc[1][0] = 5;
    second.patch.osc[0][2] = 100; second.patch.osc[1][1] = 35;
    gf::ModelDPreview a, b;
    a.prepare(44100); b.prepare(44100);
    gf::apply_model_d_preset_preview(a, first); gf::apply_model_d_preset_preview(b, second);
    a.note_on(60, 1); b.note_on(60, 1);
    double difference = 0;
    for (int i = 0; i < 12000; ++i) {
        const float x = a.process_sample(), y = b.process_sample();
        if (!std::isfinite(x + y)) return 1;
        difference += std::abs(x - y);
    }
    if (difference < .01) { std::fprintf(stderr, "Model D GFP oscillator mapping failed\n"); return 1; }
    gf::ModelDPreview glide, immediate;
    glide.prepare(44100); immediate.prepare(44100);
    gf::ModelDSoundPatch sound;
    immediate.set_sound_patch(sound); sound.glide_ms = 700; glide.set_sound_patch(sound);
    glide.note_on(48, 1); immediate.note_on(48, 1);
    for (int i = 0; i < 4000; ++i) { glide.process_sample(); immediate.process_sample(); }
    glide.note_on(72, 1); immediate.note_on(72, 1);
    double glide_difference = 0;
    for (int i = 0; i < 8000; ++i) glide_difference += std::abs(glide.process_sample() - immediate.process_sample());
    if (glide_difference < .01) { std::fprintf(stderr, "Model D legato glide failed\n"); return 1; }
    for (double fs : {44100.0, 48000.0, 96000.0, 192000.0}) {
        studio::Preset white_preset = first, pink_preset = first, zero_preset = first;
        for (auto* p : {&white_preset, &pink_preset, &zero_preset}) {
            for (int osc = 0; osc < 3; ++osc) p->patch.osc[osc][2] = 0;
            p->patch.osc[3][0] = 100;
        }
        pink_preset.patch.osc[3][1] = 1; zero_preset.patch.osc[3][0] = 0;
        gf::ModelDPreview white, white_copy, pink, zero;
        for (auto* engine : {&white, &white_copy, &pink, &zero}) { engine->prepare(fs); engine->set_parameters(8000, .707f, 1, 200); }
        gf::apply_model_d_preset_preview(white, white_preset); gf::apply_model_d_preset_preview(white_copy, white_preset);
        gf::apply_model_d_preset_preview(pink, pink_preset); gf::apply_model_d_preset_preview(zero, zero_preset);
        for (auto* engine : {&white, &white_copy, &pink, &zero}) engine->note_on(60, 1);
        double energy = 0, pink_energy = 0, color_difference = 0;
        for (int i = 0; i < int(fs * .25); ++i) {
            const float w = white.process_sample(), duplicate = white_copy.process_sample();
            const float p = pink.process_sample(), silent = zero.process_sample();
            if (!std::isfinite(w + p) || w != duplicate || silent != 0) return 1;
            energy += w * w; pink_energy += p * p; color_difference += std::abs(w - p);
        }
        if (energy < .01 || pink_energy < .01 || pink_energy > 10 * energy || color_difference < .01) { std::fprintf(stderr, "Model D GFP noise failed at %.0f Hz\n", fs); return 1; }
    }
    studio::Preset base = first, fixed = first, low_rate = first, driven = first, tracked = first;
    for (auto* preset : {&base, &fixed, &low_rate, &driven, &tracked}) {
        preset->patch.osc[2][2] = 100; preset->patch.osc[3][0] = 0;
        preset->patch.mod[0][4] = 1; preset->patch.mod[0][5] = 0;
        preset->patch.mod[0][6] = 0; preset->patch.mod[0][7] = 0;
    }
    fixed.patch.mod[0][4] = 0; low_rate.patch.mod[0][5] = 1;
    driven.patch.mod[0][6] = 100; tracked.patch.mod[0][7] = 3;
    gf::ModelDPreview ref, fixed_engine, low_engine, driven_engine, tracked_engine;
    for (auto* engine : {&ref, &fixed_engine, &low_engine, &driven_engine, &tracked_engine}) {
        engine->prepare(44100); engine->set_parameters(900, .707f, 10, 300);
    }
    gf::apply_model_d_preset_preview(ref, base); gf::apply_model_d_preset_preview(fixed_engine, fixed);
    gf::apply_model_d_preset_preview(low_engine, low_rate); gf::apply_model_d_preset_preview(driven_engine, driven);
    gf::apply_model_d_preset_preview(tracked_engine, tracked);
    for (auto* engine : {&ref, &fixed_engine, &low_engine, &driven_engine, &tracked_engine}) engine->note_on(72, 1);
    std::array<double, 4> changes{};
    for (int i = 0; i < 10000; ++i) {
        const float value = ref.process_sample();
        const float variants[] = {fixed_engine.process_sample(), low_engine.process_sample(), driven_engine.process_sample(), tracked_engine.process_sample()};
        for (size_t j = 0; j < changes.size(); ++j) {
            if (!std::isfinite(value + variants[j])) return 1;
            changes[j] += std::abs(value - variants[j]);
        }
    }
    for (double change : changes) if (change < .01) { std::fprintf(stderr, "Model D GFP VCO3 / filter controls failed\n"); return 1; }
    studio::Preset plain = first, fm = first, filter_mod = first, lfo_mod = first;
    for (auto* p : {&plain, &fm, &filter_mod, &lfo_mod}) {
        p->patch.osc[3][0] = 0; p->patch.mod[0][0] = 0; p->patch.mod[0][1] = 0; p->patch.mod[0][2] = 0;
        p->patch.lfo_amt[GF_DEST_CUTOFF] = p->patch.lfo_amt[GF_DEST_PITCH] = p->patch.lfo_amt[GF_DEST_PWM] = 0;
    }
    fm.patch.mod[0][1] = 1; filter_mod.patch.mod[0][2] = 1;
    lfo_mod.patch.lfo_amt[GF_DEST_CUTOFF] = .7f; lfo_mod.patch.lfo_amt[GF_DEST_PITCH] = .5f; lfo_mod.patch.lfo_amt[GF_DEST_PWM] = .6f;
    gf::ModelDPreview unmodulated, fm_engine, filter_engine, lfo_engine;
    for (auto* engine : {&unmodulated, &fm_engine, &filter_engine, &lfo_engine}) {
        engine->prepare(44100); engine->set_parameters(1300, .707f, 10, 300); engine->set_modulation_control(.8f, 0, 120, false);
    }
    gf::apply_model_d_preset_preview(unmodulated, plain); gf::apply_model_d_preset_preview(fm_engine, fm);
    gf::apply_model_d_preset_preview(filter_engine, filter_mod); gf::apply_model_d_preset_preview(lfo_engine, lfo_mod);
    for (auto* engine : {&unmodulated, &fm_engine, &filter_engine, &lfo_engine}) engine->note_on(60, 1);
    std::array<double, 3> modulation_difference{};
    for (int i = 0; i < 12000; ++i) {
        const float value = unmodulated.process_sample();
        const float variants[] = {fm_engine.process_sample(), filter_engine.process_sample(), lfo_engine.process_sample()};
        for (size_t j = 0; j < modulation_difference.size(); ++j) {
            if (!std::isfinite(value + variants[j])) return 1;
            modulation_difference[j] += std::abs(value - variants[j]);
        }
    }
    for (double delta : modulation_difference) if (delta < .01) { std::fprintf(stderr, "Model D GFP modulation failed\n"); return 1; }
    studio::Preset no_drift = first, with_drift = first;
    no_drift.patch.groove[GF_GROOVE_MACRO][GF_MACRO_DRIFT] = 0;
    with_drift.patch.groove[GF_GROOVE_MACRO][GF_MACRO_DRIFT] = 100;
    gf::ModelDPreview no_drift_engine, drift_engine, drift_copy;
    for (auto* engine : {&no_drift_engine, &drift_engine, &drift_copy}) engine->prepare(44100);
    gf::apply_model_d_preset_preview(no_drift_engine, no_drift);
    gf::apply_model_d_preset_preview(drift_engine, with_drift); gf::apply_model_d_preset_preview(drift_copy, with_drift);
    for (auto* engine : {&no_drift_engine, &drift_engine, &drift_copy}) engine->note_on(60, 1);
    double drift_difference = 0;
    for (int i = 0; i < 10000; ++i) {
        const float a = no_drift_engine.process_sample(), b = drift_engine.process_sample(), c = drift_copy.process_sample();
        if (!std::isfinite(a + b) || b != c) return 1;
        drift_difference += std::abs(a - b);
    }
    if (drift_difference < .01) { std::fprintf(stderr, "Model D GFP drift failed\n"); return 1; }
}
