#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <memory>
#include <vector>

#include "engines/dx7_core.h"
#include "perform_view.h"
#include "studio_shell.h"

/* OSC de la referencia (1b–1d; gemelo-vst-ui-migracion.md §4): una fila de paneles por engine. Nombres, rangos y
 * unidades de la tabla de slots del firmware (GF_OSC_SLOT, gf_params.h) y de osc_catalog.cpp de la cabeza. */
namespace studio_ui {

/** Una celda de panel (`VstParam` de la referencia): perilla, segmentos, interruptor o valor con pasos. */
struct CellSpec {
    enum class Kind { knob, segments, toggle, value } kind = Kind::knob;
    const char* id = nullptr;
    juce::String label, unit, prefix;           ///< prefix: delante del número (W de WAVE en el DX7)
    juce::StringArray options;                       ///< segmentos: un texto por valor desde el inicio del rango
    int span = 1, decimals = 0;
    bool bipolar = false;
    float scale = 1.0f;                              ///< valor mostrado = valor del parámetro × scale (0–1 → %)
};

namespace osc_math {
/// Red de modulación del DX7 de 4 operadores: copia de `MODS` de dx7_core.cpp (bits 0–3 = OP1–OP4 que modulan a cada
/// operador). Un test la compara con dx7_is_carrier.
inline constexpr uint8_t DX7_MODS[8][4] = {
    {1 << 1, 1 << 2, 1 << 3, 0}, {1 << 1, (1 << 2) | (1 << 3), 0, 0}, {(1 << 1) | (1 << 3), 1 << 2, 0, 0},
    {(1 << 1) | (1 << 2), 0, 1 << 3, 0}, {1 << 1, 0, 1 << 3, 0}, {1 << 3, 1 << 3, 1 << 3, 0}, {0, 0, 1 << 3, 0}, {0, 0, 0, 0}};

inline bool modulates(int alg, int modulator, int target) { return (DX7_MODS[juce::jlimit(1, 8, alg) - 1][target] >> modulator) & 1; }
inline bool is_carrier(int alg, int op) {
    for (int t = 0; t < 4; ++t) if (modulates(alg, op, t)) return false;
    return true;
}
/// Primer operador al que modula `op` (−1 si es portadora): el subtítulo "MOD → OP n".
inline int first_target(int alg, int op) {
    for (int t = 0; t < 4; ++t) if (modulates(alg, op, t)) return t;
    return -1;
}
/// Índice de modulación en ciclos desde el LEVEL 0–99 (la escala de PERFORM, perform_math::osc_sample).
inline float fm_index(float level) {
    return level > 0 ? 6.0f * std::pow(2.0f, (level - 99.0f) / 8.0f) / juce::MathConstants<float>::twoPi : 0.0f;
}
/// Ruido estable para dibujar (el mismo hash de la referencia).
inline float noise(int i) { const double x = std::sin(double(i) * 12.9898) * 43758.5453; return float(x - std::floor(x)) * 2.0f - 1.0f; }

/** Posiciones del diagrama: nivel = camino más largo hasta una portadora; x de las portadoras repartida, cada
 *  modulador centrado sobre sus destinos y separado de sus vecinos de nivel. Devuelve x (0–1) y nivel por operador. */
inline void algorithm_layout(int alg, std::array<float, 4>& x, std::array<int, 4>& level) {
    level = {0, 0, 0, 0};
    for (int pass = 0; pass < 4; ++pass)
        for (int op = 0; op < 4; ++op)
            for (int t = 0; t < 4; ++t) if (modulates(alg, op, t)) level[size_t(op)] = std::max(level[size_t(op)], level[size_t(t)] + 1);
    int carriers = 0;
    for (int op = 0; op < 4; ++op) if (level[size_t(op)] == 0) ++carriers;
    int k = 0;
    for (int op = 0; op < 4; ++op) if (level[size_t(op)] == 0) x[size_t(op)] = (float(k++) + 0.5f) / float(carriers);
    for (int lv = 1; lv < 4; ++lv) {
        std::vector<int> row;
        for (int op = 0; op < 4; ++op) {
            if (level[size_t(op)] != lv) continue;
            float sum = 0; int n = 0;
            for (int t = 0; t < 4; ++t) if (modulates(alg, op, t)) { sum += x[size_t(t)]; ++n; }
            x[size_t(op)] = n ? sum / float(n) : 0.5f;
            row.push_back(op);
        }
        std::sort(row.begin(), row.end(), [&](int a, int b) { return x[size_t(a)] < x[size_t(b)] || (x[size_t(a)] == x[size_t(b)] && a < b); });
        constexpr float GAP = 0.34f;
        for (size_t i = 1; i < row.size(); ++i)
            if (x[size_t(row[i])] - x[size_t(row[i - 1])] < GAP) {
                // el grupo que choca se reparte alrededor de su centro
                float mid = 0; for (int op : row) mid += x[size_t(op)];
                mid /= float(row.size());
                for (size_t j = 0; j < row.size(); ++j)
                    x[size_t(row[j])] = juce::jlimit(0.12f, 0.88f, mid + (float(j) - 0.5f * float(row.size() - 1)) * GAP);
                break;
            }
    }
}
}  // namespace osc_math

/** Diagrama de un algoritmo en `box`: cajas de 28 × 22, portadoras abajo unidas por una barra, feedback en OP 4. */
inline void draw_algorithm(juce::Graphics& g, juce::Rectangle<float> box, int alg, int selected, const Mode& m) {
    std::array<float, 4> fx{}; std::array<int, 4> lv{};
    osc_math::algorithm_layout(alg, fx, lv);
    const int top_level = *std::max_element(lv.begin(), lv.end());
    const float bus = box.getBottom() - 12, step = top_level ? std::min(34.0f, (bus - box.getY() - 34.0f) / float(top_level)) : 0.0f;
    auto rect = [&](int op) {
        const float cx = box.getX() + 16 + fx[size_t(op)] * (box.getWidth() - 32);
        return juce::Rectangle<float>(cx - 14, bus - 30 - float(lv[size_t(op)]) * step, 28, 22);
    };
    g.setColour(colour::MON);
    float lo = 1e9f, hi = -1e9f;
    for (int op = 0; op < 4; ++op) {
        const auto r = rect(op);
        for (int t = 0; t < 4; ++t)
            if (osc_math::modulates(alg, op, t)) g.drawLine(r.getCentreX(), r.getBottom(), rect(t).getCentreX(), rect(t).getY(), 2.0f);
        if (lv[size_t(op)] == 0) { g.drawLine(r.getCentreX(), r.getBottom(), r.getCentreX(), bus, 2.0f); lo = std::min(lo, r.getCentreX()); hi = std::max(hi, r.getCentreX()); }
    }
    g.drawLine(lo - 1, bus, hi + 1, bus, 2.0f);
    const auto f = rect(3);
    juce::Path loop;
    loop.startNewSubPath(f.getRight(), f.getCentreY()); loop.lineTo(f.getRight() + 6, f.getCentreY());
    loop.lineTo(f.getRight() + 6, f.getY() - 5); loop.lineTo(f.getCentreX(), f.getY() - 5); loop.lineTo(f.getCentreX(), f.getY());
    g.strokePath(loop, juce::PathStrokeType(1.5f));
    g.setFont(mono(11.0f));
    for (int op = 0; op < 4; ++op) {
        const auto r = rect(op);
        g.setColour(colour::BG); g.fillRect(r);
        const bool sel = op == selected, car = lv[size_t(op)] == 0;
        g.setColour(sel ? m.hi : car ? m.mc : colour::MON); g.drawRect(r, sel ? 2.0f : 1.0f);
        g.setColour(sel ? colour::WHITE : colour::GRAY); g.drawText(juce::String(op + 1), r, juce::Justification::centred);
    }
}

/** Celda editable ligada a un parámetro. Arrastre vertical (Shift = fino), rueda, doble clic = valor por defecto. */
class ParamCell final : public juce::Component {
public:
    CellSpec spec;
    Mode mode = synth_mode();
    std::function<void()> on_touch;                  ///< el panel mueve el foco

