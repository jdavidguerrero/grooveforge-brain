#include "arp2600_preset.h"
#include "ui/groove_macros.h"
#include <algorithm>
namespace gf {
Arp2600Patch make_arp2600_patch(const studio::Preset& preset) {
    const auto p = studio::macro_patch(preset.patch, 5);
    constexpr uint16_t waves[] = {dsp::WAVE_SAWTOOTH, dsp::WAVE_TRIANGLE, dsp::WAVE_SQUARE, dsp::WAVE_PULSE, dsp::WAVE_SINE};
    Arp2600Patch patch;
    patch.wave1 = waves[std::clamp(int(p.osc[0][0]), 0, 4)];
    patch.wave2 = waves[std::clamp(int(p.osc[1][0]), 0, 4)];
    patch.pulse_width = std::clamp(p.osc[0][1] * .01f, .05f, .95f);
    patch.ratio = std::clamp(p.osc[1][1], .01f, 16.0f);
    patch.level1 = std::clamp(p.osc[0][4] * .01f, 0.0f, 1.0f);
    patch.level2 = std::clamp(p.osc[1][4] * .01f, 0.0f, 1.0f);
    patch.noise_level = std::clamp(p.osc[0][3] * .01f, 0.0f, 1.0f);
    patch.noise_color = std::clamp(p.mod[0][5] * .01f, 0.0f, 1.0f);
    patch.mono = p.mod[0][0] > .5f;
    const auto unison_index = uint8_t(std::clamp(p.groove[GF_GROOVE_UNISON][GF_UNISON_VOICES], 0.0f, 3.0f));
    patch.unison = patch.mono || !p.groove_on[GF_GROOVE_UNISON] ? 1 : std::min<uint8_t>(GF_UNISON_COUNT[unison_index], 4);
    patch.unison_cents = std::clamp(p.groove[GF_GROOVE_UNISON][GF_UNISON_DETUNE], 0.0f, 50.0f);
    patch.drift = studio::macro_amount(p, GF_MACRO_DRIFT);
    patch.sync = p.mod[0][1] > .5f;
    const float glide = std::clamp(p.glide, 0.0f, 1.0f);
    patch.glide_ms = glide * glide * 1500.0f;
    patch.vco3_rate = std::clamp(p.mod[0][2], .05f, 100.0f);
    patch.vco3_pitch = std::clamp(p.mod[0][3] * .01f, 0.0f, 1.0f);
    patch.vco3_cutoff = std::clamp(p.mod[0][4] * .01f, 0.0f, 1.0f);
    patch.decay = p.decay_ms; patch.sustain = p.sustain;
    patch.filter_attack = p.f_attack_ms; patch.filter_decay = p.f_decay_ms;
    patch.filter_sustain = p.f_sustain; patch.filter_release = p.f_release_ms;
    patch.env_amount = p.env_amt; patch.drive = .5f + 3.5f * p.drive;
    patch.lfo_rate = p.lfo_rate_hz; patch.lfo_shape = dsp::LfoShape(std::min<uint8_t>(p.lfo_shape, 4));
    patch.lfo_cutoff = p.lfo_amt[GF_DEST_CUTOFF]; patch.lfo_pitch = p.lfo_amt[GF_DEST_PITCH];
    patch.lfo_pwm = p.lfo_amt[GF_DEST_PWM]; patch.lfo_beats = GF_LFO_DIV_BEATS[std::min<unsigned>(p.lfo_div, GF_LFO_DIVS - 1)];
    studio::macro_vel_depths(p, &patch.velocity_amp, &patch.velocity_filter);
    return patch;
}
}
