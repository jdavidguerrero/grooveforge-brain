#include "arp2600_preview.h"
#include <algorithm>
#include <cmath>
#include <gf_params.h>

namespace gf {
void Arp2600Preview::prepare(double rate) {
    fs_ = std::isfinite(rate) && rate > 0 ? float(rate) : 44100.0f;
    for (auto& voice : voices_) voice.emplace(fs_);
    reset();
}
void Arp2600Preview::reset() {
    tick_ = 0; age_ = 0; next_ = 0; sustain_ = false;
    vco3_phase_ = 0; vco3_pitch_ratio_ = vco3_cutoff_ratio_ = bend_ = tape_pitch_ = 1;
    mono_notes_.clear(); mono_deferred_.fill(false);
    drift_.state = 0x2545F491u; drift_.amount = patch_.drift;
    lfo_ = dsp::Lfo{}; lfo_.set_rate_hz(patch_.lfo_rate); lfo_.set_shape(patch_.lfo_shape);
    lfo_.set_sync(lfo_sync_, bpm_, patch_.lfo_beats);
    lfo_pitch_ratio_ = lfo_cutoff_ratio_ = 1; lfo_pwm_ = 0;
    for (size_t i = 0; i < voices_.size(); ++i) if (voices_[i]) {
        auto& v = *voices_[i]; v = Voice(fs_);
        v.vco1.reset(0x26001001u + uint32_t(i) * 173u);
        v.vco2.reset(0x26002001u + uint32_t(i) * 173u);
        v.vco1.set_drift(.0008f); v.vco2.set_drift(.0009f);
        v.noise.prepare(fs_); v.noise.reset(0x26003001u + uint32_t(i) * 173u); v.noise.set_color(patch_.noise_color);
        v.spread = dsp::voice_spread(0x26002600u + uint32_t(i) * 7919u, 2, .03f);
        v.filter.set_character(dsp::LadderCharacter::Arp4072);
        v.filter.set_drive(dsp::LadderStageCore::prepare_drive(patch_.drive));
        v.filter.set_frequency(cutoff_); v.filter.set_resonance(resonance_);
        v.amp.set_rate(fs_); v.amp.setAttackOvershoot(.6f); v.amp.setADSR(attack_, patch_.decay, patch_.sustain, release_);
        v.filter_env.set_rate(1000); v.filter_env.setAttackOvershoot(.6f); v.filter_env.setADSR(patch_.filter_attack, patch_.filter_decay, patch_.filter_sustain, patch_.filter_release);
    }
}
void Arp2600Preview::note_on(uint8_t note, float velocity) {
    if (note > 127 || !std::isfinite(velocity) || !voices_[0]) return;
    if (patch_.mono) {
        auto& v = *voices_[0];
        mono_deferred_[note] = false;
        const bool legato = mono_notes_.push(note);
        v.target_hz = 440.0f * std::exp2((int(note) - 69) / 12.0f);
        if (!(legato && patch_.glide_ms >= 1 && v.note >= 0)) {
            v.hz = v.target_hz;
            v.velocity = std::clamp(velocity, 0.0f, 1.0f);
            v.pitch_ratio = drift_.pitch(); v.cutoff_ratio = drift_.cutoff();
            v.amp.noteOn(); v.filter_env.noteOn(false);
        }
        v.note = note;
        return;
    }
    const uint8_t count = std::clamp<uint8_t>(patch_.unison, 1, 4);
    if (count > 1) for (auto& v : voices_) if (v->note == note) {
        v->amp.noteOff(); v->filter_env.noteOff(); v->note = -1; v->deferred = false;
    }
    for (uint8_t k = 0; k < count; ++k) {
        Voice* v = nullptr;
        if (count == 1) for (auto& candidate : voices_) if (candidate->note == note) { v = &*candidate; break; }
        if (!v) for (size_t n = 0; n < voices_.size(); ++n) {
            const size_t i = (next_ + n) % voices_.size();
            if (voices_[i]->note < 0) { v = &*voices_[i]; next_ = (i + 1) % voices_.size(); break; }
        }
        if (!v) v = &**std::min_element(voices_.begin(), voices_.end(), [](const auto& a, const auto& b) { return a->age < b->age; });
        const bool free_voice = v->note < 0;
        v->note = note; v->velocity = std::clamp(velocity, 0.0f, 1.0f); v->age = ++age_; v->deferred = false;
        v->target_hz = 440.0f * std::exp2((int(note) - 69) / 12.0f);
        if (free_voice || patch_.glide_ms < 1) v->hz = v->target_hz;
        v->pitch_ratio = dsp::unison_ratio(k, count, patch_.unison_cents) * drift_.pitch();
        v->cutoff_ratio = drift_.cutoff();
        v->amp.noteOn(); v->filter_env.noteOn();
    }
}
void Arp2600Preview::release_mono_note(uint8_t note) {
    if (!voices_[0]) return;
    auto& v = *voices_[0];
    const uint8_t top = mono_notes_.remove(note);
    if (top == dsp::NoteStack::NONE) {
        if (v.note >= 0) { v.amp.noteOff(); v.filter_env.noteOff(); v.note = -1; }
    } else if (top != v.note) {
        v.note = top; v.target_hz = 440.0f * std::exp2((int(top) - 69) / 12.0f);
        if (patch_.glide_ms < 1) v.hz = v.target_hz;
    }
}
void Arp2600Preview::note_off(uint8_t note) {
    if (note > 127) return;
    if (patch_.mono) {
        if (sustain_) mono_deferred_[note] = true; else release_mono_note(note);
        return;
    }
    for (auto& voice : voices_) if (voice && voice->note == note) {
        if (sustain_) voice->deferred = true;
        else { voice->amp.noteOff(); voice->filter_env.noteOff(); voice->note = -1; voice->deferred = false; }
    }
}
void Arp2600Preview::set_pitch_bend(int value) {
    bend_ = std::exp2((2.0f * (std::clamp(value, 0, 16383) - 8192) / 8192.0f) / 12.0f);
}
void Arp2600Preview::set_tape_pitch(float ratio) { tape_pitch_ = std::isfinite(ratio) ? std::clamp(ratio, .5f, 2.0f) : 1.0f; }
void Arp2600Preview::set_sustain(bool down) {
    sustain_ = down;
    if (!down && patch_.mono) {
        for (size_t i = 0; i < mono_deferred_.size(); ++i) if (mono_deferred_[i]) { mono_deferred_[i] = false; release_mono_note(uint8_t(i)); }
        return;
    }
    if (!down) for (auto& voice : voices_) if (voice && voice->deferred) {
        voice->amp.noteOff(); voice->filter_env.noteOff(); voice->note = -1; voice->deferred = false;
    }
}
void Arp2600Preview::set_parameters(float cutoff, float resonance, float attack, float release) {
    cutoff_ = std::clamp(cutoff, 20.0f, 20000.0f); resonance_ = std::clamp(resonance, .707f, 5.0f);
    attack_ = std::clamp(attack, 0.0f, 10000.0f); release_ = std::clamp(release, 1.0f, 10000.0f);
    for (auto& v : voices_) if (v) { v->filter.set_resonance(resonance_); v->amp.setAttack(attack_); v->amp.setRelease(release_); }
}
void Arp2600Preview::set_patch(const Arp2600Patch& patch) {
    patch_ = patch;
    drift_.amount = std::clamp(patch_.drift, 0.0f, 1.0f);
    lfo_.set_rate_hz(patch_.lfo_rate); lfo_.set_shape(patch_.lfo_shape); lfo_.set_sync(lfo_sync_, bpm_, patch_.lfo_beats);
    for (auto& v : voices_) if (v) {
        v->amp.setDecay(patch_.decay); v->amp.setSustain(patch_.sustain);
        v->filter_env.setADSR(patch_.filter_attack, patch_.filter_decay, patch_.filter_sustain, patch_.filter_release);
        v->filter.set_drive(dsp::LadderStageCore::prepare_drive(patch_.drive));
        v->noise.set_color(patch_.noise_color);
    }
}
void Arp2600Preview::set_lfo_control(float wheel, float pressure, float bpm, bool sync) {
    wheel_ = std::clamp(wheel, 0.0f, 1.0f); pressure_ = std::clamp(pressure, 0.0f, 1.0f);
    bpm_ = std::isfinite(bpm) && bpm > 0 ? bpm : 120; lfo_sync_ = sync;
    lfo_.set_sync(sync, bpm_, patch_.lfo_beats);
}
void Arp2600Preview::set_ring_mix(float amount) { ring_ = std::clamp(amount, 0.0f, 1.0f); }
float Arp2600Preview::process_sample() {
    tick_ += 1000.0f / fs_; const bool control = tick_ >= 1;
    if (control) {
        tick_ -= 1;
        vco3_phase_ += patch_.vco3_rate * .001f;
        vco3_phase_ -= std::floor(vco3_phase_);
        const float triangle = 1.0f - 4.0f * std::abs(vco3_phase_ - .5f);
        vco3_pitch_ratio_ = std::exp2(patch_.vco3_pitch * triangle);
        vco3_cutoff_ratio_ = std::exp2(4.0f * patch_.vco3_cutoff * triangle);
        const float value = lfo_.tick(.001f);
        const float vibrato = std::min(1.0f, patch_.lfo_pitch + .3f * wheel_ + .3f * pressure_);
        lfo_pitch_ratio_ = std::exp2(gf_lfo_pitch_st(vibrato) * value / 12.0f);
        lfo_cutoff_ratio_ = std::exp2(patch_.lfo_cutoff * GF_LFO_CUT_OCT * value + pressure_);
        lfo_pwm_ = .42f * patch_.lfo_pwm * value;
    }
    float output = 0;
    for (auto& voice : voices_) if (voice) {
        auto& v = *voice;
        if (control) v.hz += (v.target_hz - v.hz) * (patch_.glide_ms >= 1 ? 1.0f - std::exp(-1.0f / patch_.glide_ms) : 1.0f);
        if (control) v.filter.set_frequency(std::clamp((cutoff_ + patch_.env_amount * (1.0f - patch_.velocity_filter * (1.0f - v.velocity)) * v.filter_env.step() * 6000) * v.spread.cutoff * v.cutoff_ratio * vco3_cutoff_ratio_ * lfo_cutoff_ratio_, 20.0f, 20000.0f));
        float a = 0, b = 0;
        dsp::SyncEvents sync_events;
        v.vco1.render(patch_.wave1, v.hz * v.spread.pitch * v.pitch_ratio * vco3_pitch_ratio_ * bend_ * lfo_pitch_ratio_ * tape_pitch_ / fs_, std::clamp(patch_.pulse_width + lfo_pwm_, .05f, .95f), 1, &a, 1, 0, patch_.sync ? &sync_events : nullptr, nullptr, nullptr, 0, 0, control);
        v.vco2.render(patch_.wave2, v.hz * v.spread.pitch * v.pitch_ratio * patch_.ratio * vco3_pitch_ratio_ * bend_ * lfo_pitch_ratio_ * tape_pitch_ / fs_, std::clamp(patch_.pulse_width + lfo_pwm_, .05f, .95f), 1, &b, 1, 0, nullptr, patch_.sync ? &sync_events : nullptr, nullptr, 0, 0, control);
        float noise = 0; v.noise.render(&noise, 1);
        const float sum = patch_.level1 + patch_.level2 + ring_ + .5f * patch_.noise_level;
        const float gain = sum > 1.15f ? 1.15f / sum : 1;
        output += .25f * v.filter.process_sample((patch_.level1 * a + patch_.level2 * b + ring_ * a * b + .5f * patch_.noise_level * noise) * gain * (1.0f - patch_.velocity_amp * (1.0f - v.velocity))) * v.amp.step();
    }
    return output * dsp::unison_gain(patch_.mono ? 1 : std::clamp<uint8_t>(patch_.unison, 1, 4));
}
}
