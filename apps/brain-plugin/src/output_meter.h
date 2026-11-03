#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include <array>
#include <atomic>
#include <cmath>

/** Lock-free handoff of sample peaks and latched 0 dBFS indicators to the editor. */
class OutputMeterState {
public:
    /** Publish one block's final output peaks without changing audio samples. */
    void publish(float left, float right) noexcept {
        const float values[] = {left, right};
        for (size_t i = 0; i < 2; ++i) {
            const float value = std::isfinite(values[i]) ? std::max(0.0f, values[i]) : 0.0f;
            auto previous = peaks_[i].load(std::memory_order_relaxed);
            while (value > previous && !peaks_[i].compare_exchange_weak(previous, value, std::memory_order_relaxed)) {}
            if (value >= 1.0f) clipped_[i].store(true, std::memory_order_relaxed);
        }
    }
    /** Read and clear the accumulated peak for one channel. */
    float consume(size_t channel) noexcept { return peaks_[channel].exchange(0.0f, std::memory_order_relaxed); }
    /** Return whether a sample reached 0 dBFS since the last clear. */
    bool clipped(size_t channel) const noexcept { return clipped_[channel].load(std::memory_order_relaxed); }
    /** Clear both latched indicators. */
    void clear_clip() noexcept { for (auto& value : clipped_) value.store(false, std::memory_order_relaxed); }
private:
    std::array<std::atomic<float>, 2> peaks_{};
    std::array<std::atomic<bool>, 2> clipped_{};
};

/** Stereo output meter driven only by samples actually produced by the processor. */
class OutputMeter final : public juce::Component, public juce::SettableTooltipClient, private juce::Timer {
public:
    explicit OutputMeter(OutputMeterState& state) : state_(state) {
        setComponentID("output.meter");
        setTooltip(juce::String::fromUTF8("Pico de salida L/R · clic para borrar 0 dBFS"));
        startTimerHz(30);
    }
    /** Consume new peaks and advance the visual falloff. */
    void refresh() {
        for (size_t i = 0; i < 2; ++i) levels_[i] = std::max(state_.consume(i), levels_[i] * 0.90f);
        repaint();
    }
    /** Last displayed linear peak before dB conversion. */
    float level(size_t channel) const noexcept { return levels_[channel]; }
    void mouseDown(const juce::MouseEvent&) override { state_.clear_clip(); repaint(); }
    void paint(juce::Graphics& g) override {
        auto area = getLocalBounds().reduced(2);
        const int width = juce::jmax(3, (area.getWidth() - 3) / 2);
        for (int i = 0; i < 2; ++i) {
            auto track = area.removeFromLeft(width); area.removeFromLeft(juce::jmin(3, area.getWidth()));
            g.setColour(juce::Colour(0xff242424)); g.fillRect(track);
            const float db = juce::Decibels::gainToDecibels(levels_[size_t(i)], -60.0f);
            const float norm = juce::jlimit(0.0f, 1.0f, (db + 60.0f) / 60.0f);
            auto fill = track.removeFromBottom(int(std::round(float(track.getHeight()) * norm)));
            g.setColour(juce::Colour(0xff5dcaa5)); g.fillRect(fill);
            if (state_.clipped(size_t(i))) { g.setColour(juce::Colour(0xffe84a4a)); g.fillRect(getLocalBounds().getX() + 2 + i * (width + 3), 2, width, 3); }
        }
    }
private:
    void timerCallback() override { refresh(); }
    OutputMeterState& state_;
    std::array<float, 2> levels_{};
};
