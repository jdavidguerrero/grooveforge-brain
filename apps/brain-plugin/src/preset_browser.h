#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include <functional>
#include <vector>

/** Searchable view of a real preset or layer bank. */
class PresetBrowser final : public juce::Component, private juce::ListBoxModel, private juce::Timer {
public:
    PresetBrowser() {
        setComponentID("browser.panel");
        title_.setText("PRESET BROWSER", juce::dontSendNotification);
        title_.setColour(juce::Label::textColourId, juce::Colour(0xfff2f2f2));
        title_.setFont(juce::Font(juce::FontOptions(14.0f, juce::Font::bold)));
        addAndMakeVisible(title_);
        search_.setComponentID("browser.search");
        search_.setTextToShowWhenEmpty("Buscar por nombre o etiqueta", juce::Colour(0xff8e8ea2));
        search_.onTextChange = [this] { filter(); };
        addAndMakeVisible(search_);
        close_.setComponentID("browser.close"); close_.setButtonText("CERRAR");
        close_.onClick = [this] { cancel_preview(); setVisible(false); };
        addAndMakeVisible(close_);
        favourites_only_.setComponentID("browser.favourites");
        favourites_only_.setButtonText("FAVORITOS");
        favourites_only_.onClick = [this] { filter(); };
        addAndMakeVisible(favourites_only_);
        hover_preview_.setComponentID("browser.preview.hover"); hover_preview_.setButtonText("PREVIEW HOVER");
        hover_preview_.onClick = [this] {
            if (!hover_preview_.getToggleState()) { hover_index_ = -1; cancel_preview(); }
        };
        addAndMakeVisible(hover_preview_);
        cancel_preview_.setComponentID("browser.preview.cancel"); cancel_preview_.setButtonText("CANCEL PREV");
        cancel_preview_.onClick = [this] { cancel_preview(); };
        addChildComponent(cancel_preview_);
        tag_editor_.setComponentID("browser.tag.editor");
        tag_editor_.setTextToShowWhenEmpty("Etiquetas separadas por comas", juce::Colour(0xff8e8ea2));
        tag_editor_.setInputRestrictions(96);
        tag_editor_.onReturnKey = [this] { apply_tags(); };
        addChildComponent(tag_editor_);
        tag_apply_.setComponentID("browser.tag.apply"); tag_apply_.setButtonText("APLICAR #");
        tag_apply_.onClick = [this] { apply_tags(); };
        addChildComponent(tag_apply_);
        list_.setComponentID("browser.list"); list_.setModel(this); list_.setRowHeight(28);
        list_.setColour(juce::ListBox::backgroundColourId, juce::Colour(0xff141414));
        addAndMakeVisible(list_);
        list_.addMouseListener(this, true);
        startTimer(50);
        empty_.setText("Sin resultados", juce::dontSendNotification);
        empty_.setColour(juce::Label::textColourId, juce::Colour(0xff8e8ea2));
        empty_.setJustificationType(juce::Justification::centred);
        addChildComponent(empty_);
        setOpaque(true);
    }
    ~PresetBrowser() override { stopTimer(); list_.removeMouseListener(this); list_.setModel(nullptr); }
    /** Replace the bank while retaining a user's current search. */
    void set_items(const juce::StringArray& names, const std::vector<bool>& favourites = {}, const juce::StringArray& tags = {}) {
        items_ = names; favourites_ = favourites; favourites_.resize(size_t(items_.size()), false);
        tags_ = tags; while (tags_.size() < items_.size()) tags_.add({});
        cancel_preview(); editing_ = -1; tag_editor_.setVisible(false); tag_apply_.setVisible(false); resized(); filter();
    }
    /** Callback receives the original zero-based bank index. */
    std::function<void(int)> on_load;
    /** Called after a user toggles a bank item's favourite flag. */
    std::function<void(int, bool)> on_favourite;
    /** Called when tags for an original bank index are updated. */
    std::function<void(int, juce::String)> on_tags;
    /** Start an isolated audition for the original bank index. */
    std::function<bool(int)> on_preview;
    /** Stop the current audition. */
    std::function<void()> on_cancel_preview;
    void paint(juce::Graphics& g) override {
        g.fillAll(juce::Colour(0xff0d0d0d));
        g.setColour(juce::Colour(0xff2fa8c8)); g.drawRect(getLocalBounds(), 1);
    }
    void resized() override {
        auto bounds = getLocalBounds().reduced(12);
        auto header = bounds.removeFromTop(26);
        title_.setBounds(header.removeFromLeft(180));
        close_.setBounds(header.removeFromRight(80));
        favourites_only_.setBounds(header.removeFromRight(110));
        cancel_preview_.setBounds(header.removeFromRight(115));
        hover_preview_.setBounds(header.removeFromRight(135));
        bounds.removeFromTop(6);
        search_.setBounds(bounds.removeFromTop(24));
        bounds.removeFromTop(6);
        if (editing_ >= 0) {
            auto row = bounds.removeFromTop(24);
            tag_apply_.setBounds(row.removeFromRight(90)); row.removeFromRight(5);
            tag_editor_.setBounds(row); bounds.removeFromTop(6);
        }
        list_.setBounds(bounds); empty_.setBounds(bounds);
    }
private:
    void filter() {
        hover_index_ = -1;
        visible_.clear();
        const auto query = search_.getText().trim();
        for (int i = 0; i < items_.size(); ++i)
            if ((!favourites_only_.getToggleState() || favourites_[size_t(i)]) &&
                (query.isEmpty() || items_[i].containsIgnoreCase(query) || tags_[i].containsIgnoreCase(query))) visible_.push_back(i);
        list_.deselectAllRows(); list_.updateContent(); list_.repaint();
        empty_.setVisible(visible_.empty());
    }
    int getNumRows() override { return int(visible_.size()); }
    void paintListBoxItem(int row, juce::Graphics& g, int width, int height, bool selected) override {
        if (row < 0 || size_t(row) >= visible_.size()) return;
        if (selected) { g.setColour(juce::Colour(0xff0f4352)); g.fillAll(); }
        const int index = visible_[size_t(row)];
        g.setColour(juce::Colour(favourites_[size_t(index)] ? 0xffe8b84a : 0xff8e8ea2));
        g.setFont(juce::Font(juce::FontOptions(15.0f)));
        g.drawText(favourites_[size_t(index)] ? juce::String::fromUTF8("★") : juce::String::fromUTF8("☆"), 5, 0, 20, height, juce::Justification::centred);
        g.setColour(juce::Colour(0xff8e8ea2)); g.setFont(juce::Font(juce::FontOptions(10.0f)));
        g.drawText("#", 27, 0, 20, height, juce::Justification::centred);
        g.drawText(juce::String(index + 1).paddedLeft('0', 2), 48, 0, 22, height, juce::Justification::centredLeft);
        g.setColour(juce::Colour(0xfff2f2f2)); g.setFont(juce::Font(juce::FontOptions(12.0f)));
        g.drawText(items_[index], 72, 0, juce::jmax(0, width - 305), height, juce::Justification::centredLeft, true);
        g.setColour(juce::Colour(0xff8e8ea2)); g.setFont(juce::Font(juce::FontOptions(10.0f)));
        g.drawText(tags_[index], width - 227, 0, 164, height, juce::Justification::centredRight, true);
        g.setColour(juce::Colour(index == preview_index_ ? 0xffe8b84a : 0xff7ad4ea));
        g.drawText(index == preview_index_ ? "PLAYING" : "PREV", width - 60, 0, 54, height, juce::Justification::centred);
    }
    void listBoxItemClicked(int row, const juce::MouseEvent& event) override {
        if (row < 0 || size_t(row) >= visible_.size()) return;
        const int index = visible_[size_t(row)];
        if (event.position.x < 26.0f) {
            favourites_[size_t(index)] = !favourites_[size_t(index)];
            if (on_favourite) on_favourite(index, favourites_[size_t(index)]);
            filter();
        } else if (event.position.x < 48.0f) {
            editing_ = index;
            tag_editor_.setText(tags_[index], false);
            tag_editor_.setVisible(true); tag_apply_.setVisible(true); resized();
            tag_editor_.grabKeyboardFocus();
        } else if (event.position.x >= float(list_.getWidth() - 60)) {
            if (on_preview && on_preview(index)) { preview_index_ = index; cancel_preview_.setVisible(true); list_.repaint(); }
        } else if (on_load) { cancel_preview(); on_load(index); }
    }
    void cancel_preview() {
        hover_index_ = -1;
        if (preview_index_ >= 0 && on_cancel_preview) on_cancel_preview();
        preview_index_ = -1; cancel_preview_.setVisible(false); list_.repaint();
    }
    void mouseMove(const juce::MouseEvent& event) override {
        if (!hover_preview_.getToggleState() || !isVisible()) return;
        const auto point = event.getEventRelativeTo(&list_).position;
        const int row = list_.getRowContainingPosition(int(point.x), int(point.y));
        const int index = point.x >= 48.0f && point.x < float(list_.getWidth() - 60) && row >= 0 && size_t(row) < visible_.size()
            ? visible_[size_t(row)] : -1;
        if (index == hover_index_) return;
        hover_index_ = index;
        hover_since_ = juce::Time::getMillisecondCounter();
        if (preview_index_ >= 0 && preview_index_ != index) {
            if (on_cancel_preview) on_cancel_preview();
            preview_index_ = -1; cancel_preview_.setVisible(false); list_.repaint();
        }
    }
    void mouseExit(const juce::MouseEvent& event) override {
        const auto point = event.getEventRelativeTo(&list_).position;
        if (hover_preview_.getToggleState() && !list_.getLocalBounds().toFloat().contains(point)) cancel_preview();
    }
    void listWasScrolled() override { if (hover_preview_.getToggleState()) cancel_preview(); }
    void timerCallback() override {
        if (!isVisible() || !hover_preview_.getToggleState() || hover_index_ < 0 || hover_index_ == preview_index_) return;
        if (juce::Time::getMillisecondCounter() - hover_since_ >= 300 && on_preview && on_preview(hover_index_)) {
            preview_index_ = hover_index_; cancel_preview_.setVisible(true); list_.repaint();
        }
    }
    void visibilityChanged() override { if (!isVisible()) cancel_preview(); }
    void apply_tags() {
        if (editing_ < 0 || editing_ >= items_.size()) return;
        const int index = editing_;
        tags_.set(index, tag_editor_.getText().trim().substring(0, 96));
        if (on_tags) on_tags(index, tags_[index]);
        editing_ = -1; tag_editor_.setVisible(false); tag_apply_.setVisible(false);
        resized(); filter();
    }
    juce::StringArray items_, tags_;
    int editing_ = -1, preview_index_ = -1, hover_index_ = -1;
    juce::uint32 hover_since_ = 0;
    std::vector<int> visible_;
    std::vector<bool> favourites_;
    juce::Label title_, empty_;
    juce::TextEditor search_, tag_editor_;
    juce::TextButton close_, tag_apply_, cancel_preview_;
    juce::ToggleButton favourites_only_, hover_preview_;
    juce::ListBox list_;
};
