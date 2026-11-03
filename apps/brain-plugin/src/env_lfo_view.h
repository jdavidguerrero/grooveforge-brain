#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include <array>
#include <cmath>
#include <functional>
#include <memory>
#include <vector>

#include "osc_view.h"

/* ENV (1e) y LFO · MOD (1f) de la referencia (gemelo-vst-ui-migracion.md §5). Las celdas son las de OSC (ParamCell);
 * las envolventes se editan arrastrando sus puntos y el LFO se arrastra sobre un destino. */
namespace studio_ui {

/** Un panel de envolvente (576 × 456): gráfica de 544 × 232 con 4 puntos arrastrables y una fila de perillas. */
class EnvPanel final : public juce::Component {
public:
    juce::String name, tag, legend, foot;
    Mode mode = synth_mode();
    bool focused = false;
    std::array<juce::RangedAudioParameter*, 4> adsr{};   ///< A · D · S · R; nullptr = el engine no lo tiene (D 0, S lleno)
    std::vector<std::unique_ptr<ParamCell>> cells;
    juce::StringArray selector;                          ///< DX7: OP 1–4 en la cabecera
    int selected_item = 0;
    std::function<void(int)> on_select;
    std::function<void()> on_focus, on_edit;
    float axis_s = 1.8f;                                 ///< eje de tiempo compartido con el otro panel
    int selected = -1;                                   ///< punto elegido (blanco, con su valor)

    static constexpr float GX = 16, GY = 48, GW = 544, GH = 232, TOP = 8, HOLD_S = 0.5f;

    ParamCell* add(CellSpec spec, juce::RangedAudioParameter* p) {
        cells.push_back(std::make_unique<ParamCell>(std::move(spec), p));
        auto* c = cells.back().get();
        c->on_touch = [this] { if (on_focus) on_focus(); };
        c->setComponentID("env.cell." + juce::String(c->spec.id ? c->spec.id : "none"));
        addAndMakeVisible(*c);
        return c;
    }
    void clear_cells() { for (auto& c : cells) removeChildComponent(c.get()); cells.clear(); }

    float seconds(int i) const {
        auto* p = adsr[size_t(i)];
        return p ? p->convertFrom0to1(p->getValue()) / 1000.0f : 0.0f;
    }
    float sustain() const {
        auto* p = adsr[2];
        if (!p) return 1.0f;
        const auto r = p->getNormalisableRange();
        return (p->convertFrom0to1(p->getValue()) - r.start) / (r.end - r.start);
    }
    /** Largo total dibujado (A + D + sostén fijo + R), para el eje compartido. */
    float total_s() const { return seconds(0) + seconds(1) + HOLD_S + seconds(3); }
    std::array<juce::Point<float>, 4> points() const {
        const float k = GW / axis_s, xa = seconds(0) * k, xd = xa + seconds(1) * k, xs = xd + HOLD_S * k, xr = xs + seconds(3) * k;
        const float ys = GH - (GH - TOP) * sustain();
        return {{{GX + xa, GY + TOP}, {GX + xd, GY + ys}, {GX + xs, GY + ys}, {GX + xr, GY + GH}}};
    }
    juce::String point_text(int i) const {
        static const char* names[] = {"ATTACK", "DECAY", "SUSTAIN", "RELEASE"};
        auto* p = adsr[size_t(i == 2 ? 2 : i)];
        if (!p) return {};
        const float v = p->convertFrom0to1(p->getValue());
        return juce::String(names[i]) + " " + (i == 2 ? juce::String(juce::roundToInt(sustain() * 100.0f)) + " %" : juce::String(juce::roundToInt(v)) + " MS");
    }

