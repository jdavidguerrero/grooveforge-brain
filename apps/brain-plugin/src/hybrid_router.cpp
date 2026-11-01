#include "hybrid_router.h"
#include <algorithm>
#include <cmath>
namespace gf {
void HybridRouter::prepare(double sample_rate) {
    reset(); fs_ = std::isfinite(sample_rate) && sample_rate >= 1000 ? sample_rate : 44100.0; sample_ = 0;
}
void HybridRouter::set_callbacks(void* context, NoteOn on, NoteOff off) { context_ = context; on_ = on; off_ = off; }
void HybridRouter::configure(float morph, float ga, float gb, uint8_t delay, int8_t transpose) {
    gain_a_db_ = std::isfinite(ga) ? std::clamp(ga, -96.0f, 24.0f) : 0;
    gain_b_db_ = std::isfinite(gb) ? std::clamp(gb, -96.0f, 24.0f) : 0;
    delay_ms_ = delay; transpose_ = transpose; set_morph(morph);
}
void HybridRouter::set_morph(float morph) {
    studio::LayerDef def{"", 0, "", 1, "", gain_a_db_, gain_b_db_, 0, 0};
    studio::layer_gains(def, std::isfinite(morph) ? morph : .5f, &gain_a_, &gain_b_);
}
void HybridRouter::start_b(Key& key) {
    key.started = true;
    if (b_refs_[key.b_note]++ == 0 && on_) on_(context_, 1, key.b_note, key.velocity);
}
void HybridRouter::update_next_due() {
    next_due_ = UINT64_MAX;
    for (const auto& key : keys_) if (key.active && !key.started) next_due_ = std::min(next_due_, key.due);
}
void HybridRouter::note_on(uint8_t note, float velocity) {
    if (note > 127 || !std::isfinite(velocity)) return;
    if (keys_[note].active) note_off(note);
    auto& key = keys_[note]; key.active = true; key.started = false;
    key.b_note = studio::layer_b_note(note, transpose_); key.velocity = std::clamp(velocity, 0.0f, 1.0f);
    key.due = sample_ + uint64_t(std::llround(fs_ * delay_ms_ * .001));
    if (on_) on_(context_, 0, note, key.velocity);
    if (key.due <= sample_) start_b(key);
    else next_due_ = std::min(next_due_, key.due);
}
void HybridRouter::note_off(uint8_t note) {
    if (note > 127) return;
    auto& key = keys_[note];
    if (!key.active) return;
    if (off_) off_(context_, 0, note);
    if (key.started && b_refs_[key.b_note] && --b_refs_[key.b_note] == 0 && off_) off_(context_, 1, key.b_note);
    const bool was_next = !key.started && key.due == next_due_;
    key = Key{};
    if (was_next) update_next_due();
}
void HybridRouter::tick() {
    if (sample_ >= next_due_) {
        for (auto& key : keys_) if (key.active && !key.started && key.due <= sample_) start_b(key);
        update_next_due();
    }
    ++sample_;
}
void HybridRouter::mix(float al, float ar, float bl, float br, float& left, float& right) const {
    left = gain_a_ * al + gain_b_ * bl; right = gain_a_ * ar + gain_b_ * br;
}
void HybridRouter::reset() {
    for (size_t i = 0; i < keys_.size(); ++i) if (keys_[i].active) note_off(uint8_t(i));
    keys_.fill(Key{}); b_refs_.fill(0); sample_ = 0; next_due_ = UINT64_MAX;
}
}
