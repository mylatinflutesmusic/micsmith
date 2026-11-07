#pragma once

#include <atomic>
#include <juce_audio_processors/juce_audio_processors.h>

#include "dsp/SpectralSuppressor.h"

class QuellAudioProcessor : public juce::AudioProcessor
{
public:
    QuellAudioProcessor();
    ~QuellAudioProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void reset() override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    juce::AudioProcessorParameter* getBypassParameter() const override;

    //==============================================================================
    const juce::String getName() const override { return "Quell"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 1.0; }

    //==============================================================================
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==============================================================================
    /** Copies the most recent analyser frame for the editor. Audio-thread safe. */
    bool readDisplay (quell::DisplaySnapshot& out) const noexcept;

    int getProcessingLatencySamples() const noexcept { return suppressor.getLatencySamples(); }

    /** A/B comparison slots. */
    void storeSlot (int slot);
    void copyCurrentToSlot (int slot);
    void recallSlot (int slot);
    int getActiveSlot() const noexcept { return activeSlot; }

    std::atomic<float> inputPeak  { 0.0f };
    std::atomic<float> outputPeak { 0.0f };
    std::atomic<float> maxCutDb   { 0.0f };

    juce::AudioProcessorValueTreeState apvts;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    struct DisplayStore
    {
        quell::DisplaySnapshot buffers[2];
        std::atomic<int> front { 0 };
    };

    quell::SpectralSuppressor suppressor;
    DisplayStore displayStore;

    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> outputGainSm;

    juce::AudioProcessorParameter* bypassParameter = nullptr;

    juce::ValueTree slotA, slotB;
    int activeSlot = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (QuellAudioProcessor)
};
