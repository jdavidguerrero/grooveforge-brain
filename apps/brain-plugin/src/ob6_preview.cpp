#include "ob6_preview.h"
#include <algorithm>
#include <cmath>

namespace gf {
void Ob6Preview::prepare(double fs) {
    fs_ = std::isfinite(fs) && fs > 0 ? float(fs) : 44100;
    reset();
}
void Ob6Preview::reset() {
    age_ = 0; next_ = 0; sustain_ = false; bend_ = 1; tick_ = 0; drift_left_ = 0;
    for (size_t i = 0; i < voices_.size(); ++i) {
        auto& v = voices_[i]; v = Voice{};
        v.vco1.reset(0x0B601001u + uint32_t(i) * 131u);
        v.vco2.reset(0x0B602001u + uint32_t(i) * 131u);
        v.sub.reset(0x0B603001u + uint32_t(i) * 131u);
        v.vco1.set_drift(.00085f); v.vco2.set_drift(.00095f); v.sub.set_drift(.0005f);
        v.spread = dsp::voice_spread(0xB6B6u + uint32_t(i) * 7919u, 2, .03f);
        v.amp.set_rate(fs_); v.amp.setAttackOvershoot(.5f); v.amp.setADSR(attack_, 300, .6f, release_);
        v.filter_env.set_rate(1000); v.filter_env.setAttackOvershoot(.5f); v.filter_env.setADSR(10, 300, .6f, 500);
        v.coeff = dsp::sem_svf_compute(cutoff_, resonance_, 0, false, 1.3f, fs_);
    }
}
void Ob6Preview::note_on(uint8_t note, float velocity) {
    if (note > 127) return;
    Voice* v = nullptr;
    for (auto& x : voices_) if (x.note == note) { v = &x; break; }
    if (!v) for (size_t k = 0; k < voices_.size(); ++k) {
        const size_t i = (next_ + k) % voices_.size();
        if (voices_[i].note < 0) { v = &voices_[i]; next_ = uint8_t((i + 1) % voices_.size()); break; }
    }
    if (!v) v = &*std::min_element(voices_.begin(), voices_.end(), [](const Voice& a, const Voice& b) { return a.age < b.age; });
    v->note = note; v->age = ++age_; v->deferred = false; v->velocity = std::clamp(velocity, 0.0f, 1.0f);
    v->hz = 440 * std::exp2((int(note) - 69) / 12.0f);
    v->amp.noteOn(); v->filter_env.noteOn(false);
}
void Ob6Preview::note_off(uint8_t note) {
    for (auto& v : voices_) if (v.note == note) {
        if (sustain_) v.deferred = true;
        else { v.amp.noteOff(); v.filter_env.noteOff(); v.note = -1; v.deferred = false; }
    }
}
void Ob6Preview::set_sustain(bool down) {
    sustain_ = down;
    if (!down) for (auto& v : voices_) if (v.deferred) { v.amp.noteOff(); v.filter_env.noteOff(); v.note = -1; v.deferred = false; }
}
void Ob6Preview::set_pitch_bend(int value) { bend_ = std::exp2((2.0f * (std::clamp(value, 0, 16383) - 8192) / 8192.0f) / 12.0f); }
void Ob6Preview::set_parameters(float cutoff, float resonance, float attack, float release) {
    cutoff_ = std::clamp(cutoff, 20.0f, 20000.0f); resonance_ = std::clamp(resonance, .707f, 5.0f);
    if (attack_ != attack || release_ != release) {
        attack_ = attack; release_ = release;
        for (auto& v : voices_) { v.amp.setAttack(attack); v.amp.setRelease(release); }
    }
}
float Ob6Preview::process_sample() {
    tick_ += 1000; const bool update = tick_ >= fs_; if (update) tick_ -= fs_;
    const bool drift = drift_left_ <= 0; if (drift) drift_left_ += 128.0 * double(fs_) / 44100.0; drift_left_ -= 1;
    float mix = 0;
    for (auto& v : voices_) {
        if (update) {
            const float env = v.filter_env.update(1);
            const float bright = 1.0f - .5f * (1.0f - v.velocity);
            const float hz = std::clamp((cutoff_ + .4f * bright * env * 7500.0f) * v.spread.cutoff, 20.0f, 20000.0f);
            v.coeff = dsp::sem_svf_compute(hz, resonance_, 0, false, 1.3f, fs_);
        }
        const float hz = v.hz * v.spread.pitch * bend_;
        float a = 0, b = 0, sub = 0;
        v.vco1.render(dsp::WAVE_SAWTOOTH, hz/fs_, .5f, 1, &a, 1, 0, nullptr, nullptr, nullptr, 0, 0, drift);
        v.vco2.render(dsp::WAVE_SAWTOOTH, hz * 1.0040511f/fs_, .5f, 1, &b, 1, 0, nullptr, nullptr, nullptr, 0, 0, drift);
        v.sub.render(dsp::WAVE_SQUARE, hz * .5f/fs_, .5f, 1, &sub, 1, 0, nullptr, nullptr, nullptr, 0, 0, drift);
        const float velocity = 1 - .65f * (1 - v.velocity);
        float filtered = dsp::sem_svf_step(v.filter, (.45f*a + .45f*b + .1f*sub)*velocity, v.coeff);
        if (!std::isfinite(filtered)) { v.filter = dsp::SemSvfState{}; filtered = 0; }
        mix += std::clamp(filtered, -1.0f, 1.0f) * v.amp.step() * .25f;
    }
    return std::clamp(mix, -1.0f, 1.0f);
}
}
