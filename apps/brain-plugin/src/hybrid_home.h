#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include <array>
#include <cmath>
#include <functional>

#include "osc_view.h"

/* HOME de HYBRID (2a; gemelo-vst-ui-migracion.md §8): lista de capas, tarjetas A y B, MORPH y el filtro de la mitad en
 * foco. El editor la alimenta con callbacks; los parámetros se ligan en `bind`. */
namespace studio_ui {

namespace hybrid_math {
/// Crossfade de igual potencia normalizado: al 50 % las dos mitades suenan a su ganancia base (nota MORPH de 2a).
inline float gain_a(float m) { return std::min(1.0f, std::cos(juce::MathConstants<float>::halfPi * m) / std::sqrt(0.5f)); }
inline float gain_b(float m) { return std::min(1.0f, std::sin(juce::MathConstants<float>::halfPi * m) / std::sqrt(0.5f)); }
/// Voces de cada engine en el firmware (ARP_VOICES, DX7_VOICES…; el Model D es mono).
inline int voices(int engine) { constexpr int v[6] = {1, 6, 5, 6, 6, 4}; return v[juce::jlimit(0, 5, engine)]; }
}  // namespace hybrid_math

class HybridHome final : public juce::Component, private juce::Timer {
public:
    Mode mode = hybrid_mode();
    // datos que da el editor
    std::function<juce::StringArray()> layer_names;
    std::function<int()> loaded_layer;
    std::function<void(int)> on_load_layer, on_focus, on_open_osc;
    std::function<void()> on_save_layer, on_custom;
    std::function<bool()> modified;                  ///< 2h: la capa cargada tiene cambios sin guardar
    std::function<float()> cpu;                      ///< carga 0–1 del callback de audio
    std::function<juce::String(int)> half_name;
    std::array<int, 2> engine{4, 3};
    int focus = 0;
    juce::RangedAudioParameter *morph = nullptr, *cutoff = nullptr, *resonance = nullptr, *other_cutoff = nullptr, *other_resonance = nullptr;
    bool link = true;                                ///< §9.2: la otra mitad sigue a la del foco (solo desde la interfaz)
    std::function<void(bool)> on_link;
    std::unique_ptr<ParamCell> delay, transpose;
    std::array<std::unique_ptr<ParamCell>, 2> gain;  ///< BASE GAIN de cada mitad en su tarjeta (§9.3; antes HOME · LAYER)
    HeroKnob hero_cutoff, hero_resonance;

    static constexpr int ROW_H = 28, LIST_X = 16, LIST_Y = 48, LIST_W = 264, LIST_ROWS = 13;
    static juce::Rectangle<int> card_rect(int side) { return {side == 0 ? 296 : 904, 16, 280, 232}; }
    static juce::Rectangle<int> morph_rect() { return {584, 16, 312, 232}; }
    static juce::Rectangle<int> morph_curve() { return {600, 56, 280, 56}; }
    static juce::Rectangle<int> filter_rect() { return {296, 256, 456, 216}; }
    static juce::Rectangle<int> save_row() { return {16, 416, 264, 28}; }
    static juce::Rectangle<int> custom_row() { return {16, 444, 264, 28}; }

