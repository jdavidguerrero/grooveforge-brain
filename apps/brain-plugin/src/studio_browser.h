#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include <algorithm>
#include <array>
#include <utility>
#include <functional>
#include <vector>

#include "studio_shell.h"

/* Navegador de la referencia (1i; gemelo-vst-ui-migracion.md §7). Solo dibuja y filtra: el editor le pasa los presets
 * con sus datos (rol, dialecto, rating, descarte) y recibe cargar, escuchar y guardar datos. */
namespace studio_ui {

/** Un preset en la lista, con los datos que el usuario le pone. */
struct BrowserItem {
    juce::String name, role, dialect, tags;          ///< tags: etiquetas libres separadas por comas
    int rating = 0;                                  ///< 0–3
    bool discarded = false, brain = true;            ///< brain: también está en el banco del hardware
};

class StudioBrowser final : public juce::Component, private juce::Timer {
public:
    static const juce::StringArray& roles() { static const juce::StringArray r{"bass", "chords", "pluck", "lead", "pad", "perc", "fx"}; return r; }
    static const juce::StringArray& dialects() { static const juce::StringArray d{"afro", "organic", "melodic", "indie", "disco"}; return d; }
    static constexpr int ROW_H = 32, LIST_X = 288, LIST_Y = 72, LIST_W = 616, LIST_H = 448;

    Mode mode = synth_mode();
    std::function<void(int)> on_engine;                          ///< la columna ENGINE pide los presets de otro engine
    std::function<void(int, int)> on_load;                       ///< engine, índice
    std::function<bool(int, int)> on_preview;
    std::function<void()> on_cancel_preview, on_close;
    std::function<void(int, int, const BrowserItem&)> on_meta;   ///< guardar los datos de un preset
    juce::TextEditor search;
    bool hover_preview = false, show_discarded = false;
    // HYBRID (2d): engine bloqueado a la mitad en foco, el navegador queda abierto al cargar, SAVE LAYER y aviso al pie
    int locked_engine = -1;
    bool keep_open_on_load = false;
    juce::String context, warn;
    std::function<void()> on_save_layer;
    int role_filter = -1, dialect_filter = -1, min_rating = 0;

    StudioBrowser() {
        setComponentID("studio.browser");
        search.setComponentID("studio.browser.search");
        search.setTextToShowWhenEmpty("Search name or tag", colour::MON_TEXT);
        search.setColour(juce::TextEditor::backgroundColourId, colour::BG);
        search.setColour(juce::TextEditor::outlineColourId, colour::MON);
        search.setColour(juce::TextEditor::focusedOutlineColourId, colour::GRAY);
        search.setColour(juce::TextEditor::textColourId, colour::WHITE);
        search.setFont(mono(13.0f)); search.setIndents(12, 8);
        search.onTextChange = [this] { filter(); };
        search.onEscapeKey = [this] { close(); };
        search.onReturnKey = [this] { load_selected(); };
        addAndMakeVisible(search);
        setWantsKeyboardFocus(true);
        startTimer(50);
    }
    ~StudioBrowser() override { stopTimer(); }

    /** Cantidad de presets de cada engine (0–5) y de capas (6); `layers_enabled` = LAYERS se puede elegir. */
    void set_engines(std::array<int, 7> counts, bool layers_enabled) { counts_ = counts; layers_enabled_ = layers_enabled; repaint(); }
    /** Presets del engine listado; `loaded` = índice cargado en este engine (−1 si no), `loaded_name` para A/B. */
    void set_items(int engine, std::vector<BrowserItem> items, int loaded, juce::String loaded_name) {
        if (engine != engine_) { selected_ = -1; scroll_ = 0; }
        stop_preview();
        engine_ = engine; items_ = std::move(items); loaded_ = loaded; loaded_name_ = std::move(loaded_name);
        if (selected_ < 0) selected_ = loaded;
        filter();
    }
    int engine() const { return engine_; }
    const std::vector<BrowserItem>& items() const { return items_; }
    const std::vector<int>& visible() const { return visible_; }
    int selected() const { return selected_; }
    int previewing() const { return preview_; }
    void select(int index) { selected_ = index; scroll_into_view(); repaint(); }
    void set_loaded(int index) { loaded_ = index; repaint(); }

