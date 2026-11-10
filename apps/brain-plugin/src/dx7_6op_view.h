#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "engines/dx7_6op_params.h"
#include <array>
#include <cstring>
#include <memory>
#include <vector>
namespace gf {
/** @brief Eight-control banks built from the same metadata exported to the synth host. */
class Dx7SixView : public juce::Component {
public:
    /** @brief Bind GUI controls to DAW parameters; called on the message thread. */
    void bind(juce::AudioProcessorValueTreeState& state) {
        state_ = &state;
        for (size_t i=0; i<dx7six::param_count; ++i) {
            const auto& p=dx7six::params[i];
            bool found=false;
            for (auto index : banks_) {
                const auto& q=dx7six::params[index];
                if (!std::strcmp(p.page,q.page) && p.bank==q.bank) { found=true; break; }
            }
            if (!found) {
                banks_.push_back(i);
                bank_.addItem(juce::String(p.page)+" / "+juce::String(p.bank+1),int(banks_.size()));
            }
        }
        bank_.setComponentID("dx7_6op.bank"); addAndMakeVisible(bank_);
        bank_.onChange=[this] { show_bank(); };
        for (size_t i=0;i<8;++i) {
            sliders_[i].setSliderStyle(juce::Slider::LinearHorizontal);
            sliders_[i].setTextBoxStyle(juce::Slider::TextBoxRight,false,120,28);
            addAndMakeVisible(sliders_[i]);addAndMakeVisible(labels_[i]);
        }
        for (int i=1;i<=32;++i) voice_.addItem("SYX voice "+juce::String(i),i);
        addAndMakeVisible(voice_); voice_.setComponentID("dx7_6op.sysex_voice");
        voice_attachment_=std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(state,"dx7_6op.sysex_voice",voice_);
        bank_.setSelectedId(1,juce::dontSendNotification);show_bank();
    }
    void paint(juce::Graphics& g) override {
        g.fillAll(juce::Colour(0xff101819));
        g.setColour(juce::Colour(0xff9fe1cb));
        g.drawText("DX7 6-OP",20,12,240,28,juce::Justification::left);
    }
    void resized() override {
        bank_.setBounds(300,12,getWidth()-580,28);
        voice_.setBounds(getWidth()-260,12,240,28);
        for(int i=0;i<8;++i) {
            labels_[i].setBounds(20,60+i*48,190,32);
            sliders_[i].setBounds(220,60+i*48,getWidth()-240,32);
        }
    }
private:
    void show_bank() {
        for (auto& a : attachments_) a.reset();
        // JUCE's conversion callbacks belong to the old attachment, not the Slider.
        for (auto& s : sliders_) { s.textFromValueFunction = nullptr; s.valueFromTextFunction = nullptr; }
        for(size_t i=0;i<8;++i){sliders_[i].setVisible(false);labels_[i].setVisible(false);}
        const int index=bank_.getSelectedId()-1;
        if (!state_ || index<0 || size_t(index)>=banks_.size()) return;
        const auto& first=dx7six::params[banks_[size_t(index)]];
        for(size_t i=0;i<dx7six::param_count;++i) {
            const auto& q=dx7six::params[i];
            if(std::strcmp(q.page,first.page) || q.bank!=first.bank)continue;
            const auto slot=q.slot;
            labels_[slot].setText(q.name,juce::dontSendNotification);
            sliders_[slot].setRange(q.minimum,q.maximum,q.step);
            sliders_[slot].setComponentID(juce::String("dx7_6op.")+q.id);
            attachments_[slot]=std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(*state_,juce::String("dx7_6op.")+q.id,sliders_[slot]);
            const auto choices=q.options[0] ? juce::StringArray::fromTokens(q.options,"|","") : juce::StringArray{};
            const int offset = std::strstr(q.id,"detune") ? 7 : !std::strcmp(q.id,"transpose") ? 24 : 0;
            sliders_[slot].textFromValueFunction=[choices,offset](double value) {
                const int v=juce::roundToInt(value);
                return !choices.isEmpty() && v<choices.size() ? choices[v] : juce::String(v-offset);
            };
            sliders_[slot].valueFromTextFunction=[choices,offset](const juce::String& text) {
                const int index=choices.indexOf(text.trim(),true);
                return index>=0 ? double(index) : text.getDoubleValue()+offset;
            };
            sliders_[slot].updateText();sliders_[slot].setVisible(true);labels_[slot].setVisible(true);
        }
    }
    juce::AudioProcessorValueTreeState* state_ = nullptr;
    juce::ComboBox bank_, voice_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> voice_attachment_;
    std::vector<size_t> banks_;
    std::array<juce::Label,8> labels_;
    std::array<juce::Slider,8> sliders_;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>,8> attachments_;
};
}
