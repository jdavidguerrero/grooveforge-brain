#include "juno_preset.h"
#include "ui/groove_macros.h"
#include <algorithm>
#include <cmath>

namespace gf {
void apply_juno_preset_preview(JunoPreview& engine, const studio::Preset& preset) {
    const auto p = studio::macro_patch(preset.patch, 1);
    const auto& o = p.osc[0];
    constexpr uint16_t waves[] = {dsp::WAVE_SAWTOOTH, dsp::WAVE_SQUARE, dsp::WAVE_PULSE, dsp::WAVE_SAW_AND_PULSE};
    float velocity_amp, velocity_filter;
    studio::macro_vel_depths(p, &velocity_amp, &velocity_filter);
    engine.set_patch(p.decay_ms, p.sustain, p.f_attack_ms, p.f_decay_ms, p.f_sustain, p.f_release_ms,
                     p.env_amt, 0.5f + 3.5f * p.drive, velocity_amp, velocity_filter);
    engine.set_lfo(p.lfo_rate_hz, dsp::LfoShape(std::min<uint8_t>(p.lfo_shape, 4)),
        p.lfo_amt[GF_DEST_CUTOFF], p.lfo_amt[GF_DEST_PITCH], p.lfo_amt[GF_DEST_PWM],
        GF_LFO_DIV_BEATS[std::min<unsigned>(p.lfo_div, GF_LFO_DIVS - 1)], p.mod[0][0]);
    const auto& unison = p.groove[GF_GROOVE_UNISON];
    const auto index = uint8_t(std::clamp(unison[GF_UNISON_VOICES], 0.0f, 3.0f));
    engine.set_voice_modulation(std::clamp(p.glide, 0.0f, 1.0f) * std::clamp(p.glide, 0.0f, 1.0f) * 1500.0f,
        p.groove_on[GF_GROOVE_UNISON] ? GF_UNISON_COUNT[index] : 1, unison[GF_UNISON_DETUNE],
        studio::macro_amount(p, GF_MACRO_DRIFT));
    engine.set_oscillator(waves[int(std::clamp(o[0], 0.0f, 3.0f))], o[1] * 0.01f, o[2] * 0.01f, o[3] * 0.01f,
                          uint8_t(int(std::lround(std::clamp(o[4], 0.0f, 3.0f)))),
                          uint8_t(int(std::lround(std::clamp(o[5], 0.0f, 3.0f)))),
                          uint8_t(int(std::lround(std::clamp(o[6], 0.0f, 2.0f)))), o[7] > 0.5f, p.mod[0][1] * 0.01f);
}
}