    // datos de un preset (menú de la fila, cuadros de rating, tests)
    void set_rating(int index, int rating) { if (valid(index)) { items_[size_t(index)].rating = juce::jlimit(0, 3, rating); changed(index); } }
    void set_role(int index, const juce::String& role) { if (valid(index)) { items_[size_t(index)].role = role; changed(index); } }
    void set_dialect(int index, const juce::String& dialect) { if (valid(index)) { items_[size_t(index)].dialect = dialect; changed(index); } }
    void set_tags(int index, const juce::String& tags) { if (valid(index)) { items_[size_t(index)].tags = tags.trim().substring(0, 96); changed(index); } }
    void set_discarded(int index, bool discarded) { if (valid(index)) { items_[size_t(index)].discarded = discarded; changed(index); } }
    void set_role_filter(int i) { role_filter = role_filter == i ? -1 : i; filter(); }
    void set_dialect_filter(int i) { dialect_filter = dialect_filter == i ? -1 : i; filter(); }
    void set_min_rating(int r) { min_rating = r; filter(); }
    void set_show_discarded(bool on) { show_discarded = on; filter(); }

    void load_selected() {
        if (!valid(selected_)) return;
        stop_preview();
        const int index = selected_;
        if (on_load) on_load(engine_, index);
        if (keep_open_on_load) set_loaded(index);
    }
    /** A/B: B = escuchar la fila elegida; A = volver al cargado. */
    void toggle_ab() {
        if (preview_ >= 0) stop_preview();
        else if (valid(selected_) && selected_ != loaded_ && on_preview && on_preview(engine_, selected_)) preview_ = selected_;
        repaint();
    }
    void stop_preview() {
        hover_ = -1;
        if (preview_ >= 0 && on_cancel_preview) on_cancel_preview();
        preview_ = -1; repaint();
    }
    void close() { stop_preview(); setVisible(false); if (on_close) on_close(); }

    // ── geometría (relativa al navegador, 1200 × 536) ──
    static juce::Rectangle<int> engine_row(int i) { return {16, 72 + i * 40, 184, 40}; }
    static juce::Rectangle<int> bank_row(int i) { return {208, 72 + i * 40, 72, 40}; }
    juce::Rectangle<int> row_rect(int visible_row) const { return {LIST_X, LIST_Y + (visible_row - scroll_) * ROW_H, LIST_W, ROW_H}; }
    static juce::Rectangle<int> rate_rect(juce::Rectangle<int> row) { return {row.getX() + 8 + 24 + 8 + 168 + 8 + 184 + 8, row.getY(), 48, ROW_H}; }
    juce::Rectangle<int> ab_rect() const { return {getWidth() - 16 - 32 - 8 - 168 - 8 - ab_width(), 8, ab_width(), 32}; }
    juce::Rectangle<int> hover_rect() const { return {getWidth() - 16 - 32 - 8 - 168, 8, 168, 32}; }
    juce::Rectangle<int> close_rect() const { return {getWidth() - 48, 8, 32, 32}; }
    static juce::Rectangle<int> chip_area() { return {920, 72, 264, 300}; }
    juce::Rectangle<int> load_rect() const { return {920, getHeight() - 56, 264, 32}; }
    juce::Rectangle<int> save_layer_rect() const { return {920, getHeight() - 96, 264, 32}; }
    bool engine_enabled(int i) const { return locked_engine >= 0 ? i == locked_engine : (i < 6 || layers_enabled_); }

