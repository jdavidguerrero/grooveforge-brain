#include "ob6_preview.h"
#include <algorithm>
#include <cmath>
#include <gf_params.h>

namespace gf {
void Ob6Preview::prepare(double fs) {
    fs_ = std::isfinite(fs) && fs > 0 ? float(fs) : 44100;
    reset();
}
void Ob6Preview::reset() {
    age_ = 0; next_ = 0; sustain_ = false; bend_ = 1; tick_ = 0; drift_left_ = 0;
    note_drift_.state = 0x2545F491u; lfo_ = dsp::Lfo{};
    lfo_.set_rate_hz(patch_.lfo_rate); lfo_.set_shape(patch_.lfo_shape); lfo_.set_sync(lfo_sync_, bpm_, patch_.lfo_beats);
    lfo_pitch_ratio_ = lfo_cutoff_ratio_ = 1; lfo_pwm_ = 0;
    for (size_t i = 0; i < voices_.size(); ++i) {
        auto& v = voices_[i]; v = Voice{};
        v.vco1.reset(0x0B601001u + uint32_t(i) * 131u);
        v.vco2.reset(0x0B602001u + uint32_t(i) * 131u);
        v.sub.reset(0x0B603001u + uint32_t(i) * 131u);
        v.noise.reset(uint32_t(i) + 7u);
        v.vco1.set_drift(.00085f); v.vco2.set_drift(.00095f); v.sub.set_drift(.0005f);
        v.spread = dsp::voice_spread(0xB6B6u + uint32_t(i) * 7919u, 2, .03f);
        v.amp.set_rate(fs_); v.amp.setAttackOvershoot(.5f); v.amp.setADSR(attack_, patch_.amp_decay, patch_.amp_sustain, release_);
        v.filter_env.set_rate(1000); v.filter_env.setAttackOvershoot(.5f); v.filter_env.setADSR(patch_.filter_attack, patch_.filter_decay, patch_.filter_sustain, patch_.filter_release);
        v.coeff = dsp::sem_svf_compute(cutoff_, resonance_, patch_.morph, patch_.bandpass, patch_.drive, fs_);
    }
}
void Ob6Preview::note_on(uint8_t note, float velocity) {
    if (note > 127) return;
    if (patch_.unison > 1) for (auto& v : voices_) if (v.note == note) {
        v.amp.noteOff(); v.filter_env.noteOff(); v.note = -1; v.deferred = false;
    }
    for (uint8_t k = 0; k < patch_.unison; ++k) {
        Voice* v = nullptr;
        if (patch_.unison == 1) for (auto& x : voices_) if (x.note == note) { v = &x; break; }
        if (!v) for (size_t n = 0; n < voices_.size(); ++n) {
            const size_t i = (next_ + n) % voices_.size();
            if (voices_[i].note < 0) { v = &voices_[i]; next_ = uint8_t((i + 1) % voices_.size()); break; }
        }
        if (!v) v = &*std::min_element(voices_.begin(), voices_.end(), [](const Voice& a, const Voice& b) { return a.age < b.age; });
        const bool free = v->note < 0;
        v->target_hz = 440 * std::exp2((int(note) - 69) / 12.0f);
        if (free || patch_.glide_ms < 1.0f) v->hz = v->target_hz;
        v->note = note; v->age = ++age_; v->deferred = false; v->velocity = std::clamp(velocity, 0.0f, 1.0f);
        v->pitch_ratio = dsp::unison_ratio(k, patch_.unison, patch_.unison_cents) * note_drift_.pitch();
        v->cutoff_ratio = note_drift_.cutoff();
        v->amp.noteOn(); v->filter_env.noteOn(false);
    }
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
void Ob6Preview::set_sound_patch(const Ob6SoundPatch& source) {
    patch_ = source;
    auto norm = [](float v) { return std::clamp(v, 0.0f, 1.0f); };
    patch_.shape1 = norm(source.shape1); patch_.shape2 = norm(source.shape2);
    patch_.level1 = norm(source.level1); patch_.level2 = norm(source.level2);
    patch_.sub = norm(source.sub); patch_.noise = norm(source.noise);
    patch_.pw1 = std::clamp(source.pw1, .05f, .95f); patch_.pw2 = std::clamp(source.pw2, .05f, .95f);
    patch_.interval = std::clamp(source.interval, -48.0f, 48.0f); patch_.detune = std::clamp(source.detune, -100.0f, 100.0f);
    patch_.morph = norm(source.morph); patch_.drive = std::clamp(source.drive, .5f, 4.0f);
    patch_.xmod_env = norm(source.xmod_env); patch_.xmod_vco2 = norm(source.xmod_vco2);
    patch_.filter_amount = norm(source.filter_amount); patch_.velocity_amp = norm(source.velocity_amp); patch_.velocity_filter = norm(source.velocity_filter);
    patch_.amp_decay = std::clamp(source.amp_decay, 0.0f, 12000.0f); patch_.amp_sustain = norm(source.amp_sustain);
    patch_.filter_attack = std::clamp(source.filter_attack, 0.0f, 12000.0f); patch_.filter_decay = std::clamp(source.filter_decay, 0.0f, 12000.0f);
    patch_.filter_sustain = norm(source.filter_sustain); patch_.filter_release = std::clamp(source.filter_release, 0.0f, 12000.0f);
    patch_.glide_ms = std::clamp(source.glide_ms, 0.0f, 1500.0f);
    patch_.unison = std::clamp<uint8_t>(source.unison, 1, 6); patch_.unison_cents = std::clamp(source.unison_cents, 0.0f, 50.0f);
    note_drift_.amount = norm(source.drift); unison_gain_ = dsp::unison_gain(patch_.unison);
    patch_.lfo_rate = std::clamp(source.lfo_rate, .05f, 20.0f); patch_.lfo_cutoff = norm(source.lfo_cutoff);
    patch_.lfo_pitch = norm(source.lfo_pitch); patch_.lfo_pwm = norm(source.lfo_pwm);
    patch_.lfo_beats = source.lfo_beats > 0 ? source.lfo_beats : .5f;
    lfo_.set_rate_hz(patch_.lfo_rate); lfo_.set_shape(patch_.lfo_shape); lfo_.set_sync(lfo_sync_, bpm_, patch_.lfo_beats);
    vco2_ratio_ = std::exp2((patch_.interval + patch_.detune * .01f) / 12.0f);
    patch_loaded_ = true;
    for (auto& v : voices_) {
        v.amp.setDecay(patch_.amp_decay); v.amp.setSustain(patch_.amp_sustain);
        v.filter_env.setADSR(patch_.filter_attack, patch_.filter_decay, patch_.filter_sustain, patch_.filter_release);
    }
}
void Ob6Preview::set_lfo_control(float wheel, float bpm, bool sync) {
    wheel_ = std::clamp(wheel, 0.0f, 1.0f); bpm_ = std::isfinite(bpm) && bpm > 0 ? bpm : 120;
    lfo_sync_ = sync; lfo_.set_sync(sync, bpm_, patch_.lfo_beats);
}
float Ob6Preview::process_sample() {
    tick_ += 1000; const bool update = tick_ >= fs_;
    if (update) {
        tick_ -= fs_;
        const float value = lfo_.tick(.001f);
        const float vib = std::min(1.0f, patch_.lfo_pitch + .3f * wheel_);
        lfo_pitch_ratio_ = std::exp2(gf_lfo_pitch_st(vib) * value / 12.0f);
        lfo_cutoff_ratio_ = std::exp2(patch_.lfo_cutoff * GF_LFO_CUT_OCT * value);
        lfo_pwm_ = .42f * patch_.lfo_pwm * value;
    }
    const bool drift = drift_left_ <= 0; if (drift) drift_left_ += 128.0 * double(fs_) / 44100.0; drift_left_ -= 1;
    float mix = 0;
    for (auto& v : voices_) {
        if (update) {
            const float env = v.filter_env.update(1);
            const float glide = patch_.glide_ms >= 1 ? 1.0f - std::exp(-1.0f / patch_.glide_ms) : 1.0f;
            v.hz += (v.target_hz - v.hz) * glide;
            const float bright = 1.0f - patch_.velocity_filter * (1.0f - v.velocity);
            const float hz = std::clamp((cutoff_ + patch_.filter_amount * bright * env * 7500.0f) * v.spread.cutoff * v.cutoff_ratio * lfo_cutoff_ratio_, 20.0f, 20000.0f);
            const float morph = patch_.xmod_morph ? std::clamp(patch_.morph + .75f * patch_.xmod_env * env, 0.0f, 1.0f) : patch_.morph;
            v.coeff = dsp::sem_svf_compute(hz, resonance_, morph, patch_.bandpass, patch_.drive, fs_);
        }
        const float hz = v.hz * v.spread.pitch * v.pitch_ratio * bend_ * lfo_pitch_ratio_;
        float a = 0, b = 0, sub = 0;
        const auto wave = patch_loaded_ ? dsp::WAVE_SHAPE_MORPH : dsp::WAVE_SAWTOOTH;
        const float env_mod = patch_.xmod_env * v.filter_env.level();
        const float master_hz = patch_.sync ? hz : hz * vco2_ratio_;
        const float slave_base_hz = patch_.sync ? hz * vco2_ratio_ : hz;
        const float slave_hz = patch_.xmod_freq ? slave_base_hz * std::exp2(2.0f * env_mod) : slave_base_hz;
        const float pw1 = std::clamp(patch_.pw1 + lfo_pwm_ + (patch_.xmod_pw ? .4f * env_mod : 0.0f), .05f, .95f);
        dsp::SyncEvents events{};
        v.vco2.render(wave, master_hz/fs_, std::clamp(patch_.pw2 + lfo_pwm_, .05f, .95f), 1, &b, 1, patch_.shape2,
                      patch_.sync ? &events : nullptr, nullptr, nullptr, 0, 0, drift);
        v.vco1.render(wave, slave_hz/fs_, pw1, 1, &a, 1, patch_.shape1, nullptr,
                      patch_.sync ? &events : nullptr, &b,
                      patch_.xmod_freq ? 2.0f * patch_.xmod_vco2 : 0.0f,
                      patch_.xmod_pw ? .4f * patch_.xmod_vco2 : 0.0f, drift);
        v.sub.render(dsp::WAVE_SQUARE, slave_hz * .5f/fs_, .5f, 1, &sub, 1, 0, nullptr, nullptr, nullptr, 0, 0, drift);
        const float velocity = 1 - patch_.velocity_amp * (1 - v.velocity);
        const float noise = float(v.noise.next()) * (1.0f / 32768.0f);
        float filtered = dsp::sem_svf_step(v.filter, (patch_.level1*a + patch_.level2*b + patch_.sub*sub + .5f*patch_.noise*noise)*velocity, v.coeff);
        if (!std::isfinite(filtered)) { v.filter = dsp::SemSvfState{}; filtered = 0; }
        mix += std::clamp(filtered, -1.0f, 1.0f) * v.amp.step() * .25f;
    }
    return std::clamp(mix * unison_gain_, -1.0f, 1.0f);
}
}
