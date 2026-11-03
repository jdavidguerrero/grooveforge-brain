#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include <dsp/lfo.h>
#include <atomic>
#include <cmath>

/** Representative LFO waveform linked to an engine's real shape parameter. */
class LfoDisplay final : public juce::Component, public juce::SettableTooltipClient, private juce::Timer {
public:
    LfoDisplay(juce::AudioProcessorValueTreeState& state, const char* shape_id, const char* rate_id)
        : shape_(state.getParameter(shape_id)), shape_value_(state.getRawParameterValue(shape_id)),
          rate_(state.getRawParameterValue(rate_id)), sync_(state.getRawParameterValue("lfo_sync")) {
        jassert(shape_ && shape_value_ && rate_ && sync_);
        setTooltip("Clic para cambiar la forma del LFO");
        startTimerHz(10);
    }
    /** Shape index corresponding to the firmware LfoShape enumeration. */
    int shape() const noexcept { return juce::jlimit(0, 4, int(std::lround(shape_value_->load(std::memory_order_relaxed)))); }
    void mouseDown(const juce::MouseEvent&) override {
        const auto next = float((shape() + 1) % 5);
        shape_->beginChangeGesture(); shape_->setValueNotifyingHost(shape_->convertTo0to1(next)); shape_->endChangeGesture();
        repaint();
    }
    void paint(juce::Graphics& g) override {
        g.fillAll(juce::Colour(0xff101918));
        g.setColour(juce::Colour(0xff242424)); g.drawRect(getLocalBounds(), 1);
        constexpr const char* names[] = {"SINE", "TRIANGLE", "SAW", "SQUARE", "SAMPLE & HOLD"};
        const auto heading = juce::String(juce::String::fromUTF8("LFO · ")) + names[shape()] + juce::String::fromUTF8("   ·   ") +
            (sync_->load(std::memory_order_relaxed) >= .5f ? juce::String("SYNC DAW") : juce::String(rate_->load(std::memory_order_relaxed), 2) + " Hz");
        g.setColour(juce::Colour(0xff9fe1cb)); g.setFont(juce::Font(juce::FontOptions(10.0f, juce::Font::bold)));
        g.drawText(heading, 10, 3, getWidth() - 20, 17, juce::Justification::left);
        const auto area = getLocalBounds().toFloat().withTrimmedTop(24).reduced(10, 8);
        dsp::Lfo preview; preview.set_rate_hz(1.0f); preview.set_shape(static_cast<dsp::LfoShape>(shape()));
        juce::Path path;
        constexpr int steps = 200;
        for (int i = 0; i <= steps; ++i) {
            const float sample = preview.tick(2.0f / float(steps));
            const float x = area.getX() + area.getWidth() * float(i) / float(steps);
            const float y = area.getCentreY() - sample * area.getHeight() * 0.45f;
            if (i == 0) path.startNewSubPath(x, y); else path.lineTo(x, y);
        }
        g.setColour(juce::Colour(0xff5dcaa5)); g.strokePath(path, juce::PathStrokeType(2.0f));
    }
private:
    void timerCallback() override { if (isVisible()) repaint(); }
    juce::RangedAudioParameter* shape_;
    std::atomic<float>* shape_value_;
    std::atomic<float>* rate_;
    std::atomic<float>* sync_;
};