    void resized() override { search.setBounds(16, 8, 320, 32); }
    void paint(juce::Graphics& g) override {
        g.fillAll(colour::BG);
        g.setColour(colour::LINE); g.drawRect(getLocalBounds(), 1);
        // contexto y botones
        g.setFont(mono(11.0f)); g.setColour(mode.hi);
        g.drawText(context.isNotEmpty() ? context : engine_name(engine_) + utf8(" \xc2\xb7 BANK A \xc2\xb7 ") + juce::String(int(items_.size())),
                   352, 16, 360, 16, juce::Justification::left);
        g.setColour(colour::LINE);
        const auto ab = ab_rect();
        g.setColour(preview_ >= 0 ? mode.hi : mode.hi.withAlpha(0.7f)); g.drawRect(ab, 1);
        if (preview_ >= 0) { g.setColour(mode.dim); g.fillRect(ab.reduced(1)); }
        g.setColour(mode.hi); g.drawText(ab_text(), ab, juce::Justification::centred);
        const auto hv = hover_rect();
        if (hover_preview) { g.setColour(mode.dim); g.fillRect(hv); }
        g.setColour(hover_preview ? mode.hi : colour::MON); g.drawRect(hv, 1);
        g.setColour(hover_preview ? colour::WHITE : colour::GRAY);
        g.drawText(utf8(hover_preview ? "\xe2\x96\xa0 PREVIEW ON HOVER" : "\xe2\x96\xa1 PREVIEW ON HOVER"), hv, juce::Justification::centred);
        g.setColour(colour::MON); g.drawRect(close_rect(), 1);
        g.setColour(colour::WHITE); g.setFont(mono(13.0f)); g.drawText(utf8("\xe2\x9c\x95"), close_rect(), juce::Justification::centred);
        // ENGINE y BANK
        g.setFont(mono(11.0f)); g.setColour(colour::MON_TEXT);
        g.drawText("ENGINE", 16, 48, 184, 24, juce::Justification::left);
        g.drawText("BANK", 208, 48, 72, 24, juce::Justification::left);
        for (int i = 0; i < 7; ++i) {
            const auto r = engine_row(i);
            const bool enabled = engine_enabled(i);
            if (i == engine_) { g.setColour(mode.dim); g.fillRect(r); }
            g.setColour(colour::SURFACE_2); g.drawHorizontalLine(r.getBottom() - 1, float(r.getX()), float(r.getRight()));
            g.setFont(mono(13.0f)); g.setColour(enabled ? colour::WHITE : colour::MON);
            g.drawText(engine_name(i), r.reduced(8, 0), juce::Justification::left);
            g.setFont(mono(11.0f)); g.setColour(colour::MON_TEXT);
            g.drawText(juce::String(counts_[size_t(i)]), r.reduced(8, 0), juce::Justification::right);
        }
        for (int i = 0; i < 4; ++i) {
            const auto r = bank_row(i);
            if (i == 0) { g.setColour(mode.dim); g.fillRect(r); }
            g.setColour(colour::SURFACE_2); g.drawHorizontalLine(r.getBottom() - 1, float(r.getX()), float(r.getRight()));
            g.setFont(mono(13.0f)); g.setColour(i == 0 ? colour::WHITE : colour::MON);
            g.drawText(juce::String::charToString(juce::juce_wchar('A' + i)), r.reduced(8, 0), juce::Justification::left);
        }
        // lista
        g.setFont(mono(11.0f)); g.setColour(colour::MON_TEXT);
        int hx = LIST_X + 8;
        for (auto [t, w] : {std::pair<const char*, int>{"#", 24}, {"NAME", 168}, {"TAGS", 184}, {"RATE", 48}, {"WHERE", 88}}) {
            g.drawText(t, hx, 48, w, 24, juce::Justification::left); hx += w + 8;
        }
        {
            juce::Graphics::ScopedSaveState clip(g);
            g.reduceClipRegion(LIST_X, LIST_Y, LIST_W, LIST_H);
            for (int row = scroll_; row < int(visible_.size()) && row < scroll_ + LIST_H / ROW_H + 1; ++row) paint_row(g, row);
        }
        if (visible_.empty()) {
            g.setColour(colour::MON_TEXT); g.setFont(mono(13.0f));
            g.drawText("NO PRESETS MATCH", LIST_X, LIST_Y + 40, LIST_W, 24, juce::Justification::centred);
        }
        // filtros
        auto chips = chip_layout();
        g.setFont(mono(11.0f)); g.setColour(colour::MON_TEXT);
        for (const auto& h : headings_) g.drawText(h.second, h.first, juce::Justification::left);
        for (const auto& c : chips) {
            if (c.on) { g.setColour(mode.hi); g.fillRect(c.r); }
            g.setColour(c.on ? mode.hi : colour::LINE); g.drawRect(c.r, 1);
            g.setColour(c.on ? mode.on : colour::GRAY); g.drawText(c.text, c.r, juce::Justification::centred);
        }
        g.setColour(colour::GRAY);
        g.drawFittedText(utf8("\xe2\x97\x8f BRAIN \xc2\xb7 in the hardware bank\n\xe2\x97\x8b VST ONLY \xc2\xb7 this machine only"),
                         920, legend_y_, 264, 32, juce::Justification::topLeft, 2);
        if (on_save_layer) {
            const auto sr = save_layer_rect();
            g.setColour(mode.mc); g.fillRect(sr);
            g.setColour(mode.on); g.drawText("SAVE LAYER", sr, juce::Justification::centred);
        }
        if (warn.isNotEmpty()) {
            g.setColour(colour::AMBER);
            g.drawFittedText(warn, 920, save_layer_rect().getY() - 40, 264, 36, juce::Justification::bottomLeft, 2);
        }
        const auto lr = load_rect();
        const bool can_load = valid(selected_);
        g.setColour(can_load ? mode.hi : colour::SURFACE_2); g.fillRect(lr);
        g.setColour(can_load ? mode.on : colour::MON); g.drawText("LOAD", lr, juce::Justification::centred);
    }

