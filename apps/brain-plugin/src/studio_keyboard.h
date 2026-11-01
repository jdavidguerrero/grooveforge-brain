#pragma once

#include <juce_audio_utils/juce_audio_utils.h>

/** Keyboard view exposing its MIDI state for programmatic note control. */
class StudioKeyboard final : public juce::MidiKeyboardComponent {
public:
    explicit StudioKeyboard(juce::MidiKeyboardState& state)
        : juce::MidiKeyboardComponent(state, juce::MidiKeyboardComponent::horizontalKeyboard), state_(state) {}
    /** Access the same state used by pointer and computer-keyboard interactions. */
    juce::MidiKeyboardState& midi_state() noexcept { return state_; }
private:
    juce::MidiKeyboardState& state_;
};
