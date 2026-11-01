#include <juce_audio_utils/juce_audio_utils.h>
#include "preset_import.h"
#include "output_meter.h"
#include "studio_keyboard.h"
#include "preset_browser.h"
#include "envelope_editor.h"
#include "lfo_display.h"
#include "hybrid_codec.h"
#include "ui/layers.h"
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

static bool ob6_midi_block_independence(double sample_rate) {
    studio::Preset preset;
    if (!studio::factory_preset(3, 0, &preset)) return false;
    preset.patch.sfx_on[GF_SFX_CHORUS] = true;
    std::array<uint8_t, studio::PRESET_BYTES> bytes{};
    studio::preset_encode(preset, 3, bytes.data(), bytes.size());
    auto large = std::unique_ptr<juce::AudioProcessor>(createPluginFilter());
    auto small = std::unique_ptr<juce::AudioProcessor>(createPluginFilter());
    for (auto* p : {large.get(), small.get()}) {
        if (!dynamic_cast<gf::PresetImport*>(p)->import_preset(bytes.data(), bytes.size())) return false;
    }
    large->prepareToPlay(sample_rate, 128); small->prepareToPlay(sample_rate, 32);
    struct Event { int sample; juce::MidiMessage message; };
    const std::array<Event, 9> events{{
        {0, juce::MidiMessage::noteOn(1, 60, uint8_t(100))},
        {40, juce::MidiMessage::controllerEvent(1, 74, 92)},
        {80, juce::MidiMessage::controllerEvent(1, 71, 50)},
        {151, juce::MidiMessage::controllerEvent(1, 15, 88)},
        {200, juce::MidiMessage::channelPressureChange(1, 84)},
        {250, juce::MidiMessage::controllerEvent(1, 19, 65)},
        {480, juce::MidiMessage::noteOff(1, 60)},
        {690, juce::MidiMessage::noteOn(1, 67, uint8_t(100))},
        {810, juce::MidiMessage::noteOff(1, 67)}
    }};
    auto add_events = [&](juce::MidiBuffer& midi, int start, int count) {
        for (const auto& event : events)
            if (event.sample >= start && event.sample < start + count) midi.addEvent(event.message, event.sample - start);
    };
    double energy = 0;
    for (int start = 0; start < 2048; start += 128) {
        juce::AudioBuffer<float> full(2, 128);
        juce::MidiBuffer midi;
        add_events(midi, start, 128);
        large->processBlock(full, midi);
        for (int offset = 0; offset < 128; offset += 32) {
            juce::AudioBuffer<float> part(2, 32);
            juce::MidiBuffer part_midi;
            add_events(part_midi, start + offset, 32);
            small->processBlock(part, part_midi);
            for (int i = 0; i < 32; ++i) for (int ch = 0; ch < 2; ++ch) {
                const float a = full.getSample(ch, offset + i), b = part.getSample(ch, i);
                energy += a * a;
                if (!std::isfinite(a + b) || std::abs(a - b) > 1e-7f) {
                    std::fprintf(stderr, "OB-6 block mismatch at %.0f Hz, sample %d, channel %d: %.9g vs %.9g\n",
                                 sample_rate, start + offset + i, ch, a, b);
                    return false;
                }
            }
        }
    }
    return energy > .001;
}

static bool hybrid_midi_block_independence(double sample_rate) {
    studio::LayerPreset layer;
    if (!studio::factory_layer(0, &layer)) return false;
    layer.delay_b_ms = 7; layer.transpose_b = 12;
    std::array<uint8_t, studio::LAYER_BYTES> bytes{};
    if (!gf::encode_hybrid_layer(layer, bytes.data(), bytes.size())) return false;
    auto large = std::unique_ptr<juce::AudioProcessor>(createPluginFilter());
    auto small = std::unique_ptr<juce::AudioProcessor>(createPluginFilter());
    for (auto* p : {large.get(), small.get()})
        if (!dynamic_cast<gf::PresetImport*>(p)->import_preset(bytes.data(), bytes.size())) return false;
    large->prepareToPlay(sample_rate, 128); small->prepareToPlay(sample_rate, 32);
    struct Event { int sample; juce::MidiMessage message; };
    const std::array<Event, 7> events{{
        {0, juce::MidiMessage::noteOn(1, 60, uint8_t(100))},
        {41, juce::MidiMessage::controllerEvent(1, 17, 95)},
        {95, juce::MidiMessage::controllerEvent(1, 74, 70)},
        {172, juce::MidiMessage::controllerEvent(1, 71, 70)},
        {410, juce::MidiMessage::noteOff(1, 60)},
        {611, juce::MidiMessage::noteOn(1, 67, uint8_t(100))},
        {900, juce::MidiMessage::noteOff(1, 67)}
    }};
    auto add = [&](juce::MidiBuffer& midi, int start, int count) {
        for (const auto& event : events) if (event.sample >= start && event.sample < start + count) midi.addEvent(event.message, event.sample - start);
    };
    double energy = 0;
    for (int start = 0; start < 2048; start += 128) {
        juce::AudioBuffer<float> full(2, 128); juce::MidiBuffer midi; add(midi, start, 128); large->processBlock(full, midi);
        for (int offset = 0; offset < 128; offset += 32) {
            juce::AudioBuffer<float> part(2, 32); juce::MidiBuffer part_midi; add(part_midi, start + offset, 32); small->processBlock(part, part_midi);
            for (int i = 0; i < 32; ++i) for (int ch = 0; ch < 2; ++ch) {
                const float a = full.getSample(ch, offset + i), b = part.getSample(ch, i); energy += a * a;
                if (!std::isfinite(a + b) || std::abs(a - b) > 1e-6f) {
                    std::fprintf(stderr, "HYBRID block mismatch at %.0f Hz, sample %d: %.9g %.9g\n", sample_rate, start + offset + i, a, b);
                    return false;
                }
            }
        }
    }
    return energy > .001;
}