    void resized() override {
        const float cw = GW / float(std::max<size_t>(4, cells.size()));
        for (size_t i = 0; i < cells.size(); ++i) cells[i]->setBounds(int(GX + float(i) * cw), 320, int(cw), 96);
    }
    juce::Rectangle<float> selector_rect(int i) const { return {float(getWidth()) - 16 - float(selector.size() - i) * 36, 12, 32, 22}; }
    void paint(juce::Graphics& g) override {
        g.setColour(colour::SURFACE); g.fillAll();
        g.setColour(focused ? mode.hi : colour::LINE); g.drawRect(getLocalBounds().toFloat(), focused ? 2.0f : 1.0f);
        g.setFont(mono(16.0f, true)); g.setColour(colour::WHITE);
        const int nw = int(float(name.length()) * 10.0f);
        g.drawText(name, 16, 12, nw + 4, 22, juce::Justification::left);
        g.setFont(mono(11.0f)); g.setColour(mode.hi); g.drawText(tag, 16 + nw + 8, 16, 160, 16, juce::Justification::left);
        g.setColour(colour::MON_TEXT);
        if (selector.isEmpty()) g.drawText(legend, getWidth() - 316, 16, 300, 16, juce::Justification::right);
        for (int i = 0; i < selector.size(); ++i) {
            const auto r = selector_rect(i);
            const bool on = i == selected_item;
            if (on) { g.setColour(mode.hi); g.fillRect(r); }
            g.setColour(on ? mode.hi : mode.dim); g.drawRect(r, 1.0f);
            g.setColour(on ? mode.on : colour::GRAY); g.drawText(selector[i], r, juce::Justification::centred);
        }
        const juce::Rectangle<float> box(GX, GY, GW, GH);
        g.setColour(colour::BG); g.fillRect(box);
        g.setColour(colour::LINE); g.drawRect(box, 1.0f);
        g.setColour(colour::SURFACE_2);
        for (int i = 1; i < 4; ++i) g.drawVerticalLine(int(GX + GW * float(i) / 4.0f), GY + 1, GY + GH - 1);
        const bool on = adsr[0] || adsr[3];
        if (on) {
            const auto p = points();
            juce::Path path;
            path.startNewSubPath(GX, GY + GH); path.lineTo(p[0]);
            path.quadraticTo(p[0].x + (p[1].x - p[0].x) * 0.12f, p[1].y, p[1].x, p[1].y);
            path.lineTo(p[2]);
            path.quadraticTo(p[2].x + (p[3].x - p[2].x) * 0.12f, GY + GH, p[3].x, p[3].y);
            auto fill = path; fill.closeSubPath();
            g.setColour(mode.dim); g.fillPath(fill);
            g.setColour(mode.hi); g.strokePath(path, juce::PathStrokeType(2.0f));
            for (int i = 0; i < 4; ++i) {
                if (!handle_active(i)) continue;
                const auto r = juce::Rectangle<float>(8, 8).withCentre(p[size_t(i)]);
                g.setColour(i == selected ? colour::WHITE : colour::BG); g.fillRect(r);
                g.setColour(i == selected ? colour::WHITE : mode.hi); g.drawRect(r, 2.0f);
            }
            if (selected >= 0 && handle_active(selected)) {
                const auto text = point_text(selected);
                const float w = float(text.length()) * 6.7f + 16;
                auto x = p[size_t(selected)].x + 10;
                if (x + w > GX + GW) x = p[size_t(selected)].x - 10 - w;
                const juce::Rectangle<float> tip(x, juce::jlimit(GY, GY + GH - 24, p[size_t(selected)].y + 4), w, 24);
                g.setColour(colour::WHITE); g.fillRect(tip);
                g.setColour(colour::BG); g.setFont(mono(11.0f)); g.drawText(text, tip, juce::Justification::centred);
            }
        } else {
            g.setColour(colour::MON); g.setFont(mono(11.0f));
            g.drawText("NOT IN THE PLUGIN FOR THIS ENGINE", box, juce::Justification::centred);
        }
        g.setColour(colour::MON_TEXT); g.setFont(mono(11.0f));
        g.drawText("0", int(GX), 284, 40, 16, juce::Justification::left);
        g.drawText(juce::String(axis_s, 1) + " S", int(GX + GW) - 80, 284, 80, 16, juce::Justification::right);
        g.drawText(foot, int(GX), 428, int(GW), 16, juce::Justification::left);
    }
    bool handle_active(int i) const { return i == 0 ? adsr[0] != nullptr : i == 1 ? adsr[1] || adsr[2] : i == 2 ? adsr[2] != nullptr : adsr[3] != nullptr; }

