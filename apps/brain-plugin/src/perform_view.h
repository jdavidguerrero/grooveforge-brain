#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include <array>
#include <cmath>
#include <cstring>
#include <functional>

#include "studio_shell.h"

/* PERFORM de la referencia (1a; gemelo-vst-ui-migracion.md §3). La onda y la curva son las de la cabeza de Studio
 * (apps/studio-head/src/ui/screen_perform.cpp y dsp_math.h, theory/08 de Studio): ilustrativas, no audio medido. */
namespace studio_ui {

/** Lo que el dibujo necesita de un engine, en las unidades de la UI (las mismas de GF_OSC_SLOT). */
struct PerformSource {
    int engine = 2;
    float wave = 0, pw = 50, sub = 0, shape = 0;     ///< forma, ancho de pulso %, SUB % (Juno), SHAPE % (OB-6)
    float fm_level = 0, fm_ratio = 1;                ///< DX7: LEVEL y RATIO de OP2
    float sem_morph = 0;                             ///< OB-6: morph % del SEM
    bool sem_bp = false;
    float cutoff_hz = 1000, res_norm = 0;            ///< resonancia normalizada 0–1 (RESO_RANGE del engine)
    float lfo_cutoff = 0, lfo_rate = 2;              ///< profundidad 0–1 hacia el cutoff, Hz
    int lfo_shape = 1;                               ///< 0 SINE · 1 TRI · 2 SAW · 3 SQR · 4 S&H
};

namespace perform_math {
inline constexpr float F0 = 110.0f;
inline constexpr int SPC = 256;                       ///< muestras por ciclo de la simulación
inline constexpr float FS = F0 * SPC;                 ///< 28,2 kHz: el ringing a cutoff alto no sale dentado

inline float freq_frac(float f) { return juce::jlimit(0.0f, 1.0f, std::log(f / 20.0f) / std::log(1000.0f)); }
inline float frac_freq(float x) { return 20.0f * std::pow(1000.0f, x); }

/// Low-pass de 4 polos (24 dB/oct) con pico de resonancia, en dB (dsp_math.h de Studio).
inline float ladder_db(float f, float fc, float res) {
    const float r = f / fc, l2 = std::log2(r);
    return -10.0f * std::log10(1.0f + std::pow(r, 8.0f)) + res * 14.0f * std::exp(-(l2 * l2) / 0.05f);
}
/// SEM SVF de 2 polos con morph LP ↔ notch ↔ HP, o BP (dsp_math.h de Studio).
inline float sem_db(float f, float fc, float res, float morph, bool bp) {
    const float r = std::max(1e-4f, f / std::max(20.0f, fc)), r2 = r * r;
    const float k = juce::jlimit(0.12f, 2.0f, 2.0f - 1.9f * res);
    const float den2 = (1.0f - r2) * (1.0f - r2) + (k * r) * (k * r);
    float num2;
    if (bp) num2 = (r * std::max(0.5f, k)) * (r * std::max(0.5f, k));
    else { const float m = juce::jlimit(0.0f, 1.0f, morph), re = (1.0f - m) - m * r2; num2 = re * re + 1e-6f; }
    return juce::jlimit(-60.0f, 14.0f, 10.0f * std::log10(num2 / std::max(1e-8f, den2)));
}
inline float response_db(const PerformSource& s, float f, float fc) {
    return s.engine == 3 ? sem_db(f, fc, s.res_norm, s.sem_morph / 100.0f, s.sem_bp) : ladder_db(f, fc, s.res_norm);
}
inline float lfo_value(int shape, float phase) {
    const float ph = phase - std::floor(phase);
    switch (shape) {
        case 0: return std::sin(juce::MathConstants<float>::twoPi * ph);
        case 1: return 1.0f - 4.0f * std::abs(ph - 0.5f);
        case 2: return 2.0f * ph - 1.0f;
        case 3: return ph < 0.5f ? 1.0f : -1.0f;
        default: return std::sin(juce::MathConstants<float>::twoPi * std::floor(phase) * 1.7f);
    }
}
/// Una muestra de la forma principal del engine (sin anti-aliasing: es para dibujar).
inline float osc_sample(const PerformSource& s, float p, float pw) {
    const float ph = p - std::floor(p), saw = 2.0f * ph - 1.0f, pulse = ph < pw ? 1.0f : -1.0f;
    const float sqr = ph < 0.5f ? 1.0f : -1.0f, tri = 1.0f - 4.0f * std::abs(ph - 0.5f);
    const int w = int(std::lround(s.wave));
    switch (s.engine) {
        case 0: return w == 1 ? tri : w == 2 ? sqr : w == 3 ? pulse : w == 4 ? 0.5f * (saw + tri) : w == 5 ? (ph < 0.10f ? 1.0f : -1.0f) : saw;
        case 1: {
            const float main = w == 1 ? sqr : w == 2 ? pulse : w == 3 ? 0.55f * (saw + pulse) : saw;
            const float sub = (p * 0.5f - std::floor(p * 0.5f)) < 0.5f ? 1.0f : -1.0f;
            return main + s.sub / 100.0f * 0.6f * sub;
        }
        case 3: { const float m = juce::jlimit(0.0f, 1.0f, s.shape / 100.0f); return saw + m * (pulse - saw); }
        case 4: {
            const float idx = s.fm_level > 0 ? 6.0f * std::pow(2.0f, (s.fm_level - 99.0f) / 8.0f) : 0.0f;
            return std::sin(juce::MathConstants<float>::twoPi * ph + idx * std::sin(juce::MathConstants<float>::twoPi * ph * s.fm_ratio));
        }
        case 5: return w == 1 ? tri : w == 2 ? sqr : w == 3 ? pulse : w == 4 ? std::sin(juce::MathConstants<float>::twoPi * ph) : saw;
        default: return saw;   // Prophet: OSC A en SAW (su página OSC no existe en el plugin)
    }
}
/// Onda filtrada: ladder lineal de 4 polos (TPT, Zavalishin) con realimentación; arranca 4 ciclos antes. `cycles` dibujados.
inline void filtered_wave(const PerformSource& s, float fc, float pw, float* out, int points, int cycles) {
    const float f = juce::jlimit(20.0f, 0.45f * FS, fc);
    const float g = std::tan(juce::MathConstants<float>::pi * f / FS), G = g / (1.0f + g);
    const float k = juce::jlimit(0.0f, 1.0f, s.res_norm) * 3.8f, comp = 1.0f + 0.5f * k;
    float st[4] = {};
    const int pre = 4 * SPC, n = cycles * SPC;
    const float G4 = G * G * G * G, H = 1.0f - G;
    for (int i = 0; i < pre + n; ++i) {
        const float x = osc_sample(s, float(i) / SPC, pw) * 0.6f;
        const float sigma = G * G * G * H * st[0] + G * G * H * st[1] + G * H * st[2] + H * st[3];
        const float y4 = (G4 * x + sigma) / (1.0f + k * G4);
        float u = x - k * y4;
        for (float& v : st) { const float d = G * (u - v), y = d + v; v = y + d; u = y; }
        if (i >= pre) {
            const int j = (i - pre) * points / n;
            if (j < points) out[j] = juce::jlimit(-1.2f, 1.2f, u * comp);
        }
    }
}
inline const char* wave_name(const PerformSource& s) {
    const int w = int(std::lround(s.wave));
    switch (s.engine) {
        case 0: { static const char* n[] = {"SAW", "TRI", "SQR", "PULSE", "SHARK", "NARROW"}; return n[juce::jlimit(0, 5, w)]; }
        case 1: { static const char* n[] = {"SAW", "SQR", "PULSE", "SAW+PULSE"}; return n[juce::jlimit(0, 3, w)]; }
        case 3: return "SHAPE";
        case 4: return "OP2 \xe2\x86\x92 OP1";
        case 5: { static const char* n[] = {"SAW", "TRI", "SQR", "PULSE", "SINE"}; return n[juce::jlimit(0, 4, w)]; }
        default: return "OSC A SAW";
    }
}
}  // namespace perform_math

/** Perilla héroe de 160 px (CUTOFF / RESONANCE): arco, puntero, arco MOD púrpura con el rango del LFO y valor grande. */
class HeroKnob final : public juce::Component {
public:
    juce::String label, encoder, unit;
    Mode mode = synth_mode();
    float mod_low = -1, mod_high = -1;               ///< rango normalizado que recorre el LFO; < 0 = sin MOD