    ParamCell(CellSpec s, juce::RangedAudioParameter* p) : spec(std::move(s)), parameter_(p) {
        if (spec.kind == CellSpec::Kind::toggle) spec.options = {"OFF", "ON"};
    }
    juce::RangedAudioParameter* parameter() const { return parameter_; }
    float raw() const { return parameter_ ? parameter_->convertFrom0to1(parameter_->getValue()) : 0.0f; }
    int index() const { return parameter_ ? juce::roundToInt(raw() - parameter_->getNormalisableRange().start) : 0; }
    /** Valor como lo muestra la celda (texto + unidad), para la tira. */
    juce::String value_text() const {
        if (!parameter_) return utf8("\xe2\x80\x94");
        if (spec.kind == CellSpec::Kind::segments || spec.kind == CellSpec::Kind::toggle)
            return spec.options[juce::jlimit(0, spec.options.size() - 1, index())];
        return number() + (spec.unit.isEmpty() ? juce::String() : " " + spec.unit);
    }
    void set_index(int i) {
        if (!parameter_) return;
        const float v = parameter_->getNormalisableRange().start + float(i);
        parameter_->beginChangeGesture(); parameter_->setValueNotifyingHost(parameter_->convertTo0to1(v)); parameter_->endChangeGesture();
        repaint();
    }