    void mouseDown(const juce::MouseEvent& e) override {
        if (on_focus) on_focus();
        for (int i = 0; i < selector.size(); ++i)
            if (selector_rect(i).contains(e.position)) { selected_item = i; if (on_select) on_select(i); repaint(); return; }
        drag_ = -1;
        const auto p = points();
        float best = 10;
        for (int i = 0; i < 4; ++i)
            if (handle_active(i) && p[size_t(i)].getDistanceFrom(e.position) < best) { best = p[size_t(i)].getDistanceFrom(e.position); drag_ = i; }
        if (drag_ < 0) return;
        selected = drag_;
        start_ = p[size_t(drag_)];
        for (auto* q : adsr) if (q) q->beginChangeGesture();
        repaint();
    }
    void mouseDrag(const juce::MouseEvent& e) override {
        if (drag_ < 0) return;
        const float fine = e.mods.isShiftDown() ? 0.2f : 1.0f;
        const auto pos = start_ + (e.position - e.mouseDownPosition) * fine;
        const auto p = points();
        const float k = GW / axis_s;
        auto set_ms = [](juce::RangedAudioParameter* q, float ms) {
            if (!q) return;
            const auto r = q->getNormalisableRange();
            q->setValueNotifyingHost(q->convertTo0to1(juce::jlimit(r.start, r.end, ms)));
        };
        auto set_sustain = [&](float y) {
            auto* q = adsr[2];
            if (!q) return;
            const auto r = q->getNormalisableRange();
            const float s = juce::jlimit(0.0f, 1.0f, 1.0f - (y - GY - TOP) / (GH - TOP));
            q->setValueNotifyingHost(q->convertTo0to1(r.start + s * (r.end - r.start)));
        };
        switch (drag_) {
            case 0: set_ms(adsr[0], (pos.x - GX) / k * 1000.0f); break;
            case 1: set_ms(adsr[1], (pos.x - p[0].x) / k * 1000.0f); set_sustain(pos.y); break;
            case 2: set_sustain(pos.y); break;
            default: set_ms(adsr[3], (pos.x - p[2].x) / k * 1000.0f); break;
        }
        if (on_edit) on_edit();
        repaint();
    }
    void mouseUp(const juce::MouseEvent&) override {
        if (drag_ < 0) return;
        for (auto* q : adsr) if (q) q->endChangeGesture();
        drag_ = -1;
        if (on_edit) on_edit();
    }
    bool dragging() const { return drag_ >= 0; }
private:
    int drag_ = -1;
    juce::Point<float> start_;
};

/** ENV entera: AMP y FILTER (en el DX7, AMP y la EG de cada operador) con un eje de tiempo común. */
class EnvPage final : public juce::Component, private juce::Timer {
public:
    Mode mode = synth_mode();
    std::function<void()> on_change;
    EnvPanel amp, filter;

    EnvPage() {
        for (auto* p : {&amp, &filter}) {
            addAndMakeVisible(*p);
            p->on_edit = [this] { refresh(); };
        }
        amp.setComponentID("env.amp"); filter.setComponentID("env.filter");
        amp.on_focus = [this] { set_focus(0); }; filter.on_focus = [this] { set_focus(1); };
        filter.on_select = [this](int op) { bind_operator(op); };
        startTimerHz(15);
    }
    int kind() const { return kind_; }
    int focus() const { return focus_; }
    void set_focus(int i) {
        if (focus_ == i && amp.focused == (i == 0)) return;
        focus_ = i; amp.focused = i == 0; filter.focused = i == 1;
        amp.repaint(); filter.repaint();
        if (on_change) on_change();
    }
    StripText strip() const {
        StripText s; s.nav = utf8("AMP \xc2\xb7 FILTER");
        const auto& p = focus_ == 0 ? amp : filter;
        if (p.cells.size() >= 2) {
            s.p1 = p.cells[0]->spec.label; s.p1_value = p.cells[0]->value_text();
            s.p2 = p.cells[1]->spec.label; s.p2_value = p.cells[1]->value_text();
        }
        if (kind_ == 4) s.nav = utf8("AMP \xc2\xb7 OP EG");
        return s;
    }