    HybridHome() {
        hero_cutoff.label = "CUTOFF"; hero_cutoff.encoder = "ENC L"; hero_cutoff.unit = "HZ";
        hero_resonance.label = "RESONANCE"; hero_resonance.encoder = "ENC R";
        for (auto* h : {&hero_cutoff, &hero_resonance}) { addAndMakeVisible(*h); h->setTransform(juce::AffineTransform::scale(0.8f)); }
        hero_cutoff.setComponentID("hybrid.home.cutoff"); hero_resonance.setComponentID("hybrid.home.resonance");
        hero_cutoff.on_drag_start = [this] { begin_follow(0); }; hero_cutoff.on_drag = [this] { follow(0); };
        hero_resonance.on_drag_start = [this] { begin_follow(1); }; hero_resonance.on_drag = [this] { follow(1); };
        hero_cutoff.on_drag_end = [this] { end_follow(0); }; hero_resonance.on_drag_end = [this] { end_follow(1); };
        startTimerHz(15);
    }
    /** Liga los parámetros de la capa y de la mitad en foco. */
    void bind(juce::AudioProcessorValueTreeState& state) {
        const juce::String mine = focus == 0 ? "hybrid.a." : "hybrid.b.", other = focus == 0 ? "hybrid.b." : "hybrid.a.";
        morph = state.getParameter("hybrid.morph");
        cutoff = state.getParameter(mine + "cutoff"); resonance = state.getParameter(mine + "resonance");
        other_cutoff = state.getParameter(other + "cutoff"); other_resonance = state.getParameter(other + "resonance");
        hero_cutoff.bind(cutoff); hero_resonance.bind(resonance);
        hero_cutoff.mode = hero_resonance.mode = mode;
        if (!delay) {
            CellSpec d; d.kind = CellSpec::Kind::knob; d.id = "hybrid.delay"; d.label = "B DELAY"; d.unit = "MS";
            delay = std::make_unique<ParamCell>(d, state.getParameter("hybrid.delay"));
            CellSpec t; t.kind = CellSpec::Kind::segments; t.id = "hybrid.transpose"; t.label = "B TRANSPOSE"; t.span = 1;
            t.options = {utf8("\xe2\x88\x92" "24"), utf8("\xe2\x88\x92" "12"), "0", "+12", "+24"}; t.values = {-24, -12, 0, 12, 24};
            transpose = std::make_unique<ParamCell>(t, state.getParameter("hybrid.transpose"));
            delay->setComponentID("hybrid.home.delay"); transpose->setComponentID("hybrid.home.transpose");
            addAndMakeVisible(*delay); addAndMakeVisible(*transpose);
            for (int side = 0; side < 2; ++side) {
                CellSpec c; c.kind = CellSpec::Kind::knob; c.id = side == 0 ? "hybrid.gain_a" : "hybrid.gain_b"; c.label = "BASE GAIN"; c.unit = "DB";
                c.format = [](float db) {   // −60…+12 dB: el arco va desde el mínimo, el signo se escribe
                    const int v = juce::roundToInt(db);
                    return v > 0 ? "+" + juce::String(v) : v < 0 ? utf8("\xe2\x88\x92") + juce::String(-v) : juce::String("0");
                };
                gain[size_t(side)] = std::make_unique<ParamCell>(c, state.getParameter(c.id));
                gain[size_t(side)]->setComponentID(side == 0 ? "hybrid.home.gain_a" : "hybrid.home.gain_b");
                addAndMakeVisible(*gain[size_t(side)]);
            }
        }
        delay->mode = transpose->mode = mode;
        for (auto& c : gain) c->mode = mode;
        resized(); repaint();
    }
    int scroll() const { return scroll_; }
    static juce::Rectangle<int> link_rect() { return {680, 262, 64, 20}; }
    /** LINK (§9.2): al empezar a mover CUTOFF (0) o RESONANCE (1) de la mitad en foco se anotan los dos valores… */
    void begin_follow(int which) {
        auto* mine = which == 0 ? cutoff : resonance;
        auto* other = which == 0 ? other_cutoff : other_resonance;
        if (!mine || !other) return;
        follow_start_[0] = mine->convertFrom0to1(mine->getValue());
        follow_start_[1] = other->convertFrom0to1(other->getValue());
        following_ = link;
        if (following_) other->beginChangeGesture();
    }
    /** …y en cada paso la otra mitad se mueve en la misma proporción (por diferencia si la inicial era 0). */
    void follow(int which) {
        if (!following_) return;
        auto* mine = which == 0 ? cutoff : resonance;
        auto* other = which == 0 ? other_cutoff : other_resonance;
        if (!mine || !other) return;
        const float now = mine->convertFrom0to1(mine->getValue());
        const auto range = other->getNormalisableRange();
        const float target = follow_start_[0] > 1e-3f ? follow_start_[1] * now / follow_start_[0] : follow_start_[1] + (now - follow_start_[0]);
        other->setValueNotifyingHost(other->convertTo0to1(juce::jlimit(range.start, range.end, target)));
        repaint();
    }
    void end_follow(int which) {
        auto* other = which == 0 ? other_cutoff : other_resonance;
        if (following_ && other) other->endChangeGesture();
        following_ = false;
    }
    /** Franja ▲ CPU HIGH (nota CPU de 2h): aparece sobre 80 % y se va bajo 70 %. @return si se muestra. */
    bool update_cpu(float load) {
        const bool before = cpu_high_;
        if (load > 0.8f) cpu_high_ = true;
        else if (load < 0.7f) cpu_high_ = false;
        cpu_shown_ = load;
        if (before != cpu_high_) repaint();
        return cpu_high_;
    }
    bool cpu_high() const { return cpu_high_; }

