#pragma once

#include <math.h>

#include "../ui/page_params.h"

namespace studio {

/** @brief Aplica envelopes, drive, glide y profundidades de velocity de un patch Prophet. */
template <typename Engine>
void apply_prophet_envelope_patch(Engine& prophet, const Patch& p, float velocity_amp, float velocity_filter) {
    prophet.setAmpEnv(p.attack_ms, p.decay_ms, p.sustain, p.release_ms);
    prophet.setFilterEnv(p.f_attack_ms, p.f_decay_ms, p.f_sustain, p.f_release_ms);
    prophet.setFilterEnvAmount(p.env_amt);
    prophet.setGlide(fmaxf(p.glide * p.glide * 1500.0f, p.osc[2][3] * 20.0f));
    prophet.setDrive(0.5f + 3.5f * p.drive);
    prophet.setVelocityDepth(velocity_amp, velocity_filter);
}

/** @brief Aplica OSC y MOD del patch Prophet a un engine con los setters del Prophet-5. */
template <typename Engine>
void apply_prophet_osc_patch(Engine& prophet, const Patch& p) {
    const auto& o = p.osc;
    prophet.setOscAFreq(o[0][0]);
    prophet.setOscAPw(o[0][1] / 100.0f);
    prophet.setOscAShape(o[0][2] > 0.5f, o[0][3] > 0.5f);
    prophet.setSync(o[0][4] > 0.5f);
    prophet.setOscBInterval(o[1][0]);
    prophet.setOscBFine(o[1][1]);
    prophet.setOscBPw(o[1][2] / 100.0f);
    prophet.setOscBKeyboard(o[1][3] > 0.5f);
    prophet.setOscBShape(o[1][4] > 0.5f, o[1][5] > 0.5f, o[1][6] > 0.5f);
    prophet.setOscBLoFreq(o[1][7] > 0.5f);
    prophet.setMixer(o[2][0] / 100.0f, o[2][1] / 100.0f, o[2][2] / 100.0f);
    prophet.setGlide(fmaxf(p.glide * p.glide * 1500.0f, o[2][3] * 20.0f));
    prophet.setFilterKeyboard(o[2][4] * 0.5f);
    const auto& m = p.mod[0];
    const auto& wm = p.mod[1];
    prophet.setPolyMod(m[0] / 100.0f, m[1] / 100.0f, m[2] > 0.5f, m[3] > 0.5f, m[4] > 0.5f);
    prophet.setWheelMod(wm[0] / 100.0f, wm[1] > 0.5f, wm[2] > 0.5f, wm[3] > 0.5f, wm[4] > 0.5f, wm[5] > 0.5f);
}

}  // namespace studio