    void build(int kind, juce::AudioProcessorValueTreeState& state) {
        if (kind == kind_ && state_ == &state) return;
        kind_ = kind; state_ = &state;
        const char* prefix = kind == 0 ? "modeld." : kind == 1 ? "juno." : kind == 3 ? "ob6." : kind == 5 ? "arp2600." : "";
        auto id = [&](const char* name) { return juce::String(prefix) + name; };
        if (kind == 4) {
            setup(amp, "AMP ENV", {"dx7.amp_attack", "dx7.amp_decay", "dx7.amp_sustain", "dx7.amp_release"}, "dx7.velocity", "VEL", 100.0f);
            amp.legend = utf8("CARRIERS \xc2\xb7 OVER THE OPERATOR EG");
            filter.name = "OP EG"; filter.selector = {"1", "2", "3", "4"}; filter.legend = {};
            bind_operator(filter.selected_item);
        } else if (kind == 2) {   // Prophet: el plugin solo tiene ATTACK y RELEASE
            setup(amp, "AMP ENV", {"attack", nullptr, nullptr, "release"}, nullptr, nullptr, 1.0f);
            setup(filter, "FILTER ENV", {nullptr, nullptr, nullptr, nullptr}, nullptr, nullptr, 1.0f);
            amp.legend = utf8("ATTACK \xc2\xb7 RELEASE IN THE PLUGIN");
        } else {
            amp_ids_ = {"attack", id("amp_decay"), id("amp_sustain"), "release"};
            filter_ids_ = {id("filter_attack"), id("filter_decay"), id("filter_sustain"), id("filter_release")};
            vel_ids_ = {id("vel_amp"), id("vel_filter")};
            setup(amp, "AMP ENV", {amp_ids_[0].toRawUTF8(), amp_ids_[1].toRawUTF8(), amp_ids_[2].toRawUTF8(), amp_ids_[3].toRawUTF8()}, vel_ids_[0].toRawUTF8(), "VEL \xe2\x86\x92 AMP", 1.0f);
            setup(filter, "FILTER ENV", {filter_ids_[0].toRawUTF8(), filter_ids_[1].toRawUTF8(), filter_ids_[2].toRawUTF8(), filter_ids_[3].toRawUTF8()},
                  vel_ids_[1].toRawUTF8(), "VEL \xe2\x86\x92 FILTER", 1.0f);
            amp.legend = {}; filter.legend = utf8("AMOUNT ON PERFORM \xc2\xb7 P1");
        }
        for (auto* p : {&amp, &filter}) p->foot = utf8("P1 ATTACK \xc2\xb7 P2 DECAY \xc2\xb7 \xe2\x87\xa7 SUSTAIN / RELEASE \xc2\xb7 DRAG THE POINTS");
        focus_ = -1; set_focus(0);
        refresh(); resized();
    }
    void refresh() {
        // eje común: el más largo de los dos, en pasos de 0,6 s (nota SCALE de 1e); quieto mientras se arrastra
        if (!amp.dragging() && !filter.dragging()) {
            const float longest = std::max(amp.total_s(), filter.total_s());
            const float axis = std::max(0.6f, std::ceil(longest / 0.6f - 1e-4f) * 0.6f);
            amp.axis_s = filter.axis_s = axis;
        }
        for (auto* p : {&amp, &filter}) { p->mode = mode; for (auto& c : p->cells) { c->mode = mode; c->repaint(); } p->repaint(); }
        snapshot_ = values();
        if (on_change) on_change();
    }
    void resized() override { amp.setBounds(16, 16, 576, 456); filter.setBounds(608, 16, 576, 456); }
    void paint(juce::Graphics& g) override { g.fillAll(colour::BG); }

private:
    juce::AudioProcessorValueTreeState* state_ = nullptr;
    int kind_ = -1, focus_ = 0;
    std::array<juce::String, 4> amp_ids_, filter_ids_, op_ids_;
    std::array<juce::String, 2> vel_ids_;
    std::vector<float> snapshot_;

