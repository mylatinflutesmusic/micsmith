#pragma once

#include <functional>
#include <memory>
#include <vector>

#include <juce_audio_processors/juce_audio_processors.h>

#include "ui/ChannelDisplay.h"
#include "ui/ChannelLookAndFeel.h"
#include "ui/Knob.h"

class KChannelAudioProcessor;

class KChannelAudioProcessorEditor : public juce::AudioProcessorEditor,
                                     private juce::Timer
{
public:
    explicit KChannelAudioProcessorEditor (KChannelAudioProcessor&);
    ~KChannelAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    enum class Group
    {
        input,
        eq,
        comp,
        gate,
        quell,
        master
    };

    struct Control
    {
        std::unique_ptr<kchannel_ui::Knob> knob;
        Group group;
        int column;
        int row;
    };

    void timerCallback() override;
    void addKnob (Group, const char* id, const char* caption, const char* tooltip,
                  std::function<juce::String (double)> formatter);
    void configureButton (juce::TextButton&, const juce::String& text, const juce::String& tooltip);

    KChannelAudioProcessor& processor;

    kchannel_ui::ChannelLookAndFeel lnf;

    juce::TextButton filtersButton { "FILTERS" }, eqButton { "EQ" },
                     compButton { "COMP" }, gateButton { "GATE" },
                     quellButton { "QUELL" }, quellPostButton { "Q POST" },
                     phaseButton { "PHASE" }, bypassButton { "BYPASS" };

    kchannel_ui::ChannelDisplay display;

    std::vector<Control> controls;

    juce::Rectangle<int> inputPanel, eqPanel, dynPanel, quellPanel, masterPanel;
    juce::Rectangle<int> compAreaRect, gateAreaRect;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> filtersAttachment,
                                                                         eqAttachment,
                                                                         compAttachment,
                                                                         gateAttachment,
                                                                         quellAttachment,
                                                                         quellPostAttachment,
                                                                         phaseAttachment,
                                                                         bypassAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KChannelAudioProcessorEditor)
};
