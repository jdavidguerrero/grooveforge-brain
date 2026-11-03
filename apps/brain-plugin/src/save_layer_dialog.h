#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include <functional>

#include "studio_browser.h"

/* SAVE LAYER (2g; gemelo-vst-ui-migracion.md §8, 7b): modal de 560 × 464 sobre un velo al 70 %. Nombre de 12 caracteres
 * (A–Z 0–9 espacio, lo que el hardware muestra), NEW u OVERWRITE, rol, dialecto y rating. Enter guarda, Esc cancela. */
namespace studio_ui {

class SaveLayerDialog final : public juce::Component {
public:
    /** Lo que el display del hardware muestra: A–Z 0–9 y espacio, en mayúsculas, 12 como máximo (al teclear y al pegar). */
    struct NameFilter final : juce::TextEditor::InputFilter {
        juce::String filterNewText(juce::TextEditor& editor, const juce::String& text) override {
            const int room = 12 - (editor.getTotalNumChars() - editor.getHighlightedRegion().getLength());
            return text.toUpperCase().retainCharacters("ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 ").substring(0, std::max(0, room));
        }
    };
    static constexpr int NAME_CHARS = 12;
    Mode mode = hybrid_mode();
    juce::TextEditor name;
    juce::String halves;                         ///< "A · DX7 + B · OB-6"
    int next_number = 1;                         ///< NEW = LAYER nn
    int overwrite_index = -1;                    ///< capa cargada (−1: no hay, OVERWRITE deshabilitado)
    juce::String overwrite_name;
    bool overwrite = false;
    int role = -1, dialect = -1, rating = 0;
    /// nombre, índice a sobrescribir (−1 = nueva), rol, dialecto, rating
    std::function<void(const juce::String&, int, const juce::String&, const juce::String&, int)> on_save;
    std::function<void()> on_cancel;