    void setup(EnvPanel& p, const char* name, std::array<const char*, 4> ids, const char* extra, const char* extra_label, float extra_scale) {
        p.clear_cells(); p.name = utf8(name); p.selector.clear(); p.selected = -1;
        static const char* labels[] = {"ATTACK", "DECAY", "SUSTAIN", "RELEASE"};
        for (int i = 0; i < 4; ++i) {
            auto* param = ids[size_t(i)] ? state_->getParameter(ids[size_t(i)]) : nullptr;
            p.adsr[size_t(i)] = param;
            CellSpec s; s.kind = CellSpec::Kind::knob; s.id = ids[size_t(i)]; s.label = labels[i];
            if (i == 2) {
                // sustain 0–1 (o 0–100 en la EG de operador del DX7) se muestra en %
                const bool unit_range = param && param->getNormalisableRange().end <= 1.0f;
                s.unit = "%"; s.scale = unit_range ? 100.0f : 1.0f;
            } else s.unit = "MS";
            p.add(s, param);
        }
        if (extra) {
            CellSpec s; s.kind = CellSpec::Kind::knob; s.id = extra; s.label = utf8(extra_label); s.unit = "%"; s.scale = extra_scale;
            p.add(s, state_->getParameter(extra));
        }
        p.resized();
    }
    void bind_operator(int op) {
        if (!state_) return;
        const juce::String base = "dx7.op" + juce::String(op + 1) + ".";
        op_ids_ = {base + "attack", base + "decay", base + "sustain", base + "release"};
        setup(filter, "OP EG", {op_ids_[0].toRawUTF8(), op_ids_[1].toRawUTF8(), op_ids_[2].toRawUTF8(), op_ids_[3].toRawUTF8()}, nullptr, nullptr, 1.0f);
        filter.selector = {"1", "2", "3", "4"}; filter.selected_item = op;
        filter.tag = "OP " + juce::String(op + 1);
        refresh();
    }
    std::vector<float> values() const {
        std::vector<float> v;
        for (auto* p : {&amp, &filter}) for (auto& c : p->cells) v.push_back(c->parameter() ? c->parameter()->getValue() : 0.0f);
        return v;
    }
    void timerCallback() override { if (isShowing() && values() != snapshot_) refresh(); }
};

/** LFO · MOD (1f): el LFO a la izquierda, los cuatro destinos a la derecha; el chip del LFO se arrastra sobre uno. */
class LfoPage final : public juce::Component, private juce::Timer {
public:
    Mode mode = synth_mode();
    std::function<void()> on_change;
    std::function<float()> host_bpm, cutoff_hz;
    std::unique_ptr<ParamCell> shape, rate, sync, division, delay;
    struct Destination { juce::String name; juce::RangedAudioParameter* depth = nullptr; std::unique_ptr<ParamCell> knob; };
    std::array<Destination, 4> dests;

    LfoPage() { startTimerHz(30); }
    int kind() const { return kind_; }

    static juce::Rectangle<int> dest_row(int i) { return {624, 72 + i * 96, 544, 88}; }
    static juce::Rectangle<int> knob_rect(int i) { return {1056, 72 + i * 96, 96, 88}; }
    static juce::Rectangle<int> chip_rect() { return {32, 424, 300, 32}; }

    StripText strip() const {
        StripText s; s.nav = "PUSH SYNC";
        if (rate) { s.p1 = "RATE"; s.p1_value = rate->value_text(); }
        if (dests[0].knob && dests[0].depth) { s.p2 = "DEPTH"; s.p2_value = dests[0].knob->value_text(); }
        return s;
    }

