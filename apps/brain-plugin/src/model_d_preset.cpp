#include "model_d_preset.h"
#include "ui/groove_macros.h"
#include <algorithm>

namespace gf {
void apply_model_d_preset_preview(ModelDPreview& engine, const studio::Preset& preset) {
    const auto p = studio::macro_patch(preset.patch, 0);
    ModelDSoundPatch patch;
    for (size_t i = 0; i < 3; ++i) {
        patch.wave[i] = uint8_t(std::clamp(p.osc[i][0], 0.0f, 5.0f));
        patch.level[i] = p.osc[i][2] * .01f;
    }
    patch.detune = p.osc[1][1]; patch.interval3 = p.osc[2][1]; patch.detune3 = p.osc[2][5];
    patch.pulse_width = p.osc[0][4] * .01f; patch.noise_level = p.osc[3][0] * .01f;
    patch.noise_pink = p.osc[3][1] > .5f;
    patch.amp_decay = p.decay_ms; patch.amp_sustain = p.sustain;
    patch.filter_attack = p.f_attack_ms; patch.filter_decay = p.f_decay_ms;
    patch.filter_sustain = p.f_sustain; patch.filter_release = p.f_release_ms;
    patch.filter_amount = p.env_amt; patch.drive = .5f + 3.5f * p.drive;
    const float glide = std::clamp(p.glide, 0.0f, 1.0f);
    patch.glide_ms = glide * glide * 1500;
    patch.decay_switch = p.mod[0][3] > .5f;
    patch.vco3_keyboard = p.mod[0][4] > .5f; patch.vco3_low = p.mod[0][5] > .5f;
    patch.overdrive = p.mod[0][6] * .01f;
    patch.filter_keyboard = std::clamp(p.mod[0][7], 0.0f, 3.0f) / 3.0f;
    patch.mod_mix = p.mod[0][0] * .01f; patch.osc_mod = p.mod[0][1] > .5f; patch.filter_mod = p.mod[0][2] > .5f;
    patch.lfo_rate = p.lfo_rate_hz; patch.lfo_shape = dsp::LfoShape(std::min<uint8_t>(p.lfo_shape, 4));
    patch.lfo_cutoff = p.lfo_amt[GF_DEST_CUTOFF]; patch.lfo_pitch = p.lfo_amt[GF_DEST_PITCH]; patch.lfo_pwm = p.lfo_amt[GF_DEST_PWM];
    patch.lfo_beats = GF_LFO_DIV_BEATS[std::min<unsigned>(p.lfo_div, GF_LFO_DIVS - 1)];
    patch.drift = studio::macro_amount(p, GF_MACRO_DRIFT);
    studio::macro_vel_depths(p, &patch.velocity_amp, &patch.velocity_filter);
    engine.set_sound_patch(patch);
}
}
