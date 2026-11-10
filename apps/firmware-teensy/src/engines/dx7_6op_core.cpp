// Curve helpers and interpolation equations adapted from MSFA, Copyright 2012 Google Inc.
// Apache-2.0; see msfa/LICENSE and msfa/NOTICE.md.
#include "dx7_6op_core.h"
#include "msfa/note_tables.h"
#include "msfa/exp2.h"
#include "msfa/sin.h"
#include <algorithm>
#include <cmath>

namespace gf::dx7six {
namespace {
void init_tables() {
    static const bool ready = [] { msfa::Sin::init(); msfa::Exp2::init(); return true; }();
    (void)ready;
}
constexpr int32_t Q24 = 1 << 24;
// DX7 AMS attenuation spans approximately 0, 13.5, 23.5, 47 dB at AMD 99.
constexpr int32_t AMS_Q24[] = {0, 37622401, 65490847, 130981694};
}
Patch init_patch() {
    Patch p;
    for (int op = 0; op < 6; ++op) {
        auto* o = p.vced.data() + op * 21;
        o[0] = 99; o[1] = 75; o[2] = 65; o[3] = 60;
        o[4] = 99; o[5] = 85; o[6] = 75; o[7] = 0;
        o[8] = 39; o[16] = op == 5 ? 99 : 0; o[18] = 1; o[20] = 7;
    }
    for (int i = 126; i < 130; ++i) p.vced[i] = 99;
    for (int i = 130; i < 134; ++i) p.vced[i] = 50;
    p.vced[134] = 31; p.vced[136] = 1; p.vced[137] = 35;
    p.vced[141] = 1; p.vced[142] = 4; p.vced[144] = 24;
    const char name[] = "GF INIT   ";
    std::copy_n(name, 10, p.vced.begin() + 145);
    return p;
}
bool valid_patch(const Patch& p) {
    if (p.enabled > 63) return false;
    constexpr uint8_t maximum[] = {99,99,99,99,99,99,99,99,99,99,99,3,3,7,3,7,99,1,31,99,14};
    for (int op = 0; op < 6; ++op) for (int i = 0; i < 21; ++i)
        if (p.vced[op * 21 + i] > maximum[i]) return false;
    constexpr uint8_t global_max[] = {99,99,99,99,99,99,99,99,31,7,1,99,99,99,99,1,5,7,48};
    for (int i = 0; i < 19; ++i) if (p.vced[126 + i] > global_max[i]) return false;
    for (int i = 145; i < 155; ++i) if (p.vced[i] > 127) return false;
    return true;
}
int scale_level(int note, int bp, int ld, int rd, int lc, int rc) {
    return msfa::ScaleLevel(note, bp, ld, rd, lc, rc);
}
void Voice::prepare(double fs) {
    for (auto& e : env_) e.prepare(fs);
    pitch_.init(fs);
}
void Voice::note_on(const Patch& p, int note, int velocity) {
    note_ = note; velocity_ = velocity;
    active_ = true; released_ = false;
    apply_patch(p, true);
    for (int op = 0; op < 6; ++op) {
        if (p.vced[136]) phase_[op] = 0;
        gain_[op] = 0;
    }
    fb_[0] = fb_[1] = 0;
}
void Voice::edit_patch(const Patch& p) {
    if (active_) apply_patch(p, false);
}
void Voice::apply_patch(const Patch& p, bool retrigger) {
    const auto* v = p.vced.data();
    const int note = note_ + int(v[144]) - 24;
    for (int op = 0; op < 6; ++op) {
        const auto* o = v + op * 21;
        int rates[4], levels[4];
        for (int i = 0; i < 4; ++i) { rates[i] = o[i]; levels[i] = o[4+i]; }
        int level = std::min(127, msfa::Env::scaleoutlevel(o[16]) +
            msfa::ScaleLevel(note,o[8],o[9],o[10],o[11],o[12]));
        level = std::max(0, level * 32 + msfa::ScaleVelocity(velocity_, o[15]));
        if (retrigger) env_[op].init(rates,levels,level,msfa::ScaleRate(note,o[13]));
        else env_[op].update(rates,levels,level,msfa::ScaleRate(note,o[13]));
        base_[op] = msfa::osc_freq(note,o[17],o[18],o[19],o[20]);
    }
    int rates[4], levels[4];
    for (int i = 0; i < 4; ++i) { rates[i] = v[126+i]; levels[i] = v[130+i]; }
    if (retrigger) pitch_.set(rates,levels);
    else pitch_.update(rates,levels);
}
void Voice::note_off() {
    for (auto& e : env_) e.keydown(false);
    pitch_.keydown(false); released_ = true;
}
void Voice::compute(const Patch& p, msfa::Freqlut& freq, int32_t lfo, int32_t delay, int32_t bend, int32_t* out) {
    if (!active_) return;
    const auto* v = p.vced.data();
    int32_t pitchmod = pitch_.getsample() + bend;
    const uint32_t pmd = ((v[139] * 165) >> 6) * uint32_t(delay);
    const int32_t senslfo = msfa::pitchmodsenstab[v[143]] * (lfo - (1 << 23));
    pitchmod += (int64_t(pmd) * senslfo) >> 39;
    int32_t gain[6], delta[6], increment[6];
    bool audible[6], add[6], input[6];
    bool contents[3] = {true, false, false};
    const auto& alg = algorithms[v[134]];
    bool alive = !released_;
    for (int op = 0; op < 6; ++op) {
        int32_t level = env_[op].getsample();
        const int ams = v[op * 21 + 14];
        const int64_t am = (int64_t(lfo) * delay) >> 24;
        level -= int32_t((((am * v[140]) / 99) * AMS_Q24[ams]) >> 24);
        const bool enabled = p.enabled & (1 << (5 - op));
        const int32_t next = enabled ? msfa::Exp2::lookup(level - 14 * Q24) : 0;
        gain[op] = gain_[op]; delta[op] = (next - gain[op] + 32) >> 6;
        gain_[op] = next;
        increment[op] = freq.lookup(base_[op] + pitchmod);
        const int flags = alg.ops[op], in = (flags >> 4) & 3, to = flags & 3;
        audible[op] = gain[op] >= 1120 || next >= 1120;
        input[op] = in && contents[in];
        add[op] = (flags & 4) && contents[to];
        if (audible[op]) contents[to] = true;
        else if (!(flags & 4)) contents[to] = false;
        if (to == 0 && enabled && (!released_ || env_[op].stage() < 4 || next >= 1120)) alive = true;
    }
    const int fbshift = v[135] ? 8 - v[135] : 16;
    for (int i = 0; i < BLOCK; ++i) {
        int32_t bus[3] = {0, 0, 0};
        for (int op = 0; op < 6; ++op) {
            const int flags = alg.ops[op], in = (flags >> 4) & 3, to = flags & 3;
            gain[op] += delta[op];
            if (audible[op]) {
                uint32_t phase = phase_[op] + (input[op] ? uint32_t(bus[in]) : 0u);
                // Flags delimit the actual loop; algorithms 4/6 return the last operator, not the first.
                if ((flags & 0x40) && fbshift < 16)
                    phase += uint32_t((int64_t(fb_[0]) + fb_[1]) >> (fbshift + 1));
                const int32_t y = int32_t((int64_t(msfa::Sin::lookup(int32_t(phase))) * gain[op]) >> 24);
                bus[to] = add[op] ? bus[to] + y : y;
                if (flags & 0x80) { fb_[0] = fb_[1]; fb_[1] = y; }
            } else if (flags & 0x80) { fb_[0] = fb_[1]; fb_[1] = 0; }
            phase_[op] += uint32_t(increment[op]);
        }
        out[i] = int32_t(std::clamp(int64_t(out[i]) + bus[0], int64_t(INT32_MIN), int64_t(INT32_MAX)));
    }
    active_ = alive;
}
void Synth::prepare(double fs) {
    fs_ = std::isfinite(fs) && fs >= 8000 && fs <= 192000 ? fs : 44100;
    init_tables(); freq_.init(fs_); reset();
}
void Synth::reset() {
    voices_ = {}; buffer_.fill(0); position_ = BLOCK; age_ = 0; bend_ = 0; sustain_ = false;
    for (auto& v : voices_) v.voice.prepare(fs_);
    lfo_ = {}; lfo_.init(fs_); lfo_.reset(reinterpret_cast<const char*>(patch_.vced.data() + 137));
}
bool Synth::set_patch(const Patch& p) {
    if (!valid_patch(p)) return false;
    patch_ = p; reset(); return true;
}
bool Synth::edit_patch(const Patch& p) {
    if (!valid_patch(p)) return false;
    patch_ = p;
    for (auto& s : voices_) s.voice.edit_patch(p);
    lfo_.reset(reinterpret_cast<const char*>(patch_.vced.data() + 137));
    return true;
}
void Synth::note_on(uint8_t note, uint8_t velocity) {
    if (note > 127) return;
    if (!velocity) { note_off(note); return; }
    Slot* target = nullptr;
    for (auto& s : voices_) if (!s.voice.active()) { target = &s; break; }
    if (!target) for (auto& s : voices_) if (s.note < 0 && (!target || s.age < target->age)) target = &s;
    if (!target) target = &*std::min_element(voices_.begin(), voices_.end(), [](const Slot& a, const Slot& b) { return a.age < b.age; });
    target->note = note; target->age = ++age_; target->deferred = false;
    target->voice.note_on(patch_, note, std::min<int>(127, velocity)); lfo_.keydown();
}
void Synth::note_off(uint8_t note) {
    for (auto& s : voices_) if (s.note == note) {
        if (sustain_) s.deferred = true;
        else { s.voice.note_off(); s.note = -1; s.deferred = false; }
    }
}
void Synth::set_sustain(bool down) {
    sustain_ = down;
    if (!down) for (auto& s : voices_) if (s.deferred) { s.voice.note_off(); s.note = -1; s.deferred = false; }
}
void Synth::set_pitch_bend(int value) { bend_ = (std::clamp(value, 0, 16383) - 8192) * 512; }
float Synth::process_sample() {
    if (position_ == BLOCK) {
        buffer_.fill(0);
        const int32_t value = lfo_.getsample(), delay = lfo_.getdelay();
        for (auto& s : voices_) s.voice.compute(patch_, freq_, value, delay, bend_, buffer_.data());
        position_ = 0;
    }
    // 18 dB voice headroom keeps six carriers and normal six-note chords below full scale.
    return buffer_[position_++] * (0.125f / Q24);
}
void Synth::render(float* out, size_t count) { for (size_t i = 0; i < count; ++i) out[i] = process_sample(); }
}