    void resized() override {
        if (delay) delay->setBounds(600, 152, 88, 88);
        if (transpose) transpose->setBounds(696, 152, 184, 88);
        for (int side = 0; side < 2; ++side)
            if (gain[size_t(side)]) gain[size_t(side)]->setBounds(card_rect(side).getX() + 184, card_rect(side).getY() + 112, 88, 88);
        // las perillas héroe de PERFORM (250 × 240) al 80 %: ocupan 200 × 192 del bloque de 200 × 216
        hero_cutoff.setBounds(960, 320, 250, 240);
        hero_resonance.setBounds(1230, 320, 250, 240);
    }
    void paint(juce::Graphics& g) override {
        g.fillAll(colour::BG);
        auto panel = [&](juce::Rectangle<int> r, juce::Colour border = colour::LINE, float w = 1.0f) {
            g.setColour(colour::SURFACE); g.fillRect(r); g.setColour(border); g.drawRect(r.toFloat(), w);
        };
        // lista de capas
        panel({16, 16, 264, 456});
        const auto names = layer_names ? layer_names() : juce::StringArray();
        const int loaded = loaded_layer ? loaded_layer() : -1;
        g.setFont(mono(11.0f)); g.setColour(colour::GRAY);
        g.drawText("LAYER PRESETS", 24, 16, 160, 32, juce::Justification::left);
        g.setColour(colour::MON_TEXT);
        g.drawText(juce::String(names.size()) + utf8(" / 32 \xc2\xb7 CC 18"), 120, 16, 152, 32, juce::Justification::right);
        g.setColour(colour::LINE); g.drawHorizontalLine(47, 17, 279);
        for (int row = 0; row < LIST_ROWS && scroll_ + row < names.size(); ++row) {
            const int i = scroll_ + row;
            const juce::Rectangle<int> r(LIST_X + 1, LIST_Y + row * ROW_H, LIST_W - 2, ROW_H);
            if (i == loaded) { g.setColour(mode.dim); g.fillRect(r); }
            g.setColour(colour::SURFACE_2); g.drawHorizontalLine(r.getBottom() - 1, float(r.getX()), float(r.getRight()));
            g.setFont(mono(11.0f)); g.setColour(colour::MON_TEXT);
            g.drawText(juce::String(i + 1).paddedLeft('0', 2), r.getX() + 8, r.getY(), 20, ROW_H, juce::Justification::left);
            g.setFont(mono(13.0f)); g.setColour(colour::WHITE);
            g.drawText(names[i] + (i == loaded && modified && modified() ? " *" : ""), r.getX() + 36, r.getY(), 160, ROW_H, juce::Justification::left, true);
            if (i == loaded) {
                const bool mod = modified && modified();
                g.setFont(mono(11.0f)); g.setColour(mod ? colour::AMBER : mode.hi);
                g.drawText(mod ? "MODIFIED" : "LOADED", r.reduced(8, 0), juce::Justification::right);
            }
        }
        g.setColour(colour::LINE); g.drawHorizontalLine(save_row().getY(), 17, 279);
        g.setFont(mono(13.0f));
        g.setColour(mode.hi); g.drawText("+ SAVE LAYER", save_row().reduced(8, 0), juce::Justification::left);
        g.setColour(mode.hi); g.drawText("+ CUSTOM", custom_row().reduced(8, 0), juce::Justification::left);
        // tarjetas A y B
        for (int side = 0; side < 2; ++side) {
            const auto r = card_rect(side);
            const bool f = side == focus;
            panel(r, f ? mode.hi : colour::LINE, f ? 2.0f : 1.0f);
            const juce::Rectangle<float> badge(float(r.getX() + 16), float(r.getY() + 16), 24, 24);
            if (f) { g.setColour(mode.hi); g.fillRect(badge); }
            g.setColour(f ? mode.hi : colour::MON); g.drawRect(badge, 1.0f);
            g.setFont(mono(13.0f, true)); g.setColour(f ? mode.on : colour::GRAY);
            g.drawText(side == 0 ? "A" : "B", badge, juce::Justification::centred);
            g.setColour(f ? colour::WHITE : colour::GRAY);
            g.drawText(engines()[size_t(engine[size_t(side)])].name, r.getX() + 48, r.getY() + 16, 120, 24, juce::Justification::left);
            g.setFont(mono(11.0f)); g.setColour(colour::GRAY);
            g.drawText(utf8(engines()[size_t(engine[size_t(side)])].tag), r.getX() + 132, r.getY() + 16, 88, 24, juce::Justification::left);
            if (f) { g.setColour(mode.hi); g.drawText("EDITING", r.getRight() - 76, r.getY() + 16, 60, 24, juce::Justification::right); }
            draw_engine(g, engine[size_t(side)], juce::Rectangle<float>(float(r.getX() + 16), float(r.getY() + 48), 248, 56), f ? 1.0f : 0.5f);
            g.setFont(mono(16.0f, true)); g.setColour(f ? colour::WHITE : colour::GRAY);
            g.drawText(half_name ? half_name(side) : juce::String(), r.getX() + 16, r.getY() + 112, 164, 20, juce::Justification::left, true);
            g.setFont(mono(11.0f)); g.setColour(colour::GRAY);
            g.drawText(utf8(side == 0 ? "HALF A \xc2\xb7 IN THE LAYER" : "HALF B \xc2\xb7 IN THE LAYER"), r.getX() + 16, r.getY() + 132, 164, 16, juce::Justification::left);
            juce::Path wave;
            PerformSource s; s.engine = engine[size_t(side)];
            for (int x = 0; x <= 160; x += 2) {
                const float y = float(r.getY() + 172) - perform_math::osc_sample(s, 2.0f * float(x) / 160.0f, 0.5f) * 12.0f;
                if (x == 0) wave.startNewSubPath(float(r.getX() + 16), y); else wave.lineTo(float(r.getX() + 16 + x), y);
            }
            g.setColour(f ? mode.hi : mode.dim); g.strokePath(wave, juce::PathStrokeType(2.0f));
            g.setColour(colour::MON_TEXT);
            g.drawText(utf8(side == 0 ? "ATTACK \xc2\xb7 DEFINITION" : "BODY \xc2\xb7 WEIGHT \xc2\xb7 SPACE"), r.getX() + 16, r.getY() + 200, 248, 16, juce::Justification::left);
        }
        // MORPH
        panel(morph_rect());
        const float m = morph ? morph->getValue() : 0.5f;
        g.setFont(mono(11.0f)); g.setColour(colour::GRAY);
        g.drawText(utf8("MORPH \xc2\xb7 CC 17"), 600, 24, 160, 24, juce::Justification::left);
        g.setFont(mono(28.0f)); g.setColour(colour::WHITE);
        g.drawText(juce::String(juce::roundToInt(m * 100.0f)) + " %", 720, 16, 160, 40, juce::Justification::right);
        const auto mc = morph_curve().toFloat();
        juce::Path pa, pb;
        for (int x = 0; x <= 280; x += 4) {
            const float u = float(x) / 280.0f, ya = mc.getBottom() - 4 - 48 * hybrid_math::gain_a(u) / std::sqrt(2.0f), yb = mc.getBottom() - 4 - 48 * hybrid_math::gain_b(u) / std::sqrt(2.0f);
            if (x == 0) { pa.startNewSubPath(mc.getX(), ya); pb.startNewSubPath(mc.getX(), yb); }
            else { pa.lineTo(mc.getX() + float(x), ya); pb.lineTo(mc.getX() + float(x), yb); }
        }
        g.setColour(mode.hi); g.strokePath(pa, juce::PathStrokeType(2.0f));
        juce::Path dashed; const float d[] = {6.0f, 4.0f};
        juce::PathStrokeType(2.0f).createDashedStroke(dashed, pb, d, 2); g.fillPath(dashed);
        g.setColour(colour::WHITE); g.drawLine(mc.getX() + m * 280.0f, mc.getY(), mc.getX() + m * 280.0f, mc.getBottom(), 2.0f);
        g.setFont(mono(11.0f)); g.setColour(colour::GRAY);
        g.drawText(utf8("A \xe2\x94\x80\xe2\x94\x80 ") + juce::String(juce::roundToInt(hybrid_math::gain_a(m) * 100.0f)) + " %", 600, 120, 140, 16, juce::Justification::left);
        g.drawText(utf8("B \xe2\x80\x92 \xe2\x80\x92 ") + juce::String(juce::roundToInt(hybrid_math::gain_b(m) * 100.0f)) + " %", 740, 120, 140, 16, juce::Justification::right);
        // filtro de la mitad en foco
        panel(filter_rect());
        g.setColour(colour::GRAY);
        g.drawText(utf8("FILTER \xc2\xb7 ") + (focus == 0 ? "A" : "B"), 308, 264, 200, 16, juce::Justification::left);
        g.setColour(colour::MON_TEXT);
        const int poly = std::min(hybrid_math::voices(engine[0]), hybrid_math::voices(engine[1]));
        g.drawText("LAYER POLY " + juce::String(poly) + utf8(" \xc2\xb7 ") + engines()[size_t(engine[0])].name + " " + juce::String(hybrid_math::voices(engine[0])) +
                   utf8(" \xc2\xb7 ") + engines()[size_t(engine[1])].name + " " + juce::String(hybrid_math::voices(engine[1])), 360, 264, 310, 16, juce::Justification::right);
        PerformSource s; s.engine = engine[size_t(focus)];
        const float fc = cutoff ? cutoff->convertFrom0to1(cutoff->getValue()) : 1000.0f;
        s.res_norm = resonance ? juce::jlimit(0.0f, 1.0f, resonance->convertFrom0to1(resonance->getValue())) : 0.0f;
        const juce::Rectangle<float> cb(297, 288, 454, 176);
        juce::Path curve, fill;
        auto db_y = [&](float db) { return juce::jlimit(cb.getY() + 1, cb.getBottom() - 1, cb.getY() + cb.getHeight() * ((18.0f - db) / 78.0f)); };
        for (int i = 0; i <= 113; ++i) {
            const float x = cb.getX() + float(i) * cb.getWidth() / 113.0f, y = db_y(perform_math::response_db(s, perform_math::frac_freq(float(i) / 113.0f), fc));
            if (i == 0) { curve.startNewSubPath(x, y); fill.startNewSubPath(x, cb.getBottom()); fill.lineTo(x, y); }
            else { curve.lineTo(x, y); fill.lineTo(x, y); }
        }
        fill.lineTo(cb.getRight(), cb.getBottom()); fill.closeSubPath();
        g.setColour(mode.dim); g.fillPath(fill);
        g.setColour(mode.hi); g.strokePath(curve, juce::PathStrokeType(2.0f));
        g.setColour(colour::WHITE);
        g.drawVerticalLine(int(cb.getX() + perform_math::freq_frac(fc) * cb.getWidth()), cb.getY(), cb.getBottom());
        // perillas de la mitad en foco; la nota dice el valor de la otra mitad
        panel({768, 256, 200, 216}); panel({984, 256, 200, 216});
        g.setFont(mono(11.0f)); g.setColour(colour::MON_TEXT);
        if (other_cutoff && cutoff) {
            const float mine_hz = cutoff->convertFrom0to1(cutoff->getValue()), other_hz = other_cutoff->convertFrom0to1(other_cutoff->getValue());
            const juce::String other_name = focus == 0 ? "B" : "A", mine_name = focus == 0 ? "A" : "B";
            g.drawText(link ? other_name + " FOLLOWS " + mine_name + utf8(" \xc2\xb7 ") + juce::String(other_hz / std::max(1.0f, mine_hz), 2) + utf8(" \xc3\x97")
                            : other_name + " " + juce::String(juce::roundToInt(other_hz)) + " HZ",
                       768, 448, 200, 16, juce::Justification::centred);
        }
        {
            const auto lr = link_rect();
            if (link) { g.setColour(mode.hi); g.fillRect(lr); }
            g.setColour(link ? mode.hi : colour::MON); g.drawRect(lr, 1);
            g.setColour(link ? mode.on : colour::GRAY); g.drawText(link ? "LINK ON" : "LINK OFF", lr, juce::Justification::centred);
        }
        g.drawText(utf8("HALF ") + (focus == 0 ? "A" : "B"), 984, 448, 200, 16, juce::Justification::centred);
    }
    void paintOverChildren(juce::Graphics& g) override {
        if (!cpu_high_) return;
        const juce::Rectangle<int> t(296, 440, 456, 32);
        g.setColour(colour::AMBER); g.fillRect(t);
        g.setColour(colour::BG); g.setFont(mono(11.0f, true));
        g.drawText(utf8("\xe2\x96\xb2 CPU HIGH"), t.reduced(12, 0), juce::Justification::left);
        g.setFont(mono(11.0f));
        g.drawText(juce::String(juce::roundToInt(cpu_shown_ * 100.0f)) + utf8(" % \xc2\xb7 LIGHTEN FX OR LOWER POLY"), t.reduced(12, 0), juce::Justification::right);
    }

