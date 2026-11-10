#pragma once

#include <atomic>

#include <juce_audio_processors/juce_audio_processors.h>

#include "dsp/ChannelDsp.h"
#include "dsp/SpectralSuppressor.h"

class KChannelAudioProcessor : public juce::AudioProcessor
{
public:
    KChannelAudioProcessor();
    ~KChannelAudioProcessor() override;

    //==========================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void reset() override;

    //==========================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    juce::AudioProcessorParameter* getBypassParameter() const override;

    //==========================================================================
    const juce::String getName() const override { return "K Channel"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 1.0; }

    //==========================================================================
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    //==========================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==========================================================================
    /** Parameters for the UI's response curve. Safe to call on the message thread. */
    kchannel::EqSettings getEqSettings() const noexcept;
    bool isQuellPost() const noexcept;

    int getProcessingLatencySamples() const noexcept { return quell.getLatencySamples(); }

    std::atomic<float> inputPeak  { 0.0f };
    std::atomic<float> outputPeak { 0.0f };
    std::atomic<float> compGrDb   { 0.0f };
    std::atomic<float> gateGrDb   { 0.0f };
    std::atomic<float> quellCutDb { 0.0f };

    juce::AudioProcessorValueTreeState apvts;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    float read (const char* id) const noexcept;
    quell::SuppressorSettings readQuellSettings() const noexcept;
    kchannel::CompressorSettings readCompressorSettings() const noexcept;
    kchannel::GateSettings readGateSettings() const noexcept;

    quell::SpectralSuppressor quell;
    kchannel::Equalizer equalizer;
    kchannel::Compressor compressor;
    kchannel::Gate gate;

    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> inputGainSm, outputGainSm;

    juce::AudioProcessorParameter* bypassParameter = nullptr;
    double currentSampleRate = 48000.0;
    int activeChannels = 2;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KChannelAudioProcessor)
};
