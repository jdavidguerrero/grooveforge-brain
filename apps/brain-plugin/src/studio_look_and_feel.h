#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include <cmath>

/** Visual tokens shared by the VST's existing JUCE controls. */
class StudioLookAndFeel final : public juce::LookAndFeel_V4 {
public:
    StudioLookAndFeel() {
        setColour(juce::TextButton::buttonColourId, juce::Colour(0xff141414));
        setColour(juce::TextButton::textColourOffId, juce::Colour(0xfff2f2f2));
        setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff141414));
        setColour(juce::ComboBox::outlineColourId, juce::Colour(0xff242424));
        setColour(juce::ComboBox::textColourId, juce::Colour(0xfff2f2f2));
        setColour(juce::PopupMenu::backgroundColourId, juce::Colour(0xff141414));
        setColour(juce::PopupMenu::textColourId, juce::Colour(0xfff2f2f2));
    }

    /** Select the accent used by SYNTH or HYBRID. */
    void set_hybrid(bool hybrid) {
        accent_ = juce::Colour(hybrid ? 0xff7ad4ea : 0xff5dcaa5);
        dim_ = juce::Colour(hybrid ? 0xff0f4352 : 0xff0e5040);
        setColour(juce::Slider::trackColourId, accent_);
        setColour(juce::Slider::thumbColourId, juce::Colour(0xfff2f2f2));
        setColour(juce::PopupMenu::highlightedBackgroundColourId, dim_);
    }

    void drawRotarySlider(juce::Graphics& g, int x, int y, int w, int h, float position,
                          float start, float end, juce::Slider& slider) override {
        const float radius = juce::jmin(float(w), float(h)) * 0.38f;
        const float cx = float(x) + float(w) * 0.5f, cy = float(y) + float(h) * 0.5f;
        juce::Path track, value;
        track.addCentredArc(cx, cy, radius, radius, 0.0f, start, end, true);
        value.addCentredArc(cx, cy, radius, radius, 0.0f, start, start + position * (end - start), true);
        g.setColour(dim_); g.strokePath(track, juce::PathStrokeType(3.0f));
        g.setColour(slider.isEnabled() ? accent_ : juce::Colour(0xff5a5a6e));
        g.strokePath(value, juce::PathStrokeType(3.0f));
        const float angle = start + position * (end - start);
        g.setColour(juce::Colour(0xfff2f2f2));
        g.drawLine(cx + std::sin(angle) * radius * 0.53f, cy - std::cos(angle) * radius * 0.53f,
                   cx + std::sin(angle) * radius * 0.87f, cy - std::cos(angle) * radius * 0.87f, 2.0f);
    }

    void drawLinearSlider(juce::Graphics& g, int x, int y, int w, int h, float pos,
                          float minPos, float maxPos, juce::Slider::SliderStyle style,
                          juce::Slider& slider) override {
        if (style != juce::Slider::LinearHorizontal) {
            juce::LookAndFeel_V4::drawLinearSlider(g, x, y, w, h, pos, minPos, maxPos, style, slider);
            return;
        }
        const float mid = float(y) + float(h) * 0.5f;
        g.setColour(dim_); g.fillRect(float(x), mid - 1.0f, float(w), 2.0f);
        g.setColour(slider.isEnabled() ? accent_ : juce::Colour(0xff5a5a6e));
        g.fillRect(float(x), mid - 2.0f, juce::jmax(0.0f, pos - float(x)), 4.0f);
        g.setColour(juce::Colour(0xfff2f2f2)); g.fillRect(pos - 2.0f, mid - 7.0f, 4.0f, 14.0f);
    }
private:
    juce::Colour accent_{0xff5dcaa5}, dim_{0xff0e5040};
};
