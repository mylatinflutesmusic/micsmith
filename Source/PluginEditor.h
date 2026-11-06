#pragma once

#include <memory>
#include <vector>

#include <juce_audio_processors/juce_audio_processors.h>

#include "ui/LabelledKnob.h"
#include "ui/MicSmithLookAndFeel.h"
#include "ui/ResponseDisplay.h"

class MicSmithAudioProcessor;

class MicSmithAudioProcessorEditor : public juce::AudioProcessorEditor,
                                     private juce::Timer
{
public:
    explicit MicSmithAudioProcessorEditor (MicSmithAudioProcessor&);
    ~MicSmithAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void updateModelText();
    void applyPreset (int presetIndex);
    void setParam (const juce::String& id, float value);
    void refreshABButtons();
    void switchToSlot (int slot);

    MicSmithAudioProcessor& processor;

    micsmith_ui::MicSmithLookAndFeel lnf;

    juce::ComboBox sourceBox, targetBox, presetBox;
    juce::Label sourceCaption, targetCaption, arrowLabel, modelText, hintText;
    juce::TextButton abA { "A" }, abB { "B" }, abCopy { "COPY" };
    juce::TextButton phaseButton { "PHASE" }, bypassButton { "BYPASS" };

    micsmith_ui::ResponseDisplay display;

    std::vector<std::unique_ptr<micsmith_ui::LabelledKnob>> knobs;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> sourceAttachment, targetAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> phaseAttachment, bypassAttachment;

    float meterIn = 0.0f, meterOut = 0.0f;
    juce::String lastSignature;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MicSmithAudioProcessorEditor)
};
