#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include <array>
#include <cmath>
#include <functional>

/* Armazón del editor según la referencia de Claude Design (apps/docs/sprints/gemelo-vst-ui-migracion.md, corte 1):
 * colores, botones rectangulares, celdas de la tira del panel, ilustraciones de engine, ENGINE SELECT y FX · LATER.
 * Todo se dibuja en coordenadas de la referencia (1200 × 760); el editor aplica la escala. */
namespace studio_ui {

namespace colour {
inline const juce::Colour BG{0xff0a0a0a}, SURFACE{0xff141414}, SURFACE_2{0xff1c1c1c}, LINE{0xff242424};
inline const juce::Colour TEAL{0xff1d9e75}, TEAL_PLUS{0xff5dcaa5}, TEAL_DIM{0xff0e5040};
inline const juce::Colour HY{0xff2fa8c8}, HY_PLUS{0xff7ad4ea}, HY_DIM{0xff0f4352};
inline const juce::Colour PURPLE{0xff534ab7}, PURPLE_BRIGHT{0xff7a6fe0}, PURPLE_DIM{0xff2b2758};
inline const juce::Colour GRAY{0xff9fe1cb}, WHITE{0xfff2f2f2}, MON{0xff5a5a6e}, MON_TEXT{0xff8e8ea2}, AMBER{0xffe8b84a};
}  // namespace colour

/** Colores de cromo de un modo: mc = línea y selección, hi = activo, dim = pistas, on = texto sobre mc. */
struct Mode {
    juce::Colour mc, hi, dim, on;
};
inline Mode synth_mode() { return {colour::TEAL, colour::TEAL_PLUS, colour::TEAL_DIM, colour::BG}; }
inline Mode hybrid_mode() { return {colour::HY, colour::HY_PLUS, colour::HY_DIM, colour::BG}; }
inline Mode fx_mode() { return {colour::PURPLE, colour::PURPLE_BRIGHT, colour::PURPLE_DIM, colour::WHITE}; }

inline juce::Font mono(float size, bool semibold = false) {
    return juce::Font(juce::FontOptions(size, semibold ? juce::Font::bold : juce::Font::plain));
}

/** Engines en el orden del firmware (GF_ENGINE_*). */
struct EngineInfo {
    const char* name;
    const char* tag;
    const char* arch;
};
inline const std::array<EngineInfo, 6>& engines() {
    static const std::array<EngineInfo, 6> list{{
        {"MODEL D", "MONO \xc2\xb7 3 VCO", "MONO \xc2\xb7 3 VCO \xc2\xb7 LADDER FILTER"},
        {"JUNO-106", "POLY 6 \xc2\xb7 CHORUS", "POLY 6 VOICES \xc2\xb7 DCO \xc2\xb7 BBD CHORUS"},
        {"PROPHET-5", "POLY 5 \xc2\xb7 2 VCO", "POLY 5 VOICES \xc2\xb7 2 VCO PER VOICE"},
        {"OB-6", "POLY 6 \xc2\xb7 SEM", "POLY 6 VOICES \xc2\xb7 2 VCO \xc2\xb7 SEM FILTER LP/BP/HP \xc2\xb7 X-MOD"},
        {"DX7", "FM \xc2\xb7 4-OP", "FM \xc2\xb7 4 OPERATORS"},
        {"ARP 2600", "SEMI-MODULAR", "SEMI-MODULAR \xc2\xb7 2 VCO \xc2\xb7 RING \xc2\xb7 SYNC"}}};
    return list;
}
inline juce::String utf8(const char* text) { return juce::String::fromUTF8(text); }

/** Ilustración de un engine, portada de `illo()` de studio-data.js (UI v3): viewBox 280 × 180, "xMidYMid slice". */
inline void draw_engine(juce::Graphics& g, int engine, juce::Rectangle<float> dst, float opacity = 1.0f) {
    using namespace colour;
    juce::Graphics::ScopedSaveState save(g);
    g.reduceClipRegion(dst.toNearestInt());
    const float scale = std::max(dst.getWidth() / 280.0f, dst.getHeight() / 180.0f);
    g.addTransform(juce::AffineTransform::translation(-140.0f, -90.0f).scaled(scale).translated(dst.getCentreX(), dst.getCentreY()));
    g.setOpacity(opacity);
    int knob_index = 0;
    auto rect = [&](float x, float y, float w, float h, juce::Colour fill, juce::Colour stroke = {}, float sw = 0) {
        g.setColour(fill); g.fillRect(x, y, w, h);
        if (sw > 0) { g.setColour(stroke); g.drawRect(x, y, w, h, sw); }
    };
    auto circ = [&](float cx, float cy, float r, juce::Colour fill, juce::Colour stroke = {}, float sw = 0) {
        g.setColour(fill); g.fillEllipse(cx - r, cy - r, 2 * r, 2 * r);
        if (sw > 0) { g.setColour(stroke); g.drawEllipse(cx - r, cy - r, 2 * r, 2 * r, sw); }
    };
    auto line = [&](float x1, float y1, float x2, float y2, juce::Colour s, float sw = 1) { g.setColour(s); g.drawLine(x1, y1, x2, y2, sw); };
    auto knob = [&](float cx, float cy, float r) {
        circ(cx, cy, r, BG, TEAL, 1.5f);
        const float a = float(((knob_index++) * 67) % 240 - 120) * juce::MathConstants<float>::pi / 180.0f;
        line(cx, cy, cx + std::sin(a) * (r - 1.5f), cy - std::cos(a) * (r - 1.5f), TEAL_PLUS, 1.5f);
    };
    auto keys = [&](float x, float y, float w, float h, int count) {
        const float kw = w / float(count);
        for (int i = 0; i < count; ++i) rect(x + float(i) * kw, y, kw, h, GRAY, BG, 1);
        constexpr int pattern[7] = {1, 1, 0, 1, 1, 1, 0};
        for (int i = 0; i < count - 1; ++i) if (pattern[i % 7]) rect(x + float(i + 1) * kw - kw * 0.3f, y, kw * 0.6f, h * 0.6f, BG);
    };
    auto body = [&](float x, float y, float w, float h) { rect(x, y, w, h, juce::Colour(0xff161616), TEAL_DIM, 2); line(x + 2, y + 2, x + w - 2, y + 2, TEAL, 2); };
    auto fader = [&](float x, float y, float h, float p) { line(x, y, x, y + h, TEAL_DIM, 2); rect(x - 3.5f, y + (h - 6) * (1 - p), 7, 6, TEAL_PLUS); };
    auto jack = [&](float cx, float cy) { circ(cx, cy, 2.5f, BG, GRAY, 1); };
    auto shadow = [&](float x, float w) { rect(x, 151, w, 5, juce::Colour(0xff050505)); };
    auto pr = [](int i) { constexpr float v[8] = {0.3f, 0.7f, 0.5f, 0.85f, 0.4f, 0.6f, 0.2f, 0.75f}; return v[i % 8]; };
    auto stroke_path = [&](juce::Path p, juce::Colour s, float sw) { g.setColour(s); g.strokePath(p, juce::PathStrokeType(sw)); };
    rect(0, 0, 280, 180, BG); rect(0, 151, 280, 29, juce::Colour(0xff0d0f0e)); line(0, 151, 280, 151, TEAL_DIM, 1);
    switch (engine) {
        case 0:   // Model D
            shadow(36, 208); rect(36, 44, 12, 107, TEAL_DIM); rect(232, 44, 12, 107, TEAL_DIM); body(48, 44, 184, 58);
            rect(48, 102, 184, 6, juce::Colour(0xff0f0f0f)); body(48, 108, 184, 43); keys(56, 116, 168, 35, 21);
            for (int r = 0; r < 3; ++r) for (int i = 0; i < 7; ++i) knob(66.0f + float(i) * 25, 58.0f + float(r) * 15, 5.5f);
            circ(226, 96, 2, TEAL_PLUS);
            break;
        case 1:   // Juno-106
            shadow(16, 248); body(16, 72, 248, 79); line(18, 110, 262, 110, TEAL_DIM, 1);
            for (int i = 0; i < 16; ++i) fader(30.0f + float(i) * 12, 80, 24, pr(i));
            for (int i = 0; i < 6; ++i) rect(222.0f + float(i % 2) * 18, 80.0f + float(i / 2) * 9, 14, 6, i == 1 || i == 2 ? TEAL_PLUS : TEAL_DIM);
            keys(24, 116, 232, 35, 36);
            break;
        case 2:   // Prophet-5
            shadow(20, 240); rect(20, 76, 10, 75, TEAL_DIM); rect(250, 76, 10, 75, TEAL_DIM); body(30, 76, 220, 75);
            for (int r = 0; r < 2; ++r) for (int i = 0; i < 11; ++i) knob(45.0f + float(i) * 19, 88.0f + float(r) * 14, 5.5f);
            for (int i = 0; i < 12; ++i) rect(45.0f + float(i) * 16.5f, 111, 10, 4, i == 3 ? TEAL_PLUS : TEAL);
            keys(36, 119, 208, 32, 29);
            break;
        case 3: {  // OB-6
            shadow(28, 224); rect(40, 145, 16, 6, TEAL_DIM); rect(224, 145, 16, 6, TEAL_DIM); body(28, 60, 224, 85);
            for (float x0 : {40.0f, 64.0f}) {
                juce::Path p; p.startNewSubPath(x0, 76); p.lineTo(x0 + 8, 68); p.lineTo(x0 + 8, 76); p.lineTo(x0 + 16, 68); p.lineTo(x0 + 16, 76);
                stroke_path(p, TEAL_PLUS, 2);
            }
            for (int r = 0; r < 2; ++r) for (int i = 0; i < 6; ++i) knob(52.0f + float(i) * 35, 94.0f + float(r) * 24, 9.5f);
            for (int i = 0; i < 9; ++i) rect(44.0f + float(i) * 22, 133, 14, 6, i == 0 ? TEAL_PLUS : TEAL_DIM);
            break;
        }
        case 4:   // DX7
            shadow(16, 248); body(16, 92, 248, 59); rect(28, 100, 52, 14, TEAL_DIM);
            line(32, 105, 66, 105, TEAL_PLUS, 1.5f); line(32, 110, 56, 110, TEAL_PLUS, 1.5f);
            for (int r = 0; r < 2; ++r) for (int i = 0; i < 16; ++i)
                rect(88.0f + float(i) * 10.4f, 99.0f + float(r) * 9, 8, 5, r == 0 ? (i % 4 == 3 ? PURPLE_BRIGHT : TEAL_DIM) : (i % 4 == 3 ? TEAL_PLUS : TEAL));
            keys(24, 120, 232, 31, 32);
            break;
        case 5: {  // ARP 2600
            shadow(36, 208); body(76, 8, 128, 104);
            for (int r = 0; r < 2; ++r) for (int i = 0; i < 8; ++i) fader(90.0f + float(i) * 14.5f, 18.0f + float(r) * 26, 20, pr(i + r * 3));
            for (int i = 0; i < 9; ++i) { jack(92.0f + float(i) * 12, 70); jack(92.0f + float(i) * 12, 80); }
            circ(96, 98, 8, juce::Colour(0xff0f0f0f), TEAL_DIM, 1.5f); circ(184, 98, 8, juce::Colour(0xff0f0f0f), TEAL_DIM, 1.5f);
            juce::Path a; a.startNewSubPath(104, 70); a.quadraticTo(124, 104, 152, 80); stroke_path(a, PURPLE_BRIGHT, 2);
            juce::Path b; b.startNewSubPath(128, 70); b.quadraticTo(160, 98, 188, 70); stroke_path(b, TEAL_PLUS, 2);
            line(100, 112, 100, 124, TEAL_DIM, 3); line(180, 112, 180, 124, TEAL_DIM, 3);
            body(36, 124, 208, 27); keys(44, 129, 192, 22, 24);
            break;
        }
        default: break;
    }
}

/** Botón rectangular de la referencia (radio 0): relleno, borde (sólido o punteado), texto y barra inferior opcional. */
class ChromeButton final : public juce::Component {
public:
    std::function<void()> onClick, onDoubleClick;
    juce::String text, caption;            ///< caption: etiqueta chica arriba a la izquierda (B1, B5…)
    juce::Colour fill{juce::Colours::transparentBlack}, border{colour::LINE}, ink{colour::GRAY}, bar{juce::Colours::transparentBlack};
    bool dashed = false, semibold = false;
    float size = 11.0f;

