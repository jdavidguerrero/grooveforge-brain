#include "prophet_preset.h"

#include "engines/prophet_patch_apply.h"
#include "ui/groove_macros.h"

#include <algorithm>

namespace gf {

void apply_synth_fx_preview(ProphetPreview& engine, const studio::Preset& preset, uint8_t engine_id) {
    const auto patch = studio::macro_patch(preset.patch, engine_id);
    const auto& drive = patch.sfx_p[GF_SFX_DRIVE];
    engine.set_synth_drive(patch.sfx_on[GF_SFX_DRIVE], uint8_t(gf_index_from_norm(drive[2], 3)),
                           drive[0], drive[1], patch.sfx_mix[GF_SFX_DRIVE]);
    engine.set_synth_tape(patch.sfx_on[GF_SFX_TAPE], gf_sfx_drive(patch.sfx_p[GF_SFX_TAPE][0]),
                          patch.sfx_mix[GF_SFX_TAPE], patch.sfx_p[GF_SFX_TAPE][1], patch.sfx_p[GF_SFX_TAPE][2]);
    const auto& chorus = patch.sfx_p[GF_SFX_CHORUS];
    const bool phaser = gf_index_from_norm(chorus[2], 2) == 1;
    engine.set_synth_phaser(patch.sfx_on[GF_SFX_CHORUS] && phaser, chorus[0],
                            gf_sfx_chorus_rate(chorus[1]), patch.sfx_mix[GF_SFX_CHORUS]);
    engine.set_synth_chorus(patch.sfx_on[GF_SFX_CHORUS] && !phaser, chorus[0],
                            gf_sfx_chorus_rate(chorus[1]), patch.sfx_mix[GF_SFX_CHORUS]);
    const auto& delay = patch.sfx_p[GF_SFX_DELAY];
    engine.set_synth_delay(patch.sfx_on[GF_SFX_DELAY], dsp::PINGPONG_DIV_BEATS[gf_index_from_norm(delay[0], 5)],
                           0.9f * delay[1], delay[2], patch.sfx_mix[GF_SFX_DELAY]);
    const auto& reverb = patch.sfx_p[GF_SFX_REVERB];
    const bool plate = gf_index_from_norm(reverb[1], 5) == GF_SFX_PLATE;
    engine.set_synth_plate(patch.sfx_on[GF_SFX_REVERB] && plate, reverb[0],
                           gf_index_from_norm(reverb[2], 3), patch.sfx_mix[GF_SFX_REVERB]);
    engine.set_synth_modal(patch.sfx_on[GF_SFX_REVERB] && !plate, gf_index_from_norm(reverb[1], 5),
                           gf_index_from_norm(reverb[2], 3), gf_sfx_decay(reverb[0]), patch.sfx_mix[GF_SFX_REVERB]);
}

void apply_groove_preview(ProphetPreview& engine, const studio::Preset& preset, bool apply_voice) {
    const auto& original = preset.patch;
    const auto& pump = original.groove[GF_GROOVE_PUMP];
    dsp::PumpParams pump_params;
    pump_params.depth = std::clamp(pump[GF_PUMP_DEPTH] * 0.01f, 0.0f, 1.0f);
    pump_params.shape = dsp::PumpShape(uint8_t(std::clamp(pump[GF_PUMP_SHAPE], 0.0f, 2.0f)));
    pump_params.rate_beats = GF_PUMP_RATE_BEATS[uint8_t(std::clamp(pump[GF_PUMP_RATE], 0.0f, 2.0f))];
    pump_params.offset_ms = pump[GF_PUMP_OFFSET];
    pump_params.release_ms = std::max(1.0f, pump[GF_PUMP_RELEASE]);
    engine.set_pump(original.groove_on[GF_GROOVE_PUMP], pump_params);
    const auto& arp = original.groove[GF_GROOVE_ARP];
    const auto mode = uint8_t(std::clamp(arp[GF_ARP_MODE], 0.0f, 4.0f));
    const auto division = uint8_t(std::clamp(arp[GF_ARP_DIV], 0.0f, 5.0f));
    engine.set_arp(original.groove_on[GF_GROOVE_ARP], dsp::ArpCore::Mode(mode), GF_ARP_DIV_BEATS[division],
                   arp[GF_ARP_GATE] * 0.01f, uint8_t(std::clamp(arp[GF_ARP_OCT], 1.0f, 3.0f)),
                   arp[GF_ARP_SWING] * 0.01f, arp[GF_ARP_LATCH] > 0.5f);
    const auto& chord = original.groove[GF_GROOVE_CHORD];
    engine.set_chord(original.groove_on[GF_GROOVE_CHORD],
                     dsp::ChordMemory::Type(uint8_t(std::clamp(chord[GF_CHORD_TYPE], 0.0f, 1.0f))),
                     uint8_t(std::clamp(chord[GF_CHORD_INV], 0.0f, 3.0f)),
                     uint8_t(std::clamp(chord[GF_CHORD_KEY], 0.0f, 23.0f)),
                     std::max(0.0f, chord[GF_CHORD_STRUM]));
    const auto& unison = original.groove[GF_GROOVE_UNISON];
    const auto unison_index = uint8_t(std::clamp(unison[GF_UNISON_VOICES], 0.0f, 3.0f));
    if (apply_voice) {
        engine.set_unison(original.groove_on[GF_GROOVE_UNISON] ? GF_UNISON_COUNT[unison_index] : 1,
                          unison[GF_UNISON_DETUNE]);
        engine.set_drift(studio::macro_amount(original, GF_MACRO_DRIFT));
    }

}

PresetPreviewLimitations apply_prophet_preset_preview(ProphetPreview& engine, const studio::Preset& preset) {
    constexpr uint8_t prophet = 2;
    const auto& original = preset.patch;
    const auto patch = studio::macro_patch(original, prophet);
    const float move = studio::macro_amount(original, GF_MACRO_MOVE);
    const float cutoff = std::clamp(preset.cutoff_hz * studio::move_cutoff_mul(move),
                                    GF_CUTOFF_RANGE[prophet].lo, GF_CUTOFF_RANGE[prophet].hi);
    const float reso = studio::move_resonance(studio::macro_resonance(preset.reso_norm, original), move);
    float velocity_amp, velocity_filter;
    studio::macro_vel_depths(original, &velocity_amp, &velocity_filter);
    engine.set_parameters(cutoff, 0.1f + std::clamp(reso, 0.0f, 1.0f) * 4.9f,
                          patch.attack_ms, patch.release_ms);
    studio::apply_prophet_envelope_patch(engine, patch, velocity_amp, velocity_filter);
    studio::apply_prophet_osc_patch(engine, patch);
    engine.set_lfo_division(GF_LFO_DIV_BEATS[std::min<unsigned>(patch.lfo_div, GF_LFO_DIVS - 1)]);
    engine.set_lfo(patch.lfo_rate_hz, dsp::LfoShape(patch.lfo_shape), patch.lfo_amt[GF_DEST_CUTOFF],
                   patch.lfo_amt[GF_DEST_PITCH], patch.lfo_amt[GF_DEST_PWM]);
    apply_synth_fx_preview(engine, preset, prophet);
    apply_groove_preview(engine, preset);

    PresetPreviewLimitations limitations;
    limitations.synth_fx = original.sfx_on[GF_SFX_REVERB] && gf_index_from_norm(original.sfx_p[GF_SFX_REVERB][1], 5) != GF_SFX_PLATE;
    for (unsigned i = 0; i < GF_GROOVE_TOGGLES; ++i)
        if (i != GF_GROOVE_PUMP && i != GF_GROOVE_ARP && i != GF_GROOVE_CHORD && i != GF_GROOVE_UNISON)
            limitations.groove |= original.groove_on[i];
    limitations.drift = false;
    return limitations;
}

}  // namespace gf