    void mouseDown(const juce::MouseEvent& e) override {
        const auto p = e.getPosition();
        dragging_morph_ = false;
        for (int side = 0; side < 2; ++side)
            if (card_rect(side).contains(p)) { if (side != focus && on_focus) on_focus(side); return; }
        if (save_row().contains(p)) { if (on_save_layer) on_save_layer(); return; }
        if (link_rect().contains(p)) { link = !link; if (on_link) on_link(link); repaint(); return; }
        if (custom_row().contains(p)) { if (on_custom) on_custom(); return; }
        if (morph && morph_rect().contains(p) && p.y < 148) { dragging_morph_ = true; morph->beginChangeGesture(); mouseDrag(e); return; }
        if (juce::Rectangle<int>(LIST_X, LIST_Y, LIST_W, LIST_ROWS * ROW_H).contains(p)) {
            const int i = scroll_ + (p.y - LIST_Y) / ROW_H;
            const auto names = layer_names ? layer_names() : juce::StringArray();
            if (i < names.size() && on_load_layer) on_load_layer(i);
        }
    }
    void mouseDrag(const juce::MouseEvent& e) override {
        if (!dragging_morph_ || !morph) return;
        morph->setValueNotifyingHost(juce::jlimit(0.0f, 1.0f, (e.position.x - float(morph_curve().getX())) / 280.0f));
        repaint(morph_rect());
    }
    void mouseUp(const juce::MouseEvent&) override { if (dragging_morph_ && morph) morph->endChangeGesture(); dragging_morph_ = false; }
    void mouseDoubleClick(const juce::MouseEvent& e) override {
        for (int side = 0; side < 2; ++side) if (card_rect(side).contains(e.getPosition()) && on_open_osc) on_open_osc(side);
    }
    void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override {
        if (!juce::Rectangle<int>(16, 16, 264, 456).contains(e.getPosition())) return;
        const int count = layer_names ? layer_names().size() : 0;
        scroll_ = juce::jlimit(0, std::max(0, count - LIST_ROWS), scroll_ + (w.deltaY < 0 ? 1 : -1));
        repaint();
    }

private:
    int scroll_ = 0;
    bool dragging_morph_ = false, cpu_high_ = false, following_ = false;
    std::array<float, 2> follow_start_{};
    float cpu_shown_ = 0;
    std::array<float, 5> last_{};
    void timerCallback() override {
        if (!isShowing()) return;
        if (cpu) update_cpu(cpu());
        const std::array<float, 5> now{morph ? morph->getValue() : 0, cutoff ? cutoff->getValue() : 0, resonance ? resonance->getValue() : 0,
                                       delay && delay->parameter() ? delay->parameter()->getValue() : 0, transpose && transpose->parameter() ? transpose->parameter()->getValue() : 0};
        if (now != last_) { last_ = now; repaint(); }
    }
};

}  // namespace studio_ui
