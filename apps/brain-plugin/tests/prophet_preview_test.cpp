#include "prophet_preview.h"
#include "preset_codec.h"
#include "prophet_preset.h"
#include "engines/prophet_patch_apply.h"
#include "dx7_preview.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

static bool check(bool ok, const char* message) {
    if (!ok) std::fprintf(stderr, "%s\n", message);
    return ok;
}

int main() {
    for (double fs : {44100.0, 48000.0, 96000.0}) {
        gf::Dx7Preview dx7;
        dx7.prepare(fs);
        for (uint8_t note = 60; note < 66; ++note) dx7.note_on(note, 0.8f);
        float dx7_peak = 0;
        for (int i = 0; i < int(fs * 0.1); ++i) {
            const float sample = dx7.process_sample();
            if (!check(std::isfinite(sample), "DX7 output finite")) return 1;
            dx7_peak = std::max(dx7_peak, std::fabs(sample));
        }
        if (!check(dx7_peak > 0.001f, "DX7 six voices produce sound")) return 1;
        dx7.set_sustain(true);
        for (uint8_t note = 60; note < 66; ++note) dx7.note_off(note);
        dx7.set_sustain(false);
        for (int i = 0; i < int(fs * 4); ++i) dx7.process_sample();
        if (!check(std::fabs(dx7.process_sample()) < 1.0e-7f, "DX7 sustain releases voices")) return 1;
        gf::Dx7Preview dx_plain, dx_unison;
        dx_plain.prepare(fs); dx_unison.prepare(fs);
        dx_unison.set_unison(3, 30.0f, 0.7f);
        dx_unison.set_modulation(0.0f, 3.0f, dsp::LfoShape::Sine, 0.3f, 0.2f, 0.5f);
        dx_plain.note_on(60, 0.8f); dx_unison.note_on(60, 0.8f);
        float dx_difference = 0.0f;
        for (int i = 0; i < int(fs * 0.1); ++i)
            dx_difference += std::fabs(dx_plain.process_sample() - dx_unison.process_sample());
        if (!check(dx_difference > 0.01f, "DX7 unison and LFO change audio")) return 1;
        dx_unison.set_sustain(true); dx_unison.note_off(60); dx_unison.set_sustain(false);
        for (int i = 0; i < int(fs * 4); ++i) dx_unison.process_sample();
        if (!check(std::fabs(dx_unison.process_sample()) < 1.0e-7f, "DX7 unison releases all voices")) return 1;
        gf::ProphetPreview a, b;
        a.prepare(fs); b.prepare(fs);
        for (int i = 0; i < 100; ++i)
            if (!check(a.process_sample() == 0.0f && b.process_sample() == 0.0f, "initial silence")) return 1;
        a.note_on(60, 0.8f); b.note_on(60, 0.8f);
        float peak = 0.0f;
        for (int i = 0; i < int(fs * 0.2); ++i) {
            const float x = a.process_sample(), y = b.process_sample();
            if (!check(std::isfinite(x) && x == y, "independent renders")) return 1;
            peak = std::max(peak, std::fabs(x));
        }
        if (!check(peak > 0.001f, "audible note")) return 1;
        a.note_off(60);
        b.note_on(67, 0.7f);
        for (int i = 0; i < int(fs * 4); ++i) a.process_sample();
        if (!check(std::fabs(a.process_sample()) < 1.0e-7f, "release completes")) return 1;
        if (!check(std::isfinite(b.process_sample()), "other instance stays valid")) return 1;

        gf::ProphetPreview held, normal;
        held.prepare(fs); normal.prepare(fs);
        held.set_sustain(true);
        held.note_on(60, 0.8f); normal.note_on(60, 0.8f);
        for (int i = 0; i < int(fs * 0.1); ++i) { held.process_sample(); normal.process_sample(); }
        held.note_off(60);
        normal.note_off(60);
        for (int i = 0; i < int(fs * 4); ++i) { held.process_sample(); normal.process_sample(); }
        float held_peak = 0.0f;
        for (int i = 0; i < 1024; ++i) held_peak = std::max(held_peak, std::fabs(held.process_sample()));
        if (!check(held_peak > 0.001f && std::fabs(normal.process_sample()) < 1.0e-7f, "sustain holds released note")) return 1;
        held.set_sustain(false);
        for (int i = 0; i < int(fs * 4); ++i) held.process_sample();
        if (!check(std::fabs(held.process_sample()) < 1.0e-7f, "sustain release")) return 1;

        gf::ProphetPreview bent, unbent;
        bent.prepare(fs); unbent.prepare(fs);
        bent.note_on(69, 1.0f); unbent.note_on(69, 1.0f);
        for (int i = 0; i < 1024; ++i) { bent.process_sample(); unbent.process_sample(); }
        bent.set_pitch_bend(16383);
        float difference = 0.0f;
        for (int i = 0; i < 1024; ++i)
            difference += std::fabs(bent.process_sample() - unbent.process_sample());
        if (!check(difference > 0.01f, "pitch bend changes sounding note")) return 1;

        studio::Preset patch;
        if (!check(gf::factory_prophet_preset(0, patch), "factory Prophet preset")) return 1;
        gf::ProphetPreview factory, altered;
        factory.prepare(fs); altered.prepare(fs);
        studio::apply_prophet_envelope_patch(factory, patch.patch, patch.patch.vel_amp / 100.0f, patch.patch.vel_cut / 100.0f);
        studio::apply_prophet_envelope_patch(altered, patch.patch, patch.patch.vel_amp / 100.0f, patch.patch.vel_cut / 100.0f);
        studio::apply_prophet_osc_patch(factory, patch.patch);
        patch.patch.osc[0][2] = 0.0f;
        patch.patch.osc[0][3] = 1.0f;
        patch.patch.osc[2][0] = 100.0f;
        patch.patch.osc[2][1] = 0.0f;
        studio::apply_prophet_osc_patch(altered, patch.patch);
        factory.note_on(60, 1.0f); altered.note_on(60, 1.0f);
        float osc_difference = 0.0f;
        for (int i = 0; i < 2048; ++i)
            osc_difference += std::fabs(factory.process_sample() - altered.process_sample());
        if (!check(osc_difference > 0.01f, "Prophet OSC patch changes timbre")) return 1;

        gf::ProphetPreview modulated, static_voice;
        modulated.prepare(fs); static_voice.prepare(fs);
        modulated.set_lfo(5.0f, dsp::LfoShape::Sine, 0.3f, 0.2f, 0.0f);
        modulated.note_on(60, 1.0f); static_voice.note_on(60, 1.0f);
        float lfo_difference = 0.0f;
        for (int i = 0; i < int(fs * 0.2); ++i)
            lfo_difference += std::fabs(modulated.process_sample() - static_voice.process_sample());
        if (!check(lfo_difference > 0.01f, "LFO changes sounding note")) return 1;
        gf::ProphetPreview slow_lfo, fast_lfo;
        slow_lfo.prepare(fs); fast_lfo.prepare(fs);
        slow_lfo.set_lfo(2.0f, dsp::LfoShape::Sine, 0.0f, 0.3f, 0.0f);
        fast_lfo.set_lfo(2.0f, dsp::LfoShape::Sine, 0.0f, 0.3f, 0.0f);
        slow_lfo.set_lfo_division(1.0f); fast_lfo.set_lfo_division(1.0f);
        slow_lfo.set_lfo_sync(true, 60.0f); fast_lfo.set_lfo_sync(true, 120.0f);
        slow_lfo.note_on(60, 1.0f); fast_lfo.note_on(60, 1.0f);
        float sync_difference = 0.0f;
        for (int i = 0; i < int(fs * 0.2); ++i)
            sync_difference += std::fabs(slow_lfo.process_sample() - fast_lfo.process_sample());
        if (!check(sync_difference > 0.01f, "synced LFO follows BPM")) return 1;

        studio::Preset decoded;
        if (!check(gf::factory_prophet_preset(0, decoded), "factory preview preset")) return 1;
        decoded.patch.sfx_on[GF_SFX_TAPE] = true;
        decoded.patch.sfx_on[GF_SFX_CHORUS] = true;
        decoded.patch.sfx_on[GF_SFX_DELAY] = true;
        decoded.patch.sfx_on[GF_SFX_REVERB] = true;
        decoded.patch.groove_on[GF_GROOVE_CHORD] = true;
        decoded.patch.groove_on[GF_GROOVE_UNISON] = true;
        decoded.patch.groove[GF_GROOVE_UNISON][GF_UNISON_VOICES] = 2;
        decoded.patch.groove[GF_GROOVE_UNISON][GF_UNISON_DETUNE] = 20;
        decoded.patch.groove[GF_GROOVE_MACRO][GF_MACRO_DRIFT] = 25.0f;
        gf::ProphetPreview preset_engine;
        preset_engine.prepare(fs);
        const auto limitations = gf::apply_prophet_preset_preview(preset_engine, decoded);
        if (!check(limitations.synth_fx && !limitations.groove && !limitations.drift,
                   "unsupported preset features reported")) return 1;
        preset_engine.note_on(60, 0.8f);
        float preset_peak = 0.0f;
        for (int i = 0; i < 2048; ++i)
            preset_peak = std::max(preset_peak, std::fabs(preset_engine.process_sample()));
        if (!check(preset_peak > 0.001f, "preset produces audio")) return 1;
        gf::ProphetPreview preset_single;
        preset_single.prepare(fs);
        auto single_preset = decoded;
        single_preset.patch.groove_on[GF_GROOVE_UNISON] = false;
        gf::apply_prophet_preset_preview(preset_single, single_preset);
        preset_engine.reset();
        preset_engine.note_on(60, 0.8f); preset_single.note_on(60, 0.8f);
        float preset_unison_difference = 0.0f;
        for (int i = 0; i < int(fs * 0.1); ++i)
            preset_unison_difference += std::fabs(preset_engine.process_sample() - preset_single.process_sample());
        if (!check(preset_unison_difference > 0.01f, "GFP enables UNISON")) return 1;
        gf::ProphetPreview preset_no_drift, preset_with_drift;
        preset_no_drift.prepare(fs); preset_with_drift.prepare(fs);
        auto no_drift_preset = decoded;
        no_drift_preset.patch.groove[GF_GROOVE_MACRO][GF_MACRO_DRIFT] = 0.0f;
        gf::apply_prophet_preset_preview(preset_no_drift, no_drift_preset);
        gf::apply_prophet_preset_preview(preset_with_drift, decoded);
        preset_no_drift.note_on(60, 0.8f); preset_with_drift.note_on(60, 0.8f);
        float preset_drift_difference = 0.0f;
        for (int i = 0; i < int(fs * 0.1); ++i)
            preset_drift_difference += std::fabs(preset_no_drift.process_sample() - preset_with_drift.process_sample());
        if (!check(preset_drift_difference > 0.001f, "GFP enables DRIFT")) return 1;

        gf::ProphetPreview clean, driven;
        clean.prepare(fs); driven.prepare(fs);
        driven.set_synth_drive(true, dsp::DriveCore::DIST, 0.8f, 0.6f, 1.0f);
        clean.note_on(60, 1.0f); driven.note_on(60, 1.0f);
        float drive_difference = 0.0f;
        for (int i = 0; i < 4096; ++i)
            drive_difference += std::fabs(clean.process_sample() - driven.process_sample());
        if (!check(drive_difference > 0.01f, "SYNTH DRIVE changes the sound")) return 1;

        gf::ProphetPreview clean_tape, tape;
        clean_tape.prepare(fs); tape.prepare(fs);
        tape.set_synth_tape(true, 6.0f, 1.0f);
        clean_tape.note_on(60, 1.0f); tape.note_on(60, 1.0f);
        float tape_difference = 0.0f;
        for (int i = 0; i < 4096; ++i)
            tape_difference += std::fabs(clean_tape.process_sample() - tape.process_sample());
        if (!check(tape_difference > 0.01f, "SYNTH TAPE changes the sound")) return 1;

        gf::ProphetPreview pumping, dry_pump;
        pumping.prepare(fs); dry_pump.prepare(fs);
        dsp::PumpParams pump;
        pump.depth = 0.9f;
        pump.shape = dsp::PumpShape::Kick;
        pumping.set_pump(true, pump);
        pumping.set_transport(120.0f, true, 0.0, true);
        dry_pump.set_transport(120.0f, true, 0.0, true);
        pumping.note_on(60, 1.0f); dry_pump.note_on(60, 1.0f);
        double wet_energy = 0.0, dry_energy = 0.0;
        for (int i = 0; i < int(fs * 0.02); ++i) {
            const float wet = pumping.process_sample(), dry = dry_pump.process_sample();
            wet_energy += wet * wet; dry_energy += dry * dry;
        }
        if (!check(wet_energy < dry_energy * 0.5, "PUMP ducks on the beat")) return 1;

        gf::ProphetPreview arp, chord;
        arp.prepare(fs); chord.prepare(fs);
        arp.set_arp(true, dsp::ArpCore::Mode::Up, 0.25f, 0.6f, 1, 0.5f, false);
        arp.note_on(60, 1.0f); arp.note_on(64, 1.0f);
        chord.note_on(60, 1.0f); chord.note_on(64, 1.0f);
        float arp_difference = 0.0f, arp_peak = 0.0f;
        for (int i = 0; i < int(fs * 0.3); ++i) {
            const float x = arp.process_sample();
            arp_peak = std::max(arp_peak, std::fabs(x));
            arp_difference += std::fabs(x - chord.process_sample());
        }
        if (!check(arp_peak > 0.001f && arp_difference > 0.01f, "ARP plays a sequence")) return 1;
        arp.note_off(60); arp.note_off(64);
        for (int i = 0; i < int(fs * 4); ++i) arp.process_sample();
        if (!check(std::fabs(arp.process_sample()) < 1.0e-7f, "ARP releases held notes")) return 1;

        gf::ProphetPreview arp_sustain;
        arp_sustain.prepare(fs);
        arp_sustain.set_arp(true, dsp::ArpCore::Mode::Up, 0.25f, 0.6f, 1, 0.5f, false);
        arp_sustain.note_on(60, 1.0f);
        arp_sustain.set_sustain(true);
        arp_sustain.note_off(60);
        float sustained_peak = 0.0f;
        for (int i = 0; i < int(fs * 0.5); ++i)
            sustained_peak = std::max(sustained_peak, std::fabs(arp_sustain.process_sample()));
        if (!check(sustained_peak > 0.001f, "sustain retains ARP input")) return 1;
        arp_sustain.set_sustain(false);
        for (int i = 0; i < int(fs * 4); ++i) arp_sustain.process_sample();
        if (!check(std::fabs(arp_sustain.process_sample()) < 1.0e-7f, "ARP sustain release")) return 1;

        gf::ProphetPreview chord_mode, manual_chord;
        chord_mode.prepare(fs); manual_chord.prepare(fs);
        chord_mode.set_chord(true, dsp::ChordMemory::Type::Key, 0, 0, 0.0f);
        chord_mode.note_on(60, 1.0f);
        manual_chord.note_on(60, 1.0f); manual_chord.note_on(64, 1.0f); manual_chord.note_on(67, 1.0f);
        for (int i = 0; i < int(fs * 0.1); ++i)
            if (!check(chord_mode.process_sample() == manual_chord.process_sample(), "CHORD produces diatonic triad")) return 1;
        chord_mode.set_sustain(true); chord_mode.note_off(60);
        float chord_held = 0.0f;
        for (int i = 0; i < int(fs * 0.2); ++i)
            chord_held = std::max(chord_held, std::fabs(chord_mode.process_sample()));
        if (!check(chord_held > 0.001f, "sustain retains CHORD")) return 1;
        chord_mode.set_sustain(false);
        for (int i = 0; i < int(fs * 4); ++i) chord_mode.process_sample();
        if (!check(std::fabs(chord_mode.process_sample()) < 1.0e-7f, "CHORD sustain release")) return 1;

        gf::ProphetPreview strummed, single;
        strummed.prepare(fs); single.prepare(fs);
        strummed.set_chord(true, dsp::ChordMemory::Type::Key, 0, 0, 100.0f);
        strummed.note_on(60, 1.0f); single.note_on(60, 1.0f);
        for (int i = 0; i < int(fs * 0.03); ++i)
            if (!check(strummed.process_sample() == single.process_sample(), "STRUM delays higher notes")) return 1;
        strummed.note_off(60); single.note_off(60);
        for (int i = 0; i < int(fs * 0.3); ++i)
            if (!check(strummed.process_sample() == single.process_sample(), "released STRUM cancels pending notes")) return 1;

        gf::ProphetPreview chord_arp;
        chord_arp.prepare(fs);
        chord_arp.set_arp(true, dsp::ArpCore::Mode::Up, 0.25f, 0.6f, 1, 0.5f, false);
        chord_arp.set_chord(true, dsp::ChordMemory::Type::Key, 0, 0, 100.0f);
        chord_arp.note_on(60, 1.0f);
        float chord_arp_peak = 0.0f;
        for (int i = 0; i < int(fs * 0.4); ++i)
            chord_arp_peak = std::max(chord_arp_peak, std::fabs(chord_arp.process_sample()));
        if (!check(chord_arp_peak > 0.001f, "CHORD feeds ARP")) return 1;
        chord_arp.note_off(60);
        for (int i = 0; i < int(fs * 4); ++i) chord_arp.process_sample();
        if (!check(std::fabs(chord_arp.process_sample()) < 1.0e-7f, "CHORD plus ARP releases")) return 1;

        gf::ProphetPreview overlap;
        overlap.prepare(fs);
        overlap.set_chord(true, dsp::ChordMemory::Type::Key, 0, 0, 0.0f);
        overlap.note_on(60, 1.0f); overlap.note_on(64, 1.0f);
        overlap.note_off(60);
        float overlap_peak = 0.0f;
        for (int i = 0; i < int(fs * 0.2); ++i)
            overlap_peak = std::max(overlap_peak, std::fabs(overlap.process_sample()));
        if (!check(overlap_peak > 0.001f, "overlapping chord remains sounding")) return 1;
        overlap.note_off(64);
        for (int i = 0; i < int(fs * 4); ++i) overlap.process_sample();
        if (!check(std::fabs(overlap.process_sample()) < 1.0e-7f, "overlapping chords release")) return 1;

        gf::ProphetPreview unison, single_voice, five_voices, six_voices;
        unison.prepare(fs); single_voice.prepare(fs); five_voices.prepare(fs); six_voices.prepare(fs);
        unison.set_unison(2, 30.0f);
        five_voices.set_unison(5, 30.0f); six_voices.set_unison(6, 30.0f);
        unison.note_on(60, 1.0f); single_voice.note_on(60, 1.0f);
        five_voices.note_on(60, 1.0f); six_voices.note_on(60, 1.0f);
        float unison_difference = 0.0f;
        for (int i = 0; i < int(fs * 0.2); ++i) {
            unison_difference += std::fabs(unison.process_sample() - single_voice.process_sample());
            if (!check(five_voices.process_sample() == six_voices.process_sample(), "UNISON clips to Prophet polyphony")) return 1;
        }
        if (!check(unison_difference > 0.01f, "UNISON changes the sound")) return 1;
        unison.set_sustain(true); unison.note_off(60);
        float unison_held = 0.0f;
        for (int i = 0; i < int(fs * 0.2); ++i)
            unison_held = std::max(unison_held, std::fabs(unison.process_sample()));
        if (!check(unison_held > 0.001f, "sustain retains UNISON")) return 1;
        unison.set_sustain(false);
        for (int i = 0; i < int(fs * 4); ++i) unison.process_sample();
        if (!check(std::fabs(unison.process_sample()) < 1.0e-7f, "UNISON releases all voices")) return 1;

        gf::ProphetPreview drift_a, drift_b, no_drift;
        drift_a.prepare(fs); drift_b.prepare(fs); no_drift.prepare(fs);
        drift_a.set_drift(1.0f); drift_b.set_drift(1.0f);
        drift_a.set_unison(2, 20.0f); drift_b.set_unison(2, 20.0f); no_drift.set_unison(2, 20.0f);
        drift_a.note_on(60, 1.0f); drift_b.note_on(60, 1.0f); no_drift.note_on(60, 1.0f);
        float drift_difference = 0.0f;
        for (int i = 0; i < int(fs * 0.2); ++i) {
            const float sample = drift_a.process_sample();
            if (!check(sample == drift_b.process_sample(), "DRIFT is deterministic per instance")) return 1;
            drift_difference += std::fabs(sample - no_drift.process_sample());
        }
        if (!check(drift_difference > 0.01f, "DRIFT changes sound")) return 1;

        gf::ProphetPreview tape_mod, tape_flat;
        tape_mod.prepare(fs); tape_flat.prepare(fs);
        tape_mod.set_synth_tape(true, 3.0f, 0.7f, 1.0f, 1.0f);
        tape_flat.set_synth_tape(true, 3.0f, 0.7f, 0.0f, 0.0f);
        tape_mod.note_on(60, 1.0f); tape_flat.note_on(60, 1.0f);
        float tape_mod_difference = 0.0f;
        for (int i = 0; i < int(fs * 0.3); ++i)
            tape_mod_difference += std::fabs(tape_mod.process_sample() - tape_flat.process_sample());
        if (!check(tape_mod_difference > 0.001f, "TAPE wow and flutter affect audio")) return 1;
        gf::ProphetPreview tape_from_preset, tape_from_control;
        tape_from_preset.prepare(fs); tape_from_control.prepare(fs);
        auto tape_preset = decoded;
        tape_preset.patch.sfx_on[GF_SFX_TAPE] = true;
        tape_preset.patch.sfx_p[GF_SFX_TAPE][1] = 0.8f;
        tape_preset.patch.sfx_p[GF_SFX_TAPE][2] = 0.6f;
        auto flat_preset = tape_preset;
        flat_preset.patch.sfx_p[GF_SFX_TAPE][1] = 0.0f;
        flat_preset.patch.sfx_p[GF_SFX_TAPE][2] = 0.0f;
        gf::apply_prophet_preset_preview(tape_from_preset, tape_preset);
        gf::apply_prophet_preset_preview(tape_from_control, flat_preset);
        tape_from_preset.note_on(60, 1.0f); tape_from_control.note_on(60, 1.0f);
        float preset_tape_difference = 0.0f;
        for (int i = 0; i < int(fs * 0.2); ++i)
            preset_tape_difference += std::fabs(tape_from_preset.process_sample() - tape_from_control.process_sample());
        if (!check(preset_tape_difference > 0.001f, "GFP applies TAPE modulation")) return 1;

        gf::ProphetPreview phaser;
        phaser.prepare(fs);
        phaser.set_synth_phaser(true, 0.8f, 0.8f, 1.0f);
        phaser.note_on(60, 1.0f);
        float stereo_difference = 0.0f, phaser_peak = 0.0f;
        for (int i = 0; i < int(fs * 0.2); ++i) {
            float left, right;
            phaser.process_frame(left, right);
            stereo_difference += std::fabs(left - right);
            phaser_peak = std::max(phaser_peak, std::max(std::fabs(left), std::fabs(right)));
        }
        if (!check(phaser_peak > 0.001f && stereo_difference > 0.01f, "PHASER produces stereo audio")) return 1;
        gf::ProphetPreview chorus, no_chorus;
        chorus.prepare(fs); no_chorus.prepare(fs);
        chorus.set_synth_chorus(true, 0.8f, 0.7f, 0.8f);
        chorus.note_on(60, 1.0f); no_chorus.note_on(60, 1.0f);
        float chorus_difference = 0.0f;
        for (int i = 0; i < int(fs * 0.1); ++i)
            chorus_difference += std::fabs(chorus.process_sample() - no_chorus.process_sample());
        if (!check(chorus_difference > 0.01f, "CHORUS changes audio")) return 1;
        gf::ProphetPreview preset_chorus, preset_no_chorus;
        preset_chorus.prepare(fs); preset_no_chorus.prepare(fs);
        auto chorus_patch = decoded;
        chorus_patch.patch.sfx_p[GF_SFX_CHORUS][0] = 0.8f;
        chorus_patch.patch.sfx_p[GF_SFX_CHORUS][2] = 0.0f;
        chorus_patch.patch.sfx_mix[GF_SFX_CHORUS] = 0.8f;
        auto dry_patch = chorus_patch;
        dry_patch.patch.sfx_on[GF_SFX_CHORUS] = false;
        gf::apply_prophet_preset_preview(preset_chorus, chorus_patch);
        gf::apply_prophet_preset_preview(preset_no_chorus, dry_patch);
        preset_chorus.note_on(60, 1.0f); preset_no_chorus.note_on(60, 1.0f);
        float preset_chorus_difference = 0.0f;
        for (int i = 0; i < int(fs * 0.1); ++i)
            preset_chorus_difference += std::fabs(preset_chorus.process_sample() - preset_no_chorus.process_sample());
        if (!check(preset_chorus_difference > 0.01f, "GFP enables CHORUS")) return 1;
        gf::ProphetPreview delayed, dry_delay;
        delayed.prepare(fs); dry_delay.prepare(fs);
        std::vector<int16_t> delay_l(size_t(fs) + 2), delay_r(size_t(fs) + 2);
        delayed.set_delay_memory(delay_l.data(), delay_r.data(), uint32_t(delay_l.size()));
        delayed.set_synth_delay(true, 0.25f, 0.5f, 0.5f, 0.8f);
        delayed.set_transport(120.0f, true, 0.0, true);
        dry_delay.set_transport(120.0f, true, 0.0, true);
        delayed.note_on(60, 1.0f); dry_delay.note_on(60, 1.0f);
        double delay_difference = 0.0, delay_stereo = 0.0;
        for (int i = 0; i < int(fs * 0.4); ++i) {
            float l, r, dl, dr;
            delayed.process_frame(l, r); dry_delay.process_frame(dl, dr);
            delay_difference += std::fabs(l - dl) + std::fabs(r - dr);
            delay_stereo += std::fabs(l - r);
        }
        if (!check(delay_difference > 0.1 && delay_stereo > 0.1, "ping-pong DELAY creates stereo echoes")) return 1;
        gf::ProphetPreview preset_delay, preset_dry_delay;
        preset_delay.prepare(fs); preset_dry_delay.prepare(fs);
        std::vector<int16_t> preset_delay_l(size_t(fs) + 2), preset_delay_r(size_t(fs) + 2);
        preset_delay.set_delay_memory(preset_delay_l.data(), preset_delay_r.data(), uint32_t(preset_delay_l.size()));
        auto delay_patch = decoded;
        delay_patch.patch.sfx_p[GF_SFX_DELAY][0] = 0.0f;
        delay_patch.patch.sfx_p[GF_SFX_DELAY][1] = 0.5f;
        delay_patch.patch.sfx_mix[GF_SFX_DELAY] = 0.8f;
        auto delay_dry_patch = delay_patch;
        delay_dry_patch.patch.sfx_on[GF_SFX_DELAY] = false;
        gf::apply_prophet_preset_preview(preset_delay, delay_patch);
        gf::apply_prophet_preset_preview(preset_dry_delay, delay_dry_patch);
        preset_delay.note_on(60, 1.0f); preset_dry_delay.note_on(60, 1.0f);
        double preset_delay_difference = 0.0;
        for (int i = 0; i < int(fs * 0.3); ++i)
            preset_delay_difference += std::fabs(preset_delay.process_sample() - preset_dry_delay.process_sample());
        if (!check(preset_delay_difference > 0.01, "GFP enables DELAY")) return 1;
        gf::ProphetPreview delay_slow, delay_fast;
        delay_slow.prepare(fs); delay_fast.prepare(fs);
        std::vector<int16_t> slow_l(size_t(fs) + 2), slow_r(size_t(fs) + 2), fast_l(size_t(fs) + 2), fast_r(size_t(fs) + 2);
        delay_slow.set_delay_memory(slow_l.data(), slow_r.data(), uint32_t(slow_l.size()));
        delay_fast.set_delay_memory(fast_l.data(), fast_r.data(), uint32_t(fast_l.size()));
        delay_slow.set_transport(60.0f, false, 0.0, true); delay_fast.set_transport(120.0f, false, 0.0, true);
        delay_slow.set_synth_delay(true, 0.25f, 0.0f, 0.5f, 1.0f);
        delay_fast.set_synth_delay(true, 0.25f, 0.0f, 0.5f, 1.0f);
        delay_slow.note_on(60, 1.0f); delay_fast.note_on(60, 1.0f);
        double tempo_difference = 0.0;
        for (int i = 0; i < int(fs * 0.2); ++i)
            tempo_difference += std::fabs(delay_slow.process_sample() - delay_fast.process_sample());
        if (!check(tempo_difference > 0.01, "DELAY follows tempo")) return 1;
        gf::PlateReverbPreview plate;
        std::vector<int16_t> plate_buffer(gf::PlateReverbPreview::required_samples(float(fs)));
        if (!check(plate.prepare(plate_buffer.data(), plate_buffer.size(), float(fs)), "PLATE memory")) return 1;
        plate.set(0.8f, 0.5f);
        double plate_energy = 0.0, plate_stereo = 0.0;
        for (int i = 0; i < int(fs * 0.5); ++i) {
            float l, r;
            plate.process(i == 0 ? 0.9f : 0.0f, l, r);
            plate_energy += std::fabs(l) + std::fabs(r);
            plate_stereo += std::fabs(l - r);
        }
        if (!check(plate_energy > 0.001 && plate_stereo > 0.001, "PLATE produces stereo tail")) return 1;
        plate.reset();
        for (int i = 0; i < int(fs * 0.1); ++i) {
            float l, r; plate.process(0.0f, l, r);
            if (!check(l == 0.0f && r == 0.0f, "PLATE reset clears tail")) return 1;
        }
        gf::ProphetPreview preset_plate, preset_no_plate;
        preset_plate.prepare(fs); preset_no_plate.prepare(fs);
        std::vector<int16_t> preset_plate_buffer(gf::PlateReverbPreview::required_samples(float(fs)));
        preset_plate.set_plate_memory(preset_plate_buffer.data(), preset_plate_buffer.size());
        auto plate_patch = decoded;
        plate_patch.patch.sfx_p[GF_SFX_REVERB][1] = 1.0f;
        plate_patch.patch.sfx_mix[GF_SFX_REVERB] = 0.8f;
        auto dry_plate_patch = plate_patch;
        dry_plate_patch.patch.sfx_on[GF_SFX_REVERB] = false;
        gf::apply_prophet_preset_preview(preset_plate, plate_patch);
        gf::apply_prophet_preset_preview(preset_no_plate, dry_plate_patch);
        preset_plate.note_on(60, 1.0f); preset_no_plate.note_on(60, 1.0f);
        double plate_difference = 0.0;
        for (int i = 0; i < int(fs * 0.15); ++i)
            plate_difference += std::fabs(preset_plate.process_sample() - preset_no_plate.process_sample());
        if (!check(plate_difference > 0.01, "GFP enables PLATE")) return 1;
        for (uint8_t material = 0; material < 4; ++material) {
            gf::ModalReverbPreview modal;
            modal.prepare(float(fs)); modal.configure(material, 1, 1.0f);
            double modal_energy = 0.0;
            for (int i = 0; i < int(fs * 0.2); ++i) {
                const float sample = modal.process(i == 0 ? 1.0f : 0.0f);
                if (!check(std::isfinite(sample), "MODAL output is finite")) return 1;
                modal_energy += std::fabs(sample);
            }
            if (!check(modal_energy > 0.01, "MODAL material produces tail")) return 1;
            modal.reset();
            if (!check(modal.process(0.0f) == 0.0f, "MODAL reset clears tail")) return 1;
        }
        gf::ProphetPreview preset_modal, preset_no_modal;
        preset_modal.prepare(fs); preset_no_modal.prepare(fs);
        auto modal_patch = decoded;
        modal_patch.patch.sfx_p[GF_SFX_REVERB][1] = 0.0f;
        modal_patch.patch.sfx_mix[GF_SFX_REVERB] = 0.8f;
        auto dry_modal_patch = modal_patch;
        dry_modal_patch.patch.sfx_on[GF_SFX_REVERB] = false;
        gf::apply_prophet_preset_preview(preset_modal, modal_patch);
        gf::apply_prophet_preset_preview(preset_no_modal, dry_modal_patch);
        preset_modal.note_on(60, 1.0f); preset_no_modal.note_on(60, 1.0f);
        double modal_difference = 0.0;
        for (int i = 0; i < int(fs * 0.1); ++i)
            modal_difference += std::fabs(preset_modal.process_sample() - preset_no_modal.process_sample());
        if (!check(modal_difference > 0.01, "GFP enables MODAL")) return 1;
        dsp::PhaserCore phased_block, phased_samples;
        float phaser_input[128], phaser_block[128], phaser_single[128];
        for (int i = 0; i < 128; ++i) phaser_input[i] = std::sin(float(i) * 0.1f);
        phased_block.render(phaser_input, phaser_block, 128, fs);
        for (int i = 0; i < 128; ++i) phased_samples.render(&phaser_input[i], &phaser_single[i], 1, fs);
        for (int i = 0; i < 128; ++i)
            if (!check(phaser_block[i] == phaser_single[i], "PHASER block partition")) return 1;

        gf::ProphetPreview before_prepare, after_prepare;
        gf::apply_prophet_preset_preview(before_prepare, decoded);
        before_prepare.prepare(fs);
        after_prepare.prepare(fs);
        gf::apply_prophet_preset_preview(after_prepare, decoded);
        before_prepare.note_on(64, 0.8f); after_prepare.note_on(64, 0.8f);
        for (int i = 0; i < 4096; ++i)
            if (!check(before_prepare.process_sample() == after_prepare.process_sample(), "preset survives prepare")) return 1;
    }
}