    void bind(juce::RangedAudioParameter* p) { parameter_ = p; repaint(); }
    juce::RangedAudioParameter* parameter() const { return parameter_; }
    void paint(juce::Graphics& g) override {
        const float w = float(getWidth());
        g.setFont(mono(13.0f)); g.setColour(colour::GRAY);
        g.drawText(label, 0, 0, int(w / 2) + 30, 16, juce::Justification::right);
        g.setFont(mono(11.0f)); g.setColour(colour::MON_TEXT);
        g.drawText(encoder, int(w / 2) + 38, 0, 80, 16, juce::Justification::left);
        const float cx = w / 2, cy = 104, r = 66;
        const float a0 = -0.75f * juce::MathConstants<float>::pi, a1 = 0.75f * juce::MathConstants<float>::pi;
        const float v = parameter_ ? parameter_->getValue() : 0.0f, av = a0 + v * (a1 - a0);
        auto arc = [&](float rad, float from, float to, juce::Colour c, float thick) {
            juce::Path p; p.addCentredArc(cx, cy, rad, rad, 0, from, to, true);
            g.setColour(c); g.strokePath(p, juce::PathStrokeType(thick, juce::PathStrokeType::curved, juce::PathStrokeType::butt));
        };
        arc(r, a0, a1, parameter_ ? mode.dim : colour::SURFACE_2, 8);
        if (parameter_) arc(r, a0, av, mode.hi, 8);
        if (parameter_ && mod_low >= 0 && mod_high > mod_low) {
            arc(r + 9, a0 + mod_low * (a1 - a0), a0 + mod_high * (a1 - a0), colour::PURPLE_BRIGHT, 4);
            g.setColour(colour::PURPLE_BRIGHT); g.setFont(mono(11.0f));
            g.drawText("MOD", juce::Rectangle<float>(cx - 30, cy + 44, 60, 16), juce::Justification::centred);
        }
        g.setColour(colour::WHITE);
        g.drawLine(cx + std::sin(av) * r * 0.30f, cy - std::cos(av) * r * 0.30f, cx + std::sin(av) * r * 0.78f, cy - std::cos(av) * r * 0.78f, 5.0f);
        juce::String value = juce::String::fromUTF8("\xe2\x80\x94");
        if (parameter_) {
            const float x = parameter_->convertFrom0to1(v);
            value = std::abs(x) >= 100 ? juce::String(juce::roundToInt(x)) : juce::String(x, 2);
        }
        g.setFont(mono(40.0f)); g.setColour(parameter_ ? colour::WHITE : colour::MON);
        const int vw = int(float(value.length()) * 24.0f), uw = unit.isEmpty() ? 0 : int(float(unit.length()) * 10.0f) + 6;
        const int x0 = int(cx) - (vw + uw) / 2;
        g.drawText(value, x0, 184, vw, 48, juce::Justification::left);
        g.setFont(mono(16.0f)); g.setColour(colour::GRAY);
        g.drawText(unit, x0 + vw + 6, 200, uw, 24, juce::Justification::left);
    }
    void mouseDown(const juce::MouseEvent&) override { if (parameter_) { start_ = parameter_->getValue(); parameter_->beginChangeGesture(); } }
    void mouseDrag(const juce::MouseEvent& e) override {
        if (!parameter_) return;
        parameter_->setValueNotifyingHost(juce::jlimit(0.0f, 1.0f, start_ - float(e.getDistanceFromDragStartY()) * (e.mods.isShiftDown() ? 0.0006f : 0.004f)));
        repaint();
    }
    void mouseUp(const juce::MouseEvent&) override { if (parameter_) parameter_->endChangeGesture(); }
    void mouseDoubleClick(const juce::MouseEvent&) override {
        if (!parameter_) return;
        parameter_->beginChangeGesture(); parameter_->setValueNotifyingHost(parameter_->getDefaultValue()); parameter_->endChangeGesture();
    }
private:
    juce::RangedAudioParameter* parameter_ = nullptr;
    float start_ = 0;
};

/** La página PERFORM entera (1200 × 488 del área de contenido). El editor la alimenta con `source` y los parámetros. */
class PerformPage final : public juce::Component, private juce::Timer {
public:
    std::function<PerformSource()> read_source;      ///< el editor arma la fuente con los parámetros del engine
    std::function<void(int)> on_chip, on_chip_jump;  ///< 0 PUMP · 1 ARP · 2 CHORD
    std::function<std::array<juce::String, 3>()> chip_texts;
    std::function<std::array<bool, 3>()> chip_states;
    std::function<float()> host_bpm;
    juce::RangedAudioParameter* cutoff = nullptr;
    juce::RangedAudioParameter* resonance = nullptr;
    Mode mode = synth_mode();
    HeroKnob hero_cutoff, hero_resonance;
    std::array<std::unique_ptr<MacroKnob>, 4> page_knobs;
    std::array<ChromeButton, 3> chips;

