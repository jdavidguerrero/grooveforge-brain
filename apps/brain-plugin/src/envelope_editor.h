#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include <array>
#include <cmath>

/** Editable schematic ADSR tied directly to four host parameters. */
class EnvelopeEditor final : public juce::Component, public juce::SettableTooltipClient, private juce::Timer {
public:
    EnvelopeEditor(juce::AudioProcessorValueTreeState& state, const juce::String& title,
                   const std::array<const char*, 4>& ids) : title_(title) {
        for (size_t i = 0; i < ids.size(); ++i) {
            params_[i] = state.getParameter(ids[i]);
            values_[i] = state.getRawParameterValue(ids[i]);
            jassert(params_[i] != nullptr && values_[i] != nullptr);
        }
        setTooltip("Arrastra ATTACK, DECAY/SUSTAIN o RELEASE");
        startTimerHz(15);
    }
    /** Use the same time range as a second envelope. */
    void set_peer(const EnvelopeEditor* peer) noexcept { peer_ = peer; }
    /** Returns the visible positions of attack, decay/sustain and release handles. */
    std::array<juce::Point<float>, 3> handles() const {
        const auto area = plot();
        const float a = read(0), d = read(1), sustain = juce::jlimit(0.0f, 1.0f, read(2)), r = read(3);
        const float scale = duration();
        const auto x = [&](float ms) { return area.getX() + area.getWidth() * ms / scale; };
        return {{{x(a), area.getY()}, {x(a + d), area.getBottom() - sustain * area.getHeight()},
                 {x(a + d + HOLD_MS + r), area.getBottom()}}};
    }
    void paint(juce::Graphics& g) override {
        g.fillAll(juce::Colour(0xff101918));
        g.setColour(juce::Colour(0xff242424)); g.drawRect(getLocalBounds(), 1);
        g.setColour(juce::Colour(0xff9fe1cb)); g.setFont(juce::Font(juce::FontOptions(10.0f, juce::Font::bold)));
        g.drawText(title_, 8, 3, getWidth() - 16, 16, juce::Justification::left);
        const auto area = plot(); const auto p = handles();
        const float hold = area.getWidth() * HOLD_MS / duration();
        juce::Path path; path.startNewSubPath(area.getX(), area.getBottom());
        path.lineTo(p[0]); path.lineTo(p[1]); path.lineTo(p[1].x + hold, p[1].y); path.lineTo(p[2]);
        g.setColour(juce::Colour(0xff5dcaa5)); g.strokePath(path, juce::PathStrokeType(2.0f));
        g.setColour(juce::Colour(0xfff2f2f2));
        for (const auto point : p) g.fillRect(point.x - 3.0f, point.y - 3.0f, 6.0f, 6.0f);
    }
    void mouseDown(const juce::MouseEvent& event) override {
        const auto p = handles(); float closest = 12.0f;
        for (int i = 0; i < 3; ++i) {
            const float distance = p[size_t(i)].getDistanceFrom(event.position);
            if (distance < closest) { closest = distance; selected_ = i; }
        }
        if (selected_ < 0) return;
        drag_duration_ = duration();
        if (selected_ == 0) params_[0]->beginChangeGesture();
        if (selected_ == 1) { params_[1]->beginChangeGesture(); params_[2]->beginChangeGesture(); }
        if (selected_ == 2) params_[3]->beginChangeGesture();
    }
    void mouseDrag(const juce::MouseEvent& event) override {
        if (selected_ < 0) return;
        const auto area = plot();
        const float ms = juce::jlimit(0.0f, drag_duration_, (event.position.x - area.getX()) / area.getWidth() * drag_duration_);
        if (selected_ == 0) write(0, ms);
        if (selected_ == 1) {
            write(1, juce::jmax(0.0f, ms - read(0)));
            write(2, juce::jlimit(0.0f, 1.0f, (area.getBottom() - event.position.y) / area.getHeight()));
        }
        if (selected_ == 2) write(3, juce::jmax(0.0f, ms - read(0) - read(1) - HOLD_MS));
        repaint();
    }
    void mouseUp(const juce::MouseEvent&) override {
        if (selected_ == 0) params_[0]->endChangeGesture();
        if (selected_ == 1) { params_[1]->endChangeGesture(); params_[2]->endChangeGesture(); }
        if (selected_ == 2) params_[3]->endChangeGesture();
        selected_ = -1;
    }
private:
    static constexpr float HOLD_MS = 250.0f;
    float read(size_t i) const { return values_[i] ? values_[i]->load(std::memory_order_relaxed) : 0.0f; }
    float extent() const { return read(0) + read(1) + HOLD_MS + read(3); }
    float duration() const { return juce::jmax(1500.0f, extent(), peer_ ? peer_->extent() : 0.0f) * 1.05f; }
    juce::Rectangle<float> plot() const { return getLocalBounds().toFloat().withTrimmedTop(22).reduced(8, 7); }
    void write(size_t i, float value) {
        if (auto* parameter = params_[i]) parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
    }
    void timerCallback() override { if (isVisible()) repaint(); }
    juce::String title_;
    const EnvelopeEditor* peer_ = nullptr;
    std::array<juce::RangedAudioParameter*, 4> params_{};
    std::array<std::atomic<float>*, 4> values_{};
    int selected_ = -1;
    float drag_duration_ = 1500.0f;
};