    void paint(juce::Graphics& g) override {
        const bool on = parameter_ != nullptr;
        const float w = float(getWidth());
        g.setFont(mono(11.0f)); g.setColour(on ? colour::GRAY : colour::MON);
        g.drawText(spec.label, 0, 4, getWidth(), 16, juce::Justification::centred);
        if (spec.kind == CellSpec::Kind::knob) {
            const float cx = w / 2, cy = 48, a0 = -0.75f * juce::MathConstants<float>::pi, a1 = 0.75f * juce::MathConstants<float>::pi;
            const float v = on ? parameter_->getValue() : 0.0f, av = a0 + v * (a1 - a0), from = spec.bipolar ? 0.0f : a0;
            g.setColour(colour::BG); g.fillEllipse(cx - 15, cy - 15, 30, 30);
            g.setColour(colour::LINE); g.drawEllipse(cx - 15, cy - 15, 30, 30, 1.0f);
            auto arc = [&](float s, float e, juce::Colour c) {
                juce::Path p; p.addCentredArc(cx, cy, 19.5f, 19.5f, 0, std::min(s, e), std::max(s, e), true);
                g.setColour(c); g.strokePath(p, juce::PathStrokeType(3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::butt));
            };
            arc(a0, a1, on ? mode.dim : colour::SURFACE_2);
            if (on && std::abs(av - from) > 0.01f) arc(from, av, mode.hi);
            g.setColour(on ? colour::WHITE : colour::MON);
            g.drawLine(cx + std::sin(av) * 6, cy - std::cos(av) * 6, cx + std::sin(av) * 15, cy - std::cos(av) * 15, 2.5f);
            draw_value(g, 70);
        } else if (spec.kind == CellSpec::Kind::value) {
            g.setFont(mono(20.0f)); g.setColour(on ? colour::WHITE : colour::MON);
            const auto n = number();
            const int nw = int(float(n.length()) * 12.0f), uw = int(float(spec.unit.length()) * 6.7f);
            const int x0 = (getWidth() - nw - uw - 4) / 2;
            g.drawText(n, x0, 28, nw, 32, juce::Justification::left);
            g.setFont(mono(11.0f)); g.setColour(colour::GRAY);
            g.drawText(spec.unit, x0 + nw + 4, 38, uw + 4, 16, juce::Justification::left);
        } else {
            const auto rects = option_rects();
            for (int i = 0; i < spec.options.size(); ++i) {
                const auto r = rects[size_t(i)];
                const bool sel = on && i == index();
                if (sel) { g.setColour(mode.hi); g.fillRect(r); }
                g.setColour(sel ? mode.hi : on ? mode.dim : colour::LINE); g.drawRect(r, 1.0f);
                g.setColour(sel ? mode.on : on ? colour::GRAY : colour::MON); g.setFont(mono(11.0f));
                g.drawText(spec.options[i], r, juce::Justification::centred);
            }
        }
    }
    /** Rectángulos de los segmentos: ancho por texto, en filas centradas que se parten si no caben. */
    /** Alto que necesita la celda a un ancho dado: perilla 88, valor 64, segmentos según cuántas filas ocupen. */
    int preferred_height(int width) {
        if (spec.kind == CellSpec::Kind::knob) return 88;
        if (spec.kind == CellSpec::Kind::value) return 64;
        setSize(width, 88);
        float bottom = 0;
        for (const auto& r : option_rects()) bottom = std::max(bottom, r.getBottom());
        return int(bottom) + 4;
    }
    std::vector<juce::Rectangle<float>> option_rects() const {
        std::vector<juce::Rectangle<float>> out;
        std::vector<float> widths;
        for (const auto& t : spec.options) widths.push_back(float(t.length()) * 6.7f + 12.0f);
        const float max_w = float(getWidth()) - 4;
        size_t start = 0;
        float y = 26;
        while (start < widths.size()) {
            float row = widths[start]; size_t end = start + 1;
            while (end < widths.size() && row + 4 + widths[end] <= max_w) row += 4 + widths[end++];
            float x = (float(getWidth()) - row) / 2;
            for (size_t i = start; i < end; ++i) { out.emplace_back(x, y, widths[i], 24.0f); x += widths[i] + 4; }
            start = end; y += 28;
        }
        return out;
    }

    void mouseDown(const juce::MouseEvent& e) override {
        if (on_touch) on_touch();
        if (!parameter_) return;
        if (spec.kind == CellSpec::Kind::segments || spec.kind == CellSpec::Kind::toggle) {
            const auto rects = option_rects();
            for (size_t i = 0; i < rects.size(); ++i) if (rects[i].contains(e.position)) { set_index(int(i)); return; }
            return;
        }
        start_ = parameter_->getValue(); dragged_ = false; parameter_->beginChangeGesture();
    }
    void mouseDrag(const juce::MouseEvent& e) override {
        if (!parameter_ || !drags()) return;
        dragged_ = dragged_ || std::abs(e.getDistanceFromDragStartY()) > 2;
        const float speed = spec.kind == CellSpec::Kind::value ? 0.012f : 0.004f;
        parameter_->setValueNotifyingHost(juce::jlimit(0.0f, 1.0f, start_ - float(e.getDistanceFromDragStartY()) * speed * (e.mods.isShiftDown() ? 0.15f : 1.0f)));
        repaint();
    }
    void mouseUp(const juce::MouseEvent&) override {
        if (!parameter_ || !drags()) return;
        if (spec.kind == CellSpec::Kind::value && !dragged_) {   // clic = siguiente, dando la vuelta
            const auto range = parameter_->getNormalisableRange();
            const int count = juce::roundToInt(range.end - range.start) + 1;
            parameter_->setValueNotifyingHost(parameter_->convertTo0to1(range.start + float((index() + 1) % count)));
        }
        parameter_->endChangeGesture(); repaint();
    }
    void mouseDoubleClick(const juce::MouseEvent&) override {
        if (!parameter_ || spec.kind != CellSpec::Kind::knob) return;
        parameter_->beginChangeGesture(); parameter_->setValueNotifyingHost(parameter_->getDefaultValue()); parameter_->endChangeGesture();
        repaint();
    }
    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails& wheel) override {
        if (!parameter_ || !drags()) return;
        const auto range = parameter_->getNormalisableRange();
        const float step = range.interval > 0 ? range.interval / (range.end - range.start) : 0.01f;
        parameter_->beginChangeGesture();
        parameter_->setValueNotifyingHost(juce::jlimit(0.0f, 1.0f, parameter_->getValue() + (wheel.deltaY > 0 ? step : -step)));
        parameter_->endChangeGesture(); repaint();
    }

private:
    juce::RangedAudioParameter* parameter_ = nullptr;
    float start_ = 0;
    bool dragged_ = false;

    bool drags() const { return spec.kind == CellSpec::Kind::knob || spec.kind == CellSpec::Kind::value; }
    juce::String number() const {
        if (!parameter_) return utf8("\xe2\x80\x94");
        const float v = raw() * spec.scale;
        const auto text = juce::String(v, spec.decimals);
        if (spec.prefix.isNotEmpty()) return spec.prefix + text;
        return spec.bipolar && v > 0.0f ? "+" + text : spec.bipolar && v < 0.0f ? utf8("\xe2\x88\x92") + text.substring(1) : text;
    }
    void draw_value(juce::Graphics& g, int y) {
        const bool on = parameter_ != nullptr;
        const auto n = number();
        const int nw = int(float(n.length()) * 7.9f), uw = spec.unit.isEmpty() ? 0 : int(float(spec.unit.length()) * 6.7f) + 4;
        const int x0 = (getWidth() - nw - uw) / 2;
        g.setFont(mono(13.0f)); g.setColour(on ? colour::WHITE : colour::MON);
        g.drawText(on ? n : juce::String("OFF"), x0, y, nw + 8, 16, juce::Justification::left);
        if (on && uw) { g.setFont(mono(11.0f)); g.setColour(colour::GRAY); g.drawText(spec.unit, x0 + nw + 4, y + 1, uw + 4, 16, juce::Justification::left); }
    }
};

