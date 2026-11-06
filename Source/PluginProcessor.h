#pragma once

#include <atomic>
#include <memory>

#include <juce_audio_processors/juce_audio_processors.h>

#include "dsp/ModelEngine.h"

class MicSmithAudioProcessor : public juce::AudioProcessor
{
public:
    MicSmithAudioProcessor();
    ~MicSmithAudioProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void reset() override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    //==============================================================================
    const juce::String getName() const override { return "MicSmith"; }
    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

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
    juce::AudioProcessorParameter* getBypassParameter() const override;

    //==============================================================================
    /** Slot management for the A/B comparison buttons. */
    void storeSlot (int slot);
    void copyCurrentToSlot (int slot);
    void recallSlot (int slot);
    int  getActiveSlot() const noexcept { return activeSlot; }

    /** Current smoothed state, safe to read from the message thread for display. */
    void fillDisplayParams (micsmith::ModelParams& out) const;

    std::atomic<float> inputPeak  { 0.0f };
    std::atomic<float> outputPeak { 0.0f };

    juce::AudioProcessorValueTreeState apvts;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    bool isBypassed() const;

    juce::AudioProcessorParameter* bypassParam = nullptr;

    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> inputGainSm, outputGainSm,
                                                                  mixSm, autoGainSm,
                                                                  proximitySm, axisSm,
                                                                  lowCutSm, highCutSm;

    micsmith::Biquad chains[2][micsmith::kMaxBands];
    int chainCount = 0;

    juce::ValueTree slotA, slotB;
    int activeSlot = 0;

    double currentSampleRate = 48000.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MicSmithAudioProcessor)
};