    void style(juce::Colour f, juce::Colour b, juce::Colour i, juce::Colour underline = juce::Colours::transparentBlack) {
        fill = f; border = b; ink = i; bar = underline; repaint();
    }
    void paint(juce::Graphics& g) override {
        auto r = getLocalBounds().toFloat();
        g.setColour(fill); g.fillRect(r);
        if (!border.isTransparent()) {
            g.setColour(border);
            if (dashed) {
                const float d[] = {4.0f, 3.0f};
                const auto in = r.reduced(0.5f);
                for (auto l : {juce::Line<float>(in.getTopLeft(), in.getTopRight()), juce::Line<float>(in.getTopRight(), in.getBottomRight()),
                               juce::Line<float>(in.getBottomRight(), in.getBottomLeft()), juce::Line<float>(in.getBottomLeft(), in.getTopLeft())})
                    g.drawDashedLine(l, d, 2, 1.0f);
            } else g.drawRect(r, 1.0f);
        }
        if (!bar.isTransparent()) { g.setColour(bar); g.fillRect(r.removeFromBottom(2.0f)); }
        if (caption.isNotEmpty()) {
            g.setColour(colour::MON_TEXT); g.setFont(mono(11.0f));
            g.drawText(caption, getLocalBounds().reduced(8, 6).removeFromTop(14), juce::Justification::topLeft);
        }
        g.setColour(isEnabled() ? ink : colour::MON); g.setFont(mono(size, semibold));
        g.drawText(text, getLocalBounds().reduced(4, 0), juce::Justification::centred, false);
    }
    void mouseUp(const juce::MouseEvent& e) override {
        if (isEnabled() && onClick && getLocalBounds().contains(e.getPosition())) onClick();
    }
    void mouseDoubleClick(const juce::MouseEvent&) override { if (isEnabled() && onDoubleClick) onDoubleClick(); }
};

/** Celda de la tira con valor editable arrastrando (ENC L / ENC R): rótulo, nombre del parámetro y valor. */
class ValueCell final : public juce::Component {
public:
    juce::String caption, name;
    bool right_aligned = false;