/** Un panel de la fila: cabecera, onda (o diagrama de algoritmo), grilla de celdas y el mapeo del panel físico al pie. */
class OscPanel final : public juce::Component {
public:
    juce::String name, sub, tag, foot;
    bool narrow = false, focused = false, algorithm = false;
    int p1 = -1, p2 = -1;                            ///< celdas que editan P1 / P2 del panel físico
    Mode mode = synth_mode();
    std::function<float(float)> wave;                ///< fase en ciclos → −1…1
    std::function<int()> algorithm_number;           ///< para el diagrama
    int selected_op = -1;
    std::function<void()> on_focus, on_diagram;
    std::vector<std::unique_ptr<ParamCell>> cells;

    ParamCell* add(CellSpec spec, juce::RangedAudioParameter* p) {
        cells.push_back(std::make_unique<ParamCell>(std::move(spec), p));
        auto* c = cells.back().get();
        c->on_touch = [this] { if (on_focus) on_focus(); };
        c->setComponentID("osc.cell." + juce::String(c->spec.id));
        addAndMakeVisible(*c);
        return c;
    }
    juce::Rectangle<float> wave_box() const { return {16.0f, 48.0f, float(getWidth()) - 32.0f, 112.0f}; }
    void resized() override {
        // filas de alto variable: cada fila mide lo que pide su celda más alta
        const int cols = narrow ? 1 : getWidth() >= 560 ? 4 : getWidth() >= 280 ? 3 : 2;
        const float cw = (float(getWidth()) - 32.0f) / float(cols);
        int col = 0, y = 168, row_h = 0;
        for (auto& c : cells) {
            const int span = std::min(cols, c->spec.span);
            if (col + span > cols) { col = 0; y += row_h + 8; row_h = 0; }
            const int w = int(cw * float(span)), h = c->preferred_height(w);
            c->setBounds(int(16.0f + float(col) * cw), y, w, h);
            row_h = std::max(row_h, h);
            col += span;
        }
    }
    void paint(juce::Graphics& g) override {
        auto r = getLocalBounds().toFloat();
        g.setColour(colour::SURFACE); g.fillRect(r);
        g.setColour(focused ? mode.hi : colour::LINE); g.drawRect(r, focused ? 2.0f : 1.0f);
        g.setFont(mono(16.0f, true)); g.setColour(colour::WHITE);
        const int nw = int(float(name.length()) * 10.0f);
        g.drawText(name, 16, 16, nw + 4, 22, juce::Justification::left);
        g.setFont(mono(11.0f)); g.setColour(colour::GRAY);
        g.drawText(sub, 16 + nw + 8, 20, getWidth() - nw - 40, 16, juce::Justification::left);
        g.setColour(focused ? mode.hi : colour::MON_TEXT);
        g.drawText(focused ? utf8("\xe2\x97\x8f NAV") : tag, getWidth() - 116, 20, 100, 16, juce::Justification::right);
        const auto box = wave_box();
        g.setColour(colour::BG); g.fillRect(box);
        g.setColour(colour::LINE); g.drawRect(box, 1.0f);
        if (algorithm && algorithm_number) draw_algorithm(g, box, algorithm_number(), selected_op, mode);
        else if (wave) {
            const float mid = box.getCentreY();
            g.setColour(colour::SURFACE_2); g.drawHorizontalLine(int(mid), box.getX() + 1, box.getRight() - 1);
            juce::Path p;
            const int n = int(box.getWidth()) - 2;
            for (int i = 0; i <= n; i += 2) {
                const float y = mid - juce::jlimit(-1.2f, 1.2f, wave(2.0f * float(i) / float(n))) * 0.7f * box.getHeight() / 2;
                if (i == 0) p.startNewSubPath(box.getX() + 1, y); else p.lineTo(box.getX() + 1 + float(i), y);
            }
            g.setColour(focused ? mode.hi : mode.mc); g.strokePath(p, juce::PathStrokeType(2.0f));
        }
        g.setColour(colour::MON_TEXT); g.setFont(mono(11.0f));
        g.drawFittedText(foot, 16, getHeight() - 36, getWidth() - 32, 28, juce::Justification::bottomLeft, 2);
    }
    void mouseDown(const juce::MouseEvent& e) override {
        if (on_focus) on_focus();
        if (algorithm && wave_box().contains(e.position) && on_diagram) on_diagram();
    }
};

/** Selector de los 8 algoritmos (11c2 de la UI v3), sobre la fila de paneles. */
class AlgorithmPicker final : public juce::Component {
public:
    Mode mode = synth_mode();
    int current = 1;
    std::function<void(int)> on_pick;
    static juce::Rectangle<int> tile(int i) { return {16 + (i % 4) * 292, 40 + (i / 4) * 216, 284, 208}; }
    void paint(juce::Graphics& g) override {
        g.fillAll(colour::BG);
        g.setFont(mono(13.0f, true)); g.setColour(mode.hi); g.drawText("ALGORITHM", 16, 8, 300, 24, juce::Justification::left);
        g.setFont(mono(11.0f)); g.setColour(colour::MON_TEXT);
        g.drawText(utf8("CLICK \xc2\xb7 CHOOSE \xc2\xb7 OUTSIDE \xc2\xb7 CLOSE"), getWidth() - 416, 8, 400, 24, juce::Justification::right);
        for (int i = 0; i < 8; ++i) {
            const auto r = tile(i).toFloat();
            const bool on = i + 1 == current;
            g.setColour(colour::SURFACE); g.fillRect(r);
            g.setColour(on ? mode.hi : colour::LINE); g.drawRect(r, on ? 2.0f : 1.0f);
            g.setColour(on ? mode.hi : colour::WHITE); g.setFont(mono(16.0f, true));
            g.drawText(juce::String(i + 1), r.reduced(12, 8).removeFromTop(20), juce::Justification::left);
            draw_algorithm(g, r.reduced(40, 0).withTrimmedTop(36).withTrimmedBottom(12), i + 1, -1, mode);
        }
    }
    void mouseUp(const juce::MouseEvent& e) override {
        for (int i = 0; i < 8; ++i) if (tile(i).contains(e.getPosition())) { if (on_pick) on_pick(i + 1); return; }
        if (on_pick) on_pick(0);
    }
};