    void mouseDown(const juce::MouseEvent& e) override {
        const auto p = e.getPosition();
        if (close_rect().contains(p)) { close(); return; }
        if (hover_rect().contains(p)) { hover_preview = !hover_preview; if (!hover_preview) stop_preview(); repaint(); return; }
        if (ab_rect().contains(p)) { toggle_ab(); return; }
        if (load_rect().contains(p)) { load_selected(); return; }
        if (on_save_layer && save_layer_rect().contains(p)) { on_save_layer(); return; }
        for (int i = 0; i < 7; ++i)
            if (engine_row(i).contains(p) && engine_enabled(i) && i != engine_) { if (on_engine) on_engine(i); return; }
        for (const auto& c : chip_layout()) if (c.r.contains(p)) { c.action(); return; }
        const int row = row_at(p);
        if (row < 0) return;
        const int index = visible_[size_t(row)];
        selected_ = index;
        if (e.mods.isPopupMenu()) { show_menu(index); repaint(); return; }
        const auto rate = rate_rect(row_rect(row));
        if (rate.contains(p) && !items_[size_t(index)].discarded) {
            const int pip = juce::jlimit(1, 3, (p.x - rate.getX()) / 12 + 1);
            set_rating(index, items_[size_t(index)].rating == pip ? 0 : pip);
        }
        repaint();
    }
    void mouseDoubleClick(const juce::MouseEvent& e) override { if (row_at(e.getPosition()) >= 0) load_selected(); }
    void mouseMove(const juce::MouseEvent& e) override {
        const int row = row_at(e.getPosition());
        const int index = row >= 0 ? visible_[size_t(row)] : -1;
        if (index == hover_) return;
        hover_ = index; hover_since_ = juce::Time::getMillisecondCounter();
        if (hover_preview && preview_ >= 0 && preview_ != index) { if (on_cancel_preview) on_cancel_preview(); preview_ = -1; }
        repaint();
    }
    void mouseExit(const juce::MouseEvent&) override { if (hover_preview) stop_preview(); hover_ = -1; repaint(); }
    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails& w) override {
        const int max_scroll = std::max(0, int(visible_.size()) - LIST_H / ROW_H);
        scroll_ = juce::jlimit(0, max_scroll, scroll_ + (w.deltaY < 0 ? 1 : -1));
        if (hover_preview) stop_preview();
        repaint();
    }
    bool keyPressed(const juce::KeyPress& key) override {
        if (key == juce::KeyPress::escapeKey) { close(); return true; }
        if (key == juce::KeyPress::returnKey) { load_selected(); return true; }
        if (key == juce::KeyPress::upKey || key == juce::KeyPress::downKey) {
            if (visible_.empty()) return true;
            auto at = std::find(visible_.begin(), visible_.end(), selected_);
            int row = at == visible_.end() ? 0 : int(at - visible_.begin()) + (key == juce::KeyPress::downKey ? 1 : -1);
            selected_ = visible_[size_t(juce::jlimit(0, int(visible_.size()) - 1, row))];
            scroll_into_view(); repaint(); return true;
        }
        return false;
    }
    void visibilityChanged() override { if (!isVisible()) stop_preview(); }

private:
    struct Chip { juce::Rectangle<int> r; juce::String text; bool on; std::function<void()> action; };
    std::vector<BrowserItem> items_;
    std::vector<int> visible_;
    std::array<int, 7> counts_{};
    bool layers_enabled_ = false;
    int engine_ = -1, loaded_ = -1, selected_ = -1, preview_ = -1, hover_ = -1, scroll_ = 0, legend_y_ = 400;
    juce::uint32 hover_since_ = 0;
    juce::String loaded_name_;
    std::vector<std::pair<juce::Rectangle<int>, juce::String>> headings_;