    void bind(juce::RangedAudioParameter* parameter, juce::String shown_name, juce::String shown_unit = {}) {
        parameter_ = parameter; name = std::move(shown_name); unit_ = std::move(shown_unit); repaint();
    }
    juce::RangedAudioParameter* parameter() const { return parameter_; }
    juce::String value_text() const {
        if (!parameter_) return utf8("\xe2\x80\x94");
        const float v = parameter_->convertFrom0to1(parameter_->getValue());
        const int decimals = std::abs(v) >= 100.0f ? 0 : std::abs(v) >= 10.0f ? 1 : 2;
        return juce::String(v, decimals) + (unit_.isEmpty() ? juce::String() : " " + unit_);
    }
    void paint(juce::Graphics& g) override {
        auto r = getLocalBounds();
        g.setColour(colour::LINE); g.drawRect(r.toFloat(), 1.0f);
        r = r.reduced(8, 6);
        const auto just = right_aligned ? juce::Justification::topRight : juce::Justification::topLeft;
        g.setFont(mono(11.0f)); g.setColour(colour::MON_TEXT); g.drawText(caption, r.removeFromTop(14), just);
        g.setColour(parameter_ ? colour::GRAY : colour::MON); g.drawText(parameter_ ? name : "NO FUNCTION", r.removeFromTop(16), just);
        g.setFont(mono(16.0f)); g.setColour(parameter_ ? colour::WHITE : colour::MON); g.drawText(value_text(), r.removeFromTop(20), just);
    }
    void mouseDown(const juce::MouseEvent&) override {
        if (!parameter_) return;
        start_ = parameter_->getValue(); parameter_->beginChangeGesture();
    }
    void mouseDrag(const juce::MouseEvent& e) override {
        if (!parameter_) return;
        const float fine = e.mods.isShiftDown() ? 0.1f : 1.0f;
        parameter_->setValueNotifyingHost(juce::jlimit(0.0f, 1.0f, start_ - float(e.getDistanceFromDragStartY()) * 0.004f * fine));
        repaint();
    }
    void mouseUp(const juce::MouseEvent&) override { if (parameter_) parameter_->endChangeGesture(); }
    void mouseDoubleClick(const juce::MouseEvent&) override {
        if (!parameter_) return;
        parameter_->beginChangeGesture(); parameter_->setValueNotifyingHost(parameter_->getDefaultValue()); parameter_->endChangeGesture();
    }
private:
    juce::RangedAudioParameter* parameter_ = nullptr;
    juce::String unit_;
    float start_ = 0;
};

/** Macro de la tira: perilla de 40 px con nombre, valor en % y su CC (cambio 02 de la referencia). */
class MacroKnob final : public juce::Component {
public:
    explicit MacroKnob(juce::String macro_name, juce::String cc_text) : name_(std::move(macro_name)), cc_(std::move(cc_text)) {
        slider_.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        slider_.setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
        slider_.setRotaryParameters(juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
        slider_.onValueChange = [this] { repaint(); };
        addAndMakeVisible(slider_);
    }
    /** Liga la perilla a un parámetro; nullptr = OFF (sin función en este modo o engine). */
    void bind(juce::AudioProcessorValueTreeState& state, const char* id) {
        attachment_.reset();
        parameter_ = id ? state.getParameter(id) : nullptr;
        slider_.setEnabled(parameter_ != nullptr);
        if (parameter_) attachment_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(state, id, slider_);
        repaint();
    }
    juce::Slider& slider() { return slider_; }
    juce::RangedAudioParameter* parameter() const { return parameter_; }
    void resized() override { slider_.setBounds((getWidth() - 48) / 2, 16, 48, 44); }
    void paint(juce::Graphics& g) override {
        const bool on = parameter_ != nullptr;
        g.setFont(mono(11.0f)); g.setColour(on ? colour::GRAY : colour::MON);
        g.drawText(name_, getLocalBounds().removeFromTop(16), juce::Justification::centred);
        auto r = getLocalBounds().withTrimmedTop(60);
        g.setFont(mono(13.0f)); g.setColour(on ? colour::WHITE : colour::MON);
        g.drawText(on ? juce::String(juce::roundToInt(parameter_->getValue() * 100.0f)) + " %" : "OFF", r.removeFromTop(18), juce::Justification::centred);
        g.setFont(mono(11.0f)); g.setColour(colour::MON_TEXT);
        g.drawText(cc_, r.removeFromTop(16), juce::Justification::centred);
    }
private:
    juce::String name_, cc_;
    juce::Slider slider_;
    juce::RangedAudioParameter* parameter_ = nullptr;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment_;
};

/** Engine de la cabecera (x 424, 176 × 40): miniatura + nombre; en HYBRID, las dos mitades; en FX, "NO ENGINE". Clic = ENGINE SELECT. */
class EngineBadge final : public juce::Component {
public:
    std::function<void()> onClick;
    enum class Kind { synth, hybrid, fx } kind = Kind::synth;
    int engine = 2, a = 4, b = 3, focus = 0;
    Mode mode = synth_mode();