/** La página OSC entera (1200 × 488). `build` arma los paneles del engine; un timer repinta si cambian los valores. */
class OscPage final : public juce::Component, private juce::Timer {
public:
    Mode mode = synth_mode();
    std::function<void()> on_change;                 ///< la tira (P1 / P2) se repinta
    std::vector<std::unique_ptr<OscPanel>> panels;
    AlgorithmPicker picker;

    OscPage() {
        picker.setComponentID("osc.algorithm.picker");
        picker.on_pick = [this](int alg) {
            if (alg > 0 && algorithm_) {
                algorithm_->beginChangeGesture(); algorithm_->setValueNotifyingHost(algorithm_->convertTo0to1(float(alg))); algorithm_->endChangeGesture();
            }
            picker.setVisible(false); refresh();
        };
        addChildComponent(picker);
        startTimerHz(15);
    }
    int kind() const { return kind_; }
    int focus() const { return focus_; }
    void set_focus(int i) {
        focus_ = juce::jlimit(0, std::max(0, int(panels.size()) - 1), i);
        for (size_t p = 0; p < panels.size(); ++p) panels[p]->focused = int(p) == focus_;
        if (kind_ == 4 && !panels.empty()) panels.back()->selected_op = focus_ < 4 ? focus_ : -1;
        repaint();
        if (on_change) on_change();
    }
    void open_picker() {
        if (!algorithm_) return;
        picker.current = juce::roundToInt(algorithm_->convertFrom0to1(algorithm_->getValue()));
        picker.mode = mode; picker.setVisible(true); picker.toFront(false); picker.repaint();
    }
    struct Strip { juce::String p1, p1_value, p2, p2_value, nav; };
    Strip strip() const {
        Strip s{"NO FUNCTION", utf8("\xe2\x80\x94"), "NO FUNCTION", utf8("\xe2\x80\x94"), nav_};
        if (focus_ < 0 || focus_ >= int(panels.size())) return s;
        const auto& p = *panels[size_t(focus_)];
        if (p.p1 >= 0) { s.p1 = p.cells[size_t(p.p1)]->spec.label; s.p1_value = p.cells[size_t(p.p1)]->value_text(); }
        if (p.p2 >= 0) { s.p2 = p.cells[size_t(p.p2)]->spec.label; s.p2_value = p.cells[size_t(p.p2)]->value_text(); }
        return s;
    }
    /** Busca la celda ligada a `id` en cualquier panel (tests y foco). */
    ParamCell* cell(const juce::String& id) const {
        for (auto& p : panels) for (auto& c : p->cells) if (id == c->spec.id) return c.get();
        return nullptr;
    }