    SaveLayerDialog() {
        setComponentID("studio.save.layer");
        name.setComponentID("studio.save.name");
        name.setFont(mono(20.0f));
        name.setInputFilter(new NameFilter(), true);
        name.setColour(juce::TextEditor::backgroundColourId, colour::BG);
        name.setColour(juce::TextEditor::textColourId, colour::WHITE);
        name.setColour(juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
        name.setColour(juce::TextEditor::focusedOutlineColourId, juce::Colours::transparentBlack);
        name.onTextChange = [this] { repaint(); };
        name.onReturnKey = [this] { save(); };
        name.onEscapeKey = [this] { cancel(); };
        addAndMakeVisible(name);
        setWantsKeyboardFocus(true);
    }
    /** Prepara el diálogo para guardar la capa actual. */
    void open(const juce::String& current_name, const juce::String& halves_text, int next, int loaded, const juce::String& loaded_name) {
        halves = halves_text; next_number = next; overwrite_index = loaded; overwrite_name = loaded_name;
        overwrite = false; role = -1; dialect = -1; rating = 0;
        name.setText(current_name.toUpperCase().retainCharacters("ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 ").substring(0, NAME_CHARS), false);
        setVisible(true); toFront(true); name.grabKeyboardFocus(); name.selectAll();
        repaint();
    }
    void save() {
        if (on_save) on_save(name.getText().trim(), overwrite && overwrite_index >= 0 ? overwrite_index : -1,
                             role >= 0 ? StudioBrowser::roles()[role] : juce::String(), dialect >= 0 ? StudioBrowser::dialects()[dialect] : juce::String(), rating);
        setVisible(false);
    }
    void cancel() { setVisible(false); if (on_cancel) on_cancel(); }

    // geometría en coordenadas del editor (1200 × 760); el cuadro va en 320, 136
    static juce::Rectangle<int> box() { return {320, 136, 560, 464}; }
    juce::Rectangle<int> slot_rect(int i) const {
        const int w0 = int(float(slot_text(0).length()) * 6.7f) + 24;
        return i == 0 ? juce::Rectangle<int>{344, 196, w0, 32} : juce::Rectangle<int>{344 + w0 + 8, 196, int(float(slot_text(1).length()) * 6.7f) + 24, 32};
    }
    juce::String slot_text(int i) const {
        if (i == 0) return "NEW " + utf8("\xc2\xb7 LAYER ") + juce::String(next_number).paddedLeft('0', 2);
        return overwrite_index >= 0 ? "OVERWRITE " + utf8("\xc2\xb7 ") + overwrite_name : juce::String("OVERWRITE");
    }
    std::vector<std::pair<juce::Rectangle<int>, int>> chips(int row) const {   // row 0 = rol, 1 = dialecto
        std::vector<std::pair<juce::Rectangle<int>, int>> out;
        const auto& texts = row == 0 ? StudioBrowser::roles() : StudioBrowser::dialects();
        int x = 344, y = row == 0 ? 344 : 404;
        for (int i = 0; i < texts.size(); ++i) {
            const int w = int(float(texts[i].length()) * 6.7f) + 16;
            out.push_back({{x, y, w, 24}, i}); x += w + 4;
        }
        return out;
    }
    static juce::Rectangle<int> pip_rect(int i) { return {420 + i * 20, 444, 18, 22}; }
    juce::Rectangle<int> cancel_rect() const { return {box().getRight() - 24 - save_width() - 8 - 96, box().getBottom() - 64, 96, 40}; }
    juce::Rectangle<int> save_rect() const { return {box().getRight() - 24 - save_width(), box().getBottom() - 64, save_width(), 40}; }
    juce::String save_text() const {
        return overwrite && overwrite_index >= 0 ? "OVERWRITE LAYER " + juce::String(overwrite_index + 1).paddedLeft('0', 2)
                                                 : "SAVE AS LAYER " + juce::String(next_number).paddedLeft('0', 2);
    }
    int save_width() const { return int(float(save_text().length()) * 7.9f) + 32; }

    void resized() override { name.setBounds(352, 268, 412, 40); }
    void paint(juce::Graphics& g) override {
        g.fillAll(juce::Colours::black.withAlpha(0.7f));
        const auto b = box();
        g.setColour(colour::SURFACE); g.fillRect(b);
        g.setColour(mode.hi); g.drawRect(b.toFloat(), 2.0f);
        g.setColour(colour::WHITE); g.setFont(mono(20.0f, true)); g.drawText("SAVE LAYER", 344, 156, 160, 28, juce::Justification::left);
        g.setColour(colour::GRAY); g.setFont(mono(11.0f)); g.drawText(halves, 504, 162, 360, 20, juce::Justification::left);
        for (int i = 0; i < 2; ++i) {
            const auto r = slot_rect(i);
            const bool on = (i == 1) == overwrite, enabled = i == 0 || overwrite_index >= 0;
            if (on) { g.setColour(mode.hi); g.fillRect(r); }
            g.setColour(on ? mode.hi : enabled ? colour::MON : colour::LINE); g.drawRect(r, 1);
            g.setColour(on ? mode.on : enabled ? colour::GRAY : colour::MON); g.drawText(slot_text(i), r, juce::Justification::centred);
        }
        g.setColour(colour::MON_TEXT); g.drawText(utf8("NAME \xc2\xb7 12 CHARACTERS \xc2\xb7 A\xe2\x80\x93Z 0\xe2\x80\x93" "9 SPACE"), 344, 244, 400, 16, juce::Justification::left);
        g.setColour(colour::BG); g.fillRect(344, 264, 512, 48);
        g.setColour(mode.hi); g.drawRect(juce::Rectangle<int>(344, 264, 512, 48), 1);
        const int count = name.getText().length();
        g.setColour(count >= NAME_CHARS ? colour::AMBER : colour::GRAY);
        g.drawText(juce::String(count) + " / " + juce::String(NAME_CHARS), 772, 264, 76, 48, juce::Justification::right);
        g.setColour(colour::MON_TEXT);
        g.drawText("ROLE", 344, 324, 100, 16, juce::Justification::left);
        g.drawText("DIALECT", 344, 384, 100, 16, juce::Justification::left);
        g.drawText("RATING", 344, 444, 70, 22, juce::Justification::left);
        for (int row = 0; row < 2; ++row)
            for (const auto& [r, i] : chips(row)) {
                const bool on = i == (row == 0 ? role : dialect);
                if (on) { g.setColour(mode.hi); g.fillRect(r); }
                g.setColour(on ? mode.hi : colour::LINE); g.drawRect(r, 1);
                g.setColour(on ? mode.on : colour::GRAY);
                g.drawText((row == 0 ? StudioBrowser::roles() : StudioBrowser::dialects())[i], r, juce::Justification::centred);
            }
        g.setColour(mode.hi); g.setFont(mono(16.0f));
        for (int i = 0; i < 3; ++i) g.drawText(utf8(i < rating ? "\xe2\x96\xa0" : "\xe2\x96\xa1"), pip_rect(i), juce::Justification::centred);
        g.setColour(colour::GRAY); g.setFont(mono(13.0f));
        g.drawFittedText("Stores both halves in full, both engines, MORPH, B delay, B transpose and the base mix.",
                         344, 478, 512, 40, juce::Justification::topLeft, 2);
        g.setColour(colour::MON); g.drawRect(cancel_rect(), 1);
        g.setColour(colour::WHITE); g.drawText("CANCEL", cancel_rect(), juce::Justification::centred);
        g.setColour(mode.hi); g.fillRect(save_rect());
        g.setColour(colour::BG); g.setFont(mono(13.0f, true)); g.drawText(save_text(), save_rect(), juce::Justification::centred);
    }
    void mouseDown(const juce::MouseEvent& e) override {
        const auto p = e.getPosition();
        if (!box().contains(p)) { cancel(); return; }
        for (int i = 0; i < 2; ++i) if (slot_rect(i).contains(p) && (i == 0 || overwrite_index >= 0)) { overwrite = i == 1; repaint(); return; }
        for (int row = 0; row < 2; ++row)
            for (const auto& [r, i] : chips(row))
                if (r.contains(p)) { int& v = row == 0 ? role : dialect; v = v == i ? -1 : i; repaint(); return; }
        for (int i = 0; i < 3; ++i) if (pip_rect(i).contains(p)) { rating = rating == i + 1 ? 0 : i + 1; repaint(); return; }
        if (cancel_rect().contains(p)) { cancel(); return; }
        if (save_rect().contains(p)) { save(); return; }
    }
    bool keyPressed(const juce::KeyPress& key) override {
        if (key == juce::KeyPress::escapeKey) { cancel(); return true; }
        if (key == juce::KeyPress::returnKey) { save(); return true; }
        return false;
    }
};

}  // namespace studio_ui