    bool valid(int index) const { return index >= 0 && index < int(items_.size()); }
    static juce::String engine_name(int i) { return i >= 0 && i < 6 ? juce::String(engines()[size_t(i)].name) : juce::String("LAYERS"); }
    juce::String ab_text() const { return utf8("A/B \xc2\xb7 vs ") + (loaded_name_.isEmpty() ? juce::String("LOADED") : loaded_name_.toUpperCase()); }
    int ab_width() const { return int(float(ab_text().length()) * 6.7f) + 24; }
    void changed(int index) {
        if (on_meta) on_meta(engine_, index, items_[size_t(index)]);
        filter();
    }
    bool matches(const BrowserItem& it) const {
        const auto q = search.getText().trim();
        if (q.isNotEmpty() && !(it.name.containsIgnoreCase(q) || it.role.containsIgnoreCase(q) || it.dialect.containsIgnoreCase(q) || it.tags.containsIgnoreCase(q))) return false;
        if (role_filter >= 0 && it.role != roles()[role_filter]) return false;
        if (dialect_filter >= 0 && it.dialect != dialects()[dialect_filter]) return false;
        if (it.rating < min_rating) return false;
        return show_discarded || !it.discarded;
    }
    void filter() {
        visible_.clear();
        for (int i = 0; i < int(items_.size()); ++i) if (matches(items_[size_t(i)])) visible_.push_back(i);
        scroll_ = juce::jlimit(0, std::max(0, int(visible_.size()) - LIST_H / ROW_H), scroll_);
        repaint();
    }
    int row_at(juce::Point<int> p) const {
        if (!juce::Rectangle<int>(LIST_X, LIST_Y, LIST_W, LIST_H).contains(p)) return -1;
        const int row = scroll_ + (p.y - LIST_Y) / ROW_H;
        return row < int(visible_.size()) ? row : -1;
    }
    void scroll_into_view() {
        auto at = std::find(visible_.begin(), visible_.end(), selected_);
        if (at == visible_.end()) return;
        const int row = int(at - visible_.begin()), rows = LIST_H / ROW_H;
        if (row < scroll_) scroll_ = row;
        if (row >= scroll_ + rows) scroll_ = row - rows + 1;
    }
    void paint_row(juce::Graphics& g, int row) {
        const int index = visible_[size_t(row)];
        const auto& it = items_[size_t(index)];
        const auto r = row_rect(row);
        const bool loaded = index == loaded_, previewing = index == preview_, sel = index == selected_;
        if (loaded) { g.setColour(mode.dim); g.fillRect(r); }
        else if (sel || index == hover_) { g.setColour(colour::SURFACE_2); g.fillRect(r); }
        if (sel) { g.setColour(mode.hi); g.drawRect(r, 1); }
        g.setColour(colour::SURFACE_2); g.drawHorizontalLine(r.getBottom() - 1, float(r.getX()), float(r.getRight()));
        int x = r.getX() + 8;
        g.setFont(mono(11.0f)); g.setColour(colour::MON_TEXT);
        g.drawText(juce::String(index + 1).paddedLeft('0', 2), x, r.getY(), 24, ROW_H, juce::Justification::left); x += 32;
        g.setFont(mono(13.0f)); g.setColour(it.discarded ? colour::MON_TEXT : colour::WHITE);
        g.drawText(it.name, x, r.getY(), 168, ROW_H, juce::Justification::left, true);
        if (it.discarded) {
            const float w = std::min(168.0f, float(it.name.length()) * 7.9f);
            g.drawLine(float(x), float(r.getCentreY()), float(x) + w, float(r.getCentreY()), 1.0f);
        }
        x += 176;
        g.setFont(mono(11.0f));
        int tx = x;
        juce::StringArray tags;
        for (const auto& t : {it.role, it.dialect}) if (t.isNotEmpty()) tags.add(t);
        tags.addTokens(it.tags, ",", {}); tags.trim(); tags.removeEmptyStrings();
        for (const auto& t : tags) {
            const int w = int(float(t.length()) * 6.7f) + 8;
            if (tx + w > x + 184) break;
            g.setColour(colour::LINE); g.drawRect(tx, r.getY() + 7, w, 18, 1);
            g.setColour(colour::GRAY); g.drawText(t, tx, r.getY() + 7, w, 18, juce::Justification::centred);
            tx += w + 4;
        }
        x += 192;
        if (!it.discarded) {
            juce::String pips;
            for (int i = 0; i < 3; ++i) pips += utf8(i < it.rating ? "\xe2\x96\xa0" : "\xe2\x96\xa1");
            g.setColour(mode.hi); g.drawText(pips, x, r.getY(), 48, ROW_H, juce::Justification::left);
        }
        x += 56;
        g.setColour(it.brain ? colour::WHITE : colour::GRAY);
        g.drawText(utf8(it.brain ? "\xe2\x97\x8f BRAIN" : "\xe2\x97\x8b VST ONLY"), x, r.getY(), 88, ROW_H, juce::Justification::left);
        const juce::String state = it.discarded ? juce::String("DISCARDED") : previewing ? utf8("\xe2\x96\xb6 PREVIEWING") : loaded ? juce::String("LOADED") : juce::String();
        g.setColour(it.discarded ? colour::MON_TEXT : previewing ? mode.hi : colour::WHITE);
        g.drawText(state, r.reduced(8, 0), juce::Justification::right);
    }
    std::vector<Chip> chip_layout() {
        std::vector<Chip> out;
        headings_.clear();
        int y = 48;
        auto heading = [&](const juce::String& t) { headings_.push_back({{920, y, 264, 16}, t}); y += 24; };
        auto row = [&](const juce::StringArray& texts, std::function<bool(int)> on, std::function<void(int)> act) {
            int x = 920;
            for (int i = 0; i < texts.size(); ++i) {
                const int w = int(float(texts[i].length()) * 6.7f) + 16;
                if (x + w > 1184) { x = 920; y += 28; }
                out.push_back({{x, y, w, 24}, texts[i], on(i), [act, i] { act(i); }});
                x += w + 4;
            }
            y += 36;
        };
        heading("ROLE");
        row(roles(), [this](int i) { return role_filter == i; }, [this](int i) { set_role_filter(i); });
        heading("DIALECT");
        row(dialects(), [this](int i) { return dialect_filter == i; }, [this](int i) { set_dialect_filter(i); });
        heading(utf8("RATING \xc2\xb7 MIN"));
        row({"ANY", utf8("\xe2\x96\xa0"), utf8("\xe2\x96\xa0\xe2\x96\xa0"), utf8("\xe2\x96\xa0\xe2\x96\xa0\xe2\x96\xa0")},
            [this](int i) { return min_rating == i; }, [this](int i) { set_min_rating(i); });
        headings_.push_back({{920, y + 4, 180, 16}, "SHOW DISCARDED"});
        out.push_back({{1184 - 48, y, 48, 24}, show_discarded ? "ON" : "OFF", show_discarded, [this] { set_show_discarded(!show_discarded); }});
        legend_y_ = y + 40;
        return out;
    }
    void show_menu(int index) {
        const auto& it = items_[size_t(index)];
        juce::PopupMenu role, dialect, rating, menu;
        for (int i = 0; i < roles().size(); ++i) role.addItem(100 + i, roles()[i], true, it.role == roles()[i]);
        role.addItem(199, "none", true, it.role.isEmpty());
        for (int i = 0; i < dialects().size(); ++i) dialect.addItem(200 + i, dialects()[i], true, it.dialect == dialects()[i]);
        dialect.addItem(299, "none", true, it.dialect.isEmpty());
        for (int i = 0; i <= 3; ++i) rating.addItem(300 + i, i == 0 ? juce::String("0") : juce::String::repeatedString(utf8("\xe2\x96\xa0"), i), true, it.rating == i);
        menu.addSectionHeader(it.name);
        menu.addSubMenu("ROLE", role); menu.addSubMenu("DIALECT", dialect); menu.addSubMenu("RATING", rating);
        menu.addItem(400, utf8("EDIT TAGS\xe2\x80\xa6"));
        menu.addItem(500, it.discarded ? "RESTORE" : "DISCARD");
        juce::Component::SafePointer<StudioBrowser> self(this);
        menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this).withMousePosition(), [self, index](int id) {
            if (!self || id == 0) return;
            if (id >= 100 && id < 199) self->set_role(index, roles()[id - 100]);
            else if (id == 199) self->set_role(index, {});
            else if (id >= 200 && id < 299) self->set_dialect(index, dialects()[id - 200]);
            else if (id == 299) self->set_dialect(index, {});
            else if (id >= 300 && id < 400) self->set_rating(index, id - 300);
            else if (id == 400) self->edit_tags(index);
            else if (id == 500) self->set_discarded(index, !self->items_[size_t(index)].discarded);
        });
    }
    void edit_tags(int index) {
        auto* window = new juce::AlertWindow("EDIT TAGS", items_[size_t(index)].name, juce::MessageBoxIconType::NoIcon, this);
        window->addTextEditor("tags", items_[size_t(index)].tags, "comma separated");
        window->addButton("APPLY", 1, juce::KeyPress(juce::KeyPress::returnKey));
        window->addButton("CANCEL", 0, juce::KeyPress(juce::KeyPress::escapeKey));
        juce::Component::SafePointer<StudioBrowser> self(this);
        window->enterModalState(true, juce::ModalCallbackFunction::create([self, window, index](int result) {
            if (self && result == 1) self->set_tags(index, window->getTextEditorContents("tags"));
        }), true);
    }
    void timerCallback() override {
        if (!isVisible() || !hover_preview || hover_ < 0 || hover_ == preview_ || hover_ == loaded_) return;
        if (juce::Time::getMillisecondCounter() - hover_since_ >= 300 && on_preview && on_preview(engine_, hover_)) { preview_ = hover_; repaint(); }
    }
};

}  // namespace studio_ui
