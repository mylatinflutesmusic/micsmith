#pragma once

#include <atomic>
#include <memory>
#include <vector>

#include <juce_audio_processors/juce_audio_processors.h>

#include "ui/LabelledKnob.h"
#include "ui/PresetBrowser.h"
#include "ui/QuellLookAndFeel.h"
#include "ui/SpectrumDisplay.h"

class QuellAudioProcessor;

class QuellAudioProcessorEditor : public juce::AudioProcessorEditor,
                                  private juce::Timer,
                                  private juce::AudioProcessorValueTreeState::Listener
{
public:
    explicit QuellAudioProcessorEditor (QuellAudioProcessor&);
    ~QuellAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void parameterChanged (const juce::String& parameterID, float newValue) override;
    void openPresetBrowser();
    void markPresetUnsaved();
    void setCurrentPresetName (const juce::String& name);
    void refreshABButtons();
    void switchToSlot (int slot);

    QuellAudioProcessor& processor;

    quell_ui::QuellLookAndFeel lnf;

    juce::TextButton presetButton { "Init" };
    juce::TextButton abA { "A" }, abB { "B" }, abCopy { "COPY" };
    juce::TextButton linkButton { "LINK" }, msButton { "M/S" }, scButton { "SC" },
                     deltaButton { "DELTA" }, bypassButton { "BYPASS" };

    quell_ui::SpectrumDisplay display;

    std::vector<std::unique_ptr<quell_ui::LabelledKnob>> knobs;

    std::unique_ptr<quell_ui::PresetBrowser> presetBrowser;
    juce::String currentPresetName { "Init" };
    std::atomic<bool> ignoreParameterChanges { false };

    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> linkAttachment,
                                                                         msAttachment,
                                                                         scAttachment,
                                                                         deltaAttachment,
                                                                         bypassAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (QuellAudioProcessorEditor)
};
