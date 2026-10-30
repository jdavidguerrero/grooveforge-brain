#include "dx7_preview.h"
#include <algorithm>
#include <cmath>

namespace gf {
void Dx7Preview::prepare(double fs) {
    fs_ = fs > 0 ? float(fs) : 44100.0f;
    patch_ = Dx7Patch{};
    patch_.algorithm = 5; patch_.feedback = 2;
    patch_.op[0] = {1.0f, 99, 0, 1800, 0.0f, 400};
    patch_.op[1] = {1.0f, 72, 0, 900, 0.15f, 300};
    patch_.op[2] = {1.0f, 88, 0, 2200, 0.0f, 400};
    patch_.op[3] = {14.0f, 52, 0, 160, 0.0f, 120};
    amp_attack_ = 2; amp_decay_ = 50; amp_sustain_ = 1; amp_release_ = amp_release_requested_ = 600; velocity_depth_ = 1;
    unison_ = 1; unison_cents_ = 0; unison_gain_ = 1; glide_ms_ = 0; drift_.amount = 0;
    lfo_rate_ = 2; lfo_pitch_ = lfo_amp_ = wheel_ = 0; lfo_beats_ = 0.5f; lfo_shape_ = dsp::LfoShape::Tri;
    tone_.emplace(fs_);
    tone_->set_character(dsp::LadderCharacter::Cem3320);
    tone_->set_frequency(20000.0f); tone_->set_resonance(0.7f);
    tone_->set_drive(dsp::LadderStageCore::prepare_drive(1.0f));
    reset();
}
void Dx7Preview::reset() {
    age_ = 0; next_ = 0; sustain_ = false; bend_ = 1;
    control_accumulator_ = 0; lfo_frequency_ = lfo_gain_ = tape_pitch_ = 1; drift_.state = 0x2545F491u;
    lfo_ = dsp::Lfo{}; lfo_.set_rate_hz(lfo_rate_); lfo_.set_shape(lfo_shape_); lfo_.set_sync(lfo_sync_, lfo_bpm_, lfo_beats_);
    for (auto& v : voices_) {
        v = Voice{}; v.amp.set_rate(fs_); v.amp.setAttack(amp_attack_); v.amp.setDecay(amp_decay_);
        v.amp.setSustain(amp_sustain_); v.amp.setRelease(amp_release_);
    }
    if (tone_) tone_->reset();
}
void Dx7Preview::note_on(uint8_t note, float velocity) {
    if (note > 127) return;
    if (unison_ > 1) release(note);
    for (uint8_t k = 0; k < unison_; ++k) {
        Voice* voice = nullptr;
        if (unison_ == 1) for (auto& v : voices_) if (v.note == note) { voice = &v; break; }
        if (!voice) for (size_t i = 0; i < voices_.size(); ++i) {
            const auto index = (next_ + i) % voices_.size();
            if (voices_[index].note < 0) { voice = &voices_[index]; next_ = uint8_t((index + 1) % voices_.size()); break; }
        }
        if (!voice) voice = &*std::min_element(voices_.begin(), voices_.end(), [](const Voice& a, const Voice& b) { return a.age < b.age; });
        const bool was_free = voice->note < 0;
        voice->target_hz = 440.0f * std::exp2((int(note) - 69) / 12.0f);
        if (was_free || glide_ms_ < 1.0f) voice->hz = voice->target_hz;
        voice->note = note; voice->age = ++age_; voice->deferred = false;
        voice->ratio = dsp::unison_ratio(k, unison_, unison_cents_) * drift_.pitch();
        const float vel = std::clamp(velocity, 0.0f, 1.0f);
        voice->core.note_on(voice->hz * bend_ * voice->ratio * lfo_frequency_ * tape_pitch_, 1.0f - velocity_depth_ * (1.0f - vel));
        voice->amp.noteOn();
    }
}
void Dx7Preview::release(uint8_t note) {
    for (auto& v : voices_) if (v.note == note) { v.core.note_off(); v.amp.noteOff(); v.note = -1; v.deferred = false; }
}
void Dx7Preview::note_off(uint8_t note) {
    if (sustain_) { for (auto& v : voices_) if (v.note == note) v.deferred = true; }
    else release(note);
}
void Dx7Preview::set_sustain(bool down) {
    sustain_ = down;
    if (!down) for (auto& v : voices_) if (v.deferred) release(uint8_t(v.note));
}
void Dx7Preview::set_pitch_bend(int value) {
    bend_ = std::exp2((2.0f * (std::clamp(value, 0, 16383) - 8192) / 8192.0f) / 12.0f);
    for (auto& v : voices_) if (v.note >= 0) v.core.set_freq(v.hz * bend_ * v.ratio * lfo_frequency_ * tape_pitch_);
}
void Dx7Preview::set_unison(uint8_t count, float cents, float drift) {
    unison_ = uint8_t(std::clamp<unsigned>(count, 1, 6));
    unison_cents_ = std::clamp(cents, 0.0f, 50.0f); unison_gain_ = dsp::unison_gain(unison_);
    drift_.amount = std::clamp(drift, 0.0f, 1.0f);
}
void Dx7Preview::set_modulation(float glide_ms, float rate, dsp::LfoShape shape, float pitch, float amp, float beats) {
    glide_ms_ = std::max(0.0f, glide_ms); lfo_rate_ = rate; lfo_shape_ = shape;
    lfo_pitch_ = std::clamp(pitch, 0.0f, 1.0f); lfo_amp_ = std::clamp(amp, 0.0f, 1.0f); lfo_beats_ = beats;
    lfo_.set_rate_hz(rate); lfo_.set_shape(shape); lfo_.set_sync(lfo_sync_, lfo_bpm_, lfo_beats_);
}
void Dx7Preview::set_lfo_control(float wheel, float bpm, bool sync) {
    wheel_ = std::clamp(wheel, 0.0f, 1.0f); lfo_bpm_ = bpm > 0 && std::isfinite(bpm) ? bpm : 120;
    lfo_sync_ = sync; lfo_.set_sync(sync, lfo_bpm_, lfo_beats_);
}
void Dx7Preview::set_patch(const Dx7Patch& patch, float cutoff, float resonance, float drive,
                           float attack, float decay, float sustain, float release_ms, float velocity_depth) {
    velocity_depth_ = std::clamp(velocity_depth, 0.0f, 1.0f);
    amp_attack_ = attack; amp_decay_ = decay; amp_sustain_ = sustain; amp_release_requested_ = release_ms;
    set_operator_patch(patch);
    for (auto& v : voices_) {
        v.amp.setAttack(attack); v.amp.setDecay(decay); v.amp.setSustain(sustain);
    }
    if (tone_) {
        tone_->set_frequency(std::max(500.0f, cutoff)); tone_->set_resonance(resonance);
        tone_->set_drive(dsp::LadderStageCore::prepare_drive(drive));
    }
}
void Dx7Preview::set_operator_patch(const Dx7Patch& patch) {
    patch_ = patch;
    amp_release_ = amp_release_requested_;
    for (unsigned i = 0; i < DX7_OPS; ++i)
        if (dx7_is_carrier(patch_.algorithm, int(i))) amp_release_ = std::max(amp_release_, patch_.op[i].release_ms);
    for (auto& v : voices_) v.amp.setRelease(amp_release_);
}
float Dx7Preview::process_sample() {
    if (!tone_) return 0;
    control_accumulator_ += 1000.0f;
    if (control_accumulator_ >= fs_) {
        control_accumulator_ -= fs_;
        const float lfo = lfo_.tick(0.001f);
        const float depth = std::min(1.0f, lfo_pitch_ + 0.3f * wheel_);
        lfo_frequency_ = std::exp2(depth * depth * lfo);
        lfo_gain_ = 1.0f - lfo_amp_ * (1.0f + lfo) * 0.5f;
        const float glide = glide_ms_ >= 1.0f ? 1.0f - std::exp(-1.0f / glide_ms_) : 1.0f;
        for (auto& v : voices_) {
            v.hz += (v.target_hz - v.hz) * glide;
            if (v.core.active()) v.core.set_freq(v.hz * bend_ * v.ratio * lfo_frequency_ * tape_pitch_);
        }
    }
    float sum = 0;
    for (auto& v : voices_) if (v.core.active() && v.amp.active()) {
        float sample = 0; v.core.render(patch_, fs_, &sample, 1);
        sum += sample * (26000.0f / 32768.0f) * v.amp.step() * 0.25f;
    }
    return std::clamp(tone_->process_sample(sum * lfo_gain_) * unison_gain_, -1.0f, 1.0f);
}
}
