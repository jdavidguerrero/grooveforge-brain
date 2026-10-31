#include "juno_preview.h"
#include <algorithm>
#include <cmath>
#include <gf_params.h>

namespace gf {
void JunoPreview::prepare(double rate) {
    fs_ = std::isfinite(rate) && rate > 0 ? float(rate) : 44100.0f;
    for (auto& voice : voices_) voice.emplace(fs_);
    reset();
}
void JunoPreview::reset() {
    age_ = 0; next_ = 0; sustain_ = false; bend_ = 1; tick_ = 0; drift_samples_remaining_ = 0;
    drift_.state = 0x2545F491u;
    lfo_ = dsp::Lfo{}; lfo_.set_rate_hz(lfo_rate_); lfo_.set_shape(lfo_shape_);
    lfo_.set_sync(lfo_sync_, bpm_, lfo_beats_);
    held_ = 0; lfo_elapsed_ms_ = 0; lfo_pitch_ratio_ = lfo_cutoff_ratio_ = 1; pwm_mod_ = 0;
    chorus_ = dsp::JunoChorusCore{};
    chorus_high_rate_ = dsp::JunoChorusCoreT<4096>{};
    chorus_.setMode(chorus_mode_); chorus_.setHpf(hpf_); chorus_.setHiss(hiss_);
    chorus_high_rate_.setMode(chorus_mode_); chorus_high_rate_.setHpf(hpf_); chorus_high_rate_.setHiss(hiss_);
    for (size_t i = 0; i < voices_.size(); ++i) {
        if (!voices_[i]) continue;
        auto& v = *voices_[i];
        v = Voice(fs_);
        v.dco.reset(0x41060001u + uint32_t(i) * 97u);
        v.sub.reset(0x41061001u + uint32_t(i) * 97u);
        v.noise.reset(uint32_t(i) + 1u);
        v.dco.set_drift(0.00015f); v.sub.set_drift(0.00015f);
        v.spread = dsp::voice_spread(0x10601u + uint32_t(i) * 7919u, 0.3f, 0.04f);
        v.filter.set_character(dsp::LadderCharacter::Ir3109);
        v.filter.set_drive(dsp::LadderStageCore::prepare_drive(drive_));
        v.filter.set_resonance(resonance_); v.filter.set_frequency(cutoff_);
        v.amp.set_rate(fs_); v.amp.setAttackOvershoot(3); v.amp.setAttack(gate_ ? 1.0f : attack_);
        v.amp.setDecay(gate_ ? 1.0f : decay_); v.amp.setSustain(gate_ ? 1.0f : sustain_level_); v.amp.setRelease(gate_ ? 8.0f : release_);
        v.filter_env.set_rate(fs_); v.filter_env.setAttackOvershoot(3); v.filter_env.setAttack(filter_attack_);
        v.filter_env.setDecay(filter_decay_); v.filter_env.setSustain(filter_sustain_); v.filter_env.setRelease(filter_release_);
    }
}
void JunoPreview::note_on(uint8_t note, float velocity) {
    if (note > 127 || !voices_[0]) return;
    if (held_ == 0) lfo_elapsed_ms_ = 0;
    if (held_ < 255) ++held_;
    if (unison_ > 1) {
        for (auto& v : voices_) if (v->note == note) {
            v->amp.noteOff(); v->filter_env.noteOff(); v->note = -1; v->deferred = false;
        }
    }
    for (uint8_t k = 0; k < unison_; ++k) {
        Voice* v = nullptr;
        if (unison_ == 1) for (auto& candidate : voices_) if (candidate->note == note) { v = &*candidate; break; }
        if (!v) for (size_t n = 0; n < voices_.size(); ++n) {
            const auto index = (next_ + n) % voices_.size();
            if (voices_[index]->note < 0) { v = &*voices_[index]; next_ = uint8_t((index + 1) % voices_.size()); break; }
        }
        if (!v) v = &**std::min_element(voices_.begin(), voices_.end(), [](const auto& x, const auto& y) { return x->age < y->age; });
        const bool free = v->note < 0;
        v->target_hz = 440.0f * std::exp2((int(note) - 69) / 12.0f) * (range_ == 0 ? 0.5f : range_ == 2 ? 2.0f : 1.0f);
        if (free || glide_ms_ < 1.0f) v->hz = v->target_hz;
        v->note = note; v->age = ++age_; v->deferred = false;
        v->pitch_ratio = dsp::unison_ratio(k, unison_, unison_cents_) * drift_.pitch();
        v->cutoff_ratio = drift_.cutoff();
        v->velocity = std::clamp(velocity, 0.0f, 1.0f);
        v->amp.noteOn(); v->filter_env.noteOn(false);
    }
}
void JunoPreview::note_off(uint8_t note) {
    if (held_ > 0) --held_;
    for (auto& v : voices_) if (v && v->note == note) {
        if (sustain_) v->deferred = true;
        else { v->amp.noteOff(); v->filter_env.noteOff(); v->note = -1; v->deferred = false; }
    }
}
void JunoPreview::set_sustain(bool down) {
    sustain_ = down;
    if (!down) for (auto& v : voices_) if (v && v->deferred) {
        v->amp.noteOff(); v->filter_env.noteOff(); v->note = -1; v->deferred = false;
    }
}
void JunoPreview::set_pitch_bend(int value) {
    bend_ = std::exp2((2.0f * (std::clamp(value, 0, 16383) - 8192) / 8192.0f) / 12.0f);
}
void JunoPreview::set_parameters(float cutoff, float resonance, float attack, float release) {
    cutoff_ = std::clamp(cutoff, 20.0f, 20000.0f);
    resonance_ = std::clamp(resonance, 0.707f, 5.0f);
    if (attack_ != attack || release_ != release) {
        attack_ = attack; release_ = release;
        for (auto& v : voices_) if (v) { v->amp.setAttack(gate_ ? 1.0f : attack_); v->amp.setRelease(gate_ ? 8.0f : release_); }
    }
    for (auto& v : voices_) if (v) v->filter.set_resonance(resonance_);
}
void JunoPreview::set_oscillator(uint16_t wave, float sub, float noise, float pulse_width,
                                 uint8_t chorus, uint8_t hpf, uint8_t range, bool gate, float hiss) {
    waveform_ = wave; sub_level_ = std::clamp(sub, 0.0f, 1.0f); noise_level_ = std::clamp(noise, 0.0f, 1.0f);
    pulse_width_ = std::clamp(pulse_width, 0.05f, 0.95f); chorus_mode_ = std::min<uint8_t>(chorus, 3);
    hpf_ = std::min<uint8_t>(hpf, 3); range_ = std::min<uint8_t>(range, 2); gate_ = gate;
    hiss_ = std::clamp(hiss, 0.0f, 1.0f);
    chorus_.setMode(chorus_mode_); chorus_.setHpf(hpf_); chorus_.setHiss(hiss_);
    chorus_high_rate_.setMode(chorus_mode_); chorus_high_rate_.setHpf(hpf_); chorus_high_rate_.setHiss(hiss_);
    for (auto& v : voices_) if (v) {
        v->amp.setAttack(gate_ ? 1.0f : attack_); v->amp.setDecay(gate_ ? 1.0f : decay_);
        v->amp.setSustain(gate_ ? 1.0f : sustain_level_); v->amp.setRelease(gate_ ? 8.0f : release_);
    }
}
void JunoPreview::set_patch(float decay, float sustain, float filter_attack, float filter_decay,
                            float filter_sustain, float filter_release, float env_amount, float drive,
                            float velocity_amp, float velocity_filter) {
    decay_ = decay; sustain_level_ = sustain; filter_attack_ = filter_attack; filter_decay_ = filter_decay;
    filter_sustain_ = filter_sustain; filter_release_ = filter_release; env_amount_ = env_amount; drive_ = drive;
    velocity_amp_ = std::clamp(velocity_amp, 0.0f, 1.0f); velocity_filter_ = std::clamp(velocity_filter, 0.0f, 1.0f);
    for (auto& v : voices_) if (v) {
        v->amp.setDecay(gate_ ? 1.0f : decay_); v->amp.setSustain(gate_ ? 1.0f : sustain_level_);
        v->filter_env.setAttack(filter_attack_); v->filter_env.setDecay(filter_decay_);
        v->filter_env.setSustain(filter_sustain_); v->filter_env.setRelease(filter_release_);
        v->filter.set_drive(dsp::LadderStageCore::prepare_drive(drive_));
    }
}
void JunoPreview::set_voice_modulation(float glide_ms, uint8_t unison, float cents, float drift) {
    glide_ms_ = std::clamp(glide_ms, 0.0f, 1500.0f);
    unison_ = std::clamp<uint8_t>(unison, 1, 6);
    unison_cents_ = std::clamp(cents, 0.0f, 50.0f);
    unison_gain_ = dsp::unison_gain(unison_);
    drift_.amount = std::clamp(drift, 0.0f, 1.0f);
}
void JunoPreview::set_lfo(float rate, dsp::LfoShape shape, float cutoff, float pitch, float pwm, float beats, float delay_ms) {
    lfo_rate_ = std::clamp(rate, 0.05f, 20.0f); lfo_shape_ = shape;
    lfo_cutoff_ = std::clamp(cutoff, 0.0f, 1.0f); lfo_pitch_ = std::clamp(pitch, 0.0f, 1.0f);
    lfo_pwm_ = std::clamp(pwm, 0.0f, 1.0f); lfo_beats_ = beats; lfo_delay_ms_ = std::max(delay_ms, 0.0f);
    lfo_.set_rate_hz(lfo_rate_); lfo_.set_shape(lfo_shape_); lfo_.set_sync(lfo_sync_, bpm_, lfo_beats_);
}
void JunoPreview::set_lfo_control(float wheel, float bpm, bool sync) {
    wheel_ = std::clamp(wheel, 0.0f, 1.0f); bpm_ = std::isfinite(bpm) && bpm > 0 ? bpm : 120;
    lfo_sync_ = sync; lfo_.set_sync(sync, bpm_, lfo_beats_);
}
void JunoPreview::process_frame(float& left, float& right) {
    left = right = 0;
    if (!voices_[0]) return;
    tick_ += 1000.0f;
    const bool update = tick_ >= fs_;
    if (update) {
        tick_ -= fs_;
        lfo_elapsed_ms_ += 1.0f;
        const float ramp = lfo_delay_ms_ > 1 ? std::min(1.0f, lfo_elapsed_ms_ / lfo_delay_ms_) : 1.0f;
        const float lfo = lfo_.tick(0.001f);
        const float vib = std::min(1.0f, lfo_pitch_ * ramp + 0.3f * wheel_);
        lfo_pitch_ratio_ = std::exp2(gf_lfo_pitch_st(vib) * lfo / 12.0f);
        lfo_cutoff_ratio_ = std::exp2(lfo_cutoff_ * ramp * GF_LFO_CUT_OCT * lfo);
        pwm_mod_ = 0.42f * lfo_pwm_ * ramp * lfo;
    }
    const bool advance_oscillator_drift = drift_samples_remaining_ <= 0.0;
    if (advance_oscillator_drift) drift_samples_remaining_ += 128.0 * double(fs_) / 44100.0;
    drift_samples_remaining_ -= 1.0;
    float mix = 0;
    for (auto& item : voices_) {
        auto& v = *item;
        const float filter_env = v.filter_env.step();
        if (update) {
            const float glide = glide_ms_ >= 1.0f ? 1.0f - std::exp(-1.0f / glide_ms_) : 1.0f;
            v.hz += (v.target_hz - v.hz) * glide;
            const float brightness = 1.0f - velocity_filter_ * (1.0f - v.velocity);
            v.filter.set_frequency(std::clamp((cutoff_ + 8000.0f * env_amount_ * brightness * filter_env) * v.spread.cutoff * v.cutoff_ratio * lfo_cutoff_ratio_, 20.0f, 20000.0f));
        }
        const float hz = v.hz * v.spread.pitch * v.pitch_ratio * bend_ * lfo_pitch_ratio_;
        float dco = 0, sub = 0;
        v.dco.render(waveform_, hz / fs_, std::clamp(pulse_width_ + pwm_mod_, 0.05f, 0.95f), 1, &dco, 1, 0, nullptr, nullptr, nullptr, 0, 0, advance_oscillator_drift);
        v.sub.render(dsp::WAVE_SQUARE, hz * 0.5f / fs_, 0.5f, 1, &sub, 1, 0, nullptr, nullptr, nullptr, 0, 0, advance_oscillator_drift);
        const float gain = 1.0f - velocity_amp_ * (1.0f - v.velocity);
        const float noise = float(v.noise.next()) * (1.0f / 32768.0f);
        mix += v.filter.process_sample((0.6f * dco + sub_level_ * sub + 0.5f * noise_level_ * noise) * gain) * v.amp.step() * 0.25f;
    }
    mix = std::clamp(mix, -1.0f, 1.0f);
    if (fs_ <= 95000) chorus_.render_stereo(&mix, &left, &right, 1, fs_);
    else chorus_high_rate_.render_stereo(&mix, &left, &right, 1, fs_);
    left = std::clamp(left * unison_gain_, -1.0f, 1.0f); right = std::clamp(right * unison_gain_, -1.0f, 1.0f);
}
}