    /** Arma los paneles de `kind` (0 Model D · 1 Juno · 3 OB-6 · 4 DX7 · 5 ARP); no hace nada si ya están. */
    void build(int kind, juce::AudioProcessorValueTreeState& state) {
        if (kind == kind_ && state_ == &state) return;
        for (auto& p : panels) removeChildComponent(p.get());
        panels.clear(); algorithm_ = nullptr; picker.setVisible(false);
        kind_ = kind; state_ = &state; focus_ = 0; ids_.clear();
        using K = CellSpec::Kind;
        auto knob = [](const char* id, const char* label, const char* unit, int dec = 0, bool bip = false, float scale = 1.0f) {
            CellSpec s; s.kind = K::knob; s.id = id; s.label = utf8(label); s.unit = utf8(unit); s.decimals = dec; s.bipolar = bip; s.scale = scale; return s; };
        auto seg = [](const char* id, const char* label, juce::StringArray options, int span = 4) {
            CellSpec s; s.kind = K::segments; s.id = id; s.label = utf8(label); s.options = std::move(options); s.span = span; return s; };
        auto toggle = [](const char* id, const char* label) { CellSpec s; s.kind = K::toggle; s.id = id; s.label = utf8(label); return s; };
        auto value = [](const char* id, const char* label, const char* unit) { CellSpec s; s.kind = K::value; s.id = id; s.label = utf8(label); s.unit = utf8(unit); return s; };
        auto panel = [this](const char* name, const char* sub, bool narrow, const char* foot) {
            panels.push_back(std::make_unique<OscPanel>());
            auto* p = panels.back().get();
            p->name = utf8(name); p->sub = utf8(sub); p->narrow = narrow; p->foot = utf8(foot);
            const int index = int(panels.size()) - 1;
            p->on_focus = [this, index] { if (focus_ != index) set_focus(index); };
            p->setComponentID("osc.panel." + juce::String(index));
            addAndMakeVisible(*p);
            return p;
        };
        auto add = [&](OscPanel* p, CellSpec spec) { auto* param = state.getParameter(spec.id); return p->add(std::move(spec), param); };
        const juce::StringArray md_waves{"SAW", "TRI", "SQR", "PULSE", "SHARK", "NARROW"}, arp_waves{"SAW", "TRI", "SQR", "PULSE", "SINE"};
        auto vco = [this](int engine, const char* wave_id, const char* pw_id, const char* shape_id) {
            return [this, engine, wave_id, pw_id, shape_id](float ph) {
                PerformSource s; s.engine = engine; s.wave = raw(wave_id); s.shape = raw(shape_id);
                return perform_math::osc_sample(s, ph, juce::jlimit(0.05f, 0.95f, raw(pw_id, 50.0f) / 100.0f));
            };
        };
        switch (kind) {
            case 0: {   // Model D (1b)
                auto* v1 = panel("VCO 1", "REF", false, "P1 WAVE \xc2\xb7 P2 \xe2\x80\x94 \xc2\xb7 ENC L LEVEL \xc2\xb7 \xe2\x87\xa7 P1 PW (ENGINE)");
                add(v1, seg("modeld.osc1.wave", "WAVE", md_waves)); add(v1, knob("modeld.osc1.level", "LEVEL", "%")); add(v1, knob("modeld.pulse_width", "PW", "%"));
                v1->p1 = 0; v1->wave = vco(0, "modeld.osc1.wave", "modeld.pulse_width", nullptr);
                auto* v2 = panel("VCO 2", "", false, "P1 WAVE \xc2\xb7 P2 DETUNE \xc2\xb7 ENC L LEVEL \xc2\xb7 \xe2\x87\xa7 P1 PW");
                add(v2, seg("modeld.osc2.wave", "WAVE", md_waves)); add(v2, knob("modeld.osc2.detune", "DETUNE", "CT", 0, true)); add(v2, knob("modeld.osc2.level", "LEVEL", "%"));
                v2->p1 = 0; v2->p2 = 1; v2->wave = vco(0, "modeld.osc2.wave", "modeld.pulse_width", nullptr);
                auto* v3 = panel("VCO 3", "", false, "P1 WAVE \xc2\xb7 P2 INTERVAL \xc2\xb7 ENC L LEVEL \xc2\xb7 \xe2\x87\xa7 P2 FINE");
                add(v3, seg("modeld.osc3.wave", "WAVE", {"SAW", "TRI", "SQR", "PULSE", "REV SAW", "NARROW"})); add(v3, knob("modeld.osc3.interval", "INTERVAL", "ST", 0, true));
                add(v3, knob("modeld.osc3.level", "LEVEL", "%")); add(v3, knob("modeld.osc3.detune", "FINE", "CT", 0, true));
                v3->p1 = 0; v3->p2 = 1;
                v3->wave = [this](float ph) {
                    PerformSource s; s.engine = 0; s.wave = raw("modeld.osc3.wave");
                    const float y = perform_math::osc_sample(s, ph, juce::jlimit(0.05f, 0.95f, raw("modeld.pulse_width", 50.0f) / 100.0f));
                    return juce::roundToInt(s.wave) == 4 ? -(2.0f * (ph - std::floor(ph)) - 1.0f) : y;   // SHARK en VCO 3 = saw inversa
                };
                auto* n = panel("NOISE", "", true, "P1 LEVEL \xc2\xb7 P2 COLOR");
                add(n, knob("modeld.noise.level", "LEVEL", "%")); add(n, seg("modeld.noise.color", "COLOR", {"WHITE", "PINK"}, 1));
                n->p1 = 0; n->p2 = 1; n->wave = [this](float ph) { return osc_math::noise(int(ph * 180.0f)) * (0.1f + 0.7f * raw("modeld.noise.level") / 100.0f); };
                nav_ = utf8("VCO 1 \xc2\xb7 2 \xc2\xb7 3 \xc2\xb7 NOISE");
                break;
            }
            case 1: {   // Juno-106: el DCO lleva sus funciones de SHIFT
                auto* d = panel("DCO", "1 OSC", false, "P1 WAVE \xc2\xb7 P2 SUB \xc2\xb7 ENC L NOISE \xc2\xb7 ENC R PW \xc2\xb7 \xe2\x87\xa7 CHORUS \xc2\xb7 HPF \xc2\xb7 RANGE \xc2\xb7 VCA");
                add(d, seg("juno.wave", "WAVE", {"SAW", "SQR", "PULSE", "SAW+PLS"})); add(d, knob("juno.pw", "PW", "%"));
                add(d, seg("juno.range", "RANGE", {"16'", "8'", "4'"}, 1));
                add(d, seg("juno.hpf", "HPF", {"0", "1", "2", "3"}, 1)); add(d, seg("juno.chorus", "CHORUS", {"OFF", "I", "II", "I+II"}, 2));
                add(d, seg("juno.gate", "VCA", {"ENV", "GATE"}, 1));
                d->p1 = 0; d->wave = vco(1, "juno.wave", "juno.pw", nullptr);
                auto* s = panel("SUB", "\xe2\x88\x92" "1 OCT", true, "P2 SUB");
                add(s, knob("juno.sub", "LEVEL", "%")); s->p2 = 0;
                s->wave = [this](float ph) { return (ph * 0.5f - std::floor(ph * 0.5f) < 0.5f ? 1.0f : -1.0f) * (0.15f + 0.85f * raw("juno.sub") / 100.0f); };
                auto* n = panel("NOISE", "", true, "ENC L NOISE \xc2\xb7 HISS = CHORUS BBD NOISE");
                add(n, knob("juno.noise", "LEVEL", "%")); add(n, knob("juno.hiss", "BBD HISS", "%"));
                n->wave = [this](float ph) { return osc_math::noise(int(ph * 180.0f)) * (0.1f + 0.7f * raw("juno.noise") / 100.0f); };
                nav_ = "DCO";
                break;
            }
            case 3: {   // OB-6 (1c)
                auto* v1 = panel("VCO 1", "REF", false, "P1 SHAPE \xc2\xb7 P2 SUB \xc2\xb7 ENC L LEVEL \xc2\xb7 ENC R NOISE \xc2\xb7 \xe2\x87\xa7 P1 PW");
                add(v1, knob("ob6.osc1.shape", "SHAPE", "%")); add(v1, knob("ob6.sub", "SUB", "%"));
                add(v1, knob("ob6.osc1.level", "LEVEL", "%")); add(v1, knob("ob6.osc1.pw", "PW", "%"));
                v1->p1 = 0; v1->p2 = 1; v1->wave = vco(3, nullptr, "ob6.osc1.pw", "ob6.osc1.shape");
                auto* v2 = panel("VCO 2", "", false, "P1 SHAPE \xc2\xb7 P2 INTERVAL \xc2\xb7 ENC L LEVEL \xc2\xb7 \xe2\x87\xa7 P1 PW \xc2\xb7 \xe2\x87\xa7 P2 DETUNE");
                add(v2, knob("ob6.osc2.shape", "SHAPE", "%")); add(v2, knob("ob6.osc2.interval", "INTERVAL", "ST", 0, true));
                add(v2, knob("ob6.osc2.level", "LEVEL", "%")); add(v2, knob("ob6.osc2.pw", "PW", "%")); add(v2, knob("ob6.osc2.detune", "DETUNE", "CT", 0, true));
                v2->p1 = 0; v2->p2 = 1; v2->wave = vco(3, nullptr, "ob6.osc2.pw", "ob6.osc2.shape");
                auto* x = panel("X-MOD", "", true, "MORE X-MOD ON LFO \xc2\xb7 MOD");
                x->tag = utf8("VCO 2 \xe2\x86\x92 1");
                add(x, knob("ob6.xmod.vco2", "AMOUNT", "%")); add(x, toggle("ob6.sync", "SYNC"));
                x->wave = [this](float ph) {
                    const float idx = raw("ob6.xmod.vco2") / 100.0f * 0.6f, ratio = std::exp2(raw("ob6.osc2.interval") / 12.0f);
                    return std::sin(juce::MathConstants<float>::twoPi * (ph + idx * std::sin(juce::MathConstants<float>::twoPi * ph * ratio)));
                };
                auto* n = panel("NOISE", "", true, "ENC R NOISE (VCO 1)");
                add(n, knob("ob6.noise", "LEVEL", "%"));
                n->wave = [this](float ph) { return osc_math::noise(int(ph * 180.0f)) * (0.1f + 0.7f * raw("ob6.noise") / 100.0f); };
                auto* f = panel("SEM", "FILTER", true, "\xe2\x87\xa7 ENC L MORPH \xc2\xb7 \xe2\x87\xa7 ENC R BP");
                add(f, knob("ob6.morph", "MORPH", "%")); add(f, toggle("ob6.bandpass", "BP MODE"));
                f->wave = [this](float ph) {   // la forma de VCO 1 por el SEM, para ver el morph sin ir a PERFORM
                    PerformSource s; s.engine = 3; s.shape = raw("ob6.osc1.shape");
                    return perform_math::osc_sample(s, ph, 0.5f) * (raw("ob6.bandpass") > 0.5f ? 0.4f : 1.0f - 0.6f * raw("ob6.morph") / 100.0f);
                };
                nav_ = utf8("VCO 1 \xc2\xb7 2");
                break;
            }
            case 4: {   // DX7 (1d)
                algorithm_ = state.getParameter("dx7.algorithm");
                for (int op = 0; op < 4; ++op) {
                    const juce::String base = "dx7.op" + juce::String(op + 1) + ".";
                    ids_.add(base + "ratio"); ids_.add(base + "level"); ids_.add(base + "wave"); ids_.add(base + "detune");
                }
                for (int op = 0; op < 4; ++op) {
                    auto* p = panel(("OP " + juce::String(op + 1)).toRawUTF8(), "", false, "P1 RATIO \xc2\xb7 P2 LEVEL \xc2\xb7 ENC L ALGORITHM \xc2\xb7 ENC R FEEDBACK");
                    const auto* id = ids_.strings.getRawDataPointer() + op * 4;
                    add(p, knob(id[0].toRawUTF8(), "RATIO", "\xc3\x97", 2)); add(p, knob(id[1].toRawUTF8(), "LEVEL", ""));
                    add(p, [&] { auto w = value(id[2].toRawUTF8(), "WAVE", "OF 8"); w.prefix = "W"; return w; }()); add(p, knob(id[3].toRawUTF8(), "DETUNE", "", 0, true));
                    p->p1 = 0; p->p2 = 1;
                    p->wave = [this, op](float ph) { return op_wave(op, ph); };
                }
                auto* a = panel("ALGORITHM", "", true, "ENC L / ENC R \xc2\xb7 CLICK THE DIAGRAM");
                a->algorithm = true;
                a->algorithm_number = [this] { return juce::roundToInt(raw("dx7.algorithm", 1.0f)); };
                a->on_diagram = [this] { open_picker(); };
                add(a, value("dx7.algorithm", "ALGORITHM", "OF 8")); add(a, value("dx7.feedback", "FEEDBACK", "OF 7"));
                a->p1 = 0; a->p2 = 1;
                nav_ = utf8("OP 1 \xc2\xb7 2 \xc2\xb7 3 \xc2\xb7 4");
                break;
            }
            case 5: {   // ARP 2600 (nota de la referencia: VCO 1, VCO 2, RING y NOISE angostos, SYNC en VCO 2)
                auto* v1 = panel("VCO 1", "REF", false, "P1 WAVE \xc2\xb7 P2 PW \xc2\xb7 \xe2\x87\xa7 P1 LEVEL");
                add(v1, seg("arp2600.osc1.wave", "WAVE", arp_waves)); add(v1, knob("arp2600.osc1.pw", "PW", "%")); add(v1, knob("arp2600.osc1.level", "LEVEL", "%"));
                v1->p1 = 0; v1->p2 = 1; v1->wave = vco(5, "arp2600.osc1.wave", "arp2600.osc1.pw", nullptr);
                auto* v2 = panel("VCO 2", "", false, "P1 WAVE \xc2\xb7 P2 RATIO \xc2\xb7 \xe2\x87\xa7 P1 LEVEL \xc2\xb7 SYNC ON PANEL: LFO \xc2\xb7 MOD");
                add(v2, seg("arp2600.osc2.wave", "WAVE", arp_waves)); add(v2, knob("arp2600.osc2.ratio", "RATIO", "\xc3\x97", 2));
                add(v2, knob("arp2600.osc2.level", "LEVEL", "%")); add(v2, toggle("arp2600.sync", "SYNC"));
                v2->p1 = 0; v2->p2 = 1;
                v2->wave = [this](float ph) {
                    PerformSource s; s.engine = 5; s.wave = raw("arp2600.osc2.wave");
                    return perform_math::osc_sample(s, ph * raw("arp2600.osc2.ratio", 1.0f), 0.5f);
                };
                auto* r = panel("RING", "", true, "ENC L RING MIX");
                r->tag = utf8("VCO 1 \xc3\x97 2");
                add(r, knob("arp2600.ring", "MIX", "%", 0, false, 100.0f));
                r->wave = [this](float ph) {
                    PerformSource a; a.engine = 5; a.wave = raw("arp2600.osc1.wave");
                    PerformSource b; b.engine = 5; b.wave = raw("arp2600.osc2.wave");
                    return perform_math::osc_sample(a, ph, 0.5f) * perform_math::osc_sample(b, ph * raw("arp2600.osc2.ratio", 1.0f), 0.5f);
                };
                auto* n = panel("NOISE", "", true, "ENC R NOISE \xc2\xb7 COLOR ON PANEL: LFO \xc2\xb7 MOD");
                add(n, knob("arp2600.noise.level", "LEVEL", "%")); add(n, knob("arp2600.noise.color", "COLOR", "%"));
                n->wave = [this](float ph) { return osc_math::noise(int(ph * 180.0f)) * (0.1f + 0.7f * raw("arp2600.noise.level") / 100.0f); };
                nav_ = utf8("VCO 1 \xc2\xb7 2");
                break;
            }
            default: nav_ = {}; break;
        }
        for (auto& p : panels) for (auto& c : p->cells) c->mode = mode;
        set_focus(0);
        refresh();
        resized();
    }
    void refresh() {
        for (auto& p : panels) {
            p->mode = mode;
            for (auto& c : p->cells) c->mode = mode;
        }
        if (kind_ == 4) {
            const int alg = juce::roundToInt(raw("dx7.algorithm", 1.0f));
            for (int op = 0; op < 4 && op < int(panels.size()); ++op) {
                const int t = osc_math::first_target(alg, op);
                panels[size_t(op)]->sub = t < 0 ? juce::String("CARRIER") : utf8("MOD \xe2\x86\x92 OP ") + juce::String(t + 1);
            }
        }
        snapshot_ = values();
        repaint();
        for (auto& p : panels) p->repaint();
        if (on_change) on_change();
    }
    void resized() override {
        int narrow = 0, full = 0;
        for (auto& p : panels) (p->narrow ? narrow : full)++;
        const int gaps = std::max(0, int(panels.size()) - 1) * 8;
        const int full_w = full ? (1168 - narrow * 176 - gaps) / full : 0;
        int x = 16;
        for (auto& p : panels) { const int w = p->narrow ? 176 : full_w; p->setBounds(x, 16, w, 456); x += w + 8; }
        picker.setBounds(getLocalBounds());
    }
    void paint(juce::Graphics& g) override { g.fillAll(colour::BG); }