    PerformPage() {
        hero_cutoff.label = "CUTOFF"; hero_cutoff.encoder = "ENC L"; hero_cutoff.unit = "HZ";
        hero_resonance.label = "RESONANCE"; hero_resonance.encoder = "ENC R";
        addAndMakeVisible(hero_cutoff); addAndMakeVisible(hero_resonance);
        constexpr const char* names[] = {"FLT ENV AMT", "LFO \xe2\x86\x92 CUTOFF", "\xe2\x87\xa7 GLIDE", "\xe2\x87\xa7 DRIVE"};
        constexpr const char* keys[] = {"P1", "P2", "\xe2\x87\xa7 P1", "\xe2\x87\xa7 P2"};
        for (size_t i = 0; i < 4; ++i) {
            page_knobs[i] = std::make_unique<MacroKnob>(juce::String::fromUTF8(names[i]), juce::String::fromUTF8(keys[i]));
            page_knobs[i]->setComponentID("perform.p." + juce::String(int(i)));
            addAndMakeVisible(*page_knobs[i]);
        }
        for (int i = 0; i < 3; ++i) {
            chips[size_t(i)].onClick = [this, i] { if (on_chip) on_chip(i); };
            chips[size_t(i)].onDoubleClick = [this, i] { if (on_chip_jump) on_chip_jump(i); };
            chips[size_t(i)].setComponentID("perform.chip." + juce::String(i));
            addAndMakeVisible(chips[size_t(i)]);
        }
        hero_cutoff.setComponentID("perform.cutoff"); hero_resonance.setComponentID("perform.resonance");
        startTimerHz(30);
    }
    /** Fuente actual (la que se dibujó por última vez); para tests. */
    const PerformSource& source() const { return source_; }
    /** Onda dibujada; para tests. */
    const std::array<float, 376>& wave_points() const { return wave_; }