    void build(int kind, juce::AudioProcessorValueTreeState& state) {
        if (kind == kind_ && state_ == &state) return;
        kind_ = kind; state_ = &state;
        for (auto* c : {shape.get(), rate.get(), sync.get(), division.get(), delay.get()}) if (c) removeChildComponent(c);
        for (auto& d : dests) if (d.knob) removeChildComponent(d.knob.get());
        const juce::String prefix = kind == 0 ? "modeld." : kind == 1 ? "juno." : kind == 3 ? "ob6." : kind == 4 ? "dx7." : "arp2600.";
        ids_ = {prefix + "lfo_shape", prefix + "lfo_rate", prefix + "lfo_div", prefix + "lfo_cutoff", prefix + "lfo_pitch",
                kind == 4 ? prefix + "lfo_amp" : juce::String(), kind == 4 ? juce::String() : prefix + "lfo_pwm", kind == 1 ? prefix + "lfo_delay" : juce::String(), "lfo_sync"};
        auto param = [&](const juce::String& id) { return id.isEmpty() ? nullptr : state.getParameter(id); };
        auto cell = [&](CellSpec s, const juce::String& id) {
            s.id = id.isEmpty() ? nullptr : id.toRawUTF8();
            auto c = std::make_unique<ParamCell>(std::move(s), param(id));
            c->setComponentID("lfo.cell." + id);
            addAndMakeVisible(*c);
            return c;
        };
        { CellSpec s; s.kind = CellSpec::Kind::segments; s.label = "SHAPE"; s.options = {"SINE", "TRI", "SAW", "SQR", "S&H"}; shape = cell(s, ids_[0]); }
        { CellSpec s; s.kind = CellSpec::Kind::knob; s.label = "RATE"; s.unit = "HZ"; s.decimals = 2; rate = cell(s, ids_[1]); }
        { CellSpec s; s.kind = CellSpec::Kind::toggle; s.label = "SYNC"; sync = cell(s, ids_[8]); }
        { CellSpec s; s.kind = CellSpec::Kind::segments; s.label = "DIVISION"; s.options = {"1/4", "1/8", "1/16", "1/8T"}; division = cell(s, ids_[2]); }
        if (ids_[7].isNotEmpty()) { CellSpec s; s.kind = CellSpec::Kind::knob; s.label = "DELAY"; s.unit = "MS"; delay = cell(s, ids_[7]); }
        else delay.reset();
        static const char* names[] = {"CUTOFF", "PITCH", "AMP", "PWM"};
        for (int i = 0; i < 4; ++i) {
            auto& d = dests[size_t(i)];
            d.name = names[i]; d.depth = param(ids_[size_t(3 + i)]);
            CellSpec s; s.kind = CellSpec::Kind::knob; s.label = "DEPTH"; s.unit = "%"; s.scale = 100.0f;
            d.knob = cell(s, ids_[size_t(3 + i)]);
            d.knob->setComponentID("lfo.dest." + juce::String(i));
        }
        resized(); refresh();
    }
    void refresh() {
        for (auto* c : {shape.get(), rate.get(), sync.get(), division.get(), delay.get()}) if (c) { c->mode = mode; c->repaint(); }
        for (auto& d : dests) if (d.knob) { d.knob->mode = mode; d.knob->repaint(); }
        const bool synced = synced_now();
        if (rate) rate->setAlpha(synced ? 0.4f : 1.0f);
        if (division) division->setAlpha(synced ? 1.0f : 0.4f);
        snapshot_ = values();
        repaint();
        if (on_change) on_change();
    }
    void resized() override {
        if (shape) shape->setBounds(32, 56, 544, 64);
        const int units = delay ? 5 : 4;
        const float cw = 544.0f / float(units);
        if (rate) rate->setBounds(32, 316, int(cw), 96);
        if (sync) sync->setBounds(32 + int(cw), 316, int(cw), 96);
        if (division) division->setBounds(32 + int(2 * cw), 316, int(2 * cw), 96);
        if (delay) delay->setBounds(32 + int(4 * cw), 316, int(cw), 96);
        for (int i = 0; i < 4; ++i) if (dests[size_t(i)].knob) dests[size_t(i)].knob->setBounds(knob_rect(i));
    }
    void paint(juce::Graphics& g) override {
        g.fillAll(colour::BG);
        for (auto r : {juce::Rectangle<float>(16, 16, 576, 456), juce::Rectangle<float>(608, 16, 576, 456)}) {
            g.setColour(colour::SURFACE); g.fillRect(r); g.setColour(colour::LINE); g.drawRect(r, 1.0f);
        }
        g.setFont(mono(16.0f, true)); g.setColour(colour::WHITE);
        g.drawText("LFO", 32, 28, 80, 22, juce::Justification::left);
        g.drawText("DESTINATIONS", 624, 28, 160, 22, juce::Justification::left);
        g.setFont(mono(11.0f)); g.setColour(colour::GRAY);
        g.drawText(note(), 256, 32, 320, 16, juce::Justification::right);
        g.drawText(utf8("4 \xc2\xb7 ONE DEPTH EACH"), 776, 32, 200, 16, juce::Justification::left);
        // onda con el punto de fase
        const juce::Rectangle<float> wb(32, 128, 544, 176);
        g.setColour(colour::BG); g.fillRect(wb); g.setColour(colour::LINE); g.drawRect(wb, 1.0f);
        g.setColour(colour::SURFACE_2); g.drawHorizontalLine(int(wb.getCentreY()), wb.getX() + 1, wb.getRight() - 1);
        const int shape_index = shape ? shape->index() : 1;
        juce::Path p;
        for (int x = 0; x <= 542; x += 2) {
            const float y = wb.getCentreY() - lfo_y(shape_index, 2.0f * float(x) / 542.0f) * 0.8f * 88.0f;
            if (x == 0) p.startNewSubPath(wb.getX() + 1, y); else p.lineTo(wb.getX() + 1 + float(x), y);
        }
        g.setColour(mode.hi); g.strokePath(p, juce::PathStrokeType(2.0f));
        const float ph = phase_ - std::floor(phase_), dx = wb.getX() + 1 + ph * 271.0f;
        g.setColour(colour::MON); g.drawVerticalLine(int(dx), wb.getY() + 1, wb.getBottom() - 1);
        g.setColour(colour::WHITE);
        g.fillEllipse(juce::Rectangle<float>(8, 8).withCentre({dx, wb.getCentreY() - lfo_y(shape_index, ph) * 0.8f * 88.0f}));
        // chip arrastrable
        const auto chip = chip_rect().toFloat();
        const float dash[] = {4.0f, 3.0f};
        g.setColour(colour::PURPLE_BRIGHT);
        for (auto l : {juce::Line<float>(chip.getTopLeft(), chip.getTopRight()), juce::Line<float>(chip.getTopRight(), chip.getBottomRight()),
                       juce::Line<float>(chip.getBottomRight(), chip.getBottomLeft()), juce::Line<float>(chip.getBottomLeft(), chip.getTopLeft())})
            g.drawDashedLine(l, dash, 2, 1.0f);
        g.drawText(utf8("\xe2\x8b\xae\xe2\x8b\xae LFO \xc2\xb7 DRAG ONTO A DESTINATION"), chip.reduced(12, 0), juce::Justification::left);
        // destinos
        for (int i = 0; i < 4; ++i) {
            const auto r = dest_row(i);
            const auto& d = dests[size_t(i)];
            g.setColour(colour::LINE); g.drawHorizontalLine(r.getY(), float(r.getX()), float(r.getRight()));
            g.setFont(mono(16.0f, true));
            g.setColour(!d.depth ? colour::MON : i == hover_ ? colour::PURPLE_BRIGHT : colour::WHITE);
            g.drawText(d.name, r.getX(), r.getY() + 14, 200, 22, juce::Justification::left);
            g.setFont(mono(11.0f)); g.setColour(d.depth ? colour::GRAY : colour::MON);
            g.drawText(dest_text(i), r.getX(), r.getY() + 44, 400, 16, juce::Justification::left);
            if (i == hover_) {
                const auto k = knob_rect(i).toFloat().expanded(2);
                g.setColour(colour::PURPLE_BRIGHT);
                for (auto l : {juce::Line<float>(k.getTopLeft(), k.getTopRight()), juce::Line<float>(k.getTopRight(), k.getBottomRight()),
                               juce::Line<float>(k.getBottomRight(), k.getBottomLeft()), juce::Line<float>(k.getBottomLeft(), k.getTopLeft())})
                    g.drawDashedLine(l, dash, 2, 2.0f);
            }
        }
    }
    void paintOverChildren(juce::Graphics& g) override {
        if (!dragging_) return;
        const auto from = chip_rect().toFloat().getCentre();
        juce::Path path; path.startNewSubPath(from);
        path.cubicTo(from.x + 300, from.y, drag_pos_.x - 200, drag_pos_.y, drag_pos_.x, drag_pos_.y);
        juce::Path dashed; const float d[] = {6.0f, 4.0f};
        juce::PathStrokeType(2.0f).createDashedStroke(dashed, path, d, 2);
        g.setColour(colour::PURPLE_BRIGHT); g.fillPath(dashed);
        const juce::String tag = hover_ >= 0 ? utf8("LFO \xe2\x86\x92 ") + dests[size_t(hover_)].name : juce::String("LFO");
        const juce::Rectangle<float> t(drag_pos_.x + 8, drag_pos_.y + 8, float(tag.length()) * 6.7f + 16, 24);
        g.fillRect(t); g.setColour(colour::WHITE); g.setFont(mono(11.0f)); g.drawText(tag, t, juce::Justification::centred);
    }

