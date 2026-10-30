#include "dx7_preset.h"
#include "ui/groove_macros.h"
#include <algorithm>
#include <cmath>

namespace gf {
Dx7Patch make_dx7_operator_patch(const studio::Preset& preset) {
    const auto p = studio::macro_patch(preset.patch, 4);
    Dx7Patch patch;
    patch.algorithm = uint8_t(std::clamp(int(std::lround(p.osc[0][2])), 1, 8));
    patch.feedback = uint8_t(std::clamp(int(std::lround(p.osc[0][3])), 0, 7));
    for (int i = 0; i < DX7_OPS; ++i) {
        patch.op[i].ratio = p.osc[i][0]; patch.op[i].level = p.osc[i][1];
        patch.op[i].attack_ms = p.osc[i][4]; patch.op[i].decay_ms = p.osc[i][5];
        patch.op[i].sustain = p.osc[i][6] * 0.01f; patch.op[i].release_ms = p.osc[i][7];
        patch.op[i].wave = uint8_t(std::clamp(int(std::lround(p.mod[0][i])) - 1, 0, 7));
        patch.op[i].detune = int8_t(std::clamp(int(std::lround(p.mod[0][4 + i])), -3, 3));
    }
    patch.kbd_level = std::clamp(p.mod[1][0], 0.0f, 99.0f);
    patch.kbd_rate = uint8_t(std::clamp(int(std::lround(p.mod[1][1])), 0, 3));
    patch.d2_ms = std::max(0.0f, p.mod[1][2]);
    return patch;
}
void apply_dx7_preset_preview(Dx7Preview& engine, const studio::Preset& preset) {
    const auto p = studio::macro_patch(preset.patch, 4);
    const auto patch = make_dx7_operator_patch(preset);
    const float move = studio::macro_amount(preset.patch, GF_MACRO_MOVE);
    const float cutoff = std::clamp(preset.cutoff_hz * studio::move_cutoff_mul(move), GF_CUTOFF_RANGE[4].lo, GF_CUTOFF_RANGE[4].hi);
    const float res = studio::move_resonance(studio::macro_resonance(preset.reso_norm, preset.patch), move);
    float amp_velocity, filter_velocity;
    studio::macro_vel_depths(preset.patch, &amp_velocity, &filter_velocity);
    engine.set_patch(patch, cutoff, 0.7f + 2.3f * std::clamp(res, 0.0f, 1.0f), 0.5f + 3.5f * p.drive,
                     p.attack_ms, p.decay_ms, p.sustain, p.release_ms, amp_velocity);
    const auto& unison = preset.patch.groove[GF_GROOVE_UNISON];
    const auto count = uint8_t(std::clamp(int(unison[GF_UNISON_VOICES]), 0, 3));
    engine.set_unison(preset.patch.groove_on[GF_GROOVE_UNISON] ? GF_UNISON_COUNT[count] : 1,
                      unison[GF_UNISON_DETUNE], studio::macro_amount(preset.patch, GF_MACRO_DRIFT));
    engine.set_modulation(p.glide * p.glide * 1500.0f, p.lfo_rate_hz, dsp::LfoShape(p.lfo_shape),
                          p.lfo_amt[GF_DEST_PITCH], p.lfo_amt[GF_DEST_AMP],
                          GF_LFO_DIV_BEATS[std::min<unsigned>(p.lfo_div, GF_LFO_DIVS - 1)]);
}
}
