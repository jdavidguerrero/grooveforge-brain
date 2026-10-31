#include "model_d_preview.h"
#include <algorithm>
#include <cmath>
#include <gf_params.h>

namespace gf {
void ModelDPreview::prepare(double sample_rate) {
    fs_ = std::isfinite(sample_rate) && sample_rate > 0 ? float(sample_rate) : 44100;
    filter_ = dsp::LadderStageCore(fs_);
    noise_.prepare(fs_);
    reset();
}
void ModelDPreview::reset() {
    count_ = 0; sustain_ = false; deferred_.fill(false); tick_ = 0; bend_ = tape_pitch_ = 1; hz_ = target_hz_ = 440; drift_left_ = 0;
    velocity_gain_ = velocity_brightness_ = 1;
    note_drift_.state = 0x2545F491u; drift_pitch_ = drift_cutoff_ = 1;
    mod_phase_ = noise_smooth_ = mod_signal_ = 0; osc_mod_ratio_ = lfo_pitch_ = lfo_cutoff_ = 1; lfo_pwm_ = 0; mod_rng_ = 0x5EED1234u;
    lfo_ = dsp::Lfo{}; lfo_.set_shape(patch_.lfo_shape); lfo_.set_rate_hz(patch_.lfo_rate); lfo_.set_sync(lfo_sync_, bpm_, patch_.lfo_beats);
    noise_.reset(0x7A3C5E91u * 1664525u + 1013904223u);
    noise_.set_color(patch_.noise_pink ? 1.0f : 0.0f);
    constexpr uint32_t seeds[] = {0x13579BDFu, 0x2468ACE0u, 0x369CF258u};
    constexpr float drift[] = {.0007f, .0009f, .0008f};
    for (size_t i = 0; i < oscillators_.size(); ++i) { oscillators_[i].reset(seeds[i]); oscillators_[i].set_drift(drift[i]); }
    filter_ = dsp::LadderStageCore(fs_);
    filter_.set_frequency(cutoff_); filter_.set_resonance(resonance_);
    filter_.set_drive(dsp::LadderStageCore::prepare_drive(patch_.drive * (1.0f + 3.0f * patch_.overdrive)));
    amp_ = dsp::AnalogEnv{}; amp_.set_rate(fs_); amp_.setAttackOvershoot(.3f); amp_.setADSR(attack_, patch_.amp_decay, patch_.amp_sustain, patch_.decay_switch ? patch_.amp_decay : release_);
    filter_env_ = dsp::AnalogEnv{}; filter_env_.set_rate(1000); filter_env_.setAttackOvershoot(.3f); filter_env_.setADSR(patch_.filter_attack, patch_.filter_decay, patch_.filter_sustain, patch_.decay_switch ? patch_.filter_decay : patch_.filter_release);
}
void ModelDPreview::note_on(uint8_t note, float velocity) {
    if (note > 127 || !std::isfinite(velocity)) return;
    const bool active = count_ > 0;
    deferred_[note] = false;
    for (uint8_t i = 0; i < count_; ++i) if (notes_[i] == note) {
        for (uint8_t j = i; j + 1 < count_; ++j) notes_[j] = notes_[j + 1];
        --count_; break;
    }
    if (count_ == notes_.size()) { for (size_t i = 1; i < notes_.size(); ++i) notes_[i - 1] = notes_[i]; --count_; }
    notes_[count_++] = note;
    target_hz_ = 440 * std::exp2((int(note) - 69) / 12.0f);
    if (!active || patch_.glide_ms < 1) hz_ = target_hz_;
    velocity_gain_ = 1 - patch_.velocity_amp * (1 - std::clamp(velocity, 0.0f, 1.0f));
    velocity_brightness_ = 1 - patch_.velocity_filter * (1 - std::clamp(velocity, 0.0f, 1.0f));
    drift_pitch_ = note_drift_.pitch(); drift_cutoff_ = note_drift_.cutoff();
    if (!active) { amp_.noteOn(); filter_env_.noteOn(); }
}
void ModelDPreview::release_note(uint8_t note) {
    bool found = false, top = false;
    for (uint8_t i = 0; i < count_; ++i) if (notes_[i] == note) {
        found = true; top = i + 1 == count_;
        for (uint8_t j = i; j + 1 < count_; ++j) notes_[j] = notes_[j + 1];
        --count_; break;
    }
    if (!found) return;
    if (count_ == 0) { amp_.noteOff(); filter_env_.noteOff(); }
    else if (top) { target_hz_ = 440 * std::exp2((int(notes_[count_ - 1]) - 69) / 12.0f); if (patch_.glide_ms < 1) hz_ = target_hz_; }
}
void ModelDPreview::note_off(uint8_t note) {
    if (note > 127) return;
    if (sustain_) deferred_[note] = true;
    else release_note(note);
}
void ModelDPreview::set_sustain(bool down) {
    sustain_ = down;
    if (!down) for (size_t i = 0; i < deferred_.size(); ++i) if (deferred_[i]) { deferred_[i] = false; release_note(uint8_t(i)); }
}
void ModelDPreview::set_pitch_bend(int value) { bend_ = std::exp2((2.0f * (std::clamp(value, 0, 16383) - 8192) / 8192.0f) / 12.0f); }
void ModelDPreview::set_tape_pitch(float ratio) { tape_pitch_ = std::isfinite(ratio) ? std::clamp(ratio, .5f, 2.0f) : 1.0f; }
void ModelDPreview::set_parameters(float cutoff, float resonance, float attack, float release) {
    const float new_cutoff = std::clamp(cutoff, 20.0f, 8000.0f);
    if (cutoff_ != new_cutoff) { cutoff_ = new_cutoff; filter_.set_frequency(cutoff_); }
    resonance_ = std::clamp(resonance, .707f, 5.0f);
    filter_.set_resonance(resonance_);
    if (attack_ != attack || release_ != release) {
        attack_ = attack; release_ = release; amp_.setAttack(attack); amp_.setRelease(patch_.decay_switch ? patch_.amp_decay : release);
    }
}
void ModelDPreview::set_sound_patch(const ModelDSoundPatch& source) {
    patch_ = source;
    for (auto& wave : patch_.wave) wave = std::min<uint8_t>(wave, 5);
    for (auto& level : patch_.level) level = std::clamp(level, 0.0f, 1.0f);
    patch_.noise_level = std::clamp(patch_.noise_level, 0.0f, 1.0f);
    patch_.pulse_width = std::clamp(patch_.pulse_width, .05f, .95f);
    patch_.detune = std::clamp(patch_.detune, -100.0f, 100.0f);
    patch_.detune3 = std::clamp(patch_.detune3, -100.0f, 100.0f);
    patch_.interval3 = std::clamp(patch_.interval3, -48.0f, 48.0f);
    patch_.glide_ms = std::clamp(patch_.glide_ms, 0.0f, 1500.0f);
    patch_.velocity_amp = std::clamp(patch_.velocity_amp, 0.0f, 1.0f);
    patch_.velocity_filter = std::clamp(patch_.velocity_filter, 0.0f, 1.0f);
    patch_.filter_amount = std::clamp(patch_.filter_amount, 0.0f, 1.0f);
    patch_.filter_keyboard = std::clamp(patch_.filter_keyboard, 0.0f, 1.0f);
    patch_.overdrive = std::clamp(patch_.overdrive, 0.0f, 1.0f);
    patch_.mod_mix = std::clamp(patch_.mod_mix, 0.0f, 1.0f);
    note_drift_.amount = std::clamp(patch_.drift, 0.0f, 1.0f);
    patch_.lfo_rate = std::clamp(patch_.lfo_rate, .05f, 20.0f);
    patch_.lfo_pitch = std::clamp(patch_.lfo_pitch, 0.0f, 1.0f);
    patch_.lfo_cutoff = std::clamp(patch_.lfo_cutoff, 0.0f, 1.0f);
    patch_.lfo_pwm = std::clamp(patch_.lfo_pwm, 0.0f, 1.0f);
    patch_.lfo_beats = std::isfinite(patch_.lfo_beats) && patch_.lfo_beats > 0 ? patch_.lfo_beats : .5f;
    lfo_.set_shape(patch_.lfo_shape); lfo_.set_rate_hz(patch_.lfo_rate); lfo_.set_sync(lfo_sync_, bpm_, patch_.lfo_beats);
    ratios_ = {1.0f, std::exp2((patch_.detune + 1.8f) / 1200.0f),
               std::exp2((patch_.interval3 + (patch_.detune3 - 2.2f) / 100.0f) / 12.0f)};
    const float sum = patch_.level[0] + patch_.level[1] + patch_.level[2] + patch_.noise_level;
    const float norm = sum > 1.15f ? 1.15f / sum : 1.0f;
    for (size_t i = 0; i < levels_.size(); ++i) levels_[i] = patch_.level[i] * norm;
    noise_gain_ = patch_.noise_level * norm * .6f;
    noise_.set_color(patch_.noise_pink ? 1.0f : 0.0f);
    amp_.setADSR(attack_, patch_.amp_decay, patch_.amp_sustain, patch_.decay_switch ? patch_.amp_decay : release_);
    filter_env_.setADSR(patch_.filter_attack, patch_.filter_decay, patch_.filter_sustain, patch_.decay_switch ? patch_.filter_decay : patch_.filter_release);
    filter_.set_drive(dsp::LadderStageCore::prepare_drive(patch_.drive * (1.0f + 3.0f * patch_.overdrive)));
}
void ModelDPreview::set_modulation_control(float wheel, float pressure, float bpm, bool sync) {
    wheel_ = std::isfinite(wheel) ? std::clamp(wheel, 0.0f, 1.0f) : 0;
    pressure_ = std::isfinite(pressure) ? std::clamp(pressure, 0.0f, 1.0f) : 0;
    bpm_ = std::isfinite(bpm) && bpm > 0 ? bpm : 120;
    lfo_sync_ = sync; lfo_.set_sync(sync, bpm_, patch_.lfo_beats);
}
float ModelDPreview::process_sample() {
    tick_ += 1000;
    if (tick_ >= fs_) {
        tick_ -= fs_;
        const float glide = patch_.glide_ms >= 1 ? 1.0f - std::exp(-1.0f / patch_.glide_ms) : 1.0f;
        hz_ += (target_hz_ - hz_) * glide;
        const float lfo = lfo_.tick(.001f);
        lfo_pitch_ = std::exp2(gf_lfo_pitch_st(std::min(1.0f, patch_.lfo_pitch + .3f * pressure_)) * lfo / 12.0f);
        lfo_cutoff_ = std::exp2(patch_.lfo_cutoff * GF_LFO_CUT_OCT * lfo + pressure_);
        lfo_pwm_ = .4f * patch_.lfo_pwm * lfo;
        mod_signal_ = 0;
        if ((patch_.osc_mod || patch_.filter_mod) && wheel_ > 0) {
            const float v3_base = patch_.vco3_keyboard ? hz_ * bend_ * lfo_pitch_ * tape_pitch_ * drift_pitch_ : 130.81f;
            const float v3_hz = v3_base * ratios_[2] * (patch_.vco3_low ? .01f : 1.0f);
            mod_phase_ += v3_hz * .001f; mod_phase_ -= std::floor(mod_phase_);
            float v3 = 1.0f - 4.0f * std::abs(mod_phase_ - .5f);
            const auto shape = patch_.wave[2];
            if (shape == 0) v3 = 2.0f * mod_phase_ - 1.0f;
            else if (shape == 4) v3 = 1.0f - 2.0f * mod_phase_;
            else if (shape == 2 || shape == 3 || shape == 5) v3 = mod_phase_ < (shape == 5 ? .1f : patch_.pulse_width) ? 1.0f : -1.0f;
            mod_rng_ ^= mod_rng_ << 13; mod_rng_ ^= mod_rng_ >> 17; mod_rng_ ^= mod_rng_ << 5;
            noise_smooth_ += .3f * (float(int32_t(mod_rng_)) * (1.0f / 2147483648.0f) - noise_smooth_);
            mod_signal_ = wheel_ * (v3 + patch_.mod_mix * (2.0f * noise_smooth_ - v3));
        }
        osc_mod_ratio_ = patch_.osc_mod ? std::exp2(wheel_ * patch_.mod_mix * 2.0f * noise_smooth_) : 1.0f;
        const float tracked = std::pow(hz_ / 130.81f, patch_.filter_keyboard);
        const float filter_mod = patch_.filter_mod ? std::exp2(3.0f * mod_signal_) : 1.0f;
        filter_.set_frequency(std::clamp((cutoff_ + patch_.filter_amount * velocity_brightness_ * filter_env_.update(1) * 8000.0f) * tracked * lfo_cutoff_ * filter_mod * drift_cutoff_, 20.0f, 20000.0f));
    }
    const bool drift = drift_left_ <= 0;
    if (drift) drift_left_ += 128.0 * double(fs_) / 44100.0;
    drift_left_ -= 1;
    constexpr uint16_t waves[] = {dsp::WAVE_SAWTOOTH, dsp::WAVE_TRIANGLE, dsp::WAVE_SQUARE, dsp::WAVE_PULSE, dsp::WAVE_SHARK, dsp::WAVE_PULSE};
    float samples[3]{};
    for (size_t order = 0; order < oscillators_.size(); ++order) {
        const size_t i = order == 0 ? 2 : order - 1;
        const uint8_t shape = patch_.wave[i];
        const auto wave = i == 2 && shape == 4 ? dsp::WAVE_REV_SAW : waves[shape];
        const float base = i == 2 && !patch_.vco3_keyboard ? 130.81f : hz_ * bend_ * lfo_pitch_ * tape_pitch_ * drift_pitch_;
        const float low = i == 2 && patch_.vco3_low ? .01f : 1.0f;
        const float frequency = base * ratios_[i] * low * (i == 2 ? 1.0f : osc_mod_ratio_);
        const float fm = i != 2 && patch_.osc_mod ? wheel_ * (1.0f - patch_.mod_mix) : 0;
        oscillators_[i].render(wave, frequency / fs_, shape == 5 ? .1f : std::clamp(patch_.pulse_width + lfo_pwm_, .05f, .95f),
                               1, &samples[i], 1, 0, nullptr, nullptr, i == 2 ? nullptr : &samples[2], fm, 0, drift);
    }
    float mix = 0;
    for (size_t i = 0; i < 3; ++i) mix += levels_[i] * velocity_gain_ * samples[i];
    float noise = 0;
    noise_.render(&noise, 1);
    mix += noise * noise_gain_ * velocity_gain_;
    return filter_.process_sample(std::clamp(mix, -1.0f, 1.0f)) * amp_.step();
}
}
