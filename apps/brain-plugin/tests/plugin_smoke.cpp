#include <juce_audio_utils/juce_audio_utils.h>
#include "preset_import.h"
#include "preset_codec.h"

#include <array>
#include <cstdio>
#include <memory>

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter();

static bool check(bool condition, const char* message) {
    if (!condition) std::fprintf(stderr, "%s\n", message);
    return condition;
}

class TestPlayHead final : public juce::AudioPlayHead {
public:
    explicit TestPlayHead(double position) : beats(position) {}
    juce::Optional<PositionInfo> getPosition() const override {
        PositionInfo info;
        info.setBpm(120.0);
        info.setPpqPosition(beats);
        info.setIsPlaying(true);
        return info;
    }
    double beats;
};

int main() {
    juce::ScopedJuceInitialiser_GUI juce_gui;
    auto a = std::unique_ptr<juce::AudioProcessor>(createPluginFilter());
    auto b = std::unique_ptr<juce::AudioProcessor>(createPluginFilter());
    a->prepareToPlay(48000.0, 128);
    b->prepareToPlay(48000.0, 128);
    juce::AudioBuffer<float> buffer(2, 128), silent(2, 128);
    juce::MidiBuffer midi, empty;
    midi.addEvent(juce::MidiMessage::noteOn(1, 60, uint8_t(100)), 40);
    a->processBlock(buffer, midi);
    b->processBlock(silent, empty);
    for (int i = 0; i < 40; ++i)
        if (!check(buffer.getSample(0, i) == 0.0f, "MIDI offset")) return 1;
    if (!check(buffer.getRMSLevel(0, 40, 88) > 0.0f, "note is audible")) return 1;
    if (!check(silent.getRMSLevel(0, 0, 128) == 0.0f, "instance isolation")) return 1;
    for (int i = 0; i < 128; ++i)
        if (!check(buffer.getSample(0, i) == buffer.getSample(1, i), "stereo output")) return 1;
    auto juno = std::unique_ptr<juce::AudioProcessor>(createPluginFilter());
    juno->prepareToPlay(48000.0, 128);
    juce::RangedAudioParameter* juno_parameter = nullptr;
    for (auto* parameter : juno->getParameters()) {
        auto* ranged = dynamic_cast<juce::RangedAudioParameter*>(parameter);
        if (ranged && ranged->getParameterID() == "juno_preview") juno_parameter = ranged;
    }
    if (!check(juno_parameter != nullptr, "Juno selector exists")) return 1;
    juno_parameter->setValueNotifyingHost(1.0f);
    juce::MidiBuffer juno_midi;
    juno_midi.addEvent(juce::MidiMessage::noteOn(1, 60, uint8_t(100)), 40);
    juno->processBlock(silent, juno_midi);
    for (int i = 0; i < 40; ++i) if (!check(silent.getSample(0, i) == 0, "Juno MIDI offset")) return 1;
    if (!check(silent.getRMSLevel(0, 40, 88) > 0, "Juno audible")) return 1;
    juce::MemoryBlock juno_state;
    juno->getStateInformation(juno_state);
    auto restored_juno = std::unique_ptr<juce::AudioProcessor>(createPluginFilter());
    restored_juno->setStateInformation(juno_state.getData(), int(juno_state.getSize()));
    bool restored_juno_selection = false;
    for (auto* parameter : restored_juno->getParameters()) {
        auto* ranged = dynamic_cast<juce::RangedAudioParameter*>(parameter);
        if (ranged && ranged->getParameterID() == "juno_preview") restored_juno_selection = ranged->getValue() > 0.5f;
    }
    if (!check(restored_juno_selection, "Juno state restore")) return 1;
    studio::Preset juno_patch;
    if (!check(studio::factory_preset(1, 0, &juno_patch), "Juno factory preset exists")) return 1;
    juno_patch.patch.mod[0][1] = 0;
    std::array<uint8_t, studio::PRESET_BYTES> juno_bytes{};
    if (!check(studio::preset_encode(juno_patch, 1, juno_bytes.data(), juno_bytes.size()) == juno_bytes.size(), "Juno encode")) return 1;
    auto* juno_import = dynamic_cast<gf::PresetImport*>(juno.get());
    if (!check(juno_import && juno_import->import_preset(juno_bytes.data(), juno_bytes.size()), "Juno GFP import")) return 1;
    juce::MemoryBlock juno_saved, juno_invalid;
    juno->getStateInformation(juno_saved);
    if (!check(!juno_import->import_preset(juno_bytes.data(), juno_bytes.size() - 1), "Juno invalid import")) return 1;
    juno->getStateInformation(juno_invalid);
    if (!check(juno_saved == juno_invalid, "Juno invalid import preserves state")) return 1;
    restored_juno->setStateInformation(juno_saved.getData(), int(juno_saved.getSize()));
    juno->prepareToPlay(44100, 128); restored_juno->prepareToPlay(44100, 128);
    juce::AudioBuffer<float> juno_a(2, 128), juno_b(2, 128);
    juce::MidiBuffer juno_note_a, juno_note_b;
    juno_note_a.addEvent(juce::MidiMessage::noteOn(1, 60, uint8_t(100)), 0);
    juno_note_b.addEvent(juce::MidiMessage::noteOn(1, 60, uint8_t(100)), 0);
    juno->processBlock(juno_a, juno_note_a); restored_juno->processBlock(juno_b, juno_note_b);
    for (int i = 0; i < 128; ++i) for (int ch = 0; ch < 2; ++ch)
        if (std::abs(juno_a.getSample(ch, i) - juno_b.getSample(ch, i)) >= 1.0e-7f) { std::fprintf(stderr, "Juno mismatch %d %d %.9f %.9f\n", ch, i, juno_a.getSample(ch, i), juno_b.getSample(ch, i)); return 1; }
    if (!check(juno_a.getRMSLevel(0, 0, 128) > 0, "Juno preset audible")) return 1;
    auto fx_juno = std::unique_ptr<juce::AudioProcessor>(createPluginFilter());
    auto dry_juno = std::unique_ptr<juce::AudioProcessor>(createPluginFilter());
    fx_juno->prepareToPlay(44100, 128); dry_juno->prepareToPlay(44100, 128);
    juno_patch.patch.osc[0][4] = 2;
    juno_patch.patch.sfx_on[GF_SFX_DRIVE] = false;
    studio::preset_encode(juno_patch, 1, juno_bytes.data(), juno_bytes.size());
    auto* dry_import = dynamic_cast<gf::PresetImport*>(dry_juno.get());
    auto* fx_import = dynamic_cast<gf::PresetImport*>(fx_juno.get());
    if (!check(dry_import && fx_import && dry_import->import_preset(juno_bytes.data(), juno_bytes.size()), "Juno dry GFP")) return 1;
    juno_patch.patch.sfx_on[GF_SFX_DRIVE] = true;
    juno_patch.patch.sfx_mix[GF_SFX_DRIVE] = 1;
    juno_patch.patch.sfx_p[GF_SFX_DRIVE][0] = 0.9f;
    studio::preset_encode(juno_patch, 1, juno_bytes.data(), juno_bytes.size());
    if (!check(fx_import->import_preset(juno_bytes.data(), juno_bytes.size()), "Juno FX GFP")) return 1;
    float stereo_delta = 0, fx_delta = 0;
    for (int block = 0; block < 50; ++block) {
        juce::MidiBuffer fx_midi, dry_midi;
        if (block == 0) { fx_midi.addEvent(juce::MidiMessage::noteOn(1, 60, uint8_t(100)), 0); dry_midi.addEvent(juce::MidiMessage::noteOn(1, 60, uint8_t(100)), 0); }
        fx_juno->processBlock(juno_a, fx_midi); dry_juno->processBlock(juno_b, dry_midi);
        for (int i = 0; i < 128; ++i) {
            stereo_delta += std::abs(juno_b.getSample(0, i) - juno_b.getSample(1, i));
            fx_delta += std::abs(juno_a.getSample(0, i) - juno_b.getSample(0, i));
        }
    }
    if (!check(stereo_delta > .01f, "Juno stereo preserved through bus")) return 1;
    if (!check(fx_delta > .01f, "Juno preset drive changes audio")) return 1;
    std::vector<uint8_t> exported_juno;
    if (!check(fx_import->export_juno_preset(exported_juno), "Juno GFP export")) return 1;
    studio::Preset exported_patch; uint8_t exported_engine = 0;
    if (!check(gf::decode_supported_preset(exported_juno.data(), exported_juno.size(), exported_patch, exported_engine) &&
               exported_engine == 1 && exported_patch.patch.sfx_on[GF_SFX_DRIVE] &&
               exported_patch.patch.osc[0][4] == 2, "Juno GFP preserves oscillator and FX")) return 1;
    if (!check(dry_import->import_preset(exported_juno.data(), exported_juno.size()), "Juno exported GFP reimport")) return 1;
    fx_juno->prepareToPlay(44100, 128); dry_juno->prepareToPlay(44100, 128);
    for (int block = 0; block < 15; ++block) {
        juce::MidiBuffer fx_events, restored_events;
        if (block == 0) { fx_events.addEvent(juce::MidiMessage::noteOn(1, 64, uint8_t(100)), 5); restored_events.addEvent(juce::MidiMessage::noteOn(1, 64, uint8_t(100)), 5); }
        fx_juno->processBlock(juno_a, fx_events); dry_juno->processBlock(juno_b, restored_events);
        for (int i = 0; i < 128; ++i) for (int ch = 0; ch < 2; ++ch)
            if (!check(std::abs(juno_a.getSample(ch, i) - juno_b.getSample(ch, i)) < 1.0e-6f, "Juno exported GFP audible roundtrip")) return 1;
    }
    auto* sync_param = dynamic_cast<juce::RangedAudioParameter*>(fx_juno->getParameters()[4]);
    sync_param->setValueNotifyingHost(1.0f);
    auto before_rejected_export = exported_juno;
    if (!check(!fx_import->export_juno_preset(exported_juno) && exported_juno == before_rejected_export,
               "Juno sync export rejected without modifying data")) return 1;
    auto find_parameter = [](juce::AudioProcessor& processor, const char* id) -> juce::RangedAudioParameter* {
        for (auto* parameter : processor.getParameters()) {
            auto* ranged = dynamic_cast<juce::RangedAudioParameter*>(parameter);
            if (ranged && ranged->getParameterID() == id) return ranged;
        }
        return nullptr;
    };
    auto* juno_wave = find_parameter(*dry_juno, "juno.wave");
    auto* juno_sub = find_parameter(*dry_juno, "juno.sub");
    auto* juno_chorus = find_parameter(*dry_juno, "juno.chorus");
    if (!check(juno_wave && juno_sub && juno_chorus, "Juno OSC host parameters")) return 1;
    juno_wave->setValueNotifyingHost(juno_wave->convertTo0to1(2));
    juno_sub->setValueNotifyingHost(juno_sub->convertTo0to1(75));
    juno_chorus->setValueNotifyingHost(juno_chorus->convertTo0to1(0));
    auto* juno_filter_decay = find_parameter(*dry_juno, "juno.filter_decay");
    auto* juno_drive = find_parameter(*dry_juno, "juno.drive");
    if (!check(juno_filter_decay && juno_drive, "Juno ENV host parameters")) return 1;
    juno_filter_decay->setValueNotifyingHost(juno_filter_decay->convertTo0to1(730));
    juno_drive->setValueNotifyingHost(juno_drive->convertTo0to1(.6f));
    auto* juno_lfo_rate = find_parameter(*dry_juno, "juno.lfo_rate");
    auto* juno_lfo_pwm = find_parameter(*dry_juno, "juno.lfo_pwm");
    auto* juno_lfo_delay = find_parameter(*dry_juno, "juno.lfo_delay");
    if (!check(juno_lfo_rate && juno_lfo_pwm && juno_lfo_delay, "Juno LFO host parameters")) return 1;
    juno_lfo_rate->setValueNotifyingHost(juno_lfo_rate->convertTo0to1(7));
    juno_lfo_pwm->setValueNotifyingHost(juno_lfo_pwm->convertTo0to1(.8f));
    juno_lfo_delay->setValueNotifyingHost(juno_lfo_delay->convertTo0to1(50));
    auto* glide_control = find_parameter(*dry_juno, "juno.glide");
    auto* unison_control = find_parameter(*dry_juno, "juno.unison_on");
    auto* count_control = find_parameter(*dry_juno, "juno.unison_count");
    auto* detune_control = find_parameter(*dry_juno, "juno.unison_detune");
    auto* drift_control = find_parameter(*dry_juno, "juno.drift");
    if (!check(glide_control && unison_control && count_control && detune_control && drift_control, "Juno voice groove parameters")) return 1;
    glide_control->setValueNotifyingHost(glide_control->convertTo0to1(375));
    unison_control->setValueNotifyingHost(unison_control->convertTo0to1(1));
    count_control->setValueNotifyingHost(count_control->convertTo0to1(2));
    detune_control->setValueNotifyingHost(detune_control->convertTo0to1(30));
    drift_control->setValueNotifyingHost(drift_control->convertTo0to1(40));
    auto* pump_on = find_parameter(*dry_juno, "juno.groove.pump.on");
    auto* pump_depth = find_parameter(*dry_juno, "juno.groove.pump.p1");
    auto* arp_on = find_parameter(*dry_juno, "juno.groove.arp.on");
    auto* chord_on = find_parameter(*dry_juno, "juno.groove.chord.on");
    if (!check(pump_on && pump_depth && arp_on && chord_on, "Juno PUMP ARP CHORD parameters")) return 1;
    pump_on->setValueNotifyingHost(1); pump_depth->setValueNotifyingHost(pump_depth->convertTo0to1(75));
    auto* pump_source = find_parameter(*dry_juno, "juno.groove.pump.p6");
    if (!check(pump_source != nullptr, "Juno GFP pump source parameter")) return 1;
    pump_source->setValueNotifyingHost(pump_source->convertTo0to1(2));
    auto* juno_fx_on = find_parameter(*dry_juno, "juno.fx.delay.on");
    auto* juno_fx_mix = find_parameter(*dry_juno, "juno.fx.delay.mix");
    if (!check(juno_fx_on && juno_fx_mix, "Juno FX host parameters")) return 1;
    juno_fx_on->setValueNotifyingHost(1); juno_fx_mix->setValueNotifyingHost(.65f);
    juce::MemoryBlock juno_edited_state;
    dry_juno->getStateInformation(juno_edited_state);
    auto edited_restore = std::unique_ptr<juce::AudioProcessor>(createPluginFilter());
    edited_restore->setStateInformation(juno_edited_state.getData(), int(juno_edited_state.getSize()));
    auto* restored_wave = find_parameter(*edited_restore, "juno.wave");
    auto* restored_sub = find_parameter(*edited_restore, "juno.sub");
    if (!check(restored_wave && restored_sub && std::abs(restored_wave->convertFrom0to1(restored_wave->getValue()) - 2) < .01f &&
               std::abs(restored_sub->convertFrom0to1(restored_sub->getValue()) - 75) < .01f, "Juno OSC state restore")) return 1;
    std::vector<uint8_t> edited_gfp;
    if (!check(dry_import->export_juno_preset(edited_gfp), "Juno edited GFP export")) return 1;
    studio::Preset edited_patch; uint8_t edited_engine = 0;
    if (!check(gf::decode_supported_preset(edited_gfp.data(), edited_gfp.size(), edited_patch, edited_engine) &&
               edited_engine == 1 && edited_patch.patch.osc[0][0] == 2 && edited_patch.patch.osc[0][1] == 75 &&
               edited_patch.patch.osc[0][4] == 0, "Juno OSC exported values")) return 1;
    if (!check(std::abs(edited_patch.patch.f_decay_ms - 730) < .01f && std::abs(edited_patch.patch.drive - .6f) < .0001f,
               "Juno ENV exported values")) return 1;
    if (!check(std::abs(edited_patch.patch.lfo_rate_hz - 7) < .001f &&
               std::abs(edited_patch.patch.lfo_amt[GF_DEST_PWM] - .8f) < .001f &&
               std::abs(edited_patch.patch.mod[0][0] - 50) < .01f, "Juno LFO exported values")) return 1;
    if (!check(std::abs(edited_patch.patch.glide - .5f) < .0001f && edited_patch.patch.groove_on[GF_GROOVE_UNISON] &&
               std::abs(edited_patch.patch.groove[GF_GROOVE_UNISON][GF_UNISON_VOICES] - 2) < .01f &&
               std::abs(edited_patch.patch.groove[GF_GROOVE_UNISON][GF_UNISON_DETUNE] - 30) < .01f &&
               std::abs(edited_patch.patch.groove[GF_GROOVE_MACRO][GF_MACRO_DRIFT] - 40) < .01f, "Juno voice groove GFP export")) return 1;
    if (!check(edited_patch.patch.groove_on[GF_GROOVE_PUMP] &&
               std::abs(edited_patch.patch.groove[GF_GROOVE_PUMP][GF_PUMP_DEPTH] - 75) < .01f &&
               std::abs(edited_patch.patch.groove[GF_GROOVE_PUMP][GF_PUMP_SOURCE] - 2) < .01f,
               "Juno PUMP GFP export")) return 1;
    auto* restored_pump_source = find_parameter(*edited_restore, "juno.groove.pump.p6");
    if (!check(restored_pump_source && std::abs(restored_pump_source->convertFrom0to1(restored_pump_source->getValue()) - 2) < .01f,
               "Juno pump source state restore")) return 1;
    auto* restored_pump = find_parameter(*edited_restore, "juno.groove.pump.on");
    if (!check(restored_pump && restored_pump->getValue() > .5f, "Juno PUMP state restore")) return 1;
    if (!check(edited_patch.patch.sfx_on[GF_SFX_DELAY] && std::abs(edited_patch.patch.sfx_mix[GF_SFX_DELAY] - .65f) < .001f,
               "Juno FX GFP export")) return 1;
    auto* restored_delay = find_parameter(*edited_restore, "juno.fx.delay.on");
    if (!check(restored_delay && restored_delay->getValue() > .5f, "Juno FX state restore")) return 1;
    auto* restored_glide = find_parameter(*edited_restore, "juno.glide");
    if (!check(restored_glide && std::abs(restored_glide->convertFrom0to1(restored_glide->getValue()) - 375) < .01f,
               "Juno voice groove state restore")) return 1;
    auto* restored_lfo = find_parameter(*edited_restore, "juno.lfo_pwm");
    if (!check(restored_lfo && std::abs(restored_lfo->convertFrom0to1(restored_lfo->getValue()) - .8f) < .001f,
               "Juno LFO state restore")) return 1;
    auto* restored_filter_decay = find_parameter(*edited_restore, "juno.filter_decay");
    if (!check(restored_filter_decay && std::abs(restored_filter_decay->convertFrom0to1(restored_filter_decay->getValue()) - 730) < .01f,
               "Juno ENV state restore")) return 1;
    auto juno_v1_xml = juce::AudioProcessor::getXmlFromBinary(juno_edited_state.getData(), int(juno_edited_state.getSize()));
    if (!check(juno_v1_xml != nullptr, "Juno v1 state XML")) return 1;
    juno_v1_xml->setAttribute("junoGrooveVersion", 1);
    juce::MemoryBlock juno_v1_state;
    juce::AudioProcessor::copyXmlToBinary(*juno_v1_xml, juno_v1_state);
    auto juno_v1_restore = std::unique_ptr<juce::AudioProcessor>(createPluginFilter());
    juno_v1_restore->setStateInformation(juno_v1_state.getData(), int(juno_v1_state.getSize()));
    auto* v1_source = find_parameter(*juno_v1_restore, "juno.groove.pump.p6");
    auto* v1_depth = find_parameter(*juno_v1_restore, "juno.groove.pump.p1");
    if (!check(v1_source && v1_depth &&
               std::abs(v1_source->convertFrom0to1(v1_source->getValue()) - juno_patch.patch.groove[GF_GROOVE_PUMP][GF_PUMP_SOURCE]) < .01f &&
               std::abs(v1_depth->convertFrom0to1(v1_depth->getValue()) - 75) < .01f, "Juno v1 source migration preserves pump edits")) return 1;
    auto juno_legacy_xml = juce::AudioProcessor::getXmlFromBinary(juno_saved.getData(), int(juno_saved.getSize()));
    if (!check(juno_legacy_xml != nullptr, "Juno legacy state XML")) return 1;
    juno_legacy_xml->removeAttribute("junoOscVersion");
    juno_legacy_xml->removeAttribute("junoEnvVersion");
    juno_legacy_xml->removeAttribute("junoLfoVersion");
    juno_legacy_xml->removeAttribute("junoVoiceGrooveVersion");
    juno_legacy_xml->removeAttribute("junoGrooveVersion");
    juno_legacy_xml->removeAttribute("junoFxVersion");
    juce::MemoryBlock juno_legacy_state;
    juce::AudioProcessor::copyXmlToBinary(*juno_legacy_xml, juno_legacy_state);
    auto juno_legacy_restore = std::unique_ptr<juce::AudioProcessor>(createPluginFilter());
    juno_legacy_restore->setStateInformation(juno_legacy_state.getData(), int(juno_legacy_state.getSize()));
    auto* juno_legacy_wave = find_parameter(*juno_legacy_restore, "juno.wave");
    if (!check(juno_legacy_wave && std::abs(juno_legacy_wave->convertFrom0to1(juno_legacy_wave->getValue()) - juno_patch.patch.osc[0][0]) < .01f,
               "Juno legacy state migrates GFP oscillator")) return 1;
    auto* juno_legacy_decay = find_parameter(*juno_legacy_restore, "juno.filter_decay");
    if (!check(juno_legacy_decay && std::abs(juno_legacy_decay->convertFrom0to1(juno_legacy_decay->getValue()) - juno_patch.patch.f_decay_ms) < .01f,
               "Juno legacy state migrates GFP envelope")) return 1;
    auto* legacy_lfo = find_parameter(*juno_legacy_restore, "juno.lfo_rate");
    if (!check(legacy_lfo && std::abs(legacy_lfo->convertFrom0to1(legacy_lfo->getValue()) - juno_patch.patch.lfo_rate_hz) < .01f,
               "Juno legacy state migrates GFP LFO")) return 1;
    auto* legacy_glide = find_parameter(*juno_legacy_restore, "juno.glide");
    if (!check(legacy_glide && std::abs(legacy_glide->convertFrom0to1(legacy_glide->getValue()) -
               juno_patch.patch.glide * juno_patch.patch.glide * 1500.0f) < .01f, "Juno legacy voice groove migration")) return 1;
    dry_juno->prepareToPlay(44100, 128); edited_restore->prepareToPlay(44100, 128);
    float edited_difference = 0;
    for (int block = 0; block < 10; ++block) {
        juce::MidiBuffer one, two;
        if (!block) { one.addEvent(juce::MidiMessage::noteOn(1, 60, uint8_t(100)), 0); two.addEvent(juce::MidiMessage::noteOn(1, 60, uint8_t(100)), 0); }
        dry_juno->processBlock(juno_a, one); edited_restore->processBlock(juno_b, two);
        for (int i = 0; i < 128; ++i) edited_difference += std::abs(juno_a.getSample(0, i) - juno_b.getSample(0, i));
    }
    if (!check(edited_difference < 1e-4f, "Juno edited OSC state audio restore")) return 1;
    auto unedited = std::unique_ptr<juce::AudioProcessor>(createPluginFilter());
    auto* unedited_import = dynamic_cast<gf::PresetImport*>(unedited.get());
    if (!check(unedited_import && unedited_import->import_preset(exported_juno.data(), exported_juno.size()), "Juno unedited preset")) return 1;
    unedited->prepareToPlay(44100, 128); dry_juno->prepareToPlay(44100, 128);
    float changed_osc_audio = 0;
    for (int block = 0; block < 10; ++block) {
        juce::MidiBuffer one, two;
        if (!block) { one.addEvent(juce::MidiMessage::noteOn(1, 60, uint8_t(100)), 0); two.addEvent(juce::MidiMessage::noteOn(1, 60, uint8_t(100)), 0); }
        dry_juno->processBlock(juno_a, one); unedited->processBlock(juno_b, two);
        for (int i = 0; i < 128; ++i) changed_osc_audio += std::abs(juno_a.getSample(0, i) - juno_b.getSample(0, i));
    }
    if (!check(changed_osc_audio > .01f, "Juno OSC automation changes audio")) return 1;
    studio::Preset macro_juno = juno_patch;
    macro_juno.patch.decay_ms = 1000; macro_juno.patch.drive = .2f;
    macro_juno.patch.groove[GF_GROOVE_MACRO][GF_MACRO_POCKET] = 100;
    macro_juno.patch.groove[GF_GROOVE_MACRO][GF_MACRO_GRIT] = 50;
    studio::preset_encode(macro_juno, 1, juno_bytes.data(), juno_bytes.size());
    auto macro_plugin = std::unique_ptr<juce::AudioProcessor>(createPluginFilter());
    auto* juno_macro_import = dynamic_cast<gf::PresetImport*>(macro_plugin.get());
    if (!check(juno_macro_import && juno_macro_import->import_preset(juno_bytes.data(), juno_bytes.size()), "Juno macro preset import")) return 1;
    std::vector<uint8_t> macro_export;
    if (!check(juno_macro_import->export_juno_preset(macro_export), "Juno macro preset export")) return 1;
    studio::Preset macro_result; uint8_t macro_engine = 0;
    if (!check(gf::decode_supported_preset(macro_export.data(), macro_export.size(), macro_result, macro_engine) &&
               std::abs(macro_result.patch.decay_ms - 200) < .02f && std::abs(macro_result.patch.drive - .5f) < .001f &&
               macro_result.patch.groove[GF_GROOVE_MACRO][GF_MACRO_GRIT] == 0 &&
               macro_result.patch.groove[GF_GROOVE_MACRO][GF_MACRO_POCKET] == 0, "Juno macro effective ENV export")) return 1;
    auto lfo_off_plugin = std::unique_ptr<juce::AudioProcessor>(createPluginFilter());
    auto lfo_on_plugin = std::unique_ptr<juce::AudioProcessor>(createPluginFilter());
    auto* lfo_off_import = dynamic_cast<gf::PresetImport*>(lfo_off_plugin.get());
    auto* lfo_on_import = dynamic_cast<gf::PresetImport*>(lfo_on_plugin.get());
    if (!check(lfo_off_import && lfo_on_import && lfo_off_import->import_preset(edited_gfp.data(), edited_gfp.size()) &&
               lfo_on_import->import_preset(edited_gfp.data(), edited_gfp.size()), "Juno LFO test presets")) return 1;
    auto* off_pwm = find_parameter(*lfo_off_plugin, "juno.lfo_pwm");
    off_pwm->setValueNotifyingHost(off_pwm->convertTo0to1(0));
    lfo_off_plugin->prepareToPlay(44100, 128); lfo_on_plugin->prepareToPlay(44100, 128);
    float lfo_difference = 0;
    for (int block = 0; block < 50; ++block) {
        juce::MidiBuffer one, two;
        if (!block) { one.addEvent(juce::MidiMessage::noteOn(1, 60, uint8_t(100)), 0); two.addEvent(juce::MidiMessage::noteOn(1, 60, uint8_t(100)), 0); }
        lfo_off_plugin->processBlock(juno_a, one); lfo_on_plugin->processBlock(juno_b, two);
        for (int i = 0; i < 128; ++i) lfo_difference += std::abs(juno_a.getSample(0, i) - juno_b.getSample(0, i));
    }
    if (!check(lfo_difference > .01f, "Juno LFO automation changes audio")) return 1;
    auto pump_off_plugin = std::unique_ptr<juce::AudioProcessor>(createPluginFilter());
    auto pump_on_plugin = std::unique_ptr<juce::AudioProcessor>(createPluginFilter());
    auto* pump_off_import = dynamic_cast<gf::PresetImport*>(pump_off_plugin.get());
    auto* pump_on_import = dynamic_cast<gf::PresetImport*>(pump_on_plugin.get());
    if (!check(pump_off_import && pump_on_import && pump_off_import->import_preset(edited_gfp.data(), edited_gfp.size()) &&
               pump_on_import->import_preset(edited_gfp.data(), edited_gfp.size()), "Juno pump test presets")) return 1;
    auto* pump_off = find_parameter(*pump_off_plugin, "juno.groove.pump.on");
    pump_off->setValueNotifyingHost(0);
    pump_off_plugin->prepareToPlay(44100, 128); pump_on_plugin->prepareToPlay(44100, 128);
    float pump_difference = 0;
    for (int block = 0; block < 40; ++block) {
        juce::MidiBuffer one, two;
        if (!block) { one.addEvent(juce::MidiMessage::noteOn(1, 60, uint8_t(100)), 0); two.addEvent(juce::MidiMessage::noteOn(1, 60, uint8_t(100)), 0); }
        pump_off_plugin->processBlock(juno_a, one); pump_on_plugin->processBlock(juno_b, two);
        for (int i = 0; i < 128; ++i) pump_difference += std::abs(juno_a.getSample(0, i) - juno_b.getSample(0, i));
    }
    if (!check(pump_difference > .01f, "Juno pump automation changes audio")) return 1;
    auto* pump_on_source = find_parameter(*pump_on_plugin, "juno.groove.pump.p6");
    auto* pump_off_source = find_parameter(*pump_off_plugin, "juno.groove.pump.p6");
    auto* pump_off_enable = find_parameter(*pump_off_plugin, "juno.groove.pump.on");
    pump_off_enable->setValueNotifyingHost(1);
    pump_off_source->setValueNotifyingHost(pump_off_source->convertTo0to1(0));
    pump_on_source->setValueNotifyingHost(pump_on_source->convertTo0to1(2));
    pump_off_plugin->prepareToPlay(44100, 128); pump_on_plugin->prepareToPlay(44100, 128);
    float source_difference = 0;
    for (int block = 0; block < 20; ++block) {
        juce::MidiBuffer one, two;
        if (!block) { one.addEvent(juce::MidiMessage::noteOn(1, 60, uint8_t(100)), 0); two.addEvent(juce::MidiMessage::noteOn(1, 60, uint8_t(100)), 0); }
        pump_off_plugin->processBlock(juno_a, one); pump_on_plugin->processBlock(juno_b, two);
        for (int i = 0; i < 128; ++i) source_difference += std::abs(juno_a.getSample(0, i) - juno_b.getSample(0, i));
    }
    if (!check(source_difference < .0001f, "Juno GFP source does not override DAW clock")) return 1;
    auto fx_auto_a = std::unique_ptr<juce::AudioProcessor>(createPluginFilter());
    auto fx_auto_b = std::unique_ptr<juce::AudioProcessor>(createPluginFilter());
    auto* fx_auto_import_a = dynamic_cast<gf::PresetImport*>(fx_auto_a.get());
    auto* fx_auto_import_b = dynamic_cast<gf::PresetImport*>(fx_auto_b.get());
    if (!check(fx_auto_import_a && fx_auto_import_b && fx_auto_import_a->import_preset(exported_juno.data(), exported_juno.size()) &&
               fx_auto_import_b->import_preset(exported_juno.data(), exported_juno.size()), "Juno FX automation preset")) return 1;
    auto* drive_on_a = find_parameter(*fx_auto_a, "juno.fx.drive.on");
    auto* drive_on_b = find_parameter(*fx_auto_b, "juno.fx.drive.on");
    drive_on_a->setValueNotifyingHost(0); drive_on_b->setValueNotifyingHost(0);
    fx_auto_a->prepareToPlay(44100, 128); fx_auto_b->prepareToPlay(44100, 128);
    float changed_fx_audio = 0, held_fx_energy = 0;
    for (int block = 0; block < 40; ++block) {
        juce::MidiBuffer one, two;
        if (!block) { one.addEvent(juce::MidiMessage::noteOn(1, 60, uint8_t(100)), 0); two.addEvent(juce::MidiMessage::noteOn(1, 60, uint8_t(100)), 0); }
        if (block == 10) drive_on_b->setValueNotifyingHost(1);
        fx_auto_a->processBlock(juno_a, one); fx_auto_b->processBlock(juno_b, two);
        if (block >= 10) for (int i = 0; i < 128; ++i) {
            changed_fx_audio += std::abs(juno_a.getSample(0, i) - juno_b.getSample(0, i));
            held_fx_energy += std::abs(juno_b.getSample(0, i));
        }
    }
    if (!check(changed_fx_audio > .01f && held_fx_energy > .01f, "Juno FX automation changes audio without ending note")) return 1;
    auto juno_macro_live = std::unique_ptr<juce::AudioProcessor>(createPluginFilter());
    auto juno_macro_reference = std::unique_ptr<juce::AudioProcessor>(createPluginFilter());
    auto* live_import = dynamic_cast<gf::PresetImport*>(juno_macro_live.get());
    auto* reference_import = dynamic_cast<gf::PresetImport*>(juno_macro_reference.get());
    if (!check(live_import && reference_import && live_import->import_preset(edited_gfp.data(), edited_gfp.size()) &&
               reference_import->import_preset(edited_gfp.data(), edited_gfp.size()), "Juno macro automation presets")) return 1;
    auto* grit_knob = find_parameter(*juno_macro_live, "juno.macro.grit");
    auto* pocket_knob = find_parameter(*juno_macro_live, "juno.macro.pocket");
    auto* move_knob = find_parameter(*juno_macro_live, "juno.macro.move");
    if (!check(grit_knob && pocket_knob && move_knob, "Juno macro parameters")) return 1;
    grit_knob->setValueNotifyingHost(.5f); pocket_knob->setValueNotifyingHost(.4f); move_knob->setValueNotifyingHost(.3f);
    juce::MemoryBlock macro_state;
    juno_macro_live->getStateInformation(macro_state);
    auto macro_restored = std::unique_ptr<juce::AudioProcessor>(createPluginFilter());
    macro_restored->setStateInformation(macro_state.getData(), int(macro_state.getSize()));
    auto* macro_restored_grit = find_parameter(*macro_restored, "juno.macro.grit");
    if (!check(macro_restored_grit && std::abs(macro_restored_grit->getValue() - .5f) < .001f, "Juno macro state restore")) return 1;
    std::vector<uint8_t> macro_live_gfp;
    if (!check(live_import->export_juno_preset(macro_live_gfp), "Juno live macro export")) return 1;
    auto juno_macro_roundtrip = std::unique_ptr<juce::AudioProcessor>(createPluginFilter());
    auto* macro_roundtrip_import = dynamic_cast<gf::PresetImport*>(juno_macro_roundtrip.get());
    if (!check(macro_roundtrip_import && macro_roundtrip_import->import_preset(macro_live_gfp.data(), macro_live_gfp.size()), "Juno live macro reimport")) return 1;
    juno_macro_live->prepareToPlay(44100, 128); juno_macro_reference->prepareToPlay(44100, 128);
    juno_macro_roundtrip->prepareToPlay(44100, 128); macro_restored->prepareToPlay(44100, 128);
    juce::AudioBuffer<float> juno_macro_audio(2, 128), restored_juno_macro_audio(2, 128);
    float macro_delta = 0, macro_roundtrip_delta = 0, macro_state_delta = 0;
    for (int block = 0; block < 30; ++block) {
        juce::MidiBuffer one, two, three, four;
        if (!block) for (auto* events : {&one, &two, &three, &four}) events->addEvent(juce::MidiMessage::noteOn(1, 60, uint8_t(100)), 0);
        juno_macro_live->processBlock(juno_a, one); juno_macro_reference->processBlock(juno_b, two);
        juno_macro_roundtrip->processBlock(juno_macro_audio, three); macro_restored->processBlock(restored_juno_macro_audio, four);
        for (int i = 0; i < 128; ++i) {
            macro_delta += std::abs(juno_a.getSample(0, i) - juno_b.getSample(0, i));
            macro_roundtrip_delta += std::abs(juno_a.getSample(0, i) - juno_macro_audio.getSample(0, i));
            macro_state_delta += std::abs(juno_a.getSample(0, i) - restored_juno_macro_audio.getSample(0, i));
        }
    }
    if (!check(macro_delta > .01f, "Juno macros change audio")) return 1;
    if (!check(macro_roundtrip_delta < .01f, "Juno macro GFP audio roundtrip")) return 1;
    if (!check(macro_state_delta < .0001f, "Juno macro project audio restore")) return 1;
    auto juno_editor = std::unique_ptr<juce::AudioProcessorEditor>(dry_juno->createEditor());
    if (!check(juno_editor != nullptr, "Juno editor constructed")) return 1;
    auto juno_large_blocks = std::unique_ptr<juce::AudioProcessor>(createPluginFilter());
    auto juno_small_blocks = std::unique_ptr<juce::AudioProcessor>(createPluginFilter());
    auto* large_select = find_parameter(*juno_large_blocks, "juno_preview");
    auto* small_select = find_parameter(*juno_small_blocks, "juno_preview");
    large_select->setValueNotifyingHost(1); small_select->setValueNotifyingHost(1);
    juno_large_blocks->prepareToPlay(48000, 128); juno_small_blocks->prepareToPlay(48000, 32);
    for (int block = 0; block < 40; ++block) {
        juce::MidiBuffer large_events;
        if (!block) large_events.addEvent(juce::MidiMessage::noteOn(1, 60, uint8_t(100)), 40);
        juno_large_blocks->processBlock(juno_a, large_events);
        for (int part = 0; part < 4; ++part) {
            juce::AudioBuffer<float> small(2, 32);
            juce::MidiBuffer small_events;
            if (!block && part == 1) small_events.addEvent(juce::MidiMessage::noteOn(1, 60, uint8_t(100)), 8);
            juno_small_blocks->processBlock(small, small_events);
            for (int i = 0; i < 32; ++i) for (int channel = 0; channel < 2; ++channel)
                if (!check(std::abs(juno_a.getSample(channel, part * 32 + i) - small.getSample(channel, i)) < 1e-7f,
                           "Juno audio independent of DAW block size")) return 1;
        }
    }
    auto ob6_plugin = std::unique_ptr<juce::AudioProcessor>(createPluginFilter());
    auto* ob6_select = find_parameter(*ob6_plugin, "ob6_preview");
    if (!check(ob6_select != nullptr, "OB-6 selector exists")) return 1;
    ob6_select->setValueNotifyingHost(1);
    ob6_plugin->prepareToPlay(48000, 128);
    juce::AudioBuffer<float> ob6_audio(2, 128);
    juce::MidiBuffer ob6_midi;
    ob6_midi.addEvent(juce::MidiMessage::noteOn(1, 60, uint8_t(100)), 40);
    ob6_plugin->processBlock(ob6_audio, ob6_midi);
    for (int i = 0; i < 40; ++i) if (!check(ob6_audio.getSample(0, i) == 0, "OB-6 MIDI offset")) return 1;
    if (!check(ob6_audio.getRMSLevel(0, 40, 88) > 0, "OB-6 audible")) return 1;
    juce::MemoryBlock ob6_state;
    ob6_plugin->getStateInformation(ob6_state);
    auto ob6_restore = std::unique_ptr<juce::AudioProcessor>(createPluginFilter());
    ob6_restore->setStateInformation(ob6_state.getData(), int(ob6_state.getSize()));
    auto* ob6_restored_select = find_parameter(*ob6_restore, "ob6_preview");
    if (!check(ob6_restored_select && ob6_restored_select->getValue() > .5f, "OB-6 selection restores")) return 1;
    studio::Preset ob6_preset;
    if (!check(studio::factory_preset(3, 0, &ob6_preset), "OB-6 factory preset")) return 1;
    ob6_preset.patch.osc[0][6] = 80;
    std::array<uint8_t, studio::PRESET_BYTES> ob6_bytes{};
    studio::preset_encode(ob6_preset, 3, ob6_bytes.data(), ob6_bytes.size());
    auto* ob6_import = dynamic_cast<gf::PresetImport*>(ob6_plugin.get());
    if (!check(ob6_import && ob6_import->import_preset(ob6_bytes.data(), ob6_bytes.size()), "OB-6 GFP import")) return 1;
    juce::MemoryBlock ob6_saved, ob6_after_invalid;
    ob6_plugin->getStateInformation(ob6_saved);
    if (!check(!ob6_import->import_preset(ob6_bytes.data(), ob6_bytes.size() - 1), "OB-6 rejects truncated GFP")) return 1;
    ob6_plugin->getStateInformation(ob6_after_invalid);
    if (!check(ob6_saved == ob6_after_invalid, "OB-6 invalid GFP preserves state")) return 1;
    ob6_restore->setStateInformation(ob6_saved.getData(), int(ob6_saved.getSize()));
    ob6_plugin->prepareToPlay(44100, 128); ob6_restore->prepareToPlay(44100, 128);
    juce::AudioBuffer<float> ob6_restored_audio(2, 128);
    juce::MidiBuffer ob6_event_a, ob6_event_b;
    ob6_event_a.addEvent(juce::MidiMessage::noteOn(1, 60, uint8_t(100)), 0);
    ob6_event_b.addEvent(juce::MidiMessage::noteOn(1, 60, uint8_t(100)), 0);
    ob6_plugin->processBlock(ob6_audio, ob6_event_a); ob6_restore->processBlock(ob6_restored_audio, ob6_event_b);
    for (int i = 0; i < 128; ++i) if (!check(std::abs(ob6_audio.getSample(0, i) - ob6_restored_audio.getSample(0, i)) < 1e-7f,
                                               "OB-6 GFP audio restores")) return 1;
    if (!check(ob6_audio.getRMSLevel(0, 0, 128) > 0, "OB-6 GFP audible")) return 1;
    auto params = a->getParameters();
    if (!check(!params.isEmpty(), "parameters available")) return 1;
    params[0]->setValueNotifyingHost(0.75f);
    params[4]->setValueNotifyingHost(1.0f);
    juce::MemoryBlock state;
    a->getStateInformation(state);
    b->setStateInformation(state.getData(), int(state.getSize()));
    if (!check(std::abs(b->getParameters()[0]->getValue() - 0.75f) < 0.001f, "state restore")) return 1;
    if (!check(b->getParameters()[4]->getValue() > 0.5f, "LFO sync state restore")) return 1;
    auto c = std::unique_ptr<juce::AudioProcessor>(createPluginFilter());
    c->prepareToPlay(48000.0, 32);
    for (int block = 0; block < 4; ++block) {
        juce::AudioBuffer<float> part(2, 32);
        juce::MidiBuffer events;
        if (block == 1) events.addEvent(juce::MidiMessage::noteOn(1, 60, uint8_t(100)), 8);
        c->processBlock(part, events);
        for (int i = 0; i < 32; ++i)
            if (!check(std::abs(part.getSample(0, i) - buffer.getSample(0, block * 32 + i)) < 1.0e-7f,
                       "variable block partition")) return 1;
    }
    auto d = std::unique_ptr<juce::AudioProcessor>(createPluginFilter());
    d->prepareToPlay(48000.0, 128);
    juce::MidiBuffer held;
    held.addEvent(juce::MidiMessage::controllerEvent(1, 64, 127), 0);
    held.addEvent(juce::MidiMessage::noteOn(1, 60, uint8_t(100)), 1);
    held.addEvent(juce::MidiMessage::noteOff(1, 60), 32);
    d->processBlock(silent, held);
    juce::MidiBuffer none;
    float max_level = 0.0f;
    for (int i = 0; i < 400; ++i) {
        d->processBlock(silent, none);
        if (i > 390) max_level = std::max(max_level, silent.getRMSLevel(0, 0, 128));
    }
    if (!check(max_level > 0.001f, "MIDI sustain controller holds note")) return 1;
    juce::MidiBuffer release_pedal;
    release_pedal.addEvent(juce::MidiMessage::controllerEvent(1, 64, 0), 0);
    d->processBlock(silent, release_pedal);
    for (int i = 0; i < 1600; ++i) d->processBlock(silent, none);
    if (!check(silent.getRMSLevel(0, 0, 128) == 0.0f, "MIDI sustain pedal release")) return 1;
    auto e = std::unique_ptr<juce::AudioProcessor>(createPluginFilter());
    auto f = std::unique_ptr<juce::AudioProcessor>(createPluginFilter());
    e->prepareToPlay(48000.0, 128); f->prepareToPlay(48000.0, 128);
    juce::AudioBuffer<float> bent(2, 128), straight(2, 128);
    juce::MidiBuffer e_note, f_note;
    e_note.addEvent(juce::MidiMessage::noteOn(1, 69, uint8_t(100)), 0);
    f_note.addEvent(juce::MidiMessage::noteOn(1, 69, uint8_t(100)), 0);
    e->processBlock(bent, e_note); f->processBlock(straight, f_note);
    juce::MidiBuffer bend, no_bend;
    bend.addEvent(juce::MidiMessage::pitchWheel(1, 16383), 0);
    e->processBlock(bent, bend); f->processBlock(straight, no_bend);
    float pitch_difference = 0.0f;
    for (int i = 0; i < 128; ++i) pitch_difference += std::abs(bent.getSample(0, i) - straight.getSample(0, i));
    if (!check(pitch_difference > 0.001f, "MIDI pitch wheel")) return 1;
    studio::Preset preset;
    if (!check(gf::factory_prophet_preset(0, preset), "factory preset")) return 1;
    preset.patch.osc[0][2] = 0.0f;
    preset.patch.osc[0][3] = 1.0f;
    std::array<uint8_t, studio::PRESET_BYTES> encoded{};
    if (!check(studio::preset_encode(preset, 2, encoded.data(), encoded.size()) == encoded.size(), "encode preset")) return 1;
    auto* importer = dynamic_cast<gf::PresetImport*>(e.get());
    if (!check(importer && importer->import_preset(encoded.data(), encoded.size()), "import preset")) return 1;
    juce::MemoryBlock saved_preset, after_invalid;
    e->getStateInformation(saved_preset);
    if (!check(!importer->import_preset(encoded.data(), encoded.size() - 1), "reject invalid preset")) return 1;
    e->getStateInformation(after_invalid);
    if (!check(saved_preset == after_invalid, "invalid preset preserves state")) return 1;
    auto restored = std::unique_ptr<juce::AudioProcessor>(createPluginFilter());
    restored->setStateInformation(saved_preset.getData(), int(saved_preset.getSize()));
    e->prepareToPlay(44100.0, 128); restored->prepareToPlay(44100.0, 128);
    juce::AudioBuffer<float> original_audio(2, 128), restored_audio(2, 128);
    juce::MidiBuffer original_midi, restored_midi;
    original_midi.addEvent(juce::MidiMessage::noteOn(1, 60, uint8_t(100)), 0);
    restored_midi.addEvent(juce::MidiMessage::noteOn(1, 60, uint8_t(100)), 0);
    e->processBlock(original_audio, original_midi);
    restored->processBlock(restored_audio, restored_midi);
    for (int i = 0; i < 128; ++i)
        if (!check(std::abs(original_audio.getSample(0, i) - restored_audio.getSample(0, i)) < 1.0e-7f,
                   "preset state restores audio")) return 1;

    auto on_beat = std::unique_ptr<juce::AudioProcessor>(createPluginFilter());
    auto off_beat = std::unique_ptr<juce::AudioProcessor>(createPluginFilter());
    TestPlayHead downbeat(0.0), later(0.25);
    on_beat->setPlayHead(&downbeat); off_beat->setPlayHead(&later);
    on_beat->prepareToPlay(48000.0, 128); off_beat->prepareToPlay(48000.0, 128);
    preset.patch.groove_on[GF_GROOVE_PUMP] = true;
    preset.patch.groove[GF_GROOVE_PUMP][GF_PUMP_DEPTH] = 100.0f;
    preset.patch.groove[GF_GROOVE_PUMP][GF_PUMP_SHAPE] = 2.0f;
    studio::preset_encode(preset, 2, encoded.data(), encoded.size());
    auto* a_import = dynamic_cast<gf::PresetImport*>(on_beat.get());
    auto* b_import = dynamic_cast<gf::PresetImport*>(off_beat.get());
    if (!check(a_import && b_import && a_import->import_preset(encoded.data(), encoded.size()) &&
               b_import->import_preset(encoded.data(), encoded.size()), "pump preset import")) return 1;
    juce::MidiBuffer beat_midi, later_midi;
    beat_midi.addEvent(juce::MidiMessage::noteOn(1, 60, uint8_t(100)), 0);
    later_midi.addEvent(juce::MidiMessage::noteOn(1, 60, uint8_t(100)), 0);
    on_beat->processBlock(original_audio, beat_midi);
    off_beat->processBlock(restored_audio, later_midi);
    float beat_difference = 0.0f;
    for (int i = 0; i < 128; ++i)
        beat_difference += std::abs(original_audio.getSample(0, i) - restored_audio.getSample(0, i));
    if (!check(beat_difference > 0.001f, "PUMP follows host beat position")) return 1;
    preset.patch.groove_on[GF_GROOVE_PUMP] = false;
    preset.patch.sfx_on[GF_SFX_TAPE] = true;
    preset.patch.sfx_p[GF_SFX_TAPE][1] = 0.8f;
    preset.patch.sfx_p[GF_SFX_TAPE][2] = 0.7f;
    preset.patch.sfx_on[GF_SFX_DELAY] = true;
    preset.patch.sfx_p[GF_SFX_DELAY][0] = 0.0f;
    preset.patch.sfx_p[GF_SFX_DELAY][1] = 0.4f;
    preset.patch.sfx_mix[GF_SFX_DELAY] = 0.8f;
    studio::preset_encode(preset, 2, encoded.data(), encoded.size());
    auto tape_big = std::unique_ptr<juce::AudioProcessor>(createPluginFilter());
    auto tape_small = std::unique_ptr<juce::AudioProcessor>(createPluginFilter());
    tape_big->prepareToPlay(48000.0, 128); tape_small->prepareToPlay(48000.0, 32);
    auto* big_import = dynamic_cast<gf::PresetImport*>(tape_big.get());
    auto* small_import = dynamic_cast<gf::PresetImport*>(tape_small.get());
    if (!check(big_import && small_import && big_import->import_preset(encoded.data(), encoded.size()) &&
               small_import->import_preset(encoded.data(), encoded.size()), "TAPE preset import")) return 1;
    for (int block = 0; block < 64; ++block) {
        juce::AudioBuffer<float> full(2, 128);
        juce::MidiBuffer events;
        if (block == 0) events.addEvent(juce::MidiMessage::noteOn(1, 60, uint8_t(100)), 0);
        tape_big->processBlock(full, events);
        for (int part = 0; part < 4; ++part) {
            juce::AudioBuffer<float> chunk(2, 32);
            juce::MidiBuffer part_events;
            if (block == 0 && part == 0) part_events.addEvent(juce::MidiMessage::noteOn(1, 60, uint8_t(100)), 0);
            tape_small->processBlock(chunk, part_events);
            for (int i = 0; i < 32; ++i)
                if (!check(std::fabs(full.getSample(0, part * 32 + i) - chunk.getSample(0, i)) < 1.0e-7f,
                           "TAPE and DELAY independent of block size")) return 1;
        }
    }
    preset.patch.sfx_on[GF_SFX_CHORUS] = true;
    preset.patch.sfx_p[GF_SFX_CHORUS][0] = 0.8f;
    preset.patch.sfx_p[GF_SFX_CHORUS][2] = 1.0f;
    preset.patch.sfx_mix[GF_SFX_CHORUS] = 1.0f;
    studio::preset_encode(preset, 2, encoded.data(), encoded.size());
    auto phaser_plugin = std::unique_ptr<juce::AudioProcessor>(createPluginFilter());
    phaser_plugin->prepareToPlay(48000.0, 128);
    auto* phaser_import = dynamic_cast<gf::PresetImport*>(phaser_plugin.get());
    if (!check(phaser_import && phaser_import->import_preset(encoded.data(), encoded.size()), "PHASER preset import")) return 1;
    juce::MidiBuffer phaser_note;
    phaser_note.addEvent(juce::MidiMessage::noteOn(1, 60, uint8_t(100)), 0);
    juce::AudioBuffer<float> phaser_audio(2, 128);
    phaser_plugin->processBlock(phaser_audio, phaser_note);
    float phaser_stereo = 0.0f;
    for (int i = 0; i < 128; ++i)
        phaser_stereo += std::fabs(phaser_audio.getSample(0, i) - phaser_audio.getSample(1, i));
    if (!check(phaser_stereo > 0.0001f, "GFP enables stereo PHASER")) return 1;
    preset.patch.sfx_on[GF_SFX_CHORUS] = false;
    preset.patch.sfx_on[GF_SFX_DELAY] = false;
    preset.patch.sfx_on[GF_SFX_REVERB] = true;
    preset.patch.sfx_p[GF_SFX_REVERB][1] = 1.0f;
    preset.patch.sfx_mix[GF_SFX_REVERB] = 0.8f;
    studio::preset_encode(preset, 2, encoded.data(), encoded.size());
    auto plate_plugin = std::unique_ptr<juce::AudioProcessor>(createPluginFilter());
    plate_plugin->prepareToPlay(48000.0, 128);
    auto* plate_import = dynamic_cast<gf::PresetImport*>(plate_plugin.get());
    if (!check(plate_import && plate_import->import_preset(encoded.data(), encoded.size()), "PLATE preset import")) return 1;
    double plate_stereo = 0.0;
    for (int block = 0; block < 60; ++block) {
        juce::AudioBuffer<float> audio(2, 128);
        juce::MidiBuffer events;
        if (block == 0) events.addEvent(juce::MidiMessage::noteOn(1, 60, uint8_t(100)), 0);
        plate_plugin->processBlock(audio, events);
        for (int i = 0; i < 128; ++i)
            plate_stereo += std::fabs(audio.getSample(0, i) - audio.getSample(1, i));
    }
    if (!check(plate_stereo > 0.001, "GFP PLATE produces stereo in plugin")) return 1;
    auto dx7_plugin = std::unique_ptr<juce::AudioProcessor>(createPluginFilter());
    dx7_plugin->prepareToPlay(48000.0, 128);
    dx7_plugin->getParameters()[5]->setValueNotifyingHost(1.0f);
    juce::AudioBuffer<float> dx7_audio(2, 128);
    juce::MidiBuffer dx7_note;
    dx7_note.addEvent(juce::MidiMessage::noteOn(1, 60, uint8_t(100)), 0);
    dx7_plugin->processBlock(dx7_audio, dx7_note);
    if (!check(dx7_audio.getRMSLevel(0, 0, 128) > 0.00001f, "DX7 MIDI produces audio")) return 1;
    juce::MemoryBlock dx7_state;
    dx7_plugin->getStateInformation(dx7_state);
    auto dx7_restored = std::unique_ptr<juce::AudioProcessor>(createPluginFilter());
    dx7_restored->setStateInformation(dx7_state.getData(), int(dx7_state.getSize()));
    if (!check(dx7_restored->getParameters()[5]->getValue() > 0.5f, "DX7 selector state")) return 1;
    studio::Preset dx7_preset;
    if (!check(studio::factory_preset(4, 0, &dx7_preset), "DX7 factory preset")) return 1;
    dx7_preset.patch.osc[0][2] = 8.0f;
    dx7_preset.patch.osc[3][1] = 99.0f;
    dx7_preset.patch.groove[GF_GROOVE_MACRO][GF_MACRO_GRIT] = 60.0f;
    dx7_preset.patch.groove[GF_GROOVE_MACRO][GF_MACRO_GRIT_MOD] = 100.0f;
    dx7_preset.patch.groove[GF_GROOVE_MACRO][GF_MACRO_POCKET] = 60.0f;
    dx7_preset.patch.groove[GF_GROOVE_MACRO][GF_MACRO_MOVE] = 30.0f;
    if (!check(studio::preset_encode(dx7_preset, 4, encoded.data(), encoded.size()) == encoded.size(), "DX7 GFP encode")) return 1;
    auto* dx7_import = dynamic_cast<gf::PresetImport*>(dx7_plugin.get());
    if (!check(dx7_import && dx7_import->import_preset(encoded.data(), encoded.size()), "DX7 GFP import")) return 1;
    dx7_plugin->getStateInformation(dx7_state);
    auto imported_restore = std::unique_ptr<juce::AudioProcessor>(createPluginFilter());
    imported_restore->setStateInformation(dx7_state.getData(), int(dx7_state.getSize()));
    dx7_plugin->prepareToPlay(48000.0, 128); imported_restore->prepareToPlay(48000.0, 128);
    juce::AudioBuffer<float> dx7_original(2, 128), dx7_copy(2, 128);
    juce::MidiBuffer midi_original, midi_copy;
    midi_original.addEvent(juce::MidiMessage::noteOn(1, 60, uint8_t(100)), 0);
    midi_copy.addEvent(juce::MidiMessage::noteOn(1, 60, uint8_t(100)), 0);
    dx7_plugin->processBlock(dx7_original, midi_original);
    imported_restore->processBlock(dx7_copy, midi_copy);
    for (int i = 0; i < 128; ++i)
        if (!check(std::fabs(dx7_original.getSample(0, i) - dx7_copy.getSample(0, i)) < 1.0e-7f,
                   "DX7 GFP state restores audio")) return 1;
    float dx7_patch_difference = 0.0f;
    for (int i = 0; i < 128; ++i) dx7_patch_difference += std::fabs(dx7_original.getSample(0, i) - dx7_audio.getSample(0, i));
    if (!check(dx7_patch_difference > 0.001f, "DX7 GFP changes audio")) return 1;
    juce::RangedAudioParameter* ratio_parameter = nullptr;
    for (auto* parameter : dx7_plugin->getParameters()) {
        auto* ranged = dynamic_cast<juce::RangedAudioParameter*>(parameter);
        if (ranged && ranged->paramID == "dx7.op4.ratio") ratio_parameter = ranged;
    }
    if (!check(ratio_parameter != nullptr, "DX7 operator automation exists")) return 1;
    ratio_parameter->setValueNotifyingHost(ratio_parameter->convertTo0to1(3.5f));
    dx7_plugin->getStateInformation(dx7_state);
    auto automated_restore = std::unique_ptr<juce::AudioProcessor>(createPluginFilter());
    automated_restore->setStateInformation(dx7_state.getData(), int(dx7_state.getSize()));
    dx7_plugin->prepareToPlay(48000.0, 128); automated_restore->prepareToPlay(48000.0, 128);
    juce::AudioBuffer<float> automated_audio(2, 128), automated_copy(2, 128);
    juce::MidiBuffer automated_note, automated_copy_note;
    automated_note.addEvent(juce::MidiMessage::noteOn(1, 60, uint8_t(100)), 0);
    automated_copy_note.addEvent(juce::MidiMessage::noteOn(1, 60, uint8_t(100)), 0);
    dx7_plugin->processBlock(automated_audio, automated_note);
    automated_restore->processBlock(automated_copy, automated_copy_note);
    float automation_difference = 0.0f;
    for (int i = 0; i < 128; ++i) {
        if (!check(std::fabs(automated_audio.getSample(0, i) - automated_copy.getSample(0, i)) < 1.0e-7f,
                   "DX7 automation state restores audio")) return 1;
        automation_difference += std::fabs(automated_audio.getSample(0, i) - dx7_original.getSample(0, i));
    }
    if (!check(automation_difference > 0.001f, "DX7 operator automation changes audio")) return 1;
    juce::RangedAudioParameter* dx_cutoff = nullptr;
    for (auto* parameter : dx7_plugin->getParameters()) {
        auto* ranged = dynamic_cast<juce::RangedAudioParameter*>(parameter);
        if (ranged && ranged->paramID == "dx7.cutoff") dx_cutoff = ranged;
    }
    if (!check(dx_cutoff != nullptr, "DX7 cutoff automation exists")) return 1;
    dx_cutoff->setValueNotifyingHost(dx_cutoff->convertTo0to1(500.0f));
    dx7_plugin->getStateInformation(dx7_state);
    auto voice_restore = std::unique_ptr<juce::AudioProcessor>(createPluginFilter());
    voice_restore->setStateInformation(dx7_state.getData(), int(dx7_state.getSize()));
    dx7_plugin->prepareToPlay(48000.0, 128); voice_restore->prepareToPlay(48000.0, 128);
    juce::AudioBuffer<float> voice_audio(2, 128), voice_copy(2, 128);
    juce::MidiBuffer voice_note, voice_copy_note;
    voice_note.addEvent(juce::MidiMessage::noteOn(1, 60, uint8_t(100)), 0);
    voice_copy_note.addEvent(juce::MidiMessage::noteOn(1, 60, uint8_t(100)), 0);
    dx7_plugin->processBlock(voice_audio, voice_note);
    voice_restore->processBlock(voice_copy, voice_copy_note);
    float voice_difference = 0.0f;
    for (int i = 0; i < 128; ++i) {
        if (!check(std::fabs(voice_audio.getSample(0, i) - voice_copy.getSample(0, i)) < 1.0e-7f,
                   "DX7 voice automation state restores audio")) return 1;
        voice_difference += std::fabs(voice_audio.getSample(0, i) - automated_audio.getSample(0, i));
    }
    if (!check(voice_difference > 0.001f, "DX7 cutoff automation changes audio")) return 1;
    juce::RangedAudioParameter* dx_unison_on = nullptr;
    juce::RangedAudioParameter* dx_unison_count = nullptr;
    for (auto* parameter : dx7_plugin->getParameters()) {
        auto* ranged = dynamic_cast<juce::RangedAudioParameter*>(parameter);
        if (ranged && ranged->paramID == "dx7.unison_on") dx_unison_on = ranged;
        if (ranged && ranged->paramID == "dx7.unison_count") dx_unison_count = ranged;
    }
    if (!check(dx_unison_on && dx_unison_count, "DX7 unison automation exists")) return 1;
    dx_unison_on->setValueNotifyingHost(dx_unison_on->convertTo0to1(1.0f));
    dx_unison_count->setValueNotifyingHost(dx_unison_count->convertTo0to1(1.0f));
    dx7_plugin->getStateInformation(dx7_state);
    auto unison_restore = std::unique_ptr<juce::AudioProcessor>(createPluginFilter());
    unison_restore->setStateInformation(dx7_state.getData(), int(dx7_state.getSize()));
    dx7_plugin->prepareToPlay(48000.0, 128); unison_restore->prepareToPlay(48000.0, 128);
    juce::AudioBuffer<float> uni_audio(2, 128), uni_copy(2, 128);
    juce::MidiBuffer uni_note, uni_copy_note;
    uni_note.addEvent(juce::MidiMessage::noteOn(1, 60, uint8_t(100)), 0);
    uni_copy_note.addEvent(juce::MidiMessage::noteOn(1, 60, uint8_t(100)), 0);
    dx7_plugin->processBlock(uni_audio, uni_note); unison_restore->processBlock(uni_copy, uni_copy_note);
    float uni_difference = 0.0f;
    for (int i = 0; i < 128; ++i) {
        if (!check(std::fabs(uni_audio.getSample(0, i) - uni_copy.getSample(0, i)) < 1.0e-7f,
                   "DX7 unison state restores audio")) return 1;
        uni_difference += std::fabs(uni_audio.getSample(0, i) - voice_audio.getSample(0, i));
    }
    if (!check(uni_difference > 0.001f, "DX7 unison automation changes audio")) return 1;
    std::vector<uint8_t> exported_dx7;
    auto* dx7_export = dynamic_cast<gf::PresetImport*>(dx7_plugin.get());
    if (!check(dx7_export && dx7_export->export_dx7_preset(exported_dx7), "DX7 GFP export")) return 1;
    auto exported_plugin = std::unique_ptr<juce::AudioProcessor>(createPluginFilter());
    exported_plugin->prepareToPlay(48000.0, 128);
    auto* exported_import = dynamic_cast<gf::PresetImport*>(exported_plugin.get());
    if (!check(exported_import && exported_import->import_preset(exported_dx7.data(), exported_dx7.size()), "exported DX7 GFP import")) return 1;
    juce::AudioBuffer<float> export_audio(2, 128);
    juce::MidiBuffer export_note;
    export_note.addEvent(juce::MidiMessage::noteOn(1, 60, uint8_t(100)), 0);
    exported_plugin->processBlock(export_audio, export_note);
    for (int i = 0; i < 128; ++i)
        if (!check(std::fabs(export_audio.getSample(0, i) - uni_audio.getSample(0, i)) < 1.0e-6f,
                   "exported DX7 GFP restores audio")) return 1;
    juce::RangedAudioParameter *dx_grit = nullptr, *dx_move = nullptr, *dx_pocket = nullptr;
    for (auto* parameter : dx7_plugin->getParameters()) {
        auto* ranged = dynamic_cast<juce::RangedAudioParameter*>(parameter);
        if (ranged && ranged->paramID == "dx7.macro.grit") dx_grit = ranged;
        if (ranged && ranged->paramID == "dx7.macro.move") dx_move = ranged;
        if (ranged && ranged->paramID == "dx7.macro.pocket") dx_pocket = ranged;
    }
    if (!check(dx_grit && dx_move && dx_pocket, "DX7 macro automation exists")) return 1;
    dx_grit->setValueNotifyingHost(0.6f); dx_move->setValueNotifyingHost(0.4f); dx_pocket->setValueNotifyingHost(0.5f);
    dx7_plugin->prepareToPlay(48000.0, 128);
    juce::AudioBuffer<float> macro_audio(2, 128), macro_copy(2, 128);
    juce::MidiBuffer macro_note, macro_copy_note;
    macro_note.addEvent(juce::MidiMessage::noteOn(1, 60, uint8_t(100)), 0);
    macro_copy_note.addEvent(juce::MidiMessage::noteOn(1, 60, uint8_t(100)), 0);
    dx7_plugin->processBlock(macro_audio, macro_note);
    if (!check(dx7_export->export_dx7_preset(exported_dx7), "DX7 macro GFP export")) return 1;
    auto macro_restore = std::unique_ptr<juce::AudioProcessor>(createPluginFilter());
    macro_restore->prepareToPlay(48000.0, 128);
    auto* macro_import = dynamic_cast<gf::PresetImport*>(macro_restore.get());
    if (!check(macro_import && macro_import->import_preset(exported_dx7.data(), exported_dx7.size()), "DX7 macro GFP reimport")) return 1;
    macro_restore->processBlock(macro_copy, macro_copy_note);
    float macro_difference = 0.0f;
    for (int i = 0; i < 128; ++i) {
        if (!check(std::fabs(macro_audio.getSample(0, i) - macro_copy.getSample(0, i)) < 1.0e-6f,
                   "DX7 macro export restores audio")) return 1;
        macro_difference += std::fabs(macro_audio.getSample(0, i) - uni_audio.getSample(0, i));
    }
    if (!check(macro_difference > 0.001f, "DX7 macro changes audio")) return 1;
    dx7_plugin->getParameters()[4]->setValueNotifyingHost(1.0f);
    if (!check(!dx7_export->export_dx7_preset(exported_dx7), "DX7 export rejects LFO sync")) return 1;
    dx7_plugin->getParameters()[4]->setValueNotifyingHost(0.0f);
    auto legacy_xml = juce::AudioProcessor::getXmlFromBinary(dx7_state.getData(), int(dx7_state.getSize()));
    if (!check(legacy_xml != nullptr, "DX7 state XML")) return 1;
    legacy_xml->removeAttribute("dx7ParamVersion");
    juce::MemoryBlock legacy_state;
    juce::AudioProcessor::copyXmlToBinary(*legacy_xml, legacy_state);
    auto legacy_restore = std::unique_ptr<juce::AudioProcessor>(createPluginFilter());
    legacy_restore->setStateInformation(legacy_state.getData(), int(legacy_state.getSize()));
    legacy_restore->prepareToPlay(48000.0, 128);
    juce::AudioBuffer<float> legacy_audio(2, 128);
    juce::MidiBuffer legacy_note;
    legacy_note.addEvent(juce::MidiMessage::noteOn(1, 60, uint8_t(100)), 0);
    legacy_restore->processBlock(legacy_audio, legacy_note);
    for (int i = 0; i < 128; ++i)
        if (!check(std::fabs(legacy_audio.getSample(0, i) - dx7_original.getSample(0, i)) < 1.0e-7f,
                   "legacy DX7 state restores GFP operators")) return 1;
    legacy_xml->setAttribute("dx7ParamVersion", 1);
    juce::AudioProcessor::copyXmlToBinary(*legacy_xml, legacy_state);
    auto v1_restore = std::unique_ptr<juce::AudioProcessor>(createPluginFilter());
    v1_restore->setStateInformation(legacy_state.getData(), int(legacy_state.getSize()));
    v1_restore->prepareToPlay(48000.0, 128);
    juce::AudioBuffer<float> v1_audio(2, 128);
    juce::MidiBuffer v1_note;
    v1_note.addEvent(juce::MidiMessage::noteOn(1, 60, uint8_t(100)), 0);
    v1_restore->processBlock(v1_audio, v1_note);
    for (int i = 0; i < 128; ++i)
        if (!check(std::fabs(v1_audio.getSample(0, i) - automated_audio.getSample(0, i)) < 1.0e-7f,
                   "DX7 v1 state preserves edited operator and restores voice")) return 1;
    dx7_preset.patch.sfx_on[GF_SFX_DRIVE] = true;
    dx7_preset.patch.sfx_p[GF_SFX_DRIVE][0] = 1.0f;
    dx7_preset.patch.sfx_mix[GF_SFX_DRIVE] = 1.0f;
    studio::preset_encode(dx7_preset, 4, encoded.data(), encoded.size());
    auto dx7_effect = std::unique_ptr<juce::AudioProcessor>(createPluginFilter());
    dx7_effect->prepareToPlay(48000.0, 128);
    auto* effect_import = dynamic_cast<gf::PresetImport*>(dx7_effect.get());
    if (!check(effect_import && effect_import->import_preset(encoded.data(), encoded.size()), "DX7 effect preset import")) return 1;
    juce::AudioBuffer<float> dx7_effect_audio(2, 128);
    juce::MidiBuffer dx7_effect_note;
    dx7_effect_note.addEvent(juce::MidiMessage::noteOn(1, 60, uint8_t(100)), 0);
    dx7_effect->processBlock(dx7_effect_audio, dx7_effect_note);
    float effect_difference = 0.0f;
    for (int i = 0; i < 128; ++i) effect_difference += std::fabs(dx7_effect_audio.getSample(0, i) - dx7_original.getSample(0, i));
    if (!check(effect_difference > 0.001f, "DX7 SYNTH FX changes audio")) return 1;
    juce::RangedAudioParameter* dx_drive_on = nullptr;
    for (auto* parameter : dx7_effect->getParameters()) {
        auto* ranged = dynamic_cast<juce::RangedAudioParameter*>(parameter);
        if (ranged && ranged->paramID == "dx7.fx.drive.on") dx_drive_on = ranged;
    }
    if (!check(dx_drive_on != nullptr, "DX7 FX automation exists")) return 1;
    dx_drive_on->setValueNotifyingHost(0.0f);
    juce::MemoryBlock fx_state;
    dx7_effect->getStateInformation(fx_state);
    auto fx_restore = std::unique_ptr<juce::AudioProcessor>(createPluginFilter());
    fx_restore->setStateInformation(fx_state.getData(), int(fx_state.getSize()));
    dx7_effect->prepareToPlay(48000.0, 128); fx_restore->prepareToPlay(48000.0, 128);
    juce::AudioBuffer<float> fx_off(2, 128), fx_copy(2, 128);
    juce::MidiBuffer fx_note, fx_copy_note;
    fx_note.addEvent(juce::MidiMessage::noteOn(1, 60, uint8_t(100)), 0);
    fx_copy_note.addEvent(juce::MidiMessage::noteOn(1, 60, uint8_t(100)), 0);
    dx7_effect->processBlock(fx_off, fx_note); fx_restore->processBlock(fx_copy, fx_copy_note);
    float fx_automation_difference = 0;
    for (int i = 0; i < 128; ++i) {
        if (!check(std::fabs(fx_off.getSample(0, i) - fx_copy.getSample(0, i)) < 1.0e-7f,
                   "DX7 FX state restores audio")) return 1;
        fx_automation_difference += std::fabs(fx_off.getSample(0, i) - dx7_effect_audio.getSample(0, i));
    }
    if (!check(fx_automation_difference > 0.001f, "DX7 FX automation changes audio")) return 1;
    std::vector<uint8_t> fx_export;
    if (!check(effect_import->export_dx7_preset(fx_export), "DX7 FX export")) return 1;
    studio::Preset fx_decoded;
    uint8_t fx_engine = 0;
    if (!check(gf::decode_supported_preset(fx_export.data(), fx_export.size(), fx_decoded, fx_engine) &&
               fx_engine == 4 && !fx_decoded.patch.sfx_on[GF_SFX_DRIVE], "DX7 FX export saves edit")) return 1;
    dx7_preset.patch.groove_on[GF_GROOVE_ARP] = true;
    for (auto& enabled : dx7_preset.patch.sfx_on) enabled = false;
    dx7_preset.patch.groove_on[GF_GROOVE_CHORD] = false;
    dx7_preset.patch.groove[GF_GROOVE_ARP][GF_ARP_GATE] = 60.0f;
    dx7_preset.patch.groove[GF_GROOVE_ARP][GF_ARP_OCT] = 1.0f;
    dx7_preset.patch.groove[GF_GROOVE_ARP][GF_ARP_SWING] = 50.0f;
    dx7_preset.patch.groove[GF_GROOVE_ARP][GF_ARP_LATCH] = 0.0f;
    studio::preset_encode(dx7_preset, 4, encoded.data(), encoded.size());
    auto dx7_arp = std::unique_ptr<juce::AudioProcessor>(createPluginFilter());
    dx7_arp->prepareToPlay(48000.0, 128);
    auto* arp_import = dynamic_cast<gf::PresetImport*>(dx7_arp.get());
    if (!check(arp_import && arp_import->import_preset(encoded.data(), encoded.size()), "DX7 ARP preset import")) return 1;
    float arp_peak = 0.0f;
    for (int block = 0; block < 200; ++block) {
        juce::AudioBuffer<float> audio(2, 128);
        juce::MidiBuffer events;
        if (block == 0) {
            events.addEvent(juce::MidiMessage::noteOn(1, 60, uint8_t(100)), 0);
            events.addEvent(juce::MidiMessage::noteOn(1, 64, uint8_t(100)), 0);
        }
        if (block == 40) {
            events.addEvent(juce::MidiMessage::controllerEvent(1, 64, 127), 0);
            events.addEvent(juce::MidiMessage::noteOff(1, 60), 1);
            events.addEvent(juce::MidiMessage::noteOff(1, 64), 1);
        }
        if (block == 100) events.addEvent(juce::MidiMessage::controllerEvent(1, 64, 0), 0);
        dx7_arp->processBlock(audio, events);
        if (block >= 50 && block < 100) arp_peak = std::max(arp_peak, audio.getRMSLevel(0, 0, 128));
    }
    if (!check(arp_peak > 0.00001f, "DX7 ARP sustain produces audio")) return 1;
    juce::MidiBuffer no_events;
    juce::AudioBuffer<float> arp_tail(2, 128);
    for (int block = 0; block < 1800; ++block) dx7_arp->processBlock(arp_tail, no_events);
    if (!check(arp_tail.getRMSLevel(0, 0, 128) < 0.000001f, "DX7 ARP releases notes")) return 1;
    juce::RangedAudioParameter* dx_arp_on = nullptr;
    for (auto* parameter : dx7_arp->getParameters()) {
        auto* ranged = dynamic_cast<juce::RangedAudioParameter*>(parameter);
        if (ranged && ranged->paramID == "dx7.groove.arp.on") dx_arp_on = ranged;
    }
    if (!check(dx_arp_on && dx_arp_on->getValue() > 0.5f, "DX7 GROOVE preset populates control")) return 1;
    dx_arp_on->setValueNotifyingHost(0.0f);
    juce::MidiBuffer direct_note;
    direct_note.addEvent(juce::MidiMessage::noteOn(1, 60, uint8_t(100)), 0);
    dx7_arp->processBlock(arp_tail, direct_note);
    if (!check(arp_tail.getRMSLevel(0, 0, 128) > 0.00001f, "DX7 GROOVE automation disables ARP")) return 1;
    std::vector<uint8_t> groove_export;
    if (!check(arp_import->export_dx7_preset(groove_export), "DX7 GROOVE export")) return 1;
    studio::Preset groove_decoded;
    uint8_t groove_engine = 0;
    if (!check(gf::decode_supported_preset(groove_export.data(), groove_export.size(), groove_decoded, groove_engine) &&
               groove_engine == 4 && !groove_decoded.patch.groove_on[GF_GROOVE_ARP], "DX7 GROOVE export saves automation")) return 1;
    auto cc_dx = std::unique_ptr<juce::AudioProcessor>(createPluginFilter());
    auto cc_plain = std::unique_ptr<juce::AudioProcessor>(createPluginFilter());
    cc_dx->prepareToPlay(48000.0, 128); cc_plain->prepareToPlay(48000.0, 128);
    cc_dx->getParameters()[5]->setValueNotifyingHost(1.0f); cc_plain->getParameters()[5]->setValueNotifyingHost(1.0f);
    juce::AudioBuffer<float> cc_audio(2, 128), cc_dry(2, 128);
    juce::MidiBuffer cc_events, cc_dry_events;
    cc_events.addEvent(juce::MidiMessage::noteOn(1, 60, uint8_t(100)), 0);
    cc_events.addEvent(juce::MidiMessage::controllerEvent(1, 15, 127), 64);
    cc_dry_events.addEvent(juce::MidiMessage::noteOn(1, 60, uint8_t(100)), 0);
    cc_dx->processBlock(cc_audio, cc_events); cc_plain->processBlock(cc_dry, cc_dry_events);
    float cc_difference = 0.0f;
    for (int i = 0; i < 128; ++i) {
        if (i < 64 && !check(std::fabs(cc_audio.getSample(0, i) - cc_dry.getSample(0, i)) < 1.0e-7f,
                             "DX7 macro CC respects MIDI offset")) return 1;
        if (i >= 64) cc_difference += std::fabs(cc_audio.getSample(0, i) - cc_dry.getSample(0, i));
    }
    if (!check(cc_difference > 0.001f, "DX7 macro CC changes audio")) return 1;
    juce::MemoryBlock cc_state;
    cc_dx->getStateInformation(cc_state);
    auto cc_restore = std::unique_ptr<juce::AudioProcessor>(createPluginFilter());
    cc_restore->setStateInformation(cc_state.getData(), int(cc_state.getSize()));
    juce::RangedAudioParameter* restored_grit = nullptr;
    for (auto* parameter : cc_restore->getParameters()) {
        auto* ranged = dynamic_cast<juce::RangedAudioParameter*>(parameter);
        if (ranged && ranged->paramID == "dx7.macro.grit") restored_grit = ranged;
    }
    if (!check(restored_grit && restored_grit->getValue() > 0.99f, "pending DX7 MIDI macro survives state save")) return 1;
    std::vector<uint8_t> cc_exported;
    auto* cc_export = dynamic_cast<gf::PresetImport*>(cc_dx.get());
    if (!check(cc_export && cc_export->export_dx7_preset(cc_exported), "pending MIDI macro GFP export")) return 1;
    studio::Preset cc_decoded;
    uint8_t cc_engine = 0;
    if (!check(gf::decode_supported_preset(cc_exported.data(), cc_exported.size(), cc_decoded, cc_engine) &&
               cc_engine == 4 && cc_decoded.patch.osc[0][3] == 5.0f && cc_decoded.patch.osc[1][1] == 92.0f,
               "pending MIDI macro is baked into GFP")) return 1;
    juce::MidiBuffer more_cc;
    more_cc.addEvent(juce::MidiMessage::controllerEvent(1, 14, 32), 0);
    more_cc.addEvent(juce::MidiMessage::controllerEvent(1, 16, 64), 1);
    more_cc.addEvent(juce::MidiMessage::controllerEvent(1, 19, 96), 2);
    cc_dx->processBlock(cc_audio, more_cc);
    cc_dx->getStateInformation(cc_state);
    cc_restore->setStateInformation(cc_state.getData(), int(cc_state.getSize()));
    constexpr const char* cc_ids[] = {"dx7.drift", "dx7.macro.pocket", "dx7.macro.move"};
    constexpr float cc_expected[] = {32.0f / 127.0f, 64.0f / 127.0f, 96.0f / 127.0f};
    for (int i = 0; i < 3; ++i) {
        juce::RangedAudioParameter* found = nullptr;
        for (auto* parameter : cc_restore->getParameters()) {
            auto* ranged = dynamic_cast<juce::RangedAudioParameter*>(parameter);
            if (ranged && ranged->paramID == cc_ids[i]) found = ranged;
        }
        if (!check(found && std::fabs(found->getValue() - cc_expected[i]) < 1.0e-5f,
                   "DX7 MIDI CC value survives state save")) return 1;
    }
    auto cc_juno = std::unique_ptr<juce::AudioProcessor>(createPluginFilter());
    auto cc_juno_plain = std::unique_ptr<juce::AudioProcessor>(createPluginFilter());
    find_parameter(*cc_juno, "juno_preview")->setValueNotifyingHost(1);
    find_parameter(*cc_juno_plain, "juno_preview")->setValueNotifyingHost(1);
    cc_juno->prepareToPlay(48000, 128); cc_juno_plain->prepareToPlay(48000, 128);
    juce::AudioBuffer<float> cc_juno_audio(2, 128), cc_juno_dry(2, 128);
    juce::MidiBuffer juno_cc_events, juno_cc_plain_events;
    juno_cc_events.addEvent(juce::MidiMessage::noteOn(1, 60, uint8_t(100)), 0);
    juno_cc_events.addEvent(juce::MidiMessage::controllerEvent(1, 15, 127), 64);
    juno_cc_plain_events.addEvent(juce::MidiMessage::noteOn(1, 60, uint8_t(100)), 0);
    cc_juno->processBlock(cc_juno_audio, juno_cc_events); cc_juno_plain->processBlock(cc_juno_dry, juno_cc_plain_events);
    float juno_cc_difference = 0;
    for (int i = 0; i < 128; ++i) {
        const float delta = std::abs(cc_juno_audio.getSample(0, i) - cc_juno_dry.getSample(0, i));
        if (i < 64 && !check(delta < 1e-7f, "Juno MIDI macro respects offset")) return 1;
        if (i >= 64) juno_cc_difference += delta;
    }
    if (!check(juno_cc_difference > .0001f, "Juno MIDI GRIT changes audio")) return 1;
    juce::MidiBuffer juno_more_cc;
    juno_more_cc.addEvent(juce::MidiMessage::controllerEvent(1, 14, 32), 0);
    juno_more_cc.addEvent(juce::MidiMessage::controllerEvent(1, 16, 64), 1);
    juno_more_cc.addEvent(juce::MidiMessage::controllerEvent(1, 19, 96), 2);
    cc_juno->processBlock(cc_juno_audio, juno_more_cc);
    juce::MemoryBlock juno_cc_state;
    cc_juno->getStateInformation(juno_cc_state);
    auto juno_cc_restore = std::unique_ptr<juce::AudioProcessor>(createPluginFilter());
    juno_cc_restore->setStateInformation(juno_cc_state.getData(), int(juno_cc_state.getSize()));
    constexpr const char* juno_cc_ids[] = {"juno.drift", "juno.macro.grit", "juno.macro.pocket", "juno.macro.move"};
    constexpr float juno_cc_expected[] = {32.0f / 127.0f, 1, 64.0f / 127.0f, 96.0f / 127.0f};
    for (int i = 0; i < 4; ++i) {
        auto* p = find_parameter(*juno_cc_restore, juno_cc_ids[i]);
        if (!check(p && std::abs(p->getValue() - juno_cc_expected[i]) < .00001f, "Juno pending MIDI CC state restore")) return 1;
    }
    auto* cc_juno_import = dynamic_cast<gf::PresetImport*>(cc_juno.get());
    std::vector<uint8_t> juno_cc_export;
    if (!check(cc_juno_import && cc_juno_import->export_juno_preset(juno_cc_export), "Juno pending MIDI GFP export")) return 1;
    studio::Preset juno_cc_patch; uint8_t juno_cc_engine = 0;
    if (!check(gf::decode_supported_preset(juno_cc_export.data(), juno_cc_export.size(), juno_cc_patch, juno_cc_engine) &&
               juno_cc_engine == 1 && std::abs(juno_cc_patch.patch.groove[GF_GROOVE_MACRO][GF_MACRO_DRIFT] - 100.0f * 32 / 127) < .001f &&
               juno_cc_patch.patch.drive > .7f && juno_cc_patch.cutoff_hz > 2000, "Juno pending CC exported as effective sound")) return 1;
    studio::Preset reset_juno;
    if (!check(studio::factory_preset(1, 0, &reset_juno), "Juno reset preset")) return 1;
    std::array<uint8_t, studio::PRESET_BYTES> reset_juno_bytes{};
    studio::preset_encode(reset_juno, 1, reset_juno_bytes.data(), reset_juno_bytes.size());
    if (!check(cc_juno_import->import_preset(reset_juno_bytes.data(), reset_juno_bytes.size()), "Juno reset CC with preset")) return 1;
    cc_juno->getStateInformation(juno_cc_state);
    juno_cc_restore->setStateInformation(juno_cc_state.getData(), int(juno_cc_state.getSize()));
    if (!check(find_parameter(*juno_cc_restore, "juno.macro.grit")->getValue() < .001f, "Juno preset clears pending macro CC")) return 1;
    {
        auto editor = std::unique_ptr<juce::AudioProcessorEditor>(a->createEditor());
        if (!check(editor && editor->getWidth() == 720 && editor->getHeight() == 360, "editor creation")) return 1;
    }
    return 0;
}
