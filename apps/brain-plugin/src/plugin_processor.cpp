#include <juce_audio_utils/juce_audio_utils.h>
#include <array>
#include <limits>
#include <mutex>

#include "prophet_preview.h"
#include "prophet_preset.h"
#include "preset_import.h"
#include "dx7_preview.h"
#include "juno_preview.h"
#include "ob6_preview.h"
#include "ob6_preset.h"
#include "juno_preset.h"
#include "dx7_preset.h"
#include "ui/groove_macros.h"

namespace {

static_assert(std::atomic<uint8_t>::is_always_lock_free && std::atomic<uint64_t>::is_always_lock_free && std::atomic<float>::is_always_lock_free &&
              std::atomic<size_t>::is_always_lock_free, "Preset mailbox requires lock-free atomics");

constexpr std::array<const char*, 8> DX_FIELDS{"ratio", "level", "attack", "decay", "sustain", "release", "wave", "detune"};
juce::String dx_id(int op, int field) { return "dx7.op" + juce::String(op + 1) + "." + DX_FIELDS[size_t(field)]; }
constexpr std::array<const char*, 5> DX_FX_NAMES{"drive", "tape", "chorus", "delay", "reverb"};
constexpr std::array<const char*, 5> DX_FX_FIELDS{"on", "mix", "p1", "p2", "p3"};
constexpr const char* DX_FX_LABELS[5][5] = {
    {"On", "Mix", "Amount", "Tone", "Type"}, {"On", "Mix", "Drive", "Wow", "Flutter"},
    {"On", "Mix", "Depth", "Rate", "Type"}, {"On", "Mix", "Time", "Feedback", "Tone"},
    {"On", "Mix", "Decay", "Type", "Size"}};
juce::String dx_fx_id(int fx, int field) { return "dx7.fx." + juce::String(DX_FX_NAMES[size_t(fx)]) + "." + DX_FX_FIELDS[size_t(field)]; }
juce::String juno_fx_id(int fx, int field) { return "juno.fx." + juce::String(DX_FX_NAMES[size_t(fx)]) + "." + DX_FX_FIELDS[size_t(field)]; }
constexpr std::array<const char*, 3> DX_GROOVE_NAMES{"pump", "arp", "chord"};
constexpr std::array<int, 3> DX_GROOVE_FIELDS{6, 7, 5};
constexpr std::array<int, 3> JUNO_GROOVE_FIELDS{7, 7, 5};
juce::String dx_groove_id(int group, int field) {
    return "dx7.groove." + juce::String(DX_GROOVE_NAMES[size_t(group)]) + "." + (field == 0 ? juce::String("on") : "p" + juce::String(field));
}
juce::String juno_groove_id(int group, int field) {
    return "juno.groove." + juce::String(DX_GROOVE_NAMES[size_t(group)]) + "." + (field == 0 ? juce::String("on") : "p" + juce::String(field));
}
constexpr std::array<const char*, 6> DX_MACRO_IDS{"dx7.macro.grit", "dx7.macro.pocket", "dx7.macro.move",
                                                  "dx7.macro.grit_res", "dx7.macro.grit_mod", "dx7.macro.pocket_vel"};
constexpr std::array<const char*, 6> DX_MACRO_LABELS{"GRIT", "POCKET", "MOVE", "GRIT RES", "GRIT MOD", "POCKET VEL"};
constexpr std::array<const char*, 6> JUNO_MACRO_IDS{"juno.macro.grit", "juno.macro.pocket", "juno.macro.move",
    "juno.macro.grit_res", "juno.macro.grit_mod", "juno.macro.pocket_vel"};

struct DxGeneral { const char* id; const char* label; float lo, hi, step, initial; };
constexpr std::array<DxGeneral, 9> JUNO_OSC{{
    {"juno.wave", "WAVE 0-3", 0,3,1,3}, {"juno.sub", "SUB %", 0,100,0,30},
    {"juno.noise", "NOISE %", 0,100,0,0}, {"juno.pw", "PULSE WIDTH %", 5,95,0,50},
    {"juno.chorus", "CHORUS 0-3", 0,3,1,1}, {"juno.hpf", "HPF 0-3", 0,3,1,1},
    {"juno.range", "RANGE 0-2", 0,2,1,1}, {"juno.gate", "VCA 0-1", 0,1,1,0},
    {"juno.hiss", "HISS %", 0,100,0,0}
}};

constexpr std::array<DxGeneral, 10> JUNO_ENV{{
    {"juno.amp_decay", "AMP DECAY ms", 1.5f,12000,0,200}, {"juno.amp_sustain", "AMP SUSTAIN", 0,1,0,0.5f},
    {"juno.filter_attack", "FLT ATTACK ms", 0,12000,0,10}, {"juno.filter_decay", "FLT DECAY ms", 0,12000,0,200},
    {"juno.filter_sustain", "FLT SUSTAIN", 0,1,0,0.5f}, {"juno.filter_release", "FLT RELEASE ms", 0,12000,0,300},
    {"juno.env_amount", "FLT ENV AMT", 0,1,0,0.5f}, {"juno.drive", "DRIVE", 0,1,0,0.17142857f},
    {"juno.vel_amp", "VEL AMP %", 0,100,0,65}, {"juno.vel_filter", "VEL FILTER %", 0,100,0,50}
}};
constexpr std::array<DxGeneral, 7> JUNO_LFO{{
    {"juno.lfo_rate", "RATE Hz", .05f,20,0,2}, {"juno.lfo_shape", "SHAPE 0-4", 0,4,1,1},
    {"juno.lfo_div", "DIV 0-3", 0,3,1,1}, {"juno.lfo_cutoff", "CUTOFF DEPTH", 0,1,0,0},
    {"juno.lfo_pitch", "PITCH DEPTH", 0,1,0,0}, {"juno.lfo_pwm", "PWM DEPTH", 0,1,0,0},
    {"juno.lfo_delay", "DELAY ms", 0,3000,0,0}
}};
constexpr std::array<DxGeneral, 5> JUNO_VOICE_GROOVE{{
    {"juno.glide", "GLIDE ms", 0,1500,0,0}, {"juno.unison_on", "UNISON ON", 0,1,1,0},
    {"juno.unison_count", "VOICES 0-3", 0,3,1,0}, {"juno.unison_detune", "DETUNE cents", 0,50,0,0},
    {"juno.drift", "DRIFT %", 0,100,0,0}
}};
constexpr std::array<DxGeneral, 18> DX_GENERAL{{
    {"dx7.cutoff", "CUTOFF", 500,20000,0,20000}, {"dx7.resonance", "RESONANCE", 0.7f,3,0,0.7f},
    {"dx7.drive", "DRIVE", 0.5f,4,0,1}, {"dx7.amp_attack", "AMP ATTACK", 0,5000,0,2},
    {"dx7.amp_decay", "AMP DECAY", 0,5000,0,50}, {"dx7.amp_sustain", "AMP SUSTAIN", 0,1,0,1},
    {"dx7.amp_release", "AMP RELEASE", 0,10000,0,600}, {"dx7.velocity", "VELOCITY", 0,1,0,1},
    {"dx7.glide", "GLIDE MS", 0,1500,0,0}, {"dx7.lfo_rate", "LFO RATE", 0.05f,20,0,2},
    {"dx7.lfo_shape", "LFO SHAPE", 0,4,1,1}, {"dx7.lfo_pitch", "LFO PITCH", 0,1,0,0},
    {"dx7.lfo_amp", "LFO AMP", 0,1,0,0}, {"dx7.lfo_div", "LFO DIV", 0,3,1,1},
    {"dx7.unison_on", "UNISON ON", 0,1,1,0}, {"dx7.unison_count", "UNISON COUNT", 0,3,1,0},
    {"dx7.unison_detune", "DETUNE", 0,50,0,0}, {"dx7.drift", "DRIFT", 0,1,0,0}
}};

juce::AudioProcessorValueTreeState::ParameterLayout make_layout() {
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> parameters;
    parameters.push_back(std::make_unique<AudioParameterFloat>(ParameterID{"cutoff", 1}, "Cutoff",
        NormalisableRange<float>{20.0f, 20000.0f, 0.0f, 0.3f}, 2000.0f));
    parameters.push_back(std::make_unique<AudioParameterFloat>(ParameterID{"resonance", 1}, "Resonance",
        NormalisableRange<float>{0.707f, 5.0f}, 0.707f));
    parameters.push_back(std::make_unique<AudioParameterFloat>(ParameterID{"attack", 1}, "Attack ms",
        NormalisableRange<float>{0.0f, 5000.0f, 0.0f, 0.35f}, 10.0f));
    parameters.push_back(std::make_unique<AudioParameterFloat>(ParameterID{"release", 1}, "Release ms",
        NormalisableRange<float>{1.0f, 10000.0f, 0.0f, 0.35f}, 500.0f));
    parameters.push_back(std::make_unique<AudioParameterBool>(ParameterID{"lfo_sync", 1}, "LFO Sync", false));
    parameters.push_back(std::make_unique<AudioParameterBool>(ParameterID{"dx7_preview", 1}, "DX7 Preview", false));
    parameters.push_back(std::make_unique<AudioParameterInt>(ParameterID{"dx7.algorithm", 1}, "DX7 Algorithm", 1, 8, 5));
    parameters.push_back(std::make_unique<AudioParameterInt>(ParameterID{"dx7.feedback", 1}, "DX7 Feedback", 0, 7, 2));
    parameters.push_back(std::make_unique<AudioParameterFloat>(ParameterID{"dx7.kbd_level", 1}, "DX7 Keyboard Level", NormalisableRange<float>{0, 99}, 0));
    parameters.push_back(std::make_unique<AudioParameterInt>(ParameterID{"dx7.kbd_rate", 1}, "DX7 Keyboard Rate", 0, 3, 0));
    parameters.push_back(std::make_unique<AudioParameterFloat>(ParameterID{"dx7.d2", 1}, "DX7 Decay 2", NormalisableRange<float>{0, 5000}, 0));
    constexpr float defaults[4][8] = {{1,99,0,1800,0,400,1,0}, {1,72,0,900,15,300,1,0},
                                      {1,88,0,2200,0,400,1,0}, {14,52,0,160,0,120,1,0}};
    constexpr float lows[8] = {0.5f,0,0,0,0,0,1,-3};
    constexpr float highs[8] = {16,99,3000,5000,100,5000,8,3};
    constexpr float steps[8] = {0,0,0,0,0,0,1,1};
    for (int op = 0; op < 4; ++op) for (int field = 0; field < 8; ++field) {
        const auto id = dx_id(op, field);
        parameters.push_back(std::make_unique<AudioParameterFloat>(ParameterID{id, 1},
            "DX7 OP" + String(op + 1) + " " + DX_FIELDS[size_t(field)],
            NormalisableRange<float>{lows[field], highs[field], steps[field]}, defaults[op][field]));
    }
    for (const auto& parameter : DX_GENERAL)
        parameters.push_back(std::make_unique<AudioParameterFloat>(ParameterID{parameter.id, 1},
            "DX7 " + String(parameter.label), NormalisableRange<float>{parameter.lo, parameter.hi, parameter.step}, parameter.initial));
    for (int fx = 0; fx < 5; ++fx) for (int field = 0; field < 5; ++field) {
        const auto id = dx_fx_id(fx, field);
        parameters.push_back(std::make_unique<AudioParameterFloat>(ParameterID{id, 1},
            "DX7 FX " + String(DX_FX_NAMES[size_t(fx)]) + " " + DX_FX_LABELS[fx][field],
            NormalisableRange<float>{0, 1, field == 0 ? 1.0f : 0.0f}, 0.0f));
    }
    const studio::Patch default_patch;
    for (int group = 0; group < 3; ++group) for (int field = 0; field < DX_GROOVE_FIELDS[size_t(group)]; ++field) {
        const auto slot = field == 0 ? GF_OscSlot{0, 1, 1, 2, false} : GF_GROOVE_SLOT[group][field - 1];
        const float initial = field == 0 ? 0.0f : default_patch.groove[group][field - 1];
        parameters.push_back(std::make_unique<AudioParameterFloat>(ParameterID{dx_groove_id(group, field), 1},
            "DX7 GROOVE " + String(DX_GROOVE_NAMES[size_t(group)]) + " " + String(field),
            NormalisableRange<float>{slot.lo, slot.hi, slot.count ? 1.0f : 0.0f}, initial));
    }
    for (size_t i = 0; i < DX_MACRO_IDS.size(); ++i)
        parameters.push_back(std::make_unique<AudioParameterFloat>(ParameterID{DX_MACRO_IDS[i], 1},
            "DX7 " + String(DX_MACRO_LABELS[i]), NormalisableRange<float>{0, 1}, i < 3 ? 0.0f : 1.0f));
    parameters.push_back(std::make_unique<AudioParameterBool>(ParameterID{"juno_preview", 1}, "Juno Preview", false));
    for (const auto& p : JUNO_OSC)
        parameters.push_back(std::make_unique<AudioParameterFloat>(ParameterID{p.id, 1}, "Juno " + String(p.label),
            NormalisableRange<float>{p.lo, p.hi, p.step}, p.initial));
    for (const auto& p : JUNO_ENV)
        parameters.push_back(std::make_unique<AudioParameterFloat>(ParameterID{p.id, 1}, "Juno " + String(p.label),
            NormalisableRange<float>{p.lo, p.hi, p.step}, p.initial));
    for (const auto& p : JUNO_LFO)
        parameters.push_back(std::make_unique<AudioParameterFloat>(ParameterID{p.id, 1}, "Juno " + String(p.label),
            NormalisableRange<float>{p.lo, p.hi, p.step}, p.initial));
    for (const auto& p : JUNO_VOICE_GROOVE)
        parameters.push_back(std::make_unique<AudioParameterFloat>(ParameterID{p.id, 1}, "Juno " + String(p.label),
            NormalisableRange<float>{p.lo, p.hi, p.step}, p.initial));
    for (int group = 0; group < 3; ++group) for (int field = 0; field < JUNO_GROOVE_FIELDS[size_t(group)]; ++field) {
        const auto slot = field == 0 ? GF_OscSlot{0, 1, 1, 2, false} : GF_GROOVE_SLOT[group][field - 1];
        const float initial = field == 0 ? 0.0f : default_patch.groove[group][field - 1];
        parameters.push_back(std::make_unique<AudioParameterFloat>(ParameterID{juno_groove_id(group, field), 1},
            "Juno GROOVE " + String(DX_GROOVE_NAMES[size_t(group)]) + " " + String(field),
            NormalisableRange<float>{slot.lo, slot.hi, slot.count ? 1.0f : 0.0f}, initial));
    }
    for (int fx = 0; fx < 5; ++fx) for (int field = 0; field < 5; ++field) {
        const float initial = field == 0 ? 0.0f : field == 1 ? default_patch.sfx_mix[fx] : default_patch.sfx_p[fx][field - 2];
        parameters.push_back(std::make_unique<AudioParameterFloat>(ParameterID{juno_fx_id(fx, field), 1},
            "Juno FX " + String(DX_FX_NAMES[size_t(fx)]) + " " + DX_FX_LABELS[fx][field],
            NormalisableRange<float>{0, 1, field == 0 ? 1.0f : 0.0f}, initial));
    }
    for (size_t i = 0; i < JUNO_MACRO_IDS.size(); ++i)
        parameters.push_back(std::make_unique<AudioParameterFloat>(ParameterID{JUNO_MACRO_IDS[i], 1},
            "Juno " + String(DX_MACRO_LABELS[i]), NormalisableRange<float>{0, 1}, i < 3 ? 0.0f : 1.0f));
    parameters.push_back(std::make_unique<AudioParameterBool>(ParameterID{"ob6_preview", 1}, "OB-6 Preview", false));
    return {parameters.begin(), parameters.end()};
}

class BrainPrototype final : public juce::AudioProcessor, public gf::PresetImport, private juce::Timer {
public:
    BrainPrototype() : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
        state_(*this, nullptr, "BrainPrototype", make_layout()) {
        cutoff_ = state_.getRawParameterValue("cutoff");
        resonance_ = state_.getRawParameterValue("resonance");
        attack_ = state_.getRawParameterValue("attack");
        release_ = state_.getRawParameterValue("release");
        lfo_sync_ = state_.getRawParameterValue("lfo_sync");
        dx7_enabled_ = state_.getRawParameterValue("dx7_preview");
        juno_enabled_ = state_.getRawParameterValue("juno_preview");
        ob6_enabled_ = state_.getRawParameterValue("ob6_preview");
        for (size_t i = 0; i < JUNO_OSC.size(); ++i) juno_osc_[i] = state_.getRawParameterValue(JUNO_OSC[i].id);
        juno_osc_last_.fill(std::numeric_limits<float>::quiet_NaN());
        for (size_t i = 0; i < JUNO_ENV.size(); ++i) juno_env_[i] = state_.getRawParameterValue(JUNO_ENV[i].id);
        juno_env_last_.fill(std::numeric_limits<float>::quiet_NaN());
        for (size_t i = 0; i < JUNO_LFO.size(); ++i) juno_lfo_[i] = state_.getRawParameterValue(JUNO_LFO[i].id);
        juno_lfo_last_.fill(std::numeric_limits<float>::quiet_NaN());
        for (size_t i = 0; i < JUNO_VOICE_GROOVE.size(); ++i) juno_voice_groove_[i] = state_.getRawParameterValue(JUNO_VOICE_GROOVE[i].id);
        juno_voice_groove_last_.fill(std::numeric_limits<float>::quiet_NaN());
        for (int group = 0; group < 3; ++group) for (int field = 0; field < JUNO_GROOVE_FIELDS[size_t(group)]; ++field)
            juno_groove_[group][field] = state_.getRawParameterValue(juno_groove_id(group, field));
        juno_groove_last_.fill(std::numeric_limits<float>::quiet_NaN());
        for (int fx = 0; fx < 5; ++fx) for (int field = 0; field < 5; ++field)
            juno_fx_[fx][field] = state_.getRawParameterValue(juno_fx_id(fx, field));
        juno_fx_last_.fill(std::numeric_limits<float>::quiet_NaN());
        for (size_t i = 0; i < JUNO_MACRO_IDS.size(); ++i) juno_macro_[i] = state_.getRawParameterValue(JUNO_MACRO_IDS[i]);
        juno_macro_last_.fill(std::numeric_limits<float>::quiet_NaN());
        dx_algorithm_ = state_.getRawParameterValue("dx7.algorithm");
        dx_feedback_ = state_.getRawParameterValue("dx7.feedback");
        dx_kbd_level_ = state_.getRawParameterValue("dx7.kbd_level");
        dx_kbd_rate_ = state_.getRawParameterValue("dx7.kbd_rate");
        dx_d2_ = state_.getRawParameterValue("dx7.d2");
        for (int op = 0; op < 4; ++op) for (int field = 0; field < 8; ++field)
            dx_fields_[op][field] = state_.getRawParameterValue(dx_id(op, field));
        for (size_t i = 0; i < DX_GENERAL.size(); ++i) dx_general_[i] = state_.getRawParameterValue(DX_GENERAL[i].id);
        dx_general_last_.fill(std::numeric_limits<float>::quiet_NaN());
        for (int fx = 0; fx < 5; ++fx) for (int field = 0; field < 5; ++field)
            dx_fx_[fx][field] = state_.getRawParameterValue(dx_fx_id(fx, field));
        dx_fx_last_.fill(std::numeric_limits<float>::quiet_NaN());
        for (int group = 0; group < 3; ++group) for (int field = 0; field < DX_GROOVE_FIELDS[size_t(group)]; ++field)
            dx_groove_[group][field] = state_.getRawParameterValue(dx_groove_id(group, field));
        dx_groove_last_.fill(std::numeric_limits<float>::quiet_NaN());
        for (size_t i = 0; i < DX_MACRO_IDS.size(); ++i) dx_macro_[i] = state_.getRawParameterValue(DX_MACRO_IDS[i]);
        juno_cc_parameters_[0] = state_.getParameter("juno.drift");
        for (size_t i = 1; i < juno_cc_parameters_.size(); ++i) juno_cc_parameters_[i] = state_.getParameter(JUNO_MACRO_IDS[i - 1]);
        cc_parameters_[0] = state_.getParameter("dx7.drift");
        for (size_t i = 1; i < cc_parameters_.size(); ++i) cc_parameters_[i] = state_.getParameter(DX_MACRO_IDS[i - 1]);
        dx_macro_last_.fill(std::numeric_limits<float>::quiet_NaN());
        startTimerHz(30);
    }

