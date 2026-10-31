#include "arp2600_preview.h"
#include "arp2600_preset.h"
#include <cmath>
#include <gf_params.h>
#include <cstdio>

int main() {
    for (double fs : {44100.0, 48000.0, 96000.0}) {
        gf::Arp2600Preview engine;
        engine.prepare(fs);
        engine.set_parameters(1500, .8f, 5, 100);
        engine.set_ring_mix(.7f);
        for (int note : {48, 55, 60, 64, 67}) engine.note_on(uint8_t(note), .8f);
        double energy = 0;
        for (int i = 0; i < int(fs * .3); ++i) {
            float value = engine.process_sample();
            if (!std::isfinite(value)) return 1;
            energy += value * value;
        }
        if (energy < .01) { std::fprintf(stderr, "ARP output silent at %.0f\n", fs); return 1; }
        engine.set_sustain(true);
        for (int note : {48, 55, 60, 64, 67}) engine.note_off(uint8_t(note));
        double held = 0;
        for (int i = 0; i < int(fs * .1); ++i) { const float v = engine.process_sample(); held += v * v; }
        if (held < .001) return 1;
        engine.set_sustain(false);
        for (int i = 0; i < int(fs); ++i) if (!std::isfinite(engine.process_sample())) return 1;
        if (std::abs(engine.process_sample()) > .0001f) return 1;
    }
    for (double fs : {44100.0, 48000.0, 96000.0}) {
        studio::Preset preset;
        if (!studio::factory_preset(5, 0, &preset)) return 1;
        preset.patch.osc[0][4] = 0; preset.patch.osc[1][4] = 0;
        preset.patch.osc[0][3] = 100; preset.patch.mod[0][5] = 100;
        const auto mapped = gf::make_arp2600_patch(preset);
        if (mapped.noise_level != 1 || mapped.noise_color != 1) return 1;
        gf::Arp2600Preview pink, white, silent;
        for (auto* engine : {&pink, &white, &silent}) { engine->prepare(fs); engine->set_parameters(8000, .707f, 1, 100); engine->set_ring_mix(0); }
        auto patch = mapped; pink.set_patch(patch);
        patch.noise_color = 0; white.set_patch(patch);
        patch.noise_level = 0; silent.set_patch(patch);
        for (auto* engine : {&pink, &white, &silent}) engine->note_on(60, 1);
        double pink_energy = 0, white_energy = 0, difference = 0;
        for (int i = 0; i < int(fs * .2); ++i) {
            const float p = pink.process_sample(), w = white.process_sample(), z = silent.process_sample();
            if (!std::isfinite(p) || !std::isfinite(w) || z != 0) return 1;
            pink_energy += p * p; white_energy += w * w; difference += std::abs(p - w);
        }
        if (pink_energy < .001 || white_energy < .001 || difference < .01) return 1;
    }
    {
        studio::Preset preset;
        if (!studio::factory_preset(5, 0, &preset)) return 1;
        preset.patch.mod[0][1] = 1; preset.patch.osc[1][1] = 2.3f;
        preset.patch.osc[0][4] = 0; preset.patch.osc[1][4] = 100;
        auto patch = gf::make_arp2600_patch(preset);
        if (!patch.sync) return 1;
        gf::Arp2600Preview synced, free;
        for (auto* engine : {&synced, &free}) { engine->prepare(48000); engine->set_ring_mix(0); }
        synced.set_patch(patch); patch.sync = false; free.set_patch(patch);
        synced.note_on(60, 1); free.note_on(60, 1);
        double change = 0;
        for (int i = 0; i < 12000; ++i) {
            const float a = synced.process_sample(), b = free.process_sample();
            if (!std::isfinite(a) || !std::isfinite(b)) return 1;
            change += std::abs(a - b);
        }
        if (change < .01) return 1;
    }
    {
        studio::Preset preset;
        if (!studio::factory_preset(5, 0, &preset)) return 1;
        preset.patch.mod[0][2] = 5; preset.patch.mod[0][3] = 80; preset.patch.mod[0][4] = 65;
        const auto mapped = gf::make_arp2600_patch(preset);
        if (std::abs(mapped.vco3_rate - 5) > .001f || std::abs(mapped.vco3_pitch - .8f) > .001f ||
            std::abs(mapped.vco3_cutoff - .65f) > .001f) return 1;
        gf::Arp2600Preview pitch, cutoff, reference;
        for (auto* engine : {&pitch, &cutoff, &reference}) { engine->prepare(48000); engine->set_parameters(800, .8f, 5, 100); }
        auto p = mapped; p.vco3_cutoff = 0; pitch.set_patch(p);
        p = mapped; p.vco3_pitch = 0; cutoff.set_patch(p);
        p.vco3_cutoff = 0; reference.set_patch(p);
        for (auto* engine : {&pitch, &cutoff, &reference}) engine->note_on(60, 1);
        double pitch_change = 0, cutoff_change = 0;
        for (int i = 0; i < 10000; ++i) {
            const float x = pitch.process_sample(), y = cutoff.process_sample(), z = reference.process_sample();
            if (!std::isfinite(x + y + z)) return 1;
            pitch_change += std::abs(x - z); cutoff_change += std::abs(y - z);
        }
        if (pitch_change < .01 || cutoff_change < .01) return 1;
    }
    {
        studio::Preset preset;
        if (!studio::factory_preset(5, 0, &preset)) return 1;
        preset.patch.mod[0][0] = 1; preset.patch.glide = .5f;
        auto patch = gf::make_arp2600_patch(preset);
        if (!patch.mono || std::abs(patch.glide_ms - 375.0f) > .001f) return 1;
        gf::Arp2600Preview mono;
        mono.prepare(48000); mono.set_patch(patch); mono.set_parameters(1800, .8f, 5, 100);
        mono.note_on(48, 1);
        for (int i = 0; i < 4000; ++i) if (!std::isfinite(mono.process_sample())) return 1;
        mono.note_on(72, 1);
        double high_energy = 0;
        for (int i = 0; i < 4000; ++i) { const float x = mono.process_sample(); high_energy += x * x; }
        mono.note_off(72);
        double return_energy = 0;
        for (int i = 0; i < 4000; ++i) { const float x = mono.process_sample(); return_energy += x * x; }
        if (high_energy < .001 || return_energy < .001) return 1;
        mono.set_sustain(true); mono.note_off(48);
        double held = 0;
        for (int i = 0; i < 6000; ++i) { const float x = mono.process_sample(); held += x * x; }
        if (held < .001) return 1;
        mono.set_sustain(false);
        for (int i = 0; i < 48000; ++i) if (!std::isfinite(mono.process_sample())) return 1;
        if (std::abs(mono.process_sample()) > .0001f) return 1;
    }
    {
        gf::Arp2600Patch patch;
        patch.mono = true; patch.glide_ms = 600;
        gf::Arp2600Preview glide, immediate;
        glide.prepare(48000); immediate.prepare(48000);
        glide.set_patch(patch); patch.glide_ms = 0; immediate.set_patch(patch);
        glide.note_on(48, 1); immediate.note_on(48, 1);
        for (int i = 0; i < 4000; ++i) { glide.process_sample(); immediate.process_sample(); }
        glide.note_on(72, 1); immediate.note_on(72, 1);
        double effect = 0;
        for (int i = 0; i < 8000; ++i) effect += std::abs(glide.process_sample() - immediate.process_sample());
        if (effect < .01) return 1;
    }
    {
        studio::Preset preset;
        if (!studio::factory_preset(5, 0, &preset)) return 1;
        preset.patch.lfo_rate_hz = 6; preset.patch.lfo_amt[GF_DEST_CUTOFF] = .8f;
        preset.patch.lfo_amt[GF_DEST_PITCH] = .7f; preset.patch.lfo_amt[GF_DEST_PWM] = .6f;
        const auto patch = gf::make_arp2600_patch(preset);
        if (std::abs(patch.lfo_cutoff - .8f) > .001f || std::abs(patch.lfo_pitch - .7f) > .001f || std::abs(patch.lfo_pwm - .6f) > .001f) return 1;
        for (int destination = 0; destination < 3; ++destination) {
            gf::Arp2600Preview synth, reference;
            synth.prepare(48000); reference.prepare(48000);
            auto p = patch;
            p.lfo_cutoff = destination == 0 ? .8f : 0; p.lfo_pitch = destination == 1 ? .7f : 0; p.lfo_pwm = destination == 2 ? .6f : 0;
            p.wave1 = dsp::WAVE_PULSE; synth.set_patch(p);
            p.lfo_cutoff = p.lfo_pitch = p.lfo_pwm = 0; reference.set_patch(p);
            synth.note_on(60, 1); reference.note_on(60, 1);
            double energy = 0, effect = 0;
            for (int i = 0; i < 12000; ++i) {
                const float x = synth.process_sample(), y = reference.process_sample();
                if (!std::isfinite(x) || !std::isfinite(y)) return 1;
                energy += x * x; effect += std::abs(x - y);
            }
            if (energy < .001 || effect < .01) return 1;
        }
    }
    {
        studio::Preset preset;
        if (!studio::factory_preset(5, 0, &preset)) return 1;
        preset.patch.mod[0][0] = 0;
        preset.patch.groove_on[GF_GROOVE_UNISON] = true;
        preset.patch.groove[GF_GROOVE_UNISON][GF_UNISON_VOICES] = 3;
        preset.patch.groove[GF_GROOVE_UNISON][GF_UNISON_DETUNE] = 30;
        preset.patch.groove[GF_GROOVE_MACRO][GF_MACRO_DRIFT] = 80;
        const auto patch = gf::make_arp2600_patch(preset);
        if (patch.unison != 4 || std::abs(patch.unison_cents - 30) > .001 || std::abs(patch.drift - .8f) > .001) return 1;
        auto mono_preset = preset; mono_preset.patch.mod[0][0] = 1;
        if (gf::make_arp2600_patch(mono_preset).unison != 1) return 1;
        gf::Arp2600Preview synth, same;
        for (auto* engine : {&synth, &same}) {
            engine->prepare(48000); engine->set_patch(patch); engine->set_parameters(1600, .8f, 5, 100); engine->note_on(60, .8f);
        }
        double energy = 0;
        for (int i = 0; i < 8000; ++i) {
            const float a = synth.process_sample(), b = same.process_sample();
            if (!std::isfinite(a) || a != b) return 1;
            energy += a * a;
        }
        if (energy < .001) return 1;
        synth.set_sustain(true); synth.note_off(60);
        for (int i = 0; i < 1000; ++i) if (!std::isfinite(synth.process_sample())) return 1;
        synth.set_sustain(false);
        for (int i = 0; i < 48000; ++i) if (!std::isfinite(synth.process_sample())) return 1;
        if (std::abs(synth.process_sample()) > .0001f) return 1;
    }
    return 0;
}
