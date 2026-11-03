#include <juce_audio_utils/juce_audio_utils.h>
#include <array>
#include <limits>
#include <mutex>
#include <cstring>

#include "studio_look_and_feel.h"
#include "output_meter.h"
#include "studio_keyboard.h"
#include "preset_browser.h"
#include "envelope_editor.h"
#include "lfo_display.h"
#include "prophet_preview.h"
#include "prophet_preset.h"
#include "preset_import.h"
#include "dx7_preview.h"
#include "juno_preview.h"
#include "ob6_preview.h"
#include "model_d_preview.h"
#include "arp2600_preview.h"
#include "arp2600_preset.h"
#include "hybrid_preview.h"
#include "hybrid_codec.h"
#include "model_d_preset.h"
#include "ob6_preset.h"
#include "juno_preset.h"
#include "dx7_preset.h"
#include "ui/groove_macros.h"

namespace {

void compact_slider_text(juce::Slider& slider) {
    slider.textFromValueFunction = [](double value) {
        if (std::abs(value - std::round(value)) < 1e-5) return juce::String(int(std::lround(value)));
        return juce::String(value, std::abs(value) >= 10.0 ? 1 : 2);
    };
    slider.updateText();
}

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
juce::String ob6_fx_id(int fx, int field) { return "ob6.fx." + juce::String(DX_FX_NAMES[size_t(fx)]) + "." + DX_FX_FIELDS[size_t(field)]; }
juce::String hybrid_osc_id(unsigned side, unsigned page, unsigned slot) {
    return juce::String(side == 0 ? "hybrid.a.osc." : "hybrid.b.osc.") + juce::String(int(page)) + "." + juce::String(int(slot));
}
const char* hybrid_osc_label(uint8_t engine, unsigned page, unsigned slot) {
    static const char* const labels[6][6][8] = {
        {{"WAVE", "", "LEVEL", "", "PULSE WIDTH", "", "", ""}, {"WAVE", "DETUNE", "LEVEL", "", "PULSE WIDTH", "", "", ""},
         {"WAVE", "INTERVAL", "LEVEL", "", "PULSE WIDTH", "FINE", "", ""}, {"NOISE LEVEL", "NOISE COLOR", "", "", "", "", "", ""},
         {"MOD MIX", "OSC MOD", "FILTER MOD", "DECAY", "VCO3 KBD", "VCO3 LOW", "OVERDRIVE", "FILTER KBD"}, {}},
        {{"WAVE", "SUB", "NOISE", "PULSE WIDTH", "CHORUS", "HPF", "RANGE", "VCA GATE"}, {}, {}, {}, {"LFO DELAY", "CHORUS HISS", "", "", "", "", "", ""}, {}},
        {{"FREQ A", "PW A", "SAW A", "PULSE A", "SYNC", "", "", ""}, {"FREQ B", "FINE B", "PW B", "KEYBOARD", "SAW B", "TRI B", "PULSE B", "LOW B"},
         {"LEVEL A", "LEVEL B", "NOISE", "GLIDE", "FILTER KBD", "", "", ""}, {},
         {"FILTER ENV", "OSC B", "FREQ A", "PW A", "FILTER", "", "", ""}, {"SOURCE", "FREQ A", "FREQ B", "PW A", "PW B", "FILTER", "", ""}},
        {{"SHAPE 1", "SUB", "LEVEL 1", "NOISE", "PW 1", "", "FLT MORPH", "BANDPASS"}, {"SHAPE 2", "INTERVAL", "LEVEL 2", "NOISE", "PW 2", "DETUNE", "FLT MORPH", "BANDPASS"}, {}, {},
         {"FILTER ENV", "VCO2 XMOD", "FREQ 1", "FILTER", "PW 1", "SYNC", "", ""}, {}},
        {{"RATIO", "LEVEL", "ALGORITHM", "FEEDBACK", "ATTACK", "DECAY", "SUSTAIN", "RELEASE"},
         {"RATIO", "LEVEL", "ALGORITHM", "FEEDBACK", "ATTACK", "DECAY", "SUSTAIN", "RELEASE"},
         {"RATIO", "LEVEL", "ALGORITHM", "FEEDBACK", "ATTACK", "DECAY", "SUSTAIN", "RELEASE"},
         {"RATIO", "LEVEL", "ALGORITHM", "FEEDBACK", "ATTACK", "DECAY", "SUSTAIN", "RELEASE"},
         {"OP1 WAVE", "OP2 WAVE", "OP3 WAVE", "OP4 WAVE", "OP1 DETUNE", "OP2 DETUNE", "OP3 DETUNE", "OP4 DETUNE"},
         {"KBD LEVEL", "KBD RATE", "D2 RATE", "", "", "", "", ""}},
        {{"WAVE 1", "PULSE WIDTH", "RING", "NOISE", "LEVEL 1", "", "", ""}, {"WAVE 2", "RATIO", "RING", "NOISE", "LEVEL 2", "", "", ""}, {}, {},
         {"MONO", "SYNC", "VCO3 RATE", "VCO3 PITCH", "VCO3 CUTOFF", "NOISE COLOR", "", ""}, {}}
    };
    return engine < 6 && page < 6 && slot < 8 && labels[engine][page][slot] ? labels[engine][page][slot] : "";
}
GF_OscSlot hybrid_osc_descriptor(uint8_t engine, unsigned page, unsigned slot) {
    return page < 4 ? GF_OSC_SLOT[engine][page][slot] : GF_MOD_SLOT[engine][page - 4][slot];
}
float hybrid_osc_normalized(const studio::Patch& patch, uint8_t engine, unsigned page, unsigned slot) {
    const auto d = hybrid_osc_descriptor(engine, page, slot);
    if (!gf_osc_used(&d)) return 0;
    const unsigned stop = d.global ? 0 : (page < 4 ? page : page - 4);
    const float value = page < 4 ? patch.osc[stop][slot] : patch.mod[stop][slot];
    return std::clamp((value - d.lo) / (d.hi - d.lo), 0.0f, 1.0f);
}
juce::String hybrid_fx_id(int fx, int field) { return "hybrid.fx." + juce::String(DX_FX_NAMES[size_t(fx)]) + "." + DX_FX_FIELDS[size_t(field)]; }
juce::String arp2600_fx_id(int fx, int field) { return "arp2600.fx." + juce::String(DX_FX_NAMES[size_t(fx)]) + "." + DX_FX_FIELDS[size_t(field)]; }
juce::String modeld_fx_id(int fx, int field) { return "modeld.fx." + juce::String(DX_FX_NAMES[size_t(fx)]) + "." + DX_FX_FIELDS[size_t(field)]; }
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
juce::String ob6_groove_id(int group, int field) {
    return "ob6.groove." + juce::String(DX_GROOVE_NAMES[size_t(group)]) + "." + (field == 0 ? juce::String("on") : "p" + juce::String(field));
}
juce::String hybrid_groove_id(int group, int field) {
    return "hybrid.groove." + juce::String(DX_GROOVE_NAMES[size_t(group)]) + "." + (field == 0 ? juce::String("on") : "p" + juce::String(field));
}
juce::String arp2600_groove_id(int group, int field) {
    return "arp2600.groove." + juce::String(DX_GROOVE_NAMES[size_t(group)]) + "." + (field == 0 ? juce::String("on") : "p" + juce::String(field));
}
juce::String modeld_groove_id(int group, int field) {
    return "modeld.groove." + juce::String(DX_GROOVE_NAMES[size_t(group)]) + "." + (field == 0 ? juce::String("on") : "p" + juce::String(field));
}
constexpr std::array<const char*, 6> DX_MACRO_IDS{"dx7.macro.grit", "dx7.macro.pocket", "dx7.macro.move",
                                                  "dx7.macro.grit_res", "dx7.macro.grit_mod", "dx7.macro.pocket_vel"};
constexpr std::array<const char*, 6> DX_MACRO_LABELS{"GRIT", "POCKET", "MOVE", "GRIT RES", "GRIT MOD", "POCKET VEL"};
constexpr std::array<const char*, 7> HYBRID_MACRO_IDS{"hybrid.macro.drift", "hybrid.macro.grit", "hybrid.macro.pocket", "hybrid.macro.move", "hybrid.macro.grit_res", "hybrid.macro.grit_mod", "hybrid.macro.pocket_vel"};
constexpr std::array<int, 7> HYBRID_MACRO_SLOTS{GF_MACRO_DRIFT, GF_MACRO_GRIT, GF_MACRO_POCKET, GF_MACRO_MOVE, GF_MACRO_GRIT_RES, GF_MACRO_GRIT_MOD, GF_MACRO_POCKET_VEL};
constexpr std::array<const char*, 6> ARP2600_MACRO_IDS{"arp2600.macro.grit", "arp2600.macro.pocket", "arp2600.macro.move",
    "arp2600.macro.grit_res", "arp2600.macro.grit_mod", "arp2600.macro.pocket_vel"};
constexpr std::array<const char*, 6> MODELD_MACRO_IDS{"modeld.macro.grit", "modeld.macro.pocket", "modeld.macro.move",
    "modeld.macro.grit_res", "modeld.macro.grit_mod", "modeld.macro.pocket_vel"};
constexpr std::array<const char*, 6> OB6_MACRO_IDS{"ob6.macro.grit", "ob6.macro.pocket", "ob6.macro.move",
    "ob6.macro.grit_res", "ob6.macro.grit_mod", "ob6.macro.pocket_vel"};
constexpr std::array<const char*, 6> JUNO_MACRO_IDS{"juno.macro.grit", "juno.macro.pocket", "juno.macro.move",
    "juno.macro.grit_res", "juno.macro.grit_mod", "juno.macro.pocket_vel"};

struct Ob6Control { const char* id; const char* label; bool mod; uint8_t stop, slot; };
constexpr std::array<Ob6Control, 13> ARP2600_OSC{{
    {"arp2600.osc1.wave", "VCO1 WAVE", false, 0, 0}, {"arp2600.osc1.pw", "PULSE WIDTH", false, 0, 1},
    {"arp2600.noise.level", "NOISE LEVEL", false, 0, 3}, {"arp2600.osc1.level", "VCO1 LEVEL", false, 0, 4},
    {"arp2600.osc2.wave", "VCO2 WAVE", false, 1, 0}, {"arp2600.osc2.ratio", "VCO2 RATIO", false, 1, 1},
    {"arp2600.osc2.level", "VCO2 LEVEL", false, 1, 4}, {"arp2600.mono", "MONO", true, 0, 0},
    {"arp2600.sync", "SYNC", true, 0, 1}, {"arp2600.vco3.rate", "VCO3 RATE", true, 0, 2},
    {"arp2600.vco3.pitch", "VCO3 PITCH", true, 0, 3}, {"arp2600.vco3.cutoff", "VCO3 CUTOFF", true, 0, 4},
    {"arp2600.noise.color", "NOISE COLOR", true, 0, 5}
}};
constexpr std::array<Ob6Control, 20> MODELD_OSC{{
    {"modeld.osc1.wave", "VCO1 WAVE", false, 0, 0}, {"modeld.osc1.level", "VCO1 LEVEL", false, 0, 2},
    {"modeld.osc2.wave", "VCO2 WAVE", false, 1, 0}, {"modeld.osc2.level", "VCO2 LEVEL", false, 1, 2},
    {"modeld.osc2.detune", "VCO2 DETUNE", false, 1, 1}, {"modeld.osc3.wave", "VCO3 WAVE", false, 2, 0},
    {"modeld.osc3.level", "VCO3 LEVEL", false, 2, 2}, {"modeld.osc3.interval", "VCO3 INTERVAL", false, 2, 1},
    {"modeld.osc3.detune", "VCO3 DETUNE", false, 2, 5}, {"modeld.pulse_width", "PULSE WIDTH", false, 0, 4},
    {"modeld.noise.level", "NOISE LEVEL", false, 3, 0}, {"modeld.noise.color", "NOISE COLOR", false, 3, 1},
    {"modeld.mod.mix", "MOD MIX", true, 0, 0}, {"modeld.mod.osc", "OSC MOD", true, 0, 1},
    {"modeld.mod.filter", "FILTER MOD", true, 0, 2}, {"modeld.decay_switch", "DECAY SWITCH", true, 0, 3},
    {"modeld.vco3.keyboard", "VCO3 KBD", true, 0, 4}, {"modeld.vco3.low", "VCO3 LOW", true, 0, 5},
    {"modeld.overdrive", "OVERDRIVE", true, 0, 6}, {"modeld.filter.keyboard", "FILTER KBD", true, 0, 7}
}};
constexpr std::array<Ob6Control, 18> OB6_OSC{{
    {"ob6.osc1.shape", "VCO1 SHAPE", false, 0, 0}, {"ob6.sub", "SUB", false, 0, 1},
    {"ob6.osc1.level", "VCO1 LEVEL", false, 0, 2}, {"ob6.noise", "NOISE", false, 0, 3},
    {"ob6.osc1.pw", "VCO1 PW", false, 0, 4}, {"ob6.morph", "FLT MORPH", false, 0, 6},
    {"ob6.bandpass", "BANDPASS", false, 0, 7}, {"ob6.osc2.shape", "VCO2 SHAPE", false, 1, 0},
    {"ob6.osc2.interval", "INTERVAL", false, 1, 1}, {"ob6.osc2.level", "VCO2 LEVEL", false, 1, 2},
    {"ob6.osc2.pw", "VCO2 PW", false, 1, 4}, {"ob6.osc2.detune", "DETUNE", false, 1, 5},
    {"ob6.xmod.env", "XMOD ENV", true, 0, 0}, {"ob6.xmod.vco2", "XMOD VCO2", true, 0, 1},
    {"ob6.xmod.freq", "XMOD FREQ", true, 0, 2}, {"ob6.xmod.morph", "XMOD FILTER", true, 0, 3},
    {"ob6.xmod.pw", "XMOD PW", true, 0, 4}, {"ob6.sync", "SYNC", true, 0, 5}
}};
struct DxGeneral { const char* id; const char* label; float lo, hi, step, initial; };
constexpr std::array<DxGeneral, 20> HYBRID_VOICE{{
    {"decay", "AMP DECAY", 1.5f,12000,0,300}, {"sustain", "AMP SUSTAIN", 0,1,0,.6f},
    {"filter_attack", "FLT ATTACK", 0,12000,0,10}, {"filter_decay", "FLT DECAY", 0,12000,0,300},
    {"filter_sustain", "FLT SUSTAIN", 0,1,0,.6f}, {"filter_release", "FLT RELEASE", 0,12000,0,500},
    {"env_amount", "FLT ENV AMT", 0,1,0,.5f}, {"drive", "FILTER DRIVE", 0,1,0,.25f},
    {"vel_amp", "VEL AMP %", 0,100,0,65}, {"vel_filter", "VEL FILTER %", 0,100,0,50},
    {"lfo_rate", "LFO RATE Hz", .05f,20,0,2}, {"lfo_shape", "LFO SHAPE", 0,4,1,1},
    {"lfo_div", "LFO DIV", 0,3,1,1}, {"lfo_cutoff", "LFO CUTOFF", 0,1,0,0},
    {"lfo_pitch", "LFO PITCH", 0,1,0,0}, {"lfo_pwm", "LFO PWM / AMP", 0,1,0,0},
    {"glide", "GLIDE", 0,1,0,0}, {"unison_on", "UNISON ON", 0,1,1,0},
    {"unison_count", "UNISON COUNT", 0,3,1,0}, {"unison_detune", "UNISON CENTS", 0,50,0,15}
}};
std::array<float, 20> hybrid_voice_values(const studio::Patch& p, uint8_t engine) {
    const uint8_t third = engine == 4 ? GF_DEST_AMP : GF_DEST_PWM;
    return {p.decay_ms, p.sustain, p.f_attack_ms, p.f_decay_ms, p.f_sustain, p.f_release_ms, p.env_amt, p.drive,
        p.vel_amp, p.vel_cut, p.lfo_rate_hz, float(p.lfo_shape), float(p.lfo_div), p.lfo_amt[GF_DEST_CUTOFF],
        p.lfo_amt[GF_DEST_PITCH], p.lfo_amt[third], p.glide, p.groove_on[GF_GROOVE_UNISON] ? 1.0f : 0.0f,
        p.groove[GF_GROOVE_UNISON][GF_UNISON_VOICES], p.groove[GF_GROOVE_UNISON][GF_UNISON_DETUNE]};
}
constexpr std::array<DxGeneral, 21> ARP2600_VOICE{{
    {"arp2600.amp_decay", "AMP DECAY", 1.5f,12000,0,300}, {"arp2600.amp_sustain", "AMP SUSTAIN", 0,1,0,.6f},
    {"arp2600.filter_attack", "FLT ATTACK", 0,12000,0,10}, {"arp2600.filter_decay", "FLT DECAY", 0,12000,0,300},
    {"arp2600.filter_sustain", "FLT SUSTAIN", 0,1,0,.6f}, {"arp2600.filter_release", "FLT RELEASE", 0,12000,0,500},
    {"arp2600.env_amount", "FLT ENV AMT", 0,1,0,.5f}, {"arp2600.drive", "FILTER DRIVE", 0,1,0,.25f},
    {"arp2600.vel_amp", "VEL AMP %", 0,100,0,65}, {"arp2600.vel_filter", "VEL FILTER %", 0,100,0,50},
    {"arp2600.lfo_rate", "LFO RATE Hz", .05f,20,0,2}, {"arp2600.lfo_shape", "LFO SHAPE", 0,4,1,1},
    {"arp2600.lfo_div", "LFO DIV", 0,3,1,1}, {"arp2600.lfo_cutoff", "LFO CUTOFF", 0,1,0,0},
    {"arp2600.lfo_pitch", "LFO PITCH", 0,1,0,0}, {"arp2600.lfo_pwm", "LFO PWM", 0,1,0,0},
    {"arp2600.glide", "GLIDE", 0,1,0,0}, {"arp2600.unison_on", "UNISON ON", 0,1,1,0},
    {"arp2600.unison_count", "UNISON COUNT", 0,3,1,0}, {"arp2600.unison_detune", "UNISON CENTS", 0,50,0,15},
    {"arp2600.drift", "DRIFT %", 0,100,0,0}
}};
constexpr std::array<DxGeneral, 18> MODELD_VOICE{{
    {"modeld.amp_decay", "AMP DECAY", 1.5f,12000,0,300}, {"modeld.amp_sustain", "AMP SUSTAIN", 0,1,0,.7f},
    {"modeld.filter_attack", "FLT ATTACK", 0,12000,0,5}, {"modeld.filter_decay", "FLT DECAY", 0,12000,0,400},
    {"modeld.filter_sustain", "FLT SUSTAIN", 0,1,0,.3f}, {"modeld.filter_release", "FLT RELEASE", 0,12000,0,300},
    {"modeld.env_amount", "FLT ENV AMT", 0,1,0,.3f}, {"modeld.drive", "FILTER DRIVE", 0,1,0,.2f},
    {"modeld.vel_amp", "VEL AMP %", 0,100,0,0}, {"modeld.vel_filter", "VEL FILTER %", 0,100,0,0},
    {"modeld.lfo_rate", "LFO RATE Hz", .05f,20,0,2}, {"modeld.lfo_shape", "LFO SHAPE 0-4", 0,4,1,1},
    {"modeld.lfo_div", "LFO DIV 0-3", 0,3,1,1}, {"modeld.lfo_cutoff", "LFO CUTOFF", 0,1,0,0},
    {"modeld.lfo_pitch", "LFO PITCH", 0,1,0,0}, {"modeld.lfo_pwm", "LFO PWM", 0,1,0,0},
    {"modeld.glide", "GLIDE 0-1", 0,1,0,0}, {"modeld.drift", "DRIFT %", 0,100,0,0}
}};
constexpr std::array<DxGeneral, 21> OB6_VOICE{{
    {"ob6.amp_decay", "AMP DECAY ms", 1.5f,12000,0,300}, {"ob6.amp_sustain", "AMP SUSTAIN", 0,1,0,.7f},
    {"ob6.filter_attack", "FLT ATTACK ms", 0,12000,0,5}, {"ob6.filter_decay", "FLT DECAY ms", 0,12000,0,400},
    {"ob6.filter_sustain", "FLT SUSTAIN", 0,1,0,.3f}, {"ob6.filter_release", "FLT RELEASE ms", 0,12000,0,300},
    {"ob6.env_amount", "FLT ENV AMT", 0,1,0,.3f}, {"ob6.drive", "FILTER DRIVE", 0,1,0,.2f},
    {"ob6.vel_amp", "VEL AMP %", 0,100,0,65}, {"ob6.vel_filter", "VEL FILTER %", 0,100,0,50},
    {"ob6.lfo_rate", "LFO RATE Hz", .05f,20,0,2}, {"ob6.lfo_shape", "LFO SHAPE 0-4", 0,4,1,1},
    {"ob6.lfo_div", "LFO DIV 0-3", 0,3,1,1}, {"ob6.lfo_cutoff", "LFO CUTOFF", 0,1,0,0},
    {"ob6.lfo_pitch", "LFO PITCH", 0,1,0,0}, {"ob6.lfo_pwm", "LFO PWM", 0,1,0,0},
    {"ob6.glide", "GLIDE 0-1", 0,1,0,0}, {"ob6.unison_on", "UNISON ON", 0,1,1,0},
    {"ob6.unison_count", "UNISON COUNT", 0,3,1,0}, {"ob6.unison_detune", "UNISON CENTS", 0,50,0,15},
    {"ob6.drift", "DRIFT %", 0,100,0,0}
}};
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
    for (int group = 0; group < 3; ++group) for (int field = 0; field < JUNO_GROOVE_FIELDS[size_t(group)]; ++field) {
        const auto slot = field == 0 ? GF_OscSlot{0, 1, 1, 2, false} : GF_GROOVE_SLOT[group][field - 1];
        const float initial = field == 0 ? 0.0f : default_patch.groove[group][field - 1];
        parameters.push_back(std::make_unique<AudioParameterFloat>(ParameterID{ob6_groove_id(group, field), 1},
            "OB-6 GROOVE " + String(DX_GROOVE_NAMES[size_t(group)]) + " " + String(field),
            NormalisableRange<float>{slot.lo, slot.hi, slot.count ? 1.0f : 0.0f}, initial));
    }
    for (int fx = 0; fx < 5; ++fx) for (int field = 0; field < 5; ++field) {
        const float initial = field == 0 ? 0.0f : field == 1 ? default_patch.sfx_mix[fx] : default_patch.sfx_p[fx][field - 2];
        parameters.push_back(std::make_unique<AudioParameterFloat>(ParameterID{ob6_fx_id(fx, field), 1},
            "OB-6 FX " + String(DX_FX_NAMES[size_t(fx)]) + " " + DX_FX_LABELS[fx][field],
            NormalisableRange<float>{0, 1, field == 0 ? 1.0f : 0.0f}, initial));
    }
    studio::Patch ob6_defaults;
    studio::patch_init(ob6_defaults, 3);
    for (const auto& control : OB6_OSC) {
        const auto slot = control.mod ? GF_MOD_SLOT[3][control.stop][control.slot] : GF_OSC_SLOT[3][control.stop][control.slot];
        const float initial = control.mod ? ob6_defaults.mod[control.stop][control.slot] : ob6_defaults.osc[control.stop][control.slot];
        parameters.push_back(std::make_unique<AudioParameterFloat>(ParameterID{control.id, 1}, "OB-6 " + String(control.label),
            NormalisableRange<float>{slot.lo, slot.hi, slot.count ? 1.0f : 0.0f}, initial));
    }
    for (const auto& p : OB6_VOICE)
        parameters.push_back(std::make_unique<AudioParameterFloat>(ParameterID{p.id, 1}, "OB-6 " + String(p.label),
            NormalisableRange<float>{p.lo, p.hi, p.step}, p.initial));
    for (size_t i = 0; i < OB6_MACRO_IDS.size(); ++i)
        parameters.push_back(std::make_unique<AudioParameterFloat>(ParameterID{OB6_MACRO_IDS[i], 1},
            "OB-6 " + String(DX_MACRO_LABELS[i]), NormalisableRange<float>{0, 1}, i < 3 ? 0.0f : 1.0f));
    studio::Patch arp_defaults; studio::patch_init(arp_defaults, 5);
    for (const auto& control : ARP2600_OSC) {
        const auto slot = control.mod ? GF_MOD_SLOT[5][control.stop][control.slot] : GF_OSC_SLOT[5][control.stop][control.slot];
        const float value = control.mod ? arp_defaults.mod[control.stop][control.slot] : arp_defaults.osc[control.stop][control.slot];
        parameters.push_back(std::make_unique<AudioParameterFloat>(ParameterID{control.id, 1}, "ARP " + String(control.label),
            NormalisableRange<float>{slot.lo, slot.hi, slot.count ? 1.0f : 0.0f}, value));
    }
    for (int group = 0; group < 3; ++group) for (int field = 0; field < JUNO_GROOVE_FIELDS[size_t(group)]; ++field) {
        const auto slot = field == 0 ? GF_OscSlot{0, 1, 1, 2, false} : GF_GROOVE_SLOT[group][field - 1];
        const float initial = field == 0 ? 0.0f : default_patch.groove[group][field - 1];
        parameters.push_back(std::make_unique<AudioParameterFloat>(ParameterID{arp2600_groove_id(group, field), 1},
            "ARP GROOVE " + String(DX_GROOVE_NAMES[size_t(group)]) + " " + String(field),
            NormalisableRange<float>{slot.lo, slot.hi, slot.count ? 1.0f : 0.0f}, initial));
    }
    for (int fx = 0; fx < 5; ++fx) for (int field = 0; field < 5; ++field) {
        const float initial = field == 0 ? 0.0f : field == 1 ? default_patch.sfx_mix[fx] : default_patch.sfx_p[fx][field - 2];
        parameters.push_back(std::make_unique<AudioParameterFloat>(ParameterID{arp2600_fx_id(fx, field), 1},
            "ARP FX " + String(DX_FX_NAMES[size_t(fx)]) + " " + DX_FX_LABELS[fx][field],
            NormalisableRange<float>{0, 1, field == 0 ? 1.0f : 0.0f}, initial));
    }
    for (size_t i = 0; i < ARP2600_MACRO_IDS.size(); ++i)
        parameters.push_back(std::make_unique<AudioParameterFloat>(ParameterID{ARP2600_MACRO_IDS[i], 1},
            "ARP " + String(DX_MACRO_LABELS[i]), NormalisableRange<float>{0, 1}, i < 3 ? 0.0f : 1.0f));
    for (const auto& p : ARP2600_VOICE)
        parameters.push_back(std::make_unique<AudioParameterFloat>(ParameterID{p.id, 1}, "ARP " + String(p.label),
            NormalisableRange<float>{p.lo, p.hi, p.step}, p.initial));
    studio::LayerPreset hybrid_defaults; studio::factory_layer(0, &hybrid_defaults);
    for (int g = 0; g < 3; ++g) for (int f = 0; f < JUNO_GROOVE_FIELDS[size_t(g)]; ++f) {
        const auto slot = f == 0 ? GF_OscSlot{0, 1, 1, 2, false} : GF_GROOVE_SLOT[g][f - 1];
        const float initial = f == 0 ? (hybrid_defaults.a.patch.groove_on[g] ? 1.0f : 0.0f) : hybrid_defaults.a.patch.groove[g][f - 1];
        parameters.push_back(std::make_unique<AudioParameterFloat>(ParameterID{hybrid_groove_id(g, f), 1}, "Hybrid GROOVE " + String(g) + " " + String(f),
            NormalisableRange<float>{slot.lo, slot.hi, slot.count ? 1.0f : 0.0f}, initial));
    }
    for (int fx = 0; fx < 5; ++fx) for (int f = 0; f < 5; ++f) {
        const auto& p = hybrid_defaults.a.patch;
        const float initial = f == 0 ? (p.sfx_on[fx] ? 1.0f : 0.0f) : f == 1 ? p.sfx_mix[fx] : p.sfx_p[fx][f - 2];
        parameters.push_back(std::make_unique<AudioParameterFloat>(ParameterID{hybrid_fx_id(fx, f), 1}, "Hybrid FX " + String(fx) + " " + String(f),
            NormalisableRange<float>{0, 1, f == 0 ? 1.0f : 0.0f}, initial));
    }
    for (size_t i = 0; i < HYBRID_MACRO_IDS.size(); ++i)
        parameters.push_back(std::make_unique<AudioParameterFloat>(ParameterID{HYBRID_MACRO_IDS[i], 1}, "Hybrid Macro " + String(int(i)),
            NormalisableRange<float>{0, 1}, std::clamp(hybrid_defaults.a.patch.groove[GF_GROOVE_MACRO][HYBRID_MACRO_SLOTS[i]] * .01f, 0.0f, 1.0f)));
    for (unsigned side = 0; side < 2; ++side) for (unsigned page = 0; page < 6; ++page) for (unsigned slot = 0; slot < 8; ++slot) {
        const auto& p = side == 0 ? hybrid_defaults.a : hybrid_defaults.b;
        const auto engine = side == 0 ? hybrid_defaults.eng_a : hybrid_defaults.eng_b;
        const auto id = hybrid_osc_id(side, page, slot);
        parameters.push_back(std::make_unique<AudioParameterFloat>(ParameterID{id, 1}, id, NormalisableRange<float>{0, 1}, hybrid_osc_normalized(p.patch, engine, page, slot)));
    }
    for (int side = 0; side < 2; ++side) {
        const auto& preset = side == 0 ? hybrid_defaults.a : hybrid_defaults.b;
        const auto values = hybrid_voice_values(preset.patch, side == 0 ? hybrid_defaults.eng_a : hybrid_defaults.eng_b);
        for (size_t i = 0; i < HYBRID_VOICE.size(); ++i) {
            const auto& p = HYBRID_VOICE[i]; const String id = String(side == 0 ? "hybrid.a.voice." : "hybrid.b.voice.") + p.id;
            parameters.push_back(std::make_unique<AudioParameterFloat>(ParameterID{id, 1}, id, NormalisableRange<float>{p.lo, p.hi, p.step}, values[i]));
        }
    }
    for (int side = 0; side < 2; ++side) {
        const auto& p = side == 0 ? hybrid_defaults.a : hybrid_defaults.b;
        const String prefix = side == 0 ? "hybrid.a." : "hybrid.b.";
        parameters.push_back(std::make_unique<AudioParameterFloat>(ParameterID{prefix + "cutoff", 1}, prefix + "Cutoff", NormalisableRange<float>{20, 20000}, p.cutoff_hz));
        parameters.push_back(std::make_unique<AudioParameterFloat>(ParameterID{prefix + "resonance", 1}, prefix + "Resonance", NormalisableRange<float>{0, 1}, p.reso_norm));
        parameters.push_back(std::make_unique<AudioParameterFloat>(ParameterID{prefix + "attack", 1}, prefix + "Attack ms", NormalisableRange<float>{0, 12000}, p.patch.attack_ms));
        parameters.push_back(std::make_unique<AudioParameterFloat>(ParameterID{prefix + "release", 1}, prefix + "Release ms", NormalisableRange<float>{1, 12000}, p.patch.release_ms));
    }
    parameters.push_back(std::make_unique<AudioParameterChoice>(ParameterID{"hybrid.focus", 1}, "Hybrid Focus", StringArray{"A", "B"}, 0));
    parameters.push_back(std::make_unique<AudioParameterFloat>(ParameterID{"hybrid.b.move", 1}, "Hybrid B MOVE", NormalisableRange<float>{0, 1}, hybrid_defaults.b.patch.groove[GF_GROOVE_MACRO][GF_MACRO_MOVE] * .01f));
    parameters.push_back(std::make_unique<AudioParameterBool>(ParameterID{"hybrid_preview", 1}, "Hybrid Preview", false));
    parameters.push_back(std::make_unique<AudioParameterFloat>(ParameterID{"hybrid.morph", 1}, "Hybrid Morph", NormalisableRange<float>{0, 1}, .5f));
    parameters.push_back(std::make_unique<AudioParameterFloat>(ParameterID{"hybrid.delay", 1}, "Hybrid Delay B ms", NormalisableRange<float>{0, 255, 1}, 10));
    parameters.push_back(std::make_unique<AudioParameterFloat>(ParameterID{"hybrid.transpose", 1}, "Hybrid Transpose B", NormalisableRange<float>{-24, 24, 1}, 0));
    parameters.push_back(std::make_unique<AudioParameterFloat>(ParameterID{"hybrid.gain_a", 1}, "Hybrid Gain A dB", NormalisableRange<float>{-60, 12, 1}, -6));
    parameters.push_back(std::make_unique<AudioParameterFloat>(ParameterID{"hybrid.gain_b", 1}, "Hybrid Gain B dB", NormalisableRange<float>{-60, 12, 1}, 0));
    parameters.push_back(std::make_unique<AudioParameterBool>(ParameterID{"arp2600_preview", 1}, "ARP 2600 Preview", false));
    parameters.push_back(std::make_unique<AudioParameterFloat>(ParameterID{"arp2600.ring", 1}, "ARP 2600 Ring", NormalisableRange<float>{0, 1}, .2f));
    parameters.push_back(std::make_unique<AudioParameterBool>(ParameterID{"modeld_preview", 1}, "Model D Preview", false));
    studio::Patch modeld_defaults;
    studio::patch_init(modeld_defaults, 0);
    for (const auto& control : MODELD_OSC) {
        const auto slot = control.mod ? GF_MOD_SLOT[0][control.stop][control.slot] : GF_OSC_SLOT[0][control.stop][control.slot];
        const float initial = control.mod ? modeld_defaults.mod[control.stop][control.slot] : modeld_defaults.osc[control.stop][control.slot];
        parameters.push_back(std::make_unique<AudioParameterFloat>(ParameterID{control.id, 1}, "Model D " + String(control.label),
            NormalisableRange<float>{slot.lo, slot.hi, slot.count ? 1.0f : 0.0f}, initial));
    }
    for (const auto& p : MODELD_VOICE)
        parameters.push_back(std::make_unique<AudioParameterFloat>(ParameterID{p.id, 1}, "Model D " + String(p.label),
            NormalisableRange<float>{p.lo, p.hi, p.step}, p.initial));
    for (int group = 0; group < 3; ++group) for (int field = 0; field < JUNO_GROOVE_FIELDS[size_t(group)]; ++field) {
        const auto slot = field == 0 ? GF_OscSlot{0, 1, 1, 2, false} : GF_GROOVE_SLOT[group][field - 1];
        const float initial = field == 0 ? 0.0f : default_patch.groove[group][field - 1];
        parameters.push_back(std::make_unique<AudioParameterFloat>(ParameterID{modeld_groove_id(group, field), 1},
            "Model D GROOVE " + String(DX_GROOVE_NAMES[size_t(group)]) + " " + String(field),
            NormalisableRange<float>{slot.lo, slot.hi, slot.count ? 1.0f : 0.0f}, initial));
    }
    for (int fx = 0; fx < 5; ++fx) for (int field = 0; field < 5; ++field) {
        const float initial = field == 0 ? 0.0f : field == 1 ? default_patch.sfx_mix[fx] : default_patch.sfx_p[fx][field - 2];
        parameters.push_back(std::make_unique<AudioParameterFloat>(ParameterID{modeld_fx_id(fx, field), 1},
            "Model D FX " + String(DX_FX_NAMES[size_t(fx)]) + " " + DX_FX_LABELS[fx][field],
            NormalisableRange<float>{0, 1, field == 0 ? 1.0f : 0.0f}, initial));
    }
    for (size_t i = 0; i < MODELD_MACRO_IDS.size(); ++i)
        parameters.push_back(std::make_unique<AudioParameterFloat>(ParameterID{MODELD_MACRO_IDS[i], 1},
            "Model D " + String(DX_MACRO_LABELS[i]), NormalisableRange<float>{0, 1}, i < 3 ? 0.0f : 1.0f));
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
        modeld_enabled_ = state_.getRawParameterValue("modeld_preview");
        for (int g = 0; g < 3; ++g) for (int f = 0; f < JUNO_GROOVE_FIELDS[size_t(g)]; ++f)
            hybrid_groove_[g][f] = state_.getRawParameterValue(hybrid_groove_id(g, f));
        for (int fx = 0; fx < 5; ++fx) for (int f = 0; f < 5; ++f)
            hybrid_fx_[fx][f] = state_.getRawParameterValue(hybrid_fx_id(fx, f));
        for (size_t i = 0; i < HYBRID_MACRO_IDS.size(); ++i) hybrid_macro_[i] = state_.getRawParameterValue(HYBRID_MACRO_IDS[i]);
        for (size_t i = 0; i < hybrid_macro_parameters_.size(); ++i) hybrid_macro_parameters_[i] = state_.getParameter(HYBRID_MACRO_IDS[i]);
        hybrid_enabled_ = state_.getRawParameterValue("hybrid_preview");
        hybrid_focus_ = state_.getRawParameterValue("hybrid.focus");
        hybrid_b_move_ = state_.getRawParameterValue("hybrid.b.move");
        hybrid_morph_ = state_.getRawParameterValue("hybrid.morph"); hybrid_delay_ = state_.getRawParameterValue("hybrid.delay");
        hybrid_transpose_ = state_.getRawParameterValue("hybrid.transpose");
        for (unsigned side = 0; side < 2; ++side) for (unsigned page = 0; page < 6; ++page) for (unsigned slot = 0; slot < 8; ++slot)
            hybrid_osc_[side][page * 8 + slot] = state_.getRawParameterValue(hybrid_osc_id(side, page, slot));
        for (int side = 0; side < 2; ++side) for (size_t i = 0; i < HYBRID_VOICE.size(); ++i)
            hybrid_voice_[side][i] = state_.getRawParameterValue(juce::String(side == 0 ? "hybrid.a.voice." : "hybrid.b.voice.") + HYBRID_VOICE[i].id);
        constexpr const char* half_names[] = {"cutoff", "resonance", "attack", "release"};
        for (int side = 0; side < 2; ++side) for (int i = 0; i < 4; ++i)
            hybrid_half_[side][i] = state_.getRawParameterValue(juce::String(side == 0 ? "hybrid.a." : "hybrid.b.") + half_names[i]);
        hybrid_gain_a_ = state_.getRawParameterValue("hybrid.gain_a"); hybrid_gain_b_ = state_.getRawParameterValue("hybrid.gain_b");
        for (size_t side = 0; side < 2; ++side) for (size_t kind = 0; kind < 2; ++kind)
            hybrid_filter_parameters_[side * 2 + kind] = state_.getParameter(juce::String(side == 0 ? "hybrid.a." : "hybrid.b.") + (kind == 0 ? "cutoff" : "resonance"));
        arp2600_enabled_ = state_.getRawParameterValue("arp2600_preview");
        arp2600_ring_ = state_.getRawParameterValue("arp2600.ring");
        studio::patch_init(arp2600_base_.patch, 5);
        for (size_t i = 0; i < ARP2600_OSC.size(); ++i) arp2600_osc_[i] = state_.getRawParameterValue(ARP2600_OSC[i].id);
        arp2600_osc_last_.fill(std::numeric_limits<float>::quiet_NaN());
        for (size_t i = 0; i < ARP2600_VOICE.size(); ++i) arp2600_voice_[i] = state_.getRawParameterValue(ARP2600_VOICE[i].id);
        arp2600_voice_last_.fill(std::numeric_limits<float>::quiet_NaN());
        for (size_t i = 0; i < ARP2600_MACRO_IDS.size(); ++i) arp2600_macro_[i] = state_.getRawParameterValue(ARP2600_MACRO_IDS[i]);
        arp2600_macro_last_.fill(std::numeric_limits<float>::quiet_NaN());
        for (int g = 0; g < 3; ++g) for (int f = 0; f < JUNO_GROOVE_FIELDS[size_t(g)]; ++f)
            arp2600_groove_[g][f] = state_.getRawParameterValue(arp2600_groove_id(g, f));
        for (int fx = 0; fx < 5; ++fx) for (int f = 0; f < 5; ++f)
            arp2600_fx_[fx][f] = state_.getRawParameterValue(arp2600_fx_id(fx, f));
        arp2600_groove_last_.fill(std::numeric_limits<float>::quiet_NaN());
        arp2600_fx_last_.fill(std::numeric_limits<float>::quiet_NaN());
        studio::patch_init(modeld_base_.patch, 0);
        for (size_t i = 0; i < MODELD_OSC.size(); ++i) modeld_osc_[i] = state_.getRawParameterValue(MODELD_OSC[i].id);
        modeld_osc_last_.fill(std::numeric_limits<float>::quiet_NaN());
        for (size_t i = 0; i < MODELD_MACRO_IDS.size(); ++i) modeld_macro_[i] = state_.getRawParameterValue(MODELD_MACRO_IDS[i]);
        modeld_macro_last_.fill(std::numeric_limits<float>::quiet_NaN());
        for (int g = 0; g < 3; ++g) for (int f = 0; f < JUNO_GROOVE_FIELDS[size_t(g)]; ++f)
            modeld_groove_[g][f] = state_.getRawParameterValue(modeld_groove_id(g, f));
        for (int fx = 0; fx < 5; ++fx) for (int f = 0; f < 5; ++f)
            modeld_fx_[fx][f] = state_.getRawParameterValue(modeld_fx_id(fx, f));
        modeld_groove_last_.fill(std::numeric_limits<float>::quiet_NaN());
        modeld_fx_last_.fill(std::numeric_limits<float>::quiet_NaN());
        for (size_t i = 0; i < MODELD_VOICE.size(); ++i) modeld_voice_[i] = state_.getRawParameterValue(MODELD_VOICE[i].id);
        modeld_voice_last_.fill(std::numeric_limits<float>::quiet_NaN());
        studio::patch_init(ob6_base_.patch, 3);
        for (size_t i = 0; i < OB6_OSC.size(); ++i) ob6_osc_[i] = state_.getRawParameterValue(OB6_OSC[i].id);
        ob6_osc_last_.fill(std::numeric_limits<float>::quiet_NaN());
        for (size_t i = 0; i < OB6_MACRO_IDS.size(); ++i) ob6_macro_[i] = state_.getRawParameterValue(OB6_MACRO_IDS[i]);
        ob6_macro_last_.fill(std::numeric_limits<float>::quiet_NaN());
        for (size_t i = 0; i < OB6_VOICE.size(); ++i) ob6_voice_[i] = state_.getRawParameterValue(OB6_VOICE[i].id);
        ob6_voice_last_.fill(std::numeric_limits<float>::quiet_NaN());
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
        for (int group = 0; group < 3; ++group) for (int field = 0; field < JUNO_GROOVE_FIELDS[size_t(group)]; ++field)
            ob6_groove_[group][field] = state_.getRawParameterValue(ob6_groove_id(group, field));
        ob6_groove_last_.fill(std::numeric_limits<float>::quiet_NaN());
        for (int fx = 0; fx < 5; ++fx) for (int field = 0; field < 5; ++field)
            ob6_fx_[fx][field] = state_.getRawParameterValue(ob6_fx_id(fx, field));
        ob6_fx_last_.fill(std::numeric_limits<float>::quiet_NaN());
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
        arp2600_cc_parameters_[0] = state_.getParameter("arp2600.drift");
        for (size_t i = 1; i < 4; ++i) arp2600_cc_parameters_[i] = state_.getParameter(ARP2600_MACRO_IDS[i - 1]);
        arp2600_cc_parameters_[4] = state_.getParameter("cutoff"); arp2600_cc_parameters_[5] = state_.getParameter("resonance");
        modeld_cc_parameters_[0] = state_.getParameter("modeld.drift");
        for (size_t i = 1; i < 4; ++i) modeld_cc_parameters_[i] = state_.getParameter(MODELD_MACRO_IDS[i - 1]);
        modeld_cc_parameters_[4] = state_.getParameter("cutoff"); modeld_cc_parameters_[5] = state_.getParameter("resonance");
        ob6_cc_parameters_[0] = state_.getParameter("ob6.drift");
        for (size_t i = 1; i < 4; ++i) ob6_cc_parameters_[i] = state_.getParameter(OB6_MACRO_IDS[i - 1]);
        ob6_cc_parameters_[4] = state_.getParameter("cutoff");
        ob6_cc_parameters_[5] = state_.getParameter("resonance");
        juno_cc_parameters_[0] = state_.getParameter("juno.drift");
        for (size_t i = 1; i < 4; ++i) juno_cc_parameters_[i] = state_.getParameter(JUNO_MACRO_IDS[i - 1]);
        juno_cc_parameters_[4] = state_.getParameter("cutoff"); juno_cc_parameters_[5] = state_.getParameter("resonance");
        cc_parameters_[0] = state_.getParameter("dx7.drift");
        for (size_t i = 1; i < 4; ++i) cc_parameters_[i] = state_.getParameter(DX_MACRO_IDS[i - 1]);
        cc_parameters_[4] = state_.getParameter("dx7.cutoff"); cc_parameters_[5] = state_.getParameter("dx7.resonance");
        prophet_cc_parameters_[0] = state_.getParameter("cutoff"); prophet_cc_parameters_[1] = state_.getParameter("resonance");
        studio::patch_init(prophet_base_.patch, 2);
        dx_macro_last_.fill(std::numeric_limits<float>::quiet_NaN());
        initialize_hybrid_bank(hybrid_bank_, hybrid_bank_count_);
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
    void prepareToPlay(double sample_rate, int maximum_block_size) override {
        sample_rate_ = sample_rate;
        // gemelo-correcciones-2026-11-03 §1: la salida física es acoplada en alterna; el VST no.
        dc_r_ = float(std::exp(-2.0 * juce::MathConstants<double>::pi * 5.0 / (sample_rate > 0.0 ? sample_rate : 44100.0)));
        reset_dc_blocker();
        const size_t preview_capacity = size_t(juce::jmax(4096, maximum_block_size));
        preview_left_.assign(preview_capacity, 0.0f); preview_right_.assign(preview_capacity, 0.0f);
        preview_mix_ = 0.0f;
        preview_sample_rate_.store(sample_rate > 0.0 ? sample_rate : 44100.0, std::memory_order_relaxed);
        if (auto* preview = preview_pointer_.load(std::memory_order_acquire)) preview->prepareToPlay(sample_rate, 512);
        const auto delay_length = size_t(sample_rate > 0.0 ? sample_rate : 44100.0) + 2;
        delay_left_.assign(delay_length, 0);
        delay_right_.assign(delay_length, 0);
        dx_delay_left_.assign(delay_length, 0);
        dx_delay_right_.assign(delay_length, 0);
        juno_delay_left_.assign(delay_length, 0);
        juno_delay_right_.assign(delay_length, 0);
        ob6_delay_left_.assign(delay_length, 0);
        ob6_delay_right_.assign(delay_length, 0);
        modeld_delay_left_.assign(delay_length, 0);
        modeld_delay_right_.assign(delay_length, 0);
        arp2600_delay_left_.assign(delay_length, 0); arp2600_delay_right_.assign(delay_length, 0);
        plate_memory_.assign(gf::PlateReverbPreview::required_samples(float(sample_rate > 0.0 ? sample_rate : 44100.0)), 0);
        dx_plate_memory_.assign(plate_memory_.size(), 0);
        juno_plate_memory_.assign(plate_memory_.size(), 0);
        ob6_plate_memory_.assign(plate_memory_.size(), 0);
        modeld_plate_memory_.assign(plate_memory_.size(), 0);
        arp2600_plate_memory_.assign(plate_memory_.size(), 0);
        engine_.prepare(sample_rate);
        dx_bus_.prepare(sample_rate);
        juno_bus_.prepare(sample_rate);
        ob6_bus_.prepare(sample_rate);
        modeld_bus_.prepare(sample_rate);
        arp2600_bus_.prepare(sample_rate);
        dx7_.prepare(sample_rate);
        juno_.set_parameters(juno_effective_cutoff(), juno_effective_resonance(), attack_->load(), juno_effective_env().release_ms);
        juno_.prepare(sample_rate);
        ob6_.prepare(sample_rate);
        modeld_.prepare(sample_rate);
        arp2600_.prepare(sample_rate);
        hybrid_.prepare(sample_rate);
        studio::LayerPreset default_layer;
        if (studio::factory_layer(0, &default_layer)) hybrid_.load(default_layer);
        hybrid_applied_sequence_ = 0;
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
        ob6_bus_.set_voice_sink(&ob6_,
            [](void* context, uint8_t note, float velocity) { static_cast<gf::Ob6Preview*>(context)->note_on(note, velocity); },
            [](void* context, uint8_t note) { static_cast<gf::Ob6Preview*>(context)->note_off(note); });
        modeld_bus_.set_voice_sink(&modeld_,
            [](void* context, uint8_t note, float velocity) { static_cast<gf::ModelDPreview*>(context)->note_on(note, velocity); },
            [](void* context, uint8_t note) { static_cast<gf::ModelDPreview*>(context)->note_off(note); });
        arp2600_bus_.set_voice_sink(&arp2600_,
            [](void* context, uint8_t note, float velocity) { static_cast<gf::Arp2600Preview*>(context)->note_on(note, velocity); },
            [](void* context, uint8_t note) { static_cast<gf::Arp2600Preview*>(context)->note_off(note); });
        arp2600_bus_.set_delay_memory(arp2600_delay_left_.data(), arp2600_delay_right_.data(), uint32_t(delay_length));
        arp2600_bus_.set_plate_memory(arp2600_plate_memory_.data(), arp2600_plate_memory_.size());
        engine_.set_delay_memory(delay_left_.data(), delay_right_.data(), uint32_t(delay_length));
        engine_.set_plate_memory(plate_memory_.data(), plate_memory_.size());
        dx_bus_.set_delay_memory(dx_delay_left_.data(), dx_delay_right_.data(), uint32_t(delay_length));
        dx_bus_.set_plate_memory(dx_plate_memory_.data(), dx_plate_memory_.size());
        juno_bus_.set_delay_memory(juno_delay_left_.data(), juno_delay_right_.data(), uint32_t(delay_length));
        juno_bus_.set_plate_memory(juno_plate_memory_.data(), juno_plate_memory_.size());
        ob6_bus_.set_delay_memory(ob6_delay_left_.data(), ob6_delay_right_.data(), uint32_t(delay_length));
        ob6_bus_.set_plate_memory(ob6_plate_memory_.data(), ob6_plate_memory_.size());
        modeld_bus_.set_delay_memory(modeld_delay_left_.data(), modeld_delay_right_.data(), uint32_t(delay_length));
        modeld_bus_.set_plate_memory(modeld_plate_memory_.data(), modeld_plate_memory_.size());
        juno_osc_last_.fill(std::numeric_limits<float>::quiet_NaN());
        juno_env_last_.fill(std::numeric_limits<float>::quiet_NaN());
        juno_lfo_last_.fill(std::numeric_limits<float>::quiet_NaN());
        juno_voice_groove_last_.fill(std::numeric_limits<float>::quiet_NaN());
        juno_groove_last_.fill(std::numeric_limits<float>::quiet_NaN());
        juno_fx_last_.fill(std::numeric_limits<float>::quiet_NaN());
        juno_macro_last_.fill(std::numeric_limits<float>::quiet_NaN());
        ob6_groove_last_.fill(std::numeric_limits<float>::quiet_NaN());
        ob6_fx_last_.fill(std::numeric_limits<float>::quiet_NaN());
        ob6_osc_last_.fill(std::numeric_limits<float>::quiet_NaN());
        ob6_voice_last_.fill(std::numeric_limits<float>::quiet_NaN());
        ob6_macro_last_.fill(std::numeric_limits<float>::quiet_NaN());
        modeld_osc_last_.fill(std::numeric_limits<float>::quiet_NaN());
        modeld_voice_last_.fill(std::numeric_limits<float>::quiet_NaN());
        modeld_groove_last_.fill(std::numeric_limits<float>::quiet_NaN());
        modeld_fx_last_.fill(std::numeric_limits<float>::quiet_NaN());
        modeld_macro_last_.fill(std::numeric_limits<float>::quiet_NaN());
        arp2600_osc_last_.fill(std::numeric_limits<float>::quiet_NaN());
        applied_sequence_ = 0;
        last_juno_mode_ = false; last_dx7_mode_ = false; last_ob6_mode_ = false; last_modeld_mode_ = false; last_arp2600_mode_ = false; last_hybrid_mode_ = false;
    }
    void releaseResources() override { engine_.reset(); dx_bus_.reset(); dx7_.reset(); juno_.reset(); juno_bus_.reset(); ob6_.reset(); ob6_bus_.reset(); modeld_.reset(); modeld_bus_.reset(); arp2600_.reset(); arp2600_bus_.reset(); hybrid_.reset(); }
    void reset() override { engine_.reset(); dx_bus_.reset(); dx7_.reset(); juno_.reset(); juno_bus_.reset(); ob6_.reset(); ob6_bus_.reset(); modeld_.reset(); modeld_bus_.reset(); arp2600_.reset(); arp2600_bus_.reset(); hybrid_.reset(); reset_dc_blocker(); }
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override {
        juce::ScopedNoDenormals guard;
        buffer.clear();
        const bool hybrid_mode = hybrid_enabled_->load() >= 0.5f;
        const bool arp2600_mode = !hybrid_mode && arp2600_enabled_->load() >= 0.5f;
        const bool modeld_mode = !hybrid_mode && !arp2600_mode && modeld_enabled_->load() >= 0.5f;
        const bool ob6_mode = !hybrid_mode && !arp2600_mode && !modeld_mode && ob6_enabled_->load() >= 0.5f;
        const bool juno_mode = !hybrid_mode && !arp2600_mode && !modeld_mode && !ob6_mode && juno_enabled_->load() >= 0.5f;
        const bool dx7_mode = !hybrid_mode && !arp2600_mode && !modeld_mode && !ob6_mode && !juno_mode && dx7_enabled_->load() >= 0.5f;
        const bool prophet_mode = !hybrid_mode && !arp2600_mode && !modeld_mode && !ob6_mode && !juno_mode && !dx7_mode;
        if (juno_mode) {
            for (size_t i = 0; i < 2; ++i)
                if (juno_filter_override_[i].load(std::memory_order_acquire) && !juno_filter_override_active(i))
                    juno_filter_override_[i].store(false, std::memory_order_release);
            juno_.set_parameters(juno_effective_cutoff(), juno_effective_resonance(), attack_->load(), juno_effective_env().release_ms);
        }
        if (dx7_mode != last_dx7_mode_ || juno_mode != last_juno_mode_ || ob6_mode != last_ob6_mode_ || modeld_mode != last_modeld_mode_ || arp2600_mode != last_arp2600_mode_ || hybrid_mode != last_hybrid_mode_) { engine_.reset(); dx_bus_.reset(); dx7_.reset(); juno_.reset(); juno_bus_.reset(); ob6_.reset(); ob6_bus_.reset(); modeld_.reset(); modeld_bus_.reset(); last_dx7_mode_ = dx7_mode; last_juno_mode_ = juno_mode; last_ob6_mode_ = ob6_mode; last_modeld_mode_ = modeld_mode; arp2600_.reset(); arp2600_bus_.reset(); last_arp2600_mode_ = arp2600_mode; hybrid_.reset(); last_hybrid_mode_ = hybrid_mode; keyboard_playing_.fill(false); host_note_playing_.fill(false); reset_dc_blocker(); }
        apply_pending_preset();
        apply_pending_hybrid();
        if (hybrid_mode) {
            std::array<float, 21> groove{}; std::array<float, 25> effects{};
            for (int g = 0; g < 3; ++g) for (int f = 0; f < JUNO_GROOVE_FIELDS[size_t(g)]; ++f) groove[size_t(g * 7 + f)] = hybrid_groove_[g][f]->load();
            for (int fx = 0; fx < 5; ++fx) for (int f = 0; f < 5; ++f) effects[size_t(fx * 5 + f)] = hybrid_fx_[fx][f]->load();
            hybrid_.set_bus(groove, effects);
            hybrid_.set_macros(hybrid_macro_values());
            hybrid_.set_b_move(hybrid_b_move_current());
            for (unsigned side = 0; side < 2; ++side) {
                std::array<float, 48> osc{};
                for (size_t i = 0; i < osc.size(); ++i) osc[i] = hybrid_osc_[side][i]->load();
                hybrid_.set_half_osc(side, osc);
                std::array<float, 20> voice{};
                for (size_t i = 0; i < voice.size(); ++i) voice[i] = hybrid_voice_[side][i]->load();
                hybrid_.set_half_voice(side, voice);
            }
            hybrid_.set_layer_controls(hybrid_morph_current(), uint8_t(hybrid_delay_->load()), int8_t(hybrid_transpose_->load()), hybrid_gain_a_->load(), hybrid_gain_b_->load());
            for (unsigned side = 0; side < 2; ++side)
                hybrid_.set_half_controls(side, hybrid_half_current(side, 0), hybrid_half_current(side, 1), hybrid_half_[side][2]->load(), hybrid_half_[side][3]->load());
        }
        if (arp2600_mode) {
            for (size_t i = 0; i < 2; ++i)
                if (arp2600_filter_override_[i].load(std::memory_order_acquire) && !arp2600_filter_override_active(i))
                    arp2600_filter_override_[i].store(false, std::memory_order_release);
            bool changed = false;
            for (size_t i = 0; i < ARP2600_MACRO_IDS.size(); ++i) {
                const float v = arp2600_macro_value(i);
                if (v != arp2600_macro_last_[i]) { arp2600_macro_last_[i] = v; changed = true; }
            }
            if (changed) { arp2600_osc_last_.fill(std::numeric_limits<float>::quiet_NaN()); arp2600_fx_last_.fill(std::numeric_limits<float>::quiet_NaN()); }
            apply_arp2600_osc(); apply_arp2600_bus();
            const auto controls = arp2600_effective_controls(arp2600_base_.patch);
            arp2600_.set_parameters(controls[0], controls[1], attack_->load(), controls[2]); arp2600_.set_ring_mix(arp2600_ring_->load());
        }
        if (modeld_mode) {
            for (size_t i = 0; i < 2; ++i)
                if (modeld_filter_override_[i].load(std::memory_order_acquire) && !modeld_filter_override_active(i))
                    modeld_filter_override_[i].store(false, std::memory_order_release);
            bool macro_changed = false;
            for (size_t i = 0; i < MODELD_MACRO_IDS.size(); ++i) {
                const float value = modeld_macro_value(i);
                if (value != modeld_macro_last_[i]) { modeld_macro_last_[i] = value; macro_changed = true; }
            }
            if (macro_changed) { modeld_osc_last_.fill(std::numeric_limits<float>::quiet_NaN()); modeld_fx_last_.fill(std::numeric_limits<float>::quiet_NaN()); }
            apply_modeld_osc_automation(); apply_modeld_groove_automation(); apply_modeld_fx_automation();
            const auto effective = modeld_effective_controls(modeld_base_.patch);
            modeld_.set_parameters(effective[0], effective[1], attack_->load(), effective[2]);
        }
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
        if (ob6_mode) {
            for (size_t i = 0; i < 2; ++i) {
                if (ob6_filter_override_[i].load(std::memory_order_acquire) && !ob6_filter_override_active(i))
                    ob6_filter_override_[i].store(false, std::memory_order_release);
            }
            bool macro_changed = false;
            for (size_t i = 0; i < OB6_MACRO_IDS.size(); ++i) {
                const float value = ob6_macro_value(i);
                if (value != ob6_macro_last_[i]) { ob6_macro_last_[i] = value; macro_changed = true; }
            }
            if (macro_changed) { ob6_osc_last_.fill(std::numeric_limits<float>::quiet_NaN()); ob6_fx_last_.fill(std::numeric_limits<float>::quiet_NaN()); }
            apply_ob6_osc_automation(); apply_ob6_groove_automation(); apply_ob6_fx_automation();
            const auto effective = ob6_effective_controls(ob6_base_.patch);
            ob6_.set_parameters(effective[0], effective[1], attack_->load(), effective[2]);
        }
        engine_.set_parameters(prophet_cc_value(0, cutoff_->load()), prophet_cc_value(1, resonance_->load()), attack_->load(), release_->load());
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
        modeld_bus_.set_transport(bpm, has_position, position_beats, running);
        arp2600_bus_.set_transport(bpm, has_position, position_beats, running);
        hybrid_.set_transport(bpm, has_position, position_beats, running);
        hybrid_.set_expression(hybrid_wheel_, hybrid_pressure_, bpm, lfo_sync_->load() >= .5f);
        engine_.set_lfo_sync(lfo_sync_->load() >= 0.5f, bpm);
        dx7_.set_lfo_control(dx7_wheel_, bpm, lfo_sync_->load() >= 0.5f);
        juno_.set_lfo_control(juno_wheel_, bpm, lfo_sync_->load() >= 0.5f);
        ob6_.set_lfo_control(ob6_wheel_, bpm, lfo_sync_->load() >= 0.5f, ob6_pressure_);
        modeld_.set_modulation_control(modeld_wheel_, modeld_pressure_, bpm, lfo_sync_->load() >= 0.5f);
        arp2600_.set_lfo_control(arp2600_wheel_, arp2600_pressure_, bpm, lfo_sync_->load() >= 0.5f);
        float meter_left = 0.0f, meter_right = 0.0f;
        auto note_on = [&](uint8_t note, float velocity) {
            if (hybrid_mode) hybrid_.note_on(note, velocity);
            else if (arp2600_mode) arp2600_bus_.note_on(note, velocity);
            else if (modeld_mode) modeld_bus_.note_on(note, velocity);
            else if (ob6_mode) ob6_bus_.note_on(note, velocity);
            else if (juno_mode) juno_bus_.note_on(note, velocity);
            else if (dx7_mode) dx_bus_.note_on(note, velocity);
            else engine_.note_on(note, velocity);
        };
        auto note_off = [&](uint8_t note) {
            if (hybrid_mode) hybrid_.note_off(note);
            else if (arp2600_mode) arp2600_bus_.note_off(note);
            else if (modeld_mode) modeld_bus_.note_off(note);
            else if (ob6_mode) ob6_bus_.note_off(note);
            else if (juno_mode) juno_bus_.note_off(note);
            else if (dx7_mode) dx_bus_.note_off(note);
            else engine_.note_off(note);
        };
        for (size_t note = 0; note < keyboard_desired_.size(); ++note) {
            const bool down = keyboard_desired_[note].load(std::memory_order_relaxed);
            if (down != keyboard_playing_[note]) {
                keyboard_playing_[note] = down;
                if (down) note_on(uint8_t(note), 0.8f);
                else if (!host_note_playing_[note]) note_off(uint8_t(note));
            }
        }
        int position = 0;
        auto render_until = [&](int end) {
            end = juce::jlimit(position, buffer.getNumSamples(), end);
            while (position < end) {
                float left, right;
                if (hybrid_mode) hybrid_.process_frame(left, right);
                else if (arp2600_mode) {
                    arp2600_.set_tape_pitch(arp2600_bus_.tape_pitch_ratio());
                    arp2600_bus_.process_external_frame(arp2600_.process_sample(), left, right);
                }
                else if (modeld_mode) {
                    modeld_.set_tape_pitch(modeld_bus_.tape_pitch_ratio());
                    modeld_bus_.process_external_frame(modeld_.process_sample(), left, right);
                }
                else if (ob6_mode) {
                    ob6_.set_tape_pitch(ob6_bus_.tape_pitch_ratio());
                    ob6_bus_.process_external_frame(ob6_.process_sample(), left, right);
                }
                else if (juno_mode) { float in_l, in_r; juno_.set_tape_pitch(juno_bus_.tape_pitch_ratio()); juno_.process_frame(in_l, in_r); juno_bus_.process_external_stereo(in_l, in_r, left, right); }
                else if (dx7_mode) {
                    dx7_.set_tape_pitch(dx_bus_.tape_pitch_ratio());
                    dx_bus_.process_external_frame(dx7_.process_sample(), left, right);
                }
                else engine_.process_frame(left, right);
                meter_left = std::max(meter_left, std::abs(left));
                meter_right = std::max(meter_right, std::abs(right));
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
                host_note_playing_[size_t(message.getNoteNumber())] = true;
                note_on(uint8_t(message.getNoteNumber()), message.getFloatVelocity());
            } else if (message.isNoteOff()) {
                host_note_playing_[size_t(message.getNoteNumber())] = false;
                if (!keyboard_playing_[size_t(message.getNoteNumber())]) note_off(uint8_t(message.getNoteNumber()));
            } else if (message.isPitchWheel()) {
                if (hybrid_mode) hybrid_.set_pitch_bend(message.getPitchWheelValue());
                else if (arp2600_mode) arp2600_.set_pitch_bend(message.getPitchWheelValue());
                else if (modeld_mode) modeld_.set_pitch_bend(message.getPitchWheelValue());
                else if (ob6_mode) ob6_.set_pitch_bend(message.getPitchWheelValue());
                else if (juno_mode) juno_.set_pitch_bend(message.getPitchWheelValue());
                else if (dx7_mode) dx7_.set_pitch_bend(message.getPitchWheelValue());
                else engine_.set_pitch_bend(message.getPitchWheelValue());
            }
            else if (message.isController() && message.getControllerNumber() == 1) {
                if (hybrid_mode) { hybrid_wheel_ = message.getControllerValue() / 127.0f; hybrid_.set_expression(hybrid_wheel_, hybrid_pressure_, bpm, lfo_sync_->load() >= .5f); }
                else if (arp2600_mode) { arp2600_wheel_ = message.getControllerValue() / 127.0f; arp2600_.set_lfo_control(arp2600_wheel_, arp2600_pressure_, bpm, lfo_sync_->load() >= 0.5f); }
                else if (modeld_mode) { modeld_wheel_ = message.getControllerValue() / 127.0f; modeld_.set_modulation_control(modeld_wheel_, modeld_pressure_, bpm, lfo_sync_->load() >= 0.5f); }
                else if (ob6_mode) { ob6_wheel_ = message.getControllerValue() / 127.0f; ob6_.set_lfo_control(ob6_wheel_, bpm, lfo_sync_->load() >= 0.5f, ob6_pressure_); }
                else if (juno_mode) { juno_wheel_ = message.getControllerValue() / 127.0f; juno_.set_lfo_control(juno_wheel_, bpm, lfo_sync_->load() >= 0.5f); }
                else if (dx7_mode) { dx7_wheel_ = message.getControllerValue() / 127.0f; dx7_.set_lfo_control(dx7_wheel_, bpm, lfo_sync_->load() >= 0.5f); }
                else engine_.setWheel(message.getControllerValue() / 127.0f);
            }
            else if (hybrid_mode && message.isChannelPressure()) {
                hybrid_pressure_ = message.getChannelPressureValue() / 127.0f;
                hybrid_.set_expression(hybrid_wheel_, hybrid_pressure_, bpm, lfo_sync_->load() >= .5f);
            }
            else if (arp2600_mode && message.isChannelPressure()) {
                arp2600_pressure_ = message.getChannelPressureValue() / 127.0f;
                arp2600_.set_lfo_control(arp2600_wheel_, arp2600_pressure_, bpm, lfo_sync_->load() >= 0.5f);
            }
            else if (modeld_mode && message.isChannelPressure()) {
                modeld_pressure_ = message.getChannelPressureValue() / 127.0f;
                modeld_.set_modulation_control(modeld_wheel_, modeld_pressure_, bpm, lfo_sync_->load() >= 0.5f);
            }
            else if (ob6_mode && message.isChannelPressure()) {
                ob6_pressure_ = message.getChannelPressureValue() / 127.0f;
                ob6_.set_lfo_control(ob6_wheel_, bpm, lfo_sync_->load() >= 0.5f, ob6_pressure_);
            }
            else if (message.isController() && message.getControllerNumber() == 64) {
                if (hybrid_mode) hybrid_.set_sustain(message.getControllerValue() >= 64);
                else if (arp2600_mode) arp2600_bus_.set_sustain(message.getControllerValue() >= 64);
                else if (modeld_mode) modeld_bus_.set_sustain(message.getControllerValue() >= 64);
                else if (ob6_mode) ob6_bus_.set_sustain(message.getControllerValue() >= 64);
                else if (juno_mode) juno_bus_.set_sustain(message.getControllerValue() >= 64);
                else if (dx7_mode) dx_bus_.set_sustain(message.getControllerValue() >= 64);
                else engine_.set_sustain(message.getControllerValue() >= 64);
            } else if (hybrid_mode && hybrid_focus_->load() >= .5f && message.isController() && message.getControllerNumber() == 19) {
                if (!hybrid_b_move_pending_active()) hybrid_b_move_origin_.store(hybrid_b_move_->load(), std::memory_order_relaxed);
                const float value = message.getControllerValue() / 127.0f;
                hybrid_b_move_value_.store(value, std::memory_order_relaxed);
                hybrid_b_move_sequence_.fetch_add(1, std::memory_order_release);
                hybrid_.set_b_move(value);
            } else if (hybrid_mode && message.isController() && (message.getControllerNumber() == 14 || message.getControllerNumber() == 15 ||
                       message.getControllerNumber() == 16 || message.getControllerNumber() == 19)) {
                const int cc = message.getControllerNumber();
                const size_t index = cc == 14 ? 0u : cc == 15 ? 1u : cc == 16 ? 2u : 3u;
                if (!hybrid_macro_pending_active(index)) hybrid_macro_origin_[index].store(hybrid_macro_parameters_[index]->getValue(), std::memory_order_relaxed);
                hybrid_macro_values_[index].store(message.getControllerValue() / 127.0f, std::memory_order_relaxed);
                hybrid_macro_sequence_[index].fetch_add(1, std::memory_order_release);
                hybrid_.set_macros(hybrid_macro_values());
            } else if (hybrid_mode && message.isController() && (message.getControllerNumber() == 74 || message.getControllerNumber() == 71)) {
                const unsigned focus = hybrid_focus_->load() >= .5f ? 1u : 0u, other = 1u - focus;
                const unsigned kind = message.getControllerNumber() == 74 ? 0u : 1u;
                const float raw = message.getControllerValue() / 127.0f;
                const float previous = hybrid_half_current(focus, kind);
                const float value = kind == 0 ? gf_cutoff_from_cc_norm(hybrid_.engine_id(focus), raw) : raw;
                const float other_previous = hybrid_half_current(other, kind);
                const auto other_engine = hybrid_.engine_id(other);
                const float followed = kind == 0 ? std::clamp(other_previous * (previous > 0 ? value / previous : 1.0f), GF_CUTOFF_RANGE[other_engine].lo, GF_CUTOFF_RANGE[other_engine].hi)
                    : std::clamp(other_previous + value - previous, 0.0f, 1.0f);
                set_hybrid_filter_pending(focus, kind, value); set_hybrid_filter_pending(other, kind, followed);
                for (unsigned side = 0; side < 2; ++side)
                    hybrid_.set_half_controls(side, hybrid_half_current(side, 0), hybrid_half_current(side, 1), hybrid_half_[side][2]->load(), hybrid_half_[side][3]->load());
            } else if (message.isController() && message.getControllerNumber() == 18) {
                hybrid_bank_value_.store(uint8_t(message.getControllerValue()), std::memory_order_relaxed);
                hybrid_bank_sequence_.fetch_add(1, std::memory_order_release);
            } else if (hybrid_mode && message.isController() && message.getControllerNumber() == 17) {
                const float value = message.getControllerValue() / 127.0f;
                if (!hybrid_morph_pending_active()) hybrid_morph_origin_.store(hybrid_morph_->load(), std::memory_order_relaxed);
                hybrid_morph_value_.store(value, std::memory_order_relaxed);
                hybrid_morph_sequence_.fetch_add(1, std::memory_order_release);
                hybrid_.set_morph(value);
            } else if (arp2600_mode && message.isController() && (message.getControllerNumber() == 74 || message.getControllerNumber() == 71)) {
                const size_t filter = message.getControllerNumber() == 74 ? 0u : 1u, index = filter + 4;
                const float raw = message.getControllerValue() / 127.0f;
                if (arp2600_cc_sequence_[index].load(std::memory_order_acquire) == arp2600_cc_delivered_[index].load(std::memory_order_acquire))
                    arp2600_filter_origin_[filter].store(arp2600_cc_parameters_[index]->getValue(), std::memory_order_relaxed);
                const auto& original = arp2600_base_.patch;
                const float old_move = studio::macro_amount(original, GF_MACRO_MOVE);
                const float baseline = filter == 0
                    ? std::clamp(gf_cutoff_from_cc_norm(5, raw) * studio::move_cutoff_mul(old_move), GF_CUTOFF_RANGE[5].lo, GF_CUTOFF_RANGE[5].hi)
                    : std::clamp(.1f + 4.9f * studio::move_resonance(studio::macro_resonance(raw, original), old_move), .707f, 5.0f);
                const float normalized = arp2600_cc_parameters_[index]->convertTo0to1(baseline);
                arp2600_filter_raw_[filter].store(raw, std::memory_order_relaxed);
                arp2600_filter_expected_[filter].store(normalized, std::memory_order_relaxed);
                arp2600_cc_values_[index].store(normalized, std::memory_order_relaxed);
                arp2600_filter_override_[filter].store(true, std::memory_order_release);
                arp2600_cc_sequence_[index].fetch_add(1, std::memory_order_release);
                const auto controls = arp2600_effective_controls(arp2600_base_.patch);
                arp2600_.set_parameters(controls[0], controls[1], attack_->load(), controls[2]);
            } else if (arp2600_mode && message.isController() && (message.getControllerNumber() == 14 ||
                       message.getControllerNumber() == 15 || message.getControllerNumber() == 16 || message.getControllerNumber() == 19)) {
                const int number = message.getControllerNumber();
                const size_t index = number == 14 ? 0u : number == 15 ? 1u : number == 16 ? 2u : 3u;
                arp2600_cc_values_[index].store(message.getControllerValue() / 127.0f, std::memory_order_relaxed);
                arp2600_cc_sequence_[index].fetch_add(1, std::memory_order_release);
                arp2600_osc_last_.fill(std::numeric_limits<float>::quiet_NaN());
                arp2600_fx_last_.fill(std::numeric_limits<float>::quiet_NaN());
                apply_arp2600_osc(); apply_arp2600_bus();
                const auto controls = arp2600_effective_controls(arp2600_base_.patch);
                arp2600_.set_parameters(controls[0], controls[1], attack_->load(), controls[2]);
            } else if (modeld_mode && message.isController() && (message.getControllerNumber() == 74 || message.getControllerNumber() == 71)) {
                const size_t filter = message.getControllerNumber() == 74 ? 0u : 1u, index = filter + 4;
                const float raw = message.getControllerValue() / 127.0f;
                if (modeld_cc_sequence_[index].load(std::memory_order_acquire) == modeld_cc_delivered_[index].load(std::memory_order_acquire))
                    modeld_filter_origin_[filter].store(modeld_cc_parameters_[index]->getValue(), std::memory_order_relaxed);
                const auto& original = modeld_base_.patch;
                const float old_move = studio::macro_amount(original, GF_MACRO_MOVE);
                const float baseline = filter == 0
                    ? std::clamp(gf_cutoff_from_cc_norm(0, raw) * studio::move_cutoff_mul(old_move), 20.0f, 8000.0f)
                    : std::clamp(.1f + 4.9f * studio::move_resonance(studio::macro_resonance(raw, original), old_move), .707f, 5.0f);
                const float normalized = modeld_cc_parameters_[index]->convertTo0to1(baseline);
                modeld_filter_raw_[filter].store(raw, std::memory_order_relaxed);
                modeld_filter_expected_[filter].store(normalized, std::memory_order_relaxed);
                modeld_cc_values_[index].store(normalized, std::memory_order_relaxed);
                modeld_filter_override_[filter].store(true, std::memory_order_release);
                modeld_cc_sequence_[index].fetch_add(1, std::memory_order_release);
                const auto effective = modeld_effective_controls(modeld_base_.patch);
                modeld_.set_parameters(effective[0], effective[1], attack_->load(), effective[2]);
            } else if (modeld_mode && message.isController() && (message.getControllerNumber() == 14 ||
                       message.getControllerNumber() == 15 || message.getControllerNumber() == 16 || message.getControllerNumber() == 19)) {
                const int number = message.getControllerNumber();
                const size_t index = number == 14 ? 0u : number == 15 ? 1u : number == 16 ? 2u : 3u;
                modeld_cc_values_[index].store(message.getControllerValue() / 127.0f, std::memory_order_relaxed);
                modeld_cc_sequence_[index].fetch_add(1, std::memory_order_release);
                modeld_osc_last_.fill(std::numeric_limits<float>::quiet_NaN());
                modeld_fx_last_.fill(std::numeric_limits<float>::quiet_NaN());
                apply_modeld_osc_automation(); apply_modeld_fx_automation();
                const auto effective = modeld_effective_controls(modeld_base_.patch);
                modeld_.set_parameters(effective[0], effective[1], attack_->load(), effective[2]);
            } else if (ob6_mode && message.isController() && (message.getControllerNumber() == 74 || message.getControllerNumber() == 71)) {
                const size_t filter = message.getControllerNumber() == 74 ? 0u : 1u;
                const size_t index = filter + 4;
                const float raw = message.getControllerValue() / 127.0f;
                if (ob6_cc_sequence_[index].load(std::memory_order_acquire) == ob6_cc_delivered_[index].load(std::memory_order_acquire))
                    ob6_filter_origin_[filter].store(ob6_cc_parameters_[index]->getValue(), std::memory_order_relaxed);
                const auto& original = ob6_base_.patch;
                const float old_move = studio::macro_amount(original, GF_MACRO_MOVE);
                const float baseline = filter == 0
                    ? std::clamp(gf_cutoff_from_cc_norm(3, raw) * studio::move_cutoff_mul(old_move), 20.0f, 20000.0f)
                    : std::clamp(.7f + 4.3f * studio::move_resonance(studio::macro_resonance(raw, original), old_move), .707f, 5.0f);
                const float normalized = ob6_cc_parameters_[index]->convertTo0to1(baseline);
                ob6_filter_raw_[filter].store(raw, std::memory_order_relaxed);
                ob6_filter_expected_[filter].store(normalized, std::memory_order_relaxed);
                ob6_cc_values_[index].store(normalized, std::memory_order_relaxed);
                ob6_filter_override_[filter].store(true, std::memory_order_release);
                ob6_cc_sequence_[index].fetch_add(1, std::memory_order_release);
                const auto effective = ob6_effective_controls(ob6_base_.patch);
                ob6_.set_parameters(effective[0], effective[1], attack_->load(), effective[2]);
            } else if (ob6_mode && message.isController() && (message.getControllerNumber() == 14 ||
                       message.getControllerNumber() == 15 || message.getControllerNumber() == 16 || message.getControllerNumber() == 19)) {
                const int number = message.getControllerNumber();
                const size_t index = number == 14 ? 0u : number == 15 ? 1u : number == 16 ? 2u : 3u;
                ob6_cc_values_[index].store(message.getControllerValue() / 127.0f, std::memory_order_relaxed);
                ob6_cc_sequence_[index].fetch_add(1, std::memory_order_release);
                ob6_osc_last_.fill(std::numeric_limits<float>::quiet_NaN());
                ob6_fx_last_.fill(std::numeric_limits<float>::quiet_NaN());
                apply_ob6_osc_automation(); apply_ob6_fx_automation();
                const auto effective = ob6_effective_controls(ob6_base_.patch);
                ob6_.set_parameters(effective[0], effective[1], attack_->load(), effective[2]);
            } else if (juno_mode && message.isController() && (message.getControllerNumber() == 74 || message.getControllerNumber() == 71)) {
                const size_t filter = message.getControllerNumber() == 74 ? 0u : 1u, index = filter + 4;
                const float raw = message.getControllerValue() / 127.0f;
                if (juno_cc_sequence_[index].load(std::memory_order_acquire) == juno_cc_delivered_[index].load(std::memory_order_acquire))
                    juno_filter_origin_[filter].store(juno_cc_parameters_[index]->getValue(), std::memory_order_relaxed);
                const float baseline = filter == 0 ? gf_cutoff_from_cc_norm(1, raw) : std::clamp(.1f + 4.9f * raw, .707f, 5.0f);
                const float normalized = juno_cc_parameters_[index]->convertTo0to1(baseline);
                juno_filter_raw_[filter].store(raw, std::memory_order_relaxed);
                juno_filter_expected_[filter].store(normalized, std::memory_order_relaxed);
                juno_cc_values_[index].store(normalized, std::memory_order_relaxed);
                juno_filter_override_[filter].store(true, std::memory_order_release);
                juno_cc_sequence_[index].fetch_add(1, std::memory_order_release);
                juno_.set_parameters(juno_effective_cutoff(), juno_effective_resonance(), attack_->load(), juno_effective_env().release_ms);
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
            } else if (prophet_mode && message.isController() && (message.getControllerNumber() == 74 || message.getControllerNumber() == 71)) {
                // Los parámetros del Prophet ya traen MOVE y GRIT del preset (import_preset): el CC se convierte con ellos.
                const bool cutoff = message.getControllerNumber() == 74;
                const float raw = message.getControllerValue() / 127.0f;
                const auto& original = prophet_base_.patch;
                const float move = studio::macro_amount(original, GF_MACRO_MOVE);
                const float value = cutoff
                    ? std::clamp(gf_cutoff_from_cc_norm(2, raw) * studio::move_cutoff_mul(move), GF_CUTOFF_RANGE[2].lo, GF_CUTOFF_RANGE[2].hi)
                    : .1f + 4.9f * std::clamp(studio::move_resonance(studio::macro_resonance(raw, original), move), 0.0f, 1.0f);
                const size_t index = cutoff ? 0u : 1u;
                prophet_cc_values_[index].store(prophet_cc_parameters_[index]->convertTo0to1(value), std::memory_order_relaxed);
                prophet_cc_sequence_[index].fetch_add(1, std::memory_order_release);
                engine_.set_parameters(prophet_cc_value(0, cutoff_->load()), prophet_cc_value(1, resonance_->load()), attack_->load(), release_->load());
            } else if (dx7_mode && message.isController() && (message.getControllerNumber() == 74 || message.getControllerNumber() == 71)) {
                const bool cutoff = message.getControllerNumber() == 74;
                const float raw = message.getControllerValue() / 127.0f;
                const size_t index = cutoff ? 4u : 5u;
                const float value = cutoff ? gf_cutoff_from_cc_norm(4, raw) : .7f + 2.3f * raw;   // GF_CUTOFF_RANGE / RESO_RANGE del DX7
                cc_values_[index].store(cc_parameters_[index]->convertTo0to1(value), std::memory_order_relaxed);
                cc_sequence_[index].fetch_add(1, std::memory_order_release);
                apply_dx7_general(true);
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
            } else if (message.isAllNotesOff() || message.isAllSoundOff()) { engine_.reset(); dx_bus_.reset(); dx7_.reset(); juno_.reset(); juno_bus_.reset(); ob6_.reset(); ob6_bus_.reset(); modeld_.reset(); modeld_bus_.reset(); arp2600_.reset(); arp2600_bus_.reset(); hybrid_.reset(); keyboard_playing_.fill(false); host_note_playing_.fill(false); reset_dc_blocker(); }
        }
        render_until(buffer.getNumSamples());
        // Antes de mezclar la preescucha: esa instancia filtra su propia salida y la memoria del filtro no la arrastra.
        for (int ch = 0; ch < 2; ++ch) {
            auto* data = buffer.getWritePointer(ch);
            float x1 = dc_x1_[size_t(ch)], y1 = dc_y1_[size_t(ch)];
            for (int i = 0; i < buffer.getNumSamples(); ++i) {
                const float x = data[i];
                y1 = x - x1 + dc_r_ * y1;
                x1 = x;
                data[i] = y1;
            }
            dc_x1_[size_t(ch)] = x1; dc_y1_[size_t(ch)] = y1;
        }
        const bool preview_requested = preview_active_.load(std::memory_order_acquire);
        if (preview_requested || preview_mix_ > 0.0f) {
            if (auto* preview = preview_pointer_.load(std::memory_order_acquire)) {
                for (size_t i = 0; i < keyboard_desired_.size(); ++i)
                    preview->set_keyboard_note(int(i), keyboard_desired_[i].load(std::memory_order_relaxed));
                preview->setPlayHead(getPlayHead());
                if (size_t(buffer.getNumSamples()) <= preview_left_.size()) {
                    float* channels[] = {preview_left_.data(), preview_right_.data()};
                    juce::AudioBuffer<float> preview_buffer(channels, 2, buffer.getNumSamples());
                    preview->processBlock(preview_buffer, midi);
                    const float step = 1.0f / juce::jmax(1.0f, float(sample_rate_) * .010f);
                    auto* left = buffer.getWritePointer(0); auto* right = buffer.getWritePointer(1);
                    meter_left = meter_right = 0.0f;
                    for (int i = 0; i < buffer.getNumSamples(); ++i) {
                        preview_mix_ = juce::jlimit(0.0f, 1.0f, preview_mix_ + (preview_requested ? step : -step));
                        left[i] += (preview_left_[size_t(i)] - left[i]) * preview_mix_;
                        right[i] += (preview_right_[size_t(i)] - right[i]) * preview_mix_;
                        meter_left = std::max(meter_left, std::abs(left[i])); meter_right = std::max(meter_right, std::abs(right[i]));
                    }
                } else {
                    preview_mix_ = preview_requested ? 1.0f : 0.0f;
                    if (preview_requested) {
                        preview->processBlock(buffer, midi);
                        meter_left = buffer.getMagnitude(0, 0, buffer.getNumSamples());
                        meter_right = buffer.getMagnitude(1, 0, buffer.getNumSamples());
                    }
                }
            }
        }
        meter_state_.publish(meter_left, meter_right);
    }
    /** Audition a factory preset or project layer without changing this processor's state. */
    bool preview_bank_item(int engine, unsigned index) {
        preview_active_.store(false, std::memory_order_release);
        if (engine < 0 || engine > 6 || index >= 256) return false;
        juce::MemoryBlock data;
        if (engine == 6) {
            std::lock_guard<std::mutex> guard(preset_mutex_);
            if (index >= hybrid_bank_count_) return false;
            data = hybrid_bank_[index];
        } else {
            studio::Preset preset; std::array<uint8_t, studio::PRESET_BYTES> bytes{};
            if (!studio::factory_preset(uint8_t(engine), uint8_t(index), &preset)) return false;
            const auto size = studio::preset_encode(preset, uint8_t(engine), bytes.data(), bytes.size());
            if (!size) return false;
            data.replaceAll(bytes.data(), size);
        }
        if (!preview_processor_) {
            preview_processor_ = std::make_unique<BrainPrototype>();
            preview_processor_->prepareToPlay(preview_sample_rate_.load(std::memory_order_relaxed), 512);
            preview_pointer_.store(preview_processor_.get(), std::memory_order_release);
        }
        if (!preview_processor_->import_preset(data.getData(), data.getSize())) return false;
        preview_active_.store(true, std::memory_order_release);
        return true;
    }
    /** Stop auditioning and return output to the original processor. */
    void stop_preset_preview() noexcept { preview_active_.store(false, std::memory_order_release); }
    /** Provide the editor with the final-output meter state. */
    OutputMeterState& output_meter_state() noexcept { return meter_state_; }
    /** Set a screen-key state for consumption at the next audio block. */
    void set_keyboard_note(int note, bool down) noexcept {
        if (note >= 0 && note < 128) keyboard_desired_[size_t(note)].store(down, std::memory_order_relaxed);
    }
    /** Release screen-key notes when its editor closes. */
    void clear_keyboard_notes() noexcept { for (auto& value : keyboard_desired_) value.store(false, std::memory_order_relaxed); }
    void getStateInformation(juce::MemoryBlock& data) override {
        std::lock_guard<std::mutex> guard(preset_mutex_);
        auto snapshot = state_.copyState();
        for (size_t i = 0; i < cc_parameters_.size(); ++i)
            if (cc_sequence_[i].load(std::memory_order_acquire) != cc_delivered_[i].load(std::memory_order_acquire))
                snapshot.setProperty("dxPendingCC" + juce::String(int(i)), cc_values_[i].load(std::memory_order_relaxed), nullptr);
        for (size_t i = 0; i < prophet_cc_parameters_.size(); ++i)
            if (prophet_cc_sequence_[i].load(std::memory_order_acquire) != prophet_cc_delivered_[i].load(std::memory_order_acquire))
                snapshot.setProperty("prophetPendingCC" + juce::String(int(i)), prophet_cc_values_[i].load(std::memory_order_relaxed), nullptr);
        for (size_t i = 0; i < juno_cc_parameters_.size(); ++i)
            if (juno_cc_sequence_[i].load(std::memory_order_acquire) != juno_cc_delivered_[i].load(std::memory_order_acquire) &&
                (i < 4 || juno_filter_override_active(i - 4)))
                snapshot.setProperty("junoPendingCC" + juce::String(int(i)), juno_cc_values_[i].load(std::memory_order_relaxed), nullptr);
        for (size_t i = 0; i < 2; ++i) {
            snapshot.setProperty("junoFilterOverride" + juce::String(int(i)), juno_filter_override_active(i), nullptr);
            snapshot.setProperty("junoFilterRaw" + juce::String(int(i)), juno_filter_raw_[i].load(std::memory_order_relaxed), nullptr);
            snapshot.setProperty("junoFilterExpected" + juce::String(int(i)), juno_filter_expected_[i].load(std::memory_order_relaxed), nullptr);
        }
        for (size_t i = 0; i < ob6_cc_parameters_.size(); ++i)
            if (ob6_cc_sequence_[i].load(std::memory_order_acquire) != ob6_cc_delivered_[i].load(std::memory_order_acquire) &&
                (i < 4 || ob6_filter_override_active(i - 4)))
                snapshot.setProperty("ob6PendingCC" + juce::String(int(i)), ob6_cc_values_[i].load(std::memory_order_relaxed), nullptr);
        for (size_t i = 0; i < 2; ++i) {
            snapshot.setProperty("ob6FilterOverride" + juce::String(int(i)), ob6_filter_override_active(i), nullptr);
            snapshot.setProperty("ob6FilterRaw" + juce::String(int(i)), ob6_filter_raw_[i].load(std::memory_order_relaxed), nullptr);
            snapshot.setProperty("ob6FilterExpected" + juce::String(int(i)), ob6_filter_expected_[i].load(std::memory_order_relaxed), nullptr);
        }
        for (size_t i = 0; i < arp2600_cc_parameters_.size(); ++i)
            if (arp2600_cc_sequence_[i].load(std::memory_order_acquire) != arp2600_cc_delivered_[i].load(std::memory_order_acquire) &&
                (i < 4 || arp2600_filter_override_active(i - 4)))
                snapshot.setProperty("arp2600PendingCC" + juce::String(int(i)), arp2600_cc_values_[i].load(std::memory_order_relaxed), nullptr);
        for (size_t i = 0; i < 2; ++i) {
            snapshot.setProperty("arp2600FilterOverride" + juce::String(int(i)), arp2600_filter_override_active(i), nullptr);
            snapshot.setProperty("arp2600FilterRaw" + juce::String(int(i)), arp2600_filter_raw_[i].load(std::memory_order_relaxed), nullptr);
            snapshot.setProperty("arp2600FilterExpected" + juce::String(int(i)), arp2600_filter_expected_[i].load(std::memory_order_relaxed), nullptr);
        }
        for (size_t i = 0; i < modeld_cc_parameters_.size(); ++i)
            if (modeld_cc_sequence_[i].load(std::memory_order_acquire) != modeld_cc_delivered_[i].load(std::memory_order_acquire) &&
                (i < 4 || modeld_filter_override_active(i - 4)))
                snapshot.setProperty("modeldPendingCC" + juce::String(int(i)), modeld_cc_values_[i].load(std::memory_order_relaxed), nullptr);
        for (size_t i = 0; i < 2; ++i) {
            snapshot.setProperty("modeldFilterOverride" + juce::String(int(i)), modeld_filter_override_active(i), nullptr);
            snapshot.setProperty("modeldFilterRaw" + juce::String(int(i)), modeld_filter_raw_[i].load(std::memory_order_relaxed), nullptr);
            snapshot.setProperty("modeldFilterExpected" + juce::String(int(i)), modeld_filter_expected_[i].load(std::memory_order_relaxed), nullptr);
        }
        snapshot.setProperty("dx7ParamVersion", 6, nullptr);
        snapshot.setProperty("junoOscVersion", 1, nullptr);
        snapshot.setProperty("junoEnvVersion", 1, nullptr);
        snapshot.setProperty("junoLfoVersion", 1, nullptr);
        snapshot.setProperty("junoVoiceGrooveVersion", 1, nullptr);
        snapshot.setProperty("junoGrooveVersion", 2, nullptr);
        snapshot.setProperty("junoFxVersion", 1, nullptr);
        snapshot.setProperty("junoMacroVersion", 1, nullptr);
        snapshot.setProperty("ob6GrooveVersion", 1, nullptr);
        snapshot.setProperty("ob6FxVersion", 1, nullptr);
        snapshot.setProperty("ob6OscVersion", 1, nullptr);
        snapshot.setProperty("ob6VoiceVersion", 1, nullptr);
        snapshot.setProperty("ob6MacroVersion", 1, nullptr);
        snapshot.setProperty("arp2600MacroVersion", 1, nullptr);
        snapshot.setProperty("arp2600BusVersion", 1, nullptr);
        snapshot.setProperty("arp2600VoiceVersion", 1, nullptr);
        snapshot.setProperty("arp2600OscVersion", 1, nullptr);
        snapshot.setProperty("modeldOscVersion", 1, nullptr);
        snapshot.setProperty("modeldVoiceVersion", 1, nullptr);
        snapshot.setProperty("modeldBusVersion", 1, nullptr);
        snapshot.setProperty("modeldMacroVersion", 1, nullptr);
        if (preset_raw_.getSize() > 0) snapshot.setProperty("gfPreset", preset_raw_.toBase64Encoding(), nullptr);
        if (hybrid_raw_.getSize() > 0) snapshot.setProperty("gfLayer", hybrid_raw_.toBase64Encoding(), nullptr);
        snapshot.setProperty("gfLayerBankCount", int(hybrid_bank_count_), nullptr);
        for (size_t i = 0; i < hybrid_bank_count_; ++i)
            snapshot.setProperty("gfLayerBank" + juce::String(int(i)), hybrid_bank_[i].toBase64Encoding(), nullptr);
        snapshot.setProperty("hybridOscVersion", 1, nullptr);
        snapshot.setProperty("hybridVoiceVersion", 1, nullptr);
        snapshot.setProperty("hybridBusVersion", 1, nullptr);
        snapshot.setProperty("hybridBMoveVersion", 1, nullptr);
        snapshot.setProperty("hybridMacroVersion", 1, nullptr);
        if (hybrid_b_move_pending_active()) snapshot.setProperty("hybridPendingBMove", hybrid_b_move_value_.load(std::memory_order_relaxed), nullptr);
        for (size_t i = 0; i < hybrid_macro_parameters_.size(); ++i)
            if (hybrid_macro_pending_active(i)) snapshot.setProperty("hybridPendingMacro" + juce::String(int(i)), hybrid_macro_values_[i].load(std::memory_order_relaxed), nullptr);
        for (size_t i = 0; i < hybrid_filter_parameters_.size(); ++i)
            if (hybrid_filter_pending_active(i)) snapshot.setProperty("hybridPendingFilter" + juce::String(int(i)), hybrid_filter_values_[i].load(std::memory_order_relaxed), nullptr);
        snapshot.setProperty("hybridGainVersion", 1, nullptr);
        snapshot.setProperty("hybridHalfVersion", 1, nullptr);
        if (hybrid_morph_pending_active())
            snapshot.setProperty("hybridPendingMorph", hybrid_morph_value_.load(std::memory_order_relaxed), nullptr);
        if (hybrid_bank_sequence_.load(std::memory_order_acquire) != hybrid_bank_delivered_.load(std::memory_order_acquire))
            snapshot.setProperty("hybridPendingLayer", int(hybrid_bank_value_.load(std::memory_order_relaxed)), nullptr);
        if (auto xml = snapshot.createXml()) copyXmlToBinary(*xml, data);
    }
    void setStateInformation(const void* data, int size) override {
        auto xml = getXmlFromBinary(data, size);
        if (!xml || !xml->hasTagName(state_.state.getType())) return;
        stop_preset_preview();
        auto tree = juce::ValueTree::fromXml(*xml);
        std::array<float, 2> prophet_pending_cc{};
        std::array<bool, 2> has_prophet_pending_cc{};
        for (size_t i = 0; i < prophet_pending_cc.size(); ++i) {
            const auto id = "prophetPendingCC" + juce::String(int(i));
            has_prophet_pending_cc[i] = tree.hasProperty(id);
            if (has_prophet_pending_cc[i]) { prophet_pending_cc[i] = float(tree.getProperty(id)); tree.removeProperty(id, nullptr); }
        }
        std::array<float, 6> pending_cc{};
        std::array<bool, 6> has_pending_cc{};
        for (size_t i = 0; i < pending_cc.size(); ++i) {
            const auto id = "dxPendingCC" + juce::String(int(i));
            has_pending_cc[i] = tree.hasProperty(id);
            if (has_pending_cc[i]) { pending_cc[i] = float(tree.getProperty(id)); tree.removeProperty(id, nullptr); }
        }
        std::array<float, 6> juno_pending_cc{};
        std::array<bool, 6> has_juno_pending_cc{};
        for (size_t i = 0; i < juno_pending_cc.size(); ++i) {
            const auto id = "junoPendingCC" + juce::String(int(i));
            has_juno_pending_cc[i] = tree.hasProperty(id);
            if (has_juno_pending_cc[i]) { juno_pending_cc[i] = float(tree.getProperty(id)); tree.removeProperty(id, nullptr); }
        }
        std::array<bool, 2> juno_filter_override{};
        std::array<float, 2> juno_filter_raw{}, juno_filter_expected{};
        for (size_t i = 0; i < 2; ++i) {
            const auto suffix = juce::String(int(i));
            juno_filter_override[i] = bool(tree.getProperty("junoFilterOverride" + suffix, false));
            juno_filter_raw[i] = float(tree.getProperty("junoFilterRaw" + suffix, 0.0f));
            juno_filter_expected[i] = float(tree.getProperty("junoFilterExpected" + suffix, 0.0f));
            tree.removeProperty("junoFilterOverride" + suffix, nullptr);
            tree.removeProperty("junoFilterRaw" + suffix, nullptr);
            tree.removeProperty("junoFilterExpected" + suffix, nullptr);
        }
        std::array<float, 6> ob6_pending_cc{};
        std::array<bool, 6> has_ob6_pending_cc{};
        for (size_t i = 0; i < ob6_pending_cc.size(); ++i) {
            const auto id = "ob6PendingCC" + juce::String(int(i));
            has_ob6_pending_cc[i] = tree.hasProperty(id);
            if (has_ob6_pending_cc[i]) { ob6_pending_cc[i] = float(tree.getProperty(id)); tree.removeProperty(id, nullptr); }
        }
        std::array<bool, 2> filter_override{};
        std::array<float, 2> filter_raw{}, filter_expected{};
        for (size_t i = 0; i < 2; ++i) {
            const auto suffix = juce::String(int(i));
            filter_override[i] = bool(tree.getProperty("ob6FilterOverride" + suffix, false));
            filter_raw[i] = float(tree.getProperty("ob6FilterRaw" + suffix, 0.0f));
            filter_expected[i] = float(tree.getProperty("ob6FilterExpected" + suffix, 0.0f));
            tree.removeProperty("ob6FilterOverride" + suffix, nullptr);
            tree.removeProperty("ob6FilterRaw" + suffix, nullptr);
            tree.removeProperty("ob6FilterExpected" + suffix, nullptr);
        }
        std::array<float, 6> arp2600_pending_cc{};
        std::array<bool, 6> has_arp2600_pending_cc{};
        for (size_t i = 0; i < arp2600_pending_cc.size(); ++i) {
            const auto id = "arp2600PendingCC" + juce::String(int(i));
            has_arp2600_pending_cc[i] = tree.hasProperty(id);
            if (has_arp2600_pending_cc[i]) { arp2600_pending_cc[i] = float(tree.getProperty(id)); tree.removeProperty(id, nullptr); }
        }
        std::array<bool, 2> arp2600_filter_override{};
        std::array<float, 2> arp2600_filter_raw{}, arp2600_filter_expected{};
        for (size_t i = 0; i < 2; ++i) {
            const auto suffix = juce::String(int(i));
            arp2600_filter_override[i] = bool(tree.getProperty("arp2600FilterOverride" + suffix, false));
            arp2600_filter_raw[i] = float(tree.getProperty("arp2600FilterRaw" + suffix, 0.0f));
            arp2600_filter_expected[i] = float(tree.getProperty("arp2600FilterExpected" + suffix, 0.0f));
            tree.removeProperty("arp2600FilterOverride" + suffix, nullptr);
            tree.removeProperty("arp2600FilterRaw" + suffix, nullptr);
            tree.removeProperty("arp2600FilterExpected" + suffix, nullptr);
        }
        std::array<float, 6> modeld_pending_cc{};
        std::array<bool, 6> has_modeld_pending_cc{};
        for (size_t i = 0; i < modeld_pending_cc.size(); ++i) {
            const auto id = "modeldPendingCC" + juce::String(int(i));
            has_modeld_pending_cc[i] = tree.hasProperty(id);
            if (has_modeld_pending_cc[i]) { modeld_pending_cc[i] = float(tree.getProperty(id)); tree.removeProperty(id, nullptr); }
        }
        std::array<bool, 2> modeld_filter_override{};
        std::array<float, 2> modeld_filter_raw{}, modeld_filter_expected{};
        for (size_t i = 0; i < 2; ++i) {
            const auto suffix = juce::String(int(i));
            modeld_filter_override[i] = bool(tree.getProperty("modeldFilterOverride" + suffix, false));
            modeld_filter_raw[i] = float(tree.getProperty("modeldFilterRaw" + suffix, 0.0f));
            modeld_filter_expected[i] = float(tree.getProperty("modeldFilterExpected" + suffix, 0.0f));
            tree.removeProperty("modeldFilterOverride" + suffix, nullptr);
            tree.removeProperty("modeldFilterRaw" + suffix, nullptr);
            tree.removeProperty("modeldFilterExpected" + suffix, nullptr);
        }
        const int modeld_macro_version = int(tree.getProperty("modeldMacroVersion", 0));
        tree.removeProperty("modeldMacroVersion", nullptr);
        const int modeld_bus_version = int(tree.getProperty("modeldBusVersion", 0));
        tree.removeProperty("modeldBusVersion", nullptr);
        const int modeld_voice_version = int(tree.getProperty("modeldVoiceVersion", 0));
        tree.removeProperty("modeldVoiceVersion", nullptr);
        const int arp2600_macro_version = int(tree.getProperty("arp2600MacroVersion", 0));
        tree.removeProperty("arp2600MacroVersion", nullptr);
        const int arp2600_bus_version = int(tree.getProperty("arp2600BusVersion", 0));
        tree.removeProperty("arp2600BusVersion", nullptr);
        const int arp2600_voice_version = int(tree.getProperty("arp2600VoiceVersion", 0));
        tree.removeProperty("arp2600VoiceVersion", nullptr);
        const int arp2600_osc_version = int(tree.getProperty("arp2600OscVersion", 0));
        tree.removeProperty("arp2600OscVersion", nullptr);
        const int modeld_osc_version = int(tree.getProperty("modeldOscVersion", 0));
        tree.removeProperty("modeldOscVersion", nullptr);
        const int ob6_macro_version = int(tree.getProperty("ob6MacroVersion", 0));
        tree.removeProperty("ob6MacroVersion", nullptr);
        const int ob6_voice_version = int(tree.getProperty("ob6VoiceVersion", 0));
        tree.removeProperty("ob6VoiceVersion", nullptr);
        const int ob6_osc_version = int(tree.getProperty("ob6OscVersion", 0));
        tree.removeProperty("ob6OscVersion", nullptr);
        const int ob6_fx_version = int(tree.getProperty("ob6FxVersion", 0));
        tree.removeProperty("ob6FxVersion", nullptr);
        const int ob6_groove_version = int(tree.getProperty("ob6GrooveVersion", 0));
        tree.removeProperty("ob6GrooveVersion", nullptr);
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
        std::array<juce::MemoryBlock, studio::LAYER_MAX> layer_bank;
        uint8_t layer_bank_count = 0;
        if (tree.hasProperty("gfLayerBankCount")) {
            const int count = int(tree.getProperty("gfLayerBankCount"));
            if (count < 1 || count > studio::LAYER_MAX) return;
            layer_bank_count = uint8_t(count); tree.removeProperty("gfLayerBankCount", nullptr);
            for (int i = 0; i < count; ++i) {
                const auto id = "gfLayerBank" + juce::String(i); studio::LayerPreset check;
                if (!tree.hasProperty(id) || !layer_bank[size_t(i)].fromBase64Encoding(tree.getProperty(id).toString()) ||
                    !gf::decode_hybrid_layer(static_cast<const uint8_t*>(layer_bank[size_t(i)].getData()), layer_bank[size_t(i)].getSize(), check)) return;
                tree.removeProperty(id, nullptr);
            }
        } else initialize_hybrid_bank(layer_bank, layer_bank_count);
        juce::MemoryBlock layer_raw;
        studio::LayerPreset decoded_layer;
        if (tree.hasProperty("gfLayer")) {
            if (!layer_raw.fromBase64Encoding(tree.getProperty("gfLayer").toString()) ||
                !gf::decode_hybrid_layer(static_cast<const uint8_t*>(layer_raw.getData()), layer_raw.getSize(), decoded_layer)) return;
            tree.removeProperty("gfLayer", nullptr);
        }
        const int hybrid_osc_version = int(tree.getProperty("hybridOscVersion", 0));
        tree.removeProperty("hybridOscVersion", nullptr);
        const int hybrid_voice_version = int(tree.getProperty("hybridVoiceVersion", 0));
        tree.removeProperty("hybridVoiceVersion", nullptr);
        const int hybrid_bus_version = int(tree.getProperty("hybridBusVersion", 0));
        tree.removeProperty("hybridBusVersion", nullptr);
        const int hybrid_b_move_version = int(tree.getProperty("hybridBMoveVersion", 0));
        tree.removeProperty("hybridBMoveVersion", nullptr);
        const bool has_hybrid_b_move = tree.hasProperty("hybridPendingBMove");
        const float pending_hybrid_b_move = float(tree.getProperty("hybridPendingBMove", 0.0f));
        tree.removeProperty("hybridPendingBMove", nullptr);
        const int hybrid_macro_version = int(tree.getProperty("hybridMacroVersion", 0));
        tree.removeProperty("hybridMacroVersion", nullptr);
        std::array<float, 4> hybrid_pending_macro{};
        std::array<bool, 4> has_hybrid_pending_macro{};
        for (size_t i = 0; i < hybrid_pending_macro.size(); ++i) {
            const auto id = "hybridPendingMacro" + juce::String(int(i));
            has_hybrid_pending_macro[i] = tree.hasProperty(id);
            if (has_hybrid_pending_macro[i]) { hybrid_pending_macro[i] = float(tree.getProperty(id)); tree.removeProperty(id, nullptr); }
        }
        const int hybrid_half_version = int(tree.getProperty("hybridHalfVersion", 0));
        tree.removeProperty("hybridHalfVersion", nullptr);
        const int hybrid_gain_version = int(tree.getProperty("hybridGainVersion", 0));
        tree.removeProperty("hybridGainVersion", nullptr);
        std::array<float, 4> hybrid_pending_filter{};
        std::array<bool, 4> has_hybrid_pending_filter{};
        for (size_t i = 0; i < hybrid_pending_filter.size(); ++i) {
            const auto id = "hybridPendingFilter" + juce::String(int(i));
            has_hybrid_pending_filter[i] = tree.hasProperty(id);
            if (has_hybrid_pending_filter[i]) { hybrid_pending_filter[i] = float(tree.getProperty(id)); tree.removeProperty(id, nullptr); }
        }
        const bool has_hybrid_morph = tree.hasProperty("hybridPendingMorph");
        const float pending_hybrid_morph = float(tree.getProperty("hybridPendingMorph", .5f));
        tree.removeProperty("hybridPendingMorph", nullptr);
        const bool has_hybrid_bank = tree.hasProperty("hybridPendingLayer");
        const int pending_hybrid_bank = int(tree.getProperty("hybridPendingLayer", 0));
        tree.removeProperty("hybridPendingLayer", nullptr);
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
        if (layer_raw.getSize() && hybrid_osc_version < 1) { set_hybrid_osc_parameters(decoded_layer.a.patch, decoded_layer.eng_a, 0); set_hybrid_osc_parameters(decoded_layer.b.patch, decoded_layer.eng_b, 1); }
        if (layer_raw.getSize() && hybrid_voice_version < 1) { set_hybrid_voice_parameters(decoded_layer.a.patch, decoded_layer.eng_a, 0); set_hybrid_voice_parameters(decoded_layer.b.patch, decoded_layer.eng_b, 1); }
        if (layer_raw.getSize() && hybrid_b_move_version < 1) state_.getParameter("hybrid.b.move")->setValueNotifyingHost(std::clamp(decoded_layer.b.patch.groove[GF_GROOVE_MACRO][GF_MACRO_MOVE] * .01f, 0.0f, 1.0f));
        if (layer_raw.getSize() && hybrid_bus_version < 1) set_hybrid_bus_parameters(decoded_layer.a.patch);
        if (layer_raw.getSize() && hybrid_macro_version < 1) set_hybrid_macro_parameters(decoded_layer.a.patch);
        if (layer_raw.getSize() && hybrid_half_version < 1) {
            set_hybrid_half_parameters(decoded_layer.a, 0); set_hybrid_half_parameters(decoded_layer.b, 1);
        }
        if (layer_raw.getSize() && hybrid_gain_version < 1) {
            auto* ga = state_.getParameter("hybrid.gain_a"); auto* gb = state_.getParameter("hybrid.gain_b");
            ga->setValueNotifyingHost(ga->convertTo0to1(float(decoded_layer.gain_a_db)));
            gb->setValueNotifyingHost(gb->convertTo0to1(float(decoded_layer.gain_b_db)));
        }
        if (raw.getSize() && engine == 5 && arp2600_macro_version < 1) set_arp2600_macro_parameters(decoded);
        if (raw.getSize() && engine == 5 && arp2600_bus_version < 1) set_arp2600_bus_parameters(decoded);
        if (raw.getSize() && engine == 5 && arp2600_voice_version < 1) set_arp2600_voice_parameters(decoded);
        if (raw.getSize() && engine == 5 && arp2600_osc_version < 1) set_arp2600_osc_parameters(decoded);
        if (raw.getSize() && engine == 0 && modeld_macro_version < 1) set_modeld_macro_parameters(decoded);
        if (raw.getSize() && engine == 0 && modeld_bus_version < 1) set_modeld_bus_parameters(decoded);
        if (raw.getSize() && engine == 0 && modeld_voice_version < 1) set_modeld_voice_parameters(decoded);
        if (raw.getSize() && engine == 0 && modeld_osc_version < 1) set_modeld_osc_parameters(decoded);
        if (raw.getSize() && engine == 3 && ob6_macro_version < 1) set_ob6_macro_parameters(decoded);
        if (raw.getSize() && engine == 3 && ob6_voice_version < 1) set_ob6_voice_parameters(decoded);
        if (raw.getSize() && engine == 3 && ob6_osc_version < 1) set_ob6_osc_parameters(decoded);
        if (raw.getSize() && engine == 3 && ob6_fx_version < 1) set_ob6_fx_parameters(decoded);
        if (raw.getSize() && engine == 3 && ob6_groove_version < 1) set_ob6_groove_parameters(decoded);
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
        for (size_t i = 0; i < prophet_pending_cc.size(); ++i)
            if (has_prophet_pending_cc[i]) prophet_cc_parameters_[i]->setValueNotifyingHost(std::clamp(prophet_pending_cc[i], 0.0f, 1.0f));
        for (size_t i = 0; i < juno_pending_cc.size(); ++i)
            if (has_juno_pending_cc[i]) juno_cc_parameters_[i]->setValueNotifyingHost(std::clamp(juno_pending_cc[i], 0.0f, 1.0f));
        for (size_t i = 0; i < 2; ++i) {
            juno_filter_raw_[i].store(std::clamp(juno_filter_raw[i], 0.0f, 1.0f), std::memory_order_relaxed);
            juno_filter_expected_[i].store(std::clamp(juno_filter_expected[i], 0.0f, 1.0f), std::memory_order_relaxed);
            juno_filter_override_[i].store(juno_filter_override[i], std::memory_order_release);
        }
        for (size_t i = 0; i < ob6_pending_cc.size(); ++i)
            if (has_ob6_pending_cc[i]) ob6_cc_parameters_[i]->setValueNotifyingHost(std::clamp(ob6_pending_cc[i], 0.0f, 1.0f));
        for (size_t i = 0; i < 2; ++i) {
            ob6_filter_raw_[i].store(std::clamp(filter_raw[i], 0.0f, 1.0f), std::memory_order_relaxed);
            ob6_filter_expected_[i].store(std::clamp(filter_expected[i], 0.0f, 1.0f), std::memory_order_relaxed);
            ob6_filter_override_[i].store(filter_override[i], std::memory_order_release);
        }
        for (size_t i = 0; i < arp2600_pending_cc.size(); ++i)
            if (has_arp2600_pending_cc[i]) arp2600_cc_parameters_[i]->setValueNotifyingHost(std::clamp(arp2600_pending_cc[i], 0.0f, 1.0f));
        for (size_t i = 0; i < 2; ++i) {
            arp2600_filter_raw_[i].store(std::clamp(arp2600_filter_raw[i], 0.0f, 1.0f), std::memory_order_relaxed);
            arp2600_filter_expected_[i].store(std::clamp(arp2600_filter_expected[i], 0.0f, 1.0f), std::memory_order_relaxed);
            arp2600_filter_override_[i].store(arp2600_filter_override[i], std::memory_order_release);
        }
        for (size_t i = 0; i < modeld_pending_cc.size(); ++i)
            if (has_modeld_pending_cc[i]) modeld_cc_parameters_[i]->setValueNotifyingHost(std::clamp(modeld_pending_cc[i], 0.0f, 1.0f));
        for (size_t i = 0; i < 2; ++i) {
            modeld_filter_raw_[i].store(std::clamp(modeld_filter_raw[i], 0.0f, 1.0f), std::memory_order_relaxed);
            modeld_filter_expected_[i].store(std::clamp(modeld_filter_expected[i], 0.0f, 1.0f), std::memory_order_relaxed);
            modeld_filter_override_[i].store(modeld_filter_override[i], std::memory_order_release);
        }
        hybrid_b_move_delivered_.store(hybrid_b_move_sequence_.load(std::memory_order_acquire), std::memory_order_release);
        if (has_hybrid_b_move) state_.getParameter("hybrid.b.move")->setValueNotifyingHost(std::clamp(pending_hybrid_b_move, 0.0f, 1.0f));
        for (size_t i = 0; i < hybrid_pending_macro.size(); ++i)
            if (has_hybrid_pending_macro[i]) hybrid_macro_parameters_[i]->setValueNotifyingHost(std::clamp(hybrid_pending_macro[i], 0.0f, 1.0f));
        for (size_t i = 0; i < hybrid_pending_filter.size(); ++i)
            if (has_hybrid_pending_filter[i]) hybrid_filter_parameters_[i]->setValueNotifyingHost(std::clamp(hybrid_pending_filter[i], 0.0f, 1.0f));
        hybrid_bank_delivered_.store(hybrid_bank_sequence_.load(std::memory_order_acquire), std::memory_order_release);
        if (has_hybrid_bank) { hybrid_bank_value_.store(uint8_t(std::clamp(pending_hybrid_bank, 0, 127)), std::memory_order_relaxed); hybrid_bank_sequence_.fetch_add(1, std::memory_order_release); }
        hybrid_morph_delivered_.store(hybrid_morph_sequence_.load(std::memory_order_acquire), std::memory_order_release);
        if (has_hybrid_morph) state_.getParameter("hybrid.morph")->setValueNotifyingHost(std::clamp(pending_hybrid_morph, 0.0f, 1.0f));
        hybrid_bank_ = std::move(layer_bank); hybrid_bank_count_ = layer_bank_count;
        hybrid_bank_revision_.fetch_add(1, std::memory_order_release);
        hybrid_raw_ = layer_raw; publish_hybrid(hybrid_raw_);
        hybrid_revision_.fetch_add(1, std::memory_order_release);
        preset_raw_ = raw;
        preset_name_ = raw.getSize() ? juce::String::fromUTF8(decoded.name) : juce::String();
        publish_preset(raw);
    }
    bool import_preset(const void* data, size_t size) override {
        if (data && size >= 3 && std::memcmp(data, "GFL", 3) == 0) return import_hybrid(data, size);
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
        if (engine == 0 || engine == 2 || engine == 1 || engine == 3 || engine == 5) {
            set("cutoff", cutoff);
            set("resonance", std::clamp(engine == 3 ? .7f + reso * 4.3f : 0.1f + reso * 4.9f, 0.707f, 5.0f));
            set("attack", patch.attack_ms);
            set("release", patch.release_ms);
        }
        if (engine == 5) { set_arp2600_osc_parameters(preset); set_arp2600_voice_parameters(preset); set_arp2600_bus_parameters(preset); set_arp2600_macro_parameters(preset); }
        if (engine == 5) set("arp2600.ring", std::clamp(patch.osc[0][2] * .01f, 0.0f, 1.0f));
        if (engine == 0) { set_modeld_osc_parameters(preset); set_modeld_voice_parameters(preset); set_modeld_bus_parameters(preset); set_modeld_macro_parameters(preset); }
        if (engine == 1) { set_juno_osc_parameters(preset); set_juno_env_parameters(preset); set_juno_lfo_parameters(preset); set_juno_voice_groove_parameters(preset); set_juno_groove_parameters(preset); set_juno_fx_parameters(preset); set_juno_macro_parameters(preset); }
        if (engine == 3) { set_ob6_macro_parameters(preset); set_ob6_voice_parameters(preset); set_ob6_osc_parameters(preset); set_ob6_groove_parameters(preset); set_ob6_fx_parameters(preset); }
        if (engine == 4) {
            set_dx7_parameters(gf::make_dx7_operator_patch(preset));
            set_dx7_general_parameters(preset, 0);
            set_dx7_fx_parameters(preset);
            set_dx7_groove_parameters(preset);
            set_dx7_macro_parameters(preset);
        }
        set("hybrid_preview", 0.0f);
        set("arp2600_preview", engine == 5 ? 1.0f : 0.0f);
        set("modeld_preview", engine == 0 ? 1.0f : 0.0f);
        set("ob6_preview", engine == 3 ? 1.0f : 0.0f);
        set("juno_preview", engine == 1 ? 1.0f : 0.0f);
        set("dx7_preview", engine == 4 ? 1.0f : 0.0f);
        std::lock_guard<std::mutex> guard(preset_mutex_);
        preset_raw_.replaceAll(data, size);
        preset_name_ = juce::String::fromUTF8(preset.name);
        publish_preset(preset_raw_);
        return true;
    }
    bool import_hybrid(const void* data, size_t size) {
        studio::LayerPreset layer;
        if (!gf::decode_hybrid_layer(static_cast<const uint8_t*>(data), size, layer)) return false;
        auto set = [&](const char* id, float value) { auto* p = state_.getParameter(id); p->setValueNotifyingHost(p->convertTo0to1(value)); };
        hybrid_morph_delivered_.store(hybrid_morph_sequence_.load(std::memory_order_acquire), std::memory_order_release);
        for (size_t i = 0; i < hybrid_filter_parameters_.size(); ++i) hybrid_filter_delivered_[i].store(hybrid_filter_sequence_[i].load(std::memory_order_acquire), std::memory_order_release);
        for (size_t i = 0; i < hybrid_macro_parameters_.size(); ++i) hybrid_macro_delivered_[i].store(hybrid_macro_sequence_[i].load(std::memory_order_acquire), std::memory_order_release);
        hybrid_b_move_delivered_.store(hybrid_b_move_sequence_.load(std::memory_order_acquire), std::memory_order_release);
        set("hybrid.b.move", layer.b.patch.groove[GF_GROOVE_MACRO][GF_MACRO_MOVE] * .01f);
        set_hybrid_macro_parameters(layer.a.patch); set_hybrid_bus_parameters(layer.a.patch);
        set_hybrid_half_parameters(layer.a, 0); set_hybrid_half_parameters(layer.b, 1);
        set_hybrid_voice_parameters(layer.a.patch, layer.eng_a, 0); set_hybrid_voice_parameters(layer.b.patch, layer.eng_b, 1);
        set_hybrid_osc_parameters(layer.a.patch, layer.eng_a, 0); set_hybrid_osc_parameters(layer.b.patch, layer.eng_b, 1);
        set("hybrid.morph", layer.morph * .01f); set("hybrid.delay", float(layer.delay_b_ms)); set("hybrid.transpose", float(layer.transpose_b));
        set("hybrid.gain_a", float(layer.gain_a_db)); set("hybrid.gain_b", float(layer.gain_b_db));
        set("hybrid_preview", 1);
        std::lock_guard<std::mutex> guard(preset_mutex_);
        hybrid_raw_.replaceAll(data, size); publish_hybrid(hybrid_raw_);
        hybrid_revision_.fetch_add(1, std::memory_order_release);
        return true;
    }
    void set_hybrid_osc_parameters(const studio::Patch& patch, uint8_t engine, unsigned side) {
        for (unsigned page = 0; page < 6; ++page) for (unsigned slot = 0; slot < 8; ++slot)
            state_.getParameter(hybrid_osc_id(side, page, slot))->setValueNotifyingHost(hybrid_osc_normalized(patch, engine, page, slot));
    }
    void write_hybrid_osc_parameters(studio::Patch& patch, uint8_t engine, unsigned side) const {
        for (unsigned page = 0; page < 6; ++page) for (unsigned slot = 0; slot < 8; ++slot) {
            const auto desc = hybrid_osc_descriptor(engine, page, slot); const unsigned stop = page < 4 ? page : page - 4;
            if (!gf_osc_used(&desc) || (desc.global && stop != 0)) continue;
            float value = desc.lo + std::clamp(hybrid_osc_[side][page * 8 + slot]->load(), 0.0f, 1.0f) * (desc.hi - desc.lo);
            if (desc.count) value = std::round(value);
            (page < 4 ? patch.osc[stop][slot] : patch.mod[stop][slot]) = value;
        }
    }
    void set_hybrid_voice_parameters(const studio::Patch& patch, uint8_t engine, unsigned side) {
        const auto values = hybrid_voice_values(patch, engine);
        for (size_t i = 0; i < HYBRID_VOICE.size(); ++i) {
            auto* p = state_.getParameter(juce::String(side == 0 ? "hybrid.a.voice." : "hybrid.b.voice.") + HYBRID_VOICE[i].id);
            p->setValueNotifyingHost(p->convertTo0to1(values[i]));
        }
    }
    void write_hybrid_voice_parameters(studio::Patch& p, uint8_t engine, unsigned side) const {
        auto value = [&](size_t i) { return hybrid_voice_[side][i]->load(); };
        p.decay_ms = value(0); p.sustain = value(1); p.f_attack_ms = value(2); p.f_decay_ms = value(3);
        p.f_sustain = value(4); p.f_release_ms = value(5); p.env_amt = value(6); p.drive = value(7);
        p.vel_amp = value(8); p.vel_cut = value(9); p.lfo_rate_hz = value(10); p.lfo_shape = uint8_t(value(11));
        p.lfo_div = uint8_t(value(12)); p.lfo_amt[GF_DEST_CUTOFF] = value(13); p.lfo_amt[GF_DEST_PITCH] = value(14);
        p.lfo_amt[engine == 4 ? GF_DEST_AMP : GF_DEST_PWM] = value(15); p.glide = value(16);
        p.groove_on[GF_GROOVE_UNISON] = value(17) >= .5f; p.groove[GF_GROOVE_UNISON][GF_UNISON_VOICES] = value(18);
        p.groove[GF_GROOVE_UNISON][GF_UNISON_DETUNE] = value(19);
    }
    void set_hybrid_bus_parameters(const studio::Patch& patch) {
        for (int g = 0; g < 3; ++g) for (int f = 0; f < JUNO_GROOVE_FIELDS[size_t(g)]; ++f) {
            auto* p = state_.getParameter(hybrid_groove_id(g, f));
            const float value = f == 0 ? (patch.groove_on[g] ? 1.0f : 0.0f) : patch.groove[g][f - 1];
            p->setValueNotifyingHost(p->convertTo0to1(value));
        }
        for (int fx = 0; fx < 5; ++fx) for (int f = 0; f < 5; ++f) {
            auto* p = state_.getParameter(hybrid_fx_id(fx, f));
            const float value = f == 0 ? (patch.sfx_on[fx] ? 1.0f : 0.0f) : f == 1 ? patch.sfx_mix[fx] : patch.sfx_p[fx][f - 2];
            p->setValueNotifyingHost(p->convertTo0to1(value));
        }
    }
    void write_hybrid_bus(studio::Patch& patch) const {
        for (int g = 0; g < 3; ++g) {
            patch.groove_on[g] = hybrid_groove_[g][0]->load() >= .5f;
            for (int f = 1; f < JUNO_GROOVE_FIELDS[size_t(g)]; ++f) patch.groove[g][f - 1] = hybrid_groove_[g][f]->load();
        }
        for (int fx = 0; fx < 5; ++fx) {
            patch.sfx_on[fx] = hybrid_fx_[fx][0]->load() >= .5f; patch.sfx_mix[fx] = hybrid_fx_[fx][1]->load();
            for (int f = 0; f < 3; ++f) patch.sfx_p[fx][f] = hybrid_fx_[fx][f + 2]->load();
        }
    }
    void set_hybrid_macro_parameters(const studio::Patch& patch) {
        for (size_t i = 0; i < HYBRID_MACRO_IDS.size(); ++i)
            state_.getParameter(HYBRID_MACRO_IDS[i])->setValueNotifyingHost(std::clamp(patch.groove[GF_GROOVE_MACRO][HYBRID_MACRO_SLOTS[i]] * .01f, 0.0f, 1.0f));
    }
    void write_hybrid_macros(studio::Patch& patch) const {
        for (size_t i = 0; i < HYBRID_MACRO_IDS.size(); ++i)
            patch.groove[GF_GROOVE_MACRO][HYBRID_MACRO_SLOTS[i]] = (i < 4 ? hybrid_macro_current(i) : hybrid_macro_[i]->load()) * 100;
    }
    void set_hybrid_half_parameters(const studio::Preset& preset, unsigned side) {
        const float values[] = {preset.cutoff_hz, preset.reso_norm, preset.patch.attack_ms, preset.patch.release_ms};
        constexpr const char* names[] = {"cutoff", "resonance", "attack", "release"};
        for (int i = 0; i < 4; ++i) {
            auto* parameter = state_.getParameter(juce::String(side == 0 ? "hybrid.a." : "hybrid.b.") + names[i]);
            parameter->setValueNotifyingHost(parameter->convertTo0to1(values[i]));
        }
    }
    void write_hybrid_half_parameters(studio::Preset& preset, unsigned side) const {
        preset.cutoff_hz = hybrid_half_current(side, 0); preset.reso_norm = hybrid_half_current(side, 1);
        preset.patch.attack_ms = hybrid_half_[side][2]->load(); preset.patch.release_ms = hybrid_half_[side][3]->load();
    }
    static void initialize_hybrid_bank(std::array<juce::MemoryBlock, studio::LAYER_MAX>& bank, uint8_t& count) {
        count = 0;
        for (uint8_t i = 0; i < studio::factory_layer_count(); ++i) {
            studio::LayerPreset layer; std::array<uint8_t, studio::LAYER_BYTES> bytes{};
            if (!studio::factory_layer(i, &layer) || !gf::encode_hybrid_layer(layer, bytes.data(), bytes.size())) break;
            bank[count++].replaceAll(bytes.data(), bytes.size());
        }
    }
    bool load_hybrid_bank(unsigned index) {
        juce::MemoryBlock bytes;
        {
            std::lock_guard<std::mutex> guard(preset_mutex_);
            if (index >= hybrid_bank_count_) return false;
            bytes = hybrid_bank_[index];
        }
        return import_hybrid(bytes.getData(), bytes.getSize());
    }
    bool save_hybrid_bank() {
        std::vector<uint8_t> encoded;
        if (!export_hybrid_preset(encoded)) return false;
        studio::LayerPreset layer;
        if (!gf::decode_hybrid_layer(encoded.data(), encoded.size(), layer)) return false;
        std::lock_guard<std::mutex> guard(preset_mutex_);
        if (hybrid_bank_count_ >= studio::LAYER_MAX) return false;
        std::snprintf(layer.name, sizeof(layer.name), "LAYER %02u", unsigned(hybrid_bank_count_ + 1));
        if (!gf::encode_hybrid_layer(layer, encoded.data(), encoded.size())) return false;
        hybrid_bank_[hybrid_bank_count_++].replaceAll(encoded.data(), encoded.size());
        hybrid_bank_revision_.fetch_add(1, std::memory_order_release); return true;
    }
    juce::StringArray hybrid_bank_names() {
        juce::StringArray names;
        std::lock_guard<std::mutex> guard(preset_mutex_);
        for (size_t i = 0; i < hybrid_bank_count_; ++i) {
            studio::LayerPreset layer;
            if (gf::decode_hybrid_layer(static_cast<const uint8_t*>(hybrid_bank_[i].getData()), hybrid_bank_[i].getSize(), layer))
                names.add(juce::String::fromUTF8(layer.name));
        }
        return names;
    }
    uint64_t hybrid_bank_revision() const { return hybrid_bank_revision_.load(std::memory_order_acquire); }
    uint8_t hybrid_engine_id(unsigned side) {
        studio::LayerPreset layer;
        std::lock_guard<std::mutex> guard(preset_mutex_);
        if (hybrid_raw_.getSize()) {
            if (!gf::decode_hybrid_layer(static_cast<const uint8_t*>(hybrid_raw_.getData()), hybrid_raw_.getSize(), layer)) return 0;
        } else if (!studio::factory_layer(0, &layer)) return 0;
        return side == 0 ? layer.eng_a : layer.eng_b;
    }
    uint64_t hybrid_revision() const { return hybrid_revision_.load(std::memory_order_acquire); }
    juce::String hybrid_description() {
        studio::LayerPreset layer;
        std::lock_guard<std::mutex> guard(preset_mutex_);
        if (hybrid_raw_.getSize()) {
            if (!gf::decode_hybrid_layer(static_cast<const uint8_t*>(hybrid_raw_.getData()), hybrid_raw_.getSize(), layer)) return "HYBRID";
        } else if (!studio::factory_layer(0, &layer)) return "HYBRID";
        constexpr const char* names[] = {"MODEL D", "JUNO", "PROPHET", "OB-6", "DX7", "ARP 2600"};
        return juce::String::fromUTF8(layer.name) + " · A " + names[layer.eng_a] + " / B " + names[layer.eng_b];
    }
    bool import_hybrid_half(const void* data, size_t size, unsigned side) override {
        if (side > 1 || !data) return false;
        studio::Preset preset; uint8_t engine = 255;
        if (!gf::decode_supported_preset(static_cast<const uint8_t*>(data), size, preset, engine)) return false;
        studio::LayerPreset layer;
        {
            std::lock_guard<std::mutex> guard(preset_mutex_);
            if (hybrid_raw_.getSize()) {
                if (!gf::decode_hybrid_layer(static_cast<const uint8_t*>(hybrid_raw_.getData()), hybrid_raw_.getSize(), layer)) return false;
            } else if (!studio::factory_layer(0, &layer)) return false;
        }
        if (engine == (side == 0 ? layer.eng_b : layer.eng_a)) return false;
        write_hybrid_half_parameters(layer.a, 0); write_hybrid_half_parameters(layer.b, 1); write_hybrid_voice_parameters(layer.a.patch, layer.eng_a, 0); write_hybrid_voice_parameters(layer.b.patch, layer.eng_b, 1); write_hybrid_osc_parameters(layer.a.patch, layer.eng_a, 0); write_hybrid_osc_parameters(layer.b.patch, layer.eng_b, 1); write_hybrid_macros(layer.a.patch); write_hybrid_bus(layer.a.patch); layer.b.patch.groove[GF_GROOVE_MACRO][GF_MACRO_MOVE] = hybrid_b_move_current() * 100;
        if (side == 0) { layer.eng_a = engine; layer.a = preset; } else { layer.eng_b = engine; layer.b = preset; }
        layer.morph = uint8_t(std::clamp(int(std::lround(hybrid_morph_current() * 100)), 0, 100));
        layer.delay_b_ms = uint8_t(std::clamp(int(std::lround(hybrid_delay_->load())), 0, 255));
        layer.transpose_b = int8_t(std::clamp(int(std::lround(hybrid_transpose_->load())), -24, 24));
        layer.gain_a_db = int8_t(std::clamp(int(std::lround(hybrid_gain_a_->load())), -60, 12));
        layer.gain_b_db = int8_t(std::clamp(int(std::lround(hybrid_gain_b_->load())), -60, 12));
        std::array<uint8_t, studio::LAYER_BYTES> bytes{};
        return gf::encode_hybrid_layer(layer, bytes.data(), bytes.size()) && import_hybrid(bytes.data(), bytes.size());
    }
    bool export_hybrid_preset(std::vector<uint8_t>& data) override {
        if (hybrid_enabled_->load() < .5f || lfo_sync_->load() >= .5f) return false;
        studio::LayerPreset layer;
        {
            std::lock_guard<std::mutex> guard(preset_mutex_);
            if (hybrid_raw_.getSize()) {
                if (!gf::decode_hybrid_layer(static_cast<const uint8_t*>(hybrid_raw_.getData()), hybrid_raw_.getSize(), layer)) return false;
            } else if (!studio::factory_layer(0, &layer)) return false;
        }
        write_hybrid_half_parameters(layer.a, 0); write_hybrid_half_parameters(layer.b, 1); write_hybrid_voice_parameters(layer.a.patch, layer.eng_a, 0); write_hybrid_voice_parameters(layer.b.patch, layer.eng_b, 1); write_hybrid_osc_parameters(layer.a.patch, layer.eng_a, 0); write_hybrid_osc_parameters(layer.b.patch, layer.eng_b, 1); write_hybrid_macros(layer.a.patch); write_hybrid_bus(layer.a.patch); layer.b.patch.groove[GF_GROOVE_MACRO][GF_MACRO_MOVE] = hybrid_b_move_current() * 100;
        layer.morph = uint8_t(std::clamp(int(std::lround(hybrid_morph_current() * 100)), 0, 100));
        layer.delay_b_ms = uint8_t(std::clamp(int(std::lround(hybrid_delay_->load())), 0, 255));
        layer.transpose_b = int8_t(std::clamp(int(std::lround(hybrid_transpose_->load())), -24, 24));
        layer.gain_a_db = int8_t(std::clamp(int(std::lround(hybrid_gain_a_->load())), -60, 12));
        layer.gain_b_db = int8_t(std::clamp(int(std::lround(hybrid_gain_b_->load())), -60, 12));
        std::vector<uint8_t> encoded(studio::LAYER_BYTES);
        if (!gf::encode_hybrid_layer(layer, encoded.data(), encoded.size())) return false;
        data = std::move(encoded); return true;
    }
    bool export_arp2600_preset(std::vector<uint8_t>& data) override {
        if (arp2600_enabled_->load() < .5f || lfo_sync_->load() >= .5f) return false;
        studio::Preset preset;
        {
            std::lock_guard<std::mutex> guard(preset_mutex_);
            uint8_t source = 99;
            if (!preset_raw_.getSize() || !gf::decode_supported_preset(static_cast<const uint8_t*>(preset_raw_.getData()),
                    preset_raw_.getSize(), preset, source) || source != 5) return false;
        }
        const auto controls = arp2600_effective_controls(preset.patch);
        write_arp2600_osc(preset.patch); write_arp2600_voice(preset.patch); write_arp2600_bus(preset.patch); write_arp2600_macros(preset.patch);
        const float move = studio::macro_amount(preset.patch, GF_MACRO_MOVE);
        float va, vf; studio::macro_vel_depths(preset.patch, &va, &vf);
        preset.patch = studio::move_sfx(studio::macro_patch(preset.patch, 5), move);
        preset.patch.vel_amp = va * 100; preset.patch.vel_cut = vf * 100;
        preset.patch.groove[GF_GROOVE_MACRO][GF_MACRO_GRIT] = 0;
        preset.patch.groove[GF_GROOVE_MACRO][GF_MACRO_POCKET] = 0;
        preset.patch.groove[GF_GROOVE_MACRO][GF_MACRO_MOVE] = 0;
        preset.cutoff_hz = controls[0];
        preset.reso_norm = std::clamp((controls[1] - .1f) / 4.9f, 0.0f, 1.0f);
        preset.patch.attack_ms = attack_->load(); preset.patch.release_ms = controls[2];
        preset.patch.osc[0][2] = arp2600_ring_->load() * 100.0f;
        std::vector<uint8_t> encoded(studio::PRESET_BYTES);
        if (studio::preset_encode(preset, 5, encoded.data(), encoded.size()) != encoded.size()) return false;
        data = std::move(encoded);
        return true;
    }
    bool export_model_d_preset(std::vector<uint8_t>& data) override {
        if (modeld_enabled_->load() < .5f || lfo_sync_->load() >= .5f) return false;
        studio::Preset preset;
        {
            std::lock_guard<std::mutex> guard(preset_mutex_);
            uint8_t source = 99;
            if (!preset_raw_.getSize() || !gf::decode_supported_preset(static_cast<const uint8_t*>(preset_raw_.getData()),
                    preset_raw_.getSize(), preset, source) || source != 0) return false;
        }
        const auto controls = modeld_effective_controls(preset.patch);
        write_modeld_osc(preset.patch); write_modeld_voice(preset.patch); write_modeld_bus(preset.patch); write_modeld_macros(preset.patch);
        float velocity_amp, velocity_filter;
        studio::macro_vel_depths(preset.patch, &velocity_amp, &velocity_filter);
        const float move = studio::macro_amount(preset.patch, GF_MACRO_MOVE);
        preset.patch = studio::move_sfx(studio::macro_patch(preset.patch, 0), move);
        preset.patch.vel_amp = velocity_amp * 100; preset.patch.vel_cut = velocity_filter * 100;
        preset.patch.groove[GF_GROOVE_MACRO][GF_MACRO_GRIT] = 0;
        preset.patch.groove[GF_GROOVE_MACRO][GF_MACRO_POCKET] = 0;
        preset.patch.groove[GF_GROOVE_MACRO][GF_MACRO_MOVE] = 0;
        preset.cutoff_hz = controls[0];
        preset.reso_norm = std::clamp((controls[1] - .1f) / 4.9f, 0.0f, 1.0f);
        preset.patch.attack_ms = attack_->load(); preset.patch.release_ms = controls[2];
        std::vector<uint8_t> encoded(studio::PRESET_BYTES);
        if (studio::preset_encode(preset, 0, encoded.data(), encoded.size()) != encoded.size()) return false;
        data = std::move(encoded);
        return true;
    }
    bool export_ob6_preset(std::vector<uint8_t>& data) override {
        if (ob6_enabled_->load() < .5f || lfo_sync_->load() >= .5f) return false;
        studio::Preset preset;
        {
            std::lock_guard<std::mutex> guard(preset_mutex_);
            uint8_t source = 0;
            if (!preset_raw_.getSize() || !gf::decode_supported_preset(static_cast<const uint8_t*>(preset_raw_.getData()),
                    preset_raw_.getSize(), preset, source) || source != 3) return false;
        }
        const auto controls = ob6_effective_controls(preset.patch);
        write_ob6_osc(preset.patch); write_ob6_voice(preset.patch); write_ob6_macros(preset.patch);
        const float move = studio::macro_amount(preset.patch, GF_MACRO_MOVE);
        float amp, filter;
        studio::macro_vel_depths(preset.patch, &amp, &filter);
        preset.patch = studio::macro_patch(preset.patch, 3);
        preset.patch.vel_amp = amp * 100.0f; preset.patch.vel_cut = filter * 100.0f;
        preset.patch.groove[GF_GROOVE_MACRO][GF_MACRO_GRIT] = 0;
        preset.patch.groove[GF_GROOVE_MACRO][GF_MACRO_POCKET] = 0;
        preset.patch.groove[GF_GROOVE_MACRO][GF_MACRO_MOVE] = 0;
        preset.cutoff_hz = controls[0];
        preset.reso_norm = std::clamp((controls[1] - .7f) / 4.3f, 0.0f, 1.0f);
        preset.patch.attack_ms = attack_->load(); preset.patch.release_ms = controls[2];
        for (int group = 0; group < 3; ++group) {
            preset.patch.groove_on[group] = ob6_groove_[group][0]->load() >= .5f;
            for (int field = 1; field < JUNO_GROOVE_FIELDS[size_t(group)]; ++field)
                preset.patch.groove[group][field - 1] = ob6_groove_[group][field]->load();
        }
        for (int fx = 0; fx < 5; ++fx) {
            preset.patch.sfx_on[fx] = ob6_fx_[fx][0]->load() >= .5f;
            preset.patch.sfx_mix[fx] = ob6_fx_[fx][1]->load();
            for (int field = 0; field < 3; ++field) preset.patch.sfx_p[fx][field] = ob6_fx_[fx][field + 2]->load();
        }
        std::vector<uint8_t> encoded(studio::PRESET_BYTES);
        preset.patch = studio::move_sfx(preset.patch, move);
        if (studio::preset_encode(preset, 3, encoded.data(), encoded.size()) != encoded.size()) return false;
        data = std::move(encoded);
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
        hybrid_b_move_delivered_.store(hybrid_b_move_sequence_.load(std::memory_order_acquire), std::memory_order_release);
        for (size_t i = 0; i < hybrid_macro_parameters_.size(); ++i) hybrid_macro_delivered_[i].store(hybrid_macro_sequence_[i].load(std::memory_order_acquire), std::memory_order_release);
        for (size_t i = 0; i < hybrid_filter_parameters_.size(); ++i) hybrid_filter_delivered_[i].store(hybrid_filter_sequence_[i].load(std::memory_order_acquire), std::memory_order_release);
        hybrid_bank_delivered_.store(hybrid_bank_sequence_.load(std::memory_order_acquire), std::memory_order_release);
        hybrid_morph_delivered_.store(hybrid_morph_sequence_.load(std::memory_order_acquire), std::memory_order_release);
        for (auto& enabled : arp2600_filter_override_) enabled.store(false, std::memory_order_release);
        for (size_t i = 0; i < arp2600_cc_parameters_.size(); ++i)
            arp2600_cc_delivered_[i].store(arp2600_cc_sequence_[i].load(std::memory_order_acquire), std::memory_order_release);
        for (auto& enabled : modeld_filter_override_) enabled.store(false, std::memory_order_release);
        for (size_t i = 0; i < modeld_cc_parameters_.size(); ++i)
            modeld_cc_delivered_[i].store(modeld_cc_sequence_[i].load(std::memory_order_acquire), std::memory_order_release);
        for (auto& enabled : ob6_filter_override_) enabled.store(false, std::memory_order_release);
        for (size_t i = 0; i < ob6_cc_parameters_.size(); ++i)
            ob6_cc_delivered_[i].store(ob6_cc_sequence_[i].load(std::memory_order_acquire), std::memory_order_release);
        for (auto& enabled : juno_filter_override_) enabled.store(false, std::memory_order_release);
        for (size_t i = 0; i < juno_cc_parameters_.size(); ++i)
            juno_cc_delivered_[i].store(juno_cc_sequence_[i].load(std::memory_order_acquire), std::memory_order_release);
        for (size_t i = 0; i < cc_parameters_.size(); ++i)
            cc_delivered_[i].store(cc_sequence_[i].load(std::memory_order_acquire), std::memory_order_release);
        for (size_t i = 0; i < prophet_cc_parameters_.size(); ++i)
            prophet_cc_delivered_[i].store(prophet_cc_sequence_[i].load(std::memory_order_acquire), std::memory_order_release);
    }
    float cc_value(size_t index, float fallback) const {
        return cc_sequence_[index].load(std::memory_order_acquire) != cc_delivered_[index].load(std::memory_order_acquire)
            ? cc_values_[index].load(std::memory_order_relaxed) : fallback;
    }
    void reset_dc_blocker() noexcept { dc_x1_.fill(0.0f); dc_y1_.fill(0.0f); }
    /** Cutoff / resonancia del Prophet en sus unidades: el CC pendiente o el del host. */
    float prophet_cc_value(size_t index, float fallback) const {
        return prophet_cc_sequence_[index].load(std::memory_order_acquire) != prophet_cc_delivered_[index].load(std::memory_order_acquire)
            ? prophet_cc_parameters_[index]->convertFrom0to1(prophet_cc_values_[index].load(std::memory_order_relaxed)) : fallback;
    }
    /** Valor del parámetro DX7 en sus unidades: el CC pendiente (guardado normalizado) o el del host. */
    float cc_parameter_value(size_t index, float fallback) const {
        return cc_sequence_[index].load(std::memory_order_acquire) != cc_delivered_[index].load(std::memory_order_acquire)
            ? cc_parameters_[index]->convertFrom0to1(cc_values_[index].load(std::memory_order_relaxed)) : fallback;
    }
    float juno_cc_value(size_t index, float fallback) const {
        return juno_cc_sequence_[index].load(std::memory_order_acquire) != juno_cc_delivered_[index].load(std::memory_order_acquire)
            ? juno_cc_values_[index].load(std::memory_order_relaxed) : fallback;
    }
    bool hybrid_b_move_pending_active() const {
        if (hybrid_b_move_sequence_.load(std::memory_order_acquire) == hybrid_b_move_delivered_.load(std::memory_order_acquire)) return false;
        const float actual = hybrid_b_move_->load();
        return std::abs(actual - hybrid_b_move_origin_.load(std::memory_order_relaxed)) < 1e-5f ||
               std::abs(actual - hybrid_b_move_value_.load(std::memory_order_relaxed)) < 1e-5f;
    }
    float hybrid_b_move_current() const { return hybrid_b_move_pending_active() ? hybrid_b_move_value_.load(std::memory_order_relaxed) : hybrid_b_move_->load(); }
    bool hybrid_macro_pending_active(size_t i) const {
        if (hybrid_macro_sequence_[i].load(std::memory_order_acquire) == hybrid_macro_delivered_[i].load(std::memory_order_acquire)) return false;
        const float actual = hybrid_macro_parameters_[i]->getValue();
        return std::abs(actual - hybrid_macro_origin_[i].load(std::memory_order_relaxed)) < 1e-5f ||
               std::abs(actual - hybrid_macro_values_[i].load(std::memory_order_relaxed)) < 1e-5f;
    }
    float hybrid_macro_current(size_t i) const {
        return hybrid_macro_pending_active(i) ? hybrid_macro_values_[i].load(std::memory_order_relaxed) : hybrid_macro_[i]->load();
    }
    std::array<float, 7> hybrid_macro_values() const {
        std::array<float, 7> values{};
        for (size_t i = 0; i < values.size(); ++i) values[i] = i < 4 ? hybrid_macro_current(i) : hybrid_macro_[i]->load();
        return values;
    }
    bool hybrid_filter_pending_active(size_t index) const {
        if (hybrid_filter_sequence_[index].load(std::memory_order_acquire) == hybrid_filter_delivered_[index].load(std::memory_order_acquire)) return false;
        const float actual = hybrid_filter_parameters_[index]->getValue();
        return std::abs(actual - hybrid_filter_origin_[index].load(std::memory_order_relaxed)) < 1e-5f ||
               std::abs(actual - hybrid_filter_values_[index].load(std::memory_order_relaxed)) < 1e-5f;
    }
    float hybrid_half_current(unsigned side, unsigned kind) const {
        const size_t i = side * 2 + kind;
        return hybrid_filter_pending_active(i) ? hybrid_filter_parameters_[i]->convertFrom0to1(hybrid_filter_values_[i].load(std::memory_order_relaxed)) : hybrid_half_[side][kind]->load();
    }
    void set_hybrid_filter_pending(unsigned side, unsigned kind, float value) {
        const size_t i = side * 2 + kind;
        if (!hybrid_filter_pending_active(i)) hybrid_filter_origin_[i].store(hybrid_filter_parameters_[i]->getValue(), std::memory_order_relaxed);
        hybrid_filter_values_[i].store(hybrid_filter_parameters_[i]->convertTo0to1(value), std::memory_order_relaxed);
        hybrid_filter_sequence_[i].fetch_add(1, std::memory_order_release);
    }
    bool hybrid_morph_pending_active() const {
        if (hybrid_morph_sequence_.load(std::memory_order_acquire) == hybrid_morph_delivered_.load(std::memory_order_acquire)) return false;
        const float actual = hybrid_morph_->load();
        return std::abs(actual - hybrid_morph_origin_.load(std::memory_order_relaxed)) < 1e-5f ||
               std::abs(actual - hybrid_morph_value_.load(std::memory_order_relaxed)) < 1e-5f;
    }
    float hybrid_morph_current() const {
        return hybrid_morph_pending_active() ? hybrid_morph_value_.load(std::memory_order_relaxed) : hybrid_morph_->load();
    }
    float arp2600_cc_value(size_t index, float fallback) const {
        return arp2600_cc_sequence_[index].load(std::memory_order_acquire) != arp2600_cc_delivered_[index].load(std::memory_order_acquire)
            ? arp2600_cc_values_[index].load(std::memory_order_relaxed) : fallback;
    }
    float arp2600_macro_value(size_t index) const {
        return index < 3 ? arp2600_cc_value(index + 1, arp2600_macro_[index]->load()) : arp2600_macro_[index]->load();
    }
    bool arp2600_filter_override_active(size_t filter) const {
        if (!arp2600_filter_override_[filter].load(std::memory_order_acquire)) return false;
        const size_t index = filter + 4;
        const float value = arp2600_cc_parameters_[index]->getValue();
        if (std::abs(value - arp2600_filter_expected_[filter].load(std::memory_order_relaxed)) <= 1e-5f) return true;
        return arp2600_cc_sequence_[index].load(std::memory_order_acquire) != arp2600_cc_delivered_[index].load(std::memory_order_acquire) &&
            std::abs(value - arp2600_filter_origin_[filter].load(std::memory_order_relaxed)) <= 1e-5f;
    }
    float modeld_cc_value(size_t index, float fallback) const {
        return modeld_cc_sequence_[index].load(std::memory_order_acquire) != modeld_cc_delivered_[index].load(std::memory_order_acquire)
            ? modeld_cc_values_[index].load(std::memory_order_relaxed) : fallback;
    }
    float modeld_macro_value(size_t index) const {
        return index < 3 ? modeld_cc_value(index + 1, modeld_macro_[index]->load()) : modeld_macro_[index]->load();
    }
    bool modeld_filter_override_active(size_t filter) const {
        if (!modeld_filter_override_[filter].load(std::memory_order_acquire)) return false;
        const size_t index = filter + 4;
        const float value = modeld_cc_parameters_[index]->getValue();
        if (std::abs(value - modeld_filter_expected_[filter].load(std::memory_order_relaxed)) <= 1e-5f) return true;
        return modeld_cc_sequence_[index].load(std::memory_order_acquire) != modeld_cc_delivered_[index].load(std::memory_order_acquire) &&
            std::abs(value - modeld_filter_origin_[filter].load(std::memory_order_relaxed)) <= 1e-5f;
    }
    float ob6_cc_value(size_t index, float fallback) const {
        return ob6_cc_sequence_[index].load(std::memory_order_acquire) != ob6_cc_delivered_[index].load(std::memory_order_acquire)
            ? ob6_cc_values_[index].load(std::memory_order_relaxed) : fallback;
    }
    float ob6_macro_value(size_t index) const {
        return index < 3 ? ob6_cc_value(index + 1, ob6_macro_[index]->load()) : ob6_macro_[index]->load();
    }
    bool ob6_filter_override_active(size_t filter) const {
        if (!ob6_filter_override_[filter].load(std::memory_order_acquire)) return false;
        const size_t index = filter + 4;
        const float value = ob6_cc_parameters_[index]->getValue();
        if (std::abs(value - ob6_filter_expected_[filter].load(std::memory_order_relaxed)) <= 1e-5f) return true;
        return ob6_cc_sequence_[index].load(std::memory_order_acquire) != ob6_cc_delivered_[index].load(std::memory_order_acquire) &&
            std::abs(value - ob6_filter_origin_[filter].load(std::memory_order_relaxed)) <= 1e-5f;
    }
    bool juno_filter_override_active(size_t filter) const {
        if (!juno_filter_override_[filter].load(std::memory_order_acquire)) return false;
        const size_t index = filter + 4;
        const float value = juno_cc_parameters_[index]->getValue();
        if (std::abs(value - juno_filter_expected_[filter].load(std::memory_order_relaxed)) <= 1e-5f) return true;
        return juno_cc_sequence_[index].load(std::memory_order_acquire) != juno_cc_delivered_[index].load(std::memory_order_acquire) &&
            std::abs(value - juno_filter_origin_[filter].load(std::memory_order_relaxed)) <= 1e-5f;
    }
    float juno_macro_value(size_t index) const {
        return index < 3 ? juno_cc_value(index + 1, juno_macro_[index]->load()) : juno_macro_[index]->load();
    }
    void load_dx_macros() {
        for (size_t i = 0; i < DX_MACRO_IDS.size(); ++i)
            dx_macro_current_[i] = i < 3 ? cc_value(i + 1, dx_macro_[i]->load()) : dx_macro_[i]->load();
    }
    void timerCallback() override {
        const auto b_move_sequence = hybrid_b_move_sequence_.load(std::memory_order_acquire);
        if (b_move_sequence != hybrid_b_move_delivered_.load(std::memory_order_relaxed)) {
            if (hybrid_b_move_pending_active()) state_.getParameter("hybrid.b.move")->setValueNotifyingHost(hybrid_b_move_value_.load(std::memory_order_relaxed));
            hybrid_b_move_delivered_.store(b_move_sequence, std::memory_order_release);
        }
        for (size_t i = 0; i < hybrid_macro_parameters_.size(); ++i) {
            const auto sequence = hybrid_macro_sequence_[i].load(std::memory_order_acquire);
            if (sequence == hybrid_macro_delivered_[i].load(std::memory_order_relaxed)) continue;
            if (hybrid_macro_pending_active(i)) hybrid_macro_parameters_[i]->setValueNotifyingHost(hybrid_macro_values_[i].load(std::memory_order_relaxed));
            hybrid_macro_delivered_[i].store(sequence, std::memory_order_release);
        }
        for (size_t i = 0; i < hybrid_filter_parameters_.size(); ++i) {
            const auto sequence = hybrid_filter_sequence_[i].load(std::memory_order_acquire);
            if (sequence == hybrid_filter_delivered_[i].load(std::memory_order_relaxed)) continue;
            if (hybrid_filter_pending_active(i)) hybrid_filter_parameters_[i]->setValueNotifyingHost(hybrid_filter_values_[i].load(std::memory_order_relaxed));
            hybrid_filter_delivered_[i].store(sequence, std::memory_order_release);
        }
        const auto bank_sequence = hybrid_bank_sequence_.load(std::memory_order_acquire);
        if (bank_sequence != hybrid_bank_delivered_.load(std::memory_order_relaxed)) {
            uint8_t count = 0;
            { std::lock_guard<std::mutex> guard(preset_mutex_); count = hybrid_bank_count_; }
            const uint8_t index = studio::layer_from_cc(hybrid_bank_value_.load(std::memory_order_relaxed), count);
            if (index == 0) state_.getParameter("hybrid_preview")->setValueNotifyingHost(0);
            else load_hybrid_bank(unsigned(index - 1));
            hybrid_bank_delivered_.store(bank_sequence, std::memory_order_release);
        }
        const auto hybrid_sequence = hybrid_morph_sequence_.load(std::memory_order_acquire);
        if (hybrid_sequence != hybrid_morph_delivered_.load(std::memory_order_relaxed)) {
            if (hybrid_morph_pending_active()) state_.getParameter("hybrid.morph")->setValueNotifyingHost(hybrid_morph_value_.load(std::memory_order_relaxed));
            hybrid_morph_delivered_.store(hybrid_sequence, std::memory_order_release);
        }
        for (size_t i = 0; i < arp2600_cc_parameters_.size(); ++i) {
            const auto sequence = arp2600_cc_sequence_[i].load(std::memory_order_acquire);
            if (sequence == arp2600_cc_delivered_[i].load(std::memory_order_relaxed)) continue;
            if (i >= 4 && !arp2600_filter_override_active(i - 4)) {
                arp2600_filter_override_[i - 4].store(false, std::memory_order_release);
                arp2600_cc_delivered_[i].store(sequence, std::memory_order_release);
                continue;
            }
            arp2600_cc_parameters_[i]->setValueNotifyingHost(arp2600_cc_values_[i].load(std::memory_order_relaxed));
            arp2600_cc_delivered_[i].store(sequence, std::memory_order_release);
        }
        for (size_t i = 0; i < modeld_cc_parameters_.size(); ++i) {
            const auto sequence = modeld_cc_sequence_[i].load(std::memory_order_acquire);
            if (sequence == modeld_cc_delivered_[i].load(std::memory_order_relaxed)) continue;
            if (i >= 4 && !modeld_filter_override_active(i - 4)) {
                modeld_filter_override_[i - 4].store(false, std::memory_order_release);
                modeld_cc_delivered_[i].store(sequence, std::memory_order_release);
                continue;
            }
            modeld_cc_parameters_[i]->setValueNotifyingHost(modeld_cc_values_[i].load(std::memory_order_relaxed));
            modeld_cc_delivered_[i].store(sequence, std::memory_order_release);
        }
        for (size_t i = 0; i < ob6_cc_parameters_.size(); ++i) {
            const auto sequence = ob6_cc_sequence_[i].load(std::memory_order_acquire);
            if (sequence == ob6_cc_delivered_[i].load(std::memory_order_relaxed)) continue;
            if (i >= 4 && !ob6_filter_override_active(i - 4)) {
                ob6_filter_override_[i - 4].store(false, std::memory_order_release);
                ob6_cc_delivered_[i].store(sequence, std::memory_order_release);
                continue;
            }
            ob6_cc_parameters_[i]->setValueNotifyingHost(ob6_cc_values_[i].load(std::memory_order_relaxed));
            ob6_cc_delivered_[i].store(sequence, std::memory_order_release);
        }
        for (size_t i = 0; i < juno_cc_parameters_.size(); ++i) {
            const auto sequence = juno_cc_sequence_[i].load(std::memory_order_acquire);
            if (sequence == juno_cc_delivered_[i].load(std::memory_order_relaxed)) continue;
            if (i >= 4 && !juno_filter_override_active(i - 4)) {
                juno_filter_override_[i - 4].store(false, std::memory_order_release);
                juno_cc_delivered_[i].store(sequence, std::memory_order_release);
                continue;
            }
            juno_cc_parameters_[i]->setValueNotifyingHost(juno_cc_values_[i].load(std::memory_order_relaxed));
            juno_cc_delivered_[i].store(sequence, std::memory_order_release);
        }
        for (size_t i = 0; i < cc_parameters_.size(); ++i) {
            const auto sequence = cc_sequence_[i].load(std::memory_order_acquire);
            if (sequence == cc_delivered_[i].load(std::memory_order_relaxed)) continue;
            cc_parameters_[i]->setValueNotifyingHost(cc_values_[i].load(std::memory_order_relaxed));
            cc_delivered_[i].store(sequence, std::memory_order_release);
        }
        for (size_t i = 0; i < prophet_cc_parameters_.size(); ++i) {
            const auto sequence = prophet_cc_sequence_[i].load(std::memory_order_acquire);
            if (sequence == prophet_cc_delivered_[i].load(std::memory_order_relaxed)) continue;
            prophet_cc_parameters_[i]->setValueNotifyingHost(prophet_cc_values_[i].load(std::memory_order_relaxed));
            prophet_cc_delivered_[i].store(sequence, std::memory_order_release);
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
            const float value = i == 17 ? cc_value(0, dx_general_[i]->load())
                : i < 2 ? cc_parameter_value(4 + i, dx_general_[i]->load()) : dx_general_[i]->load();
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
    float juno_effective_cutoff() const {
        const float cutoff = juno_filter_override_active(0) ? gf_cutoff_from_cc_norm(1, juno_filter_raw_[0].load(std::memory_order_relaxed)) : cutoff_->load();
        return std::clamp(cutoff * studio::move_cutoff_mul(juno_macro_value(2)), 20.0f, 8000.0f);
    }
    float juno_effective_resonance() const {
        const auto p = juno_base_env();
        const float norm = juno_filter_override_active(1) ? juno_filter_raw_[1].load(std::memory_order_relaxed) :
            std::clamp((resonance_->load() - .1f) / 4.9f, 0.0f, 1.0f);
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
    void set_modeld_macro_parameters(const studio::Preset& preset) {
        const auto& m = preset.patch.groove[GF_GROOVE_MACRO];
        const float values[] = {m[GF_MACRO_GRIT], m[GF_MACRO_POCKET], m[GF_MACRO_MOVE],
            m[GF_MACRO_GRIT_RES], m[GF_MACRO_GRIT_MOD], m[GF_MACRO_POCKET_VEL]};
        for (size_t i = 0; i < MODELD_MACRO_IDS.size(); ++i)
            state_.getParameter(MODELD_MACRO_IDS[i])->setValueNotifyingHost(std::clamp(values[i] * .01f, 0.0f, 1.0f));
    }
    void write_modeld_macros(studio::Patch& patch) const {
        auto& m = patch.groove[GF_GROOVE_MACRO];
        m[GF_MACRO_GRIT] = modeld_macro_value(0) * 100; m[GF_MACRO_POCKET] = modeld_macro_value(1) * 100;
        m[GF_MACRO_MOVE] = modeld_macro_value(2) * 100; m[GF_MACRO_GRIT_RES] = modeld_macro_[3]->load() * 100;
        m[GF_MACRO_GRIT_MOD] = modeld_macro_[4]->load() * 100; m[GF_MACRO_POCKET_VEL] = modeld_macro_[5]->load() * 100;
    }
    std::array<float, 3> modeld_effective_controls(const studio::Patch& original) const {
        auto current = original; write_modeld_macros(current);
        const float old_move = studio::macro_amount(original, GF_MACRO_MOVE), move = studio::macro_amount(current, GF_MACRO_MOVE);
        const float cutoff = modeld_filter_override_active(0)
            ? std::clamp(gf_cutoff_from_cc_norm(0, modeld_filter_raw_[0].load(std::memory_order_relaxed)) * studio::move_cutoff_mul(move), 20.0f, 8000.0f)
            : std::clamp(cutoff_->load() * studio::move_cutoff_mul(move) / studio::move_cutoff_mul(old_move), 20.0f, 8000.0f);
        const float old_res = studio::move_resonance(studio::macro_resonance(0.0f, original), old_move);
        const float new_res = studio::move_resonance(studio::macro_resonance(0.0f, current), move);
        const float resonance = modeld_filter_override_active(1)
            ? std::clamp(.1f + 4.9f * studio::move_resonance(studio::macro_resonance(modeld_filter_raw_[1].load(std::memory_order_relaxed), current), move), .707f, 5.0f)
            : std::clamp(resonance_->load() + 4.9f * (new_res - old_res), .707f, 5.0f);
        const float old_release = studio::macro_patch(original, 0).release_ms, new_release = studio::macro_patch(current, 0).release_ms;
        const float release = std::clamp(release_->load() * (old_release > .001f ? new_release / old_release : 1.0f), 1.0f, 10000.0f);
        return {cutoff, resonance, release};
    }
    void set_modeld_bus_parameters(const studio::Preset& preset) {
        for (int g = 0; g < 3; ++g) for (int f = 0; f < JUNO_GROOVE_FIELDS[size_t(g)]; ++f) {
            auto* p = state_.getParameter(modeld_groove_id(g, f));
            const float value = f == 0 ? (preset.patch.groove_on[g] ? 1.0f : 0.0f) : preset.patch.groove[g][f - 1];
            p->setValueNotifyingHost(p->convertTo0to1(value));
        }
        for (int fx = 0; fx < 5; ++fx) for (int f = 0; f < 5; ++f) {
            auto* p = state_.getParameter(modeld_fx_id(fx, f));
            const float value = f == 0 ? (preset.patch.sfx_on[fx] ? 1.0f : 0.0f) : f == 1 ? preset.patch.sfx_mix[fx] : preset.patch.sfx_p[fx][f - 2];
            p->setValueNotifyingHost(p->convertTo0to1(value));
        }
    }
    void write_modeld_bus(studio::Patch& p) const {
        for (int g = 0; g < 3; ++g) {
            p.groove_on[g] = modeld_groove_[g][0]->load() >= .5f;
            for (int f = 1; f < JUNO_GROOVE_FIELDS[size_t(g)]; ++f) p.groove[g][f - 1] = modeld_groove_[g][f]->load();
        }
        for (int fx = 0; fx < 5; ++fx) {
            p.sfx_on[fx] = modeld_fx_[fx][0]->load() >= .5f; p.sfx_mix[fx] = modeld_fx_[fx][1]->load();
            for (int f = 0; f < 3; ++f) p.sfx_p[fx][f] = modeld_fx_[fx][f + 2]->load();
        }
    }
    void apply_modeld_groove_automation() {
        bool changed = false;
        for (int g = 0; g < 3; ++g) for (int f = 0; f < JUNO_GROOVE_FIELDS[size_t(g)]; ++f) {
            const size_t i = size_t(g * 7 + f); const float value = modeld_groove_[g][f]->load();
            if (value != modeld_groove_last_[i]) { modeld_groove_last_[i] = value; changed = true; }
        }
        if (!changed) return;
        auto preset = modeld_base_; write_modeld_bus(preset.patch); gf::apply_groove_preview(modeld_bus_, preset, false);
    }
    void apply_modeld_fx_automation() {
        bool changed = false;
        for (int fx = 0; fx < 5; ++fx) for (int f = 0; f < 5; ++f) {
            const size_t i = size_t(fx * 5 + f); const float value = modeld_fx_[fx][f]->load();
            if (value != modeld_fx_last_[i]) { modeld_fx_last_[i] = value; changed = true; }
        }
        if (!changed) return;
        auto preset = modeld_base_; write_modeld_bus(preset.patch); write_modeld_macros(preset.patch);
        preset.patch = studio::move_sfx(preset.patch, studio::macro_amount(preset.patch, GF_MACRO_MOVE));
        gf::apply_synth_fx_preview(modeld_bus_, preset, 0);
    }
    static std::array<float, MODELD_VOICE.size()> modeld_voice_values(const studio::Patch& p) {
        return {p.decay_ms, p.sustain, p.f_attack_ms, p.f_decay_ms, p.f_sustain, p.f_release_ms,
            p.env_amt, p.drive, p.vel_amp, p.vel_cut, p.lfo_rate_hz, float(p.lfo_shape), float(p.lfo_div),
            p.lfo_amt[GF_DEST_CUTOFF], p.lfo_amt[GF_DEST_PITCH], p.lfo_amt[GF_DEST_PWM], p.glide,
            p.groove[GF_GROOVE_MACRO][GF_MACRO_DRIFT]};
    }
    void set_modeld_voice_parameters(const studio::Preset& preset) {
        const auto values = modeld_voice_values(preset.patch);
        for (size_t i = 0; i < MODELD_VOICE.size(); ++i) {
            auto* parameter = state_.getParameter(MODELD_VOICE[i].id);
            parameter->setValueNotifyingHost(parameter->convertTo0to1(values[i]));
        }
    }
    void write_modeld_voice(studio::Patch& p) const {
        auto v = [&](size_t i) { return modeld_voice_[i]->load(); };
        p.decay_ms = v(0); p.sustain = v(1); p.f_attack_ms = v(2); p.f_decay_ms = v(3);
        p.f_sustain = v(4); p.f_release_ms = v(5); p.env_amt = v(6); p.drive = v(7);
        p.vel_amp = v(8); p.vel_cut = v(9); p.lfo_rate_hz = v(10); p.lfo_shape = uint8_t(v(11));
        p.lfo_div = uint8_t(v(12)); p.lfo_amt[GF_DEST_CUTOFF] = v(13); p.lfo_amt[GF_DEST_PITCH] = v(14);
        p.lfo_amt[GF_DEST_PWM] = v(15); p.glide = v(16); p.groove[GF_GROOVE_MACRO][GF_MACRO_DRIFT] = modeld_cc_value(0, v(17) * .01f) * 100;
    }
    void set_arp2600_macro_parameters(const studio::Preset& preset) {
        const auto& m = preset.patch.groove[GF_GROOVE_MACRO];
        const float values[] = {m[GF_MACRO_GRIT], m[GF_MACRO_POCKET], m[GF_MACRO_MOVE], m[GF_MACRO_GRIT_RES], m[GF_MACRO_GRIT_MOD], m[GF_MACRO_POCKET_VEL]};
        for (size_t i = 0; i < ARP2600_MACRO_IDS.size(); ++i)
            state_.getParameter(ARP2600_MACRO_IDS[i])->setValueNotifyingHost(std::clamp(values[i] * .01f, 0.0f, 1.0f));
    }
    void write_arp2600_macros(studio::Patch& p) const {
        auto& m = p.groove[GF_GROOVE_MACRO];
        m[GF_MACRO_GRIT] = arp2600_macro_value(0) * 100; m[GF_MACRO_POCKET] = arp2600_macro_value(1) * 100;
        m[GF_MACRO_MOVE] = arp2600_macro_value(2) * 100; m[GF_MACRO_GRIT_RES] = arp2600_macro_[3]->load() * 100;
        m[GF_MACRO_GRIT_MOD] = arp2600_macro_[4]->load() * 100; m[GF_MACRO_POCKET_VEL] = arp2600_macro_[5]->load() * 100;
    }
    std::array<float, 3> arp2600_effective_controls(const studio::Patch& original) const {
        auto current = original; write_arp2600_macros(current);
        const float old_move = studio::macro_amount(original, GF_MACRO_MOVE), move = studio::macro_amount(current, GF_MACRO_MOVE);
        const float cutoff = arp2600_filter_override_active(0)
            ? std::clamp(gf_cutoff_from_cc_norm(5, arp2600_filter_raw_[0].load(std::memory_order_relaxed)) * studio::move_cutoff_mul(move), GF_CUTOFF_RANGE[5].lo, GF_CUTOFF_RANGE[5].hi)
            : std::clamp(cutoff_->load() * studio::move_cutoff_mul(move) / studio::move_cutoff_mul(old_move), GF_CUTOFF_RANGE[5].lo, GF_CUTOFF_RANGE[5].hi);
        const float old_res = studio::move_resonance(studio::macro_resonance(0.0f, original), old_move);
        const float new_res = studio::move_resonance(studio::macro_resonance(0.0f, current), move);
        const float resonance = arp2600_filter_override_active(1)
            ? std::clamp(.1f + 4.9f * studio::move_resonance(studio::macro_resonance(arp2600_filter_raw_[1].load(std::memory_order_relaxed), current), move), .707f, 5.0f)
            : std::clamp(resonance_->load() + 4.9f * (new_res - old_res), .707f, 5.0f);
        const float old_release = studio::macro_patch(original, 5).release_ms, new_release = studio::macro_patch(current, 5).release_ms;
        return {cutoff, resonance, std::clamp(release_->load() * (old_release > .001f ? new_release / old_release : 1.0f), 1.0f, 10000.0f)};
    }
    void set_arp2600_bus_parameters(const studio::Preset& preset) {
        for (int g = 0; g < 3; ++g) for (int f = 0; f < JUNO_GROOVE_FIELDS[size_t(g)]; ++f) {
            auto* p = state_.getParameter(arp2600_groove_id(g, f));
            const float value = f == 0 ? (preset.patch.groove_on[g] ? 1.0f : 0.0f) : preset.patch.groove[g][f - 1];
            p->setValueNotifyingHost(p->convertTo0to1(value));
        }
        for (int fx = 0; fx < 5; ++fx) for (int f = 0; f < 5; ++f) {
            auto* p = state_.getParameter(arp2600_fx_id(fx, f));
            const float value = f == 0 ? (preset.patch.sfx_on[fx] ? 1.0f : 0.0f) : f == 1 ? preset.patch.sfx_mix[fx] : preset.patch.sfx_p[fx][f - 2];
            p->setValueNotifyingHost(p->convertTo0to1(value));
        }
    }
    void write_arp2600_bus(studio::Patch& p) const {
        for (int g = 0; g < 3; ++g) {
            p.groove_on[g] = arp2600_groove_[g][0]->load() >= .5f;
            for (int f = 1; f < JUNO_GROOVE_FIELDS[size_t(g)]; ++f) p.groove[g][f - 1] = arp2600_groove_[g][f]->load();
        }
        for (int fx = 0; fx < 5; ++fx) {
            p.sfx_on[fx] = arp2600_fx_[fx][0]->load() >= .5f; p.sfx_mix[fx] = arp2600_fx_[fx][1]->load();
            for (int f = 0; f < 3; ++f) p.sfx_p[fx][f] = arp2600_fx_[fx][f + 2]->load();
        }
    }
    void apply_arp2600_bus() {
        bool groove_changed = false, fx_changed = false;
        for (int g = 0; g < 3; ++g) for (int f = 0; f < JUNO_GROOVE_FIELDS[size_t(g)]; ++f) {
            const size_t i = size_t(g * 7 + f); const float v = arp2600_groove_[g][f]->load();
            if (v != arp2600_groove_last_[i]) { arp2600_groove_last_[i] = v; groove_changed = true; }
        }
        for (int fx = 0; fx < 5; ++fx) for (int f = 0; f < 5; ++f) {
            const size_t i = size_t(fx * 5 + f); const float v = arp2600_fx_[fx][f]->load();
            if (v != arp2600_fx_last_[i]) { arp2600_fx_last_[i] = v; fx_changed = true; }
        }
        if (!groove_changed && !fx_changed) return;
        auto p = arp2600_base_; write_arp2600_bus(p.patch); write_arp2600_macros(p.patch);
        if (groove_changed) gf::apply_groove_preview(arp2600_bus_, p, false);
        if (fx_changed) { p.patch = studio::move_sfx(p.patch, studio::macro_amount(p.patch, GF_MACRO_MOVE)); gf::apply_synth_fx_preview(arp2600_bus_, p, 5); }
    }
    void set_arp2600_voice_parameters(const studio::Preset& preset) {
        const auto& p = preset.patch;
        const float values[] = {p.decay_ms, p.sustain, p.f_attack_ms, p.f_decay_ms, p.f_sustain, p.f_release_ms,
            p.env_amt, p.drive, p.vel_amp, p.vel_cut, p.lfo_rate_hz, float(p.lfo_shape), float(p.lfo_div),
            p.lfo_amt[GF_DEST_CUTOFF], p.lfo_amt[GF_DEST_PITCH], p.lfo_amt[GF_DEST_PWM], p.glide,
            p.groove_on[GF_GROOVE_UNISON] ? 1.0f : 0.0f, p.groove[GF_GROOVE_UNISON][GF_UNISON_VOICES],
            p.groove[GF_GROOVE_UNISON][GF_UNISON_DETUNE], p.groove[GF_GROOVE_MACRO][GF_MACRO_DRIFT]};
        for (size_t i = 0; i < ARP2600_VOICE.size(); ++i) {
            auto* parameter = state_.getParameter(ARP2600_VOICE[i].id);
            parameter->setValueNotifyingHost(parameter->convertTo0to1(values[i]));
        }
    }
    void write_arp2600_voice(studio::Patch& p) const {
        auto value = [&](size_t i) { return arp2600_voice_[i]->load(); };
        p.decay_ms = value(0); p.sustain = value(1); p.f_attack_ms = value(2); p.f_decay_ms = value(3);
        p.f_sustain = value(4); p.f_release_ms = value(5); p.env_amt = value(6); p.drive = value(7);
        p.vel_amp = value(8); p.vel_cut = value(9); p.lfo_rate_hz = value(10); p.lfo_shape = uint8_t(value(11));
        p.lfo_div = uint8_t(value(12)); p.lfo_amt[GF_DEST_CUTOFF] = value(13); p.lfo_amt[GF_DEST_PITCH] = value(14);
        p.lfo_amt[GF_DEST_PWM] = value(15); p.glide = value(16); p.groove_on[GF_GROOVE_UNISON] = value(17) >= .5f;
        p.groove[GF_GROOVE_UNISON][GF_UNISON_VOICES] = value(18); p.groove[GF_GROOVE_UNISON][GF_UNISON_DETUNE] = value(19);
        p.groove[GF_GROOVE_MACRO][GF_MACRO_DRIFT] = arp2600_cc_value(0, value(20) * .01f) * 100;
    }
    void set_arp2600_osc_parameters(const studio::Preset& preset) {
        for (const auto& c : ARP2600_OSC) {
            auto* parameter = state_.getParameter(c.id);
            const float value = c.mod ? preset.patch.mod[c.stop][c.slot] : preset.patch.osc[c.stop][c.slot];
            parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
        }
    }
    void write_arp2600_osc(studio::Patch& patch) const {
        for (size_t i = 0; i < ARP2600_OSC.size(); ++i) {
            const auto& c = ARP2600_OSC[i];
            (c.mod ? patch.mod[c.stop][c.slot] : patch.osc[c.stop][c.slot]) = arp2600_osc_[i]->load();
        }
    }
    void apply_arp2600_osc() {
        bool changed = false;
        for (size_t i = 0; i < ARP2600_OSC.size(); ++i) {
            const float value = arp2600_osc_[i]->load();
            if (value != arp2600_osc_last_[i]) { arp2600_osc_last_[i] = value; changed = true; }
        }
        for (size_t i = 0; i < ARP2600_VOICE.size(); ++i) {
            const float value = i == 20 ? arp2600_cc_value(0, arp2600_voice_[i]->load() * .01f) * 100 : arp2600_voice_[i]->load();
            if (value != arp2600_voice_last_[i]) { arp2600_voice_last_[i] = value; changed = true; }
        }
        if (!changed) return;
        auto preset = arp2600_base_; write_arp2600_osc(preset.patch); write_arp2600_voice(preset.patch); write_arp2600_macros(preset.patch);
        arp2600_.set_patch(gf::make_arp2600_patch(preset));
    }
    void set_modeld_osc_parameters(const studio::Preset& preset) {
        for (const auto& c : MODELD_OSC) {
            auto* parameter = state_.getParameter(c.id);
            const float value = c.mod ? preset.patch.mod[c.stop][c.slot] : preset.patch.osc[c.stop][c.slot];
            parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
        }
    }
    void write_modeld_osc(studio::Patch& patch) const {
        for (size_t i = 0; i < MODELD_OSC.size(); ++i) {
            const auto& c = MODELD_OSC[i];
            (c.mod ? patch.mod[c.stop][c.slot] : patch.osc[c.stop][c.slot]) = modeld_osc_[i]->load();
        }
    }
    void apply_modeld_osc_automation() {
        bool changed = false;
        for (size_t i = 0; i < MODELD_OSC.size(); ++i) {
            const float value = modeld_osc_[i]->load();
            if (value != modeld_osc_last_[i]) { modeld_osc_last_[i] = value; changed = true; }
        }
        for (size_t i = 0; i < MODELD_VOICE.size(); ++i) {
            const float value = i == 17 ? modeld_cc_value(0, modeld_voice_[i]->load() * .01f) * 100 : modeld_voice_[i]->load();
            if (value != modeld_voice_last_[i]) { modeld_voice_last_[i] = value; changed = true; }
        }
        if (!changed) return;
        auto preset = modeld_base_;
        write_modeld_osc(preset.patch); write_modeld_voice(preset.patch); write_modeld_macros(preset.patch);
        gf::apply_model_d_preset_preview(modeld_, preset);
    }
    void set_ob6_macro_parameters(const studio::Preset& preset) {
        const auto& m = preset.patch.groove[GF_GROOVE_MACRO];
        const float values[] = {m[GF_MACRO_GRIT], m[GF_MACRO_POCKET], m[GF_MACRO_MOVE],
            m[GF_MACRO_GRIT_RES], m[GF_MACRO_GRIT_MOD], m[GF_MACRO_POCKET_VEL]};
        for (size_t i = 0; i < OB6_MACRO_IDS.size(); ++i) {
            auto* p = state_.getParameter(OB6_MACRO_IDS[i]); p->setValueNotifyingHost(std::clamp(values[i] * .01f, 0.0f, 1.0f));
        }
    }
    void write_ob6_macros(studio::Patch& patch) const {
        auto& m = patch.groove[GF_GROOVE_MACRO];
        m[GF_MACRO_GRIT] = ob6_macro_value(0) * 100; m[GF_MACRO_POCKET] = ob6_macro_value(1) * 100;
        m[GF_MACRO_MOVE] = ob6_macro_value(2) * 100; m[GF_MACRO_GRIT_RES] = ob6_macro_[3]->load() * 100;
        m[GF_MACRO_GRIT_MOD] = ob6_macro_[4]->load() * 100; m[GF_MACRO_POCKET_VEL] = ob6_macro_[5]->load() * 100;
    }
    std::array<float, 3> ob6_effective_controls(const studio::Patch& original) const {
        auto current = original;
        write_ob6_macros(current);
        const float old_move = studio::macro_amount(original, GF_MACRO_MOVE);
        const float move = studio::macro_amount(current, GF_MACRO_MOVE);
        const float cutoff = ob6_filter_override_active(0)
            ? std::clamp(gf_cutoff_from_cc_norm(3, ob6_filter_raw_[0].load(std::memory_order_relaxed)) * studio::move_cutoff_mul(move), 20.0f, 20000.0f)
            : std::clamp(cutoff_->load() * studio::move_cutoff_mul(move) / studio::move_cutoff_mul(old_move), 20.0f, 20000.0f);
        const float old_res = studio::move_resonance(studio::macro_resonance(0.0f, original), old_move);
        const float new_res = studio::move_resonance(studio::macro_resonance(0.0f, current), move);
        const float resonance = ob6_filter_override_active(1)
            ? std::clamp(.7f + 4.3f * studio::move_resonance(studio::macro_resonance(ob6_filter_raw_[1].load(std::memory_order_relaxed), current), move), .707f, 5.0f)
            : std::clamp(resonance_->load() + 4.3f * (new_res - old_res), .707f, 5.0f);
        const float old_release = studio::macro_patch(original, 3).release_ms;
        const float new_release = studio::macro_patch(current, 3).release_ms;
        const float release = std::clamp(release_->load() * (old_release > 0.001f ? new_release / old_release : 1.0f), 1.0f, 10000.0f);
        return {cutoff, resonance, release};
    }
    static std::array<float, OB6_VOICE.size()> ob6_voice_values(const studio::Patch& p) {
        return {p.decay_ms, p.sustain, p.f_attack_ms, p.f_decay_ms, p.f_sustain, p.f_release_ms,
            p.env_amt, p.drive, p.vel_amp, p.vel_cut, p.lfo_rate_hz, float(p.lfo_shape), float(p.lfo_div),
            p.lfo_amt[GF_DEST_CUTOFF], p.lfo_amt[GF_DEST_PITCH], p.lfo_amt[GF_DEST_PWM], p.glide,
            p.groove_on[GF_GROOVE_UNISON] ? 1.0f : 0.0f, p.groove[GF_GROOVE_UNISON][GF_UNISON_VOICES],
            p.groove[GF_GROOVE_UNISON][GF_UNISON_DETUNE], p.groove[GF_GROOVE_MACRO][GF_MACRO_DRIFT]};
    }
    void write_ob6_voice(studio::Patch& p) const {
        auto v = [&](size_t i) { return ob6_voice_[i]->load(); };
        p.decay_ms = v(0); p.sustain = v(1); p.f_attack_ms = v(2); p.f_decay_ms = v(3);
        p.f_sustain = v(4); p.f_release_ms = v(5); p.env_amt = v(6); p.drive = v(7);
        p.vel_amp = v(8); p.vel_cut = v(9); p.lfo_rate_hz = v(10); p.lfo_shape = uint8_t(v(11));
        p.lfo_div = uint8_t(v(12)); p.lfo_amt[GF_DEST_CUTOFF] = v(13); p.lfo_amt[GF_DEST_PITCH] = v(14);
        p.lfo_amt[GF_DEST_PWM] = v(15); p.glide = v(16); p.groove_on[GF_GROOVE_UNISON] = v(17) >= .5f;
        p.groove[GF_GROOVE_UNISON][GF_UNISON_VOICES] = v(18); p.groove[GF_GROOVE_UNISON][GF_UNISON_DETUNE] = v(19);
        p.groove[GF_GROOVE_MACRO][GF_MACRO_DRIFT] = ob6_cc_value(0, v(20) * .01f) * 100;
    }
    void set_ob6_voice_parameters(const studio::Preset& preset) {
        const auto values = ob6_voice_values(preset.patch);
        for (size_t i = 0; i < OB6_VOICE.size(); ++i) {
            auto* p = state_.getParameter(OB6_VOICE[i].id);
            p->setValueNotifyingHost(p->convertTo0to1(values[i]));
        }
    }
    void write_ob6_osc(studio::Patch& patch) const {
        for (size_t i = 0; i < OB6_OSC.size(); ++i) {
            const auto& c = OB6_OSC[i];
            (c.mod ? patch.mod[c.stop][c.slot] : patch.osc[c.stop][c.slot]) = ob6_osc_[i]->load();
        }
    }
    void set_ob6_osc_parameters(const studio::Preset& preset) {
        for (const auto& c : OB6_OSC) {
            auto* parameter = state_.getParameter(c.id);
            const float value = c.mod ? preset.patch.mod[c.stop][c.slot] : preset.patch.osc[c.stop][c.slot];
            parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
        }
    }
    void apply_ob6_osc_automation() {
        bool changed = false;
        for (size_t i = 0; i < OB6_OSC.size(); ++i) {
            const float value = ob6_osc_[i]->load();
            if (value != ob6_osc_last_[i]) { ob6_osc_last_[i] = value; changed = true; }
        }
        for (size_t i = 0; i < OB6_VOICE.size(); ++i) {
            const float value = i == 20 ? ob6_cc_value(0, ob6_voice_[i]->load() * .01f) * 100 : ob6_voice_[i]->load();
            if (value != ob6_voice_last_[i]) { ob6_voice_last_[i] = value; changed = true; }
        }
        if (!changed) return;
        auto preset = ob6_base_;
        write_ob6_osc(preset.patch); write_ob6_voice(preset.patch); write_ob6_macros(preset.patch);
        gf::apply_ob6_preset_preview(ob6_, preset);
    }
    void set_ob6_fx_parameters(const studio::Preset& preset) {
        for (int fx = 0; fx < 5; ++fx) for (int field = 0; field < 5; ++field) {
            auto* p = state_.getParameter(ob6_fx_id(fx, field));
            const float value = field == 0 ? (preset.patch.sfx_on[fx] ? 1.0f : 0.0f) :
                field == 1 ? preset.patch.sfx_mix[fx] : preset.patch.sfx_p[fx][field - 2];
            p->setValueNotifyingHost(p->convertTo0to1(value));
        }
    }
    void apply_ob6_fx_automation() {
        bool changed = false;
        for (int fx = 0; fx < 5; ++fx) for (int field = 0; field < 5; ++field) {
            const auto i = size_t(fx * 5 + field);
            const float value = ob6_fx_[fx][field]->load();
            if (value != ob6_fx_last_[i]) { ob6_fx_last_[i] = value; changed = true; }
        }
        if (!changed) return;
        studio::Preset p;
        for (int fx = 0; fx < 5; ++fx) {
            p.patch.sfx_on[fx] = ob6_fx_last_[size_t(fx * 5)] >= .5f;
            p.patch.sfx_mix[fx] = ob6_fx_last_[size_t(fx * 5 + 1)];
            for (int field = 0; field < 3; ++field) p.patch.sfx_p[fx][field] = ob6_fx_last_[size_t(fx * 5 + field + 2)];
        }
        p.patch = studio::move_sfx(p.patch, ob6_macro_value(2));
        gf::apply_synth_fx_preview(ob6_bus_, p, 3);
    }
    void set_ob6_groove_parameters(const studio::Preset& preset) {
        for (int group = 0; group < 3; ++group) for (int field = 0; field < JUNO_GROOVE_FIELDS[size_t(group)]; ++field) {
            auto* p = state_.getParameter(ob6_groove_id(group, field));
            const float value = field == 0 ? (preset.patch.groove_on[group] ? 1.0f : 0.0f) : preset.patch.groove[group][field - 1];
            p->setValueNotifyingHost(p->convertTo0to1(value));
        }
    }
    void apply_ob6_groove_automation() {
        bool changed = false;
        for (int group = 0; group < 3; ++group) for (int field = 0; field < JUNO_GROOVE_FIELDS[size_t(group)]; ++field) {
            const auto i = size_t(group * 7 + field);
            const float value = ob6_groove_[group][field]->load();
            if (value != ob6_groove_last_[i]) { ob6_groove_last_[i] = value; changed = true; }
        }
        if (!changed) return;
        studio::Preset p;
        for (int group = 0; group < 3; ++group) {
            p.patch.groove_on[group] = ob6_groove_last_[size_t(group * 7)] >= .5f;
            for (int field = 1; field < JUNO_GROOVE_FIELDS[size_t(group)]; ++field)
                p.patch.groove[group][field - 1] = ob6_groove_last_[size_t(group * 7 + field)];
        }
        gf::apply_groove_preview(ob6_bus_, p, false);
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
    void publish_hybrid(const juce::MemoryBlock& raw) {
        hybrid_sequence_.fetch_add(1, std::memory_order_acq_rel);
        const auto* bytes = static_cast<const uint8_t*>(raw.getData());
        for (size_t i = 0; i < raw.getSize(); ++i) hybrid_bytes_[i].store(bytes[i], std::memory_order_relaxed);
        hybrid_size_.store(raw.getSize(), std::memory_order_relaxed);
        hybrid_sequence_.fetch_add(1, std::memory_order_release);
    }
    void apply_pending_hybrid() {
        const auto sequence = hybrid_sequence_.load(std::memory_order_acquire);
        if ((sequence & 1u) || sequence == hybrid_applied_sequence_) return;
        const size_t size = hybrid_size_.load(std::memory_order_relaxed);
        if (size > studio::LAYER_BYTES) return;
        std::array<uint8_t, studio::LAYER_BYTES> bytes{};
        for (size_t i = 0; i < size; ++i) bytes[i] = hybrid_bytes_[i].load(std::memory_order_relaxed);
        if (sequence != hybrid_sequence_.load(std::memory_order_acquire)) return;
        studio::LayerPreset layer;
        const bool valid = size ? gf::decode_hybrid_layer(bytes.data(), size, layer) : studio::factory_layer(0, &layer);
        if (valid) hybrid_.load(layer);
        hybrid_applied_sequence_ = sequence;
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
            studio::patch_init(arp2600_base_.patch, 5);
            arp2600_osc_last_.fill(std::numeric_limits<float>::quiet_NaN());
            arp2600_.set_patch(gf::Arp2600Patch{});
            studio::Preset empty_arp; studio::patch_init(empty_arp.patch, 5);
            gf::apply_synth_fx_preview(arp2600_bus_, empty_arp, 5); gf::apply_groove_preview(arp2600_bus_, empty_arp, false);
            engine_ = gf::ProphetPreview{};
            studio::patch_init(prophet_base_.patch, 2);
            engine_.prepare(sample_rate_);
            engine_.set_delay_memory(delay_left_.data(), delay_right_.data(), uint32_t(delay_left_.size()));
            engine_.set_plate_memory(plate_memory_.data(), plate_memory_.size());
        } else {
            studio::Preset preset;
            uint8_t preset_engine = 0;
            if (!gf::decode_supported_preset(bytes.data(), size, preset, preset_engine)) return;
            if (preset_engine == 5) {
                const auto initial_arp = arp2600_effective_controls(preset.patch);
                arp2600_.set_parameters(initial_arp[0], initial_arp[1], attack_->load(), initial_arp[2]);
                arp2600_base_ = preset;
                arp2600_groove_last_.fill(std::numeric_limits<float>::quiet_NaN());
                arp2600_fx_last_.fill(std::numeric_limits<float>::quiet_NaN());
                arp2600_osc_last_.fill(std::numeric_limits<float>::quiet_NaN());
                auto effective_arp = preset; write_arp2600_osc(effective_arp.patch); write_arp2600_voice(effective_arp.patch); write_arp2600_macros(effective_arp.patch);
                arp2600_.set_patch(gf::make_arp2600_patch(effective_arp));
                arp2600_.reset(); arp2600_bus_.reset();
                auto effects = preset;
                effects.patch = studio::move_sfx(preset.patch, studio::macro_amount(preset.patch, GF_MACRO_MOVE));
                gf::apply_synth_fx_preview(arp2600_bus_, effects, 5);
                gf::apply_groove_preview(arp2600_bus_, preset, false);
            } else if (preset_engine == 0) {
                const auto initial = modeld_effective_controls(preset.patch);
                modeld_.set_parameters(initial[0], initial[1], attack_->load(), initial[2]);
                modeld_.reset(); modeld_bus_.reset(); modeld_base_ = preset;
                modeld_osc_last_.fill(std::numeric_limits<float>::quiet_NaN());
                modeld_voice_last_.fill(std::numeric_limits<float>::quiet_NaN());
                modeld_groove_last_.fill(std::numeric_limits<float>::quiet_NaN());
                modeld_fx_last_.fill(std::numeric_limits<float>::quiet_NaN());
            } else if (preset_engine == 3) {
                ob6_.reset(); ob6_bus_.reset(); ob6_base_ = preset;
                ob6_osc_last_.fill(std::numeric_limits<float>::quiet_NaN());
                ob6_voice_last_.fill(std::numeric_limits<float>::quiet_NaN());
                ob6_fx_last_.fill(std::numeric_limits<float>::quiet_NaN());
                ob6_groove_last_.fill(std::numeric_limits<float>::quiet_NaN());
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
            else { engine_.reset(); gf::apply_prophet_preset_preview(engine_, preset); prophet_base_ = preset; }
        }
        applied_sequence_ = sequence;
    }
    gf::ProphetPreview engine_;
    gf::ProphetPreview dx_bus_;
    gf::ProphetPreview juno_bus_;
    gf::ProphetPreview ob6_bus_;
    gf::ProphetPreview modeld_bus_;
    gf::ProphetPreview arp2600_bus_;
    gf::Dx7Preview dx7_;
    gf::JunoPreview juno_;
    gf::Ob6Preview ob6_;
    gf::ModelDPreview modeld_;
    gf::Arp2600Preview arp2600_;
    gf::HybridPreview hybrid_;
    studio::Preset arp2600_base_;
    std::array<std::atomic<float>*, ARP2600_OSC.size()> arp2600_osc_{};
    std::array<float, ARP2600_OSC.size()> arp2600_osc_last_{};
    std::array<std::atomic<float>*, ARP2600_VOICE.size()> arp2600_voice_{};
    std::array<float, ARP2600_VOICE.size()> arp2600_voice_last_{};
    std::array<std::atomic<float>*, ARP2600_MACRO_IDS.size()> arp2600_macro_{};
    std::array<float, ARP2600_MACRO_IDS.size()> arp2600_macro_last_{};
    std::array<std::array<std::atomic<float>*, 7>, 3> arp2600_groove_{};
    std::array<float, 21> arp2600_groove_last_{};
    std::array<std::array<std::atomic<float>*, 5>, 5> arp2600_fx_{};
    std::array<float, 25> arp2600_fx_last_{};
    studio::Preset modeld_base_;
    std::array<std::atomic<float>*, MODELD_OSC.size()> modeld_osc_{};
    std::array<float, MODELD_OSC.size()> modeld_osc_last_{};
    std::array<std::atomic<float>*, MODELD_VOICE.size()> modeld_voice_{};
    std::array<float, MODELD_VOICE.size()> modeld_voice_last_{};
    std::array<std::array<std::atomic<float>*, 7>, 3> modeld_groove_{};
    std::array<float, 21> modeld_groove_last_{};
    std::array<std::array<std::atomic<float>*, 5>, 5> modeld_fx_{};
    std::array<float, 25> modeld_fx_last_{};
    std::array<std::atomic<float>*, MODELD_MACRO_IDS.size()> modeld_macro_{};
    std::array<float, MODELD_MACRO_IDS.size()> modeld_macro_last_{};
    float modeld_wheel_ = 0, modeld_pressure_ = 0;
    float arp2600_wheel_ = 0, arp2600_pressure_ = 0;
    float hybrid_wheel_ = 0, hybrid_pressure_ = 0;
    studio::Preset ob6_base_;
    std::array<std::atomic<float>*, OB6_OSC.size()> ob6_osc_{};
    std::array<float, OB6_OSC.size()> ob6_osc_last_{};
    std::array<std::atomic<float>*, OB6_VOICE.size()> ob6_voice_{};
    std::array<float, OB6_VOICE.size()> ob6_voice_last_{};
    std::array<std::atomic<float>*, OB6_MACRO_IDS.size()> ob6_macro_{};
    std::array<float, OB6_MACRO_IDS.size()> ob6_macro_last_{};
    float ob6_wheel_ = 0.0f, ob6_pressure_ = 0.0f;
    bool last_ob6_mode_ = false, last_modeld_mode_ = false, last_arp2600_mode_ = false, last_hybrid_mode_ = false;
    float juno_wheel_ = 0.0f;
    bool last_juno_mode_ = false;
    bool last_dx7_mode_ = false;
    float dx7_wheel_ = 0.0f;
    std::vector<int16_t> delay_left_, delay_right_;
    std::vector<int16_t> dx_delay_left_, dx_delay_right_;
    std::vector<int16_t> juno_delay_left_, juno_delay_right_;
    std::vector<int16_t> ob6_delay_left_, ob6_delay_right_;
    std::vector<int16_t> modeld_delay_left_, modeld_delay_right_;
    std::vector<int16_t> arp2600_delay_left_, arp2600_delay_right_;
    std::vector<int16_t> plate_memory_;
    std::vector<int16_t> dx_plate_memory_;
    std::vector<int16_t> juno_plate_memory_;
    std::vector<int16_t> ob6_plate_memory_;
    std::vector<int16_t> modeld_plate_memory_;
    std::vector<int16_t> arp2600_plate_memory_;
    juce::AudioProcessorValueTreeState state_;
    std::atomic<float>* cutoff_ = nullptr;
    std::atomic<float>* resonance_ = nullptr;
    std::atomic<float>* attack_ = nullptr;
    std::atomic<float>* release_ = nullptr;
    std::atomic<float>* lfo_sync_ = nullptr;
    std::atomic<float>* dx7_enabled_ = nullptr;
    std::atomic<float>* juno_enabled_ = nullptr;
    std::atomic<float>* ob6_enabled_ = nullptr;
    std::atomic<float>* modeld_enabled_ = nullptr;
    std::atomic<float>* hybrid_enabled_ = nullptr;
    OutputMeterState meter_state_;
    std::unique_ptr<BrainPrototype> preview_processor_;
    std::atomic<BrainPrototype*> preview_pointer_{nullptr};
    std::atomic<bool> preview_active_{false};
    std::atomic<double> preview_sample_rate_{44100.0};
    std::vector<float> preview_left_, preview_right_;
    float preview_mix_ = 0.0f;
    float dc_r_ = 0.99928796f;                 // exp(−2π·5/44100), por si se procesa antes de prepareToPlay
    std::array<float, 2> dc_x1_{}, dc_y1_{};
    std::array<std::atomic<bool>, 128> keyboard_desired_{};
    std::array<bool, 128> keyboard_playing_{}, host_note_playing_{};
    std::atomic<float>* hybrid_focus_ = nullptr;
    std::atomic<float>* hybrid_b_move_ = nullptr;
    std::atomic<float>* hybrid_morph_ = nullptr;
    std::atomic<float>* hybrid_delay_ = nullptr;
    std::atomic<float>* hybrid_transpose_ = nullptr;
    std::atomic<float>* hybrid_gain_a_ = nullptr;
    std::atomic<float>* hybrid_gain_b_ = nullptr;
    std::array<std::array<std::atomic<float>*, 4>, 2> hybrid_half_{};
    std::array<std::array<std::atomic<float>*, 20>, 2> hybrid_voice_{};
    std::array<std::array<std::atomic<float>*, 48>, 2> hybrid_osc_{};
    std::array<std::atomic<float>*, 7> hybrid_macro_{};
    std::array<std::array<std::atomic<float>*, 7>, 3> hybrid_groove_{};
    std::array<std::array<std::atomic<float>*, 5>, 5> hybrid_fx_{};
    std::atomic<float>* arp2600_enabled_ = nullptr;
    std::atomic<float>* arp2600_ring_ = nullptr;
    std::array<std::atomic<float>*, JUNO_OSC.size()> juno_osc_{};
    std::array<float, JUNO_OSC.size()> juno_osc_last_{};
    std::array<std::atomic<float>*, JUNO_ENV.size()> juno_env_{};
    std::array<float, JUNO_ENV.size()> juno_env_last_{};
    std::array<std::atomic<float>*, JUNO_LFO.size()> juno_lfo_{};
    std::array<float, JUNO_LFO.size()> juno_lfo_last_{};
    std::array<std::atomic<float>*, JUNO_VOICE_GROOVE.size()> juno_voice_groove_{};
    std::array<float, JUNO_VOICE_GROOVE.size()> juno_voice_groove_last_{};
    std::array<std::array<std::atomic<float>*, 5>, 5> ob6_fx_{};
    std::array<float, 25> ob6_fx_last_{};
    std::array<std::array<std::atomic<float>*, 7>, 3> ob6_groove_{};
    std::array<float, 21> ob6_groove_last_{};
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
    std::array<juce::RangedAudioParameter*, 6> arp2600_cc_parameters_{};
    std::array<std::atomic<float>, 6> arp2600_cc_values_{};
    std::array<std::atomic<uint64_t>, 6> arp2600_cc_sequence_{}, arp2600_cc_delivered_{};
    std::array<std::atomic<bool>, 2> arp2600_filter_override_{};
    std::array<std::atomic<float>, 2> arp2600_filter_raw_{}, arp2600_filter_expected_{}, arp2600_filter_origin_{};
    std::array<juce::RangedAudioParameter*, 6> modeld_cc_parameters_{};
    std::array<std::atomic<float>, 6> modeld_cc_values_{};
    std::array<std::atomic<uint64_t>, 6> modeld_cc_sequence_{}, modeld_cc_delivered_{};
    std::array<std::atomic<bool>, 2> modeld_filter_override_{};
    std::array<std::atomic<float>, 2> modeld_filter_raw_{}, modeld_filter_expected_{}, modeld_filter_origin_{};
    std::array<juce::RangedAudioParameter*, 6> ob6_cc_parameters_{};
    std::array<std::atomic<float>, 6> ob6_cc_values_{};
    std::array<std::atomic<uint64_t>, 6> ob6_cc_sequence_{}, ob6_cc_delivered_{};
    std::array<std::atomic<bool>, 2> ob6_filter_override_{};
    std::array<std::atomic<float>, 2> ob6_filter_raw_{}, ob6_filter_expected_{}, ob6_filter_origin_{};
    std::array<juce::RangedAudioParameter*, 6> juno_cc_parameters_{};
    std::array<std::atomic<float>, 6> juno_cc_values_{};
    std::array<std::atomic<uint64_t>, 6> juno_cc_sequence_{}, juno_cc_delivered_{};
    std::array<std::atomic<bool>, 2> juno_filter_override_{};
    std::array<std::atomic<float>, 2> juno_filter_raw_{}, juno_filter_expected_{}, juno_filter_origin_{};
    studio::Preset prophet_base_;                                         ///< preset Prophet aplicado (hilo de audio)
    std::array<juce::RangedAudioParameter*, 2> prophet_cc_parameters_{};  ///< cutoff (CC 74), resonance (CC 71)
    std::array<std::atomic<float>, 2> prophet_cc_values_{};
    std::array<std::atomic<uint64_t>, 2> prophet_cc_sequence_{}, prophet_cc_delivered_{};
    std::array<juce::RangedAudioParameter*, 6> cc_parameters_{};       ///< DX7: DRIFT, GRIT, POCKET, MOVE, CUTOFF (74), RESONANCE (71)
    std::array<std::atomic<float>, 6> cc_values_{};
    std::array<std::atomic<uint64_t>, 6> cc_sequence_{}, cc_delivered_{};
    double sample_rate_ = 44100.0;
    std::mutex preset_mutex_;
    juce::MemoryBlock preset_raw_;
    juce::String preset_name_;
    juce::MemoryBlock hybrid_raw_;
    std::array<juce::MemoryBlock, studio::LAYER_MAX> hybrid_bank_{};
    uint8_t hybrid_bank_count_ = 0;
    std::atomic<uint64_t> hybrid_bank_revision_{0};
    std::array<std::atomic<uint8_t>, studio::LAYER_BYTES> hybrid_bytes_{};
    std::atomic<size_t> hybrid_size_{0};
    std::atomic<uint64_t> hybrid_sequence_{0};
    std::atomic<uint64_t> hybrid_revision_{0};
    uint64_t hybrid_applied_sequence_ = 0;
    std::atomic<float> hybrid_b_move_value_{0}, hybrid_b_move_origin_{0};
    std::atomic<uint64_t> hybrid_b_move_sequence_{0}, hybrid_b_move_delivered_{0};
    std::array<juce::RangedAudioParameter*, 4> hybrid_macro_parameters_{};
    std::array<std::atomic<float>, 4> hybrid_macro_values_{}, hybrid_macro_origin_{};
    std::array<std::atomic<uint64_t>, 4> hybrid_macro_sequence_{}, hybrid_macro_delivered_{};
    std::array<juce::RangedAudioParameter*, 4> hybrid_filter_parameters_{};
    std::array<std::atomic<float>, 4> hybrid_filter_values_{}, hybrid_filter_origin_{};
    std::array<std::atomic<uint64_t>, 4> hybrid_filter_sequence_{}, hybrid_filter_delivered_{};
    std::atomic<uint8_t> hybrid_bank_value_{0};
    std::atomic<uint64_t> hybrid_bank_sequence_{0}, hybrid_bank_delivered_{0};
    std::atomic<float> hybrid_morph_value_{.5f}, hybrid_morph_origin_{.5f};
    std::atomic<uint64_t> hybrid_morph_sequence_{0}, hybrid_morph_delivered_{0};
    std::array<std::atomic<uint8_t>, studio::PRESET_BYTES> preset_bytes_{};
    std::atomic<size_t> preset_size_{0};
    std::atomic<uint64_t> preset_sequence_{0};
    uint64_t applied_sequence_ = 0;
};

class BrainEditor final : public juce::AudioProcessorEditor, private juce::Timer, private juce::MidiKeyboardStateListener {
public:
    explicit BrainEditor(BrainPrototype& owner) : AudioProcessorEditor(owner), owner_(owner), output_meter_(owner.output_meter_state()), keyboard_(keyboard_state_),
        juno_amp_envelope_(owner.parameters(), "AMP ENV", {"attack", "juno.amp_decay", "juno.amp_sustain", "release"}),
        juno_filter_envelope_(owner.parameters(), "FILTER ENV", {"juno.filter_attack", "juno.filter_decay", "juno.filter_sustain", "juno.filter_release"}),
        ob6_amp_envelope_(owner.parameters(), "AMP ENV", {"attack", "ob6.amp_decay", "ob6.amp_sustain", "release"}),
        ob6_filter_envelope_(owner.parameters(), "FILTER ENV", {"ob6.filter_attack", "ob6.filter_decay", "ob6.filter_sustain", "ob6.filter_release"}),
        modeld_amp_envelope_(owner.parameters(), "AMP ENV", {"attack", "modeld.amp_decay", "modeld.amp_sustain", "release"}),
        modeld_filter_envelope_(owner.parameters(), "FILTER ENV", {"modeld.filter_attack", "modeld.filter_decay", "modeld.filter_sustain", "modeld.filter_release"}),
        arp_amp_envelope_(owner.parameters(), "AMP ENV", {"attack", "arp2600.amp_decay", "arp2600.amp_sustain", "release"}),
        arp_filter_envelope_(owner.parameters(), "FILTER ENV", {"arp2600.filter_attack", "arp2600.filter_decay", "arp2600.filter_sustain", "arp2600.filter_release"}),
        dx_amp_envelope_(owner.parameters(), "GLOBAL AMP ENV", {"dx7.amp_attack", "dx7.amp_decay", "dx7.amp_sustain", "dx7.amp_release"}),
        hybrid_a_amp_envelope_(owner.parameters(), "A AMP ENV", {"hybrid.a.attack", "hybrid.a.voice.decay", "hybrid.a.voice.sustain", "hybrid.a.release"}),
        hybrid_a_filter_envelope_(owner.parameters(), "A FILTER ENV", {"hybrid.a.voice.filter_attack", "hybrid.a.voice.filter_decay", "hybrid.a.voice.filter_sustain", "hybrid.a.voice.filter_release"}),
        hybrid_b_amp_envelope_(owner.parameters(), "B AMP ENV", {"hybrid.b.attack", "hybrid.b.voice.decay", "hybrid.b.voice.sustain", "hybrid.b.release"}),
        hybrid_b_filter_envelope_(owner.parameters(), "B FILTER ENV", {"hybrid.b.voice.filter_attack", "hybrid.b.voice.filter_decay", "hybrid.b.voice.filter_sustain", "hybrid.b.voice.filter_release"}),
        juno_lfo_display_(owner.parameters(), "juno.lfo_shape", "juno.lfo_rate"),
        ob6_lfo_display_(owner.parameters(), "ob6.lfo_shape", "ob6.lfo_rate"),
        modeld_lfo_display_(owner.parameters(), "modeld.lfo_shape", "modeld.lfo_rate"),
        arp_lfo_display_(owner.parameters(), "arp2600.lfo_shape", "arp2600.lfo_rate"),
        dx_lfo_display_(owner.parameters(), "dx7.lfo_shape", "dx7.lfo_rate"),
        hybrid_a_lfo_display_(owner.parameters(), "hybrid.a.voice.lfo_shape", "hybrid.a.voice.lfo_rate"),
        hybrid_b_lfo_display_(owner.parameters(), "hybrid.b.voice.lfo_shape", "hybrid.b.voice.lfo_rate") {
        setLookAndFeel(&studio_look_);
        studio_look_.set_hybrid(false);
        juno_amp_envelope_.set_peer(&juno_filter_envelope_);
        juno_filter_envelope_.set_peer(&juno_amp_envelope_);
        juno_amp_envelope_.setComponentID("juno.env.graph.amp");
        juno_filter_envelope_.setComponentID("juno.env.graph.filter");
        addChildComponent(juno_amp_envelope_); addChildComponent(juno_filter_envelope_);
        ob6_amp_envelope_.set_peer(&ob6_filter_envelope_); ob6_filter_envelope_.set_peer(&ob6_amp_envelope_);
        modeld_amp_envelope_.set_peer(&modeld_filter_envelope_); modeld_filter_envelope_.set_peer(&modeld_amp_envelope_);
        ob6_amp_envelope_.setComponentID("ob6.env.graph.amp"); ob6_filter_envelope_.setComponentID("ob6.env.graph.filter");
        modeld_amp_envelope_.setComponentID("modeld.env.graph.amp"); modeld_filter_envelope_.setComponentID("modeld.env.graph.filter");
        addChildComponent(ob6_amp_envelope_); addChildComponent(ob6_filter_envelope_);
        addChildComponent(modeld_amp_envelope_); addChildComponent(modeld_filter_envelope_);
        arp_amp_envelope_.set_peer(&arp_filter_envelope_); arp_filter_envelope_.set_peer(&arp_amp_envelope_);
        arp_amp_envelope_.setComponentID("arp2600.env.graph.amp"); arp_filter_envelope_.setComponentID("arp2600.env.graph.filter");
        dx_amp_envelope_.setComponentID("dx7.env.graph.amp");
        addChildComponent(arp_amp_envelope_); addChildComponent(arp_filter_envelope_); addChildComponent(dx_amp_envelope_);
        hybrid_a_amp_envelope_.set_peer(&hybrid_a_filter_envelope_); hybrid_a_filter_envelope_.set_peer(&hybrid_a_amp_envelope_);
        hybrid_b_amp_envelope_.set_peer(&hybrid_b_filter_envelope_); hybrid_b_filter_envelope_.set_peer(&hybrid_b_amp_envelope_);
        hybrid_a_amp_envelope_.setComponentID("hybrid.a.env.graph.amp"); hybrid_a_filter_envelope_.setComponentID("hybrid.a.env.graph.filter");
        hybrid_b_amp_envelope_.setComponentID("hybrid.b.env.graph.amp"); hybrid_b_filter_envelope_.setComponentID("hybrid.b.env.graph.filter");
        addChildComponent(hybrid_a_amp_envelope_); addChildComponent(hybrid_a_filter_envelope_);
        addChildComponent(hybrid_b_amp_envelope_); addChildComponent(hybrid_b_filter_envelope_);
        juno_lfo_display_.setComponentID("juno.lfo.display"); addChildComponent(juno_lfo_display_);
        ob6_lfo_display_.setComponentID("ob6.lfo.display"); modeld_lfo_display_.setComponentID("modeld.lfo.display");
        arp_lfo_display_.setComponentID("arp2600.lfo.display");
        addChildComponent(ob6_lfo_display_); addChildComponent(modeld_lfo_display_); addChildComponent(arp_lfo_display_);
        dx_lfo_display_.setComponentID("dx7.lfo.display"); hybrid_a_lfo_display_.setComponentID("hybrid.a.lfo.display");
        hybrid_b_lfo_display_.setComponentID("hybrid.b.lfo.display");
        addChildComponent(dx_lfo_display_); addChildComponent(hybrid_a_lfo_display_); addChildComponent(hybrid_b_lfo_display_);
        addAndMakeVisible(output_meter_);
        keyboard_state_.addListener(this);
        keyboard_.setComponentID("keyboard.keys");
        keyboard_.setAvailableRange(48, 71);
        keyboard_.setColour(juce::MidiKeyboardComponent::keyDownOverlayColourId, juce::Colour(0xff5dcaa5));
        addChildComponent(keyboard_);
        keyboard_toggle_.setComponentID("keyboard.toggle");
        keyboard_toggle_.setButtonText("KEYS ▴");
        keyboard_toggle_.onClick = [this] {
            keyboard_open_ = !keyboard_open_;
            if (!keyboard_open_) { keyboard_state_.reset(); owner_.clear_keyboard_notes(); }
            keyboard_.setVisible(keyboard_open_);
            keyboard_toggle_.setButtonText(keyboard_open_ ? "KEYS ▾" : "KEYS ▴");
            const double ratio = keyboard_open_ ? 1200.0 / 960.0 : 1200.0 / 760.0;
            getConstrainer()->setFixedAspectRatio(ratio);
            setSize(getWidth(), int(std::lround(double(getWidth()) / ratio)));
        };
        addAndMakeVisible(keyboard_toggle_);
        preset_browser_.setComponentID("preset.browser");
        preset_browser_.setTextWhenNothingSelected("PRESETS · FACTORY");
        preset_browser_.onChange = [this] {
            const int index = preset_browser_.getSelectedId() - 1;
            if (index < 0) return;
            bool loaded = false;
            if (browser_engine_ == 6) loaded = owner_.load_hybrid_bank(unsigned(index));
            else if (browser_engine_ >= 0 && browser_engine_ < 6) {
                studio::Preset preset; std::array<uint8_t, studio::PRESET_BYTES> data{};
                const auto engine = uint8_t(browser_engine_);
                if (studio::factory_preset(engine, uint8_t(index), &preset)) {
                    const auto size = studio::preset_encode(preset, engine, data.data(), data.size());
                    loaded = size > 0 && owner_.import_preset(data.data(), size);
                }
            }
            if (loaded) update_status();
        };
        addAndMakeVisible(preset_browser_);
        browse_button_.setComponentID("browser.open"); browse_button_.setButtonText("BROWSE");
        browse_button_.onClick = [this] {
            juce::StringArray names, tags; std::vector<bool> favourites;
            for (int i = 0; i < preset_browser_.getNumItems(); ++i) {
                names.add(preset_browser_.getItemText(i));
                const juce::Identifier key("browserFav" + juce::String(browser_engine_) + "_" + juce::String(i));
                favourites.push_back(bool(owner_.parameters().state.getProperty(key, false)));
                const juce::Identifier tag_key("browserTags" + juce::String(browser_engine_) + "_" + juce::String(i));
                tags.add(owner_.parameters().state.getProperty(tag_key, {}).toString());
            }
            browser_panel_.set_items(names, favourites, tags);
            browser_panel_.setVisible(true); browser_panel_.toFront(false);
        };
        browser_panel_.on_favourite = [this](int index, bool enabled) {
            const juce::Identifier key("browserFav" + juce::String(browser_engine_) + "_" + juce::String(index));
            owner_.parameters().state.setProperty(key, enabled, nullptr);
        };
        browser_panel_.on_tags = [this](int index, juce::String tags) {
            const juce::Identifier key("browserTags" + juce::String(browser_engine_) + "_" + juce::String(index));
            owner_.parameters().state.setProperty(key, tags, nullptr);
        };
        browser_panel_.on_preview = [this](int index) { return owner_.preview_bank_item(browser_engine_, unsigned(index)); };
        browser_panel_.on_cancel_preview = [this] { owner_.stop_preset_preview(); };
        browser_panel_.on_load = [this](int index) {
            owner_.stop_preset_preview();
            preset_browser_.setSelectedId(index + 1, juce::sendNotificationSync);
            browser_panel_.setVisible(false);
        };
        addAndMakeVisible(browse_button_); addChildComponent(browser_panel_);
        for (size_t i = 0; i < page_tabs_.size(); ++i) {
            auto& tab = page_tabs_[i];
            tab.setComponentID("navigation.tab." + juce::String(int(i)));
            tab.onClick = [this, i] {
                if (hybrid_toggle_.getToggleState()) hybrid_page_index_ = int(i);
                else if (arp2600_toggle_.getToggleState()) arp2600_page_index_ = int(i);
                else if (modeld_toggle_.getToggleState()) modeld_page_index_ = int(i);
                else if (ob6_toggle_.getToggleState()) ob6_page_index_ = int(i);
                else if (juno_toggle_.getToggleState()) juno_page_index_ = int(i);
                else if (dx7_toggle_.getToggleState()) dx_page_index_ = int(i);
                update_status();
            };
            addChildComponent(tab);
        }
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
            compact_slider_text(knob);
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
        juno_toggle_.setComponentID("engine.juno");
        juno_toggle_.setColour(juce::ToggleButton::textColourId, juce::Colour(0xff9fe1cb));
        addAndMakeVisible(juno_toggle_);
        juno_attachment_ = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(owner.parameters(), "juno_preview", juno_toggle_);
        ob6_toggle_.setButtonText("OB-6 PREVIEW");
        ob6_toggle_.setComponentID("engine.ob6");
        ob6_toggle_.setColour(juce::ToggleButton::textColourId, juce::Colour(0xff9fe1cb));
        addAndMakeVisible(ob6_toggle_);
        ob6_attachment_ = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(owner.parameters(), "ob6_preview", ob6_toggle_);
        modeld_toggle_.setButtonText("MODEL D");
        modeld_toggle_.setColour(juce::ToggleButton::textColourId, juce::Colour(0xff9fe1cb));
        addAndMakeVisible(modeld_toggle_);
        modeld_attachment_ = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(owner.parameters(), "modeld_preview", modeld_toggle_);
        hybrid_toggle_.setButtonText("HYBRID PREVIEW");
        hybrid_toggle_.setComponentID("engine.hybrid");
        hybrid_toggle_.setColour(juce::ToggleButton::textColourId, juce::Colour(0xff9fe1cb)); addAndMakeVisible(hybrid_toggle_);
        hybrid_attachment_ = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(owner.parameters(), "hybrid_preview", hybrid_toggle_);
        hybrid_toggle_.onClick = [this] { update_status(); };
        hybrid_focus_combo_.setComponentID("hybrid.focus"); hybrid_focus_combo_.addItem("FOCO A", 1); hybrid_focus_combo_.addItem("FOCO B", 2);
        addChildComponent(hybrid_focus_combo_);
        hybrid_focus_attachment_ = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(owner.parameters(), "hybrid.focus", hybrid_focus_combo_);
        hybrid_page_.setComponentID("hybrid.page");
        hybrid_page_.onClick = [this] { hybrid_page_index_ = (hybrid_page_index_ + 1) % 9; update_status(); };
        addChildComponent(hybrid_page_);
        hybrid_factory_.setComponentID("hybrid.factory");
        hybrid_factory_.setTextWhenNothingSelected("CAPAS DE FÁBRICA");
        hybrid_factory_.onChange = [this] {
            const int index = hybrid_factory_.getSelectedId() - 1;
            if (index >= 0) { owner_.load_hybrid_bank(unsigned(index)); update_status(); }
        };
        hybrid_save_.setButtonText("GUARDAR CAPA"); hybrid_save_.setComponentID("hybrid.save");
        hybrid_save_.onClick = [this] {
            const bool ok = owner_.save_hybrid_bank(); update_status();
            status_.setText(ok ? "CAPA GUARDADA EN EL PROYECTO" : "NO SE PUDO GUARDAR · BANCO LLENO O LFO SYNC ACTIVO", juce::dontSendNotification);
        };
        addChildComponent(hybrid_save_);
        addChildComponent(hybrid_factory_);
        hybrid_half_a_.setButtonText("A: CARGAR GFP"); hybrid_half_a_.setComponentID("hybrid.load_a");
        hybrid_half_b_.setButtonText("B: CARGAR GFP"); hybrid_half_b_.setComponentID("hybrid.load_b");
        hybrid_half_a_.onClick = [this] { choose_hybrid_half(0); }; hybrid_half_b_.onClick = [this] { choose_hybrid_half(1); };
        addChildComponent(hybrid_half_a_); addChildComponent(hybrid_half_b_);
        for (size_t side = 0; side < hybrid_focus_buttons_.size(); ++side) {
            auto& button = hybrid_focus_buttons_[side];
            button.setComponentID("hybrid.card." + juce::String(int(side)));
            button.onClick = [this, side] {
                if (auto* parameter = owner_.parameters().getParameter("hybrid.focus"))
                    parameter->setValueNotifyingHost(float(side));
                update_status();
            };
            addChildComponent(button);
        }
        for (int side = 0; side < 2; ++side) for (size_t i = 0; i < HYBRID_VOICE.size(); ++i) {
            const auto id = juce::String(side == 0 ? "hybrid.a.voice." : "hybrid.b.voice.") + HYBRID_VOICE[i].id;
            auto& label = hybrid_voice_labels_[side][i]; auto& slider = hybrid_voice_sliders_[side][i];
            label.setText(HYBRID_VOICE[i].label, juce::dontSendNotification);
            label.setColour(juce::Label::textColourId, juce::Colour(0xff9fe1cb)); label.setFont(juce::Font(juce::FontOptions(10.0f))); addChildComponent(label);
            slider.setComponentID(id); slider.setSliderStyle(juce::Slider::LinearHorizontal);
            slider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 60, 19); addChildComponent(slider);
            hybrid_voice_attachments_[side][i] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(owner.parameters(), id, slider);
            compact_slider_text(slider);
        }
        hybrid_osc_stop_.setComponentID("hybrid.osc.stop");
        for (int i = 0; i < 6; ++i) hybrid_osc_stop_.addItem(i < 4 ? "OSC " + juce::String(i + 1) : "MOD " + juce::String(i - 3), i + 1);
        hybrid_osc_stop_.setSelectedId(1, juce::dontSendNotification);
        hybrid_osc_stop_.onChange = [this] { hybrid_osc_attached_key_ = -1; update_status(); };
        addChildComponent(hybrid_osc_stop_);
        for (size_t i = 0; i < hybrid_osc_sliders_.size(); ++i) {
            hybrid_osc_labels_[i].setColour(juce::Label::textColourId, juce::Colour(0xff9fe1cb)); addChildComponent(hybrid_osc_labels_[i]);
            hybrid_osc_sliders_[i].setComponentID("hybrid.osc.slot." + juce::String(int(i)));
            hybrid_osc_sliders_[i].setSliderStyle(juce::Slider::LinearHorizontal);
            hybrid_osc_sliders_[i].setTextBoxStyle(juce::Slider::TextBoxRight, false, 65, 22); addChildComponent(hybrid_osc_sliders_[i]);
        }
        constexpr const char* macro_names[] = {"DRIFT", "GRIT", "POCKET", "MOVE A", "GRIT RES", "GRIT MOD", "POCKET VEL", "MOVE B"};
        for (size_t i = 0; i < hybrid_macro_sliders_.size(); ++i) {
            hybrid_macro_labels_[i].setText(macro_names[i], juce::dontSendNotification);
            hybrid_macro_labels_[i].setColour(juce::Label::textColourId, juce::Colour(0xff9fe1cb)); addChildComponent(hybrid_macro_labels_[i]);
            const char* id = i < HYBRID_MACRO_IDS.size() ? HYBRID_MACRO_IDS[i] : "hybrid.b.move";
            hybrid_macro_sliders_[i].setComponentID(id); hybrid_macro_sliders_[i].setSliderStyle(juce::Slider::LinearHorizontal);
            hybrid_macro_sliders_[i].setTextBoxStyle(juce::Slider::TextBoxRight, false, 65, 22); addChildComponent(hybrid_macro_sliders_[i]);
            hybrid_macro_attachments_[i] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(owner.parameters(), id, hybrid_macro_sliders_[i]);
            compact_slider_text(hybrid_macro_sliders_[i]);
        }
        constexpr const char* half_ids[] = {"hybrid.a.cutoff", "hybrid.a.resonance", "hybrid.a.attack", "hybrid.a.release",
                                               "hybrid.b.cutoff", "hybrid.b.resonance", "hybrid.b.attack", "hybrid.b.release"};
        constexpr const char* half_names[] = {"A CUTOFF", "A RESONANCE", "A ATTACK", "A RELEASE", "B CUTOFF", "B RESONANCE", "B ATTACK", "B RELEASE"};
        for (size_t i = 0; i < hybrid_half_sliders_.size(); ++i) {
            hybrid_half_labels_[i].setText(half_names[i], juce::dontSendNotification);
            hybrid_half_labels_[i].setColour(juce::Label::textColourId, juce::Colour(0xff9fe1cb)); addChildComponent(hybrid_half_labels_[i]);
            hybrid_half_sliders_[i].setComponentID(half_ids[i]); hybrid_half_sliders_[i].setSliderStyle(juce::Slider::LinearHorizontal);
            hybrid_half_sliders_[i].setTextBoxStyle(juce::Slider::TextBoxRight, false, 65, 22); addChildComponent(hybrid_half_sliders_[i]);
            hybrid_half_attachments_[i] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(owner.parameters(), half_ids[i], hybrid_half_sliders_[i]);
            compact_slider_text(hybrid_half_sliders_[i]);
        }
        constexpr const char* hybrid_ids[] = {"hybrid.morph", "hybrid.delay", "hybrid.transpose", "hybrid.gain_a", "hybrid.gain_b"};
        constexpr const char* hybrid_names[] = {"MORPH", "DELAY B ms", "TRANSPOSE B", "GAIN A dB", "GAIN B dB"};
        for (size_t i = 0; i < hybrid_sliders_.size(); ++i) {
            hybrid_labels_[i].setText(hybrid_names[i], juce::dontSendNotification);
            hybrid_labels_[i].setColour(juce::Label::textColourId, juce::Colour(0xff9fe1cb)); addChildComponent(hybrid_labels_[i]);
            hybrid_sliders_[i].setComponentID(hybrid_ids[i]); hybrid_sliders_[i].setSliderStyle(juce::Slider::LinearHorizontal);
            hybrid_sliders_[i].setTextBoxStyle(juce::Slider::TextBoxRight, false, 65, 22); addChildComponent(hybrid_sliders_[i]);
            hybrid_attachments_[i] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(owner.parameters(), hybrid_ids[i], hybrid_sliders_[i]);
            compact_slider_text(hybrid_sliders_[i]);
        }
        arp2600_toggle_.setButtonText("ARP 2600 PREVIEW");
        arp2600_toggle_.setComponentID("engine.arp2600");
        arp2600_toggle_.setColour(juce::ToggleButton::textColourId, juce::Colour(0xff9fe1cb));
        addAndMakeVisible(arp2600_toggle_);
        arp2600_attachment_ = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(owner.parameters(), "arp2600_preview", arp2600_toggle_);
        arp2600_toggle_.onClick = [this] { if (arp2600_toggle_.getToggleState()) { hybrid_toggle_.setToggleState(false, juce::sendNotification); modeld_toggle_.setToggleState(false, juce::sendNotification); juno_toggle_.setToggleState(false, juce::sendNotification); dx7_toggle_.setToggleState(false, juce::sendNotification); ob6_toggle_.setToggleState(false, juce::sendNotification); } update_status(); };
        arp2600_ring_label_.setText("RING", juce::dontSendNotification);
        arp2600_ring_label_.setColour(juce::Label::textColourId, juce::Colour(0xff9fe1cb));
        addChildComponent(arp2600_ring_label_);
        arp2600_ring_slider_.setComponentID("arp2600.ring");
        arp2600_ring_slider_.setSliderStyle(juce::Slider::LinearHorizontal);
        arp2600_ring_slider_.setTextBoxStyle(juce::Slider::TextBoxRight, false, 60, 19);
        addChildComponent(arp2600_ring_slider_);
        arp2600_ring_attachment_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(owner.parameters(), "arp2600.ring", arp2600_ring_slider_);
        modeld_toggle_.onClick = [this] { if (modeld_toggle_.getToggleState()) { hybrid_toggle_.setToggleState(false, juce::sendNotification); arp2600_toggle_.setToggleState(false, juce::sendNotification); juno_toggle_.setToggleState(false, juce::sendNotification); dx7_toggle_.setToggleState(false, juce::sendNotification); ob6_toggle_.setToggleState(false, juce::sendNotification); } };
        juno_toggle_.onClick = [this] { if (juno_toggle_.getToggleState()) { hybrid_toggle_.setToggleState(false, juce::sendNotification); arp2600_toggle_.setToggleState(false, juce::sendNotification); modeld_toggle_.setToggleState(false, juce::sendNotification); dx7_toggle_.setToggleState(false, juce::sendNotification); ob6_toggle_.setToggleState(false, juce::sendNotification); } };
        dx7_toggle_.onClick = [this] { if (dx7_toggle_.getToggleState()) { hybrid_toggle_.setToggleState(false, juce::sendNotification); arp2600_toggle_.setToggleState(false, juce::sendNotification); modeld_toggle_.setToggleState(false, juce::sendNotification); juno_toggle_.setToggleState(false, juce::sendNotification); ob6_toggle_.setToggleState(false, juce::sendNotification); } };
        ob6_toggle_.onClick = [this] { if (ob6_toggle_.getToggleState()) { hybrid_toggle_.setToggleState(false, juce::sendNotification); arp2600_toggle_.setToggleState(false, juce::sendNotification); modeld_toggle_.setToggleState(false, juce::sendNotification); juno_toggle_.setToggleState(false, juce::sendNotification); dx7_toggle_.setToggleState(false, juce::sendNotification); } };
        juno_page_.setButtonText("OSC");
        juno_page_.setComponentID("engine.page");
        juno_page_.setTooltip("Cambiar de página de edición");
        juno_page_.onClick = [this] { if (arp2600_toggle_.getToggleState()) arp2600_page_index_ = (arp2600_page_index_ + 1) % 6; else if (modeld_toggle_.getToggleState()) modeld_page_index_ = (modeld_page_index_ + 1) % 6; else if (ob6_toggle_.getToggleState()) ob6_page_index_ = (ob6_page_index_ + 1) % 6; else juno_page_index_ = (juno_page_index_ + 1) % 8; update_status(); };
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
            juno_macro_sliders_[i].setComponentID("shared.macro." + juce::String(int(i)));
            juno_macro_sliders_[i].setTextBoxStyle(juce::Slider::TextBoxRight, false, 65, 22);
            juno_macro_sliders_[i].setColour(juce::Slider::textBoxTextColourId, juce::Colour(0xfff2f2f2));
            addChildComponent(juno_macro_sliders_[i]);
            juno_macro_attachments_[i] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(owner.parameters(), JUNO_MACRO_IDS[i], juno_macro_sliders_[i]);
            compact_slider_text(juno_macro_sliders_[i]);
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
        for (size_t i = 0; i < OB6_OSC.size(); ++i) {
            ob6_osc_labels_[i].setText(OB6_OSC[i].label, juce::dontSendNotification);
            ob6_osc_labels_[i].setColour(juce::Label::textColourId, juce::Colour(0xff9fe1cb));
            ob6_osc_labels_[i].setFont(juce::Font(juce::FontOptions(11.0f)));
            addChildComponent(ob6_osc_labels_[i]);
            ob6_osc_sliders_[i].setSliderStyle(juce::Slider::LinearHorizontal);
            ob6_osc_sliders_[i].setComponentID(OB6_OSC[i].id);
            ob6_osc_sliders_[i].setTextBoxStyle(juce::Slider::TextBoxRight, false, 65, 20);
            ob6_osc_sliders_[i].setColour(juce::Slider::textBoxTextColourId, juce::Colour(0xfff2f2f2));
            addChildComponent(ob6_osc_sliders_[i]);
            ob6_osc_attachments_[i] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(owner.parameters(), OB6_OSC[i].id, ob6_osc_sliders_[i]);
            compact_slider_text(ob6_osc_sliders_[i]);
        }
        for (size_t i = 0; i < OB6_VOICE.size(); ++i) {
            ob6_voice_labels_[i].setText(OB6_VOICE[i].label, juce::dontSendNotification);
            ob6_voice_labels_[i].setColour(juce::Label::textColourId, juce::Colour(0xff9fe1cb));
            ob6_voice_labels_[i].setFont(juce::Font(juce::FontOptions(10.0f)));
            addChildComponent(ob6_voice_labels_[i]);
            ob6_voice_sliders_[i].setSliderStyle(juce::Slider::LinearHorizontal);
            ob6_voice_sliders_[i].setComponentID(OB6_VOICE[i].id);
            ob6_voice_sliders_[i].setTextBoxStyle(juce::Slider::TextBoxRight, false, 60, 19);
            ob6_voice_sliders_[i].setColour(juce::Slider::textBoxTextColourId, juce::Colour(0xfff2f2f2));
            addChildComponent(ob6_voice_sliders_[i]);
            ob6_voice_attachments_[i] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(owner.parameters(), OB6_VOICE[i].id, ob6_voice_sliders_[i]);
            compact_slider_text(ob6_voice_sliders_[i]);
        }
        for (size_t i = 0; i < ARP2600_VOICE.size(); ++i) {
            arp2600_voice_labels_[i].setText(ARP2600_VOICE[i].label, juce::dontSendNotification);
            arp2600_voice_labels_[i].setColour(juce::Label::textColourId, juce::Colour(0xff9fe1cb));
            arp2600_voice_labels_[i].setFont(juce::Font(juce::FontOptions(10.0f)));
            addChildComponent(arp2600_voice_labels_[i]);
            auto& slider = arp2600_voice_sliders_[i];
            slider.setSliderStyle(juce::Slider::LinearHorizontal); slider.setComponentID(ARP2600_VOICE[i].id);
            slider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 60, 18);
            slider.setColour(juce::Slider::textBoxTextColourId, juce::Colour(0xfff2f2f2)); addChildComponent(slider);
            arp2600_voice_attachments_[i] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(owner.parameters(), ARP2600_VOICE[i].id, slider);
            compact_slider_text(slider);
        }
        for (size_t i = 0; i < ARP2600_OSC.size(); ++i) {
            arp2600_osc_labels_[i].setText(ARP2600_OSC[i].label, juce::dontSendNotification);
            arp2600_osc_labels_[i].setColour(juce::Label::textColourId, juce::Colour(0xff9fe1cb));
            arp2600_osc_labels_[i].setFont(juce::Font(juce::FontOptions(10.0f)));
            addChildComponent(arp2600_osc_labels_[i]);
            auto& slider = arp2600_osc_sliders_[i];
            slider.setSliderStyle(juce::Slider::LinearHorizontal); slider.setComponentID(ARP2600_OSC[i].id);
            slider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 60, 19);
            slider.setColour(juce::Slider::textBoxTextColourId, juce::Colour(0xfff2f2f2)); addChildComponent(slider);
            arp2600_osc_attachments_[i] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(owner.parameters(), ARP2600_OSC[i].id, slider);
            compact_slider_text(slider);
        }
        for (size_t i = 0; i < MODELD_OSC.size(); ++i) {
            modeld_osc_labels_[i].setText(MODELD_OSC[i].label, juce::dontSendNotification);
            modeld_osc_labels_[i].setColour(juce::Label::textColourId, juce::Colour(0xff9fe1cb));
            modeld_osc_labels_[i].setFont(juce::Font(juce::FontOptions(10.0f)));
            addChildComponent(modeld_osc_labels_[i]);
            auto& slider = modeld_osc_sliders_[i];
            slider.setSliderStyle(juce::Slider::LinearHorizontal);
            slider.setComponentID(MODELD_OSC[i].id);
            slider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 60, 19);
            slider.setColour(juce::Slider::textBoxTextColourId, juce::Colour(0xfff2f2f2));
            addChildComponent(slider);
            modeld_osc_attachments_[i] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(owner.parameters(), MODELD_OSC[i].id, slider);
            compact_slider_text(slider);
        }
        for (size_t i = 0; i < MODELD_VOICE.size(); ++i) {
            modeld_voice_labels_[i].setText(MODELD_VOICE[i].label, juce::dontSendNotification);
            modeld_voice_labels_[i].setColour(juce::Label::textColourId, juce::Colour(0xff9fe1cb));
            modeld_voice_labels_[i].setFont(juce::Font(juce::FontOptions(10.0f)));
            addChildComponent(modeld_voice_labels_[i]);
            auto& slider = modeld_voice_sliders_[i];
            slider.setSliderStyle(juce::Slider::LinearHorizontal); slider.setComponentID(MODELD_VOICE[i].id);
            slider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 60, 19);
            slider.setColour(juce::Slider::textBoxTextColourId, juce::Colour(0xfff2f2f2)); addChildComponent(slider);
            modeld_voice_attachments_[i] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(owner.parameters(), MODELD_VOICE[i].id, slider);
            compact_slider_text(slider);
        }
        constexpr const char* fx_headers[] = {"ON", "MIX", "P1", "P2", "P3"};
        for (int fx = 0; fx < 5; ++fx) for (int field = 0; field < 5; ++field) {
            auto& label = fx_parameter_labels_[size_t(fx)][size_t(field)];
            label.setText(juce::String(DX_FX_LABELS[fx][field]).toUpperCase(), juce::dontSendNotification);
            label.setComponentID("fx.label." + juce::String(fx) + "." + juce::String(field));
            label.setFont(juce::Font(juce::FontOptions(9.0f)));
            label.setColour(juce::Label::textColourId, juce::Colour(0xff8e8ea2));
            addChildComponent(label);
        }
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
            juno_fx_headers_[fx].setText(fx_headers[fx], juce::dontSendNotification);
            juno_fx_headers_[fx].setColour(juce::Label::textColourId, juce::Colour(0xff9fe1cb));
            addChildComponent(juno_fx_headers_[fx]);
            for (int field = 0; field < 5; ++field) {
                auto& slider = juno_fx_sliders_[fx][field];
                slider.setComponentID("shared.fx." + juce::String(fx) + "." + juce::String(field));
                slider.setSliderStyle(juce::Slider::LinearHorizontal);
                slider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 40, 18);
                slider.setColour(juce::Slider::textBoxTextColourId, juce::Colour(0xfff2f2f2));
                slider.setTooltip(DX_FX_LABELS[fx][field]);
                addChildComponent(slider);
                juno_fx_attachments_[fx][field] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(owner.parameters(), juno_fx_id(fx, field), slider);
                compact_slider_text(slider);
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
                slider.setComponentID("dx.groove." + juce::String(group) + "." + juce::String(field));
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
                slider.setComponentID("shared.groove." + juce::String(group) + "." + juce::String(field));
                slider.setSliderStyle(juce::Slider::LinearHorizontal);
                slider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 40, 18);
                slider.setColour(juce::Slider::textBoxTextColourId, juce::Colour(0xfff2f2f2));
                if (group == 0 && field == 6) slider.setTooltip("SOURCE GFP: 0 AUTO, 1 MIDI, 2 INT. El plugin usa el reloj del DAW.");
                addChildComponent(slider);
                juno_groove_attachments_[group][field] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(owner.parameters(), juno_groove_id(group, field), slider);
                compact_slider_text(slider);
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
        status_.setColour(juce::Label::textColourId, juce::Colour(0xff9fe1cb));
        status_.setJustificationType(juce::Justification::centred);
        status_.setFont(juce::Font(juce::FontOptions(12.0f)));
        update_status();
        addAndMakeVisible(status_);
        startTimerHz(5);
        setResizable(true, true);
        setResizeLimits(900, 570, 2400, 1520);
        getConstrainer()->setFixedAspectRatio(1200.0 / 760.0);
        setSize(1200, 760);
    }

    ~BrainEditor() override { owner_.stop_preset_preview(); keyboard_state_.removeListener(this); owner_.clear_keyboard_notes(); setLookAndFeel(nullptr); }

    void paint(juce::Graphics& g) override {
        g.fillAll(juce::Colour(0xff0a0a0a));
        const float scale = juce::jmin(float(getWidth()) / 720.0f, float(getHeight()) / (keyboard_open_ ? 576.0f : 456.0f));
        const auto accent = juce::Colour(hybrid_toggle_.getToggleState() ? 0xff2fa8c8 : 0xff1d9e75);
        g.setColour(accent); g.fillRect(0, 0, getWidth(), juce::jmax(2, int(scale)));
        g.setColour(juce::Colour(0xff242424)); g.drawHorizontalLine(int(40.0f * scale), 0.0f, float(getWidth()));
        g.setColour(juce::Colour(0xfff2f2f2));
        g.setFont(juce::Font(juce::FontOptions(10.0f * scale, juce::Font::bold)));
        g.drawText("GROOVEFORGE  /  BRAIN", int(16 * scale), int(8 * scale), int(220 * scale), int(22 * scale), juce::Justification::left);
        g.setColour(accent);
        g.drawText(hybrid_toggle_.getToggleState() ? "HYBRID" : "SYNTH", int(245 * scale), int(8 * scale), int(100 * scale), int(22 * scale), juce::Justification::left);
        g.addTransform(juce::AffineTransform::scale(scale).translated(0.0f, 80.0f * scale));
        g.setColour(juce::Colour(0xff141414));
        g.fillRoundedRectangle(18.0f, 74.0f, 684.0f, 250.0f, 6.0f);
        if (hybrid_toggle_.getToggleState() && hybrid_page_index_ == 0) {
            const auto focus = owner_.parameters().getRawParameterValue("hybrid.focus")->load() >= 0.5f ? 1 : 0;
            for (int side = 0; side < 2; ++side) {
                const float x = side == 0 ? 30.0f : 505.0f;
                g.setColour(juce::Colour(0xff101a1c)); g.fillRect(x, 112.0f, 185.0f, 165.0f);
                g.setColour(juce::Colour(side == focus ? 0xff7ad4ea : 0xff242424));
                g.drawRect(x, 112.0f, 185.0f, 165.0f, side == focus ? 2.0f : 1.0f);
            }
            g.setColour(juce::Colour(0xff10191b)); g.fillRect(225.0f, 112.0f, 270.0f, 165.0f);
            g.setColour(juce::Colour(0xff0f4352)); g.drawRect(225.0f, 112.0f, 270.0f, 165.0f, 1.0f);
            g.setColour(juce::Colour(0xff7ad4ea)); g.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
            g.drawText("MORPH", 240, 120, 240, 20, juce::Justification::centred);
        }
        if (hybrid_toggle_.getToggleState() && hybrid_page_index_ == 1) {
            const int focus = owner_.parameters().getRawParameterValue("hybrid.focus")->load() >= 0.5f ? 1 : 0;
            constexpr const char* names[] = {"MODEL D", "JUNO", "PROPHET", "OB-6", "DX7", "ARP 2600"};
            for (int side = 0; side < 2; ++side) {
                const float x = side == 0 ? 20.0f : 365.0f;
                g.setColour(juce::Colour(0xff101a1c)); g.fillRect(x, 102.0f, 335.0f, 176.0f);
                g.setColour(juce::Colour(side == focus ? 0xff7ad4ea : 0xff242424));
                g.drawRect(x, 102.0f, 335.0f, 176.0f, side == focus ? 2.0f : 1.0f);
                const auto engine = owner_.hybrid_engine_id(unsigned(side));
                const auto title = juce::String(side == 0 ? "A  ·  " : "B  ·  ") +
                    (engine < 6 ? names[engine] : "?") + (side == focus ? "  ·  FOCUS" : "");
                g.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
                g.drawText(title, int(x) + 10, 106, 300, 20, juce::Justification::left);
            }
        }
        if (fx_parameter_labels_[0][0].isVisible()) {
            for (int row = 0; row < 5; ++row) {
                const float y = 81.0f + float(row) * 39.0f;
                g.setColour(juce::Colour(row % 2 == 0 ? 0xff171717 : 0xff121212));
                g.fillRect(22.0f, y, 676.0f, 37.0f);
                g.setColour(juce::Colour(0xff242424)); g.drawRect(22.0f, y, 676.0f, 37.0f, 1.0f);
            }
        }
        const bool show_dx_groove = dx_groove_group_labels_[0].isVisible();
        if (show_dx_groove || juno_groove_group_labels_[0].isVisible()) {
            for (int group = 0; group < 3; ++group) {
                const float y = 92.0f + float(group) * 68.0f;
                const auto& enabled = show_dx_groove ? dx_groove_sliders_[size_t(group)][0] : juno_groove_sliders_[size_t(group)][0];
                g.setColour(juce::Colour(0xff141c1b)); g.fillRect(20.0f, y, 680.0f, 58.0f);
                g.setColour(juce::Colour(0xff242424)); g.drawRect(20.0f, y, 680.0f, 58.0f, 1.0f);
                g.setColour(enabled.getValue() >= 0.5 ? accent : juce::Colour(0xff8e8ea2));
                g.setFont(juce::Font(juce::FontOptions(9.0f, juce::Font::bold)));
                g.drawText(enabled.getValue() >= 0.5 ? "ON" : "OFF", 30, int(y) + 33, 50, 16, juce::Justification::left);
            }
        }
        g.setColour(juce::Colour(0xff5dcaa5));
        g.setFont(juce::Font(juce::FontOptions(14.0f, juce::Font::bold)));
        g.drawText("GROOVEFORGE  /  BRAIN", 28, 15, 350, 24, juce::Justification::left);
        g.setColour(juce::Colour(0xfff2f2f2));
        g.setFont(juce::Font(juce::FontOptions(22.0f, juce::Font::bold)));
        g.drawText(hybrid_toggle_.getToggleState() ? "HYBRID" : arp2600_toggle_.getToggleState() ? "ARP 2600" : modeld_toggle_.getToggleState() ? "MODEL D" : ob6_toggle_.getToggleState() ? "OB-6" : juno_toggle_.getToggleState() ? "JUNO" : dx7_toggle_.getToggleState() ? "DX7" : "PROPHET", 28, 40, 155, 28, juce::Justification::left);
        g.setColour(juce::Colour(0xff9fe1cb));
        g.setFont(juce::Font(juce::FontOptions(12.0f)));
        if (!hybrid_toggle_.getToggleState() && !arp2600_toggle_.getToggleState() && !dx7_toggle_.getToggleState() && !juno_toggle_.getToggleState() && !ob6_toggle_.getToggleState() && !modeld_toggle_.getToggleState()) g.drawText("EXPERIMENTAL  ·  5 VOCES", 400, 42, 290, 24, juce::Justification::right);
    }

    void resized() override {
        output_meter_.setBounds(685, -73, 25, 28);
        keyboard_toggle_.setBounds(605, -73, 70, 26);
        preset_browser_.setBounds(385, -73, 145, 26);
        browse_button_.setBounds(535, -73, 65, 26);
        browser_panel_.setBounds(20, 74, 680, 250);
        keyboard_.setBounds(30, 370, 660, 75);
        for (size_t i = 0; i < page_tabs_.size(); ++i)
            page_tabs_[i].setBounds(10 + int(i) * 64, -34, 62, 24);
        const int width = (720 - 60) / 4;
        for (size_t i = 0; i < knobs_.size(); ++i) {
            const int x = 30 + int(i) * width;
            labels_[i].setBounds(x, 102, width, 24);
            knobs_[i].setBounds(x + 8, 130, width - 16, 155);
        }
        import_.setBounds(720 - 145, 12, 115, 25);
        sync_.setBounds(720 - 260, 12, 105, 25);
        dx7_toggle_.setBounds(720 - 380, 12, 115, 25);
        juno_toggle_.setBounds(720 - 510, 12, 125, 25);
        hybrid_toggle_.setBounds(490, 295, 180, 25);
        hybrid_save_.setBounds(300, 295, 160, 25);
        hybrid_factory_.setBounds(30, 77, 190, 23);
        hybrid_half_a_.setBounds(43, 182, 158, 23); hybrid_half_b_.setBounds(518, 182, 158, 23);
        hybrid_focus_buttons_[0].setBounds(43, 125, 158, 35); hybrid_focus_buttons_[1].setBounds(518, 125, 158, 35);
        hybrid_page_.setBounds(720 - 155, 42, 120, 25);
        hybrid_focus_combo_.setBounds(290, 77, 140, 23);
        hybrid_osc_stop_.setBounds(290, 77, 140, 23);
        for (size_t i = 0; i < hybrid_osc_sliders_.size(); ++i) {
            const int x = i < 4 ? 25 : 370, y = 105 + int(i % 4) * 44;
            hybrid_osc_labels_[i].setBounds(x, y, 110, 24); hybrid_osc_sliders_[i].setBounds(x + 110, y, 190, 24);
        }
        hybrid_a_amp_envelope_.setBounds(30, 92, 305, 70); hybrid_a_filter_envelope_.setBounds(370, 92, 305, 70);
        hybrid_b_amp_envelope_.setBounds(30, 92, 305, 70); hybrid_b_filter_envelope_.setBounds(370, 92, 305, 70);
        for (int side = 0; side < 2; ++side) for (size_t i = 0; i < HYBRID_VOICE.size(); ++i) {
            const int x = 20 + int(i / 7) * 230, y = 166 + int(i % 7) * 21;
            const bool lfo_page = hybrid_page_index_ == 9 + side && i >= 10 && i <= 15;
            const int lx = i < 13 ? 30 : 370, ly = 181 + ((int(i) - 10) % 3) * 35;
            hybrid_voice_labels_[side][i].setBounds(lfo_page ? lx : x, lfo_page ? ly : y, lfo_page ? 125 : 100, 20);
            hybrid_voice_sliders_[side][i].setBounds(lfo_page ? lx + 125 : x + 100, lfo_page ? ly : y, lfo_page ? 175 : 125, 20);
        }
        for (size_t i = 0; i < hybrid_macro_sliders_.size(); ++i) {
            const int x = i < 4 ? 25 : 370, y = 105 + int(i % 4) * 44;
            hybrid_macro_labels_[i].setBounds(x, y, 110, 24); hybrid_macro_sliders_[i].setBounds(x + 110, y, 190, 24);
        }
        for (size_t i = 0; i < hybrid_half_sliders_.size(); ++i) {
            const int x = i < 4 ? 30 : 375, y = 130 + int(i % 4) * 35;
            hybrid_half_labels_[i].setBounds(x, y, 110, 24); hybrid_half_sliders_[i].setBounds(x + 110, y, 190, 24);
        }
        hybrid_labels_[0].setBounds(240, 140, 70, 22); hybrid_sliders_[0].setBounds(300, 140, 180, 22);
        hybrid_labels_[1].setBounds(240, 185, 95, 22); hybrid_sliders_[1].setBounds(330, 185, 150, 22);
        hybrid_labels_[2].setBounds(240, 225, 95, 22); hybrid_sliders_[2].setBounds(330, 225, 150, 22);
        hybrid_labels_[3].setBounds(43, 222, 70, 22); hybrid_sliders_[3].setBounds(105, 222, 96, 22);
        hybrid_labels_[4].setBounds(518, 222, 70, 22); hybrid_sliders_[4].setBounds(580, 222, 96, 22);
        arp2600_toggle_.setBounds(30, 295, 160, 25);
        arp2600_ring_label_.setBounds(205, 295, 45, 25);
        arp2600_ring_slider_.setBounds(250, 295, 220, 25);
        modeld_toggle_.setBounds(195, 42, 100, 25);
        ob6_toggle_.setBounds(300, 42, 125, 25);
        dx_page_.setBounds(720 - 155, 42, 120, 25);
        juno_page_.setBounds(720 - 155, 42, 120, 25);
        for (size_t i = 0; i < JUNO_OSC.size(); ++i) {
            const int x = i < 5 ? 30 : 370, y = 100 + int(i % 5) * 42;
            juno_labels_[i].setBounds(x, y, 120, 24);
            juno_sliders_[i].setBounds(x + 120, y, 180, 24);
        }
        juno_amp_envelope_.setBounds(30, 96, 305, 86);
        juno_filter_envelope_.setBounds(370, 96, 305, 86);
        for (size_t i = 0; i < JUNO_ENV.size(); ++i) {
            const int x = i < 5 ? 30 : 370, y = 191 + int(i % 5) * 23;
            juno_env_labels_[i].setBounds(x, y, 125, 22);
            juno_env_sliders_[i].setBounds(x + 125, y, 175, 22);
        }
        dx_lfo_display_.setBounds(30, 92, 645, 80);
        hybrid_a_lfo_display_.setBounds(30, 92, 645, 80); hybrid_b_lfo_display_.setBounds(30, 92, 645, 80);
        juno_lfo_display_.setBounds(30, 92, 645, 80);
        ob6_lfo_display_.setBounds(30, 92, 645, 80); modeld_lfo_display_.setBounds(30, 92, 645, 80); arp_lfo_display_.setBounds(30, 92, 645, 80);
        for (size_t i = 0; i < JUNO_LFO.size(); ++i) {
            const int x = i < 4 ? 30 : 370, y = 181 + int(i % 4) * 30;
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
        export_.setBounds(720 - 285, 42, 120, 25);
        for (size_t i = 0; i < DX_MACRO_IDS.size(); ++i) {
            const int x = i < 3 ? 30 : 370, y = 125 + int(i % 3) * 50;
            dx_macro_labels_[i].setBounds(x, y, 100, 24);
            dx_macro_sliders_[i].setBounds(x + 100, y, 200, 24);
        }
        dx_amp_envelope_.setBounds(30, 92, 645, 70);
        for (size_t i = 0; i < DX_GENERAL.size(); ++i) {
            const int x = 20 + int(i / 6) * 230, y = 171 + int(i % 6) * 22;
            const bool lfo_page = dx_page_index_ == 5 && i >= 9 && i <= 13;
            const int lx = i < 12 ? 30 : 370, ly = 181 + ((int(i) - 9) % 3) * 35;
            dx_voice_labels_[i].setBounds(lfo_page ? lx : x, lfo_page ? ly : y, lfo_page ? 125 : 100, 20);
            dx_voice_sliders_[i].setBounds(lfo_page ? lx + 125 : x + 100, lfo_page ? ly : y, lfo_page ? 175 : 125, 20);
        }
        for (size_t i = 0; i < OB6_OSC.size(); ++i) {
            const int x = i < 9 ? 30 : 370, y = 100 + int(i % 9) * 23;
            ob6_osc_labels_[i].setBounds(x, y, 100, 24);
            ob6_osc_sliders_[i].setBounds(x + 100, y, 200, 24);
        }
        ob6_amp_envelope_.setBounds(30, 92, 305, 70); ob6_filter_envelope_.setBounds(370, 92, 305, 70);
        for (size_t i = 0; i < OB6_VOICE.size(); ++i) {
            const int x = 20 + int(i / 7) * 230, y = 166 + int(i % 7) * 21;
            const bool lfo_page = ob6_page_index_ == 6 && i >= 10 && i <= 15;
            const int lx = i < 13 ? 30 : 370, ly = 181 + ((int(i) - 10) % 3) * 35;
            ob6_voice_labels_[i].setBounds(lfo_page ? lx : x, lfo_page ? ly : y, lfo_page ? 125 : 100, 20);
            ob6_voice_sliders_[i].setBounds(lfo_page ? lx + 125 : x + 100, lfo_page ? ly : y, lfo_page ? 175 : 125, 20);
        }
        arp_amp_envelope_.setBounds(30, 92, 305, 70); arp_filter_envelope_.setBounds(370, 92, 305, 70);
        for (size_t i = 0; i < ARP2600_VOICE.size(); ++i) {
            const int x = 20 + int(i / 7) * 230, y = 166 + int(i % 7) * 21;
            const bool lfo_page = arp2600_page_index_ == 6 && i >= 10 && i <= 15;
            const int lx = i < 13 ? 30 : 370, ly = 181 + ((int(i) - 10) % 3) * 35;
            arp2600_voice_labels_[i].setBounds(lfo_page ? lx : x, lfo_page ? ly : y, lfo_page ? 125 : 100, 20);
            arp2600_voice_sliders_[i].setBounds(lfo_page ? lx + 125 : x + 100, lfo_page ? ly : y, lfo_page ? 175 : 125, 20);
        }
        for (size_t i = 0; i < ARP2600_OSC.size(); ++i) {
            const int x = i < 7 ? 30 : 370, y = 95 + int(i % 7) * 29;
            arp2600_osc_labels_[i].setBounds(x, y, 100, 20);
            arp2600_osc_sliders_[i].setBounds(x + 100, y, 200, 20);
        }
        for (size_t i = 0; i < MODELD_OSC.size(); ++i) {
            const int x = i < 10 ? 30 : 370, y = 100 + int(i % 10) * 20;
            modeld_osc_labels_[i].setBounds(x, y, 100, 20);
            modeld_osc_sliders_[i].setBounds(x + 100, y, 200, 20);
        }
        modeld_amp_envelope_.setBounds(30, 92, 305, 70); modeld_filter_envelope_.setBounds(370, 92, 305, 70);
        for (size_t i = 0; i < MODELD_VOICE.size(); ++i) {
            const int x = 20 + int(i / 6) * 230, y = 171 + int(i % 6) * 22;
            const bool lfo_page = modeld_page_index_ == 6 && i >= 10 && i <= 15;
            const int lx = i < 13 ? 30 : 370, ly = 181 + ((int(i) - 10) % 3) * 35;
            modeld_voice_labels_[i].setBounds(lfo_page ? lx : x, lfo_page ? ly : y, lfo_page ? 125 : 100, 20);
            modeld_voice_sliders_[i].setBounds(lfo_page ? lx + 125 : x + 100, lfo_page ? ly : y, lfo_page ? 175 : 125, 20);
        }
        const int fx_width = (720 - 120) / 5;
        for (int i = 0; i < 5; ++i) {
            dx_fx_headers_[i].setBounds(90 + i * fx_width, 80, fx_width, 22);
            dx_fx_labels_[i].setBounds(25, 87 + i * 39, 65, 24);
            for (int field = 0; field < 5; ++field) {
                fx_parameter_labels_[size_t(i)][size_t(field)].setBounds(90 + field * fx_width, 82 + i * 39, fx_width - 4, 15);
                dx_fx_sliders_[i][field].setBounds(90 + field * fx_width, 96 + i * 39, fx_width - 4, 20);
            }
        }
        const int dx_width = (720 - 120) / 4;
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
        const int juno_fx_width = (720 - 120) / 5;
        for (int fx = 0; fx < 5; ++fx) {
            juno_fx_headers_[fx].setBounds(90 + fx * juno_fx_width, 80, juno_fx_width, 22);
            juno_fx_labels_[fx].setBounds(25, 87 + fx * 39, 65, 24);
            for (int field = 0; field < 5; ++field)
                juno_fx_sliders_[fx][field].setBounds(90 + field * juno_fx_width, 96 + fx * 39, juno_fx_width - 4, 20);
        }
        const int juno_groove_width = (720 - 120) / 7;
        for (int group = 0; group < 3; ++group) {
            const int y = 100 + group * 68;
            juno_groove_group_labels_[group].setBounds(25, y, 65, 22);
            dx_groove_group_labels_[group].setBounds(25, y, 65, 22);
            for (int field = 0; field < DX_GROOVE_FIELDS[size_t(group)]; ++field) {
                dx_groove_labels_[group][field].setBounds(90 + field * juno_groove_width, y, juno_groove_width, 18);
                dx_groove_sliders_[group][field].setBounds(90 + field * juno_groove_width, y + 19, juno_groove_width - 4, 22);
            }
            for (int field = 0; field < JUNO_GROOVE_FIELDS[size_t(group)]; ++field) {
                juno_groove_labels_[group][field].setBounds(90 + field * juno_groove_width, y, juno_groove_width, 18);
                juno_groove_sliders_[group][field].setBounds(90 + field * juno_groove_width, y + 19, juno_groove_width - 4, 22);
            }
        }
        status_.setBounds(28, 328, 720 - 56, 24);
        const float scale = juce::jmin(float(getWidth()) / 720.0f, float(getHeight()) / (keyboard_open_ ? 576.0f : 456.0f));
        const auto transform = juce::AffineTransform::scale(scale).translated(0.0f, 80.0f * scale);
        for (int i = 0; i < getNumChildComponents(); ++i)
            getChildComponent(i)->setTransform(transform);
    }

private:
    void handleNoteOn(juce::MidiKeyboardState*, int, int note, float) override { owner_.set_keyboard_note(note, true); }
    void handleNoteOff(juce::MidiKeyboardState*, int, int note, float) override { owner_.set_keyboard_note(note, false); }
    void update_status() {
        const auto name = owner_.preset_name();
        displayed_preset_ = name;
        const bool hybrid = hybrid_toggle_.getToggleState();
        studio_look_.set_hybrid(hybrid);
        const bool arp2600 = !hybrid && arp2600_toggle_.getToggleState();
        const bool modeld = !hybrid && !arp2600 && modeld_toggle_.getToggleState();
        const bool ob6 = !hybrid && !arp2600 && !modeld && ob6_toggle_.getToggleState();
        const bool juno = !hybrid && !arp2600 && !modeld && !ob6 && juno_toggle_.getToggleState();
        const bool dx7 = !hybrid && !arp2600 && !modeld && !ob6 && !juno && dx7_toggle_.getToggleState();
        const int browser_engine = hybrid ? 6 : arp2600 ? 5 : modeld ? 0 : ob6 ? 3 : juno ? 1 : dx7 ? 4 : 2;
        const auto revision = owner_.hybrid_bank_revision();
        if (browser_engine_ != browser_engine || (browser_engine == 6 && browser_bank_revision_ != revision)) {
            browser_engine_ = browser_engine; browser_bank_revision_ = revision;
            preset_browser_.clear(juce::dontSendNotification);
            if (hybrid) {
                const auto names = owner_.hybrid_bank_names();
                for (int i = 0; i < names.size(); ++i) preset_browser_.addItem(names[i], i + 1);
                preset_browser_.setTextWhenNothingSelected("LAYERS · PROJECT");
            } else {
                studio::Preset preset;
                for (int i = 0; i < 256 && studio::factory_preset(uint8_t(browser_engine), uint8_t(i), &preset); ++i)
                    preset_browser_.addItem(juce::String::fromUTF8(preset.name), i + 1);
                preset_browser_.setTextWhenNothingSelected("PRESETS · FACTORY");
            }
        }
        constexpr const char* hybrid_tabs[] = {"HOME", "FILTER", "MACROS", "GROOVE", "FX", "A VOICE", "B VOICE", "A OSC", "B OSC", "A LFO", "B LFO"};
        constexpr const char* analog_tabs[] = {"VOICE", "OSC", "ENV / LFO", "GROOVE", "FX", "MACROS", "LFO"};
        constexpr const char* juno_tabs[] = {"VOICE", "OSC", "ENV", "LFO", "UNISON", "GROOVE", "FX", "MACROS"};
        constexpr const char* dx_tabs[] = {"OPERATORS", "VOICE", "FX", "GROOVE", "MACROS", "LFO"};
        const int count = hybrid ? 11 : (arp2600 || modeld || ob6) ? 7 : juno ? 8 : dx7 ? 6 : 0;
        const int selected = hybrid ? hybrid_page_index_ : arp2600 ? arp2600_page_index_ : modeld ? modeld_page_index_ : ob6 ? ob6_page_index_ : juno ? juno_page_index_ : dx_page_index_;
        const auto accent = juce::Colour(hybrid ? 0xff0f4352 : 0xff0e5040);
        for (int i = 0; i < int(page_tabs_.size()); ++i) {
            auto& tab = page_tabs_[size_t(i)];
            tab.setVisible(i < count);
            if (i < count) tab.setButtonText(hybrid ? hybrid_tabs[i] : (arp2600 || modeld || ob6) ? analog_tabs[i] : juno ? juno_tabs[i] : dx_tabs[i]);
            tab.setColour(juce::TextButton::buttonColourId, i == selected ? accent : juce::Colour(0xff141414));
            tab.setColour(juce::TextButton::textColourOffId, i == selected ? juce::Colours::white : juce::Colour(0xff9fe1cb));
        }
        const bool dx_ops = dx7 && dx_page_index_ == 0, dx_voice = dx7 && dx_page_index_ == 1;
        const bool dx_fx = dx7 && dx_page_index_ == 2, dx_groove = dx7 && dx_page_index_ == 3, dx_macro = dx7 && dx_page_index_ == 4;
        dx_page_.setVisible(dx7);
        const int shared_engine = hybrid ? 4 : arp2600 ? 3 : modeld ? 2 : ob6 ? 1 : 0;
        if (shared_engine != shared_editor_engine_) {
            for (int group = 0; group < 3; ++group) for (int field = 0; field < JUNO_GROOVE_FIELDS[size_t(group)]; ++field) {
                juno_groove_attachments_[group][field].reset();
                const auto id = hybrid ? hybrid_groove_id(group, field) : arp2600 ? arp2600_groove_id(group, field) : modeld ? modeld_groove_id(group, field) : ob6 ? ob6_groove_id(group, field) : juno_groove_id(group, field);
                juno_groove_attachments_[group][field] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(owner_.parameters(), id, juno_groove_sliders_[group][field]);
                compact_slider_text(juno_groove_sliders_[group][field]);
            }
            for (int fx = 0; fx < 5; ++fx) for (int field = 0; field < 5; ++field) {
                juno_fx_attachments_[fx][field].reset();
                const auto id = hybrid ? hybrid_fx_id(fx, field) : arp2600 ? arp2600_fx_id(fx, field) : modeld ? modeld_fx_id(fx, field) : ob6 ? ob6_fx_id(fx, field) : juno_fx_id(fx, field);
                juno_fx_attachments_[fx][field] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(owner_.parameters(), id, juno_fx_sliders_[fx][field]);
                compact_slider_text(juno_fx_sliders_[fx][field]);
            }
            for (size_t i = 0; i < OB6_MACRO_IDS.size(); ++i) {
                juno_macro_attachments_[i].reset();
                const auto* id = arp2600 ? ARP2600_MACRO_IDS[i] : modeld ? MODELD_MACRO_IDS[i] : ob6 ? OB6_MACRO_IDS[i] : JUNO_MACRO_IDS[i];
                juno_macro_attachments_[i] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(owner_.parameters(), id, juno_macro_sliders_[i]);
                compact_slider_text(juno_macro_sliders_[i]);
            }
            shared_editor_engine_ = shared_engine;
        }
        const bool show_groove = (hybrid && hybrid_page_index_ == 3) || (arp2600 && arp2600_page_index_ == 3) || (juno && juno_page_index_ == 5) || (ob6 && ob6_page_index_ == 3) || (modeld && modeld_page_index_ == 3);
        juno_page_.setVisible(juno || ob6 || modeld || arp2600);
        juno_page_.setButtonText(juno_page_index_ == 0 ? "OSC" : juno_page_index_ == 1 ? "ENV" : juno_page_index_ == 2 ? "LFO" : juno_page_index_ == 3 ? "UNISON" : juno_page_index_ == 4 ? "GROOVE" : juno_page_index_ == 5 ? "FX" : juno_page_index_ == 6 ? "MACROS" : "VOICE");
        if (arp2600) juno_page_.setButtonText(arp2600_page_index_ == 0 ? "VOICE  >" : arp2600_page_index_ == 1 ? "OSC / PATCH  >" : arp2600_page_index_ == 2 ? "ENV / LFO  >" : arp2600_page_index_ == 3 ? "GROOVE  >" : arp2600_page_index_ == 4 ? "FX  >" : arp2600_page_index_ == 5 ? "MACROS  >" : "LFO  >");
        if (modeld) juno_page_.setButtonText(modeld_page_index_ == 0 ? "VOICE  >" : modeld_page_index_ == 1 ? "OSC / MOD  >" : modeld_page_index_ == 2 ? "ENV / LFO  >" : modeld_page_index_ == 3 ? "GROOVE  >" : modeld_page_index_ == 4 ? "FX  >" : modeld_page_index_ == 5 ? "MACROS  >" : "LFO  >");
        if (ob6) {
            constexpr const char* names[] = {"VOICE", "OSC", "ENV/LFO", "GROOVE", "FX", "MACROS", "LFO"};
            juno_page_.setButtonText(juce::String(names[ob6_page_index_]) + "  >");
        }
        for (auto& label : juno_labels_) label.setVisible(juno && juno_page_index_ == 1);
        for (auto& slider : juno_sliders_) slider.setVisible(juno && juno_page_index_ == 1);
        juno_amp_envelope_.setVisible(juno && juno_page_index_ == 2);
        juno_filter_envelope_.setVisible(juno && juno_page_index_ == 2);
        for (auto& label : juno_env_labels_) label.setVisible(juno && juno_page_index_ == 2);
        for (auto& slider : juno_env_sliders_) slider.setVisible(juno && juno_page_index_ == 2);
        juno_lfo_display_.setVisible(juno && juno_page_index_ == 3);
        for (auto& label : juno_lfo_labels_) label.setVisible(juno && juno_page_index_ == 3);
        for (auto& slider : juno_lfo_sliders_) slider.setVisible(juno && juno_page_index_ == 3);
        for (auto& label : juno_voice_groove_labels_) label.setVisible(juno && juno_page_index_ == 4);
        for (auto& slider : juno_voice_groove_sliders_) slider.setVisible(juno && juno_page_index_ == 4);
        for (auto& label : juno_macro_labels_) label.setVisible((juno && juno_page_index_ == 7) || (ob6 && ob6_page_index_ == 5) || (modeld && modeld_page_index_ == 5) || (arp2600 && arp2600_page_index_ == 5));
        for (auto& slider : juno_macro_sliders_) slider.setVisible((juno && juno_page_index_ == 7) || (ob6 && ob6_page_index_ == 5) || (modeld && modeld_page_index_ == 5) || (arp2600 && arp2600_page_index_ == 5));
        for (auto& label : juno_fx_labels_) label.setVisible((juno && juno_page_index_ == 6) || (ob6 && ob6_page_index_ == 4) || (modeld && modeld_page_index_ == 4) || (arp2600 && arp2600_page_index_ == 4) || (hybrid && hybrid_page_index_ == 4));
        for (auto& label : juno_fx_headers_) label.setVisible(false);
        for (auto& row : juno_fx_sliders_) for (auto& slider : row) slider.setVisible((juno && juno_page_index_ == 6) || (ob6 && ob6_page_index_ == 4) || (modeld && modeld_page_index_ == 4) || (arp2600 && arp2600_page_index_ == 4) || (hybrid && hybrid_page_index_ == 4));
        for (auto& label : juno_groove_group_labels_) label.setVisible(show_groove);
        for (int group = 0; group < 3; ++group) for (int field = 0; field < JUNO_GROOVE_FIELDS[size_t(group)]; ++field) {
            juno_groove_labels_[group][field].setVisible(show_groove);
            juno_groove_sliders_[group][field].setVisible(show_groove);
        }
        for (auto& label : ob6_osc_labels_) label.setVisible(ob6 && ob6_page_index_ == 1);
        for (auto& slider : ob6_osc_sliders_) slider.setVisible(ob6 && ob6_page_index_ == 1);
        ob6_lfo_display_.setVisible(ob6 && ob6_page_index_ == 6);
        modeld_lfo_display_.setVisible(modeld && modeld_page_index_ == 6);
        arp_lfo_display_.setVisible(arp2600 && arp2600_page_index_ == 6);
        ob6_amp_envelope_.setVisible(ob6 && ob6_page_index_ == 2); ob6_filter_envelope_.setVisible(ob6 && ob6_page_index_ == 2);
        modeld_amp_envelope_.setVisible(modeld && modeld_page_index_ == 2); modeld_filter_envelope_.setVisible(modeld && modeld_page_index_ == 2);
        for (size_t i = 0; i < ob6_voice_labels_.size(); ++i) {
            const bool visible = ob6 && (ob6_page_index_ == 2 || (ob6_page_index_ == 6 && i >= 10 && i <= 15));
            ob6_voice_labels_[i].setVisible(visible); ob6_voice_sliders_[i].setVisible(visible);
        }
        arp_amp_envelope_.setVisible(arp2600 && arp2600_page_index_ == 2); arp_filter_envelope_.setVisible(arp2600 && arp2600_page_index_ == 2);
        dx_amp_envelope_.setVisible(dx_voice);
        for (size_t i = 0; i < arp2600_voice_labels_.size(); ++i) {
            const bool visible = arp2600 && (arp2600_page_index_ == 2 || (arp2600_page_index_ == 6 && i >= 10 && i <= 15));
            arp2600_voice_labels_[i].setVisible(visible); arp2600_voice_sliders_[i].setVisible(visible);
        }
        for (auto& label : arp2600_osc_labels_) label.setVisible(arp2600 && arp2600_page_index_ == 1);
        for (auto& slider : arp2600_osc_sliders_) slider.setVisible(arp2600 && arp2600_page_index_ == 1);
        for (auto& label : modeld_osc_labels_) label.setVisible(modeld && modeld_page_index_ == 1);
        for (auto& slider : modeld_osc_sliders_) slider.setVisible(modeld && modeld_page_index_ == 1);
        for (size_t i = 0; i < modeld_voice_labels_.size(); ++i) {
            const bool visible = modeld && (modeld_page_index_ == 2 || (modeld_page_index_ == 6 && i >= 10 && i <= 15));
            modeld_voice_labels_[i].setVisible(visible); modeld_voice_sliders_[i].setVisible(visible);
        }
        export_.setVisible(dx7 || juno || ob6 || modeld || arp2600 || hybrid);
        dx_page_.setButtonText(dx_ops ? "VOICE" : dx_voice ? "FX" : dx_fx ? "GROOVE" : dx_groove ? "MACROS" : "OPERATORS");
        for (auto& knob : knobs_) knob.setVisible(!hybrid && !dx7 && !(juno && juno_page_index_ != 0) && !(ob6 && ob6_page_index_ != 0) && !(modeld && modeld_page_index_ != 0) && !(arp2600 && arp2600_page_index_ != 0));
        for (auto& label : labels_) label.setVisible(!hybrid && !dx7 && !(juno && juno_page_index_ != 0) && !(ob6 && ob6_page_index_ != 0) && !(modeld && modeld_page_index_ != 0) && !(arp2600 && arp2600_page_index_ != 0));
        for (auto& label : dx_field_labels_) label.setVisible(dx_ops);
        for (auto& label : dx_op_labels_) label.setVisible(dx_ops);
        for (auto& row : dx_sliders_) for (auto& slider : row) slider.setVisible(dx_ops);
        for (auto& label : dx_global_labels_) label.setVisible(dx_ops);
        for (auto& slider : dx_globals_) slider.setVisible(dx_ops);
        dx_lfo_display_.setVisible(dx7 && dx_page_index_ == 5);
        for (size_t i = 0; i < dx_voice_labels_.size(); ++i) {
            const bool visible = dx_voice || (dx7 && dx_page_index_ == 5 && i >= 9 && i <= 13);
            dx_voice_labels_[i].setVisible(visible); dx_voice_sliders_[i].setVisible(visible);
        }
        for (auto& label : dx_fx_labels_) label.setVisible(dx_fx);
        for (auto& label : dx_fx_headers_) label.setVisible(false);
        const bool any_fx = dx_fx || (juno && juno_page_index_ == 6) || (ob6 && ob6_page_index_ == 4) || (modeld && modeld_page_index_ == 4) || (arp2600 && arp2600_page_index_ == 4) || (hybrid && hybrid_page_index_ == 4);
        for (auto& row : fx_parameter_labels_) for (auto& label : row) label.setVisible(any_fx);
        for (auto& row : dx_fx_sliders_) for (auto& slider : row) slider.setVisible(dx_fx);
        for (auto& label : dx_groove_group_labels_) label.setVisible(dx_groove);
        for (int group = 0; group < 3; ++group) for (int field = 0; field < DX_GROOVE_FIELDS[size_t(group)]; ++field) {
            dx_groove_labels_[group][field].setVisible(dx_groove);
            dx_groove_sliders_[group][field].setVisible(dx_groove);
        }
        for (auto& label : dx_macro_labels_) label.setVisible(dx_macro);
        for (auto& slider : dx_macro_sliders_) slider.setVisible(dx_macro);
        hybrid_focus_combo_.setVisible(hybrid && hybrid_page_index_ == 1);
        hybrid_page_.setVisible(hybrid); hybrid_page_.setButtonText(hybrid_page_index_ == 0 ? "CAPAS  >" : hybrid_page_index_ == 1 ? "FILTROS / ENV  >" : hybrid_page_index_ == 2 ? "MACROS  >" : hybrid_page_index_ == 3 ? "GROOVE  >" : hybrid_page_index_ == 4 ? "FX  >" : hybrid_page_index_ == 5 ? "A VOICE  >" : hybrid_page_index_ == 6 ? "B VOICE  >" : hybrid_page_index_ == 7 ? "A OSC / MOD  >" : hybrid_page_index_ == 8 ? "B OSC / MOD  >" : hybrid_page_index_ == 9 ? "A LFO  >" : "B LFO  >");
        if (displayed_hybrid_bank_revision_ != owner_.hybrid_bank_revision()) {
            const auto names = owner_.hybrid_bank_names();
            hybrid_factory_.clear(juce::dontSendNotification);
            for (int i = 0; i < names.size(); ++i) hybrid_factory_.addItem(names[i], i + 1);
            displayed_hybrid_bank_revision_ = owner_.hybrid_bank_revision();
        }
        hybrid_save_.setVisible(hybrid && hybrid_page_index_ == 0);
        hybrid_factory_.setVisible(hybrid && hybrid_page_index_ == 0);
        hybrid_half_a_.setVisible(hybrid && hybrid_page_index_ == 0); hybrid_half_b_.setVisible(hybrid && hybrid_page_index_ == 0);
        constexpr const char* engine_names[] = {"MODEL D", "JUNO", "PROPHET", "OB-6", "DX7", "ARP 2600"};
        const int focus = owner_.parameters().getRawParameterValue("hybrid.focus")->load() >= 0.5f ? 1 : 0;
        for (int side = 0; side < 2; ++side) {
            auto& button = hybrid_focus_buttons_[size_t(side)];
            button.setVisible(hybrid && hybrid_page_index_ == 0);
            const auto engine = owner_.hybrid_engine_id(unsigned(side));
            button.setButtonText(juce::String(side == 0 ? "A · " : "B · ") + (engine < 6 ? engine_names[engine] : "?") + (side == focus ? " · FOCUS" : ""));
            button.setColour(juce::TextButton::buttonColourId, juce::Colour(side == focus ? 0xff0f4352 : 0xff141414));
        }
        displayed_hybrid_focus_ = focus;
        for (auto& label : hybrid_labels_) label.setVisible(hybrid && hybrid_page_index_ == 0);
        for (auto& slider : hybrid_sliders_) slider.setVisible(hybrid && hybrid_page_index_ == 0);
        const bool show_hybrid_osc = hybrid && hybrid_page_index_ >= 7 && hybrid_page_index_ <= 8;
        hybrid_osc_stop_.setVisible(show_hybrid_osc);
        if (show_hybrid_osc) configure_hybrid_osc_page(unsigned(hybrid_page_index_ - 7));
        else for (size_t i = 0; i < hybrid_osc_sliders_.size(); ++i) { hybrid_osc_labels_[i].setVisible(false); hybrid_osc_sliders_[i].setVisible(false); }
        hybrid_a_amp_envelope_.setVisible(hybrid && hybrid_page_index_ == 5);
        hybrid_a_filter_envelope_.setVisible(hybrid && hybrid_page_index_ == 5 && owner_.hybrid_engine_id(0) != 4);
        hybrid_b_amp_envelope_.setVisible(hybrid && hybrid_page_index_ == 6);
        hybrid_b_filter_envelope_.setVisible(hybrid && hybrid_page_index_ == 6 && owner_.hybrid_engine_id(1) != 4);
        hybrid_a_lfo_display_.setVisible(hybrid && hybrid_page_index_ == 9);
        hybrid_b_lfo_display_.setVisible(hybrid && hybrid_page_index_ == 10);
        for (int side = 0; side < 2; ++side) for (size_t i = 0; i < HYBRID_VOICE.size(); ++i) {
            const bool visible = hybrid && (hybrid_page_index_ == 5 + side || (hybrid_page_index_ == 9 + side && i >= 10 && i <= 15));
            hybrid_voice_labels_[side][i].setVisible(visible); hybrid_voice_sliders_[side][i].setVisible(visible);
        }
        for (auto& label : hybrid_macro_labels_) label.setVisible(hybrid && hybrid_page_index_ == 2);
        for (auto& slider : hybrid_macro_sliders_) slider.setVisible(hybrid && hybrid_page_index_ == 2);
        for (auto& label : hybrid_half_labels_) label.setVisible(hybrid && hybrid_page_index_ == 1);
        for (auto& slider : hybrid_half_sliders_) slider.setVisible(hybrid && hybrid_page_index_ == 1);
        arp2600_ring_label_.setVisible(arp2600 && arp2600_page_index_ == 0);
        arp2600_ring_slider_.setVisible(arp2600 && arp2600_page_index_ == 0);
        sync_.setEnabled(true);
        status_.setText(hybrid ? owner_.hybrid_description() : arp2600 ? (name.isEmpty() ? juce::String("ARP 2600 INIT") : name) + " · ARP 2600 GFP PARCIAL" : modeld ? juce::String((name.isEmpty() ? juce::String("MODEL D INIT") : name) + " · MODEL D GFP PARCIAL") : ob6 ? juce::String((name.isEmpty() ? juce::String("OB-6 INIT") : name) + " · OB-6 GFP PARCIAL") : juno ? juce::String((name.isEmpty() ? juce::String("INIT") : name) + " · JUNO PREVIEW") : dx7 ? (name.isEmpty() ? juce::String("DX7 E.PIANO") : name) + " · DX7 PREVIEW" :
            (name.isEmpty() ? juce::String("INIT") : name) + "  ·  PREVIEW: CADENA FX INCOMPLETA", juce::dontSendNotification);
        displayed_dx7_ = dx7_toggle_.getToggleState();
        displayed_juno_ = juno_toggle_.getToggleState();
        displayed_ob6_ = ob6_toggle_.getToggleState();
        displayed_modeld_ = modeld_toggle_.getToggleState();
        displayed_arp2600_ = arp2600_toggle_.getToggleState();
        displayed_hybrid_ = hybrid; displayed_hybrid_revision_ = owner_.hybrid_revision();
        if (getWidth() > 0 && getHeight() > 0) resized();
        repaint();
    }
    void timerCallback() override {
        if (dx_groove_group_labels_[0].isVisible() || juno_groove_group_labels_[0].isVisible()) repaint();
        if (owner_.preset_name() != displayed_preset_ || displayed_dx7_ != dx7_toggle_.getToggleState() || displayed_juno_ != juno_toggle_.getToggleState() || displayed_ob6_ != ob6_toggle_.getToggleState() || displayed_modeld_ != modeld_toggle_.getToggleState() || displayed_arp2600_ != arp2600_toggle_.getToggleState() || displayed_hybrid_ != hybrid_toggle_.getToggleState() || displayed_hybrid_revision_ != owner_.hybrid_revision() || displayed_hybrid_bank_revision_ != owner_.hybrid_bank_revision() || (hybrid_toggle_.getToggleState() && displayed_hybrid_focus_ != (owner_.parameters().getRawParameterValue("hybrid.focus")->load() >= 0.5f ? 1 : 0))) update_status();
    }
    void configure_hybrid_osc_page(unsigned side) {
        const uint8_t engine = owner_.hybrid_engine_id(side);
        for (int page = 0; page < 6; ++page) {
            const bool valid = page < 4 ? page < GF_OSC_STOP_COUNT[engine] : page - 4 < GF_MOD_STOPS[engine];
            hybrid_osc_stop_.setItemEnabled(page + 1, valid);
        }
        int page = hybrid_osc_stop_.getSelectedId() - 1;
        if (page < 0 || (page < 4 ? page >= GF_OSC_STOP_COUNT[engine] : page - 4 >= GF_MOD_STOPS[engine])) {
            page = 0; hybrid_osc_stop_.setSelectedId(1, juce::dontSendNotification);
        }
        const int key = int(side) * 100 + int(engine) * 10 + page;
        for (size_t i = 0; i < hybrid_osc_sliders_.size(); ++i) {
            const auto desc = hybrid_osc_descriptor(engine, unsigned(page), unsigned(i));
            const bool used = gf_osc_used(&desc);
            hybrid_osc_labels_[i].setVisible(used); hybrid_osc_sliders_[i].setVisible(used);
            if (!used) continue;
            hybrid_osc_labels_[i].setText(hybrid_osc_label(engine, unsigned(page), unsigned(i)), juce::dontSendNotification);
            if (key == hybrid_osc_attached_key_) continue;
            hybrid_osc_attachments_[i].reset();
            const unsigned actual_page = desc.global ? (page < 4 ? 0u : 4u) : unsigned(page);
            hybrid_osc_attachments_[i] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(owner_.parameters(), hybrid_osc_id(side, actual_page, unsigned(i)), hybrid_osc_sliders_[i]);
            hybrid_osc_sliders_[i].textFromValueFunction = [desc](double n) {
                double value = desc.lo + n * (desc.hi - desc.lo); if (desc.count) value = std::round(value);
                return juce::String(value, desc.count || std::abs(value - std::round(value)) < 1e-5 ? 0 : 2);
            };
            hybrid_osc_sliders_[i].valueFromTextFunction = [desc](const juce::String& text) {
                return std::clamp((text.getDoubleValue() - desc.lo) / (desc.hi - desc.lo), 0.0, 1.0);
            };
            hybrid_osc_sliders_[i].updateText();
        }
        hybrid_osc_attached_key_ = key;
    }
    void choose_hybrid_half(unsigned side) {
        chooser_ = std::make_unique<juce::FileChooser>(side == 0 ? "Cargar GFP de la mitad A" : "Cargar GFP de la mitad B", juce::File(), "*.gfp");
        juce::Component::SafePointer<BrainEditor> safe(this);
        chooser_->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
            [safe, side](const juce::FileChooser& chooser) {
                if (!safe) return;
                const auto file = chooser.getResult(); if (file == juce::File()) return;
                juce::MemoryBlock bytes;
                if (file.getSize() <= 0 || file.getSize() > int64_t(studio::PRESET_BYTES) || !file.loadFileAsData(bytes) ||
                    !safe->owner_.import_hybrid_half(bytes.getData(), bytes.getSize(), side)) {
                    safe->status_.setText("GFP INVÁLIDO O ENGINE REPETIDO · SIN CAMBIOS", juce::dontSendNotification); return;
                }
                safe->status_.setText(side == 0 ? "HYBRID · MITAD A ACTUALIZADA" : "HYBRID · MITAD B ACTUALIZADA", juce::dontSendNotification);
            });
    }
    void choose_preset() {
        chooser_ = std::make_unique<juce::FileChooser>("Importar preset GFP o capa GFL", juce::File(), "*.gfp;*.gfl");
        juce::Component::SafePointer<BrainEditor> safe(this);
        chooser_->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
            [safe](const juce::FileChooser& chooser) {
                if (!safe) return;
                const auto file = chooser.getResult();
                if (file == juce::File()) return;
                juce::MemoryBlock data;
                if (file.getSize() <= 0 || file.getSize() > int64_t(studio::LAYER_BYTES) ||
                    !file.loadFileAsData(data) || !safe->owner_.import_preset(data.getData(), data.getSize())) {
                    safe->status_.setText("GFP INVÁLIDO O DE OTRO ENGINE · SIN CAMBIOS", juce::dontSendNotification);
                    return;
                }
                safe->update_status();
            });
    }
    void export_dx7() {
        std::vector<uint8_t> bytes;
        const bool hybrid = hybrid_toggle_.getToggleState();
        const bool arp2600 = arp2600_toggle_.getToggleState();
        const bool juno = juno_toggle_.getToggleState();
        const bool ob6 = ob6_toggle_.getToggleState();
        const bool modeld = modeld_toggle_.getToggleState();
        if (!(hybrid ? owner_.export_hybrid_preset(bytes) : arp2600 ? owner_.export_arp2600_preset(bytes) : modeld ? owner_.export_model_d_preset(bytes) : ob6 ? owner_.export_ob6_preset(bytes) : juno ? owner_.export_juno_preset(bytes) : owner_.export_dx7_preset(bytes))) {
            status_.setText("EXPORT: DESACTIVA LFO SYNC Y CARGA UN GFP DEL ENGINE", juce::dontSendNotification);
            return;
        }
        chooser_ = std::make_unique<juce::FileChooser>("Guardar preset GFP", juce::File(hybrid ? "Hybrid.gfl" : arp2600 ? "ARP2600.gfp" : modeld ? "ModelD.gfp" : ob6 ? "OB6.gfp" : juno ? "Juno.gfp" : "DX7.gfp"), hybrid ? "*.gfl" : "*.gfp");
        juce::Component::SafePointer<BrainEditor> safe(this);
        chooser_->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles |
                              juce::FileBrowserComponent::warnAboutOverwriting,
            [safe, hybrid, bytes = std::move(bytes)](const juce::FileChooser& chooser) {
                if (!safe) return;
                const auto file = chooser.getResult();
                if (file == juce::File()) return;
                const bool ok = file.replaceWithData(bytes.data(), bytes.size());
                safe->status_.setText(ok ? (hybrid ? "GFL GUARDADO · CAPAS Y CONTROLES" : "GFP GUARDADO · GRIT / POCKET / MOVE INCORPORADOS") : "NO SE PUDO GUARDAR EL PRESET",
                                      juce::dontSendNotification);
            });
    }
    BrainPrototype& owner_;
    StudioLookAndFeel studio_look_;
    OutputMeter output_meter_;
    juce::MidiKeyboardState keyboard_state_;
    StudioKeyboard keyboard_;
    juce::TextButton keyboard_toggle_;
    juce::ComboBox preset_browser_;
    juce::TextButton browse_button_;
    PresetBrowser browser_panel_;
    int browser_engine_ = -1;
    uint64_t browser_bank_revision_ = UINT64_MAX;
    bool keyboard_open_ = false;
    EnvelopeEditor juno_amp_envelope_, juno_filter_envelope_;
    EnvelopeEditor ob6_amp_envelope_, ob6_filter_envelope_;
    EnvelopeEditor modeld_amp_envelope_, modeld_filter_envelope_;
    EnvelopeEditor arp_amp_envelope_, arp_filter_envelope_, dx_amp_envelope_;
    EnvelopeEditor hybrid_a_amp_envelope_, hybrid_a_filter_envelope_;
    EnvelopeEditor hybrid_b_amp_envelope_, hybrid_b_filter_envelope_;
    LfoDisplay juno_lfo_display_, ob6_lfo_display_, modeld_lfo_display_, arp_lfo_display_;
    LfoDisplay dx_lfo_display_, hybrid_a_lfo_display_, hybrid_b_lfo_display_;
    std::array<std::array<juce::Label, 5>, 5> fx_parameter_labels_;
    std::array<juce::TextButton, 11> page_tabs_;
    std::array<juce::TextButton, 2> hybrid_focus_buttons_;
    int displayed_hybrid_focus_ = -1;
    juce::TooltipWindow tooltip_{this, 500};
    juce::TextButton import_;
    juce::ToggleButton sync_;
    juce::TextButton juno_page_;
    int juno_page_index_ = 0;
    std::array<juce::Label, OB6_OSC.size()> ob6_osc_labels_;
    std::array<juce::Slider, OB6_OSC.size()> ob6_osc_sliders_;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>, OB6_OSC.size()> ob6_osc_attachments_;
    std::array<juce::Label, OB6_VOICE.size()> ob6_voice_labels_;
    std::array<juce::Slider, OB6_VOICE.size()> ob6_voice_sliders_;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>, OB6_VOICE.size()> ob6_voice_attachments_;
    int ob6_page_index_ = 0;
    int shared_editor_engine_ = 0;
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
    std::array<juce::Label, MODELD_OSC.size()> modeld_osc_labels_;
    std::array<juce::Slider, MODELD_OSC.size()> modeld_osc_sliders_;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>, MODELD_OSC.size()> modeld_osc_attachments_;
    std::array<juce::Label, MODELD_VOICE.size()> modeld_voice_labels_;
    std::array<juce::Slider, MODELD_VOICE.size()> modeld_voice_sliders_;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>, MODELD_VOICE.size()> modeld_voice_attachments_;
    int modeld_page_index_ = 0;
    std::array<juce::Label, ARP2600_VOICE.size()> arp2600_voice_labels_;
    std::array<juce::Slider, ARP2600_VOICE.size()> arp2600_voice_sliders_;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>, ARP2600_VOICE.size()> arp2600_voice_attachments_;
    std::array<juce::Label, ARP2600_OSC.size()> arp2600_osc_labels_;
    std::array<juce::Slider, ARP2600_OSC.size()> arp2600_osc_sliders_;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>, ARP2600_OSC.size()> arp2600_osc_attachments_;
    juce::ComboBox hybrid_factory_, hybrid_focus_combo_, hybrid_osc_stop_;
    juce::TextButton hybrid_save_;
    uint64_t displayed_hybrid_bank_revision_ = UINT64_MAX;
    int hybrid_osc_attached_key_ = -1;
    std::array<juce::Label, 8> hybrid_osc_labels_;
    std::array<juce::Slider, 8> hybrid_osc_sliders_;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>, 8> hybrid_osc_attachments_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> hybrid_focus_attachment_;
    uint64_t displayed_hybrid_revision_ = 0;
    juce::TextButton hybrid_half_a_, hybrid_half_b_, hybrid_page_;
    int hybrid_page_index_ = 0;
    std::array<std::array<juce::Label, 20>, 2> hybrid_voice_labels_;
    std::array<std::array<juce::Slider, 20>, 2> hybrid_voice_sliders_;
    std::array<std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>, 20>, 2> hybrid_voice_attachments_;
    std::array<juce::Label, 8> hybrid_macro_labels_;
    std::array<juce::Slider, 8> hybrid_macro_sliders_;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>, 8> hybrid_macro_attachments_;
    std::array<juce::Label, 8> hybrid_half_labels_;
    std::array<juce::Slider, 8> hybrid_half_sliders_;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>, 8> hybrid_half_attachments_;
    juce::ToggleButton hybrid_toggle_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> hybrid_attachment_;
    std::array<juce::Label, 5> hybrid_labels_;
    std::array<juce::Slider, 5> hybrid_sliders_;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>, 5> hybrid_attachments_;
    bool displayed_hybrid_ = false;
    int arp2600_page_index_ = 0;
    juce::ToggleButton arp2600_toggle_;
    juce::Label arp2600_ring_label_;
    juce::Slider arp2600_ring_slider_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> arp2600_attachment_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> arp2600_ring_attachment_;
    bool displayed_arp2600_ = false;
    juce::ToggleButton modeld_toggle_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> modeld_attachment_;
    bool displayed_modeld_ = false;
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
