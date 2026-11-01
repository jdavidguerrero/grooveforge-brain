#include "prophet_preview.h"
#include "dsp/prophet_control_core.h"

#include <algorithm>
#include <cmath>

namespace gf {

ProphetPreview::Voice::Voice(float fs) : filter(fs) {
    amp.set_rate(fs);
    amp.setDecay(300.0f);
    amp.setSustain(0.7f);
    amp.setAttackOvershoot(0.5f);
    filter_env.setAttackOvershoot(0.5f);
    filter_env.setAttack(10.0f);
    filter_env.setDecay(300.0f);
    filter_env.setSustain(0.7f);
    filter_env.setRelease(500.0f);
    filter.set_character(dsp::LadderCharacter::Cem3320);
    filter.set_drive(dsp::LadderStageCore::prepare_drive(1.2f));
}

void ProphetPreview::prepare(double sample_rate) {
    sample_rate_ = sample_rate > 0.0 ? float(sample_rate) : 44100.0f;
    for (size_t i = 0; i < voices_.size(); ++i) {
        auto& v = voices_[i];
        v.emplace(sample_rate_);
        v->spread = dsp::voice_spread(0x50505u + uint32_t(i) * 7919u, 4.0f, 0.06f);
        v->filter.set_frequency(2000.0f);
        if (cutoff_ != 2000.0f) v->filter.set_frequency(cutoff_);
        v->filter.set_resonance(0.7f);
        if (resonance_ != 0.7f) v->filter.set_resonance(resonance_);
        v->amp.setAttack(attack_);
        v->amp.setDecay(decay_);
        v->amp.setSustain(sustain_level_);
        v->amp.setRelease(release_);
        v->filter_env.setAttack(filter_attack_);
        v->filter_env.setDecay(filter_decay_);
        v->filter_env.setSustain(filter_sustain_);
        v->filter_env.setRelease(filter_release_);
        v->filter.set_drive(dsp::LadderStageCore::prepare_drive(drive_));
    }
    age_ = 0;
    sustain_ = false;
    bend_ratio_ = 1.0f;
    control_accumulator_ = 0.0f;
    next_voice_ = 0;
    rng_ = 0x9E3779B9u;
    drift_.state = 0x2545F491u;
    lfo_ = dsp::Lfo{};
    lfo_.set_rate_hz(lfo_rate_);
    lfo_.set_shape(lfo_shape_);
    lfo_.set_sync(lfo_sync_, lfo_bpm_, lfo_beats_);
    lfo_value_ = 0.0f;
    lfo_pitch_multiplier_ = 1.0f;
    output_stage_.reset();
    output_stage_right_.reset();
    output_stage_.set(8.0f, 1.2f, sample_rate_);
    output_stage_right_.set(8.0f, 1.2f, sample_rate_);
    pump_gain_ = pump_target_ = 1.0f;
    transport_beats_ = 0.0;
    synth_drive_ = dsp::DriveCore{};
    synth_drive_right_ = dsp::DriveCore{};
    set_synth_drive(synth_drive_enabled_, synth_drive_type_, synth_drive_amount_, synth_drive_tone_, synth_drive_mix_);
    synth_tape_ = dsp::TapeCore{};
    synth_tape_right_ = dsp::TapeCore{};
    tape_modulation_ = dsp::TapeModulation{};
    tape_pitch_ = external_tape_pitch_ = 1.0f;
    set_synth_tape(synth_tape_enabled_, synth_tape_drive_, synth_tape_mix_, tape_wow_, tape_flutter_);
    synth_phaser_ = {};
    synth_phaser_[1].set_phase(0.25f);
    set_synth_phaser(synth_phaser_enabled_, phaser_depth_, phaser_rate_, phaser_mix_);
    for (auto& chorus : synth_chorus_) chorus.prepare();
    set_synth_chorus(synth_chorus_enabled_, chorus_depth_, chorus_rate_, chorus_mix_);
    for (auto& modal : synth_modal_) modal.prepare(sample_rate_);
    set_synth_modal(modal_enabled_, modal_material_, modal_size_, modal_decay_, modal_mix_);
    arp_ = dsp::ArpCore{};
    roots_ = {};
    sounding_notes_.fill(0);
    configure_arp();
}

void ProphetPreview::reset() {
    for (auto& v : voices_) if (v) { v->amp.reset(); v->filter_env.reset(); v->filter.reset(); v->osc.reset(); v->note = -1; v->deferred_release = false; }
    age_ = 0;
    sustain_ = false;
    bend_ratio_ = 1.0f;
    control_accumulator_ = 0.0f;
    next_voice_ = 0;
    rng_ = 0x9E3779B9u;
    drift_.state = 0x2545F491u;
    lfo_ = dsp::Lfo{};
    lfo_.set_rate_hz(lfo_rate_);
    lfo_.set_shape(lfo_shape_);
    lfo_.set_sync(lfo_sync_, lfo_bpm_, lfo_beats_);
    lfo_value_ = 0.0f;
    lfo_pitch_multiplier_ = 1.0f;
    output_stage_.reset();
    output_stage_right_.reset();
    pump_gain_ = pump_target_ = 1.0f;
    transport_beats_ = 0.0;
    synth_drive_ = dsp::DriveCore{};
    synth_drive_right_ = dsp::DriveCore{};
    set_synth_drive(synth_drive_enabled_, synth_drive_type_, synth_drive_amount_, synth_drive_tone_, synth_drive_mix_);
    synth_tape_ = dsp::TapeCore{};
    synth_tape_right_ = dsp::TapeCore{};
    tape_modulation_ = dsp::TapeModulation{};
    tape_pitch_ = external_tape_pitch_ = 1.0f;
    set_synth_tape(synth_tape_enabled_, synth_tape_drive_, synth_tape_mix_, tape_wow_, tape_flutter_);
    synth_phaser_ = {};
    synth_phaser_[1].set_phase(0.25f);
    set_synth_phaser(synth_phaser_enabled_, phaser_depth_, phaser_rate_, phaser_mix_);
    for (auto& chorus : synth_chorus_) chorus.reset();
    set_synth_chorus(synth_chorus_enabled_, chorus_depth_, chorus_rate_, chorus_mix_);
    arp_ = dsp::ArpCore{};
    roots_ = {};
    sounding_notes_.fill(0);
    configure_arp();
    if (delay_ready_) synth_delay_.clear();
    synth_plate_.reset();
    for (auto& modal : synth_modal_) modal.reset();
}

void ProphetPreview::configure_arp() {
    arp_.set_callbacks(arp_note_on, arp_note_off, this);
    arp_.set_mode(arp_mode_);
    arp_.set_step_beats(arp_step_);
    arp_.set_gate(arp_gate_);
    arp_.set_octaves(arp_octaves_);
    arp_.set_swing(arp_swing_);
    arp_.set_latch(arp_latch_);
}

void ProphetPreview::set_arp(bool enabled, dsp::ArpCore::Mode mode, float step, float gate,
                             uint8_t octaves, float swing, bool latch) {
    if (arp_enabled_ != enabled) {
        for (unsigned i = 0; i < roots_.size(); ++i) release_root(uint8_t(i));
        arp_.clear();
    }
    arp_enabled_ = enabled; arp_mode_ = mode; arp_step_ = step; arp_gate_ = gate;
    arp_octaves_ = octaves; arp_swing_ = swing; arp_latch_ = latch;
    configure_arp();
}

void ProphetPreview::set_chord(bool enabled, dsp::ChordMemory::Type type, uint8_t inversion, uint8_t key, float strum_ms) {
    if (chord_enabled_ != enabled)
        for (unsigned i = 0; i < roots_.size(); ++i) release_root(uint8_t(i));
    chord_enabled_ = enabled;
    chord_.set_type(type);
    chord_.set_inversion(inversion);
    chord_.set_key(key);
    strum_ms_ = std::max(0.0f, strum_ms);
}

void ProphetPreview::set_unison(uint8_t voices, float cents) {
    const auto count = uint8_t(std::clamp<unsigned>(voices, 1, voices_.size()));
    if (count != unison_voices_) {
        for (unsigned i = 0; i < roots_.size(); ++i) release_root(uint8_t(i));
        arp_.clear();
    }
    unison_voices_ = count;
    unison_cents_ = std::clamp(cents, 0.0f, 50.0f);
    unison_gain_ = dsp::unison_gain(count);
}

void ProphetPreview::set_drift(float amount) {
    drift_.amount = std::clamp(amount, 0.0f, 1.0f);
}

void ProphetPreview::arp_note_on(uint8_t note, uint8_t velocity, void* context) {
    static_cast<ProphetPreview*>(context)->voice_note_on(note, velocity / 127.0f);
}

void ProphetPreview::arp_note_off(uint8_t note, void* context) {
    static_cast<ProphetPreview*>(context)->voice_note_off(note);
}

void ProphetPreview::set_voice_sink(void* context, void (*on)(void*, uint8_t, float), void (*off)(void*, uint8_t)) {
    voice_sink_context_ = context; voice_sink_on_ = on; voice_sink_off_ = off;
}

void ProphetPreview::set_synth_tape(bool enabled, float drive, float mix, float wow, float flutter) {
    synth_tape_enabled_ = enabled;
    synth_tape_drive_ = drive;
    synth_tape_mix_ = mix;
    tape_wow_ = std::clamp(wow, 0.0f, 1.0f);
    tape_flutter_ = std::clamp(flutter, 0.0f, 1.0f);
    tape_pitch_ = 1.0f;
    tape_cutoff_ = 20000.0f;
    synth_tape_right_.set_drive(drive);
    synth_tape_right_.set_mix(mix);
    synth_tape_right_.set_cutoff(20000.0f, sample_rate_);
    synth_tape_.set_drive(drive);
    synth_tape_.set_mix(mix);
    synth_tape_.set_cutoff(20000.0f, sample_rate_);
}

void ProphetPreview::set_synth_phaser(bool enabled, float depth, float rate, float mix) {
    synth_phaser_enabled_ = enabled;
    phaser_depth_ = std::clamp(depth, 0.0f, 1.0f);
    phaser_rate_ = rate;
    phaser_mix_ = std::clamp(mix, 0.0f, 1.0f);
    for (auto& phaser : synth_phaser_) { phaser.set_depth(phaser_depth_); phaser.set_rate(phaser_rate_); }
}

void ProphetPreview::set_synth_chorus(bool enabled, float depth, float rate, float mix) {
    synth_chorus_enabled_ = enabled;
    chorus_depth_ = depth; chorus_rate_ = rate; chorus_mix_ = mix;
    for (auto& chorus : synth_chorus_) chorus.set(enabled, depth, rate, mix);
}

void ProphetPreview::set_delay_memory(int16_t* left, int16_t* right, uint32_t length) {
    delay_ready_ = left && right && length >= 2;
    if (!delay_ready_) return;
    synth_delay_ = dsp::PingPongDelayCore{};
    synth_delay_.init(left, right, length, sample_rate_);
    if (delay_enabled_) set_synth_delay(true, delay_beats_, delay_feedback_, delay_tone_, delay_mix_);
}

void ProphetPreview::set_synth_delay(bool enabled, float beats, float feedback, float tone, float mix) {
    if (enabled && !delay_enabled_ && delay_ready_) synth_delay_.clear();
    delay_enabled_ = enabled;
    delay_beats_ = beats; delay_feedback_ = feedback; delay_tone_ = tone; delay_mix_ = mix;
    if (!delay_ready_) return;
    synth_delay_.set_time(delay_beats_ * 60.0f / (transport_bpm_ > 20.0f ? transport_bpm_ : 120.0f));
    synth_delay_.set_feedback(feedback);
    synth_delay_.set_tone(tone);
    synth_delay_.set_mix(mix);
}

void ProphetPreview::set_plate_memory(int16_t* memory, size_t count) {
    synth_plate_.prepare(memory, count, sample_rate_);
}

void ProphetPreview::set_synth_plate(bool enabled, float decay, uint8_t size, float mix) {
    plate_enabled_ = enabled;
    plate_mix_ = std::clamp(mix, 0.0f, 1.0f);
    synth_plate_.set(0.3f + 0.68f * std::clamp(decay, 0.0f, 1.0f), 0.7f - 0.25f * std::min<uint8_t>(size, 2));
}

void ProphetPreview::set_synth_modal(bool enabled, uint8_t material, uint8_t size, float decay, float mix) {
    modal_enabled_ = enabled; modal_material_ = material; modal_size_ = size;
    modal_decay_ = decay; modal_mix_ = std::clamp(mix, 0.0f, 1.0f);
    for (auto& modal : synth_modal_) modal.configure(material, size, decay);
}

void ProphetPreview::set_pump(bool enabled, const dsp::PumpParams& parameters) {
    pump_enabled_ = enabled;
    pump_parameters_ = parameters;
}

void ProphetPreview::set_transport(float bpm, bool has_position, double position_beats, bool running) {
    transport_bpm_ = std::isfinite(bpm) && bpm > 0.0f ? bpm : 120.0f;
    if (delay_ready_ && delay_enabled_) synth_delay_.set_time(delay_beats_ * 60.0f / (transport_bpm_ > 20.0f ? transport_bpm_ : 120.0f));
    if (has_position && std::isfinite(position_beats)) transport_beats_ = position_beats;
    transport_running_ = running;
}

void ProphetPreview::set_synth_drive(bool enabled, uint8_t type, float amount, float tone, float mix) {
    synth_drive_enabled_ = enabled;
    synth_drive_type_ = type;
    synth_drive_amount_ = amount;
    synth_drive_tone_ = tone;
    synth_drive_mix_ = mix;
    synth_drive_right_.set_type(type);
    synth_drive_right_.set_amount(amount);
    synth_drive_right_.set_tone(tone, sample_rate_);
    synth_drive_right_.set_mix(mix);
    synth_drive_.set_type(type);
    synth_drive_.set_amount(amount);
    synth_drive_.set_tone(tone, sample_rate_);
    synth_drive_.set_mix(mix);
}

void ProphetPreview::set_lfo(float rate, dsp::LfoShape shape, float cutoff, float pitch, float pwm) {
    lfo_rate_ = rate;
    lfo_shape_ = shape;
    lfo_.set_rate_hz(rate);
    lfo_.set_shape(shape);
    lfo_cutoff_ = std::clamp(cutoff, 0.0f, 1.0f);
    lfo_pitch_ = std::clamp(pitch, 0.0f, 1.0f);
    lfo_pwm_ = std::clamp(pwm, 0.0f, 1.0f);
}

void ProphetPreview::set_lfo_sync(bool enabled, float bpm) {
    lfo_sync_ = enabled;
    lfo_bpm_ = std::isfinite(bpm) && bpm > 0.0f ? bpm : 120.0f;
    lfo_.set_sync(lfo_sync_, lfo_bpm_, lfo_beats_);
}

void ProphetPreview::set_lfo_division(float beats) {
    if (std::isfinite(beats) && beats > 0.0f) lfo_beats_ = beats;
    lfo_.set_sync(lfo_sync_, lfo_bpm_, lfo_beats_);
}

void ProphetPreview::set_parameters(float cutoff_hz, float resonance, float attack_ms, float release_ms) {
    cutoff_hz = std::clamp(cutoff_hz, 20.0f, 20000.0f);
    resonance = std::clamp(resonance, 0.707f, 5.0f);
    attack_ms = std::clamp(attack_ms, 0.0f, 5000.0f);
    release_ms = std::clamp(release_ms, 1.0f, 10000.0f);
    if (cutoff_ != cutoff_hz) { cutoff_ = cutoff_hz; for (auto& v : voices_) if (v) v->filter.set_frequency(cutoff_); }
    if (resonance_ != resonance) { resonance_ = resonance; for (auto& v : voices_) if (v) v->filter.set_resonance(resonance_); }
    if (attack_ != attack_ms) { attack_ = attack_ms; for (auto& v : voices_) if (v) v->amp.setAttack(attack_); }
    if (release_ != release_ms) { release_ = release_ms; for (auto& v : voices_) if (v) v->amp.setRelease(release_); }
}

void ProphetPreview::setAmpEnv(float attack, float decay, float sustain, float release) {
    attack_ = attack; decay_ = decay; sustain_level_ = sustain; release_ = release;
    for (auto& v : voices_) if (v) {
        v->amp.setAttack(attack); v->amp.setDecay(decay); v->amp.setSustain(sustain); v->amp.setRelease(release);
    }
}

void ProphetPreview::setFilterEnv(float attack, float decay, float sustain, float release) {
    filter_attack_ = attack; filter_decay_ = decay; filter_sustain_ = sustain; filter_release_ = release;
    for (auto& v : voices_) if (v) {
        v->filter_env.setAttack(attack); v->filter_env.setDecay(decay);
        v->filter_env.setSustain(sustain); v->filter_env.setRelease(release);
    }
}

void ProphetPreview::setDrive(float drive) {
    drive_ = drive;
    const auto setting = dsp::LadderStageCore::prepare_drive(drive);
    for (auto& v : voices_) if (v) v->filter.set_drive(setting);
}

void ProphetPreview::note_on(uint8_t note, float velocity) {
    if (note > 127) return;
    release_root(note);
    auto& root = roots_[note];
    uint8_t notes[dsp::ChordMemory::MAX] = {};
    root.count = chord_enabled_ ? chord_.notes(note, notes) : 1;
    if (!chord_enabled_) notes[0] = note;
    root.velocity = std::clamp(velocity, 0.0f, 1.0f);
    for (uint8_t i = 0; i < root.count; ++i) {
        root.notes[i] = {notes[i], arp_enabled_ ? 0.0f : strum_ms_ * i, false};
        if (root.notes[i].delay_ms <= 0.0f) start_chord_note(note, i);
    }
}

void ProphetPreview::start_chord_note(uint8_t root, uint8_t index) {
    auto& key = roots_[root];
    auto& entry = key.notes[index];
    entry.started = true;
    const bool first = sounding_notes_[entry.note]++ == 0;
    if (arp_enabled_) {
        if (first) arp_.note_on(entry.note, uint8_t(key.velocity * 127.0f + 0.5f));
    } else {
        voice_note_on(entry.note, key.velocity);
    }
}

void ProphetPreview::release_root(uint8_t root) {
    auto& key = roots_[root];
    for (uint8_t i = 0; i < key.count; ++i) {
        const auto& entry = key.notes[i];
        if (entry.started && sounding_notes_[entry.note] > 0 && --sounding_notes_[entry.note] == 0) {
            if (arp_enabled_) arp_.note_off(entry.note);
            else voice_note_off(entry.note);
        }
    }
    key = {};
}

void ProphetPreview::tick_strum() {
    for (unsigned root = 0; root < roots_.size(); ++root) {
        auto& key = roots_[root];
        for (uint8_t i = 0; i < key.count; ++i) {
            auto& entry = key.notes[i];
            if (!entry.started && (entry.delay_ms -= 1.0f) <= 0.0f)
                start_chord_note(uint8_t(root), i);
        }
    }
}

void ProphetPreview::voice_note_on(uint8_t note, float velocity) {
    if (voice_sink_on_) { voice_sink_on_(voice_sink_context_, note, velocity); return; }
    if (!voices_[0]) return;
    if (unison_voices_ > 1) voice_note_off(note);
    for (uint8_t k = 0; k < unison_voices_; ++k) {
    Voice* selected = nullptr;
    if (unison_voices_ == 1)
        for (auto& v : voices_) if (v->note == note) { selected = &*v; break; }
    if (!selected) for (size_t i = 0; i < voices_.size(); ++i) {
        const auto index = (next_voice_ + i) % voices_.size();
        if (voices_[index]->note < 0) {
            selected = &*voices_[index];
            next_voice_ = uint8_t((index + 1) % voices_.size());
            break;
        }
    }
    if (!selected) selected = &**std::min_element(voices_.begin(), voices_.end(),
        [](const auto& a, const auto& b) { return a->age < b->age; });
    const bool was_free = selected->note < 0;
    selected->note = note;
    selected->deferred_release = false;
    selected->age = ++age_;
    selected->target_hz = 440.0f * std::pow(2.0f, (int(note) - 69) / 12.0f);
    if (was_free || glide_ms_ < 1.0f) selected->hz = selected->target_hz;
    selected->velocity = std::clamp(velocity, 0.0f, 1.0f);
    selected->unison_ratio = dsp::unison_ratio(k, unison_voices_, unison_cents_) * drift_.pitch();
    selected->drift_cutoff = drift_.cutoff();
    selected->amp.noteOn();
    selected->filter_env.noteOn(false);
    }
}

void ProphetPreview::note_off(uint8_t note) {
    if (note > 127) return;
    if (sustain_) roots_[note].deferred = true;
    else release_root(note);
}

void ProphetPreview::voice_note_off(uint8_t note) {
    if (voice_sink_off_) { voice_sink_off_(voice_sink_context_, note); return; }
    for (auto& v : voices_) if (v && v->note == note) {
        v->amp.noteOff(); v->filter_env.noteOff(); v->note = -1; v->deferred_release = false;
    }
}

void ProphetPreview::set_sustain(bool down) {
    sustain_ = down;
    if (!down) for (unsigned i = 0; i < roots_.size(); ++i)
        if (roots_[i].deferred) release_root(uint8_t(i));
}

void ProphetPreview::set_external_tape_pitch(float ratio) {
    external_tape_pitch_ = std::isfinite(ratio) ? std::clamp(ratio, .5f, 2.0f) : 1.0f;
}
void ProphetPreview::set_pitch_bend(int value) {
    const int limited = std::clamp(value, 0, 16383);
    const float semitones = 2.0f * float(limited - 8192) / 8192.0f;
    bend_ratio_ = std::pow(2.0f, semitones / 12.0f);
}

float ProphetPreview::process_sample() {
    float left, right;
    process_frame(left, right);
    return left;
}

void ProphetPreview::process_frame(float& left, float& right) {
    process_frame_impl(left, right, false, 0.0f);
}

void ProphetPreview::process_voice_frame(float& left, float& right) {
    process_frame_impl(left, right, false, 0.0f, false, 0.0f, true);
}

void ProphetPreview::process_external_frame(float input, float& left, float& right) {
    process_frame_impl(left, right, true, input);
}

void ProphetPreview::process_external_stereo(float input_left, float input_right, float& left, float& right) {
    process_frame_impl(left, right, true, input_left, true, input_right);
}

void ProphetPreview::process_frame_impl(float& left, float& right, bool external, float input, bool stereo, float input_right, bool voice_only) {
    control_accumulator_ += 1000.0f;
    if (control_accumulator_ >= sample_rate_) {
        control_accumulator_ -= sample_rate_;
        rng_ ^= rng_ << 13; rng_ ^= rng_ >> 17; rng_ ^= rng_ << 5;
        wheel_noise_ = float(int32_t(rng_)) * (1.0f / 2147483648.0f);
        lfo_value_ = lfo_.tick(0.001f);
        lfo_pitch_multiplier_ = std::exp2(lfo_pitch_ * lfo_pitch_ * lfo_value_);
        for (auto& chorus : synth_chorus_) chorus.tick(0.001f);
        if (synth_tape_enabled_) {
            const auto mod = tape_modulation_.tick(0.001f, tape_wow_, tape_flutter_, 0.0f);
            tape_pitch_ = std::exp2(mod.pitch * (0.5f / 12.0f));
            if (std::fabs(mod.cutoff - tape_cutoff_) > 0.01f * tape_cutoff_) {
                tape_cutoff_ = mod.cutoff;
                synth_tape_.set_cutoff(tape_cutoff_, sample_rate_);
                synth_tape_right_.set_cutoff(tape_cutoff_, sample_rate_);
            }
        }
        tick_strum();
        const float wm = ((1.0f - wheel_mix_) * lfo_value_ + wheel_mix_ * wheel_noise_) * wheel_;
        if (arp_enabled_) {
            if (transport_running_) arp_.update(transport_beats_);
            else arp_.pause();
        }
        if (pump_enabled_ && transport_running_)
            pump_target_ = dsp::pump_gain(pump_parameters_, transport_beats_, transport_bpm_);
        else
            pump_target_ += (1.0f - pump_target_) * 0.05f;
        const float glide = glide_ms_ >= 1.0f ? 1.0f - std::exp(-1.0f / glide_ms_) : 1.0f;
        for (auto& v : voices_) if (v) {
            v->hz += (v->target_hz - v->hz) * glide;
            const float env = v->filter_env.update(1);
            const float vel_bright = dsp::prophet_velocity_gain(v->velocity, velocity_filter_);
            const float lfo_cut = std::exp2(lfo_cutoff_ * 4.0f * lfo_value_);
            float cutoff = dsp::prophet_envelope_cutoff(cutoff_, filter_env_amount_, vel_bright, env, lfo_cut, v->spread.cutoff, v->drift_cutoff);
            if (pm_filter_) {
                const float phase = v->osc.phase_b();
                const float osc_b = tri_b_ ? 1.0f - 4.0f * std::fabs(phase - 0.5f) : 2.0f * phase - 1.0f;
                cutoff *= prophet_fast_exp2(PROPHET_PM_OCT * (pm_env_ * env + 0.5f * pm_b_ * osc_b));
            }
            if (wheel_filter_) cutoff *= prophet_fast_exp2(2.0f * wm);
            if (filter_keyboard_ > 0.0f) cutoff *= std::pow(v->hz / 261.63f, filter_keyboard_);
            v->filter.set_frequency(std::clamp(cutoff, 20.0f, 20000.0f));
        }
    }
    float mixed = 0.0f;
    if (external) mixed = input;
    else for (auto& v : voices_) {
        if (!v || !v->amp.active()) continue;
        Prophet5OscParams p;
        const float wm = ((1.0f - wheel_mix_) * lfo_value_ + wheel_mix_ * wheel_noise_) * wheel_;
        const float hz = v->hz * v->spread.pitch * v->unison_ratio * bend_ratio_ * lfo_pitch_multiplier_ * tape_pitch_ * external_tape_pitch_;
        p.inc_a = hz * prophet_fast_exp2((osc_a_st_ + (wheel_freq_a_ ? 12.0f * wm : 0.0f)) / 12.0f) / sample_rate_;
        float hz_b = (b_keyboard_ ? hz : 261.63f) * prophet_fast_exp2(
            (osc_b_st_ + osc_b_fine_ / 100.0f + (wheel_freq_b_ ? 12.0f * wm : 0.0f)) / 12.0f);
        if (b_lo_) hz_b *= 0.01f;
        p.inc_b = hz_b / sample_rate_;
        p.a_saw = saw_a_; p.a_pulse = pulse_a_; p.sync = sync_;
        p.b_saw = saw_b_; p.b_tri = tri_b_; p.b_pulse = pulse_b_;
        p.pw_a = std::clamp(pw_a_ + 0.45f * lfo_pwm_ * lfo_value_ + (wheel_pw_a_ ? 0.45f * wm : 0.0f), 0.05f, 0.95f);
        p.pw_b = std::clamp(pw_b_ + 0.45f * lfo_pwm_ * lfo_value_ + (wheel_pw_b_ ? 0.45f * wm : 0.0f), 0.05f, 0.95f);
        p.pm_env = pm_env_; p.pm_b = pm_b_; p.env = v->filter_env.level();
        p.pm_freq_a = pm_freq_; p.pm_pw_a = pm_pw_;
        const float vel_gain = dsp::prophet_velocity_gain(v->velocity, velocity_amp_);
        p.lvl_a = level_a_ * vel_gain;
        p.lvl_b = level_b_ * vel_gain;
        p.lvl_n = level_noise_ * vel_gain;
        float sample = 0.0f;
        v->osc.render(p, &sample, 1);
        mixed += v->filter.process_sample(sample * (20000.0f / 32768.0f)) * v->amp.step() * 0.25f;
    }
    if (voice_only) {
        left = right = mixed * unison_gain_;
        if (transport_running_) transport_beats_ += double(transport_bpm_) / (60.0 * sample_rate_);
        return;
    }
    float out = output_stage_.tick(std::clamp(mixed * 0.7f, -1.0f, 1.0f));
    if (synth_drive_enabled_) synth_drive_.render(&out, &out, 1);
    if (synth_tape_enabled_) synth_tape_.render(&out, &out, 1);
    float out_right = out;
    if (stereo) {
        out_right = output_stage_right_.tick(std::clamp(input_right * 0.7f, -1.0f, 1.0f));
        if (synth_drive_enabled_) synth_drive_right_.render(&out_right, &out_right, 1);
        if (synth_tape_enabled_) synth_tape_right_.render(&out_right, &out_right, 1);
    }
    left = synth_chorus_[0].process(out);
    right = synth_chorus_[1].process(out_right);
    if (synth_phaser_enabled_) {
        float wet_left, wet_right;
        synth_phaser_[0].render(&out, &wet_left, 1, sample_rate_);
        synth_phaser_[1].render(&out_right, &wet_right, 1, sample_rate_);
        left += phaser_mix_ * (wet_left - out);
        right += phaser_mix_ * (wet_right - out_right);
    }
    if (delay_enabled_ && delay_ready_) {
        float delayed_left, delayed_right;
        synth_delay_.render(&left, &right, &delayed_left, &delayed_right, 1);
        left = delayed_left; right = delayed_right;
    }
    const float modal_left = synth_modal_[0].process(left);
    const float modal_right = synth_modal_[1].process(right);
    if (plate_enabled_) {
        float wet_left, wet_right;
        synth_plate_.process(0.5f * (left + right), wet_left, wet_right);
        const float dry = 1.0f - 0.4f * plate_mix_;
        left = dry * left + plate_mix_ * wet_left;
        right = dry * right + plate_mix_ * wet_right;
    } else if (modal_enabled_) {
        left = (1.0f - modal_mix_) * left + modal_mix_ * modal_left;
        right = (1.0f - modal_mix_) * right + modal_mix_ * modal_right;
    }
    pump_gain_ += (pump_target_ - pump_gain_) * std::min(1.0f, 1000.0f / sample_rate_);
    if (transport_running_) transport_beats_ += double(transport_bpm_) / (60.0 * sample_rate_);
    const float gain = pump_gain_ * unison_gain_;
    left = std::clamp(left * gain, -1.0f, 1.0f);
    right = std::clamp(right * gain, -1.0f, 1.0f);
}

} // namespace gf