    /** Salida de un operador del DX7 para dibujar: moduladores = su propia onda; portadoras = su salida FM (nota 1d). */
    float op_wave(int op, float ph) const {
        const int alg = juce::roundToInt(raw("dx7.algorithm", 1.0f));
        if (!osc_math::is_carrier(alg, op)) return op_signal(alg, op, ph, false);
        return op_signal(alg, op, ph, true);
    }

private:
    juce::AudioProcessorValueTreeState* state_ = nullptr;
    juce::RangedAudioParameter* algorithm_ = nullptr;
    juce::StringArray ids_;
    juce::String nav_;
    int kind_ = -1, focus_ = 0;
    std::vector<float> snapshot_;

    float raw(const char* id, float fallback = 0.0f) const {
        if (!id || !state_) return fallback;
        auto* v = state_->getRawParameterValue(id);
        return v ? v->load() : fallback;
    }
    float op_field(int op, const char* field, float fallback = 0.0f) const {
        return raw(("dx7.op" + juce::String(op + 1) + "." + field).toRawUTF8(), fallback);
    }
    float op_signal(int alg, int op, float ph, bool modulated, int depth = 0) const {
        float phase = op_field(op, "ratio", 1.0f) * ph;
        if (modulated && depth < 4)
            for (int m = 0; m < 4; ++m)
                if (osc_math::modulates(alg, m, op)) phase += osc_math::fm_index(op_field(m, "level")) * op_signal(alg, m, ph, true, depth + 1);
        return ::dx7_wave(juce::roundToInt(op_field(op, "wave", 1.0f)) - 1, phase);
    }
    std::vector<float> values() const {
        std::vector<float> v;
        for (auto& p : panels) for (auto& c : p->cells) v.push_back(c->parameter() ? c->parameter()->getValue() : 0.0f);
        return v;
    }
    void timerCallback() override {
        if (!isShowing() || panels.empty()) return;
        if (values() != snapshot_) refresh();
    }
};

}  // namespace studio_ui