int main() {
    juce::ScopedJuceInitialiser_GUI juce_gui;
    for (double fs : {44100.0, 48000.0, 96000.0})
        if (!check(hybrid_midi_block_independence(fs), "HYBRID MIDI block independence")) return 1;
    {
        studio::LayerPreset layer;
        if (!check(studio::factory_layer(0, &layer), "HYBRID factory layer")) return 1;
        layer.delay_b_ms = 24; layer.transpose_b = 7; layer.morph = 60;
        std::array<uint8_t, studio::LAYER_BYTES> bytes{};
        if (!check(gf::encode_hybrid_layer(layer, bytes.data(), bytes.size()), "HYBRID encode layer")) return 1;
        auto synth = std::unique_ptr<juce::AudioProcessor>(createPluginFilter());
        auto find = [](juce::AudioProcessor& p, const char* id) -> juce::RangedAudioParameter* {
            for (auto* item : p.getParameters())
                if (auto* param = dynamic_cast<juce::RangedAudioParameter*>(item); param && param->paramID == id) return param;
            return nullptr;
        };
        find(*synth, "juno_preview")->setValueNotifyingHost(1);
        auto* importer = dynamic_cast<gf::PresetImport*>(synth.get());
        if (!check(importer && importer->import_preset(bytes.data(), bytes.size()) && find(*synth, "hybrid_preview")->getValue() > .5f &&
                   std::abs(find(*synth, "hybrid.morph")->getValue() - .6f) < .001f, "HYBRID import GFL")) return 1;
        auto malformed = bytes; malformed[0] = 'X';
        if (!check(!importer->import_preset(malformed.data(), malformed.size()), "HYBRID invalid layer rejected")) return 1;
        juce::MemoryBlock state; synth->getStateInformation(state);
        auto restored = std::unique_ptr<juce::AudioProcessor>(createPluginFilter());
        restored->setStateInformation(state.getData(), int(state.getSize()));
        if (!check(find(*restored, "hybrid_preview")->getValue() > .5f &&
                   std::abs(find(*restored, "hybrid.morph")->getValue() - .6f) < .001f, "HYBRID project restores")) return 1;
        synth->prepareToPlay(48000, 128); restored->prepareToPlay(48000, 128);
        juce::AudioBuffer<float> audio(2, 128), other(2, 128);
        juce::MidiBuffer note, other_note;
        note.addEvent(juce::MidiMessage::noteOn(1, 60, uint8_t(100)), 40);
        other_note.addEvent(juce::MidiMessage::noteOn(1, 60, uint8_t(100)), 40);
        synth->processBlock(audio, note); restored->processBlock(other, other_note);
        for (int i = 0; i < 128; ++i) {
            if (i < 40 && !check(audio.getSample(0, i) == 0, "HYBRID MIDI offset")) return 1;
            if (!check(std::abs(audio.getSample(0, i) - other.getSample(0, i)) < 1e-6f, "HYBRID layer audio restores")) return 1;
        }
        if (!check(audio.getRMSLevel(0, 40, 88) > 0, "HYBRID GFL audible")) return 1;
        auto* held_cutoff = find(*synth, "hybrid.a.cutoff");
        held_cutoff->setValueNotifyingHost(held_cutoff->convertTo0to1(1200));
        juce::MidiBuffer held_no_events;
        synth->processBlock(audio, held_no_events);
        if (!check(audio.getRMSLevel(0, 0, 128) > 0, "HYBRID half edit preserves held note")) return 1;
        juce::MidiBuffer morph;
        morph.addEvent(juce::MidiMessage::controllerEvent(1, 17, 127), 64);
        synth->processBlock(audio, morph); synth->getStateInformation(state);
        auto cc_restore = std::unique_ptr<juce::AudioProcessor>(createPluginFilter());
        cc_restore->setStateInformation(state.getData(), int(state.getSize()));
        if (!check(find(*cc_restore, "hybrid.morph")->getValue() > .99f, "HYBRID pending CC17 restores")) return 1;
        find(*synth, "hybrid.morph")->setValueNotifyingHost(.25f);
        std::vector<uint8_t> manual_export; studio::LayerPreset manual_layer;
        if (!check(importer->export_hybrid_preset(manual_export) && gf::decode_hybrid_layer(manual_export.data(), manual_export.size(), manual_layer) &&
                   manual_layer.morph == 25, "HYBRID DAW morph overrides pending CC")) return 1;
        find(*synth, "hybrid.morph")->setValueNotifyingHost(1);
        auto editor = std::unique_ptr<juce::AudioProcessorEditor>(cc_restore->createEditor());
        auto* toggle = dynamic_cast<juce::ToggleButton*>(editor->findChildWithID("engine.hybrid"));
        auto* morph_slider = dynamic_cast<juce::Slider*>(editor->findChildWithID("hybrid.morph"));
        if (!check(toggle && toggle->getToggleState() && morph_slider && morph_slider->isVisible(), "HYBRID editor")) return 1;
        auto* card_a = dynamic_cast<juce::TextButton*>(editor->findChildWithID("hybrid.card.0"));
        auto* card_b = dynamic_cast<juce::TextButton*>(editor->findChildWithID("hybrid.card.1"));
        if (!check(card_a && card_b && card_a->isVisible() && card_b->isVisible(), "HYBRID layer cards")) return 1;
        card_b->onClick();
        if (!check(find(*cc_restore, "hybrid.focus")->getValue() > .99f && card_b->getButtonText().contains("FOCUS"), "HYBRID B card selects focus")) return 1;
        card_a->onClick();
        if (!check(find(*cc_restore, "hybrid.focus")->getValue() < .01f && card_a->getButtonText().contains("FOCUS"), "HYBRID A card selects focus")) return 1;
        auto* hybrid_page = dynamic_cast<juce::TextButton*>(editor->findChildWithID("hybrid.page"));
        auto* b_cutoff = dynamic_cast<juce::Slider*>(editor->findChildWithID("hybrid.b.cutoff"));
        if (!check(hybrid_page && b_cutoff && !b_cutoff->isVisible(), "HYBRID half controls")) return 1;
        auto* hybrid_filter_tab = dynamic_cast<juce::TextButton*>(editor->findChildWithID("navigation.tab.1"));
        auto* hybrid_home_tab = dynamic_cast<juce::TextButton*>(editor->findChildWithID("navigation.tab.0"));
        if (!check(hybrid_filter_tab && hybrid_home_tab && hybrid_filter_tab->isVisible(), "HYBRID direct navigation")) return 1;
        hybrid_filter_tab->onClick();
        if (!check(b_cutoff->isVisible(), "HYBRID filter tab opens controls")) return 1;
        auto* a_filter = dynamic_cast<juce::Slider*>(editor->findChildWithID("hybrid.a.cutoff"));
        auto* focus_selector = dynamic_cast<juce::ComboBox*>(editor->findChildWithID("hybrid.focus"));
        if (!check(a_filter && a_filter->isVisible() && focus_selector && focus_selector->isVisible(), "HYBRID comparison controls visible")) return 1;
        const double a_before_focus = a_filter->getValue(), b_before_focus = b_cutoff->getValue();
        focus_selector->setSelectedId(2, juce::sendNotificationSync);
        if (!check(find(*cc_restore, "hybrid.focus")->getValue() > .99f && a_filter->getValue() == a_before_focus && b_cutoff->getValue() == b_before_focus, "HYBRID filter focus preserves both layers")) return 1;
        focus_selector->setSelectedId(1, juce::sendNotificationSync);
        hybrid_home_tab->onClick();
        if (!check(morph_slider->isVisible() && !b_cutoff->isVisible(), "HYBRID home tab returns to layer")) return 1;
        hybrid_page->onClick();
        if (!check(b_cutoff->isVisible() && !morph_slider->isVisible(), "HYBRID half page")) return 1;
        b_cutoff->setValue(1400, juce::sendNotificationSync);
        std::vector<uint8_t> exported;
        studio::LayerPreset exported_layer;
        if (!check(dynamic_cast<gf::PresetImport*>(cc_restore.get())->export_hybrid_preset(exported) &&
                   gf::decode_hybrid_layer(exported.data(), exported.size(), exported_layer) &&
                   exported_layer.eng_a == layer.eng_a && exported_layer.eng_b == layer.eng_b &&
                   exported_layer.morph == 100 && exported_layer.delay_b_ms == 24 && exported_layer.transpose_b == 7 &&
                   std::abs(exported_layer.b.cutoff_hz - 1400) < .01f,
                   "HYBRID export layer controls")) return 1;
        studio::Preset new_half;
        std::array<uint8_t, studio::PRESET_BYTES> half_bytes{};
        if (!check(studio::factory_preset(1, 0, &new_half) && studio::preset_encode(new_half, 1, half_bytes.data(), half_bytes.size()) == half_bytes.size(), "HYBRID replacement GFP")) return 1;
        auto* a_cutoff = find(*synth, "hybrid.a.cutoff");
        a_cutoff->setValueNotifyingHost(a_cutoff->convertTo0to1(1234));
        auto* gain_a = find(*synth, "hybrid.gain_a");
        gain_a->setValueNotifyingHost(gain_a->convertTo0to1(-9));
        if (!check(importer->import_hybrid_half(half_bytes.data(), half_bytes.size(), 1), "HYBRID replace half B")) return 1;
        exported.clear();
        if (!check(importer->export_hybrid_preset(exported) && gf::decode_hybrid_layer(exported.data(), exported.size(), exported_layer) &&
                   exported_layer.eng_a == layer.eng_a && exported_layer.eng_b == 1 && exported_layer.gain_a_db == -9 &&
                   std::abs(exported_layer.a.cutoff_hz - 1234) < .1f &&
                   exported_layer.morph == 100 && exported_layer.delay_b_ms == 24 && exported_layer.transpose_b == 7,
                   "HYBRID replacement preserves controls")) return 1;
        if (!check(studio::factory_preset(exported_layer.eng_a, 0, &new_half) &&
                   studio::preset_encode(new_half, exported_layer.eng_a, half_bytes.data(), half_bytes.size()) == half_bytes.size() &&
                   !importer->import_hybrid_half(half_bytes.data(), half_bytes.size(), 1), "HYBRID reject duplicate engines")) return 1;
        juce::MidiBuffer bank_midi;
        bank_midi.addEvent(juce::MidiMessage::controllerEvent(1, 18, 127), 0);
        synth->processBlock(audio, bank_midi);
        juce::Thread::sleep(50); juce::Timer::callPendingTimersSynchronously();
        exported.clear();
        if (!check(importer->export_hybrid_preset(exported) && gf::decode_hybrid_layer(exported.data(), exported.size(), exported_layer) &&
                   std::strcmp(exported_layer.name, studio::layer_def(7).name) == 0, "HYBRID CC18 factory layer")) return 1;
        juce::MidiBuffer bank_off;
        bank_off.addEvent(juce::MidiMessage::controllerEvent(1, 18, 0), 0);
        synth->processBlock(audio, bank_off);
        for (int attempt = 0; attempt < 10 && (find(*synth, "hybrid_preview")->getValue() >= .5f || find(*synth, "juno_preview")->getValue() <= .5f); ++attempt) {
            juce::Thread::sleep(35); juce::Timer::callPendingTimersSynchronously();
        }
        if (!check(find(*synth, "hybrid_preview")->getValue() < .5f && find(*synth, "juno_preview")->getValue() > .5f,
                   "HYBRID CC18 restores SYNTH")) return 1;
        hybrid_page->onClick();
        auto* hybrid_grit = dynamic_cast<juce::Slider*>(editor->findChildWithID("hybrid.macro.grit"));
        if (!check(hybrid_grit && hybrid_grit->isVisible(), "HYBRID macro page")) return 1;
        hybrid_grit->setValue(.45, juce::sendNotificationSync);
        if (!check(std::abs(find(*cc_restore, "hybrid.macro.grit")->getValue() - .45f) < .001f, "HYBRID macro editor binding")) return 1;
        hybrid_page->onClick();
        auto* hybrid_groove = dynamic_cast<juce::Slider*>(editor->findChildWithID("shared.groove.0.0"));
        auto* hybrid_fx = dynamic_cast<juce::Slider*>(editor->findChildWithID("shared.fx.0.0"));
        if (!check(hybrid_groove && hybrid_fx && hybrid_groove->isVisible(), "HYBRID groove page")) return 1;
        hybrid_groove->setValue(1, juce::sendNotificationSync);
        if (!check(find(*cc_restore, "hybrid.groove.pump.on")->getValue() > .5f &&
                   find(*cc_restore, "arp2600.groove.pump.on")->getValue() < .5f, "HYBRID groove isolation")) return 1;
        hybrid_page->onClick();
        if (!check(hybrid_fx->isVisible() && !hybrid_groove->isVisible(), "HYBRID FX page")) return 1;
        hybrid_fx->setValue(1, juce::sendNotificationSync);
        if (!check(find(*cc_restore, "hybrid.fx.drive.on")->getValue() > .5f &&
                   find(*cc_restore, "arp2600.fx.drive.on")->getValue() < .5f, "HYBRID FX isolation")) return 1;
        std::vector<uint8_t> bus_gfl; studio::LayerPreset bus_layer;
        if (!check(dynamic_cast<gf::PresetImport*>(cc_restore.get())->export_hybrid_preset(bus_gfl) &&
                   gf::decode_hybrid_layer(bus_gfl.data(), bus_gfl.size(), bus_layer) &&
                   bus_layer.a.patch.groove_on[GF_GROOVE_PUMP] && bus_layer.a.patch.sfx_on[GF_SFX_DRIVE], "HYBRID bus export")) return 1;
        hybrid_page->onClick();
        auto* a_decay = dynamic_cast<juce::Slider*>(editor->findChildWithID("hybrid.a.voice.decay"));
        auto* b_pitch = dynamic_cast<juce::Slider*>(editor->findChildWithID("hybrid.b.voice.lfo_pitch"));
        if (!check(a_decay && b_pitch && a_decay->isVisible() && !b_pitch->isVisible(), "HYBRID voice A page")) return 1;
        a_decay->setValue(777, juce::sendNotificationSync);
        hybrid_page->onClick();
        if (!check(b_pitch->isVisible() && !a_decay->isVisible(), "HYBRID voice B page")) return 1;
        b_pitch->setValue(.33, juce::sendNotificationSync);
        std::vector<uint8_t> voice_gfl; studio::LayerPreset voice_layer;
        if (!check(dynamic_cast<gf::PresetImport*>(cc_restore.get())->export_hybrid_preset(voice_gfl) &&
                   gf::decode_hybrid_layer(voice_gfl.data(), voice_gfl.size(), voice_layer) &&
                   std::abs(voice_layer.a.patch.decay_ms - 777) < .1f &&
                   std::abs(voice_layer.b.patch.lfo_amt[GF_DEST_PITCH] - .33f) < .001f, "HYBRID voice export")) return 1;
        hybrid_page->onClick();
        auto* osc_stop = dynamic_cast<juce::ComboBox*>(editor->findChildWithID("hybrid.osc.stop"));
        auto* osc_slot0 = dynamic_cast<juce::Slider*>(editor->findChildWithID("hybrid.osc.slot.0"));
        auto* osc_slot2 = dynamic_cast<juce::Slider*>(editor->findChildWithID("hybrid.osc.slot.2"));
        if (!check(osc_stop && osc_slot0 && osc_slot2 && osc_stop->isVisible() && osc_slot0->isVisible(), "HYBRID oscillator A page")) return 1;
        osc_stop->setSelectedId(2, juce::sendNotificationSync);
        osc_slot2->setValue(1, juce::sendNotificationSync);
        hybrid_page->onClick();
        osc_stop->setSelectedId(1, juce::sendNotificationSync);
        if (!check(osc_slot0->isVisible(), "HYBRID oscillator B page")) return 1;
        osc_slot0->setValue(1, juce::sendNotificationSync);
        std::vector<uint8_t> osc_gfl; studio::LayerPreset osc_layer;
        if (!check(dynamic_cast<gf::PresetImport*>(cc_restore.get())->export_hybrid_preset(osc_gfl) &&
                   gf::decode_hybrid_layer(osc_gfl.data(), osc_gfl.size(), osc_layer) &&
                   osc_layer.a.patch.osc[0][2] == 8 && osc_layer.b.patch.osc[0][0] == 5,
                   "HYBRID oscillator pages export engine slots")) return 1;
        hybrid_page->onClick();
        auto* factory = dynamic_cast<juce::ComboBox*>(editor->findChildWithID("hybrid.factory"));
        if (!check(factory && factory->isVisible(), "HYBRID factory selector")) return 1;
        auto filter_hybrid = std::unique_ptr<juce::AudioProcessor>(createPluginFilter());
        auto filter_reference = std::unique_ptr<juce::AudioProcessor>(createPluginFilter());
        for (auto* p : {filter_hybrid.get(), filter_reference.get()}) {
            if (!check(dynamic_cast<gf::PresetImport*>(p)->import_preset(bytes.data(), bytes.size()), "HYBRID filter layer import")) return 1;
            find(*p, "hybrid.focus")->setValueNotifyingHost(1); p->prepareToPlay(48000, 128);
        }
        juce::MidiBuffer filter_events, filter_none;
        filter_events.addEvent(juce::MidiMessage::noteOn(1, 60, uint8_t(100)), 0);
        filter_none.addEvent(juce::MidiMessage::noteOn(1, 60, uint8_t(100)), 0);
        filter_events.addEvent(juce::MidiMessage::controllerEvent(1, 74, 0), 64);
        filter_events.addEvent(juce::MidiMessage::controllerEvent(1, 71, 127), 64);
        filter_hybrid->processBlock(audio, filter_events); filter_reference->processBlock(other, filter_none);
        double filter_effect = 0;
        for (int i = 0; i < 128; ++i) {
            if (i < 64 && !check(audio.getSample(0, i) == other.getSample(0, i), "HYBRID filter CC offset")) return 1;
            if (i >= 64) filter_effect += std::abs(audio.getSample(0, i) - other.getSample(0, i));
        }
        if (!check(filter_effect > .00001, "HYBRID filter CC changes audio")) return 1;
        auto macros = std::unique_ptr<juce::AudioProcessor>(createPluginFilter());
        auto macros_reference = std::unique_ptr<juce::AudioProcessor>(createPluginFilter());
        for (auto* p : {macros.get(), macros_reference.get()}) {
            if (!check(dynamic_cast<gf::PresetImport*>(p)->import_preset(bytes.data(), bytes.size()), "HYBRID macro layer import")) return 1;
            p->prepareToPlay(48000, 128);
        }
        juce::MidiBuffer macro_events, macro_plain;
        macro_events.addEvent(juce::MidiMessage::noteOn(1, 60, uint8_t(100)), 0);
        macro_plain.addEvent(juce::MidiMessage::noteOn(1, 60, uint8_t(100)), 0);
        for (int cc : {14, 15, 16, 19}) macro_events.addEvent(juce::MidiMessage::controllerEvent(1, cc, 127), 64);
        macros->processBlock(audio, macro_events); macros_reference->processBlock(other, macro_plain);
        double macro_effect = 0;
        for (int i = 0; i < 128; ++i) {
            if (i < 64 && !check(audio.getSample(0, i) == other.getSample(0, i), "HYBRID macro CC offset")) return 1;
            if (i >= 64) macro_effect += std::abs(audio.getSample(0, i) - other.getSample(0, i));
        }
        if (!check(macro_effect > .00001, "HYBRID macro CC changes audio")) return 1;
        juce::MemoryBlock macro_state; macros->getStateInformation(macro_state);
        auto macro_restored = std::unique_ptr<juce::AudioProcessor>(createPluginFilter());
        macro_restored->setStateInformation(macro_state.getData(), int(macro_state.getSize()));
        if (!check(find(*macro_restored, "hybrid.macro.drift")->getValue() > .99f &&
                   find(*macro_restored, "hybrid.macro.grit")->getValue() > .99f &&
                   find(*macro_restored, "hybrid.macro.pocket")->getValue() > .99f &&
                   find(*macro_restored, "hybrid.macro.move")->getValue() > .99f, "HYBRID pending macro CC restores")) return 1;
        std::vector<uint8_t> macro_gfl; studio::LayerPreset macro_layer;
        if (!check(dynamic_cast<gf::PresetImport*>(macros.get())->export_hybrid_preset(macro_gfl) &&
                   gf::decode_hybrid_layer(macro_gfl.data(), macro_gfl.size(), macro_layer) &&
                   macro_layer.a.patch.groove[GF_GROOVE_MACRO][GF_MACRO_GRIT] > 99 &&
                   macro_layer.a.patch.groove[GF_GROOVE_MACRO][GF_MACRO_MOVE] > 99, "HYBRID pending macro CC export")) return 1;
        juce::MemoryBlock filter_state; filter_hybrid->getStateInformation(filter_state);
        auto filter_restored = std::unique_ptr<juce::AudioProcessor>(createPluginFilter());
        filter_restored->setStateInformation(filter_state.getData(), int(filter_state.getSize()));
        std::vector<uint8_t> filter_bytes; studio::LayerPreset filter_layer;
        if (!check(dynamic_cast<gf::PresetImport*>(filter_restored.get())->export_hybrid_preset(filter_bytes) &&
                   gf::decode_hybrid_layer(filter_bytes.data(), filter_bytes.size(), filter_layer) &&
                   std::abs(filter_layer.b.cutoff_hz - GF_CUTOFF_RANGE[layer.eng_b].lo) < .1f &&
                   filter_layer.a.cutoff_hz >= GF_CUTOFF_RANGE[layer.eng_a].lo && filter_layer.b.reso_norm > .99f,
                   "HYBRID focused filter CC restores")) return 1;
        auto* manual_cutoff = find(*filter_hybrid, "hybrid.b.cutoff");
        manual_cutoff->setValueNotifyingHost(manual_cutoff->convertTo0to1(1000));
        filter_bytes.clear();
        if (!check(dynamic_cast<gf::PresetImport*>(filter_hybrid.get())->export_hybrid_preset(filter_bytes) &&
                   gf::decode_hybrid_layer(filter_bytes.data(), filter_bytes.size(), filter_layer) &&
                   std::abs(filter_layer.b.cutoff_hz - 1000) < .1f, "HYBRID DAW cutoff overrides pending CC")) return 1;
        juce::MidiBuffer b_move_events;
        b_move_events.addEvent(juce::MidiMessage::controllerEvent(1, 19, 127), 64);
        filter_hybrid->processBlock(audio, b_move_events);
        juce::MemoryBlock b_move_state; filter_hybrid->getStateInformation(b_move_state);
        auto b_move_restore = std::unique_ptr<juce::AudioProcessor>(createPluginFilter());
        b_move_restore->setStateInformation(b_move_state.getData(), int(b_move_state.getSize()));
        filter_bytes.clear();
        if (!check(find(*b_move_restore, "hybrid.b.move")->getValue() > .99f &&
                   dynamic_cast<gf::PresetImport*>(b_move_restore.get())->export_hybrid_preset(filter_bytes) &&
                   gf::decode_hybrid_layer(filter_bytes.data(), filter_bytes.size(), filter_layer) &&
                   filter_layer.b.patch.groove[GF_GROOVE_MACRO][GF_MACRO_MOVE] > 99 &&
                   std::abs(filter_layer.a.patch.groove[GF_GROOVE_MACRO][GF_MACRO_MOVE] - layer.a.patch.groove[GF_GROOVE_MACRO][GF_MACRO_MOVE]) < .1f,
                   "HYBRID focused B MOVE CC restores")) return 1;
        factory->setSelectedId(2, juce::sendNotificationSync);
        exported.clear();
        if (!check(dynamic_cast<gf::PresetImport*>(cc_restore.get())->export_hybrid_preset(exported) &&
                   gf::decode_hybrid_layer(exported.data(), exported.size(), exported_layer) &&
                   std::strcmp(exported_layer.name, studio::layer_def(1).name) == 0, "HYBRID factory selector loads layer")) return 1;
        auto* save_layer = dynamic_cast<juce::TextButton*>(editor->findChildWithID("hybrid.save"));
        if (!check(save_layer && save_layer->isVisible() && factory->getNumItems() == 8, "HYBRID bank controls")) return 1;
        save_layer->onClick();
        if (!check(factory->getNumItems() == 9, "HYBRID save adds user layer")) return 1;
        for (int i = 9; i < studio::LAYER_MAX; ++i) save_layer->onClick();
        if (!check(factory->getNumItems() == studio::LAYER_MAX, "HYBRID bank reaches capacity")) return 1;
        save_layer->onClick();
        if (!check(factory->getNumItems() == studio::LAYER_MAX, "HYBRID bank rejects overflow")) return 1;
        juce::MemoryBlock bank_state; cc_restore->getStateInformation(bank_state);
        auto bank_restored = std::unique_ptr<juce::AudioProcessor>(createPluginFilter());
        bank_restored->setStateInformation(bank_state.getData(), int(bank_state.getSize()));
        auto bank_editor = std::unique_ptr<juce::AudioProcessorEditor>(bank_restored->createEditor());
        auto* restored_factory = dynamic_cast<juce::ComboBox*>(bank_editor->findChildWithID("hybrid.factory"));
        if (!check(restored_factory && restored_factory->getNumItems() == studio::LAYER_MAX, "HYBRID bank project restores")) return 1;
        restored_factory->setSelectedId(9, juce::sendNotificationSync);
        exported.clear();
        if (!check(dynamic_cast<gf::PresetImport*>(bank_restored.get())->export_hybrid_preset(exported) &&
                   gf::decode_hybrid_layer(exported.data(), exported.size(), exported_layer) &&
                   std::strcmp(exported_layer.name, "LAYER 09") == 0, "HYBRID saved layer loads")) return 1;
        restored_factory->setSelectedId(1, juce::sendNotificationSync);
        bank_restored->prepareToPlay(48000, 128);
        juce::MidiBuffer custom_bank_cc;
        custom_bank_cc.addEvent(juce::MidiMessage::controllerEvent(1, 18, 33), 0);
        bank_restored->processBlock(audio, custom_bank_cc);
        bool selected_custom = false;
        for (int attempt = 0; attempt < 10 && !selected_custom; ++attempt) {
            juce::Thread::sleep(35); juce::Timer::callPendingTimersSynchronously(); exported.clear();
            selected_custom = dynamic_cast<gf::PresetImport*>(bank_restored.get())->export_hybrid_preset(exported) &&
                gf::decode_hybrid_layer(exported.data(), exported.size(), exported_layer) && std::strcmp(exported_layer.name, "LAYER 09") == 0;
        }
        if (!check(selected_custom, "HYBRID CC18 selects user layer")) return 1;
    }
    {
        auto p = std::unique_ptr<juce::AudioProcessor>(createPluginFilter());
        auto parameter = [](juce::AudioProcessor& processor, const char* id) -> juce::RangedAudioParameter* {
            for (auto* item : processor.getParameters())
                if (auto* value = dynamic_cast<juce::RangedAudioParameter*>(item); value && value->paramID == id) return value;
            return nullptr;
        };
        auto* select = parameter(*p, "arp2600_preview");
        auto* ring = parameter(*p, "arp2600.ring");
        if (!check(select && ring, "ARP VST parameters")) return 1;
        select->setValueNotifyingHost(1); ring->setValueNotifyingHost(.75f);
        p->prepareToPlay(48000, 128);
        juce::AudioBuffer<float> audio(2, 128);
        juce::MidiBuffer midi;
        midi.addEvent(juce::MidiMessage::noteOn(1, 60, uint8_t(100)), 40);
        p->processBlock(audio, midi);
        for (int i = 0; i < 40; ++i) if (!check(audio.getSample(0, i) == 0, "ARP MIDI offset")) return 1;
        if (!check(audio.getRMSLevel(0, 40, 88) > 0, "ARP VST audible")) return 1;
        juce::MemoryBlock state;
        p->getStateInformation(state);
        auto restored = std::unique_ptr<juce::AudioProcessor>(createPluginFilter());
        restored->setStateInformation(state.getData(), int(state.getSize()));
        if (!check(parameter(*restored, "arp2600_preview")->getValue() > .5f &&
                   std::abs(parameter(*restored, "arp2600.ring")->getValue() - .75f) < .001f, "ARP VST state restores")) return 1;
        auto editor = std::unique_ptr<juce::AudioProcessorEditor>(restored->createEditor());
        auto* toggle = dynamic_cast<juce::ToggleButton*>(editor->findChildWithID("engine.arp2600"));
        auto* slider = dynamic_cast<juce::Slider*>(editor->findChildWithID("arp2600.ring"));
        if (!check(toggle && slider && toggle->getToggleState() && slider->isVisible(), "ARP editor state")) return 1;
        slider->setValue(.4, juce::sendNotificationSync);
        if (!check(std::abs(parameter(*restored, "arp2600.ring")->getValue() - .4f) < .001f, "ARP ring editor binding")) return 1;
        auto* page = dynamic_cast<juce::TextButton*>(editor->findChildWithID("engine.page"));
        auto* wave = dynamic_cast<juce::Slider*>(editor->findChildWithID("arp2600.osc1.wave"));
        if (!check(page && wave && !wave->isVisible(), "ARP oscillator page controls")) return 1;
        page->onClick();
        if (!check(wave->isVisible() && !slider->isVisible(), "ARP oscillator page visibility")) return 1;
        wave->setValue(4, juce::sendNotificationSync);
        if (!check(parameter(*restored, "arp2600.osc1.wave")->getValue() > .99f, "ARP oscillator editor binding")) return 1;
        page->onClick();
        auto* lfo = dynamic_cast<juce::Slider*>(editor->findChildWithID("arp2600.lfo_pitch"));
        if (!check(lfo && lfo->isVisible() && !wave->isVisible(), "ARP envelope and LFO page")) return 1;
        lfo->setValue(.65, juce::sendNotificationSync);
        if (!check(std::abs(parameter(*restored, "arp2600.lfo_pitch")->getValue() - .65f) < .001f, "ARP voice editor binding")) return 1;
        page->onClick();
        auto* groove = dynamic_cast<juce::Slider*>(editor->findChildWithID("shared.groove.0.0"));
        auto* fx = dynamic_cast<juce::Slider*>(editor->findChildWithID("shared.fx.0.0"));
        if (!check(groove && fx && groove->isVisible() && !fx->isVisible(), "ARP groove page")) return 1;
        groove->setValue(1, juce::sendNotificationSync);
        if (!check(parameter(*restored, "arp2600.groove.pump.on")->getValue() > .5f &&
                   parameter(*restored, "ob6.groove.pump.on")->getValue() < .5f, "ARP groove editor isolation")) return 1;
        page->onClick();
        if (!check(fx->isVisible() && !groove->isVisible(), "ARP FX page")) return 1;
        fx->setValue(1, juce::sendNotificationSync);
        if (!check(parameter(*restored, "arp2600.fx.drive.on")->getValue() > .5f &&
                   parameter(*restored, "ob6.fx.drive.on")->getValue() < .5f, "ARP FX editor isolation")) return 1;
        page->onClick();
        auto* macro = dynamic_cast<juce::Slider*>(editor->findChildWithID("shared.macro.0"));
        if (!check(macro && macro->isVisible() && !fx->isVisible(), "ARP macro page")) return 1;
        macro->setValue(.6, juce::sendNotificationSync);
        if (!check(parameter(*restored, "arp2600.macro.grit")->getValue() > .59f &&
                   parameter(*restored, "ob6.macro.grit")->getValue() < .01f, "ARP macro editor isolation")) return 1;
        select->setValueNotifyingHost(0);
        juce::MidiBuffer empty;
        p->processBlock(audio, empty);
        select->setValueNotifyingHost(1);
        p->processBlock(audio, empty);
        if (!check(audio.getRMSLevel(0, 0, 128) == 0, "ARP motor switch clears notes")) return 1;
        studio::Preset arp_preset;
        if (!check(studio::factory_preset(5, 0, &arp_preset), "ARP factory preset")) return 1;
        arp_preset.patch.osc[0][2] = 73;
        arp_preset.patch.osc[0][0] = 2;
        arp_preset.patch.osc[0][3] = 80; arp_preset.patch.mod[0][5] = 100;
        arp_preset.patch.mod[0][1] = 1; arp_preset.patch.osc[1][1] = 2.3f;
        arp_preset.patch.mod[0][2] = 5; arp_preset.patch.mod[0][3] = 80; arp_preset.patch.mod[0][4] = 65;
        arp_preset.patch.mod[0][0] = 0; arp_preset.patch.glide = .5f;
        arp_preset.patch.groove_on[GF_GROOVE_UNISON] = true;
        arp_preset.patch.groove[GF_GROOVE_UNISON][GF_UNISON_VOICES] = 3;
        arp_preset.patch.groove[GF_GROOVE_UNISON][GF_UNISON_DETUNE] = 30;
        arp_preset.patch.groove[GF_GROOVE_MACRO][GF_MACRO_DRIFT] = 80;
        arp_preset.patch.lfo_amt[GF_DEST_PITCH] = .5f; arp_preset.patch.lfo_amt[GF_DEST_CUTOFF] = .4f;
        std::array<uint8_t, studio::PRESET_BYTES> bytes{};
        studio::preset_encode(arp_preset, 5, bytes.data(), bytes.size());
        auto* importer = dynamic_cast<gf::PresetImport*>(p.get());
        if (!check(importer && importer->import_preset(bytes.data(), bytes.size()) &&
                   select->getValue() > .5f && std::abs(ring->getValue() - .73f) < .001f, "ARP GFP import")) return 1;
        std::vector<uint8_t> exported;
        studio::Preset decoded;
        uint8_t exported_engine = 99;
        ring->setValueNotifyingHost(.45f);
        auto* wave_parameter = parameter(*p, "arp2600.osc1.wave");
        if (!check(wave_parameter != nullptr, "ARP oscillator parameter")) return 1;
        wave_parameter->setValueNotifyingHost(wave_parameter->convertTo0to1(4));
        auto* lfo_parameter = parameter(*p, "arp2600.lfo_pitch");
        if (!check(lfo_parameter != nullptr, "ARP voice parameter")) return 1;
        lfo_parameter->setValueNotifyingHost(.65f);
        parameter(*p, "arp2600.groove.pump.on")->setValueNotifyingHost(1);
        parameter(*p, "arp2600.fx.drive.on")->setValueNotifyingHost(1);
        parameter(*p, "arp2600.macro.grit")->setValueNotifyingHost(.6f);
        parameter(*p, "arp2600.macro.move")->setValueNotifyingHost(.5f);
        if (!check(importer->export_arp2600_preset(exported) &&
                   gf::decode_supported_preset(exported.data(), exported.size(), decoded, exported_engine) &&
                   exported_engine == 5 && std::abs(decoded.patch.osc[0][2] - 45) < .1f && decoded.patch.osc[0][0] == 4 &&
                   decoded.patch.mod[0][1] > .5f && std::abs(decoded.patch.lfo_amt[GF_DEST_PITCH] - .65f) < .02f && decoded.patch.groove_on[GF_GROOVE_UNISON] && decoded.patch.groove_on[GF_GROOVE_PUMP] &&
                   decoded.patch.sfx_on[GF_SFX_DRIVE] && decoded.patch.groove[GF_GROOVE_MACRO][GF_MACRO_MOVE] == 0 &&
                   decoded.patch.groove[GF_GROOVE_MACRO][GF_MACRO_GRIT] == 0 && decoded.patch.drive > arp_preset.patch.drive, "ARP GFP export")) return 1;
        ring->setValueNotifyingHost(.73f);
        auto invalid = bytes; invalid[0] ^= 0xff;
        if (!check(!importer->import_preset(invalid.data(), invalid.size()), "ARP invalid GFP rejected")) return 1;
        juce::MemoryBlock preset_state;
        p->getStateInformation(preset_state);
        auto preset_restore = std::unique_ptr<juce::AudioProcessor>(createPluginFilter());
        preset_restore->setStateInformation(preset_state.getData(), int(preset_state.getSize()));
        preset_restore->prepareToPlay(48000, 128);
        juce::AudioBuffer<float> restored_audio(2, 128);
        juce::MidiBuffer note_a, note_b;
        note_a.addEvent(juce::MidiMessage::noteOn(1, 60, uint8_t(100)), 0);
        note_b.addEvent(juce::MidiMessage::noteOn(1, 60, uint8_t(100)), 0);
        p->processBlock(audio, note_a); preset_restore->processBlock(restored_audio, note_b);
        for (int i = 0; i < 128; ++i)
            if (!check(std::abs(audio.getSample(0, i) - restored_audio.getSample(0, i)) < 1e-6f, "ARP GFP state audio restores")) return 1;
        if (!check(restored_audio.getRMSLevel(0, 0, 128) > 0, "ARP restored preset audible")) return 1;
        juce::MidiBuffer bend, no_bend;
        bend.addEvent(juce::MidiMessage::pitchWheel(1, 16383), 64);
        p->processBlock(audio, bend); preset_restore->processBlock(restored_audio, no_bend);
        double bend_change = 0;
        for (int i = 0; i < 128; ++i) {
            if (i < 64 && !check(audio.getSample(0, i) == restored_audio.getSample(0, i), "ARP pitch bend offset")) return 1;
            if (i >= 64) bend_change += std::abs(audio.getSample(0, i) - restored_audio.getSample(0, i));
        }
        if (!check(bend_change > .0001, "ARP pitch bend changes sound")) return 1;
        auto expression = std::unique_ptr<juce::AudioProcessor>(createPluginFilter());
        auto expression_reference = std::unique_ptr<juce::AudioProcessor>(createPluginFilter());
        for (auto* synth : {expression.get(), expression_reference.get()}) {
            synth->setStateInformation(preset_state.getData(), int(preset_state.getSize()));
            synth->prepareToPlay(48000, 128);
        }
        juce::MidiBuffer expression_note_a, expression_note_b;
        expression_note_a.addEvent(juce::MidiMessage::noteOn(1, 60, uint8_t(100)), 0);
        expression_note_b.addEvent(juce::MidiMessage::noteOn(1, 60, uint8_t(100)), 0);
        expression->processBlock(audio, expression_note_a); expression_reference->processBlock(restored_audio, expression_note_b);
        juce::MidiBuffer expression_events, expression_empty;
        expression_events.addEvent(juce::MidiMessage::controllerEvent(1, 1, 127), 64);
        expression_events.addEvent(juce::MidiMessage::channelPressureChange(1, 127), 64);
        expression->processBlock(audio, expression_events); expression_reference->processBlock(restored_audio, expression_empty);
        double expression_change = 0;
        for (int i = 0; i < 128; ++i) {
            if (i < 64 && !check(audio.getSample(0, i) == restored_audio.getSample(0, i), "ARP expression MIDI offset")) return 1;
            if (i >= 64) expression_change += std::abs(audio.getSample(0, i) - restored_audio.getSample(0, i));
        }
        if (!check(expression_change > .0001, "ARP MIDI expression affects sound")) return 1;
        auto macro_synth = std::unique_ptr<juce::AudioProcessor>(createPluginFilter());
        auto macro_reference = std::unique_ptr<juce::AudioProcessor>(createPluginFilter());
        for (auto* synth : {macro_synth.get(), macro_reference.get()}) {
            synth->setStateInformation(preset_state.getData(), int(preset_state.getSize())); synth->prepareToPlay(48000, 128);
        }
        juce::MidiBuffer macro_note, reference_note;
        macro_note.addEvent(juce::MidiMessage::noteOn(1, 60, uint8_t(100)), 0);
        reference_note.addEvent(juce::MidiMessage::noteOn(1, 60, uint8_t(100)), 0);
        macro_synth->processBlock(audio, macro_note); macro_reference->processBlock(restored_audio, reference_note);
        parameter(*macro_synth, "arp2600.macro.grit")->setValueNotifyingHost(1);
        parameter(*macro_synth, "arp2600.macro.move")->setValueNotifyingHost(1);
        double macro_change = 0;
        for (int block = 0; block < 16; ++block) {
            juce::MidiBuffer a, b;
            macro_synth->processBlock(audio, a); macro_reference->processBlock(restored_audio, b);
            for (int i = 0; i < 128; ++i) macro_change += std::abs(audio.getSample(0, i) - restored_audio.getSample(0, i));
        }
        if (!check(macro_change > .01, "ARP macros affect audio")) return 1;
        auto cc_synth = std::unique_ptr<juce::AudioProcessor>(createPluginFilter());
        auto cc_reference = std::unique_ptr<juce::AudioProcessor>(createPluginFilter());
        for (auto* synth : {cc_synth.get(), cc_reference.get()}) {
            synth->setStateInformation(preset_state.getData(), int(preset_state.getSize())); synth->prepareToPlay(48000, 128);
        }
        juce::MidiBuffer cc_events, cc_plain;
        cc_events.addEvent(juce::MidiMessage::noteOn(1, 60, uint8_t(100)), 0);
        cc_plain.addEvent(juce::MidiMessage::noteOn(1, 60, uint8_t(100)), 0);
        for (int cc : {14, 15, 16, 19}) cc_events.addEvent(juce::MidiMessage::controllerEvent(1, cc, 127), 64);
        cc_synth->processBlock(audio, cc_events); cc_reference->processBlock(restored_audio, cc_plain);
        double cc_change = 0;
        for (int i = 0; i < 128; ++i) {
            if (i < 64 && !check(audio.getSample(0, i) == restored_audio.getSample(0, i), "ARP macro CC offset")) return 1;
            if (i >= 64) cc_change += std::abs(audio.getSample(0, i) - restored_audio.getSample(0, i));
        }
        if (!check(cc_change > .0001, "ARP macro CC changes audio")) return 1;
        juce::MemoryBlock cc_state; cc_synth->getStateInformation(cc_state);
        auto cc_restore = std::unique_ptr<juce::AudioProcessor>(createPluginFilter());
        cc_restore->setStateInformation(cc_state.getData(), int(cc_state.getSize()));
        if (!check(parameter(*cc_restore, "arp2600.drift")->getValue() > .99f &&
                   parameter(*cc_restore, "arp2600.macro.grit")->getValue() > .99f &&
                   parameter(*cc_restore, "arp2600.macro.pocket")->getValue() > .99f &&
                   parameter(*cc_restore, "arp2600.macro.move")->getValue() > .99f, "ARP pending macro CC restores")) return 1;
        std::vector<uint8_t> cc_export; studio::Preset cc_preset; uint8_t cc_engine = 99;
        if (!check(dynamic_cast<gf::PresetImport*>(cc_synth.get())->export_arp2600_preset(cc_export) &&
                   gf::decode_supported_preset(cc_export.data(), cc_export.size(), cc_preset, cc_engine) && cc_engine == 5 &&
                   cc_preset.patch.groove[GF_GROOVE_MACRO][GF_MACRO_DRIFT] > 99 &&
                   cc_preset.patch.groove[GF_GROOVE_MACRO][GF_MACRO_MOVE] == 0, "ARP pending macro CC exports")) return 1;
        auto filter_synth = std::unique_ptr<juce::AudioProcessor>(createPluginFilter());
        auto filter_reference = std::unique_ptr<juce::AudioProcessor>(createPluginFilter());
        for (auto* synth : {filter_synth.get(), filter_reference.get()}) {
            synth->setStateInformation(preset_state.getData(), int(preset_state.getSize())); synth->prepareToPlay(48000, 128);
        }
        juce::MidiBuffer filter_events, filter_plain;
        filter_events.addEvent(juce::MidiMessage::noteOn(1, 60, uint8_t(100)), 0);
        filter_plain.addEvent(juce::MidiMessage::noteOn(1, 60, uint8_t(100)), 0);
        filter_events.addEvent(juce::MidiMessage::controllerEvent(1, 74, 127), 64);
        filter_events.addEvent(juce::MidiMessage::controllerEvent(1, 71, 64), 64);
        filter_events.addEvent(juce::MidiMessage::controllerEvent(1, 19, 0), 96);
        filter_synth->processBlock(audio, filter_events); filter_reference->processBlock(restored_audio, filter_plain);
        double filter_change = 0;
        for (int i = 0; i < 128; ++i) {
            if (i < 64 && !check(audio.getSample(0, i) == restored_audio.getSample(0, i), "ARP filter CC offset")) return 1;
            if (i >= 64) filter_change += std::abs(audio.getSample(0, i) - restored_audio.getSample(0, i));
        }
        if (!check(filter_change > .0001, "ARP filter CC changes audio")) return 1;
        std::vector<uint8_t> filter_export; studio::Preset filter_preset; uint8_t filter_engine = 99;
        if (!check(dynamic_cast<gf::PresetImport*>(filter_synth.get())->export_arp2600_preset(filter_export) &&
                   gf::decode_supported_preset(filter_export.data(), filter_export.size(), filter_preset, filter_engine) &&
                   filter_engine == 5 && filter_preset.cutoff_hz > GF_CUTOFF_RANGE[5].hi * .99f &&
                   std::abs(filter_preset.reso_norm - (64.0f / 127.0f + .12f)) < .03f, "ARP filter CC export")) return 1;
        juce::MemoryBlock filter_state; filter_synth->getStateInformation(filter_state);
        auto filter_restore = std::unique_ptr<juce::AudioProcessor>(createPluginFilter());
        filter_restore->setStateInformation(filter_state.getData(), int(filter_state.getSize()));
        std::vector<uint8_t> filter_restored_export;
        if (!check(dynamic_cast<gf::PresetImport*>(filter_restore.get())->export_arp2600_preset(filter_restored_export) &&
                   filter_restored_export == filter_export, "ARP filter CC state restores")) return 1;
        auto* cutoff_parameter = parameter(*filter_synth, "cutoff");
        cutoff_parameter->setValueNotifyingHost(cutoff_parameter->convertTo0to1(1000));
        if (!check(dynamic_cast<gf::PresetImport*>(filter_synth.get())->export_arp2600_preset(filter_export) &&
                   gf::decode_supported_preset(filter_export.data(), filter_export.size(), filter_preset, filter_engine) &&
                   filter_preset.cutoff_hz < 2000, "ARP DAW cutoff overrides MIDI")) return 1;
        studio::Preset bus_preset;
        if (!check(studio::factory_preset(5, 0, &bus_preset), "ARP bus factory preset")) return 1;
        for (auto& on : bus_preset.patch.sfx_on) on = false;
        for (auto& on : bus_preset.patch.groove_on) on = false;
        std::array<uint8_t, studio::PRESET_BYTES> bus_bytes{};
        studio::preset_encode(bus_preset, 5, bus_bytes.data(), bus_bytes.size());
        auto dry = std::unique_ptr<juce::AudioProcessor>(createPluginFilter());
        auto wet = std::unique_ptr<juce::AudioProcessor>(createPluginFilter());
        if (!check(dynamic_cast<gf::PresetImport*>(dry.get())->import_preset(bus_bytes.data(), bus_bytes.size()), "ARP dry GFP")) return 1;
        bus_preset.patch.sfx_on[GF_SFX_DRIVE] = true; bus_preset.patch.sfx_mix[GF_SFX_DRIVE] = 1;
        bus_preset.patch.sfx_p[GF_SFX_DRIVE][0] = .9f;
        studio::preset_encode(bus_preset, 5, bus_bytes.data(), bus_bytes.size());
        if (!check(dynamic_cast<gf::PresetImport*>(wet.get())->import_preset(bus_bytes.data(), bus_bytes.size()), "ARP FX GFP")) return 1;
        dry->prepareToPlay(44100, 128); wet->prepareToPlay(44100, 128);
        double fx_change = 0;
        for (int block = 0; block < 24; ++block) {
            juce::MidiBuffer dry_midi, wet_midi;
            if (!block) { dry_midi.addEvent(juce::MidiMessage::noteOn(1, 60, uint8_t(100)), 0); wet_midi.addEvent(juce::MidiMessage::noteOn(1, 60, uint8_t(100)), 0); }
            dry->processBlock(audio, dry_midi); wet->processBlock(restored_audio, wet_midi);
            for (int i = 0; i < 128; ++i) fx_change += std::abs(audio.getSample(0, i) - restored_audio.getSample(0, i));
        }
        if (!check(fx_change > .01, "ARP GFP effects process audio")) return 1;
        bus_preset.patch.groove[GF_GROOVE_MACRO][GF_MACRO_MOVE] = 50;
        bus_preset.patch.sfx_on[GF_SFX_CHORUS] = true;
        bus_preset.patch.sfx_p[GF_SFX_CHORUS][0] = .2f;
        studio::preset_encode(bus_preset, 5, bus_bytes.data(), bus_bytes.size());
        auto* wet_import = dynamic_cast<gf::PresetImport*>(wet.get());
        if (!check(wet_import->import_preset(bus_bytes.data(), bus_bytes.size()), "ARP MOVE preset")) return 1;
        std::vector<uint8_t> bus_export;
        studio::Preset bus_exported; uint8_t bus_engine = 99;
        if (!check(wet_import->export_arp2600_preset(bus_export) &&
                   gf::decode_supported_preset(bus_export.data(), bus_export.size(), bus_exported, bus_engine) &&
                   bus_engine == 5 && bus_exported.patch.groove[GF_GROOVE_MACRO][GF_MACRO_MOVE] == 0 &&
                   std::abs(bus_exported.patch.sfx_p[GF_SFX_CHORUS][0] - .35f) < .02f,
                   "ARP export preserves MOVE effects")) return 1;
        bus_preset.patch.sfx_on[GF_SFX_DRIVE] = false;
        bus_preset.patch.groove_on[GF_GROOVE_ARP] = true;
        bus_preset.patch.groove[GF_GROOVE_ARP][GF_ARP_LATCH] = 0;
        studio::preset_encode(bus_preset, 5, bus_bytes.data(), bus_bytes.size());
        auto arp = std::unique_ptr<juce::AudioProcessor>(createPluginFilter());
        if (!check(dynamic_cast<gf::PresetImport*>(arp.get())->import_preset(bus_bytes.data(), bus_bytes.size()), "ARP arpeggiator GFP")) return 1;
        arp->prepareToPlay(44100, 128);
        double arp_energy = 0;
        for (int block = 0; block < 200; ++block) {
            juce::MidiBuffer events;
            if (!block) events.addEvent(juce::MidiMessage::noteOn(1, 60, uint8_t(100)), 0);
            arp->processBlock(audio, events);
            arp_energy += audio.getRMSLevel(0, 0, 128);
        }
        if (!check(arp_energy > .001, "ARP GFP arpeggiator produces audio")) return 1;
    }
    {
        const auto model_parameter = [](juce::AudioProcessor& processor) -> juce::RangedAudioParameter* {
            for (auto* item : processor.getParameters())
                if (auto* p = dynamic_cast<juce::RangedAudioParameter*>(item); p && p->paramID == "modeld_preview") return p;
            return nullptr;
        };
        auto model = std::unique_ptr<juce::AudioProcessor>(createPluginFilter());
        auto quiet = std::unique_ptr<juce::AudioProcessor>(createPluginFilter());
        auto* selection = model_parameter(*model);
        if (!check(selection != nullptr, "Model D selector")) return 1;
        selection->setValueNotifyingHost(1);
        model->prepareToPlay(48000, 128); quiet->prepareToPlay(48000, 128);
        juce::AudioBuffer<float> audio(2, 128), other(2, 128);
        juce::MidiBuffer events, empty;
        events.addEvent(juce::MidiMessage::noteOn(1, 60, uint8_t(100)), 40);
        model->processBlock(audio, events); quiet->processBlock(other, empty);
        for (int i = 0; i < 40; ++i) if (!check(audio.getSample(0, i) == 0, "Model D MIDI offset")) return 1;
        if (!check(audio.getRMSLevel(0, 40, 88) > 0 && other.getRMSLevel(0, 0, 128) == 0, "Model D audible and isolated")) return 1;
        juce::MemoryBlock state;
        model->getStateInformation(state);
        auto restored = std::unique_ptr<juce::AudioProcessor>(createPluginFilter());
        restored->setStateInformation(state.getData(), int(state.getSize()));
        if (!check(model_parameter(*restored) && model_parameter(*restored)->getValue() > .5f, "Model D selection restores")) return 1;
        studio::Preset preset;
        if (!check(studio::factory_preset(0, 0, &preset), "Model D factory preset")) return 1;
        std::array<uint8_t, studio::PRESET_BYTES> bytes{};
        studio::preset_encode(preset, 0, bytes.data(), bytes.size());
        auto* importer = dynamic_cast<gf::PresetImport*>(model.get());
        if (!check(importer && importer->import_preset(bytes.data(), bytes.size()), "Model D GFP import")) return 1;
        model->getStateInformation(state);
        restored->setStateInformation(state.getData(), int(state.getSize()));
        model->prepareToPlay(44100, 128); restored->prepareToPlay(44100, 128);
        juce::MidiBuffer note_a, note_b;
        note_a.addEvent(juce::MidiMessage::noteOn(1, 60, uint8_t(100)), 0);
        note_b.addEvent(juce::MidiMessage::noteOn(1, 60, uint8_t(100)), 0);
        model->processBlock(audio, note_a); restored->processBlock(other, note_b);
        for (int i = 0; i < 128; ++i)
            if (std::abs(audio.getSample(0, i) - other.getSample(0, i)) >= 1e-7f) { std::fprintf(stderr, "Model D state mismatch at %d: %.9g %.9g\n", i, audio.getSample(0, i), other.getSample(0, i)); return 1; }
        if (!check(audio.getRMSLevel(0, 0, 128) > 0, "Model D GFP audible")) return 1;
        auto find_model_parameter = [](juce::AudioProcessor& processor, const char* id) -> juce::RangedAudioParameter* {
            for (auto* item : processor.getParameters())
                if (auto* p = dynamic_cast<juce::RangedAudioParameter*>(item); p && p->paramID == id) return p;
            return nullptr;
        };
        auto* model_wave = find_model_parameter(*model, "modeld.osc1.wave");
        auto* model_filter_mod = find_model_parameter(*model, "modeld.mod.filter");
        if (!check(model_wave && model_filter_mod, "Model D oscillator automation parameters")) return 1;
        model_wave->setValueNotifyingHost(1);
        model_filter_mod->setValueNotifyingHost(1);
        auto* model_lfo_pitch = find_model_parameter(*model, "modeld.lfo_pitch");
        auto* model_drift = find_model_parameter(*model, "modeld.drift");
        auto* model_fx_drive = find_model_parameter(*model, "modeld.fx.drive.on");
        auto* model_pump = find_model_parameter(*model, "modeld.groove.pump.on");
        auto* model_grit = find_model_parameter(*model, "modeld.macro.grit");
        auto* model_move = find_model_parameter(*model, "modeld.macro.move");
        if (!check(model_lfo_pitch && model_drift && model_fx_drive && model_pump && model_grit && model_move, "Model D voice, bus and macro automation parameters")) return 1;
        model_lfo_pitch->setValueNotifyingHost(.65f); model_drift->setValueNotifyingHost(.7f);
        model_fx_drive->setValueNotifyingHost(1); model_pump->setValueNotifyingHost(1);
        model_grit->setValueNotifyingHost(.6f); model_move->setValueNotifyingHost(.5f);
        std::vector<uint8_t> model_export;
        studio::Preset model_decoded;
        uint8_t model_engine = 99;
        if (!check(importer->export_model_d_preset(model_export) &&
                   gf::decode_supported_preset(model_export.data(), model_export.size(), model_decoded, model_engine) &&
                   model_engine == 0 && model_decoded.patch.osc[0][0] == 5 && model_decoded.patch.mod[0][2] > .5f &&
                   std::abs(model_decoded.patch.lfo_amt[GF_DEST_PITCH] - .65f) < .02f &&
                   std::abs(model_decoded.patch.groove[GF_GROOVE_MACRO][GF_MACRO_DRIFT] - 70) < .1f &&
                   model_decoded.patch.sfx_on[GF_SFX_DRIVE] && model_decoded.patch.groove_on[GF_GROOVE_PUMP] &&
                   model_decoded.patch.drive > preset.patch.drive && model_decoded.cutoff_hz > preset.cutoff_hz &&
                   model_decoded.patch.groove[GF_GROOVE_MACRO][GF_MACRO_MOVE] == 0,
                   "Model D GFP export preserves oscillator and bakes macros")) return 1;
        auto model_roundtrip = std::unique_ptr<juce::AudioProcessor>(createPluginFilter());
        if (!check(dynamic_cast<gf::PresetImport*>(model_roundtrip.get())->import_preset(model_export.data(), model_export.size()),
                   "Model D exported GFP imports")) return 1;
        model->getStateInformation(state);
        model_roundtrip->setStateInformation(state.getData(), int(state.getSize()));
        if (!check(find_model_parameter(*model_roundtrip, "modeld.osc1.wave")->getValue() > .99f &&
                   find_model_parameter(*model_roundtrip, "modeld.mod.filter")->getValue() > .5f &&
                   std::abs(find_model_parameter(*model_roundtrip, "modeld.lfo_pitch")->getValue() - .65f) < .001f &&
                   find_model_parameter(*model_roundtrip, "modeld.fx.drive.on")->getValue() > .5f &&
                   find_model_parameter(*model_roundtrip, "modeld.groove.pump.on")->getValue() > .5f &&
                   std::abs(find_model_parameter(*model_roundtrip, "modeld.macro.grit")->getValue() - .6f) < .001f,
                   "Model D oscillator automation restores")) return 1;
        model->prepareToPlay(44100, 128); restored->prepareToPlay(44100, 128);
        double osc_delta = 0;
        for (int block = 0; block < 12; ++block) {
            juce::MidiBuffer changed, original;
            if (!block) { changed.addEvent(juce::MidiMessage::noteOn(1, 60, uint8_t(100)), 0); original.addEvent(juce::MidiMessage::noteOn(1, 60, uint8_t(100)), 0); }
            model->processBlock(audio, changed); restored->processBlock(other, original);
            for (int i = 0; i < 128; ++i) osc_delta += std::abs(audio.getSample(0, i) - other.getSample(0, i));
        }
        if (!check(osc_delta > .01, "Model D oscillator automation changes audio")) return 1;
        for (auto& enabled : preset.patch.sfx_on) enabled = false;
        for (auto& enabled : preset.patch.groove_on) enabled = false;
        studio::preset_encode(preset, 0, bytes.data(), bytes.size());
        auto dry = std::unique_ptr<juce::AudioProcessor>(createPluginFilter());
        auto wet = std::unique_ptr<juce::AudioProcessor>(createPluginFilter());
        if (!check(dynamic_cast<gf::PresetImport*>(dry.get())->import_preset(bytes.data(), bytes.size()), "Model D dry preset")) return 1;
        preset.patch.sfx_on[GF_SFX_DRIVE] = true;
        preset.patch.sfx_mix[GF_SFX_DRIVE] = 1;
        preset.patch.sfx_p[GF_SFX_DRIVE][0] = .9f;
        studio::preset_encode(preset, 0, bytes.data(), bytes.size());
        if (!check(dynamic_cast<gf::PresetImport*>(wet.get())->import_preset(bytes.data(), bytes.size()), "Model D FX preset")) return 1;
        dry->prepareToPlay(44100, 128); wet->prepareToPlay(44100, 128);
        double fx_delta = 0;
        for (int block = 0; block < 24; ++block) {
            juce::MidiBuffer dry_events, wet_events;
            if (!block) {
                dry_events.addEvent(juce::MidiMessage::noteOn(1, 60, uint8_t(100)), 0);
                wet_events.addEvent(juce::MidiMessage::noteOn(1, 60, uint8_t(100)), 0);
            }
            dry->processBlock(audio, dry_events); wet->processBlock(other, wet_events);
            for (int i = 0; i < 128; ++i) fx_delta += std::abs(audio.getSample(0, i) - other.getSample(0, i));
        }
        if (!check(fx_delta > .01, "Model D GFP synth FX changes audio")) return 1;
        preset.patch.sfx_on[GF_SFX_DRIVE] = false;
        preset.patch.groove_on[GF_GROOVE_ARP] = true;
        preset.patch.groove[GF_GROOVE_ARP][GF_ARP_LATCH] = 0;
        studio::preset_encode(preset, 0, bytes.data(), bytes.size());
        auto arp_model = std::unique_ptr<juce::AudioProcessor>(createPluginFilter());
        if (!check(dynamic_cast<gf::PresetImport*>(arp_model.get())->import_preset(bytes.data(), bytes.size()), "Model D ARP preset")) return 1;
        arp_model->prepareToPlay(44100, 128);
        float arp_peak = 0;
        for (int block = 0; block < 130; ++block) {
            juce::MidiBuffer arp_events;
            if (block == 0) arp_events.addEvent(juce::MidiMessage::noteOn(1, 60, uint8_t(100)), 0);
            if (block == 30) { arp_events.addEvent(juce::MidiMessage::controllerEvent(1, 64, 127), 0); arp_events.addEvent(juce::MidiMessage::noteOff(1, 60), 1); }
            if (block == 90) arp_events.addEvent(juce::MidiMessage::controllerEvent(1, 64, 0), 0);
            arp_model->processBlock(audio, arp_events);
            if (block >= 40 && block < 90) arp_peak = std::max(arp_peak, audio.getRMSLevel(0, 0, 128));
        }
        if (!check(arp_peak > .00001f, "Model D ARP sustain produces audio")) return 1;
        preset.patch.groove_on[GF_GROOVE_ARP] = false;
        preset.patch.mod[0][0] = 0; preset.patch.mod[0][1] = 1;
        studio::preset_encode(preset, 0, bytes.data(), bytes.size());
        auto wheel_a = std::unique_ptr<juce::AudioProcessor>(createPluginFilter());
        auto wheel_b = std::unique_ptr<juce::AudioProcessor>(createPluginFilter());
        for (auto* processor : {wheel_a.get(), wheel_b.get()}) {
            if (!check(dynamic_cast<gf::PresetImport*>(processor)->import_preset(bytes.data(), bytes.size()), "Model D modulation preset")) return 1;
            processor->prepareToPlay(44100, 128);
        }
        double wheel_delta = 0;
        for (int block = 0; block < 16; ++block) {
            juce::MidiBuffer a_events, b_events;
            if (!block) {
                a_events.addEvent(juce::MidiMessage::noteOn(1, 60, uint8_t(100)), 0);
                b_events.addEvent(juce::MidiMessage::noteOn(1, 60, uint8_t(100)), 0);
                a_events.addEvent(juce::MidiMessage::controllerEvent(1, 1, 127), 64);
            }
            wheel_a->processBlock(audio, a_events); wheel_b->processBlock(other, b_events);
            for (int i = 0; i < 128; ++i) {
                const float x = audio.getSample(0, i), y = other.getSample(0, i);
                if (!block && i < 64 && !check(x == y, "Model D wheel respects MIDI offset")) return 1;
                if (block > 0) wheel_delta += std::abs(x - y);
            }
        }
        if (!check(wheel_delta > .01, "Model D wheel changes audio")) return 1;
        auto model_cc = std::unique_ptr<juce::AudioProcessor>(createPluginFilter());
        auto model_cc_plain = std::unique_ptr<juce::AudioProcessor>(createPluginFilter());
        for (auto* processor : {model_cc.get(), model_cc_plain.get()}) {
            if (!check(dynamic_cast<gf::PresetImport*>(processor)->import_preset(bytes.data(), bytes.size()), "Model D macro CC preset")) return 1;
            processor->prepareToPlay(44100, 128);
        }
        juce::MidiBuffer model_cc_events, model_cc_plain_events;
        model_cc_events.addEvent(juce::MidiMessage::noteOn(1, 60, uint8_t(100)), 0);
        model_cc_plain_events.addEvent(juce::MidiMessage::noteOn(1, 60, uint8_t(100)), 0);
        for (int cc : {14, 15, 16, 19}) model_cc_events.addEvent(juce::MidiMessage::controllerEvent(1, cc, 127), 64);
        model_cc->processBlock(audio, model_cc_events); model_cc_plain->processBlock(other, model_cc_plain_events);
        double model_cc_delta = 0;
        for (int i = 0; i < 128; ++i) {
            const float x = audio.getSample(0, i), y = other.getSample(0, i);
            if (i < 64 && !check(x == y, "Model D macro CC respects offset")) return 1;
            if (i >= 64) model_cc_delta += std::abs(x - y);
        }
        if (!check(model_cc_delta > .0001, "Model D macro CC changes audio")) return 1;
        juce::MemoryBlock model_cc_state;
        model_cc->getStateInformation(model_cc_state);
        auto model_cc_restore = std::unique_ptr<juce::AudioProcessor>(createPluginFilter());
        model_cc_restore->setStateInformation(model_cc_state.getData(), int(model_cc_state.getSize()));
        if (!check(find_model_parameter(*model_cc_restore, "modeld.drift")->getValue() > .99f &&
                   find_model_parameter(*model_cc_restore, "modeld.macro.grit")->getValue() > .99f &&
                   find_model_parameter(*model_cc_restore, "modeld.macro.pocket")->getValue() > .99f &&
                   find_model_parameter(*model_cc_restore, "modeld.macro.move")->getValue() > .99f,
                   "Model D pending CC restores")) return 1;
        std::vector<uint8_t> model_cc_export;
        if (!check(dynamic_cast<gf::PresetImport*>(model_cc.get())->export_model_d_preset(model_cc_export) &&
                   gf::decode_supported_preset(model_cc_export.data(), model_cc_export.size(), model_decoded, model_engine) &&
                   model_engine == 0 && model_decoded.patch.groove[GF_GROOVE_MACRO][GF_MACRO_DRIFT] > 99 &&
                   model_decoded.patch.drive > preset.patch.drive && model_decoded.cutoff_hz > preset.cutoff_hz,
                   "Model D pending CC exports")) return 1;
        studio::Preset filter_preset;
        if (!check(studio::factory_preset(0, 0, &filter_preset), "Model D filter factory preset")) return 1;
        filter_preset.patch.groove[GF_GROOVE_MACRO][GF_MACRO_GRIT] = 30;
        filter_preset.patch.groove[GF_GROOVE_MACRO][GF_MACRO_MOVE] = 25;
        std::array<uint8_t, studio::PRESET_BYTES> filter_bytes{};
        studio::preset_encode(filter_preset, 0, filter_bytes.data(), filter_bytes.size());
        auto filter_model = std::unique_ptr<juce::AudioProcessor>(createPluginFilter());
        auto filter_plain = std::unique_ptr<juce::AudioProcessor>(createPluginFilter());
        for (auto* p : {filter_model.get(), filter_plain.get()}) {
            if (!check(dynamic_cast<gf::PresetImport*>(p)->import_preset(filter_bytes.data(), filter_bytes.size()), "Model D filter CC preset")) return 1;
            p->prepareToPlay(44100, 128);
        }
        juce::MidiBuffer filter_events, filter_plain_events;
        filter_events.addEvent(juce::MidiMessage::noteOn(1, 60, uint8_t(100)), 0);
        filter_plain_events.addEvent(juce::MidiMessage::noteOn(1, 60, uint8_t(100)), 0);
        filter_events.addEvent(juce::MidiMessage::controllerEvent(1, 74, 127), 64);
        filter_events.addEvent(juce::MidiMessage::controllerEvent(1, 71, 64), 64);
        filter_events.addEvent(juce::MidiMessage::controllerEvent(1, 19, 0), 96);
        filter_model->processBlock(audio, filter_events); filter_plain->processBlock(other, filter_plain_events);
        double filter_delta = 0;
        for (int i = 0; i < 128; ++i) {
            if (i < 64 && !check(audio.getSample(0, i) == other.getSample(0, i), "Model D filter CC respects offset")) return 1;
            if (i >= 64) filter_delta += std::abs(audio.getSample(0, i) - other.getSample(0, i));
        }
        if (!check(filter_delta > .0001, "Model D filter CC changes audio")) return 1;
        std::vector<uint8_t> filter_export;
        if (!check(dynamic_cast<gf::PresetImport*>(filter_model.get())->export_model_d_preset(filter_export) &&
                   gf::decode_supported_preset(filter_export.data(), filter_export.size(), model_decoded, model_engine) &&
                   model_engine == 0 && model_decoded.cutoff_hz > 7900 &&
                   std::abs(model_decoded.reso_norm - (64.0f / 127.0f + .06f)) < .03f,
                   "Model D filter CC and macros export effective values")) return 1;
        juce::MemoryBlock filter_state;
        filter_model->getStateInformation(filter_state);
        auto filter_restored = std::unique_ptr<juce::AudioProcessor>(createPluginFilter());
        filter_restored->setStateInformation(filter_state.getData(), int(filter_state.getSize()));
        std::vector<uint8_t> filter_restored_export;
        if (!check(dynamic_cast<gf::PresetImport*>(filter_restored.get())->export_model_d_preset(filter_restored_export) &&
                   filter_restored_export == filter_export, "Model D filter CC state restores")) return 1;
    }
    for (double fs : {44100.0, 48000.0, 96000.0})
        if (!check(ob6_midi_block_independence(fs), "OB-6 MIDI block independence")) return 1;
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
    auto ob6_pressure_a = std::unique_ptr<juce::AudioProcessor>(createPluginFilter());
    auto ob6_pressure_b = std::unique_ptr<juce::AudioProcessor>(createPluginFilter());
    for (auto* p : {ob6_pressure_a.get(), ob6_pressure_b.get()}) {
        find_parameter(*p, "ob6_preview")->setValueNotifyingHost(1);
        p->prepareToPlay(44100, 128);
    }
    juce::AudioBuffer<float> pressure_a(2, 128), pressure_b(2, 128);
    double pressure_difference = 0;
    for (int block = 0; block < 16; ++block) {
        juce::MidiBuffer midi_a, midi_b;
        if (block == 0) {
            midi_a.addEvent(juce::MidiMessage::noteOn(1, 60, uint8_t(100)), 0);
            midi_b.addEvent(juce::MidiMessage::noteOn(1, 60, uint8_t(100)), 0);
            midi_a.addEvent(juce::MidiMessage::channelPressureChange(1, 127), 64);
        }
        ob6_pressure_a->processBlock(pressure_a, midi_a); ob6_pressure_b->processBlock(pressure_b, midi_b);
        for (int i = 0; i < 128; ++i) {
            const float a = pressure_a.getSample(0, i), b = pressure_b.getSample(0, i);
            if (!check(std::isfinite(a) && std::isfinite(b), "OB-6 pressure output finite")) return 1;
            if (block == 0 && i < 64 && !check(a == b, "OB-6 pressure respects MIDI offset")) return 1;
            if (block > 0) pressure_difference += std::abs(a - b);
        }
    }
    if (!check(pressure_difference > .01, "OB-6 channel pressure changes audio")) return 1;
    {
        auto editor_plugin = std::unique_ptr<juce::AudioProcessor>(createPluginFilter());
        find_parameter(*editor_plugin, "ob6_preview")->setValueNotifyingHost(1);
        auto editor = std::unique_ptr<juce::AudioProcessorEditor>(editor_plugin->createEditor());
        if (!check(editor != nullptr, "OB-6 editor creation")) return 1;
        auto* page = dynamic_cast<juce::TextButton*>(editor->findChildWithID("engine.page"));
        auto* osc = dynamic_cast<juce::Slider*>(editor->findChildWithID("ob6.osc1.shape"));
        auto* voice = dynamic_cast<juce::Slider*>(editor->findChildWithID("ob6.lfo_rate"));
        auto* groove = dynamic_cast<juce::Slider*>(editor->findChildWithID("shared.groove.0.0"));
        auto* fx = dynamic_cast<juce::Slider*>(editor->findChildWithID("shared.fx.0.0"));
        auto* macro = dynamic_cast<juce::Slider*>(editor->findChildWithID("shared.macro.0"));
        auto* juno_select = dynamic_cast<juce::ToggleButton*>(editor->findChildWithID("engine.juno"));
        if (!check(page && osc && voice && groove && fx && macro && juno_select && page->isVisible(), "OB-6 editor controls exist")) return 1;

        if (!check(!osc->isVisible() && !voice->isVisible() && !groove->isVisible() && !fx->isVisible() && !macro->isVisible(), "OB-6 initial page visibility")) return 1;
        auto* fx_tab = dynamic_cast<juce::TextButton*>(editor->findChildWithID("navigation.tab.4"));
        auto* voice_tab = dynamic_cast<juce::TextButton*>(editor->findChildWithID("navigation.tab.0"));
        if (!check(fx_tab && voice_tab && fx_tab->isVisible(), "OB-6 direct navigation")) return 1;
        fx_tab->onClick();
        if (!check(fx->isVisible() && !osc->isVisible(), "OB-6 FX tab opens controls")) return 1;
        auto* feedback_label = dynamic_cast<juce::Label*>(editor->findChildWithID("fx.label.3.3"));
        if (!check(feedback_label && feedback_label->isVisible() && feedback_label->getText() == "FEEDBACK", "FX page displays parameter names")) return 1;
        voice_tab->onClick();
        if (!check(!fx->isVisible() && !feedback_label->isVisible(), "OB-6 voice tab returns to initial page")) return 1;
        page->onClick();
        if (!check(osc->isVisible() && !voice->isVisible() && page->getButtonText().startsWith("OSC"), "OB-6 oscillator page")) return 1;
        if (!check(osc->getTextFromValue(50.0) == "50" && osc->getTextFromValue(12.34) == "12.3" &&
                   std::abs(osc->getValueFromText("12.34") - 12.34) < .01, "OB-6 compact display preserves numeric input")) return 1;
        osc->setValue(80, juce::sendNotificationSync);
        if (!check(find_parameter(*editor_plugin, "ob6.osc1.shape")->getValue() > .79f, "OB-6 oscillator widget binding")) return 1;
        page->onClick();
        if (!check(!osc->isVisible() && voice->isVisible(), "OB-6 envelope and LFO page")) return 1;
        page->onClick();
        if (!check(!voice->isVisible() && groove->isVisible(), "OB-6 groove page")) return 1;
        groove->setValue(1, juce::sendNotificationSync);
        if (!check(find_parameter(*editor_plugin, "ob6.groove.pump.on")->getValue() > .5f &&
                   find_parameter(*editor_plugin, "juno.groove.pump.on")->getValue() < .5f, "OB-6 groove widget isolation")) return 1;
        page->onClick();
        if (!check(!groove->isVisible() && fx->isVisible(), "OB-6 FX page")) return 1;
        fx->setValue(1, juce::sendNotificationSync);
        if (!check(find_parameter(*editor_plugin, "ob6.fx.drive.on")->getValue() > .5f &&
                   find_parameter(*editor_plugin, "juno.fx.drive.on")->getValue() < .5f, "OB-6 FX widget isolation")) return 1;
        page->onClick();
        if (!check(!fx->isVisible() && macro->isVisible(), "OB-6 macro page")) return 1;
        macro->setValue(.8, juce::sendNotificationSync);
        if (!check(find_parameter(*editor_plugin, "ob6.macro.grit")->getValue() > .79f &&
                   find_parameter(*editor_plugin, "juno.macro.grit")->getValue() < .01f, "OB-6 macro widget isolation")) return 1;
        page->onClick();
        if (!check(!macro->isVisible(), "OB-6 returns to voice page")) return 1;
        juno_select->setToggleState(true, juce::sendNotificationSync);
        for (int i = 0; i < 5; ++i) page->onClick();
        if (!check(groove->isVisible(), "shared groove page switches to Juno")) return 1;
        groove->setValue(1, juce::sendNotificationSync);
        if (!check(find_parameter(*editor_plugin, "juno.groove.pump.on")->getValue() > .5f &&
                   find_parameter(*editor_plugin, "ob6.groove.pump.on")->getValue() > .5f, "shared widget rebinds to Juno")) return 1;
    }
    {
        auto p = std::unique_ptr<juce::AudioProcessor>(createPluginFilter());
        find_parameter(*p, "modeld_preview")->setValueNotifyingHost(1);
        auto editor = std::unique_ptr<juce::AudioProcessorEditor>(p->createEditor());
        auto* page = dynamic_cast<juce::TextButton*>(editor->findChildWithID("engine.page"));
        auto* osc = dynamic_cast<juce::Slider*>(editor->findChildWithID("modeld.osc1.wave"));
        auto* voice = dynamic_cast<juce::Slider*>(editor->findChildWithID("modeld.lfo_rate"));
        auto* groove = dynamic_cast<juce::Slider*>(editor->findChildWithID("shared.groove.0.0"));
        auto* fx = dynamic_cast<juce::Slider*>(editor->findChildWithID("shared.fx.0.0"));
        auto* macro = dynamic_cast<juce::Slider*>(editor->findChildWithID("shared.macro.0"));
        if (!check(page && osc && voice && groove && fx && macro, "Model D editor controls exist")) return 1;
        page->onClick();
        if (!check(osc->isVisible() && !voice->isVisible(), "Model D oscillator page")) return 1;
        osc->setValue(5, juce::sendNotificationSync);
        if (!check(find_parameter(*p, "modeld.osc1.wave")->getValue() > .99f, "Model D oscillator binding")) return 1;
        page->onClick();
        if (!check(voice->isVisible() && !osc->isVisible(), "Model D envelope page")) return 1;
        page->onClick();
        if (!check(groove->isVisible(), "Model D groove page")) return 1;
        groove->setValue(1, juce::sendNotificationSync);
        if (!check(find_parameter(*p, "modeld.groove.pump.on")->getValue() > .5f && find_parameter(*p, "ob6.groove.pump.on")->getValue() < .5f, "Model D groove binding")) return 1;
        page->onClick();
        if (!check(fx->isVisible(), "Model D FX page")) return 1;
        fx->setValue(1, juce::sendNotificationSync);
        if (!check(find_parameter(*p, "modeld.fx.drive.on")->getValue() > .5f && find_parameter(*p, "ob6.fx.drive.on")->getValue() < .5f, "Model D FX binding")) return 1;
        page->onClick();
        if (!check(macro->isVisible(), "Model D macro page")) return 1;
        macro->setValue(.8, juce::sendNotificationSync);
        if (!check(find_parameter(*p, "modeld.macro.grit")->getValue() > .79f && find_parameter(*p, "ob6.macro.grit")->getValue() < .01f, "Model D macro binding")) return 1;
    }
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
    auto* ob6_shape = find_parameter(*ob6_plugin, "ob6.osc1.shape");
    if (!check(ob6_shape != nullptr, "OB-6 oscillator parameter exists")) return 1;
    ob6_shape->setValueNotifyingHost(1.0f);
    std::vector<uint8_t> ob6_osc_export;
    if (!check(ob6_import->export_ob6_preset(ob6_osc_export), "OB-6 oscillator export")) return 1;
    studio::Preset ob6_osc_decoded;
    uint8_t ob6_osc_engine = 0;
    if (!check(gf::decode_supported_preset(ob6_osc_export.data(), ob6_osc_export.size(), ob6_osc_decoded, ob6_osc_engine) &&
               ob6_osc_engine == 3 && ob6_osc_decoded.patch.osc[0][0] > 99, "OB-6 oscillator edit exported")) return 1;
    juce::MemoryBlock ob6_osc_state;
    ob6_plugin->getStateInformation(ob6_osc_state);
    auto ob6_osc_restore = std::unique_ptr<juce::AudioProcessor>(createPluginFilter());
    ob6_osc_restore->setStateInformation(ob6_osc_state.getData(), int(ob6_osc_state.getSize()));
    if (!check(find_parameter(*ob6_osc_restore, "ob6.osc1.shape")->getValue() > .99f, "OB-6 oscillator edit restored")) return 1;
    ob6_plugin->prepareToPlay(44100, 128); ob6_osc_restore->prepareToPlay(44100, 128);
    juce::AudioBuffer<float> ob6_osc_audio(2, 128);
    juce::MidiBuffer osc_note_a, osc_note_b;
    osc_note_a.addEvent(juce::MidiMessage::noteOn(1, 60, uint8_t(100)), 0);
    osc_note_b.addEvent(juce::MidiMessage::noteOn(1, 60, uint8_t(100)), 0);
    ob6_plugin->processBlock(ob6_audio, osc_note_a); ob6_osc_restore->processBlock(ob6_osc_audio, osc_note_b);
    for (int i = 0; i < 128; ++i)
        if (!check(std::abs(ob6_audio.getSample(0, i) - ob6_osc_audio.getSample(0, i)) < 1e-7f, "OB-6 oscillator audio restored")) return 1;
    auto ob6_osc_original = std::unique_ptr<juce::AudioProcessor>(createPluginFilter());
    ob6_osc_original->setStateInformation(ob6_saved.getData(), int(ob6_saved.getSize()));
    ob6_osc_original->prepareToPlay(44100, 128);
    juce::MidiBuffer osc_original_note;
    osc_original_note.addEvent(juce::MidiMessage::noteOn(1, 60, uint8_t(100)), 0);
    ob6_osc_original->processBlock(ob6_osc_audio, osc_original_note);
    float osc_difference = 0;
    for (int i = 0; i < 128; ++i) osc_difference += std::abs(ob6_audio.getSample(0, i) - ob6_osc_audio.getSample(0, i));
    if (!check(osc_difference > .001f, "OB-6 oscillator automation changes audio")) return 1;
    studio::Preset macro_ob6;
    if (!check(studio::factory_preset(3, 0, &macro_ob6), "OB-6 macro factory preset")) return 1;
    macro_ob6.patch.groove[GF_GROOVE_MACRO][GF_MACRO_GRIT] = 30;
    macro_ob6.patch.groove[GF_GROOVE_MACRO][GF_MACRO_POCKET] = 20;
    macro_ob6.patch.groove[GF_GROOVE_MACRO][GF_MACRO_MOVE] = 25;
    macro_ob6.patch.sfx_on[GF_SFX_CHORUS] = true;
    std::array<uint8_t, studio::PRESET_BYTES> macro_ob6_bytes{};
    studio::preset_encode(macro_ob6, 3, macro_ob6_bytes.data(), macro_ob6_bytes.size());
    auto ob6_filter_midi = std::unique_ptr<juce::AudioProcessor>(createPluginFilter());
    auto ob6_filter_reference = std::unique_ptr<juce::AudioProcessor>(createPluginFilter());
    for (auto* p : {ob6_filter_midi.get(), ob6_filter_reference.get()}) {
        if (!check(dynamic_cast<gf::PresetImport*>(p)->import_preset(macro_ob6_bytes.data(), macro_ob6_bytes.size()), "OB-6 filter CC preset")) return 1;
        p->prepareToPlay(44100, 128);
    }
    juce::AudioBuffer<float> ob6_filter_a(2, 128), ob6_filter_b(2, 128);
    juce::MidiBuffer ob6_filter_events, ob6_filter_plain;
    ob6_filter_events.addEvent(juce::MidiMessage::noteOn(1, 60, uint8_t(100)), 0);
    ob6_filter_plain.addEvent(juce::MidiMessage::noteOn(1, 60, uint8_t(100)), 0);
    ob6_filter_events.addEvent(juce::MidiMessage::controllerEvent(1, 74, 127), 64);
    ob6_filter_events.addEvent(juce::MidiMessage::controllerEvent(1, 71, 64), 64);
    ob6_filter_events.addEvent(juce::MidiMessage::controllerEvent(1, 19, 0), 96);
    ob6_filter_midi->processBlock(ob6_filter_a, ob6_filter_events);
    ob6_filter_reference->processBlock(ob6_filter_b, ob6_filter_plain);
    float filter_cc_difference = 0;
    for (int i = 0; i < 128; ++i) {
        const float a = ob6_filter_a.getSample(0, i), b = ob6_filter_b.getSample(0, i);
        if (i < 64 && !check(a == b, "OB-6 filter CC respects MIDI offset")) return 1;
        if (i >= 64) filter_cc_difference += std::abs(a - b);
    }
    if (!check(filter_cc_difference > .001f, "OB-6 filter CC changes audio")) return 1;
    std::vector<uint8_t> ob6_filter_export;
    studio::Preset ob6_filter_decoded;
    uint8_t ob6_filter_engine = 0;
    auto* ob6_filter_import = dynamic_cast<gf::PresetImport*>(ob6_filter_midi.get());
    if (!check(ob6_filter_import->export_ob6_preset(ob6_filter_export) &&
               gf::decode_supported_preset(ob6_filter_export.data(), ob6_filter_export.size(), ob6_filter_decoded, ob6_filter_engine) &&
               ob6_filter_engine == 3 && ob6_filter_decoded.cutoff_hz > 19900 &&
               std::abs(ob6_filter_decoded.reso_norm - (64.0f / 127.0f + .06f)) < .02f,
               "OB-6 filter CC and macros export effective values")) return 1;
    juce::MemoryBlock ob6_filter_state;
    ob6_filter_midi->getStateInformation(ob6_filter_state);
    auto ob6_filter_restore = std::unique_ptr<juce::AudioProcessor>(createPluginFilter());
    ob6_filter_restore->setStateInformation(ob6_filter_state.getData(), int(ob6_filter_state.getSize()));
    std::vector<uint8_t> ob6_filter_restored_export;
    if (!check(dynamic_cast<gf::PresetImport*>(ob6_filter_restore.get())->export_ob6_preset(ob6_filter_restored_export) &&
               ob6_filter_restored_export == ob6_filter_export, "OB-6 filter CC pending state restores")) return 1;
    auto ob6_filter_roundtrip = std::unique_ptr<juce::AudioProcessor>(createPluginFilter());
    if (!check(dynamic_cast<gf::PresetImport*>(ob6_filter_roundtrip.get())->import_preset(ob6_filter_export.data(), ob6_filter_export.size()),
               "OB-6 filter CC exported preset imports")) return 1;
    ob6_filter_restore->prepareToPlay(44100, 128); ob6_filter_roundtrip->prepareToPlay(44100, 128);
    double filter_roundtrip_error = 0, filter_roundtrip_signal = 0;
    for (int block = 0; block < 16; ++block) {
        juce::MidiBuffer a, b;
        if (!block) { a.addEvent(juce::MidiMessage::noteOn(1, 60, uint8_t(100)), 0); b.addEvent(juce::MidiMessage::noteOn(1, 60, uint8_t(100)), 0); }
        ob6_filter_restore->processBlock(ob6_filter_a, a); ob6_filter_roundtrip->processBlock(ob6_filter_b, b);
        for (int i = 0; i < 128; ++i) {
            const float x = ob6_filter_a.getSample(0, i), y = ob6_filter_b.getSample(0, i);
            filter_roundtrip_error += (x - y) * (x - y); filter_roundtrip_signal += x * x;
        }
    }
    if (!check(filter_roundtrip_signal > 0 && std::sqrt(filter_roundtrip_error / filter_roundtrip_signal) < .08,
               "OB-6 filter CC GFP roundtrip audio")) return 1;
    auto* pending_cutoff = find_parameter(*ob6_filter_midi, "cutoff");
    pending_cutoff->setValueNotifyingHost(pending_cutoff->convertTo0to1(1200.0f));
    if (!check(ob6_filter_import->export_ob6_preset(ob6_filter_restored_export) &&
               gf::decode_supported_preset(ob6_filter_restored_export.data(), ob6_filter_restored_export.size(), ob6_filter_decoded, ob6_filter_engine) &&
               ob6_filter_decoded.cutoff_hz < 2000.0f, "OB-6 host cutoff supersedes pending MIDI")) return 1;
    ob6_filter_midi->getStateInformation(ob6_filter_state);
    auto ob6_pending_host_restore = std::unique_ptr<juce::AudioProcessor>(createPluginFilter());
    ob6_pending_host_restore->setStateInformation(ob6_filter_state.getData(), int(ob6_filter_state.getSize()));
    if (!check(dynamic_cast<gf::PresetImport*>(ob6_pending_host_restore.get())->export_ob6_preset(ob6_filter_restored_export) &&
               gf::decode_supported_preset(ob6_filter_restored_export.data(), ob6_filter_restored_export.size(), ob6_filter_decoded, ob6_filter_engine) &&
               ob6_filter_decoded.cutoff_hz < 2000.0f, "OB-6 pending host override restores")) return 1;
    auto* restored_cutoff = find_parameter(*ob6_filter_restore, "cutoff");
    restored_cutoff->setValueNotifyingHost(restored_cutoff->convertTo0to1(1000.0f));
    if (!check(dynamic_cast<gf::PresetImport*>(ob6_filter_restore.get())->export_ob6_preset(ob6_filter_restored_export) &&
               gf::decode_supported_preset(ob6_filter_restored_export.data(), ob6_filter_restored_export.size(), ob6_filter_decoded, ob6_filter_engine) &&
               ob6_filter_decoded.cutoff_hz < 2000.0f && std::abs(ob6_filter_decoded.reso_norm - (64.0f / 127.0f + .06f)) < .02f,
               "OB-6 host cutoff supersedes MIDI independently")) return 1;
    auto ob6_cc_a = std::unique_ptr<juce::AudioProcessor>(createPluginFilter());
    auto ob6_cc_b = std::unique_ptr<juce::AudioProcessor>(createPluginFilter());
    for (auto* p : {ob6_cc_a.get(), ob6_cc_b.get()}) {
        if (!check(dynamic_cast<gf::PresetImport*>(p)->import_preset(macro_ob6_bytes.data(), macro_ob6_bytes.size()), "OB-6 CC preset import")) return 1;
        p->prepareToPlay(44100, 128);
    }
    juce::AudioBuffer<float> ob6_cc_audio_a(2, 128), ob6_cc_audio_b(2, 128);
    juce::MidiBuffer ob6_cc_events_a, ob6_cc_events_b;
    ob6_cc_events_a.addEvent(juce::MidiMessage::noteOn(1, 60, uint8_t(100)), 0);
    ob6_cc_events_b.addEvent(juce::MidiMessage::noteOn(1, 60, uint8_t(100)), 0);
    for (int cc : {14, 15, 16, 19}) ob6_cc_events_a.addEvent(juce::MidiMessage::controllerEvent(1, cc, 127), 64);
    ob6_cc_a->processBlock(ob6_cc_audio_a, ob6_cc_events_a); ob6_cc_b->processBlock(ob6_cc_audio_b, ob6_cc_events_b);
    for (int i = 0; i < 64; ++i)
        if (!check(ob6_cc_audio_a.getSample(0, i) == ob6_cc_audio_b.getSample(0, i), "OB-6 CC respects MIDI offset")) return 1;
    double ob6_cc_difference = 0;
    for (int i = 64; i < 128; ++i) ob6_cc_difference += std::abs(ob6_cc_audio_a.getSample(0, i) - ob6_cc_audio_b.getSample(0, i));
    if (!check(ob6_cc_difference > .0001, "OB-6 CC macros change audio")) return 1;
    juce::MemoryBlock ob6_cc_state;
    ob6_cc_a->getStateInformation(ob6_cc_state);
    auto ob6_cc_restored = std::unique_ptr<juce::AudioProcessor>(createPluginFilter());
    ob6_cc_restored->setStateInformation(ob6_cc_state.getData(), int(ob6_cc_state.getSize()));
    if (!check(find_parameter(*ob6_cc_restored, "ob6.drift")->getValue() > .99f &&
               find_parameter(*ob6_cc_restored, "ob6.macro.grit")->getValue() > .99f &&
               find_parameter(*ob6_cc_restored, "ob6.macro.pocket")->getValue() > .99f &&
               find_parameter(*ob6_cc_restored, "ob6.macro.move")->getValue() > .99f,
               "OB-6 pending CC values restore")) return 1;
    std::vector<uint8_t> ob6_cc_export;
    studio::Preset ob6_cc_decoded;
    uint8_t ob6_cc_engine = 0;
    if (!check(dynamic_cast<gf::PresetImport*>(ob6_cc_a.get())->export_ob6_preset(ob6_cc_export) &&
               gf::decode_supported_preset(ob6_cc_export.data(), ob6_cc_export.size(), ob6_cc_decoded, ob6_cc_engine) &&
               ob6_cc_engine == 3 && ob6_cc_decoded.patch.groove[GF_GROOVE_MACRO][GF_MACRO_DRIFT] > 99 &&
               ob6_cc_decoded.patch.drive > macro_ob6.patch.drive && ob6_cc_decoded.cutoff_hz > macro_ob6.cutoff_hz,
               "OB-6 pending CC values export")) return 1;
    auto macro_ob6_a = std::unique_ptr<juce::AudioProcessor>(createPluginFilter());
    auto* macro_ob6_import = dynamic_cast<gf::PresetImport*>(macro_ob6_a.get());
    if (!check(macro_ob6_import->import_preset(macro_ob6_bytes.data(), macro_ob6_bytes.size()), "OB-6 macro preset import")) return 1;
    auto* ob6_grit = find_parameter(*macro_ob6_a, "ob6.macro.grit");
    auto* ob6_pocket = find_parameter(*macro_ob6_a, "ob6.macro.pocket");
    auto* ob6_move = find_parameter(*macro_ob6_a, "ob6.macro.move");
    if (!check(ob6_grit && ob6_pocket && ob6_move && std::abs(ob6_grit->getValue() - .3f) < .02f &&
               std::abs(ob6_move->getValue() - .25f) < .02f, "OB-6 macros populate from GFP")) return 1;
    juce::MemoryBlock macro_ob6_state;
    macro_ob6_a->getStateInformation(macro_ob6_state);
    auto macro_ob6_baseline = std::unique_ptr<juce::AudioProcessor>(createPluginFilter());
    macro_ob6_baseline->setStateInformation(macro_ob6_state.getData(), int(macro_ob6_state.getSize()));
    ob6_grit->setValueNotifyingHost(.9f); ob6_pocket->setValueNotifyingHost(.8f); ob6_move->setValueNotifyingHost(.75f);
    macro_ob6_a->prepareToPlay(44100, 128); macro_ob6_baseline->prepareToPlay(44100, 128);
    double macro_ob6_difference = 0;
    for (int block = 0; block < 16; ++block) {
        juce::MidiBuffer ma, mb;
        if (block == 0) { ma.addEvent(juce::MidiMessage::noteOn(1, 60, uint8_t(100)), 0); mb.addEvent(juce::MidiMessage::noteOn(1, 60, uint8_t(100)), 0); }
        macro_ob6_a->processBlock(ob6_audio, ma); macro_ob6_baseline->processBlock(ob6_osc_audio, mb);
        for (int i = 0; i < 128; ++i) macro_ob6_difference += std::abs(ob6_audio.getSample(0, i) - ob6_osc_audio.getSample(0, i));
    }
    if (!check(macro_ob6_difference > .01, "OB-6 macro automation changes audio")) return 1;
    std::vector<uint8_t> ob6_macro_export;
    studio::Preset ob6_macro_decoded;
    uint8_t ob6_macro_engine = 0;
    if (!check(macro_ob6_import->export_ob6_preset(ob6_macro_export) &&
               gf::decode_supported_preset(ob6_macro_export.data(), ob6_macro_export.size(), ob6_macro_decoded, ob6_macro_engine) &&
               ob6_macro_engine == 3 && ob6_macro_decoded.patch.groove[GF_GROOVE_MACRO][GF_MACRO_GRIT] == 0 &&
               ob6_macro_decoded.cutoff_hz > macro_ob6.cutoff_hz &&
               ob6_macro_decoded.patch.sfx_p[GF_SFX_CHORUS][0] > macro_ob6.patch.sfx_p[GF_SFX_CHORUS][0],
               "OB-6 macro effective values export")) return 1;
    macro_ob6_a->getStateInformation(macro_ob6_state);
    auto macro_ob6_restored = std::unique_ptr<juce::AudioProcessor>(createPluginFilter());
    macro_ob6_restored->setStateInformation(macro_ob6_state.getData(), int(macro_ob6_state.getSize()));
    if (!check(std::abs(find_parameter(*macro_ob6_restored, "ob6.macro.grit")->getValue() - .9f) < .001f &&
               std::abs(find_parameter(*macro_ob6_restored, "ob6.macro.move")->getValue() - .75f) < .001f,
               "OB-6 macro automation restores")) return 1;
    auto macro_ob6_roundtrip = std::unique_ptr<juce::AudioProcessor>(createPluginFilter());
    if (!check(dynamic_cast<gf::PresetImport*>(macro_ob6_roundtrip.get())->import_preset(ob6_macro_export.data(), ob6_macro_export.size()),
               "OB-6 effective macro GFP imports")) return 1;
    macro_ob6_restored->prepareToPlay(44100, 128); macro_ob6_roundtrip->prepareToPlay(44100, 128);
    double roundtrip_error = 0, roundtrip_signal = 0;
    for (int block = 0; block < 16; ++block) {
        juce::MidiBuffer ma, mb;
        if (block == 0) { ma.addEvent(juce::MidiMessage::noteOn(1, 60, uint8_t(100)), 0); mb.addEvent(juce::MidiMessage::noteOn(1, 60, uint8_t(100)), 0); }
        macro_ob6_restored->processBlock(ob6_audio, ma); macro_ob6_roundtrip->processBlock(ob6_osc_audio, mb);
        for (int i = 0; i < 128; ++i) {
            const float a = ob6_audio.getSample(0, i), b = ob6_osc_audio.getSample(0, i);
            roundtrip_error += (a - b) * (a - b); roundtrip_signal += a * a;
        }
    }
    if (!check(roundtrip_signal > 0 && std::sqrt(roundtrip_error / roundtrip_signal) < .08,
               "OB-6 macro export preserves audio within GFP quantization")) return 1;
    auto macro_legacy_xml = juce::AudioProcessor::getXmlFromBinary(macro_ob6_state.getData(), int(macro_ob6_state.getSize()));
    macro_legacy_xml->removeAttribute("ob6MacroVersion");
    juce::MemoryBlock macro_legacy_state;
    juce::AudioProcessor::copyXmlToBinary(*macro_legacy_xml, macro_legacy_state);
    macro_ob6_restored->setStateInformation(macro_legacy_state.getData(), int(macro_legacy_state.getSize()));
    if (!check(std::abs(find_parameter(*macro_ob6_restored, "ob6.macro.grit")->getValue() - .3f) < .02f,
               "OB-6 legacy macros migrate from GFP")) return 1;
    auto* ob6_voice_sustain = find_parameter(*ob6_plugin, "ob6.amp_sustain");
    auto* ob6_voice_lfo = find_parameter(*ob6_plugin, "ob6.lfo_pitch");
    auto* ob6_voice_unison = find_parameter(*ob6_plugin, "ob6.unison_on");
    if (!check(ob6_voice_sustain && ob6_voice_lfo && ob6_voice_unison, "OB-6 voice parameters exist")) return 1;
    ob6_voice_sustain->setValueNotifyingHost(.23f);
    ob6_voice_lfo->setValueNotifyingHost(.7f);
    ob6_voice_unison->setValueNotifyingHost(1);
    ob6_osc_export.clear();
    if (!check(ob6_import->export_ob6_preset(ob6_osc_export) &&
               gf::decode_supported_preset(ob6_osc_export.data(), ob6_osc_export.size(), ob6_osc_decoded, ob6_osc_engine) &&
               std::abs(ob6_osc_decoded.patch.sustain - .23f) < .02f &&
               std::abs(ob6_osc_decoded.patch.lfo_amt[GF_DEST_PITCH] - .7f) < .02f &&
               ob6_osc_decoded.patch.groove_on[GF_GROOVE_UNISON], "OB-6 voice edits export")) return 1;
    ob6_plugin->getStateInformation(ob6_osc_state);
    ob6_osc_restore->setStateInformation(ob6_osc_state.getData(), int(ob6_osc_state.getSize()));
    if (!check(std::abs(find_parameter(*ob6_osc_restore, "ob6.amp_sustain")->getValue() - .23f) < .001f &&
               find_parameter(*ob6_osc_restore, "ob6.unison_on")->getValue() > .5f, "OB-6 voice edits restore")) return 1;
    auto ob6_dry = std::unique_ptr<juce::AudioProcessor>(createPluginFilter());
    auto ob6_wet = std::unique_ptr<juce::AudioProcessor>(createPluginFilter());
    for (auto& enabled : ob6_preset.patch.sfx_on) enabled = false;
    studio::preset_encode(ob6_preset, 3, ob6_bytes.data(), ob6_bytes.size());
    if (!check(dynamic_cast<gf::PresetImport*>(ob6_dry.get())->import_preset(ob6_bytes.data(), ob6_bytes.size()), "OB-6 dry import")) return 1;
    ob6_preset.patch.sfx_on[GF_SFX_DRIVE] = true;
    ob6_preset.patch.sfx_mix[GF_SFX_DRIVE] = 1.0f;
    ob6_preset.patch.sfx_p[GF_SFX_DRIVE][0] = .9f;
    ob6_preset.patch.sfx_p[GF_SFX_DRIVE][1] = .7f;
    studio::preset_encode(ob6_preset, 3, ob6_bytes.data(), ob6_bytes.size());
    if (!check(dynamic_cast<gf::PresetImport*>(ob6_wet.get())->import_preset(ob6_bytes.data(), ob6_bytes.size()), "OB-6 wet import")) return 1;
    juce::MemoryBlock wet_state;
    ob6_wet->getStateInformation(wet_state);
    auto ob6_wet_restore = std::unique_ptr<juce::AudioProcessor>(createPluginFilter());
    ob6_wet_restore->setStateInformation(wet_state.getData(), int(wet_state.getSize()));
    ob6_dry->prepareToPlay(44100, 128); ob6_wet->prepareToPlay(44100, 128);
    ob6_wet_restore->prepareToPlay(44100, 128);
    juce::AudioBuffer<float> wet_restored_audio(2, 128);
    double fx_difference = 0;
    for (int block = 0; block < 40; ++block) {
        juce::MidiBuffer dry_events, wet_events, restored_events;
        if (block == 0) {
            dry_events.addEvent(juce::MidiMessage::noteOn(1, 60, uint8_t(100)), 0);
            wet_events.addEvent(juce::MidiMessage::noteOn(1, 60, uint8_t(100)), 0);
            restored_events.addEvent(juce::MidiMessage::noteOn(1, 60, uint8_t(100)), 0);
        }
        ob6_dry->processBlock(ob6_audio, dry_events); ob6_wet->processBlock(ob6_restored_audio, wet_events);
        ob6_wet_restore->processBlock(wet_restored_audio, restored_events);
        for (int i = 0; i < 128; ++i) {
            const float dry = ob6_audio.getSample(0, i), wet = ob6_restored_audio.getSample(0, i);
            if (!check(std::isfinite(dry) && std::isfinite(wet), "OB-6 FX finite")) return 1;
            if (!check(std::abs(wet - wet_restored_audio.getSample(0, i)) < 1e-7f, "OB-6 FX state restores")) return 1;
            fx_difference += std::abs(dry - wet);
        }
    }
    if (!check(fx_difference > .01, "OB-6 GFP drive changes audio")) return 1;
    auto* ob6_drive_on = find_parameter(*ob6_wet, "ob6.fx.drive.on");
    if (!check(ob6_drive_on && ob6_drive_on->getValue() > .5f &&
               find_parameter(*ob6_wet, "juno.fx.drive.on")->getValue() < .5f, "OB-6 FX parameters are independent")) return 1;
    ob6_drive_on->setValueNotifyingHost(0);
    juce::MemoryBlock ob6_fx_state;
    ob6_wet->getStateInformation(ob6_fx_state);
    auto ob6_fx_restore = std::unique_ptr<juce::AudioProcessor>(createPluginFilter());
    ob6_fx_restore->setStateInformation(ob6_fx_state.getData(), int(ob6_fx_state.getSize()));
    if (!check(find_parameter(*ob6_fx_restore, "ob6.fx.drive.on")->getValue() < .5f, "OB-6 FX automation restores")) return 1;
    std::vector<uint8_t> ob6_fx_export;
    if (!check(dynamic_cast<gf::PresetImport*>(ob6_wet.get())->export_ob6_preset(ob6_fx_export), "OB-6 FX export")) return 1;
    studio::Preset ob6_fx_decoded;
    uint8_t ob6_fx_engine = 0;
    if (!check(gf::decode_supported_preset(ob6_fx_export.data(), ob6_fx_export.size(), ob6_fx_decoded, ob6_fx_engine) &&
               ob6_fx_engine == 3 && !ob6_fx_decoded.patch.sfx_on[GF_SFX_DRIVE], "OB-6 FX automation exports")) return 1;
    auto ob6_fx_legacy_xml = juce::AudioProcessor::getXmlFromBinary(ob6_fx_state.getData(), int(ob6_fx_state.getSize()));
    ob6_fx_legacy_xml->removeAttribute("ob6FxVersion");
    juce::MemoryBlock ob6_fx_legacy_state;
    juce::AudioProcessor::copyXmlToBinary(*ob6_fx_legacy_xml, ob6_fx_legacy_state);
    auto ob6_fx_legacy = std::unique_ptr<juce::AudioProcessor>(createPluginFilter());
    ob6_fx_legacy->setStateInformation(ob6_fx_legacy_state.getData(), int(ob6_fx_legacy_state.getSize()));
    if (!check(find_parameter(*ob6_fx_legacy, "ob6.fx.drive.on")->getValue() > .5f, "OB-6 legacy FX migrate from GFP")) return 1;
    ob6_wet->prepareToPlay(44100, 128); ob6_dry->prepareToPlay(44100, 128);
    juce::MidiBuffer fx_edited_note, fx_dry_note;
    fx_edited_note.addEvent(juce::MidiMessage::noteOn(1, 60, uint8_t(100)), 0);
    fx_dry_note.addEvent(juce::MidiMessage::noteOn(1, 60, uint8_t(100)), 0);
    ob6_wet->processBlock(ob6_audio, fx_edited_note); ob6_dry->processBlock(ob6_restored_audio, fx_dry_note);
    for (int i = 0; i < 128; ++i)
        if (!check(std::abs(ob6_audio.getSample(0, i) - ob6_restored_audio.getSample(0, i)) < 1e-7f,
                   "OB-6 FX automation bypasses drive")) return 1;
    for (auto& enabled : ob6_preset.patch.sfx_on) enabled = false;
    ob6_preset.patch.groove_on[GF_GROOVE_PUMP] = true;
    ob6_preset.patch.groove[GF_GROOVE_PUMP][GF_PUMP_DEPTH] = 100;
    ob6_preset.patch.groove[GF_GROOVE_PUMP][GF_PUMP_SHAPE] = 2;
    studio::preset_encode(ob6_preset, 3, ob6_bytes.data(), ob6_bytes.size());
    auto ob6_pump_a = std::unique_ptr<juce::AudioProcessor>(createPluginFilter());
    auto ob6_pump_b = std::unique_ptr<juce::AudioProcessor>(createPluginFilter());
    TestPlayHead ob6_beat(0.0), ob6_later(.25);
    ob6_pump_a->setPlayHead(&ob6_beat); ob6_pump_b->setPlayHead(&ob6_later);
    for (auto* processor : {ob6_pump_a.get(), ob6_pump_b.get()}) {
        processor->prepareToPlay(48000, 128);
        if (!check(dynamic_cast<gf::PresetImport*>(processor)->import_preset(ob6_bytes.data(), ob6_bytes.size()), "OB-6 PUMP import")) return 1;
    }
    juce::MidiBuffer pump_notes_a, pump_notes_b;
    pump_notes_a.addEvent(juce::MidiMessage::noteOn(1, 60, uint8_t(100)), 0);
    pump_notes_b.addEvent(juce::MidiMessage::noteOn(1, 60, uint8_t(100)), 0);
    ob6_pump_a->processBlock(ob6_audio, pump_notes_a); ob6_pump_b->processBlock(ob6_restored_audio, pump_notes_b);
    float ob6_pump_difference = 0;
    for (int i = 0; i < 128; ++i) ob6_pump_difference += std::abs(ob6_audio.getSample(0, i) - ob6_restored_audio.getSample(0, i));
    if (!check(ob6_pump_difference > .001f, "OB-6 PUMP follows transport")) return 1;
    auto* ob6_pump_on = find_parameter(*ob6_pump_a, "ob6.groove.pump.on");
    auto* juno_pump_on = find_parameter(*ob6_pump_a, "juno.groove.pump.on");
    if (!check(ob6_pump_on && ob6_pump_on->getValue() > .5f && juno_pump_on && juno_pump_on->getValue() < .5f,
               "OB-6 GROOVE import and engine isolation")) return 1;
    ob6_pump_on->setValueNotifyingHost(0);
    juce::MemoryBlock ob6_groove_state;
    ob6_pump_a->getStateInformation(ob6_groove_state);
    auto ob6_groove_restore = std::unique_ptr<juce::AudioProcessor>(createPluginFilter());
    ob6_groove_restore->setStateInformation(ob6_groove_state.getData(), int(ob6_groove_state.getSize()));
    auto* restored_pump_on = find_parameter(*ob6_groove_restore, "ob6.groove.pump.on");
    if (!check(restored_pump_on && restored_pump_on->getValue() < .5f, "OB-6 GROOVE automation restores")) return 1;
    ob6_groove_restore->setPlayHead(&ob6_beat);
    ob6_groove_restore->prepareToPlay(48000, 128);
    ob6_pump_a->prepareToPlay(48000, 128);
    juce::MidiBuffer automation_notes, restore_notes;
    automation_notes.addEvent(juce::MidiMessage::noteOn(1, 60, uint8_t(100)), 0);
    restore_notes.addEvent(juce::MidiMessage::noteOn(1, 60, uint8_t(100)), 0);
    ob6_pump_a->processBlock(ob6_audio, automation_notes);
    ob6_groove_restore->processBlock(ob6_restored_audio, restore_notes);
    for (int i = 0; i < 128; ++i)
        if (std::abs(ob6_audio.getSample(0, i) - ob6_restored_audio.getSample(0, i)) >= 1e-7f) { std::fprintf(stderr, "OB-6 restore mismatch %d %.9f %.9f\n", i, ob6_audio.getSample(0, i), ob6_restored_audio.getSample(0, i)); return 1; }
    ob6_pump_b->setPlayHead(&ob6_beat); ob6_pump_b->prepareToPlay(48000, 128);
    juce::MidiBuffer enabled_note;
    enabled_note.addEvent(juce::MidiMessage::noteOn(1, 60, uint8_t(100)), 0);
    ob6_pump_b->processBlock(ob6_restored_audio, enabled_note);
    float ob6_automation_difference = 0;
    for (int i = 0; i < 128; ++i) ob6_automation_difference += std::abs(ob6_audio.getSample(0, i) - ob6_restored_audio.getSample(0, i));
    if (!check(ob6_automation_difference > .001f, "OB-6 PUMP automation changes audio")) return 1;
    std::vector<uint8_t> ob6_exported;
    if (!check(dynamic_cast<gf::PresetImport*>(ob6_pump_a.get())->export_ob6_preset(ob6_exported), "OB-6 GFP export")) return 1;
    studio::Preset ob6_exported_preset;
    uint8_t ob6_exported_engine = 0;
    if (!check(gf::decode_supported_preset(ob6_exported.data(), ob6_exported.size(), ob6_exported_preset, ob6_exported_engine) &&
               ob6_exported_engine == 3 && !ob6_exported_preset.patch.groove_on[GF_GROOVE_PUMP] &&
               ob6_exported_preset.patch.groove[GF_GROOVE_MACRO][GF_MACRO_GRIT] == 0 &&
               ob6_exported_preset.patch.groove[GF_GROOVE_MACRO][GF_MACRO_POCKET] == 0 &&
               ob6_exported_preset.patch.groove[GF_GROOVE_MACRO][GF_MACRO_MOVE] == 0,
               "OB-6 GFP preserves edits without double macros")) return 1;
    auto ob6_export_import = std::unique_ptr<juce::AudioProcessor>(createPluginFilter());
    if (!check(dynamic_cast<gf::PresetImport*>(ob6_export_import.get())->import_preset(ob6_exported.data(), ob6_exported.size()),
               "OB-6 exported GFP imports")) return 1;
    if (!check(find_parameter(*ob6_export_import, "ob6.groove.pump.on")->getValue() < .5f,
               "OB-6 exported GROOVE edit imports")) return 1;
    auto ob6_legacy_xml = juce::AudioProcessor::getXmlFromBinary(ob6_groove_state.getData(), int(ob6_groove_state.getSize()));
    ob6_legacy_xml->removeAttribute("ob6GrooveVersion");
    juce::MemoryBlock ob6_legacy_state;
    juce::AudioProcessor::copyXmlToBinary(*ob6_legacy_xml, ob6_legacy_state);
    auto ob6_legacy = std::unique_ptr<juce::AudioProcessor>(createPluginFilter());
    ob6_legacy->setStateInformation(ob6_legacy_state.getData(), int(ob6_legacy_state.getSize()));
    if (!check(find_parameter(*ob6_legacy, "ob6.groove.pump.on")->getValue() > .5f,
               "OB-6 legacy state migrates GFP groove")) return 1;
    ob6_preset.patch.groove_on[GF_GROOVE_PUMP] = false;
    ob6_preset.patch.groove_on[GF_GROOVE_ARP] = true;
    ob6_preset.patch.groove[GF_GROOVE_ARP][GF_ARP_GATE] = 60;
    ob6_preset.patch.groove[GF_GROOVE_ARP][GF_ARP_OCT] = 1;
    ob6_preset.patch.groove[GF_GROOVE_ARP][GF_ARP_LATCH] = 0;
    studio::preset_encode(ob6_preset, 3, ob6_bytes.data(), ob6_bytes.size());
    auto ob6_arp = std::unique_ptr<juce::AudioProcessor>(createPluginFilter());
    ob6_arp->prepareToPlay(48000, 128);
    if (!check(dynamic_cast<gf::PresetImport*>(ob6_arp.get())->import_preset(ob6_bytes.data(), ob6_bytes.size()), "OB-6 ARP import")) return 1;
    float ob6_arp_peak = 0;
    for (int block = 0; block < 160; ++block) {
        juce::MidiBuffer events;
        if (block == 0) events.addEvent(juce::MidiMessage::noteOn(1, 60, uint8_t(100)), 0);
        if (block == 40) {
            events.addEvent(juce::MidiMessage::controllerEvent(1, 64, 127), 0);
            events.addEvent(juce::MidiMessage::noteOff(1, 60), 1);
        }
        if (block == 100) events.addEvent(juce::MidiMessage::controllerEvent(1, 64, 0), 0);
        ob6_arp->processBlock(ob6_audio, events);
        if (block >= 50 && block < 100) ob6_arp_peak = std::max(ob6_arp_peak, ob6_audio.getRMSLevel(0, 0, 128));
    }
    if (!check(ob6_arp_peak > .00001f, "OB-6 ARP sustain produces audio")) return 1;
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
        auto plugin = std::unique_ptr<juce::AudioProcessor>(createPluginFilter());
        plugin->prepareToPlay(44100, 512);
        auto editor = std::unique_ptr<juce::AudioProcessorEditor>(plugin->createEditor());
        auto* toggle = dynamic_cast<juce::TextButton*>(editor->findChildWithID("keyboard.toggle"));
        auto* keyboard = dynamic_cast<StudioKeyboard*>(editor->findChildWithID("keyboard.keys"));
        auto* browser = dynamic_cast<juce::ComboBox*>(editor->findChildWithID("preset.browser"));
        if (!check(toggle && keyboard && browser && !keyboard->isVisible() && browser->getNumItems() > 0, "keyboard and factory browser")) return 1;
        browser->setSelectedId(1, juce::sendNotificationSync);
        if (!check(browser->getSelectedId() == 1, "factory browser selects a preset")) return 1;
        juce::MemoryBlock browser_state;
        plugin->getStateInformation(browser_state);
        auto browser_xml = juce::AudioProcessor::getXmlFromBinary(browser_state.getData(), int(browser_state.getSize()));
        auto browser_tree = browser_xml ? juce::ValueTree::fromXml(*browser_xml) : juce::ValueTree();
        if (!check(browser_tree.isValid() && browser_tree.hasProperty("gfPreset"), "factory browser loads preset state")) return 1;
        auto* open = dynamic_cast<juce::TextButton*>(editor->findChildWithID("browser.open"));
        auto* panel = dynamic_cast<PresetBrowser*>(editor->findChildWithID("browser.panel"));
        auto* search = dynamic_cast<juce::TextEditor*>(panel ? panel->findChildWithID("browser.search") : nullptr);
        auto* list = dynamic_cast<juce::ListBox*>(panel ? panel->findChildWithID("browser.list") : nullptr);
        if (!check(open && panel && search && list, "preset browser controls")) return 1;
        open->onClick();
        if (!check(panel->isVisible() && list->getModel()->getNumRows() == browser->getNumItems(), "preset browser opens bank")) return 1;
        search->setText("zzzz-not-a-preset", false); search->onTextChange();
        if (!check(list->getModel()->getNumRows() == 0, "preset browser filters results")) return 1;
        search->setText(browser->getItemText(0), false); search->onTextChange();
        if (!check(list->getModel()->getNumRows() > 0, "preset browser finds preset")) return 1;
        const auto now_browser = juce::Time::getCurrentTime();
        juce::MouseEvent browser_event(juce::Desktop::getInstance().getMainMouseSource(), {55.0f, 5.0f}, juce::ModifierKeys::leftButtonModifier,
                                       1.0f, 0.0f, 0.0f, 0.0f, 0.0f, list, list, now_browser, {5.0f, 5.0f}, now_browser, 1, false);
        list->getModel()->listBoxItemClicked(0, browser_event);
        if (!check(!panel->isVisible() && browser->getSelectedId() == 1, "preset browser loads filtered bank index")) return 1;
        open->onClick();
        search->setText("", false); search->onTextChange();
        const float preview_x = float(list->getWidth() - 10);
        juce::MouseEvent preview_event(juce::Desktop::getInstance().getMainMouseSource(), {preview_x, 5.0f}, juce::ModifierKeys::leftButtonModifier,
                                       1.0f, 0.0f, 0.0f, 0.0f, 0.0f, list, list, now_browser, {preview_x, 5.0f}, now_browser, 1, false);
        list->getModel()->listBoxItemClicked(0, preview_event);
        auto* preview_cancel = dynamic_cast<juce::TextButton*>(panel->findChildWithID("browser.preview.cancel"));
        if (!check(preview_cancel && preview_cancel->isVisible() && panel->isVisible(), "browser starts isolated preview")) return 1;
        preview_cancel->onClick();
        if (!check(!preview_cancel->isVisible(), "browser cancels preview")) return 1;
        juce::MouseEvent star_event(juce::Desktop::getInstance().getMainMouseSource(), {5.0f, 5.0f}, juce::ModifierKeys::leftButtonModifier,
                                    1.0f, 0.0f, 0.0f, 0.0f, 0.0f, list, list, now_browser, {5.0f, 5.0f}, now_browser, 1, false);
        list->getModel()->listBoxItemClicked(0, star_event);
        juce::MouseEvent tag_event(juce::Desktop::getInstance().getMainMouseSource(), {35.0f, 5.0f}, juce::ModifierKeys::leftButtonModifier,
                                   1.0f, 0.0f, 0.0f, 0.0f, 0.0f, list, list, now_browser, {35.0f, 5.0f}, now_browser, 1, false);
        list->getModel()->listBoxItemClicked(0, tag_event);
        auto* tag_editor = dynamic_cast<juce::TextEditor*>(panel->findChildWithID("browser.tag.editor"));
        auto* tag_apply = dynamic_cast<juce::TextButton*>(panel->findChildWithID("browser.tag.apply"));
        if (!check(tag_editor && tag_apply && tag_editor->isVisible(), "preset tag editor opens")) return 1;
        tag_editor->setText("afro, bajo", false); tag_apply->onClick();
        search->setText("afro", false); search->onTextChange();
        if (!check(list->getModel()->getNumRows() == 1 && panel->isVisible(), "preset tag search without loading")) return 1;
        search->setText("", false); search->onTextChange();
        auto* favourites = dynamic_cast<juce::ToggleButton*>(panel->findChildWithID("browser.favourites"));
        if (!check(favourites && panel->isVisible(), "favourite toggles without loading")) return 1;
        favourites->setToggleState(true, juce::dontSendNotification); favourites->onClick();
        if (!check(list->getModel()->getNumRows() == 1, "browser favourites filter")) return 1;
        juce::MemoryBlock favourite_state; plugin->getStateInformation(favourite_state);
        auto restored = std::unique_ptr<juce::AudioProcessor>(createPluginFilter());
        restored->setStateInformation(favourite_state.getData(), int(favourite_state.getSize()));
        auto restored_editor = std::unique_ptr<juce::AudioProcessorEditor>(restored->createEditor());
        auto* restored_open = dynamic_cast<juce::TextButton*>(restored_editor->findChildWithID("browser.open"));
        auto* restored_panel = dynamic_cast<PresetBrowser*>(restored_editor->findChildWithID("browser.panel"));
        if (!check(restored_open && restored_panel, "restored browser exists")) return 1;
        restored_open->onClick();
        auto* restored_filter = dynamic_cast<juce::ToggleButton*>(restored_panel->findChildWithID("browser.favourites"));
        auto* restored_list = dynamic_cast<juce::ListBox*>(restored_panel->findChildWithID("browser.list"));
        if (!check(restored_filter && restored_list, "restored favourites controls")) return 1;
        restored_filter->setToggleState(true, juce::dontSendNotification); restored_filter->onClick();
        if (!check(restored_list->getModel()->getNumRows() == 1, "favourites restored with project")) return 1;
        auto* restored_search = dynamic_cast<juce::TextEditor*>(restored_panel->findChildWithID("browser.search"));
        if (!check(restored_search != nullptr, "restored preset search")) return 1;
        restored_search->setText("afro", false); restored_search->onTextChange();
        if (!check(restored_list->getModel()->getNumRows() == 1, "preset tags restored with project")) return 1;
        favourites->setToggleState(false, juce::dontSendNotification); favourites->onClick();
        int chosen_index = -1;
        panel->set_items(juce::StringArray{"FIRST", "SECOND", "THIRD"});
        panel->on_load = [&chosen_index](int index) { chosen_index = index; };
        search->setText("THIRD", false); search->onTextChange();
        list->getModel()->listBoxItemClicked(0, browser_event);
        if (!check(chosen_index == 2, "preset browser preserves original bank index")) return 1;
        toggle->onClick();
        if (!check(keyboard->isVisible() && editor->getHeight() == 960, "keyboard expands editor")) return 1;
        keyboard->midi_state().noteOn(1, 60, 0.8f);
        juce::AudioBuffer<float> audio(2, 512); juce::MidiBuffer midi;
        plugin->processBlock(audio, midi);
        if (!check(audio.getRMSLevel(0, 0, 512) > 0.0f, "screen keyboard produces sound")) return 1;
        keyboard->midi_state().noteOff(1, 60, 0.0f);
        toggle->onClick();
        if (!check(!keyboard->isVisible() && editor->getHeight() == 760, "keyboard collapses editor")) return 1;
    }
    {
        auto plugin = std::unique_ptr<juce::AudioProcessor>(createPluginFilter());
        auto reference = std::unique_ptr<juce::AudioProcessor>(createPluginFilter());
        for (auto* p : {plugin.get(), reference.get()}) {
            p->prepareToPlay(44100, 512);
            find_parameter(*p, "cutoff")->setValueNotifyingHost(0.31f);
        }
        auto editor = std::unique_ptr<juce::AudioProcessorEditor>(plugin->createEditor());
        auto* panel = dynamic_cast<PresetBrowser*>(editor->findChildWithID("browser.panel"));
        if (!check(panel && panel->on_preview && panel->on_cancel_preview, "isolated preset preview controls")) return 1;
        if (!check(panel->on_preview(0), "isolated preset preview starts")) return 1;
        if (!check(std::abs(find_parameter(*plugin, "cutoff")->getValue() - .31f) < .001f, "preview preserves original parameter")) return 1;
        juce::AudioBuffer<float> audio(2, 512), ref_audio(2, 512);
        juce::MidiBuffer midi, ref_midi;
        midi.addEvent(juce::MidiMessage::noteOn(1, 60, uint8_t(120)), 0);
        ref_midi.addEvent(juce::MidiMessage::noteOn(1, 60, uint8_t(120)), 0);
        plugin->processBlock(audio, midi); reference->processBlock(ref_audio, ref_midi);
        double difference = 0;
        for (int i = 0; i < 512; ++i) difference += std::abs(audio.getSample(0, i) - ref_audio.getSample(0, i));
        if (!check(difference > .001, "preview outputs the auditioned preset")) return 1;
        find_parameter(*plugin, "cutoff")->setValueNotifyingHost(.57f);
        find_parameter(*reference, "cutoff")->setValueNotifyingHost(.57f);
        panel->on_cancel_preview(); midi.clear(); ref_midi.clear();
        plugin->processBlock(audio, midi); reference->processBlock(ref_audio, ref_midi);
        difference = 0;
        for (int i = 0; i < 512; ++i) difference += std::abs(audio.getSample(0, i) - ref_audio.getSample(0, i));
        if (!check(difference < .0001 && std::abs(find_parameter(*plugin, "cutoff")->getValue() - .57f) < .001f,
                   "cancel preview restores live output and keeps automation")) return 1;
    }
    {
        OutputMeterState state;
        state.publish(0.25f, 1.1f); state.publish(0.5f, 0.1f);
        if (!check(state.consume(0) == 0.5f && state.consume(1) == 1.1f && state.clipped(1) && !state.clipped(0), "output meter peak and clip latch")) return 1;
        if (!check(state.consume(0) == 0.0f, "output meter consumes peak")) return 1;
        state.clear_clip();
        if (!check(!state.clipped(1), "output meter clears clip")) return 1;
        auto plugin = std::unique_ptr<juce::AudioProcessor>(createPluginFilter());
        plugin->prepareToPlay(44100, 512);
        auto editor = std::unique_ptr<juce::AudioProcessorEditor>(plugin->createEditor());
        auto* meter = dynamic_cast<OutputMeter*>(editor->findChildWithID("output.meter"));
        juce::AudioBuffer<float> audio(2, 512); juce::MidiBuffer notes;
        notes.addEvent(juce::MidiMessage::noteOn(1, 60, uint8_t(120)), 0);
        plugin->processBlock(audio, notes);
        if (!check(meter != nullptr, "output meter component")) return 1;
        meter->refresh();
        if (!check(meter->level(0) > 0.0f && meter->level(1) > 0.0f, "output meter follows rendered audio")) return 1;
    }
    {
        auto plugin = std::unique_ptr<juce::AudioProcessor>(createPluginFilter());
        find_parameter(*plugin, "juno_preview")->setValueNotifyingHost(1);
        auto editor = std::unique_ptr<juce::AudioProcessorEditor>(plugin->createEditor());
        auto* env_tab = dynamic_cast<juce::TextButton*>(editor->findChildWithID("navigation.tab.2"));
        auto* graph = dynamic_cast<EnvelopeEditor*>(editor->findChildWithID("juno.env.graph.filter"));
        auto* attack = find_parameter(*plugin, "juno.filter_attack");
        if (!check(env_tab && graph && attack, "Juno envelope controls")) return 1;
        env_tab->onClick();
        if (!check(graph->isVisible(), "Juno envelope graph visible")) return 1;
        const auto start = graph->handles()[0], end = start.translated(45.0f, 0.0f);
        const auto now = juce::Time::getCurrentTime();
        auto source = juce::Desktop::getInstance().getMainMouseSource();
        juce::MouseEvent down(source, start, juce::ModifierKeys::leftButtonModifier, 1.0f, 0, 0, 0, 0, graph, graph, now, start, now, 1, false);
        juce::MouseEvent drag(source, end, juce::ModifierKeys::leftButtonModifier, 1.0f, 0, 0, 0, 0, graph, graph, now, start, now, 1, true);
        const auto before = attack->getValue(); graph->mouseDown(down); graph->mouseDrag(drag); graph->mouseUp(drag);
        if (!check(attack->getValue() > before, "dragging envelope edits host attack")) return 1;
        auto* lfo_tab = dynamic_cast<juce::TextButton*>(editor->findChildWithID("navigation.tab.3"));
        auto* lfo = dynamic_cast<LfoDisplay*>(editor->findChildWithID("juno.lfo.display"));
        auto* shape = find_parameter(*plugin, "juno.lfo_shape");
        if (!check(lfo_tab && lfo && shape, "Juno LFO display exists")) return 1;
        lfo_tab->onClick();
        if (!check(lfo->isVisible() && !graph->isVisible(), "Juno LFO page visibility")) return 1;
        shape->setValueNotifyingHost(1.0f);
        juce::MouseEvent click(source, {10.0f, 10.0f}, juce::ModifierKeys::leftButtonModifier, 1.0f, 0, 0, 0, 0, lfo, lfo, now, {10.0f, 10.0f}, now, 1, false);
        lfo->mouseDown(click);
        if (!check(lfo->shape() == 0 && shape->getValue() < .01f, "Juno LFO display edits shape and wraps")) return 1;
    }
    {
        auto plugin = std::unique_ptr<juce::AudioProcessor>(createPluginFilter());
        find_parameter(*plugin, "hybrid_preview")->setValueNotifyingHost(1);
        auto editor = std::unique_ptr<juce::AudioProcessorEditor>(plugin->createEditor());
        for (int side = 0; side < 2; ++side) {
            auto* tab = dynamic_cast<juce::TextButton*>(editor->findChildWithID("navigation.tab." + juce::String(5 + side)));
            const auto prefix = juce::String(side == 0 ? "hybrid.a." : "hybrid.b.");
            auto* graph = dynamic_cast<EnvelopeEditor*>(editor->findChildWithID(prefix + "env.graph.amp"));
            auto* parameter = find_parameter(*plugin, (prefix + "attack").toRawUTF8());
            auto* other = find_parameter(*plugin, side == 0 ? "hybrid.b.attack" : "hybrid.a.attack");
            if (!check(tab && graph && parameter && other, "HYBRID envelope controls")) return 1;
            tab->onClick();
            if (!check(graph->isVisible(), "HYBRID layer envelope visible")) return 1;
            const auto start = graph->handles()[0], end = start.translated(45.0f, 0.0f);
            const auto now = juce::Time::getCurrentTime();
            auto source = juce::Desktop::getInstance().getMainMouseSource();
            juce::MouseEvent down(source, start, juce::ModifierKeys::leftButtonModifier, 1.0f, 0, 0, 0, 0, graph, graph, now, start, now, 1, false);
            juce::MouseEvent drag(source, end, juce::ModifierKeys::leftButtonModifier, 1.0f, 0, 0, 0, 0, graph, graph, now, start, now, 1, true);
            const auto before = parameter->getValue(), other_before = other->getValue();
            graph->mouseDown(down); graph->mouseDrag(drag); graph->mouseUp(drag);
            if (!check(parameter->getValue() > before && other->getValue() == other_before, "HYBRID envelope edits only its layer")) return 1;
            auto* lfo_tab = dynamic_cast<juce::TextButton*>(editor->findChildWithID("navigation.tab." + juce::String(9 + side)));
            auto* lfo = dynamic_cast<LfoDisplay*>(editor->findChildWithID(prefix + "lfo.display"));
            auto* shape = find_parameter(*plugin, (prefix + "voice.lfo_shape").toRawUTF8());
            auto* other_shape = find_parameter(*plugin, side == 0 ? "hybrid.b.voice.lfo_shape" : "hybrid.a.voice.lfo_shape");
            if (!check(lfo_tab && lfo && shape && other_shape, "HYBRID LFO controls")) return 1;
            lfo_tab->onClick();
            if (!check(lfo->isVisible() && !graph->isVisible(), "HYBRID LFO page")) return 1;
            shape->setValueNotifyingHost(1.0f); const auto other_shape_before = other_shape->getValue();
            juce::MouseEvent click(source, {10.0f, 10.0f}, juce::ModifierKeys::leftButtonModifier, 1.0f, 0, 0, 0, 0, lfo, lfo, now, {10.0f, 10.0f}, now, 1, false);
            lfo->mouseDown(click);
            if (!check(lfo->shape() == 0 && other_shape->getValue() == other_shape_before, "HYBRID LFO edits only its layer")) return 1;
        }
    }
    for (const auto& engine : std::array<std::array<const char*, 4>, 4>{{
             {{"ob6_preview", "ob6.env.graph.filter", "ob6.filter_attack", "2"}},
             {{"modeld_preview", "modeld.env.graph.filter", "modeld.filter_attack", "2"}},
             {{"arp2600_preview", "arp2600.env.graph.filter", "arp2600.filter_attack", "2"}},
             {{"dx7_preview", "dx7.env.graph.amp", "dx7.amp_attack", "1"}}}}) {
        auto plugin = std::unique_ptr<juce::AudioProcessor>(createPluginFilter());
        find_parameter(*plugin, engine[0])->setValueNotifyingHost(1);
        auto editor = std::unique_ptr<juce::AudioProcessorEditor>(plugin->createEditor());
        auto* tab = dynamic_cast<juce::TextButton*>(editor->findChildWithID(juce::String("navigation.tab.") + engine[3]));
        auto* graph = dynamic_cast<EnvelopeEditor*>(editor->findChildWithID(engine[1]));
        auto* parameter = find_parameter(*plugin, engine[2]);
        if (!check(tab && graph && parameter, "analog envelope controls")) return 1;
        tab->onClick();
        if (!check(graph->isVisible(), "analog envelope visible")) return 1;
        const auto start = graph->handles()[0], end = start.translated(45.0f, 0.0f);
        const auto now = juce::Time::getCurrentTime();
        auto source = juce::Desktop::getInstance().getMainMouseSource();
        juce::MouseEvent down(source, start, juce::ModifierKeys::leftButtonModifier, 1.0f, 0, 0, 0, 0, graph, graph, now, start, now, 1, false);
        juce::MouseEvent drag(source, end, juce::ModifierKeys::leftButtonModifier, 1.0f, 0, 0, 0, 0, graph, graph, now, start, now, 1, true);
        const auto before = parameter->getValue(); graph->mouseDown(down); graph->mouseDrag(drag); graph->mouseUp(drag);
        if (!check(parameter->getValue() > before, "analog envelope edits engine parameter")) return 1;
        if (juce::String(engine[3]) == "1") {
            auto* lfo_tab = dynamic_cast<juce::TextButton*>(editor->findChildWithID("navigation.tab.5"));
            auto* lfo = dynamic_cast<LfoDisplay*>(editor->findChildWithID("dx7.lfo.display"));
            auto* shape = find_parameter(*plugin, "dx7.lfo_shape");
            if (!check(lfo_tab && lfo && shape, "DX7 LFO controls")) return 1;
            lfo_tab->onClick();
            if (!check(lfo->isVisible() && !graph->isVisible(), "DX7 LFO page")) return 1;
            shape->setValueNotifyingHost(1.0f);
            juce::MouseEvent click(source, {10.0f, 10.0f}, juce::ModifierKeys::leftButtonModifier, 1.0f, 0, 0, 0, 0, lfo, lfo, now, {10.0f, 10.0f}, now, 1, false);
            lfo->mouseDown(click);
            if (!check(lfo->shape() == 0, "DX7 LFO changes shape")) return 1;
        }
        if (juce::String(engine[3]) == "2") {
            const auto prefix = juce::String(engine[1]).upToFirstOccurrenceOf(".env.", false, false);
            auto* lfo_tab = dynamic_cast<juce::TextButton*>(editor->findChildWithID("navigation.tab.6"));
            auto* lfo = dynamic_cast<LfoDisplay*>(editor->findChildWithID(prefix + ".lfo.display"));
            auto* shape = find_parameter(*plugin, (prefix + ".lfo_shape").toRawUTF8());
            if (!check(lfo_tab && lfo && shape, "analog LFO controls")) return 1;
            lfo_tab->onClick();
            if (!check(lfo->isVisible() && !graph->isVisible(), "analog LFO page")) return 1;
            shape->setValueNotifyingHost(1.0f);
            juce::MouseEvent click(source, {10.0f, 10.0f}, juce::ModifierKeys::leftButtonModifier, 1.0f, 0, 0, 0, 0, lfo, lfo, now, {10.0f, 10.0f}, now, 1, false);
            lfo->mouseDown(click);
            if (!check(lfo->shape() == 0 && shape->getValue() < .01f, "analog LFO edits engine shape")) return 1;
        }
    }
    {
        auto dx_plugin = std::unique_ptr<juce::AudioProcessor>(createPluginFilter());
        find_parameter(*dx_plugin, "dx7_preview")->setValueNotifyingHost(1);
        auto editor = std::unique_ptr<juce::AudioProcessorEditor>(dx_plugin->createEditor());
        auto* groove_tab = dynamic_cast<juce::TextButton*>(editor->findChildWithID("navigation.tab.3"));
        auto* pump = dynamic_cast<juce::Slider*>(editor->findChildWithID("dx.groove.0.0"));
        auto* depth = dynamic_cast<juce::Slider*>(editor->findChildWithID("dx.groove.0.1"));
        if (!check(groove_tab && pump && depth, "DX7 groove controls")) return 1;
        groove_tab->onClick();
        if (!check(pump->isVisible() && depth->isVisible() && pump->getWidth() > 0 && depth->getWidth() > 0 && !pump->getBounds().intersects(depth->getBounds()), "DX7 groove layout")) return 1;
        pump->setValue(1, juce::sendNotificationSync);
        if (!check(find_parameter(*dx_plugin, "dx7.groove.pump.on")->getValue() > .99f, "DX7 groove control binding")) return 1;
    }
    {
        auto editor = std::unique_ptr<juce::AudioProcessorEditor>(a->createEditor());
        if (!check(editor && editor->getWidth() == 1200 && editor->getHeight() == 760, "editor creation")) return 1;
        editor->setSize(900, 570);
        if (!check(editor->getWidth() == 900 && editor->getHeight() == 570, "editor resize")) return 1;
    }
    return 0;
}
