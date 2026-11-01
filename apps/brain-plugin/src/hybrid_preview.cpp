#include "hybrid_preview.h"
#include "hybrid_codec.h"
#include "model_d_preset.h"
#include "juno_preset.h"
#include "prophet_preset.h"
#include "ob6_preset.h"
#include "dx7_preset.h"
#include "arp2600_preset.h"
#include "ui/groove_macros.h"
#include <algorithm>
#include <cmath>
#include <limits>
namespace gf {
void HybridPreview::Half::prepare(double fs) {
    model.prepare(fs); juno.prepare(fs); prophet.prepare(fs); ob6.prepare(fs); dx7.prepare(fs); arp.prepare(fs);
}
void HybridPreview::Half::configure(const studio::Preset& original, bool second) {
    auto preset = original;
    if (second) preset.patch.groove_on[GF_GROOVE_UNISON] = false;
    this->preset = preset;
    last_controls.fill(std::numeric_limits<float>::quiet_NaN());
    const auto patch = studio::macro_patch(preset.patch, engine);
    const float move = studio::macro_amount(preset.patch, GF_MACRO_MOVE);
    const float cutoff = std::clamp(preset.cutoff_hz * studio::move_cutoff_mul(move), GF_CUTOFF_RANGE[engine].lo, GF_CUTOFF_RANGE[engine].hi);
    const float res = studio::move_resonance(studio::macro_resonance(preset.reso_norm, preset.patch), move);
    const float q = std::clamp((engine == 3 ? .7f + 4.3f * res : .1f + 4.9f * res), .707f, 5.0f);
    switch (engine) {
        case 0: model.set_parameters(cutoff, q, patch.attack_ms, patch.release_ms); apply_model_d_preset_preview(model, preset); model.reset(); break;
        case 1: juno.set_parameters(cutoff, q, patch.attack_ms, patch.release_ms); apply_juno_preset_preview(juno, preset); juno.reset(); break;
        case 2:
            for (auto& enabled : preset.patch.sfx_on) enabled = false;
            for (int g = 0; g < 3; ++g) preset.patch.groove_on[g] = false;
            apply_prophet_preset_preview(prophet, preset); prophet.reset(); break;
        case 3: ob6.set_parameters(cutoff, q, patch.attack_ms, patch.release_ms); apply_ob6_preset_preview(ob6, preset); ob6.reset(); break;
        case 4: apply_dx7_preset_preview(dx7, preset); dx7.reset(); break;
        case 5: arp.set_parameters(cutoff, q, patch.attack_ms, patch.release_ms); arp.set_patch(make_arp2600_patch(preset));
                arp.set_ring_mix(std::clamp(patch.osc[0][2] * .01f, 0.0f, 1.0f)); arp.reset(); break;
    }
}
void HybridPreview::Half::reset() {
    switch (engine) { case 0: model.reset(); break; case 1: juno.reset(); break; case 2: prophet.reset(); break;
        case 3: ob6.reset(); break; case 4: dx7.reset(); break; case 5: arp.reset(); break; }
}
void HybridPreview::Half::note_on(uint8_t n, float v) {
    switch (engine) { case 0: model.note_on(n,v); break; case 1: juno.note_on(n,v); break; case 2: prophet.note_on(n,v); break;
        case 3: ob6.note_on(n,v); break; case 4: dx7.note_on(n,v); break; case 5: arp.note_on(n,v); break; }
}
void HybridPreview::Half::note_off(uint8_t n) {
    switch (engine) { case 0: model.note_off(n); break; case 1: juno.note_off(n); break; case 2: prophet.note_off(n); break;
        case 3: ob6.note_off(n); break; case 4: dx7.note_off(n); break; case 5: arp.note_off(n); break; }
}
void HybridPreview::Half::update_patch(const studio::Preset& updated) {
    const auto saved = last_controls;
    preset = updated;
    switch (engine) {
        case 0: apply_model_d_preset_preview(model, preset); break;
        case 1: apply_juno_preset_preview(juno, preset); break;
        case 2: {
            auto dry = preset;
            for (auto& enabled : dry.patch.sfx_on) enabled = false;
            for (int g = 0; g < 3; ++g) dry.patch.groove_on[g] = false;
            apply_prophet_preset_preview(prophet, dry); break;
        }
        case 3: apply_ob6_preset_preview(ob6, preset); break;
        case 4: apply_dx7_preset_preview(dx7, preset); break;
        case 5: arp.set_patch(make_arp2600_patch(preset)); arp.set_ring_mix(std::clamp(preset.patch.osc[0][2] * .01f, 0.0f, 1.0f)); break;
    }
    last_controls.fill(std::numeric_limits<float>::quiet_NaN());
    if (std::isfinite(saved[0])) controls(saved[0], saved[1], saved[2], saved[3]);
}
void HybridPreview::Half::controls(float cutoff, float resonance, float attack, float release) {
    const std::array<float, 4> values{cutoff, resonance, attack, release};
    if (values == last_controls) return;
    last_controls = values;
    auto p = preset; p.cutoff_hz = cutoff; p.reso_norm = resonance; p.patch.attack_ms = attack; p.patch.release_ms = release;
    const auto effective = studio::macro_patch(p.patch, engine);
    const float move = studio::macro_amount(p.patch, GF_MACRO_MOVE);
    const float hz = std::clamp(p.cutoff_hz * studio::move_cutoff_mul(move), GF_CUTOFF_RANGE[engine].lo, GF_CUTOFF_RANGE[engine].hi);
    const float r = studio::move_resonance(studio::macro_resonance(p.reso_norm, p.patch), move);
    const float q = std::clamp(engine == 3 ? .7f + 4.3f * r : .1f + 4.9f * r, .707f, 5.0f);
    switch (engine) {
        case 0: model.set_parameters(hz, q, effective.attack_ms, effective.release_ms); break;
        case 1: juno.set_parameters(hz, q, effective.attack_ms, effective.release_ms); break;
        case 2: prophet.set_parameters(hz, q, effective.attack_ms, effective.release_ms); break;
        case 3: ob6.set_parameters(hz, q, effective.attack_ms, effective.release_ms); break;
        case 4: apply_dx7_preset_preview(dx7, p); break;
        case 5: arp.set_parameters(hz, q, effective.attack_ms, effective.release_ms); break;
    }
}
void HybridPreview::Half::pitch_bend(int value) {
    switch (engine) { case 0: model.set_pitch_bend(value); break; case 1: juno.set_pitch_bend(value); break;
        case 2: prophet.set_pitch_bend(value); break; case 3: ob6.set_pitch_bend(value); break;
        case 4: dx7.set_pitch_bend(value); break; case 5: arp.set_pitch_bend(value); break; }
}
void HybridPreview::Half::expression(float wheel, float pressure, float bpm, bool sync) {
    switch (engine) { case 0: model.set_modulation_control(wheel, pressure, bpm, sync); break;
        case 1: juno.set_lfo_control(wheel, bpm, sync); break;
        case 2: prophet.setWheel(wheel); prophet.set_lfo_sync(sync, bpm); break;
        case 3: ob6.set_lfo_control(wheel, bpm, sync, pressure); break;
        case 4: dx7.set_lfo_control(wheel, bpm, sync); break;
        case 5: arp.set_lfo_control(wheel, pressure, bpm, sync); break; }
}
void HybridPreview::Half::process(float& l, float& r, float tape) {
    switch (engine) {
        case 0: model.set_tape_pitch(tape); l = r = model.process_sample(); break;
        case 1: juno.set_tape_pitch(tape); juno.process_frame(l,r); break;
        case 2: prophet.set_external_tape_pitch(tape); prophet.process_voice_frame(l,r); break;
        case 3: ob6.set_tape_pitch(tape); l = r = ob6.process_sample(); break;
        case 4: dx7.set_tape_pitch(tape); l = r = dx7.process_sample(); break;
        case 5: arp.set_tape_pitch(tape); l = r = arp.process_sample(); break;
        default: l = r = 0;
    }
}
void HybridPreview::route_on(void* ctx, uint8_t side, uint8_t note, float velocity) { static_cast<HybridPreview*>(ctx)->halves_[side].note_on(note, velocity); }
void HybridPreview::route_off(void* ctx, uint8_t side, uint8_t note) { static_cast<HybridPreview*>(ctx)->halves_[side].note_off(note); }
void HybridPreview::groove_on(void* ctx, uint8_t note, float velocity) { static_cast<HybridPreview*>(ctx)->router_.note_on(note, velocity); }
void HybridPreview::groove_off(void* ctx, uint8_t note) { static_cast<HybridPreview*>(ctx)->router_.note_off(note); }
void HybridPreview::prepare(double fs) {
    fs_ = std::isfinite(fs) && fs >= 1000 ? fs : 44100;
    const size_t length = size_t(fs_) + 2;
    delay_l_.assign(length, 0); delay_r_.assign(length, 0);
    plate_.assign(PlateReverbPreview::required_samples(float(fs_)), 0);
    bus_.prepare(fs_); bus_.set_delay_memory(delay_l_.data(), delay_r_.data(), uint32_t(length));
    bus_.set_plate_memory(plate_.data(), plate_.size());
    bus_.set_voice_sink(this, groove_on, groove_off);
    router_.set_callbacks(this, route_on, route_off); router_.prepare(fs_);
    for (auto& half : halves_) half.prepare(fs_);
    if (loaded_) { auto layer = layer_; load(layer); }
}
bool HybridPreview::load(const studio::LayerPreset& layer) {
    std::array<uint8_t, studio::LAYER_BYTES> validation{};
    if (!encode_hybrid_layer(layer, validation.data(), validation.size())) return false;
    router_.reset(); bus_.reset();
    halves_[0].engine = layer.eng_a; halves_[1].engine = layer.eng_b;
    halves_[0].configure(layer.a, false); halves_[1].configure(layer.b, true);
    layer_ = layer;
    last_b_move_ = -1;
    update_unison_gain();
    last_macros_.fill(std::numeric_limits<float>::quiet_NaN());
    last_groove_.fill(std::numeric_limits<float>::quiet_NaN()); last_effects_.fill(std::numeric_limits<float>::quiet_NaN());
    for (auto& voice : last_voice_) voice.fill(std::numeric_limits<float>::quiet_NaN());
    for (auto& osc : last_osc_) osc.fill(std::numeric_limits<float>::quiet_NaN());
    router_.configure(layer.morph * .01f, float(layer.gain_a_db), float(layer.gain_b_db), layer.delay_b_ms, layer.transpose_b);
    auto effects = layer.a; effects.patch = studio::move_sfx(effects.patch, studio::macro_amount(effects.patch, GF_MACRO_MOVE));
    apply_synth_fx_preview(bus_, effects, layer.eng_a); apply_groove_preview(bus_, layer.a, false);
    loaded_ = true; return true;
}
void HybridPreview::reset() { router_.reset(); bus_.reset(); for (auto& h : halves_) h.reset(); }
void HybridPreview::note_on(uint8_t n, float v) { if (loaded_) bus_.note_on(n,v); }
void HybridPreview::note_off(uint8_t n) { if (loaded_) bus_.note_off(n); }
void HybridPreview::set_sustain(bool down) { bus_.set_sustain(down); }
void HybridPreview::set_pitch_bend(int value) { for (auto& half : halves_) half.pitch_bend(value); }
void HybridPreview::set_expression(float wheel, float pressure, float bpm, bool sync) {
    for (auto& half : halves_) half.expression(wheel, pressure, bpm, sync);
}
void HybridPreview::set_layer_controls(float morph, uint8_t delay, int8_t transpose, float gain_a_db, float gain_b_db) {
    router_.configure(morph, gain_a_db, gain_b_db, delay, transpose);
}
void HybridPreview::set_half_controls(unsigned side, float cutoff, float resonance, float attack, float release) {
    if (side < halves_.size() && loaded_) halves_[side].controls(cutoff, resonance, attack, release);
}
void HybridPreview::update_unison_gain() {
    constexpr uint8_t max_voices[] = {1, 6, 5, 6, 6, 4};
    const auto& patch = layer_.a.patch;
    const auto index = uint8_t(std::clamp(int(patch.groove[GF_GROOVE_UNISON][GF_UNISON_VOICES]), 0, 3));
    const uint8_t requested = patch.groove_on[GF_GROOVE_UNISON] ? GF_UNISON_COUNT[index] : 1;
    const uint8_t count = layer_.eng_a == 5 && patch.mod[0][0] > .5f ? 1 : std::min(requested, max_voices[layer_.eng_a]);
    b_unison_gain_ = dsp::unison_gain(count);
}
void HybridPreview::apply_bus_fx() {
    auto a = layer_.a;
    constexpr int slots[] = {GF_MACRO_DRIFT, GF_MACRO_GRIT, GF_MACRO_POCKET, GF_MACRO_MOVE, GF_MACRO_GRIT_RES, GF_MACRO_GRIT_MOD, GF_MACRO_POCKET_VEL};
    for (size_t i = 0; i < last_macros_.size(); ++i) if (std::isfinite(last_macros_[i]))
        a.patch.groove[GF_GROOVE_MACRO][slots[i]] = std::clamp(last_macros_[i], 0.0f, 1.0f) * 100;
    a.patch = studio::move_sfx(a.patch, studio::macro_amount(a.patch, GF_MACRO_MOVE));
    apply_synth_fx_preview(bus_, a, layer_.eng_a);
}
void HybridPreview::set_bus(const std::array<float, 21>& groove, const std::array<float, 25>& effects) {
    if (!loaded_) return;
    if (groove != last_groove_) {
        last_groove_ = groove;
        constexpr int fields[] = {7, 7, 5};
        for (int g = 0; g < 3; ++g) {
            layer_.a.patch.groove_on[g] = groove[size_t(g * 7)] >= .5f;
            for (int f = 1; f < fields[g]; ++f) layer_.a.patch.groove[g][f - 1] = groove[size_t(g * 7 + f)];
        }
        apply_groove_preview(bus_, layer_.a, false);
    }
    if (effects != last_effects_) {
        last_effects_ = effects;
        for (int fx = 0; fx < 5; ++fx) {
            layer_.a.patch.sfx_on[fx] = effects[size_t(fx * 5)] >= .5f;
            layer_.a.patch.sfx_mix[fx] = effects[size_t(fx * 5 + 1)];
            for (int f = 0; f < 3; ++f) layer_.a.patch.sfx_p[fx][f] = effects[size_t(fx * 5 + f + 2)];
        }
        apply_bus_fx();
    }
}
void HybridPreview::set_half_osc(unsigned side, const std::array<float, 48>& values) {
    if (!loaded_ || side >= 2 || values == last_osc_[side]) return;
    last_osc_[side] = values;
    auto& patch = side == 0 ? layer_.a.patch : layer_.b.patch;
    const uint8_t engine = side == 0 ? layer_.eng_a : layer_.eng_b;
    for (int page = 0; page < 6; ++page) for (int slot = 0; slot < 8; ++slot) {
        const bool mod = page >= 4; const int stop = mod ? page - 4 : page;
        const auto desc = mod ? GF_MOD_SLOT[engine][stop][slot] : GF_OSC_SLOT[engine][stop][slot];
        if (!gf_osc_used(&desc) || (desc.global && stop != 0)) continue;
        float value = desc.lo + std::clamp(values[size_t(page * 8 + slot)], 0.0f, 1.0f) * (desc.hi - desc.lo);
        if (desc.count) value = std::round(value);
        (mod ? patch.mod[stop][slot] : patch.osc[stop][slot]) = value;
    }
    if (side == 0) update_unison_gain();
    if (std::isfinite(last_macros_[0])) refresh_half(side);
}
void HybridPreview::set_half_voice(unsigned side, const std::array<float, 20>& v) {
    if (!loaded_ || side >= 2 || v == last_voice_[side]) return;
    last_voice_[side] = v;
    auto& p = side == 0 ? layer_.a.patch : layer_.b.patch;
    p.decay_ms = v[0]; p.sustain = v[1]; p.f_attack_ms = v[2]; p.f_decay_ms = v[3]; p.f_sustain = v[4];
    p.f_release_ms = v[5]; p.env_amt = v[6]; p.drive = v[7]; p.vel_amp = v[8]; p.vel_cut = v[9];
    p.lfo_rate_hz = v[10]; p.lfo_shape = uint8_t(v[11]); p.lfo_div = uint8_t(v[12]);
    p.lfo_amt[GF_DEST_CUTOFF] = v[13]; p.lfo_amt[GF_DEST_PITCH] = v[14];
    p.lfo_amt[(side == 0 ? layer_.eng_a : layer_.eng_b) == 4 ? GF_DEST_AMP : GF_DEST_PWM] = v[15];
    p.glide = v[16]; p.groove_on[GF_GROOVE_UNISON] = v[17] >= .5f;
    p.groove[GF_GROOVE_UNISON][GF_UNISON_VOICES] = v[18]; p.groove[GF_GROOVE_UNISON][GF_UNISON_DETUNE] = v[19];
    if (side == 0) update_unison_gain();
    if (std::isfinite(last_macros_[0])) refresh_half(side);
}
void HybridPreview::refresh_half(unsigned side) {
    auto preset = side == 0 ? layer_.a : layer_.b;
    constexpr int slots[] = {GF_MACRO_DRIFT, GF_MACRO_GRIT, GF_MACRO_POCKET, GF_MACRO_MOVE,
                             GF_MACRO_GRIT_RES, GF_MACRO_GRIT_MOD, GF_MACRO_POCKET_VEL};
    if (side == 0) {
        for (size_t i = 0; i < last_macros_.size(); ++i)
            preset.patch.groove[GF_GROOVE_MACRO][slots[i]] = std::clamp(last_macros_[i], 0.0f, 1.0f) * 100;
    } else {
        preset.patch.groove[GF_GROOVE_MACRO][GF_MACRO_DRIFT] = std::clamp(last_macros_[0], 0.0f, 1.0f) * 100;
        preset.patch.groove_on[GF_GROOVE_UNISON] = false;
    }
    halves_[side].update_patch(preset);
}
void HybridPreview::set_macros(const std::array<float, 7>& values) {
    if (!loaded_ || values == last_macros_) return;
    last_macros_ = values;
    refresh_half(0); refresh_half(1); apply_bus_fx();
}
void HybridPreview::set_b_move(float value) {
    if (!loaded_) return;
    value = std::clamp(value, 0.0f, 1.0f);
    if (value == last_b_move_) return;
    last_b_move_ = value;
    layer_.b.patch.groove[GF_GROOVE_MACRO][GF_MACRO_MOVE] = value * 100;
    if (std::isfinite(last_macros_[0])) refresh_half(1);
}
void HybridPreview::set_morph(float value) { router_.set_morph(value); }
void HybridPreview::set_transport(float bpm, bool has_position, double beats, bool running) { bus_.set_transport(bpm, has_position, beats, running); }
void HybridPreview::process_frame(float& left, float& right) {
    if (!loaded_) { left = right = 0; return; }
    router_.tick();
    float al = 0, ar = 0, bl = 0, br = 0, l = 0, r = 0;
    const float tape = bus_.tape_pitch_ratio();
    halves_[0].process(al, ar, tape); halves_[1].process(bl, br, tape);
    router_.mix(al, ar, bl * b_unison_gain_, br * b_unison_gain_, l, r);
    bus_.process_external_stereo(l, r, left, right);
}
}