    void paint(juce::Graphics& g) override {
        if (kind == Kind::fx) {
            g.setColour(colour::LINE); g.drawRect(0.0f, 0.0f, 64.0f, 40.0f, 1.0f);
            text(g, "NO ENGINE", utf8("AUDIO IN \xc2\xb7 PHASE 2"), 72, colour::MON, colour::MON_TEXT);
            return;
        }
        if (kind == Kind::synth) {
            draw_engine(g, engine, {0.0f, 0.0f, 64.0f, 40.0f});
            g.setColour(colour::LINE); g.drawRect(0.0f, 0.0f, 64.0f, 40.0f, 1.0f);
            text(g, engines()[size_t(engine)].name, utf8(engines()[size_t(engine)].tag), 72, colour::WHITE, colour::GRAY);
            return;
        }
        for (int side = 0; side < 2; ++side) {
            const juce::Rectangle<float> r(float(side * 44), 0.0f, 40.0f, 40.0f);
            draw_engine(g, side == 0 ? a : b, r, side == focus ? 1.0f : 0.5f);
            g.setColour(side == focus ? mode.hi : colour::LINE); g.drawRect(r, 1.0f);
        }
        g.setFont(mono(11.0f));
        for (int side = 0; side < 2; ++side) {
            g.setColour(side == focus ? mode.hi : colour::GRAY);
            g.drawText(utf8(side == 0 ? "A \xc2\xb7 " : "B \xc2\xb7 ") + engines()[size_t(side == 0 ? a : b)].name, 92, 4 + side * 16, 120, 16,
                       juce::Justification::left);
        }
    }
    void mouseUp(const juce::MouseEvent& e) override { if (onClick && getLocalBounds().contains(e.getPosition())) onClick(); }
private:
    static void text(juce::Graphics& g, juce::String name, juce::String tag, int x, juce::Colour n, juce::Colour t) {
        g.setFont(mono(13.0f, true)); g.setColour(n); g.drawText(name, x, 4, 120, 16, juce::Justification::left);
        g.setFont(mono(11.0f)); g.setColour(t); g.drawText(tag, x, 20, 120, 16, juce::Justification::left);
    }
};

/** 1j / 2e ENGINE SELECT: seis tarjetas de 384 × 216 con su ilustración. `locked` = engine de la otra mitad (HYBRID). */
class EngineSelectPage final : public juce::Component {
public:
    std::function<void(int)> on_pick;
    Mode mode = synth_mode();
    int current = -1, locked = -1;
    juce::String banner = "ENGINE", banner_right, current_badge = "LOADED", hover_badge = utf8("CLICK \xc2\xb7 LOADS A\xc2\xb7" "00");