    void refresh() {
        if (read_source) source_ = read_source();
        const float lfo = source_.lfo_cutoff > 0 ? perform_math::lfo_value(source_.lfo_shape, lfo_phase_) : 0.0f;
        fc_ = juce::jlimit(20.0f, 20000.0f, source_.cutoff_hz * std::exp2(source_.lfo_cutoff * 4.0f * lfo));
        float pw = juce::jlimit(0.05f, 0.95f, source_.pw / 100.0f);
        perform_math::filtered_wave(source_, fc_, pw, wave_.data(), int(wave_.size()), 3);
        if (cutoff && source_.lfo_cutoff > 0) {
            hero_cutoff.mod_low = cutoff->convertTo0to1(juce::jlimit(cutoff->getNormalisableRange().start, cutoff->getNormalisableRange().end, source_.cutoff_hz * std::exp2(-source_.lfo_cutoff * 4.0f)));
            hero_cutoff.mod_high = cutoff->convertTo0to1(juce::jlimit(cutoff->getNormalisableRange().start, cutoff->getNormalisableRange().end, source_.cutoff_hz * std::exp2(source_.lfo_cutoff * 4.0f)));
        } else hero_cutoff.mod_low = hero_cutoff.mod_high = -1;
        hero_cutoff.mode = hero_resonance.mode = mode;
        hero_cutoff.bind(cutoff); hero_resonance.bind(resonance);
        if (chip_texts && chip_states) {
            const auto t = chip_texts(); const auto on = chip_states();
            for (size_t i = 0; i < 3; ++i) {
                chips[i].text = t[i];
                chips[i].style(on[i] ? mode.hi : juce::Colours::transparentBlack, on[i] ? mode.hi : colour::LINE, on[i] ? mode.on : colour::GRAY);
            }
        }
        layout_chips();
        repaint();
    }
    void layout_chips() {
        int x = 784;
        for (auto& c : chips) { const int w = int(float(c.text.length()) * 6.7f) + 18; c.setBounds(x, 16, w, 24); x += w + 8; }
    }
    void resized() override {
        hero_cutoff.setBounds(784, 56, 192, 240); hero_resonance.setBounds(992, 56, 192, 240);
        for (size_t i = 0; i < 4; ++i) page_knobs[i]->setBounds(784 + int(i) * 100, 372, 100, 110);
        layout_chips();
    }
    void paint(juce::Graphics& g) override {
        using namespace perform_math;
        g.fillAll(colour::BG);
        // onda
        const juce::Rectangle<float> wb(16, 16, 752, 248), fb(16, 272, 752, 200);
        for (auto r : {wb, fb}) { g.setColour(colour::SURFACE); g.fillRect(r); g.setColour(colour::LINE); g.drawRect(r, 1.0f); }
        g.setFont(mono(11.0f)); g.setColour(colour::GRAY);
        g.drawText(juce::String::fromUTF8("OSC \xe2\x86\x92 FILTER \xc2\xb7 ") + juce::String::fromUTF8(wave_name(source_)) + juce::String::fromUTF8(" \xc2\xb7 A2 110 HZ"),
                   28, 24, 480, 16, juce::Justification::left);
        if (source_.lfo_cutoff > 0) {
            static const char* shapes[] = {"SINE", "TRI", "SAW", "SQR", "S&H"};
            g.setColour(colour::PURPLE_BRIGHT);
            g.drawText("LFO " + juce::String(shapes[juce::jlimit(0, 4, source_.lfo_shape)]) + " " + juce::String(source_.lfo_rate, 2) +
                       juce::String::fromUTF8(" HZ \xe2\x86\x92 CUTOFF"), 456, 24, 300, 16, juce::Justification::right);
        }
        const float wy = 44 + 108, amp = 216 * 0.42f;
        g.setColour(colour::LINE); g.drawHorizontalLine(int(wy), 17.0f, 767.0f);
        juce::Path wave;
        for (size_t i = 0; i < wave_.size(); ++i) {
            const float x = 17.0f + float(i) * 750.0f / float(wave_.size() - 1), y = wy - wave_[i] * amp;
            if (i == 0) wave.startNewSubPath(x, y); else wave.lineTo(x, y);
        }
        g.setColour(mode.hi); g.strokePath(wave, juce::PathStrokeType(2.0f));
        // curva del filtro
        g.setColour(colour::GRAY); g.setFont(mono(11.0f));
        g.drawText(juce::String::fromUTF8(source_.engine == 3 ? (source_.sem_bp ? "SEM \xc2\xb7 2-POLE \xc2\xb7 BANDPASS" : "SEM \xc2\xb7 2-POLE \xc2\xb7 12 DB / OCT \xc2\xb7 MORPH")
                                                             : "LADDER \xc2\xb7 4-POLE \xc2\xb7 24 DB / OCT"),
                   28, 280, 480, 16, juce::Justification::left);
        const float top = 300, h = 160, x0 = 17, w = 750;
        auto db_y = [&](float db) { return juce::jlimit(top + 1, top + h - 1, top + h * ((18.0f - db) / 78.0f)); };
        juce::Path curve, fill;
        for (int i = 0; i <= 188; ++i) {
            const float x = x0 + float(i) * w / 188.0f, y = db_y(response_db(source_, frac_freq(float(i) / 188.0f), fc_));
            if (i == 0) { curve.startNewSubPath(x, y); fill.startNewSubPath(x, top + h); fill.lineTo(x, y); }
            else { curve.lineTo(x, y); fill.lineTo(x, y); }
        }
        fill.lineTo(x0 + w, top + h); fill.closeSubPath();
        g.setColour(mode.dim); g.fillPath(fill);
        g.setColour(mode.hi); g.strokePath(curve, juce::PathStrokeType(2.0f));
        const float cx = x0 + freq_frac(fc_) * w;
        g.setColour(colour::WHITE); g.drawVerticalLine(int(cx), top, top + h);
        g.setFont(mono(11.0f));
        const bool label_left = cx > x0 + w - 96;
        g.drawText(juce::String(juce::roundToInt(fc_)) + " HZ", label_left ? int(cx) - 96 : int(cx) + 6, int(top) + 2, 90, 14,
                   label_left ? juce::Justification::right : juce::Justification::left);
        g.setColour(colour::MON_TEXT);
        for (float f : {100.0f, 1000.0f, 10000.0f})
            g.drawText(f >= 1000 ? juce::String(int(f / 1000)) + "K" : juce::String(int(f)), int(x0 + freq_frac(f) * w) - 8, 452, 40, 14, juce::Justification::left);
        // HOST y la fila P1 / P2
        g.setColour(colour::GRAY);
        g.drawText("HOST " + juce::String(host_bpm ? host_bpm() : 120.0f, 2), 1064, 20, 120, 16, juce::Justification::right);
        g.setColour(colour::MON_TEXT);
        g.drawText(juce::String::fromUTF8("PAGE \xc2\xb7 P1 \xc2\xb7 P2 \xc2\xb7 \xe2\x87\xa7 P1 \xc2\xb7 \xe2\x87\xa7 P2"), 784, 352, 400, 16, juce::Justification::left);
    }
    // arrastrar en la curva: horizontal = CUTOFF, vertical = RESONANCE
    void mouseDown(const juce::MouseEvent& e) override {
        dragging_ = juce::Rectangle<int>(16, 272, 752, 200).contains(e.getPosition());
        if (!dragging_) return;
        if (cutoff) cutoff->beginChangeGesture();
        if (resonance) { resonance->beginChangeGesture(); res_start_ = resonance->getValue(); }
        mouseDrag(e);
    }
    void mouseDrag(const juce::MouseEvent& e) override {
        if (!dragging_) return;
        if (cutoff) {
            const auto range = cutoff->getNormalisableRange();
            const float hz = perform_math::frac_freq(juce::jlimit(0.0f, 1.0f, (float(e.x) - 17.0f) / 750.0f));
            cutoff->setValueNotifyingHost(cutoff->convertTo0to1(juce::jlimit(range.start, range.end, hz)));
        }
        if (resonance) resonance->setValueNotifyingHost(juce::jlimit(0.0f, 1.0f, res_start_ - float(e.getDistanceFromDragStartY()) * 0.004f));
        refresh();
    }
    void mouseUp(const juce::MouseEvent&) override {
        if (!dragging_) return;
        if (cutoff) cutoff->endChangeGesture();
        if (resonance) resonance->endChangeGesture();
        dragging_ = false;
    }
private:
    void timerCallback() override {
        if (!isShowing() && !testing_) return;
        if (read_source) {
            const auto next = read_source();
            const bool moving = next.lfo_cutoff > 0;
            if (moving) lfo_phase_ += next.lfo_rate / 30.0f;
            if (moving || std::memcmp(&next, &source_, sizeof(PerformSource)) != 0) refresh();
        }
    }
    PerformSource source_;
    std::array<float, 376> wave_{};
    float fc_ = 1000, lfo_phase_ = 0, res_start_ = 0;
    bool dragging_ = false, testing_ = false;
};

}  // namespace studio_ui
