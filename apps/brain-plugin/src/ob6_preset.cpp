#include "ob6_preset.h"
#include "ui/groove_macros.h"
#include <algorithm>
namespace gf {
void apply_ob6_preset_preview(Ob6Preview& engine, const studio::Preset& preset) {
    const auto p = studio::macro_patch(preset.patch, 3);
    Ob6SoundPatch patch;
    patch.shape1 = p.osc[0][0] * .01f; patch.shape2 = p.osc[1][0] * .01f;
    patch.level1 = p.osc[0][2] * .01f; patch.level2 = p.osc[1][2] * .01f;
    patch.sub = p.osc[0][1] * .01f; patch.noise = p.osc[0][3] * .01f;
    patch.pw1 = p.osc[0][4] * .01f; patch.pw2 = p.osc[1][4] * .01f;
    patch.interval = p.osc[1][1]; patch.detune = p.osc[1][5];
    patch.morph = p.osc[0][6] * .01f; patch.bandpass = p.osc[0][7] > .5f;
    patch.xmod_env = p.mod[0][0] * .01f; patch.xmod_vco2 = p.mod[0][1] * .01f;
    patch.xmod_freq = p.mod[0][2] > .5f; patch.xmod_morph = p.mod[0][3] > .5f;
    patch.xmod_pw = p.mod[0][4] > .5f; patch.sync = p.mod[0][5] > .5f;
    patch.amp_decay = p.decay_ms; patch.amp_sustain = p.sustain;
    patch.filter_attack = p.f_attack_ms; patch.filter_decay = p.f_decay_ms;
    patch.filter_sustain = p.f_sustain; patch.filter_release = p.f_release_ms;
    patch.filter_amount = p.env_amt; patch.drive = .5f + 3.5f * p.drive;
    studio::macro_vel_depths(p, &patch.velocity_amp, &patch.velocity_filter);
    const float glide = std::clamp(p.glide, 0.0f, 1.0f);
    patch.glide_ms = glide * glide * 1500.0f;
    const auto index = uint8_t(std::clamp(p.groove[GF_GROOVE_UNISON][GF_UNISON_VOICES], 0.0f, 3.0f));
    patch.unison = p.groove_on[GF_GROOVE_UNISON] ? GF_UNISON_COUNT[index] : 1;
    patch.unison_cents = p.groove[GF_GROOVE_UNISON][GF_UNISON_DETUNE];
    patch.drift = studio::macro_amount(p, GF_MACRO_DRIFT);
    patch.lfo_rate = p.lfo_rate_hz; patch.lfo_shape = dsp::LfoShape(std::min<uint8_t>(p.lfo_shape, 4));
    patch.lfo_cutoff = p.lfo_amt[GF_DEST_CUTOFF]; patch.lfo_pitch = p.lfo_amt[GF_DEST_PITCH]; patch.lfo_pwm = p.lfo_amt[GF_DEST_PWM];
    patch.lfo_beats = GF_LFO_DIV_BEATS[std::min<unsigned>(p.lfo_div, GF_LFO_DIVS - 1)];
    engine.set_sound_patch(patch);
}
}