    // arrastrar el chip: soltar sobre un destino = +25 %; seguir arrastrando en vertical antes de soltar fija la profundidad
    void mouseDown(const juce::MouseEvent& e) override {
        dragging_ = chip_rect().contains(e.getPosition());
        drag_pos_ = e.position; hover_ = -1;
    }
    void mouseDrag(const juce::MouseEvent& e) override {
        if (!dragging_) return;
        drag_pos_ = e.position;
        int over = -1;
        for (int i = 0; i < 4; ++i) if (dests[size_t(i)].depth && dest_row(i).contains(e.getPosition())) over = i;
        if (over != hover_) {
            if (hover_ >= 0) { auto* p = dests[size_t(hover_)].depth; p->setValueNotifyingHost(restore_); p->endChangeGesture(); }
            hover_ = over;
            if (hover_ >= 0) { auto* p = dests[size_t(hover_)].depth; restore_ = p->getValue(); entry_y_ = e.position.y; p->beginChangeGesture(); }
        }
        if (hover_ >= 0) dests[size_t(hover_)].depth->setValueNotifyingHost(juce::jlimit(0.0f, 1.0f, 0.25f + (entry_y_ - e.position.y) * 0.004f));
        repaint();
    }
    void mouseUp(const juce::MouseEvent&) override {
        if (!dragging_) return;
        if (hover_ >= 0) dests[size_t(hover_)].depth->endChangeGesture();
        dragging_ = false; hover_ = -1;
        refresh();
    }
    /** Para tests: soltar el LFO sobre el destino `i` como si se arrastrara el chip hasta ahí. */
    void drop_on(int i) {
        const auto from = chip_rect().getCentre().toFloat(), to = dest_row(i).getCentre().toFloat();
        auto ev = [this](juce::Point<float> p, juce::Point<float> down) {
            return juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(), p, {}, 0, 0, 0, 0, 0, this, this,
                                    juce::Time::getCurrentTime(), down, juce::Time::getCurrentTime(), 1, true);
        };
        mouseDown(ev(from, from)); mouseDrag(ev(to, from)); mouseUp(ev(to, from));
    }

private:
    juce::AudioProcessorValueTreeState* state_ = nullptr;
    std::array<juce::String, 9> ids_;
    int kind_ = -1, hover_ = -1;
    bool dragging_ = false;
    float phase_ = 0, restore_ = 0, entry_y_ = 0;
    juce::Point<float> drag_pos_;
    std::vector<float> snapshot_;