    const juce::String getName() const override { return "GrooveForge Brain Prototype"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 10.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    bool hasEditor() const override { return true; }
    juce::AudioProcessorEditor* createEditor() override;
    juce::AudioProcessorValueTreeState& parameters() { return state_; }
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override {
        return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
    }
    void prepareToPlay(double sample_rate, int) override {
        sample_rate_ = sample_rate;
        const auto delay_length = size_t(sample_rate > 0.0 ? sample_rate : 44100.0) + 2;
        delay_left_.assign(delay_length, 0);
        delay_right_.assign(delay_length, 0);
        dx_delay_left_.assign(delay_length, 0);
        dx_delay_right_.assign(delay_length, 0);
        juno_delay_left_.assign(delay_length, 0);
        juno_delay_right_.assign(delay_length, 0);
        ob6_delay_left_.assign(delay_length, 0);
        ob6_delay_right_.assign(delay_length, 0);
        plate_memory_.assign(gf::PlateReverbPreview::required_samples(float(sample_rate > 0.0 ? sample_rate : 44100.0)), 0);
        dx_plate_memory_.assign(plate_memory_.size(), 0);
        juno_plate_memory_.assign(plate_memory_.size(), 0);
        ob6_plate_memory_.assign(plate_memory_.size(), 0);
        engine_.prepare(sample_rate);
        dx_bus_.prepare(sample_rate);
        juno_bus_.prepare(sample_rate);
        ob6_bus_.prepare(sample_rate);
        dx7_.prepare(sample_rate);
        juno_.set_parameters(juno_effective_cutoff(), juno_effective_resonance(), attack_->load(), juno_effective_env().release_ms);
        juno_.prepare(sample_rate);
        ob6_.prepare(sample_rate);
        dx_general_last_.fill(std::numeric_limits<float>::quiet_NaN());
        dx_fx_last_.fill(std::numeric_limits<float>::quiet_NaN());
        dx_groove_last_.fill(std::numeric_limits<float>::quiet_NaN());
        dx_macro_last_.fill(std::numeric_limits<float>::quiet_NaN());
        juno_bus_.set_voice_sink(&juno_,
            [](void* context, uint8_t note, float velocity) { static_cast<gf::JunoPreview*>(context)->note_on(note, velocity); },
            [](void* context, uint8_t note) { static_cast<gf::JunoPreview*>(context)->note_off(note); });
        dx_bus_.set_voice_sink(&dx7_,
            [](void* context, uint8_t note, float velocity) { static_cast<gf::Dx7Preview*>(context)->note_on(note, velocity); },
            [](void* context, uint8_t note) { static_cast<gf::Dx7Preview*>(context)->note_off(note); });
        engine_.set_delay_memory(delay_left_.data(), delay_right_.data(), uint32_t(delay_length));
        engine_.set_plate_memory(plate_memory_.data(), plate_memory_.size());
        dx_bus_.set_delay_memory(dx_delay_left_.data(), dx_delay_right_.data(), uint32_t(delay_length));
        dx_bus_.set_plate_memory(dx_plate_memory_.data(), dx_plate_memory_.size());
        juno_bus_.set_delay_memory(juno_delay_left_.data(), juno_delay_right_.data(), uint32_t(delay_length));
        juno_bus_.set_plate_memory(juno_plate_memory_.data(), juno_plate_memory_.size());
        ob6_bus_.set_delay_memory(ob6_delay_left_.data(), ob6_delay_right_.data(), uint32_t(delay_length));
        ob6_bus_.set_plate_memory(ob6_plate_memory_.data(), ob6_plate_memory_.size());
        juno_osc_last_.fill(std::numeric_limits<float>::quiet_NaN());
        juno_env_last_.fill(std::numeric_limits<float>::quiet_NaN());
        juno_lfo_last_.fill(std::numeric_limits<float>::quiet_NaN());
        juno_voice_groove_last_.fill(std::numeric_limits<float>::quiet_NaN());
        juno_groove_last_.fill(std::numeric_limits<float>::quiet_NaN());
        juno_fx_last_.fill(std::numeric_limits<float>::quiet_NaN());
        juno_macro_last_.fill(std::numeric_limits<float>::quiet_NaN());
        applied_sequence_ = 0;
        last_juno_mode_ = false; last_dx7_mode_ = false; last_ob6_mode_ = false;
    }
    void releaseResources() override { engine_.reset(); dx_bus_.reset(); dx7_.reset(); juno_.reset(); juno_bus_.reset(); ob6_.reset(); ob6_bus_.reset(); }
    void reset() override { engine_.reset(); dx_bus_.reset(); dx7_.reset(); juno_.reset(); juno_bus_.reset(); ob6_.reset(); ob6_bus_.reset(); }
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override {
        juce::ScopedNoDenormals guard;
        buffer.clear();
        const bool ob6_mode = ob6_enabled_->load() >= 0.5f;
        const bool juno_mode = !ob6_mode && juno_enabled_->load() >= 0.5f;
        const bool dx7_mode = !ob6_mode && !juno_mode && dx7_enabled_->load() >= 0.5f;
        if (juno_mode) juno_.set_parameters(juno_effective_cutoff(), juno_effective_resonance(), attack_->load(), juno_effective_env().release_ms);
        if (dx7_mode != last_dx7_mode_ || juno_mode != last_juno_mode_ || ob6_mode != last_ob6_mode_) { engine_.reset(); dx_bus_.reset(); dx7_.reset(); juno_.reset(); juno_bus_.reset(); ob6_.reset(); ob6_bus_.reset(); last_dx7_mode_ = dx7_mode; last_juno_mode_ = juno_mode; last_ob6_mode_ = ob6_mode; }
        apply_pending_preset();
        if (juno_mode) {
            bool macro_changed = false;
            for (size_t i = 0; i < JUNO_MACRO_IDS.size(); ++i) {
                const float value = juno_macro_value(i);
                if (value != juno_macro_last_[i]) { juno_macro_last_[i] = value; macro_changed = true; }
            }
            if (macro_changed) { juno_env_last_.fill(std::numeric_limits<float>::quiet_NaN()); juno_fx_last_.fill(std::numeric_limits<float>::quiet_NaN()); }
            apply_juno_osc_automation(); apply_juno_env_automation(); apply_juno_lfo_automation();
            apply_juno_voice_groove_automation(); apply_juno_groove_automation(); apply_juno_fx_automation();
        }
        if (juno_mode) juno_.set_parameters(juno_effective_cutoff(), juno_effective_resonance(), attack_->load(), juno_effective_env().release_ms);
        if (ob6_mode) ob6_.set_parameters(cutoff_->load(), resonance_->load(), attack_->load(), release_->load());
        engine_.set_parameters(cutoff_->load(), resonance_->load(), attack_->load(), release_->load());
        if (dx7_mode) {
            load_dx_macros();
            const bool macro_changed = dx_macro_current_ != dx_macro_last_;
            if (macro_changed) dx_fx_last_.fill(std::numeric_limits<float>::quiet_NaN());
            apply_dx7_automation(); apply_dx7_general(macro_changed); apply_dx7_fx(); apply_dx7_groove();
            dx_macro_last_ = dx_macro_current_;
        }
        float bpm = 120.0f;
        bool has_position = false, running = true;
        double position_beats = 0.0;
        if (auto* playhead = getPlayHead()) {
            if (auto position_info = playhead->getPosition()) {
                if (auto tempo = position_info->getBpm(); tempo && *tempo > 0.0 && std::isfinite(*tempo)) bpm = float(*tempo);
                if (auto beat = position_info->getPpqPosition(); beat && std::isfinite(*beat)) {
                    has_position = true;
                    position_beats = *beat;
                }
                running = position_info->getIsPlaying();
            }
        }
        engine_.set_transport(bpm, has_position, position_beats, running);
        dx_bus_.set_transport(bpm, has_position, position_beats, running);
        juno_bus_.set_transport(bpm, has_position, position_beats, running);
        ob6_bus_.set_transport(bpm, has_position, position_beats, running);
        engine_.set_lfo_sync(lfo_sync_->load() >= 0.5f, bpm);
        dx7_.set_lfo_control(dx7_wheel_, bpm, lfo_sync_->load() >= 0.5f);
        juno_.set_lfo_control(juno_wheel_, bpm, lfo_sync_->load() >= 0.5f);
        ob6_.set_lfo_control(ob6_wheel_, bpm, lfo_sync_->load() >= 0.5f);
        int position = 0;
        auto render_until = [&](int end) {
            end = juce::jlimit(position, buffer.getNumSamples(), end);
            while (position < end) {
                float left, right;
                if (ob6_mode) ob6_bus_.process_external_frame(ob6_.process_sample(), left, right);
                else if (juno_mode) { float in_l, in_r; juno_.process_frame(in_l, in_r); juno_bus_.process_external_stereo(in_l, in_r, left, right); }
                else if (dx7_mode) {
                    dx7_.set_tape_pitch(dx_bus_.tape_pitch_ratio());
                    dx_bus_.process_external_frame(dx7_.process_sample(), left, right);
                }
                else engine_.process_frame(left, right);
                buffer.setSample(0, position, left);
                buffer.setSample(1, position, right);
                ++position;
            }
        };
        for (const auto metadata : midi) {
            render_until(metadata.samplePosition);
            if (metadata.numBytes > 3) continue;
            const auto message = metadata.getMessage();
            if (message.isNoteOn()) {
                if (ob6_mode) ob6_.note_on(uint8_t(message.getNoteNumber()), message.getFloatVelocity());
                else if (juno_mode) juno_bus_.note_on(uint8_t(message.getNoteNumber()), message.getFloatVelocity());
                else if (dx7_mode) dx_bus_.note_on(uint8_t(message.getNoteNumber()), message.getFloatVelocity());
                else engine_.note_on(uint8_t(message.getNoteNumber()), message.getFloatVelocity());
            } else if (message.isNoteOff()) {
                if (ob6_mode) ob6_.note_off(uint8_t(message.getNoteNumber()));
                else if (juno_mode) juno_bus_.note_off(uint8_t(message.getNoteNumber()));
                else if (dx7_mode) dx_bus_.note_off(uint8_t(message.getNoteNumber()));
                else engine_.note_off(uint8_t(message.getNoteNumber()));
            } else if (message.isPitchWheel()) {
                if (ob6_mode) ob6_.set_pitch_bend(message.getPitchWheelValue());
                else if (juno_mode) juno_.set_pitch_bend(message.getPitchWheelValue());
                else if (dx7_mode) dx7_.set_pitch_bend(message.getPitchWheelValue());
                else engine_.set_pitch_bend(message.getPitchWheelValue());
            }
            else if (message.isController() && message.getControllerNumber() == 1) {
                if (ob6_mode) { ob6_wheel_ = message.getControllerValue() / 127.0f; ob6_.set_lfo_control(ob6_wheel_, bpm, lfo_sync_->load() >= 0.5f); }
                else if (juno_mode) { juno_wheel_ = message.getControllerValue() / 127.0f; juno_.set_lfo_control(juno_wheel_, bpm, lfo_sync_->load() >= 0.5f); }
                else if (dx7_mode) { dx7_wheel_ = message.getControllerValue() / 127.0f; dx7_.set_lfo_control(dx7_wheel_, bpm, lfo_sync_->load() >= 0.5f); }
                else engine_.setWheel(message.getControllerValue() / 127.0f);
            }
            else if (message.isController() && message.getControllerNumber() == 64) {
                if (ob6_mode) ob6_.set_sustain(message.getControllerValue() >= 64);
                else if (juno_mode) juno_bus_.set_sustain(message.getControllerValue() >= 64);
                else if (dx7_mode) dx_bus_.set_sustain(message.getControllerValue() >= 64);
                else engine_.set_sustain(message.getControllerValue() >= 64);
            } else if (juno_mode && message.isController() && (message.getControllerNumber() == 14 ||
                       message.getControllerNumber() == 15 || message.getControllerNumber() == 16 || message.getControllerNumber() == 19)) {
                const int number = message.getControllerNumber();
                const size_t index = number == 14 ? 0u : number == 15 ? 1u : number == 16 ? 2u : 3u;
                juno_cc_values_[index].store(message.getControllerValue() / 127.0f, std::memory_order_relaxed);
                juno_cc_sequence_[index].fetch_add(1, std::memory_order_release);
                juno_env_last_.fill(std::numeric_limits<float>::quiet_NaN());
                juno_fx_last_.fill(std::numeric_limits<float>::quiet_NaN());
                apply_juno_voice_groove_automation(); apply_juno_env_automation(); apply_juno_fx_automation();
                juno_.set_parameters(juno_effective_cutoff(), juno_effective_resonance(), attack_->load(), juno_effective_env().release_ms);
            } else if (dx7_mode && message.isController() && (message.getControllerNumber() == 14 ||
                       message.getControllerNumber() == 15 || message.getControllerNumber() == 16 || message.getControllerNumber() == 19)) {
                const int number = message.getControllerNumber();
                const size_t index = number == 14 ? 0u : number == 15 ? 1u : number == 16 ? 2u : 3u;
                cc_values_[index].store(message.getControllerValue() / 127.0f, std::memory_order_relaxed);
                cc_sequence_[index].fetch_add(1, std::memory_order_release);
                load_dx_macros();
                dx_fx_last_.fill(std::numeric_limits<float>::quiet_NaN());
                apply_dx7_automation(); apply_dx7_general(true); apply_dx7_fx();
                dx_macro_last_ = dx_macro_current_;
            } else if (message.isAllNotesOff() || message.isAllSoundOff()) { engine_.reset(); dx_bus_.reset(); dx7_.reset(); juno_.reset(); juno_bus_.reset(); ob6_.reset(); ob6_bus_.reset(); }
        }
        render_until(buffer.getNumSamples());
    }
    void getStateInformation(juce::MemoryBlock& data) override {
        std::lock_guard<std::mutex> guard(preset_mutex_);
        auto snapshot = state_.copyState();
        for (size_t i = 0; i < cc_parameters_.size(); ++i)
            if (cc_sequence_[i].load(std::memory_order_acquire) != cc_delivered_[i].load(std::memory_order_acquire))
                snapshot.setProperty("dxPendingCC" + juce::String(int(i)), cc_values_[i].load(std::memory_order_relaxed), nullptr);
        for (size_t i = 0; i < juno_cc_parameters_.size(); ++i)
            if (juno_cc_sequence_[i].load(std::memory_order_acquire) != juno_cc_delivered_[i].load(std::memory_order_acquire))
                snapshot.setProperty("junoPendingCC" + juce::String(int(i)), juno_cc_values_[i].load(std::memory_order_relaxed), nullptr);
        snapshot.setProperty("dx7ParamVersion", 6, nullptr);
        snapshot.setProperty("junoOscVersion", 1, nullptr);
        snapshot.setProperty("junoEnvVersion", 1, nullptr);
        snapshot.setProperty("junoLfoVersion", 1, nullptr);
        snapshot.setProperty("junoVoiceGrooveVersion", 1, nullptr);
        snapshot.setProperty("junoGrooveVersion", 2, nullptr);
        snapshot.setProperty("junoFxVersion", 1, nullptr);
        snapshot.setProperty("junoMacroVersion", 1, nullptr);
        if (preset_raw_.getSize() > 0) snapshot.setProperty("gfPreset", preset_raw_.toBase64Encoding(), nullptr);
        if (auto xml = snapshot.createXml()) copyXmlToBinary(*xml, data);
    }
    void setStateInformation(const void* data, int size) override {
        auto xml = getXmlFromBinary(data, size);
        if (!xml || !xml->hasTagName(state_.state.getType())) return;
        auto tree = juce::ValueTree::fromXml(*xml);
        std::array<float, 4> pending_cc{};
        std::array<bool, 4> has_pending_cc{};
        for (size_t i = 0; i < pending_cc.size(); ++i) {
            const auto id = "dxPendingCC" + juce::String(int(i));
            has_pending_cc[i] = tree.hasProperty(id);
            if (has_pending_cc[i]) { pending_cc[i] = float(tree.getProperty(id)); tree.removeProperty(id, nullptr); }
        }
        std::array<float, 4> juno_pending_cc{};
        std::array<bool, 4> has_juno_pending_cc{};
        for (size_t i = 0; i < juno_pending_cc.size(); ++i) {
            const auto id = "junoPendingCC" + juce::String(int(i));
            has_juno_pending_cc[i] = tree.hasProperty(id);
            if (has_juno_pending_cc[i]) { juno_pending_cc[i] = float(tree.getProperty(id)); tree.removeProperty(id, nullptr); }
        }
        const int juno_macro_version = int(tree.getProperty("junoMacroVersion", 0));
        tree.removeProperty("junoMacroVersion", nullptr);
        const int juno_fx_version = int(tree.getProperty("junoFxVersion", 0));
        tree.removeProperty("junoFxVersion", nullptr);
        const int juno_groove_version = int(tree.getProperty("junoGrooveVersion", 0));
        tree.removeProperty("junoGrooveVersion", nullptr);
        const int juno_voice_groove_version = int(tree.getProperty("junoVoiceGrooveVersion", 0));
        tree.removeProperty("junoVoiceGrooveVersion", nullptr);
        const int juno_lfo_version = int(tree.getProperty("junoLfoVersion", 0));
        tree.removeProperty("junoLfoVersion", nullptr);
        const int juno_env_version = int(tree.getProperty("junoEnvVersion", 0));
        tree.removeProperty("junoEnvVersion", nullptr);
        const int juno_version = int(tree.getProperty("junoOscVersion", 0));
        tree.removeProperty("junoOscVersion", nullptr);
        const int dx_version = int(tree.getProperty("dx7ParamVersion", 0));
        tree.removeProperty("dx7ParamVersion", nullptr);
        juce::MemoryBlock raw;
        studio::Preset decoded;
        uint8_t engine = 0;
        if (tree.hasProperty("gfPreset")) {
            if (!raw.fromBase64Encoding(tree.getProperty("gfPreset").toString()) ||
                !gf::decode_supported_preset(static_cast<const uint8_t*>(raw.getData()), raw.getSize(), decoded, engine)) return;
            tree.removeProperty("gfPreset", nullptr);
        }
        std::lock_guard<std::mutex> guard(preset_mutex_);
        clear_cc_pending();
        state_.replaceState(tree);
        if (raw.getSize() && engine == 1 && juno_macro_version < 1) set_juno_macro_parameters(decoded);
        if (raw.getSize() && engine == 1 && juno_fx_version < 1) set_juno_fx_parameters(decoded);
        if (raw.getSize() && engine == 1 && juno_groove_version < 1) set_juno_groove_parameters(decoded);
        else if (raw.getSize() && engine == 1 && juno_groove_version < 2) {
            auto* source = state_.getParameter(juno_groove_id(GF_GROOVE_PUMP, GF_PUMP_SOURCE + 1));
            source->setValueNotifyingHost(source->convertTo0to1(decoded.patch.groove[GF_GROOVE_PUMP][GF_PUMP_SOURCE]));
        }
        if (raw.getSize() && engine == 1 && juno_voice_groove_version < 1) set_juno_voice_groove_parameters(decoded);
        if (raw.getSize() && engine == 1 && juno_lfo_version < 1) set_juno_lfo_parameters(decoded);
        if (raw.getSize() && engine == 1 && juno_env_version < 1) set_juno_env_parameters(decoded);
        if (raw.getSize() && engine == 1 && juno_version < 1) set_juno_osc_parameters(decoded);
        if (raw.getSize() && engine == 4) {
            if (dx_version < 1) set_dx7_parameters(gf::make_dx7_operator_patch(decoded));
            if (dx_version < 2) set_dx7_general_parameters(decoded, 0);
            else if (dx_version < 3) set_dx7_general_parameters(decoded, 14);
            if (dx_version < 4) set_dx7_fx_parameters(decoded);
            if (dx_version < 5) set_dx7_groove_parameters(decoded);
            if (dx_version < 6) set_dx7_macro_parameters(decoded);
        }
        for (size_t i = 0; i < pending_cc.size(); ++i)
            if (has_pending_cc[i]) cc_parameters_[i]->setValueNotifyingHost(std::clamp(pending_cc[i], 0.0f, 1.0f));
        for (size_t i = 0; i < juno_pending_cc.size(); ++i)
            if (has_juno_pending_cc[i]) juno_cc_parameters_[i]->setValueNotifyingHost(std::clamp(juno_pending_cc[i], 0.0f, 1.0f));
        preset_raw_ = raw;
        preset_name_ = raw.getSize() ? juce::String::fromUTF8(decoded.name) : juce::String();
        publish_preset(raw);
    }
    bool import_preset(const void* data, size_t size) override {
        studio::Preset preset;
        uint8_t engine = 0;
        if (!gf::decode_supported_preset(static_cast<const uint8_t*>(data), size, preset, engine)) return false;
        clear_cc_pending();
        const auto patch = studio::macro_patch(preset.patch, engine);
        const float move = studio::macro_amount(preset.patch, GF_MACRO_MOVE);
        const float cutoff = std::clamp(preset.cutoff_hz * studio::move_cutoff_mul(move), GF_CUTOFF_RANGE[engine].lo, GF_CUTOFF_RANGE[engine].hi);
        const float reso = studio::move_resonance(studio::macro_resonance(preset.reso_norm, preset.patch), move);
        auto set = [&](const char* id, float value) {
            auto* p = state_.getParameter(id);
            p->setValueNotifyingHost(p->convertTo0to1(value));
        };
        if (engine == 2 || engine == 1 || engine == 3) {
            set("cutoff", cutoff);
            set("resonance", std::clamp(engine == 3 ? .7f + reso * 4.3f : 0.1f + reso * 4.9f, 0.707f, 5.0f));
            set("attack", patch.attack_ms);
            set("release", patch.release_ms);
        }
        if (engine == 1) { set_juno_osc_parameters(preset); set_juno_env_parameters(preset); set_juno_lfo_parameters(preset); set_juno_voice_groove_parameters(preset); set_juno_groove_parameters(preset); set_juno_fx_parameters(preset); set_juno_macro_parameters(preset); }
        if (engine == 4) {
            set_dx7_parameters(gf::make_dx7_operator_patch(preset));
            set_dx7_general_parameters(preset, 0);
            set_dx7_fx_parameters(preset);
            set_dx7_groove_parameters(preset);
            set_dx7_macro_parameters(preset);
        }
        set("ob6_preview", engine == 3 ? 1.0f : 0.0f);
        set("juno_preview", engine == 1 ? 1.0f : 0.0f);
        set("dx7_preview", engine == 4 ? 1.0f : 0.0f);
        std::lock_guard<std::mutex> guard(preset_mutex_);
        preset_raw_.replaceAll(data, size);
        preset_name_ = juce::String::fromUTF8(preset.name);
        publish_preset(preset_raw_);
        return true;
    }
    bool export_juno_preset(std::vector<uint8_t>& data) override {
        if (juno_enabled_->load() < 0.5f || lfo_sync_->load() >= 0.5f) return false;
        studio::Preset preset;
        {
            std::lock_guard<std::mutex> guard(preset_mutex_);
            if (preset_raw_.getSize()) {
                uint8_t source = 0;
                if (!gf::decode_supported_preset(static_cast<const uint8_t*>(preset_raw_.getData()),
                                                 preset_raw_.getSize(), preset, source) || source != 1) return false;
            } else { studio::patch_init(preset.patch, 1); studio::patch_vel_defaults(preset.patch, 1); }
        }
        float velocity_amp, velocity_filter;
        studio::macro_vel_depths(preset.patch, &velocity_amp, &velocity_filter);
        preset.patch = studio::macro_patch(preset.patch, 1);
        preset.patch.vel_amp = velocity_amp * 100.0f; preset.patch.vel_cut = velocity_filter * 100.0f;
        preset.patch.groove[GF_GROOVE_MACRO][GF_MACRO_GRIT] = 0;
        preset.patch.groove[GF_GROOVE_MACRO][GF_MACRO_POCKET] = 0;
        preset.patch.groove[GF_GROOVE_MACRO][GF_MACRO_MOVE] = 0;
        preset.cutoff_hz = juno_effective_cutoff();
        preset.reso_norm = std::clamp((juno_effective_resonance() - 0.1f) / 4.9f, 0.0f, 1.0f);
        preset.patch.attack_ms = attack_->load(); preset.patch.release_ms = release_->load();
        for (size_t i = 0; i < 8; ++i) preset.patch.osc[0][i] = juno_osc_[i]->load();
        preset.patch.mod[0][1] = juno_osc_[8]->load();
        preset.patch.decay_ms = juno_env_[0]->load(); preset.patch.sustain = juno_env_[1]->load();
        preset.patch.f_attack_ms = juno_env_[2]->load(); preset.patch.f_decay_ms = juno_env_[3]->load();
        preset.patch.f_sustain = juno_env_[4]->load(); preset.patch.f_release_ms = juno_env_[5]->load();
        preset.patch.env_amt = juno_env_[6]->load(); preset.patch.drive = juno_env_[7]->load();
        auto effective_env = juno_effective_env();
        preset.patch.decay_ms = effective_env.decay_ms; preset.patch.release_ms = effective_env.release_ms;
        preset.patch.f_decay_ms = effective_env.f_decay_ms; preset.patch.f_release_ms = effective_env.f_release_ms;
        preset.patch.env_amt = effective_env.env_amt; preset.patch.drive = effective_env.drive;
        float effective_amp, effective_filter; studio::macro_vel_depths(juno_base_env(), &effective_amp, &effective_filter);
        preset.patch.vel_amp = effective_amp * 100.0f; preset.patch.vel_cut = effective_filter * 100.0f;
        preset.patch.lfo_rate_hz = juno_lfo_[0]->load(); preset.patch.lfo_shape = uint8_t(juno_lfo_[1]->load());
        preset.patch.lfo_div = uint8_t(juno_lfo_[2]->load()); preset.patch.lfo_amt[GF_DEST_CUTOFF] = juno_lfo_[3]->load();
        preset.patch.lfo_amt[GF_DEST_PITCH] = juno_lfo_[4]->load(); preset.patch.lfo_amt[GF_DEST_PWM] = juno_lfo_[5]->load();
        preset.patch.mod[0][0] = juno_lfo_[6]->load();
        preset.patch.glide = std::sqrt(std::clamp(juno_voice_groove_[0]->load() / 1500.0f, 0.0f, 1.0f));
        preset.patch.groove_on[GF_GROOVE_UNISON] = juno_voice_groove_[1]->load() >= .5f;
        preset.patch.groove[GF_GROOVE_UNISON][GF_UNISON_VOICES] = juno_voice_groove_[2]->load();
        preset.patch.groove[GF_GROOVE_UNISON][GF_UNISON_DETUNE] = juno_voice_groove_[3]->load();
        preset.patch.groove[GF_GROOVE_MACRO][GF_MACRO_DRIFT] = juno_cc_value(0, juno_voice_groove_[4]->load() * .01f) * 100.0f;
        for (int group = 0; group < 3; ++group) {
            preset.patch.groove_on[group] = juno_groove_[group][0]->load() >= .5f;
            for (int field = 1; field < JUNO_GROOVE_FIELDS[size_t(group)]; ++field)
                preset.patch.groove[group][field - 1] = juno_groove_[group][field]->load();
        }
        for (int fx = 0; fx < 5; ++fx) {
            preset.patch.sfx_on[fx] = juno_fx_[fx][0]->load() >= .5f;
            preset.patch.sfx_mix[fx] = juno_fx_[fx][1]->load();
            for (int field = 0; field < 3; ++field) preset.patch.sfx_p[fx][field] = juno_fx_[fx][field + 2]->load();
        }
        preset.patch = studio::move_sfx(preset.patch, juno_macro_value(2));
        std::vector<uint8_t> encoded(studio::PRESET_BYTES);
        if (studio::preset_encode(preset, 1, encoded.data(), encoded.size()) != encoded.size()) return false;
        data = std::move(encoded);
        return true;
    }
    bool export_dx7_preset(std::vector<uint8_t>& data) override {
        if (dx7_enabled_->load() < 0.5f || lfo_sync_->load() >= 0.5f) return false;
        studio::Preset preset;
        {
            std::lock_guard<std::mutex> guard(preset_mutex_);
            if (preset_raw_.getSize()) {
                uint8_t source = 0;
                if (!gf::decode_supported_preset(static_cast<const uint8_t*>(preset_raw_.getData()),
                                                 preset_raw_.getSize(), preset, source) || source != 4) return false;
            } else studio::patch_init(preset.patch, 4);
        }
        auto& p = preset.patch;
        float old_amp, old_filter;
        studio::macro_vel_depths(p, &old_amp, &old_filter);
        p.osc[0][2] = dx_algorithm_->load(); p.osc[0][3] = dx_feedback_->load();
        p.mod[1][0] = dx_kbd_level_->load(); p.mod[1][1] = dx_kbd_rate_->load(); p.mod[1][2] = dx_d2_->load();
        for (int op = 0; op < 4; ++op) {
            p.osc[op][0] = dx_fields_[op][0]->load(); p.osc[op][1] = dx_fields_[op][1]->load();
            p.osc[op][4] = dx_fields_[op][2]->load(); p.osc[op][5] = dx_fields_[op][3]->load();
            p.osc[op][6] = dx_fields_[op][4]->load(); p.osc[op][7] = dx_fields_[op][5]->load();
            p.mod[0][op] = dx_fields_[op][6]->load(); p.mod[0][4 + op] = dx_fields_[op][7]->load();
        }
        auto value = [&](size_t i) { return dx_general_[i]->load(); };
        preset.cutoff_hz = value(0); preset.reso_norm = (value(1) - 0.7f) / 2.3f;
        p.drive = (value(2) - 0.5f) / 3.5f; p.attack_ms = value(3); p.decay_ms = value(4);
        p.sustain = value(5); p.release_ms = value(6); p.vel_amp = value(7) * 100.0f; p.vel_cut = old_filter * 100.0f;
        p.glide = std::sqrt(value(8) / 1500.0f); p.lfo_rate_hz = value(9); p.lfo_shape = uint8_t(value(10));
        p.lfo_amt[GF_DEST_PITCH] = value(11); p.lfo_amt[GF_DEST_AMP] = value(12); p.lfo_div = uint8_t(value(13));
        p.groove_on[GF_GROOVE_UNISON] = value(14) >= 0.5f;
        p.groove[GF_GROOVE_UNISON][GF_UNISON_VOICES] = value(15);
        p.groove[GF_GROOVE_UNISON][GF_UNISON_DETUNE] = value(16);
        p.groove[GF_GROOVE_MACRO][GF_MACRO_DRIFT] = cc_value(0, value(17)) * 100.0f;
        p.groove[GF_GROOVE_MACRO][GF_MACRO_GRIT] = cc_value(1, dx_macro_[0]->load()) * 100.0f;
        p.groove[GF_GROOVE_MACRO][GF_MACRO_POCKET] = cc_value(2, dx_macro_[1]->load()) * 100.0f;
        p.groove[GF_GROOVE_MACRO][GF_MACRO_MOVE] = cc_value(3, dx_macro_[2]->load()) * 100.0f;
        p.groove[GF_GROOVE_MACRO][GF_MACRO_GRIT_RES] = dx_macro_[3]->load() * 100.0f;
        p.groove[GF_GROOVE_MACRO][GF_MACRO_GRIT_MOD] = dx_macro_[4]->load() * 100.0f;
        p.groove[GF_GROOVE_MACRO][GF_MACRO_POCKET_VEL] = dx_macro_[5]->load() * 100.0f;
        for (int fx = 0; fx < 5; ++fx) {
            p.sfx_on[fx] = dx_fx_[fx][0]->load() >= 0.5f;
            p.sfx_mix[fx] = dx_fx_[fx][1]->load();
            for (int field = 0; field < 3; ++field) p.sfx_p[fx][field] = dx_fx_[fx][field + 2]->load();
        }
        for (int group = 0; group < 3; ++group) {
            p.groove_on[group] = dx_groove_[group][0]->load() >= 0.5f;
            for (int field = 1; field < DX_GROOVE_FIELDS[size_t(group)]; ++field)
                p.groove[group][field - 1] = dx_groove_[group][field]->load();
        }
        const float move = studio::macro_amount(p, GF_MACRO_MOVE);
        preset.cutoff_hz = std::min(20000.0f, preset.cutoff_hz * studio::move_cutoff_mul(move));
        preset.reso_norm = studio::move_resonance(studio::macro_resonance(preset.reso_norm, p), move);
        float effective_amp, effective_filter;
        studio::macro_vel_depths(p, &effective_amp, &effective_filter);
        p = studio::move_sfx(studio::macro_patch(p, 4), move);
        p.vel_amp = effective_amp * 100.0f; p.vel_cut = effective_filter * 100.0f;
        p.groove[GF_GROOVE_MACRO][GF_MACRO_GRIT] = 0;
        p.groove[GF_GROOVE_MACRO][GF_MACRO_POCKET] = 0;
        p.groove[GF_GROOVE_MACRO][GF_MACRO_MOVE] = 0;
        std::vector<uint8_t> encoded(studio::PRESET_BYTES);
        if (studio::preset_encode(preset, 4, encoded.data(), encoded.size()) != encoded.size()) return false;
        data = std::move(encoded);
        return true;
    }
    juce::String preset_name() {
        std::lock_guard<std::mutex> guard(preset_mutex_);
        return preset_name_;
    }

private:
    void clear_cc_pending() {
        for (size_t i = 0; i < juno_cc_parameters_.size(); ++i)
            juno_cc_delivered_[i].store(juno_cc_sequence_[i].load(std::memory_order_acquire), std::memory_order_release);
        for (size_t i = 0; i < cc_parameters_.size(); ++i)
            cc_delivered_[i].store(cc_sequence_[i].load(std::memory_order_acquire), std::memory_order_release);
    }
    float cc_value(size_t index, float fallback) const {
        return cc_sequence_[index].load(std::memory_order_acquire) != cc_delivered_[index].load(std::memory_order_acquire)
            ? cc_values_[index].load(std::memory_order_relaxed) : fallback;
    }
    float juno_cc_value(size_t index, float fallback) const {
        return juno_cc_sequence_[index].load(std::memory_order_acquire) != juno_cc_delivered_[index].load(std::memory_order_acquire)
            ? juno_cc_values_[index].load(std::memory_order_relaxed) : fallback;
    }
    float juno_macro_value(size_t index) const {
        return index < 3 ? juno_cc_value(index + 1, juno_macro_[index]->load()) : juno_macro_[index]->load();
    }
    void load_dx_macros() {
        for (size_t i = 0; i < DX_MACRO_IDS.size(); ++i)
            dx_macro_current_[i] = i < 3 ? cc_value(i + 1, dx_macro_[i]->load()) : dx_macro_[i]->load();
    }
    void timerCallback() override {
        for (size_t i = 0; i < juno_cc_parameters_.size(); ++i) {
            const auto sequence = juno_cc_sequence_[i].load(std::memory_order_acquire);
            if (sequence == juno_cc_delivered_[i].load(std::memory_order_relaxed)) continue;
            juno_cc_parameters_[i]->setValueNotifyingHost(juno_cc_values_[i].load(std::memory_order_relaxed));
            juno_cc_delivered_[i].store(sequence, std::memory_order_release);
        }
        for (size_t i = 0; i < cc_parameters_.size(); ++i) {
            const auto sequence = cc_sequence_[i].load(std::memory_order_acquire);
            if (sequence == cc_delivered_[i].load(std::memory_order_relaxed)) continue;
            cc_parameters_[i]->setValueNotifyingHost(cc_values_[i].load(std::memory_order_relaxed));
            cc_delivered_[i].store(sequence, std::memory_order_release);
        }
    }
    void set_dx7_macro_parameters(const studio::Preset& preset) {
        const auto& macro = preset.patch.groove[GF_GROOVE_MACRO];
        const float values[6] = {0, 0, 0, macro[GF_MACRO_GRIT_RES] * 0.01f,
                                 macro[GF_MACRO_GRIT_MOD] * 0.01f, macro[GF_MACRO_POCKET_VEL] * 0.01f};
        for (size_t i = 0; i < DX_MACRO_IDS.size(); ++i) {
            auto* parameter = state_.getParameter(DX_MACRO_IDS[i]);
            parameter->setValueNotifyingHost(parameter->convertTo0to1(values[i]));
        }
    }
    void set_dx7_groove_parameters(const studio::Preset& preset) {
        for (int group = 0; group < 3; ++group) for (int field = 0; field < DX_GROOVE_FIELDS[size_t(group)]; ++field) {
            const float value = field == 0 ? (preset.patch.groove_on[group] ? 1.0f : 0.0f) : preset.patch.groove[group][field - 1];
            auto* parameter = state_.getParameter(dx_groove_id(group, field));
            parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
        }
    }
    void apply_dx7_groove() {
        bool changed = false;
        for (int group = 0; group < 3; ++group) for (int field = 0; field < DX_GROOVE_FIELDS[size_t(group)]; ++field) {
            const size_t i = size_t(group * 7 + field);
            const float value = dx_groove_[group][field]->load();
            if (value != dx_groove_last_[i]) { dx_groove_last_[i] = value; changed = true; }
        }
        if (!changed) return;
        studio::Preset preset;
        for (int group = 0; group < 3; ++group) {
            preset.patch.groove_on[group] = dx_groove_last_[size_t(group * 7)] >= 0.5f;
            for (int field = 1; field < DX_GROOVE_FIELDS[size_t(group)]; ++field)
                preset.patch.groove[group][field - 1] = dx_groove_last_[size_t(group * 7 + field)];
        }
        gf::apply_groove_preview(dx_bus_, preset);
        dx_bus_.set_unison(1, 0.0f);
    }
    void set_dx7_fx_parameters(const studio::Preset& preset) {
        const auto& p = preset.patch;
        for (int fx = 0; fx < 5; ++fx) for (int field = 0; field < 5; ++field) {
            const float value = field == 0 ? (p.sfx_on[fx] ? 1.0f : 0.0f) :
                                field == 1 ? p.sfx_mix[fx] : p.sfx_p[fx][field - 2];
            auto* parameter = state_.getParameter(dx_fx_id(fx, field));
            parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
        }
    }
    void apply_dx7_fx() {
        bool changed = false;
        for (int fx = 0; fx < 5; ++fx) for (int field = 0; field < 5; ++field) {
            const size_t i = size_t(fx * 5 + field);
            const float value = dx_fx_[fx][field]->load();
            if (value != dx_fx_last_[i]) { dx_fx_last_[i] = value; changed = true; }
        }
        if (!changed) return;
        studio::Preset preset;
        for (int fx = 0; fx < 5; ++fx) {
            preset.patch.sfx_on[fx] = dx_fx_last_[size_t(fx * 5)] >= 0.5f;
            preset.patch.sfx_mix[fx] = dx_fx_last_[size_t(fx * 5 + 1)];
            for (int field = 0; field < 3; ++field)
                preset.patch.sfx_p[fx][field] = dx_fx_last_[size_t(fx * 5 + field + 2)];
        }
        preset.patch = studio::move_sfx(preset.patch, dx_macro_current_[2]);
        gf::apply_synth_fx_preview(dx_bus_, preset, 4);
    }
    void set_dx7_general_parameters(const studio::Preset& preset, size_t first) {
        const auto p = studio::macro_patch(preset.patch, 4);
        const float move = studio::macro_amount(preset.patch, GF_MACRO_MOVE);
        const float res = studio::move_resonance(studio::macro_resonance(preset.reso_norm, preset.patch), move);
        float velocity, unused;
        studio::macro_vel_depths(preset.patch, &velocity, &unused);
        const float values[] = {std::clamp(preset.cutoff_hz * studio::move_cutoff_mul(move), GF_CUTOFF_RANGE[4].lo, GF_CUTOFF_RANGE[4].hi),
            0.7f + 2.3f * std::clamp(res, 0.0f, 1.0f), 0.5f + 3.5f * p.drive, p.attack_ms, p.decay_ms,
            p.sustain, p.release_ms, velocity, p.glide * p.glide * 1500.0f, p.lfo_rate_hz,
            float(p.lfo_shape), p.lfo_amt[GF_DEST_PITCH], p.lfo_amt[GF_DEST_AMP], float(p.lfo_div),
            preset.patch.groove_on[GF_GROOVE_UNISON] ? 1.0f : 0.0f,
            preset.patch.groove[GF_GROOVE_UNISON][GF_UNISON_VOICES],
            preset.patch.groove[GF_GROOVE_UNISON][GF_UNISON_DETUNE], studio::macro_amount(preset.patch, GF_MACRO_DRIFT)};
        for (size_t i = first; i < DX_GENERAL.size(); ++i) {
            auto* parameter = state_.getParameter(DX_GENERAL[i].id);
            parameter->setValueNotifyingHost(parameter->convertTo0to1(values[i]));
        }
    }
    void apply_dx7_general(bool force) {
        bool changed = force;
        const float old_on = dx_general_last_[14], old_count = dx_general_last_[15];
        for (size_t i = 0; i < DX_GENERAL.size(); ++i) {
            const float value = i == 17 ? cc_value(0, dx_general_[i]->load()) : dx_general_[i]->load();
            if (value != dx_general_last_[i]) { dx_general_last_[i] = value; changed = true; }
        }
        if (!changed) return;
        const auto& v = dx_general_last_;
        if (std::isfinite(old_on) && (old_on != v[14] || old_count != v[15])) { dx_bus_.reset(); dx7_.reset(); }
        studio::Patch macro_patch;
        macro_patch.drive = (v[2] - 0.5f) / 3.5f; macro_patch.decay_ms = v[4]; macro_patch.release_ms = v[6];
        macro_patch.vel_amp = v[7] * 100.0f;
        macro_patch.groove[GF_GROOVE_MACRO][GF_MACRO_GRIT] = dx_macro_current_[0] * 100.0f;
        macro_patch.groove[GF_GROOVE_MACRO][GF_MACRO_POCKET] = dx_macro_current_[1] * 100.0f;
        macro_patch.groove[GF_GROOVE_MACRO][GF_MACRO_GRIT_RES] = dx_macro_current_[3] * 100.0f;
        macro_patch.groove[GF_GROOVE_MACRO][GF_MACRO_GRIT_MOD] = dx_macro_current_[4] * 100.0f;
        macro_patch.groove[GF_GROOVE_MACRO][GF_MACRO_POCKET_VEL] = dx_macro_current_[5] * 100.0f;
        const auto effective = studio::macro_patch(macro_patch, 4);
        float velocity, unused;
        studio::macro_vel_depths(macro_patch, &velocity, &unused);
        const float cutoff = std::min(20000.0f, v[0] * studio::move_cutoff_mul(dx_macro_current_[2]));
        const float res = studio::move_resonance(studio::macro_resonance((v[1] - 0.7f) / 2.3f, macro_patch), dx_macro_current_[2]);
        dx7_.set_patch(dx7_.operator_patch(), cutoff, 0.7f + 2.3f * res, 0.5f + 3.5f * effective.drive,
                       v[3], effective.decay_ms, v[5], effective.release_ms, velocity);
        dx7_.set_modulation(v[8], v[9], dsp::LfoShape(uint8_t(v[10])), v[11], v[12], GF_LFO_DIV_BEATS[uint8_t(v[13])]);
        dx7_.set_unison(v[14] >= 0.5f ? GF_UNISON_COUNT[uint8_t(v[15])] : 1, v[16], v[17]);
    }
    void set_dx7_parameters(const Dx7Patch& fm) {
        auto set = [&](const juce::String& id, float value) {
            auto* parameter = state_.getParameter(id);
            parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
        };
        set("dx7.algorithm", fm.algorithm); set("dx7.feedback", fm.feedback);
        set("dx7.kbd_level", fm.kbd_level); set("dx7.kbd_rate", fm.kbd_rate); set("dx7.d2", fm.d2_ms);
        for (int op = 0; op < 4; ++op) {
            const auto& o = fm.op[op];
            const float values[8] = {o.ratio, o.level, o.attack_ms, o.decay_ms, o.sustain * 100.0f,
                                     o.release_ms, float(o.wave + 1), float(o.detune)};
            for (int field = 0; field < 8; ++field) set(dx_id(op, field), values[field]);
        }
    }
    void apply_dx7_automation() {
        auto patch = dx7_.operator_patch();
        bool changed = false;
        auto update = [&](float value, float& target) { if (value != target) { target = value; changed = true; } };
        const auto algorithm = uint8_t(dx_algorithm_->load());
        const float grit_mod = dx_macro_current_[0] * dx_macro_current_[4];
        const auto feedback = uint8_t(std::min(7, int(dx_feedback_->load()) + int(3.0f * grit_mod + 0.5f)));
        if (patch.algorithm != algorithm || patch.feedback != feedback) { patch.algorithm = algorithm; patch.feedback = feedback; changed = true; }
        update(dx_kbd_level_->load(), patch.kbd_level);
        update(dx_d2_->load(), patch.d2_ms);
        const auto kbd_rate = uint8_t(dx_kbd_rate_->load());
        if (patch.kbd_rate != kbd_rate) { patch.kbd_rate = kbd_rate; changed = true; }
        for (int op = 0; op < 4; ++op) {
            auto& o = patch.op[op];
            update(dx_fields_[op][0]->load(), o.ratio);
            const float level = dx_fields_[op][1]->load() + (dx7_is_carrier(algorithm, op) ? 0.0f : 20.0f * grit_mod);
            update(std::min(99.0f, level), o.level);
            update(dx_fields_[op][2]->load(), o.attack_ms); update(dx_fields_[op][3]->load(), o.decay_ms);
            update(dx_fields_[op][4]->load() * 0.01f, o.sustain); update(dx_fields_[op][5]->load(), o.release_ms);
            const auto wave = uint8_t(dx_fields_[op][6]->load() - 1.0f);
            const auto detune = int8_t(dx_fields_[op][7]->load());
            if (o.wave != wave || o.detune != detune) { o.wave = wave; o.detune = detune; changed = true; }
        }
        if (changed) dx7_.set_operator_patch(patch);
    }
    void set_juno_macro_parameters(const studio::Preset& preset) {
        const auto& m = preset.patch.groove[GF_GROOVE_MACRO];
        const float values[] = {0, 0, 0, m[GF_MACRO_GRIT_RES] * .01f, m[GF_MACRO_GRIT_MOD] * .01f, m[GF_MACRO_POCKET_VEL] * .01f};
        for (size_t i = 0; i < JUNO_MACRO_IDS.size(); ++i) {
            auto* p = state_.getParameter(JUNO_MACRO_IDS[i]); p->setValueNotifyingHost(p->convertTo0to1(values[i]));
        }
    }
    studio::Patch juno_base_env() const {
        studio::Patch p;
        p.decay_ms = juno_env_[0]->load(); p.sustain = juno_env_[1]->load();
        p.f_attack_ms = juno_env_[2]->load(); p.f_decay_ms = juno_env_[3]->load(); p.f_sustain = juno_env_[4]->load();
        p.f_release_ms = juno_env_[5]->load(); p.env_amt = juno_env_[6]->load(); p.drive = juno_env_[7]->load();
        p.vel_amp = juno_env_[8]->load(); p.vel_cut = juno_env_[9]->load(); p.release_ms = release_->load();
        auto& m = p.groove[GF_GROOVE_MACRO];
        m[GF_MACRO_GRIT] = juno_macro_value(0) * 100; m[GF_MACRO_POCKET] = juno_macro_value(1) * 100;
        m[GF_MACRO_MOVE] = juno_macro_value(2) * 100; m[GF_MACRO_GRIT_RES] = juno_macro_[3]->load() * 100;
        m[GF_MACRO_GRIT_MOD] = juno_macro_[4]->load() * 100; m[GF_MACRO_POCKET_VEL] = juno_macro_[5]->load() * 100;
        return p;
    }
    studio::Patch juno_effective_env() const { return studio::macro_patch(juno_base_env(), 1); }
    float juno_effective_cutoff() const { return std::clamp(cutoff_->load() * studio::move_cutoff_mul(juno_macro_value(2)), 20.0f, 8000.0f); }
    float juno_effective_resonance() const {
        const auto p = juno_base_env();
        const float norm = std::clamp((resonance_->load() - .1f) / 4.9f, 0.0f, 1.0f);
        return std::clamp(.1f + 4.9f * studio::move_resonance(studio::macro_resonance(norm, p), juno_macro_value(2)), .707f, 5.0f);
    }
    void set_juno_fx_parameters(const studio::Preset& preset) {
        const auto effective = studio::move_sfx(preset.patch, studio::macro_amount(preset.patch, GF_MACRO_MOVE));
        for (int fx = 0; fx < 5; ++fx) for (int field = 0; field < 5; ++field) {
            auto* p = state_.getParameter(juno_fx_id(fx, field));
            const float value = field == 0 ? (effective.sfx_on[fx] ? 1.0f : 0.0f) :
                                field == 1 ? effective.sfx_mix[fx] : effective.sfx_p[fx][field - 2];
            p->setValueNotifyingHost(p->convertTo0to1(value));
        }
    }
    void apply_juno_fx_automation() {
        bool changed = false;
        for (int fx = 0; fx < 5; ++fx) for (int field = 0; field < 5; ++field) {
            const auto i = size_t(fx * 5 + field);
            const float v = juno_fx_[fx][field]->load();
            if (v != juno_fx_last_[i]) { juno_fx_last_[i] = v; changed = true; }
        }
        if (!changed) return;
        studio::Preset p;
        for (int fx = 0; fx < 5; ++fx) {
            p.patch.sfx_on[fx] = juno_fx_last_[size_t(fx * 5)] >= .5f;
            p.patch.sfx_mix[fx] = juno_fx_last_[size_t(fx * 5 + 1)];
            for (int field = 0; field < 3; ++field) p.patch.sfx_p[fx][field] = juno_fx_last_[size_t(fx * 5 + field + 2)];
        }
        p.patch = studio::move_sfx(p.patch, juno_macro_value(2));
        gf::apply_synth_fx_preview(juno_bus_, p, 1);
    }
    void set_juno_groove_parameters(const studio::Preset& preset) {
        for (int group = 0; group < 3; ++group) for (int field = 0; field < JUNO_GROOVE_FIELDS[size_t(group)]; ++field) {
            auto* p = state_.getParameter(juno_groove_id(group, field));
            const float value = field == 0 ? (preset.patch.groove_on[group] ? 1.0f : 0.0f) : preset.patch.groove[group][field - 1];
            p->setValueNotifyingHost(p->convertTo0to1(value));
        }
    }
    void apply_juno_groove_automation() {
        bool changed = false;
        for (int group = 0; group < 3; ++group) for (int field = 0; field < JUNO_GROOVE_FIELDS[size_t(group)]; ++field) {
            const auto i = size_t(group * 7 + field);
            const float v = juno_groove_[group][field]->load();
            if (v != juno_groove_last_[i]) { juno_groove_last_[i] = v; changed = true; }
        }
        if (!changed) return;
        studio::Preset p;
        for (int group = 0; group < 3; ++group) {
            p.patch.groove_on[group] = juno_groove_last_[size_t(group * 7)] >= .5f;
            for (int field = 1; field < JUNO_GROOVE_FIELDS[size_t(group)]; ++field)
                p.patch.groove[group][field - 1] = juno_groove_last_[size_t(group * 7 + field)];
        }
        gf::apply_groove_preview(juno_bus_, p, false);
    }
    void set_juno_voice_groove_parameters(const studio::Preset& preset) {
        const auto& p = preset.patch;
        const float glide = std::clamp(p.glide, 0.0f, 1.0f);
        const float values[] = {glide * glide * 1500.0f, p.groove_on[GF_GROOVE_UNISON] ? 1.0f : 0.0f,
            p.groove[GF_GROOVE_UNISON][GF_UNISON_VOICES], p.groove[GF_GROOVE_UNISON][GF_UNISON_DETUNE],
            p.groove[GF_GROOVE_MACRO][GF_MACRO_DRIFT]};
        for (size_t i = 0; i < JUNO_VOICE_GROOVE.size(); ++i) {
            auto* parameter = state_.getParameter(JUNO_VOICE_GROOVE[i].id);
            parameter->setValueNotifyingHost(parameter->convertTo0to1(values[i]));
        }
    }
    void apply_juno_voice_groove_automation() {
        std::array<float, JUNO_VOICE_GROOVE.size()> v{};
        for (size_t i = 0; i < v.size(); ++i) v[i] = i == 4 ? juno_cc_value(0, juno_voice_groove_[i]->load() * .01f) * 100.0f : juno_voice_groove_[i]->load();
        if (v == juno_voice_groove_last_) return;
        const auto index = size_t(std::clamp(int(v[2]), 0, 3));
        juno_.set_voice_modulation(v[0], v[1] >= .5f ? GF_UNISON_COUNT[index] : 1, v[3], v[4] * .01f);
        juno_voice_groove_last_ = v;
    }
    void set_juno_lfo_parameters(const studio::Preset& preset) {
        const auto& p = preset.patch;
        const float values[] = {p.lfo_rate_hz, float(p.lfo_shape), float(p.lfo_div), p.lfo_amt[GF_DEST_CUTOFF],
                                p.lfo_amt[GF_DEST_PITCH], p.lfo_amt[GF_DEST_PWM], p.mod[0][0]};
        for (size_t i = 0; i < JUNO_LFO.size(); ++i) {
            auto* parameter = state_.getParameter(JUNO_LFO[i].id);
            parameter->setValueNotifyingHost(parameter->convertTo0to1(values[i]));
        }
    }
    void apply_juno_lfo_automation() {
        std::array<float, JUNO_LFO.size()> v{};
        for (size_t i = 0; i < v.size(); ++i) v[i] = juno_lfo_[i]->load();
        if (v == juno_lfo_last_) return;
        const auto division = size_t(std::clamp(int(v[2]), 0, 3));
        juno_.set_lfo(v[0], dsp::LfoShape(uint8_t(v[1])), v[3], v[4], v[5], GF_LFO_DIV_BEATS[division], v[6]);
        juno_lfo_last_ = v;
    }
    void set_juno_env_parameters(const studio::Preset& preset) {
        const auto p = studio::macro_patch(preset.patch, 1);
        float va, vf; studio::macro_vel_depths(p, &va, &vf);
        const float values[] = {p.decay_ms, p.sustain, p.f_attack_ms, p.f_decay_ms, p.f_sustain, p.f_release_ms,
                                p.env_amt, p.drive, va * 100.0f, vf * 100.0f};
        for (size_t i = 0; i < JUNO_ENV.size(); ++i) {
            auto* parameter = state_.getParameter(JUNO_ENV[i].id);
            parameter->setValueNotifyingHost(parameter->convertTo0to1(values[i]));
        }
    }
    void apply_juno_env_automation() {
        std::array<float, JUNO_ENV.size()> v{};
        for (size_t i = 0; i < v.size(); ++i) v[i] = juno_env_[i]->load();
        if (v == juno_env_last_) return;
        const auto p = juno_effective_env();
        float va, vf; studio::macro_vel_depths(juno_base_env(), &va, &vf);
        juno_.set_patch(p.decay_ms, p.sustain, p.f_attack_ms, p.f_decay_ms, p.f_sustain, p.f_release_ms,
                        p.env_amt, 0.5f + 3.5f * p.drive, va, vf);
        juno_env_last_ = v;
    }
    void set_juno_osc_parameters(const studio::Preset& preset) {
        for (size_t i = 0; i < JUNO_OSC.size(); ++i) {
            auto* parameter = state_.getParameter(JUNO_OSC[i].id);
            const float value = i < 8 ? preset.patch.osc[0][i] : preset.patch.mod[0][1];
            parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
        }
    }
    void apply_juno_osc_automation() {
        std::array<float, JUNO_OSC.size()> values{};
        for (size_t i = 0; i < values.size(); ++i) values[i] = juno_osc_[i]->load();
        if (values == juno_osc_last_) return;
        constexpr uint16_t waves[] = {dsp::WAVE_SAWTOOTH, dsp::WAVE_SQUARE, dsp::WAVE_PULSE, dsp::WAVE_SAW_AND_PULSE};
        juno_.set_oscillator(waves[std::clamp(int(values[0]), 0, 3)], values[1] * .01f, values[2] * .01f,
            values[3] * .01f, uint8_t(values[4]), uint8_t(values[5]), uint8_t(values[6]), values[7] >= .5f, values[8] * .01f);
        juno_osc_last_ = values;
    }
    void publish_preset(const juce::MemoryBlock& raw) {
        preset_sequence_.fetch_add(1, std::memory_order_acq_rel);
        const auto* bytes = static_cast<const uint8_t*>(raw.getData());
        for (size_t i = 0; i < raw.getSize(); ++i) preset_bytes_[i].store(bytes[i], std::memory_order_relaxed);
        preset_size_.store(raw.getSize(), std::memory_order_relaxed);
        preset_sequence_.fetch_add(1, std::memory_order_release);
    }
    void apply_pending_preset() {
        const auto sequence = preset_sequence_.load(std::memory_order_acquire);
        if ((sequence & 1u) || sequence == applied_sequence_) return;
        const size_t size = preset_size_.load(std::memory_order_relaxed);
        if (size > studio::PRESET_BYTES) return;
        std::array<uint8_t, studio::PRESET_BYTES> bytes{};
        for (size_t i = 0; i < size; ++i) bytes[i] = preset_bytes_[i].load(std::memory_order_relaxed);
        if (sequence != preset_sequence_.load(std::memory_order_acquire)) return;
        if (size == 0) {
            engine_ = gf::ProphetPreview{};
            engine_.prepare(sample_rate_);
            engine_.set_delay_memory(delay_left_.data(), delay_right_.data(), uint32_t(delay_left_.size()));
            engine_.set_plate_memory(plate_memory_.data(), plate_memory_.size());
        } else {
            studio::Preset preset;
            uint8_t preset_engine = 0;
            if (!gf::decode_supported_preset(bytes.data(), size, preset, preset_engine)) return;
            if (preset_engine == 3) {
                ob6_.reset(); ob6_bus_.reset(); gf::apply_ob6_preset_preview(ob6_, preset);
                gf::apply_synth_fx_preview(ob6_bus_, preset, 3);
            } else if (preset_engine == 1) {
                juno_.reset(); juno_bus_.reset(); gf::apply_juno_preset_preview(juno_, preset);
                juno_osc_last_.fill(std::numeric_limits<float>::quiet_NaN());
                juno_env_last_.fill(std::numeric_limits<float>::quiet_NaN());
                juno_lfo_last_.fill(std::numeric_limits<float>::quiet_NaN());
                juno_voice_groove_last_.fill(std::numeric_limits<float>::quiet_NaN());
                gf::apply_synth_fx_preview(juno_bus_, preset, 1);
                juno_fx_last_.fill(std::numeric_limits<float>::quiet_NaN());
                gf::apply_groove_preview(juno_bus_, preset, false);
                juno_groove_last_.fill(std::numeric_limits<float>::quiet_NaN());
            } else if (preset_engine == 4) {
                dx7_.reset(); dx_bus_.reset(); gf::apply_dx7_preset_preview(dx7_, preset);
                dx_general_last_.fill(std::numeric_limits<float>::quiet_NaN());
                dx_fx_last_.fill(std::numeric_limits<float>::quiet_NaN());
                dx_groove_last_.fill(std::numeric_limits<float>::quiet_NaN());
                gf::apply_synth_fx_preview(dx_bus_, preset, 4);
                gf::apply_groove_preview(dx_bus_, preset);
                dx_bus_.set_unison(1, 0.0f);
            }
            else { engine_.reset(); gf::apply_prophet_preset_preview(engine_, preset); }
        }
        applied_sequence_ = sequence;
    }
    gf::ProphetPreview engine_;
    gf::ProphetPreview dx_bus_;
    gf::ProphetPreview juno_bus_;
    gf::ProphetPreview ob6_bus_;
    gf::Dx7Preview dx7_;
    gf::JunoPreview juno_;
    gf::Ob6Preview ob6_;
    float ob6_wheel_ = 0.0f;
    bool last_ob6_mode_ = false;
    float juno_wheel_ = 0.0f;
    bool last_juno_mode_ = false;
    bool last_dx7_mode_ = false;
    float dx7_wheel_ = 0.0f;
    std::vector<int16_t> delay_left_, delay_right_;
    std::vector<int16_t> dx_delay_left_, dx_delay_right_;
    std::vector<int16_t> juno_delay_left_, juno_delay_right_;
    std::vector<int16_t> ob6_delay_left_, ob6_delay_right_;
    std::vector<int16_t> plate_memory_;
    std::vector<int16_t> dx_plate_memory_;
    std::vector<int16_t> juno_plate_memory_;
    std::vector<int16_t> ob6_plate_memory_;
    juce::AudioProcessorValueTreeState state_;
    std::atomic<float>* cutoff_ = nullptr;
    std::atomic<float>* resonance_ = nullptr;
    std::atomic<float>* attack_ = nullptr;
    std::atomic<float>* release_ = nullptr;
    std::atomic<float>* lfo_sync_ = nullptr;
    std::atomic<float>* dx7_enabled_ = nullptr;
    std::atomic<float>* juno_enabled_ = nullptr;
    std::atomic<float>* ob6_enabled_ = nullptr;
    std::array<std::atomic<float>*, JUNO_OSC.size()> juno_osc_{};
    std::array<float, JUNO_OSC.size()> juno_osc_last_{};
    std::array<std::atomic<float>*, JUNO_ENV.size()> juno_env_{};
    std::array<float, JUNO_ENV.size()> juno_env_last_{};
    std::array<std::atomic<float>*, JUNO_LFO.size()> juno_lfo_{};
    std::array<float, JUNO_LFO.size()> juno_lfo_last_{};
    std::array<std::atomic<float>*, JUNO_VOICE_GROOVE.size()> juno_voice_groove_{};
    std::array<float, JUNO_VOICE_GROOVE.size()> juno_voice_groove_last_{};
    std::array<std::array<std::atomic<float>*, 7>, 3> juno_groove_{};
    std::array<float, 21> juno_groove_last_{};
    std::array<std::array<std::atomic<float>*, 5>, 5> juno_fx_{};
    std::array<float, 25> juno_fx_last_{};
    std::array<std::atomic<float>*, JUNO_MACRO_IDS.size()> juno_macro_{};
    std::array<float, JUNO_MACRO_IDS.size()> juno_macro_last_{};
    std::atomic<float>* dx_algorithm_ = nullptr;
    std::atomic<float>* dx_feedback_ = nullptr;
    std::atomic<float>* dx_kbd_level_ = nullptr;
    std::atomic<float>* dx_kbd_rate_ = nullptr;
    std::atomic<float>* dx_d2_ = nullptr;
    std::array<std::array<std::atomic<float>*, 8>, 4> dx_fields_{};
    std::array<std::atomic<float>*, DX_GENERAL.size()> dx_general_{};
    std::array<float, DX_GENERAL.size()> dx_general_last_{};
    std::array<std::array<std::atomic<float>*, 5>, 5> dx_fx_{};
    std::array<float, 25> dx_fx_last_{};
    std::array<std::array<std::atomic<float>*, 7>, 3> dx_groove_{};
    std::array<float, 21> dx_groove_last_{};
    std::array<std::atomic<float>*, DX_MACRO_IDS.size()> dx_macro_{};
    std::array<float, DX_MACRO_IDS.size()> dx_macro_current_{}, dx_macro_last_{};
    std::array<juce::RangedAudioParameter*, 4> juno_cc_parameters_{};
    std::array<std::atomic<float>, 4> juno_cc_values_{};
    std::array<std::atomic<uint64_t>, 4> juno_cc_sequence_{}, juno_cc_delivered_{};
    std::array<juce::RangedAudioParameter*, 4> cc_parameters_{};
    std::array<std::atomic<float>, 4> cc_values_{};
    std::array<std::atomic<uint64_t>, 4> cc_sequence_{}, cc_delivered_{};
    double sample_rate_ = 44100.0;
    std::mutex preset_mutex_;
    juce::MemoryBlock preset_raw_;
    juce::String preset_name_;
    std::array<std::atomic<uint8_t>, studio::PRESET_BYTES> preset_bytes_{};
    std::atomic<size_t> preset_size_{0};
    std::atomic<uint64_t> preset_sequence_{0};
    uint64_t applied_sequence_ = 0;
};

class BrainEditor final : public juce::AudioProcessorEditor, private juce::Timer {
public:
    explicit BrainEditor(BrainPrototype& owner) : AudioProcessorEditor(owner), owner_(owner) {
        constexpr const char* ids[] = {"cutoff", "resonance", "attack", "release"};
        constexpr const char* names[] = {"CUTOFF", "RESONANCE", "ATTACK", "RELEASE"};
        constexpr const char* units[] = {" Hz", "", " ms", " ms"};
        for (size_t i = 0; i < knobs_.size(); ++i) {
            auto& knob = knobs_[i];
            knob.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
            knob.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 100, 24);
            knob.setTextValueSuffix(units[i]);
            knob.setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xff1d9e75));
            knob.setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colour(0xff384540));
            knob.setColour(juce::Slider::thumbColourId, juce::Colour(0xff5dcaa5));
            knob.setColour(juce::Slider::textBoxTextColourId, juce::Colour(0xfff2f2f2));
            knob.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
            addAndMakeVisible(knob);
            labels_[i].setText(names[i], juce::dontSendNotification);
            labels_[i].setJustificationType(juce::Justification::centred);
            labels_[i].setColour(juce::Label::textColourId, juce::Colour(0xff9fe1cb));
            labels_[i].setFont(juce::Font(juce::FontOptions(12.0f, juce::Font::bold)));
            addAndMakeVisible(labels_[i]);
            attachments_[i] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
                owner.parameters(), ids[i], knob);
        }
        import_.setButtonText("IMPORT GFP");
        import_.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff0e5040));
        import_.setColour(juce::TextButton::textColourOffId, juce::Colour(0xfff2f2f2));
        import_.onClick = [this] { choose_preset(); };
        addAndMakeVisible(import_);
        sync_.setButtonText("LFO SYNC");
        sync_.setColour(juce::ToggleButton::textColourId, juce::Colour(0xff9fe1cb));
        sync_.setColour(juce::ToggleButton::tickColourId, juce::Colour(0xff5dcaa5));
        addAndMakeVisible(sync_);
        sync_attachment_ = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
            owner.parameters(), "lfo_sync", sync_);
        dx7_toggle_.setButtonText("DX7 PREVIEW");
        dx7_toggle_.setColour(juce::ToggleButton::textColourId, juce::Colour(0xff9fe1cb));
        addAndMakeVisible(dx7_toggle_);
        dx7_attachment_ = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
            owner.parameters(), "dx7_preview", dx7_toggle_);
        juno_toggle_.setButtonText("JUNO PREVIEW");
        juno_toggle_.setColour(juce::ToggleButton::textColourId, juce::Colour(0xff9fe1cb));
        addAndMakeVisible(juno_toggle_);
        juno_attachment_ = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(owner.parameters(), "juno_preview", juno_toggle_);
        ob6_toggle_.setButtonText("OB-6 PREVIEW");
        ob6_toggle_.setColour(juce::ToggleButton::textColourId, juce::Colour(0xff9fe1cb));
        addAndMakeVisible(ob6_toggle_);
        ob6_attachment_ = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(owner.parameters(), "ob6_preview", ob6_toggle_);
        juno_toggle_.onClick = [this] { if (juno_toggle_.getToggleState()) { dx7_toggle_.setToggleState(false, juce::sendNotification); ob6_toggle_.setToggleState(false, juce::sendNotification); } };
        dx7_toggle_.onClick = [this] { if (dx7_toggle_.getToggleState()) { juno_toggle_.setToggleState(false, juce::sendNotification); ob6_toggle_.setToggleState(false, juce::sendNotification); } };
        ob6_toggle_.onClick = [this] { if (ob6_toggle_.getToggleState()) { juno_toggle_.setToggleState(false, juce::sendNotification); dx7_toggle_.setToggleState(false, juce::sendNotification); } };
        juno_page_.setButtonText("OSC");
        juno_page_.onClick = [this] { juno_page_index_ = (juno_page_index_ + 1) % 8; update_status(); };
        addChildComponent(juno_page_);
        constexpr const char* descriptions[] = {"0 Saw · 1 Square · 2 Pulse · 3 Saw+Pulse", "Nivel del suboscilador", "Nivel de ruido", "Ancho de pulso", "0 Off · 1 I · 2 II · 3 I+II", "0 Bass boost · 1 Flat · 2 225 Hz · 3 720 Hz", "0 16 pies · 1 8 pies · 2 4 pies", "0 ENV · 1 GATE", "Ruido del chorus BBD"};
        for (size_t i = 0; i < JUNO_OSC.size(); ++i) {
            juno_labels_[i].setText(JUNO_OSC[i].label, juce::dontSendNotification);
            juno_labels_[i].setColour(juce::Label::textColourId, juce::Colour(0xff9fe1cb));
            addChildComponent(juno_labels_[i]);
            juno_sliders_[i].setSliderStyle(juce::Slider::LinearHorizontal);
            juno_sliders_[i].setTextBoxStyle(juce::Slider::TextBoxRight, false, 60, 22);
            juno_sliders_[i].setColour(juce::Slider::textBoxTextColourId, juce::Colour(0xfff2f2f2));
            juno_sliders_[i].setTooltip(descriptions[i]);
            addChildComponent(juno_sliders_[i]);
            juno_attachments_[i] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(owner.parameters(), JUNO_OSC[i].id, juno_sliders_[i]);
        }
        for (size_t i = 0; i < JUNO_ENV.size(); ++i) {
            juno_env_labels_[i].setText(JUNO_ENV[i].label, juce::dontSendNotification);
            juno_env_labels_[i].setColour(juce::Label::textColourId, juce::Colour(0xff9fe1cb));
            addChildComponent(juno_env_labels_[i]);
            juno_env_sliders_[i].setSliderStyle(juce::Slider::LinearHorizontal);
            juno_env_sliders_[i].setTextBoxStyle(juce::Slider::TextBoxRight, false, 65, 22);
            juno_env_sliders_[i].setColour(juce::Slider::textBoxTextColourId, juce::Colour(0xfff2f2f2));
            addChildComponent(juno_env_sliders_[i]);
            juno_env_attachments_[i] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(owner.parameters(), JUNO_ENV[i].id, juno_env_sliders_[i]);
        }
        constexpr const char* lfo_descriptions[] = {"Velocidad libre", "0 Sine · 1 Triangle · 2 Saw · 3 Square · 4 Sample & Hold", "0 1/4 · 1 1/8 · 2 1/16 · 3 1/8T", "Profundidad hacia cutoff", "Profundidad hacia pitch", "Profundidad hacia ancho de pulso", "Tiempo de entrada de la modulación"};
        for (size_t i = 0; i < JUNO_LFO.size(); ++i) {
            juno_lfo_labels_[i].setText(JUNO_LFO[i].label, juce::dontSendNotification);
            juno_lfo_labels_[i].setColour(juce::Label::textColourId, juce::Colour(0xff9fe1cb));
            addChildComponent(juno_lfo_labels_[i]);
            juno_lfo_sliders_[i].setSliderStyle(juce::Slider::LinearHorizontal);
            juno_lfo_sliders_[i].setTextBoxStyle(juce::Slider::TextBoxRight, false, 65, 22);
            juno_lfo_sliders_[i].setColour(juce::Slider::textBoxTextColourId, juce::Colour(0xfff2f2f2));
            juno_lfo_sliders_[i].setTooltip(lfo_descriptions[i]);
            addChildComponent(juno_lfo_sliders_[i]);
            juno_lfo_attachments_[i] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(owner.parameters(), JUNO_LFO[i].id, juno_lfo_sliders_[i]);
        }
        constexpr const char* voice_groove_descriptions[] = {"Glide en milisegundos", "Activa varias voces por nota", "0: 1 voz · 1: 2 · 2: 3 · 3: 6", "Anchura total entre voces", "Variación por nota de pitch y cutoff"};
        for (size_t i = 0; i < JUNO_VOICE_GROOVE.size(); ++i) {
            juno_voice_groove_labels_[i].setText(JUNO_VOICE_GROOVE[i].label, juce::dontSendNotification);
            juno_voice_groove_labels_[i].setColour(juce::Label::textColourId, juce::Colour(0xff9fe1cb));
            addChildComponent(juno_voice_groove_labels_[i]);
            juno_voice_groove_sliders_[i].setSliderStyle(juce::Slider::LinearHorizontal);
            juno_voice_groove_sliders_[i].setTextBoxStyle(juce::Slider::TextBoxRight, false, 65, 22);
            juno_voice_groove_sliders_[i].setColour(juce::Slider::textBoxTextColourId, juce::Colour(0xfff2f2f2));
            juno_voice_groove_sliders_[i].setTooltip(voice_groove_descriptions[i]);
            addChildComponent(juno_voice_groove_sliders_[i]);
            juno_voice_groove_attachments_[i] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(owner.parameters(), JUNO_VOICE_GROOVE[i].id, juno_voice_groove_sliders_[i]);
        }
        for (size_t i = 0; i < JUNO_MACRO_IDS.size(); ++i) {
            juno_macro_labels_[i].setText(DX_MACRO_LABELS[i], juce::dontSendNotification);
            juno_macro_labels_[i].setColour(juce::Label::textColourId, juce::Colour(0xff9fe1cb));
            addChildComponent(juno_macro_labels_[i]);
            juno_macro_sliders_[i].setSliderStyle(juce::Slider::LinearHorizontal);
            juno_macro_sliders_[i].setTextBoxStyle(juce::Slider::TextBoxRight, false, 65, 22);
            juno_macro_sliders_[i].setColour(juce::Slider::textBoxTextColourId, juce::Colour(0xfff2f2f2));
            addChildComponent(juno_macro_sliders_[i]);
            juno_macro_attachments_[i] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(owner.parameters(), JUNO_MACRO_IDS[i], juno_macro_sliders_[i]);
        }
        dx_page_.setButtonText("VOICE");
        dx_page_.onClick = [this] { dx_page_index_ = (dx_page_index_ + 1) % 5; update_status(); };
        addChildComponent(dx_page_);
        export_.setButtonText("EXPORT GFP");
        export_.onClick = [this] { export_dx7(); };
        addChildComponent(export_);
        for (size_t i = 0; i < DX_MACRO_IDS.size(); ++i) {
            dx_macro_labels_[i].setText(DX_MACRO_LABELS[i], juce::dontSendNotification);
            dx_macro_labels_[i].setColour(juce::Label::textColourId, juce::Colour(0xff9fe1cb));
            addChildComponent(dx_macro_labels_[i]);
            dx_macro_sliders_[i].setSliderStyle(juce::Slider::LinearHorizontal);
            dx_macro_sliders_[i].setTextBoxStyle(juce::Slider::TextBoxRight, false, 65, 20);
            dx_macro_sliders_[i].setColour(juce::Slider::textBoxTextColourId, juce::Colour(0xfff2f2f2));
            addChildComponent(dx_macro_sliders_[i]);
            dx_macro_attachments_[i] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
                owner.parameters(), DX_MACRO_IDS[i], dx_macro_sliders_[i]);
        }
        for (size_t i = 0; i < DX_GENERAL.size(); ++i) {
            dx_voice_labels_[i].setText(DX_GENERAL[i].label, juce::dontSendNotification);
            dx_voice_labels_[i].setColour(juce::Label::textColourId, juce::Colour(0xff9fe1cb));
            dx_voice_labels_[i].setFont(juce::Font(juce::FontOptions(11.0f)));
            addChildComponent(dx_voice_labels_[i]);
            dx_voice_sliders_[i].setSliderStyle(juce::Slider::LinearHorizontal);
            dx_voice_sliders_[i].setTextBoxStyle(juce::Slider::TextBoxRight, false, 65, 20);
            dx_voice_sliders_[i].setColour(juce::Slider::textBoxTextColourId, juce::Colour(0xfff2f2f2));
            addChildComponent(dx_voice_sliders_[i]);
            dx_voice_attachments_[i] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
                owner.parameters(), DX_GENERAL[i].id, dx_voice_sliders_[i]);
        }
        constexpr const char* fx_headers[] = {"ON", "MIX", "P1", "P2", "P3"};
        for (int i = 0; i < 5; ++i) {
            dx_fx_labels_[i].setText(juce::String(DX_FX_NAMES[size_t(i)]).toUpperCase(), juce::dontSendNotification);
            dx_fx_headers_[i].setText(fx_headers[i], juce::dontSendNotification);
            for (auto* label : {&dx_fx_labels_[i], &dx_fx_headers_[i]}) {
                label->setColour(juce::Label::textColourId, juce::Colour(0xff9fe1cb));
                label->setFont(juce::Font(juce::FontOptions(10.0f)));
                addChildComponent(*label);
            }
            for (int field = 0; field < 5; ++field) {
                auto& slider = dx_fx_sliders_[i][field];
                slider.setSliderStyle(juce::Slider::LinearHorizontal);
                slider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 50, 18);
                slider.setColour(juce::Slider::textBoxTextColourId, juce::Colour(0xfff2f2f2));
                slider.setTooltip(juce::String(DX_FX_NAMES[size_t(i)]).toUpperCase() + " · " + DX_FX_LABELS[i][field]);
                addChildComponent(slider);
                dx_fx_attachments_[i][field] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
                    owner.parameters(), dx_fx_id(i, field), slider);
            }
        }
        for (int fx = 0; fx < 5; ++fx) {
            juno_fx_labels_[fx].setText(juce::String(DX_FX_NAMES[size_t(fx)]).toUpperCase(), juce::dontSendNotification);
            juno_fx_labels_[fx].setColour(juce::Label::textColourId, juce::Colour(0xff9fe1cb));
            addChildComponent(juno_fx_labels_[fx]);
            juno_fx_headers_[fx].setText(DX_FX_LABELS[0][fx], juce::dontSendNotification);
            juno_fx_headers_[fx].setColour(juce::Label::textColourId, juce::Colour(0xff9fe1cb));
            addChildComponent(juno_fx_headers_[fx]);
            for (int field = 0; field < 5; ++field) {
                auto& slider = juno_fx_sliders_[fx][field];
                slider.setSliderStyle(juce::Slider::LinearHorizontal);
                slider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 40, 18);
                slider.setColour(juce::Slider::textBoxTextColourId, juce::Colour(0xfff2f2f2));
                slider.setTooltip(DX_FX_LABELS[fx][field]);
                addChildComponent(slider);
                juno_fx_attachments_[fx][field] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(owner.parameters(), juno_fx_id(fx, field), slider);
            }
        }
        constexpr const char* groove_labels[3][7] = {
            {"ON", "DEPTH", "SHAPE", "RATE", "OFFSET", "RELEASE", ""},
            {"ON", "MODE", "DIV", "GATE", "OCT", "SWING", "LATCH"},
            {"ON", "TYPE", "INV", "STRUM", "KEY", "", ""}};
        for (int group = 0; group < 3; ++group) {
            dx_groove_group_labels_[group].setText(juce::String(DX_GROOVE_NAMES[size_t(group)]).toUpperCase(), juce::dontSendNotification);
            dx_groove_group_labels_[group].setColour(juce::Label::textColourId, juce::Colour(0xff9fe1cb));
            addChildComponent(dx_groove_group_labels_[group]);
            for (int field = 0; field < DX_GROOVE_FIELDS[size_t(group)]; ++field) {
                auto& label = dx_groove_labels_[group][field];
                label.setText(groove_labels[group][field], juce::dontSendNotification);
                label.setColour(juce::Label::textColourId, juce::Colour(0xff9fe1cb));
                label.setFont(juce::Font(juce::FontOptions(9.0f)));
                addChildComponent(label);
                auto& slider = dx_groove_sliders_[group][field];
                slider.setSliderStyle(juce::Slider::LinearHorizontal);
                slider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 40, 18);
                slider.setColour(juce::Slider::textBoxTextColourId, juce::Colour(0xfff2f2f2));
                addChildComponent(slider);
                dx_groove_attachments_[group][field] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
                    owner.parameters(), dx_groove_id(group, field), slider);
            }
        }
        for (int group = 0; group < 3; ++group) {
            juno_groove_group_labels_[group].setText(juce::String(DX_GROOVE_NAMES[size_t(group)]).toUpperCase(), juce::dontSendNotification);
            juno_groove_group_labels_[group].setColour(juce::Label::textColourId, juce::Colour(0xff9fe1cb));
            addChildComponent(juno_groove_group_labels_[group]);
            for (int field = 0; field < JUNO_GROOVE_FIELDS[size_t(group)]; ++field) {
                auto& label = juno_groove_labels_[group][field];
                label.setText(group == 0 && field == 6 ? "SRC GFP" : groove_labels[group][field], juce::dontSendNotification);
                label.setColour(juce::Label::textColourId, juce::Colour(0xff9fe1cb));
                label.setFont(juce::Font(juce::FontOptions(9.0f)));
                addChildComponent(label);
                auto& slider = juno_groove_sliders_[group][field];
                slider.setSliderStyle(juce::Slider::LinearHorizontal);
                slider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 40, 18);
                slider.setColour(juce::Slider::textBoxTextColourId, juce::Colour(0xfff2f2f2));
                if (group == 0 && field == 6) slider.setTooltip("SOURCE GFP: 0 AUTO, 1 MIDI, 2 INT. El plugin usa el reloj del DAW.");
                addChildComponent(slider);
                juno_groove_attachments_[group][field] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(owner.parameters(), juno_groove_id(group, field), slider);
            }
        }
        dx_voice_sliders_[15].textFromValueFunction = [](double value) {
            constexpr int counts[] = {1, 2, 3, 6};
            return juce::String(counts[std::clamp(int(std::lround(value)), 0, 3)]);
        };
        dx_voice_sliders_[15].valueFromTextFunction = [](const juce::String& text) {
            const int count = text.getIntValue();
            return count <= 1 ? 0.0 : count == 2 ? 1.0 : count == 3 ? 2.0 : 3.0;
        };
        constexpr const char* field_names[] = {"RATIO", "LEVEL", "ATTACK", "DECAY", "SUSTAIN", "RELEASE", "WAVE", "DETUNE"};
        for (int field = 0; field < 8; ++field) {
            dx_field_labels_[field].setText(field_names[field], juce::dontSendNotification);
            dx_field_labels_[field].setColour(juce::Label::textColourId, juce::Colour(0xff9fe1cb));
            dx_field_labels_[field].setFont(juce::Font(juce::FontOptions(10.0f)));
            addChildComponent(dx_field_labels_[field]);
            for (int op = 0; op < 4; ++op) {
                auto& slider = dx_sliders_[op][field];
                slider.setSliderStyle(juce::Slider::LinearHorizontal);
                slider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 55, 18);
                slider.setColour(juce::Slider::trackColourId, juce::Colour(0xff1d9e75));
                slider.setColour(juce::Slider::textBoxTextColourId, juce::Colour(0xfff2f2f2));
                addChildComponent(slider);
                dx_attachments_[op][field] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
                    owner.parameters(), dx_id(op, field), slider);
            }
        }
        for (int op = 0; op < 4; ++op) {
            dx_op_labels_[op].setText("OP " + juce::String(op + 1), juce::dontSendNotification);
            dx_op_labels_[op].setJustificationType(juce::Justification::centred);
            dx_op_labels_[op].setColour(juce::Label::textColourId, juce::Colour(0xff9fe1cb));
            addChildComponent(dx_op_labels_[op]);
        }
        constexpr const char* global_ids[] = {"dx7.algorithm", "dx7.feedback", "dx7.kbd_level", "dx7.kbd_rate", "dx7.d2"};
        constexpr const char* global_names[] = {"ALG", "FB", "KBD LV", "KBD RT", "D2"};
        for (int i = 0; i < 5; ++i) {
            dx_global_labels_[i].setText(global_names[i], juce::dontSendNotification);
            dx_global_labels_[i].setColour(juce::Label::textColourId, juce::Colour(0xff9fe1cb));
            dx_global_labels_[i].setFont(juce::Font(juce::FontOptions(10.0f)));
            addChildComponent(dx_global_labels_[i]);
            dx_globals_[i].setSliderStyle(juce::Slider::LinearHorizontal);
            dx_globals_[i].setTextBoxStyle(juce::Slider::TextBoxRight, false, 40, 18);
            dx_globals_[i].setColour(juce::Slider::textBoxTextColourId, juce::Colour(0xfff2f2f2));
            addChildComponent(dx_globals_[i]);
            dx_global_attachments_[i] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
                owner.parameters(), global_ids[i], dx_globals_[i]);
        }
        const int groove_width = (getWidth() - 120) / 7;
        for (int group = 0; group < 3; ++group) {
            const int y = 100 + group * 68;
            dx_groove_group_labels_[group].setBounds(20, y + 15, 65, 24);
            for (int field = 0; field < DX_GROOVE_FIELDS[size_t(group)]; ++field) {
                dx_groove_labels_[group][field].setBounds(90 + field * groove_width, y, groove_width, 18);
                dx_groove_sliders_[group][field].setBounds(90 + field * groove_width, y + 19, groove_width - 4, 22);
            }
        }
        status_.setColour(juce::Label::textColourId, juce::Colour(0xff9fe1cb));
        status_.setJustificationType(juce::Justification::centred);
        status_.setFont(juce::Font(juce::FontOptions(12.0f)));
        update_status();
        addAndMakeVisible(status_);
        startTimerHz(5);
        setSize(720, 360);
    }

    void paint(juce::Graphics& g) override {
        g.fillAll(juce::Colour(0xff0a0a0a));
        g.setColour(juce::Colour(0xff1c1c1c));
        g.fillRoundedRectangle(18.0f, 74.0f, float(getWidth() - 36), 250.0f, 6.0f);
        g.setColour(juce::Colour(0xff5dcaa5));
        g.setFont(juce::Font(juce::FontOptions(14.0f, juce::Font::bold)));
        g.drawText("GROOVEFORGE  /  BRAIN", 28, 15, 350, 24, juce::Justification::left);
        g.setColour(juce::Colour(0xfff2f2f2));
        g.setFont(juce::Font(juce::FontOptions(22.0f, juce::Font::bold)));
        g.drawText(ob6_toggle_.getToggleState() ? "OB-6  /  PREVIEW" : juno_toggle_.getToggleState() ? "JUNO  /  PREVIEW" : dx7_toggle_.getToggleState() ? "DX7  /  PREVIEW" : "PROPHET  /  PREVIEW", 28, 40, 350, 28, juce::Justification::left);
        g.setColour(juce::Colour(0xff9fe1cb));
        g.setFont(juce::Font(juce::FontOptions(12.0f)));
        if (!dx7_toggle_.getToggleState() && !juno_toggle_.getToggleState() && !ob6_toggle_.getToggleState()) g.drawText("EXPERIMENTAL  ·  5 VOCES", 400, 42, 290, 24, juce::Justification::right);
    }

    void resized() override {
        const int width = (getWidth() - 60) / 4;
        for (size_t i = 0; i < knobs_.size(); ++i) {
            const int x = 30 + int(i) * width;
            labels_[i].setBounds(x, 102, width, 24);
            knobs_[i].setBounds(x + 8, 130, width - 16, 155);
        }
        import_.setBounds(getWidth() - 145, 12, 115, 25);
        sync_.setBounds(getWidth() - 260, 12, 105, 25);
        dx7_toggle_.setBounds(getWidth() - 380, 12, 115, 25);
        juno_toggle_.setBounds(getWidth() - 510, 12, 125, 25);
        ob6_toggle_.setBounds(300, 42, 125, 25);
        dx_page_.setBounds(getWidth() - 155, 42, 120, 25);
        juno_page_.setBounds(getWidth() - 155, 42, 120, 25);
        for (size_t i = 0; i < JUNO_OSC.size(); ++i) {
            const int x = i < 5 ? 30 : 370, y = 100 + int(i % 5) * 42;
            juno_labels_[i].setBounds(x, y, 120, 24);
            juno_sliders_[i].setBounds(x + 120, y, 180, 24);
        }
        for (size_t i = 0; i < JUNO_ENV.size(); ++i) {
            const int x = i < 5 ? 30 : 370, y = 100 + int(i % 5) * 42;
            juno_env_labels_[i].setBounds(x, y, 125, 24);
            juno_env_sliders_[i].setBounds(x + 125, y, 175, 24);
        }
        for (size_t i = 0; i < JUNO_LFO.size(); ++i) {
            const int x = i < 4 ? 30 : 370, y = 100 + int(i % 4) * 42;
            juno_lfo_labels_[i].setBounds(x, y, 125, 24);
            juno_lfo_sliders_[i].setBounds(x + 125, y, 175, 24);
        }
        for (size_t i = 0; i < JUNO_VOICE_GROOVE.size(); ++i) {
            const int x = i < 3 ? 30 : 370, y = 100 + int(i % 3) * 50;
            juno_voice_groove_labels_[i].setBounds(x, y, 125, 24);
            juno_voice_groove_sliders_[i].setBounds(x + 125, y, 175, 24);
        }
        for (size_t i = 0; i < JUNO_MACRO_IDS.size(); ++i) {
            const int x = i < 3 ? 30 : 370, y = 115 + int(i % 3) * 50;
            juno_macro_labels_[i].setBounds(x, y, 125, 24);
            juno_macro_sliders_[i].setBounds(x + 125, y, 175, 24);
        }
        export_.setBounds(getWidth() - 285, 42, 120, 25);
        for (size_t i = 0; i < DX_MACRO_IDS.size(); ++i) {
            const int x = i < 3 ? 30 : 370, y = 125 + int(i % 3) * 50;
            dx_macro_labels_[i].setBounds(x, y, 100, 24);
            dx_macro_sliders_[i].setBounds(x + 100, y, 200, 24);
        }
        for (size_t i = 0; i < DX_GENERAL.size(); ++i) {
            const int x = i < 9 ? 30 : 370;
            const int y = 100 + int(i % 9) * 23;
            dx_voice_labels_[i].setBounds(x, y, 100, 24);
            dx_voice_sliders_[i].setBounds(x + 100, y, 200, 24);
        }
        const int fx_width = (getWidth() - 120) / 5;
        for (int i = 0; i < 5; ++i) {
            dx_fx_headers_[i].setBounds(90 + i * fx_width, 80, fx_width, 22);
            dx_fx_labels_[i].setBounds(25, 110 + i * 39, 65, 24);
            for (int field = 0; field < 5; ++field)
                dx_fx_sliders_[i][field].setBounds(90 + field * fx_width, 110 + i * 39, fx_width - 4, 24);
        }
        const int dx_width = (getWidth() - 120) / 4;
        for (int op = 0; op < 4; ++op) {
            dx_op_labels_[op].setBounds(90 + op * dx_width, 78, dx_width, 22);
            for (int field = 0; field < 8; ++field)
                dx_sliders_[op][field].setBounds(90 + op * dx_width, 102 + field * 22, dx_width - 4, 21);
        }
        for (int field = 0; field < 8; ++field) dx_field_labels_[field].setBounds(27, 102 + field * 22, 65, 21);
        for (int i = 0; i < 5; ++i) {
            dx_global_labels_[i].setBounds(25 + i * 135, 288, 52, 22);
            dx_globals_[i].setBounds(77 + i * 135, 288, 76, 22);
        }
        const int juno_fx_width = (getWidth() - 120) / 5;
        for (int fx = 0; fx < 5; ++fx) {
            juno_fx_headers_[fx].setBounds(90 + fx * juno_fx_width, 80, juno_fx_width, 22);
            juno_fx_labels_[fx].setBounds(25, 110 + fx * 39, 65, 24);
            for (int field = 0; field < 5; ++field)
                juno_fx_sliders_[fx][field].setBounds(90 + field * juno_fx_width, 110 + fx * 39, juno_fx_width - 4, 24);
        }
        const int juno_groove_width = (getWidth() - 120) / 7;
        for (int group = 0; group < 3; ++group) {
            const int y = 100 + group * 68;
            juno_groove_group_labels_[group].setBounds(20, y + 15, 65, 24);
            for (int field = 0; field < JUNO_GROOVE_FIELDS[size_t(group)]; ++field) {
                juno_groove_labels_[group][field].setBounds(90 + field * juno_groove_width, y, juno_groove_width, 18);
                juno_groove_sliders_[group][field].setBounds(90 + field * juno_groove_width, y + 19, juno_groove_width - 4, 22);
            }
        }
        status_.setBounds(28, 328, getWidth() - 56, 24);
    }