    void paint(juce::Graphics& g) override {
        g.fillAll(colour::BG);
        g.setFont(mono(13.0f, true)); g.setColour(mode.hi); g.drawText(banner, 16, 8, 400, 24, juce::Justification::left);
        g.setFont(mono(11.0f)); g.setColour(colour::MON_TEXT);
        g.drawText(banner_right, getWidth() - 816, 8, 800, 24, juce::Justification::right);
        for (int i = 0; i < 6; ++i) {
            const auto r = tile(i).toFloat();
            const bool is_current = i == current, is_locked = i == locked, is_hover = i == hover_ && !is_locked && !is_current;
            const float alpha = is_locked ? 0.3f : 1.0f;
            g.setColour(colour::SURFACE.withMultipliedAlpha(alpha)); g.fillRect(r);
            draw_engine(g, i, r.withHeight(136.0f), alpha);
            g.setColour((is_current ? mode.hi : is_hover ? colour::WHITE : colour::LINE).withMultipliedAlpha(alpha));
            g.drawRect(r, is_current || is_hover ? 2.0f : 1.0f);
            auto text = r.withTrimmedTop(144).reduced(12, 0);
            g.setColour(colour::WHITE.withMultipliedAlpha(alpha)); g.setFont(mono(20.0f, true));
            g.drawText(engines()[size_t(i)].name, text.removeFromTop(26), juce::Justification::left);
            g.setColour(colour::GRAY.withMultipliedAlpha(alpha)); g.setFont(mono(11.0f));
            g.drawText(utf8(engines()[size_t(i)].arch), text.removeFromTop(18), juce::Justification::left);
            const juce::String badge = is_locked ? locked_badge : is_current ? current_badge : is_hover ? hover_badge : juce::String();
            if (badge.isNotEmpty()) {
                const float w = float(badge.length()) * 6.7f + 16.0f;
                const juce::Rectangle<float> b(r.getRight() - w - 8, r.getY() + 8, w, 20);
                g.setColour(is_current ? mode.hi : is_hover ? colour::WHITE : colour::BG); g.fillRect(b);
                g.setColour(is_current || is_hover ? colour::BG : colour::GRAY); g.setFont(mono(11.0f));
                g.drawText(badge, b, juce::Justification::centred);
            }
        }
    }
    juce::String locked_badge = "NOT AVAILABLE";
    void mouseMove(const juce::MouseEvent& e) override { set_hover(index_at(e.getPosition())); }
    void mouseExit(const juce::MouseEvent&) override { set_hover(-1); }
    void mouseUp(const juce::MouseEvent& e) override {
        const int i = index_at(e.getPosition());
        if (i >= 0 && i != locked && on_pick) on_pick(i);
    }
    static juce::Rectangle<int> tile(int i) { return {16 + (i % 3) * 392, 40 + (i / 3) * 224, 384, 216}; }
private:
    int hover_ = -1;
    int index_at(juce::Point<int> p) const { for (int i = 0; i < 6; ++i) if (tile(i).contains(p)) return i; return -1; }
    void set_hover(int i) { if (i != hover_) { hover_ = i; repaint(); } }
};

/** 1k: el modo FX es otro plugin (fase 2); el selector conserva el lugar para que el modelo de tres modos sea el del hardware. */
class FxLaterPage final : public juce::Component {
public:
    void paint(juce::Graphics& g) override {
        g.fillAll(colour::BG);
        auto r = getLocalBounds().reduced(16);
        g.setColour(colour::PURPLE); g.drawRect(r.toFloat(), 1.0f);
        r = r.reduced(32);
        g.setColour(colour::PURPLE_BRIGHT); g.setFont(mono(28.0f, true));
        g.drawText(utf8("FX \xc2\xb7 COMING LATER"), r.removeFromTop(48), juce::Justification::left);
        g.setColour(colour::GRAY); g.setFont(mono(13.0f));
        g.drawFittedText(utf8("FX mode processes an audio input with the Brain effects. It will be a separate effect plugin "
                              "(phase 2). The selector keeps its place so the three modes match the hardware: SYNTH \xc2\xb7 FX \xc2\xb7 HYBRID."),
                         r.removeFromTop(60), juce::Justification::topLeft, 3);
        g.setColour(colour::MON_TEXT); g.setFont(mono(11.0f));
        g.drawText("CHOOSE SYNTH OR HYBRID TO PLAY", r.removeFromTop(24), juce::Justification::left);
    }
};

}  // namespace studio_ui