    static float lfo_y(int shape, float ph) {
        if (shape == 4) return osc_math::noise(int(std::floor(ph * 4.0f)) + 3);   // S&H: cuatro pasos por ciclo
        return perform_math::lfo_value(shape, ph);
    }
    bool synced_now() const { return sync && sync->index() == 1; }
    float rate_hz() const {
        if (synced_now()) {
            constexpr float per_beat[] = {1.0f, 2.0f, 4.0f, 3.0f};
            return (host_bpm ? host_bpm() : 120.0f) / 60.0f * per_beat[juce::jlimit(0, 3, division ? division->index() : 1)];
        }
        return rate && rate->parameter() ? rate->raw() : 2.0f;
    }
    juce::String note() const {
        if (synced_now())
            return utf8("SYNC \xc2\xb7 ") + (division ? division->value_text() : juce::String()) + utf8(" \xc2\xb7 HOST ") + juce::String(host_bpm ? host_bpm() : 120.0f, 2);
        return utf8("FREE \xc2\xb7 ") + juce::String(1.0f / std::max(0.01f, rate_hz()), 2) + " S PER CYCLE";
    }
    juce::String dest_text(int i) const {
        const auto& d = dests[size_t(i)];
        if (!d.depth) return "NOT ON THIS ENGINE";
        const float depth = d.depth->convertFrom0to1(d.depth->getValue());
        if (depth <= 0.0f) return "OFF";
        if (i == 0) {
            const float fc = cutoff_hz ? cutoff_hz() : 1000.0f;
            const float lo = juce::jlimit(20.0f, 20000.0f, fc * std::exp2(-4.0f * depth)), hi = juce::jlimit(20.0f, 20000.0f, fc * std::exp2(4.0f * depth));
            return juce::String(juce::roundToInt(fc)) + utf8(" HZ \xc2\xb7 SWINGS ") + juce::String(juce::roundToInt(lo)) + utf8("\xe2\x80\x93") + juce::String(juce::roundToInt(hi)) + " HZ";
        }
        static const char* what[] = {"", "VIBRATO", "TREMOLO", "PULSE WIDTH"};
        return juce::String(what[i]) + utf8(" \xc2\xb7 ") + juce::String(juce::roundToInt(depth * 100.0f)) + " %";
    }
    std::vector<float> values() const {
        std::vector<float> v;
        for (auto* c : {shape.get(), rate.get(), sync.get(), division.get(), delay.get()}) if (c && c->parameter()) v.push_back(c->parameter()->getValue());
        for (auto& d : dests) if (d.depth) v.push_back(d.depth->getValue());
        return v;
    }
    void timerCallback() override {
        if (!isShowing() || kind_ < 0) return;
        phase_ += rate_hz() / 30.0f;
        if (values() != snapshot_) refresh();
        else repaint(32, 128, 544, 176);   // solo la onda: el punto de fase
    }
};

}  // namespace studio_ui