private:
    void update_status() {
        const auto name = owner_.preset_name();
        displayed_preset_ = name;
        const bool ob6 = ob6_toggle_.getToggleState();
        const bool juno = !ob6 && juno_toggle_.getToggleState();
        const bool dx7 = !ob6 && !juno && dx7_toggle_.getToggleState();
        const bool dx_ops = dx7 && dx_page_index_ == 0, dx_voice = dx7 && dx_page_index_ == 1;
        const bool dx_fx = dx7 && dx_page_index_ == 2, dx_groove = dx7 && dx_page_index_ == 3, dx_macro = dx7 && dx_page_index_ == 4;
        dx_page_.setVisible(dx7);
        juno_page_.setVisible(juno);
        juno_page_.setButtonText(juno_page_index_ == 0 ? "OSC" : juno_page_index_ == 1 ? "ENV" : juno_page_index_ == 2 ? "LFO" : juno_page_index_ == 3 ? "UNISON" : juno_page_index_ == 4 ? "GROOVE" : juno_page_index_ == 5 ? "FX" : juno_page_index_ == 6 ? "MACROS" : "VOICE");
        for (auto& label : juno_labels_) label.setVisible(juno && juno_page_index_ == 1);
        for (auto& slider : juno_sliders_) slider.setVisible(juno && juno_page_index_ == 1);
        for (auto& label : juno_env_labels_) label.setVisible(juno && juno_page_index_ == 2);
        for (auto& slider : juno_env_sliders_) slider.setVisible(juno && juno_page_index_ == 2);
        for (auto& label : juno_lfo_labels_) label.setVisible(juno && juno_page_index_ == 3);
        for (auto& slider : juno_lfo_sliders_) slider.setVisible(juno && juno_page_index_ == 3);
        for (auto& label : juno_voice_groove_labels_) label.setVisible(juno && juno_page_index_ == 4);
        for (auto& slider : juno_voice_groove_sliders_) slider.setVisible(juno && juno_page_index_ == 4);
        for (auto& label : juno_macro_labels_) label.setVisible(juno && juno_page_index_ == 7);
        for (auto& slider : juno_macro_sliders_) slider.setVisible(juno && juno_page_index_ == 7);
        for (auto& label : juno_fx_labels_) label.setVisible(juno && juno_page_index_ == 6);
        for (auto& label : juno_fx_headers_) label.setVisible(juno && juno_page_index_ == 6);
        for (auto& row : juno_fx_sliders_) for (auto& slider : row) slider.setVisible(juno && juno_page_index_ == 6);
        for (auto& label : juno_groove_group_labels_) label.setVisible(juno && juno_page_index_ == 5);
        for (int group = 0; group < 3; ++group) for (int field = 0; field < JUNO_GROOVE_FIELDS[size_t(group)]; ++field) {
            juno_groove_labels_[group][field].setVisible(juno && juno_page_index_ == 5);
            juno_groove_sliders_[group][field].setVisible(juno && juno_page_index_ == 5);
        }
        export_.setVisible(dx7 || juno);
        dx_page_.setButtonText(dx_ops ? "VOICE" : dx_voice ? "FX" : dx_fx ? "GROOVE" : dx_groove ? "MACROS" : "OPERATORS");
        for (auto& knob : knobs_) knob.setVisible(!dx7 && !(juno && juno_page_index_ != 0));
        for (auto& label : labels_) label.setVisible(!dx7 && !(juno && juno_page_index_ != 0));
        for (auto& label : dx_field_labels_) label.setVisible(dx_ops);
        for (auto& label : dx_op_labels_) label.setVisible(dx_ops);
        for (auto& row : dx_sliders_) for (auto& slider : row) slider.setVisible(dx_ops);
        for (auto& label : dx_global_labels_) label.setVisible(dx_ops);
        for (auto& slider : dx_globals_) slider.setVisible(dx_ops);
        for (auto& label : dx_voice_labels_) label.setVisible(dx_voice);
        for (auto& slider : dx_voice_sliders_) slider.setVisible(dx_voice);
        for (auto& label : dx_fx_labels_) label.setVisible(dx_fx);
        for (auto& label : dx_fx_headers_) label.setVisible(dx_fx);
        for (auto& row : dx_fx_sliders_) for (auto& slider : row) slider.setVisible(dx_fx);
        for (auto& label : dx_groove_group_labels_) label.setVisible(dx_groove);
        for (int group = 0; group < 3; ++group) for (int field = 0; field < DX_GROOVE_FIELDS[size_t(group)]; ++field) {
            dx_groove_labels_[group][field].setVisible(dx_groove);
            dx_groove_sliders_[group][field].setVisible(dx_groove);
        }
        for (auto& label : dx_macro_labels_) label.setVisible(dx_macro);
        for (auto& slider : dx_macro_sliders_) slider.setVisible(dx_macro);
        sync_.setEnabled(true);
        status_.setText(ob6 ? juce::String((name.isEmpty() ? juce::String("OB-6 INIT") : name) + " · OB-6 GFP PARCIAL · SIN ARP / PUMP") : juno ? juce::String((name.isEmpty() ? juce::String("INIT") : name) + " · JUNO PREVIEW") : dx7 ? (name.isEmpty() ? juce::String("DX7 E.PIANO") : name) + " · DX7 PREVIEW" :
            (name.isEmpty() ? juce::String("INIT") : name) + "  ·  PREVIEW: CADENA FX INCOMPLETA", juce::dontSendNotification);
        displayed_dx7_ = dx7_toggle_.getToggleState();
        displayed_juno_ = juno_toggle_.getToggleState();
        displayed_ob6_ = ob6;
        repaint();
    }
    void timerCallback() override {
        if (owner_.preset_name() != displayed_preset_ || displayed_dx7_ != dx7_toggle_.getToggleState() || displayed_juno_ != juno_toggle_.getToggleState() || displayed_ob6_ != ob6_toggle_.getToggleState()) update_status();
    }
    void choose_preset() {
        chooser_ = std::make_unique<juce::FileChooser>("Importar preset Juno, Prophet, OB-6 o DX7 GFP", juce::File(), "*.gfp");
        juce::Component::SafePointer<BrainEditor> safe(this);
        chooser_->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
            [safe](const juce::FileChooser& chooser) {
                if (!safe) return;
                const auto file = chooser.getResult();
                if (file == juce::File()) return;
                juce::MemoryBlock data;
                if (file.getSize() <= 0 || file.getSize() > int64_t(studio::PRESET_BYTES) ||
                    !file.loadFileAsData(data) || !safe->owner_.import_preset(data.getData(), data.getSize())) {
                    safe->status_.setText("GFP INVÁLIDO O DE OTRO ENGINE · SIN CAMBIOS", juce::dontSendNotification);
                    return;
                }
                safe->update_status();
            });
    }
    void export_dx7() {
        std::vector<uint8_t> bytes;
        const bool juno = juno_toggle_.getToggleState();
        if (!(juno ? owner_.export_juno_preset(bytes) : owner_.export_dx7_preset(bytes))) {
            status_.setText("EXPORT: DESACTIVA LFO SYNC Y CARGA UN GFP DEL ENGINE", juce::dontSendNotification);
            return;
        }
        chooser_ = std::make_unique<juce::FileChooser>("Guardar preset GFP", juce::File(juno ? "Juno.gfp" : "DX7.gfp"), "*.gfp");
        juce::Component::SafePointer<BrainEditor> safe(this);
        chooser_->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles |
                              juce::FileBrowserComponent::warnAboutOverwriting,
            [safe, bytes = std::move(bytes)](const juce::FileChooser& chooser) {
                if (!safe) return;
                const auto file = chooser.getResult();
                if (file == juce::File()) return;
                const bool ok = file.replaceWithData(bytes.data(), bytes.size());
                safe->status_.setText(ok ? "GFP GUARDADO · GRIT / POCKET / MOVE INCORPORADOS" : "NO SE PUDO GUARDAR GFP",
                                      juce::dontSendNotification);
            });
    }
    BrainPrototype& owner_;
    juce::TooltipWindow tooltip_{this, 500};
    juce::TextButton import_;
    juce::ToggleButton sync_;
    juce::TextButton juno_page_;
    int juno_page_index_ = 0;
    std::array<juce::Label, JUNO_MACRO_IDS.size()> juno_macro_labels_;
    std::array<juce::Slider, JUNO_MACRO_IDS.size()> juno_macro_sliders_;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>, JUNO_MACRO_IDS.size()> juno_macro_attachments_;
    std::array<juce::Label, 5> juno_fx_labels_, juno_fx_headers_;
    std::array<std::array<juce::Slider, 5>, 5> juno_fx_sliders_;
    std::array<std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>, 5>, 5> juno_fx_attachments_;
    std::array<juce::Label, 3> juno_groove_group_labels_;
    std::array<std::array<juce::Label, 7>, 3> juno_groove_labels_;
    std::array<std::array<juce::Slider, 7>, 3> juno_groove_sliders_;
    std::array<std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>, 7>, 3> juno_groove_attachments_;
    std::array<juce::Label, JUNO_VOICE_GROOVE.size()> juno_voice_groove_labels_;
    std::array<juce::Slider, JUNO_VOICE_GROOVE.size()> juno_voice_groove_sliders_;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>, JUNO_VOICE_GROOVE.size()> juno_voice_groove_attachments_;
    std::array<juce::Label, JUNO_LFO.size()> juno_lfo_labels_;
    std::array<juce::Slider, JUNO_LFO.size()> juno_lfo_sliders_;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>, JUNO_LFO.size()> juno_lfo_attachments_;
    std::array<juce::Label, JUNO_ENV.size()> juno_env_labels_;
    std::array<juce::Slider, JUNO_ENV.size()> juno_env_sliders_;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>, JUNO_ENV.size()> juno_env_attachments_;
    std::array<juce::Label, JUNO_OSC.size()> juno_labels_;
    std::array<juce::Slider, JUNO_OSC.size()> juno_sliders_;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>, JUNO_OSC.size()> juno_attachments_;
    juce::ToggleButton ob6_toggle_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> ob6_attachment_;
    bool displayed_ob6_ = false;
    juce::ToggleButton juno_toggle_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> juno_attachment_;
    bool displayed_juno_ = false;
    juce::ToggleButton dx7_toggle_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> dx7_attachment_;
    bool displayed_dx7_ = false;
    int dx_page_index_ = 0;
    juce::TextButton dx_page_;
    juce::TextButton export_;
    std::array<juce::Label, DX_GENERAL.size()> dx_voice_labels_;
    std::array<juce::Slider, DX_GENERAL.size()> dx_voice_sliders_;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>, DX_GENERAL.size()> dx_voice_attachments_;
    std::array<juce::Label, 5> dx_fx_labels_, dx_fx_headers_;
    std::array<std::array<juce::Slider, 5>, 5> dx_fx_sliders_;
    std::array<std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>, 5>, 5> dx_fx_attachments_;
    std::array<juce::Label, 3> dx_groove_group_labels_;
    std::array<std::array<juce::Label, 7>, 3> dx_groove_labels_;
    std::array<std::array<juce::Slider, 7>, 3> dx_groove_sliders_;
    std::array<std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>, 7>, 3> dx_groove_attachments_;
    std::array<juce::Label, DX_MACRO_IDS.size()> dx_macro_labels_;
    std::array<juce::Slider, DX_MACRO_IDS.size()> dx_macro_sliders_;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>, DX_MACRO_IDS.size()> dx_macro_attachments_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> sync_attachment_;
    juce::Label status_;
    std::unique_ptr<juce::FileChooser> chooser_;
    juce::String displayed_preset_;
    std::array<juce::Slider, 4> knobs_;
    std::array<juce::Label, 4> labels_;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>, 4> attachments_;
    std::array<juce::Label, 8> dx_field_labels_;
    std::array<juce::Label, 4> dx_op_labels_;
    std::array<std::array<juce::Slider, 8>, 4> dx_sliders_;
    std::array<std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>, 8>, 4> dx_attachments_;
    std::array<juce::Label, 5> dx_global_labels_;
    std::array<juce::Slider, 5> dx_globals_;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>, 5> dx_global_attachments_;
};

juce::AudioProcessorEditor* BrainPrototype::createEditor() { return new BrainEditor(*this); }

} // namespace

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new BrainPrototype; }
