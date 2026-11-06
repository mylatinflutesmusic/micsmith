#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    juce::StringArray makeMicNames()
    {
        juce::StringArray names;

        for (const auto& m : micsmith::micModels())
            names.add (m.name);

        return names;
    }

    juce::NormalisableRange<float> percentRange()
    {
        return juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f);
    }

    float nextChunkValue (juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear>& sm, int chunk)
    {
        const float v = sm.getNextValue();
        sm.skip (juce::jmax (0, chunk - 1));
        return v;
    }
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout MicSmithAudioProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    const auto names = makeMicNames();

    layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "source", 1 },
                                                              "Recorded With",
                                                              names, 0));

    layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "target", 1 },
                                                              "Emulated Microphone",
                                                              names, 1));

    layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "proximity", 1 },
                                                             "Proximity",
                                                             percentRange(), 25.0f));

    layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "axis", 1 },
                                                             "Off-Axis",
                                                             percentRange(), 0.0f));

    layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "lowcut", 1 },
                                                             "Low Cut",
                                                             juce::NormalisableRange<float> (20.0f, 400.0f, 1.0f, 0.35f), 20.0f));

    layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "highcut", 1 },
                                                             "High Cut",
                                                             juce::NormalisableRange<float> (2000.0f, 20000.0f, 1.0f, 0.5f), 20000.0f));

    layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "mix", 1 },
                                                             "Mix",
                                                             percentRange(), 100.0f));

    layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "input", 1 },
                                                             "Input",
                                                             juce::NormalisableRange<float> (-24.0f, 24.0f, 0.1f), 0.0f));

    layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "output", 1 },
                                                             "Output",
                                                             juce::NormalisableRange<float> (-24.0f, 24.0f, 0.1f), 0.0f));

    layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "phase", 1 },
                                                            "Phase Invert", false));

    layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "bypass", 1 },
                                                            "Bypass", false));

    return layout;
}

//==============================================================================
MicSmithAudioProcessor::MicSmithAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
    bypassParam = apvts.getParameter ("bypass");
    slotA = apvts.copyState();
    slotB = apvts.copyState();
}

MicSmithAudioProcessor::~MicSmithAudioProcessor() = default;

//==============================================================================
void MicSmithAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused (samplesPerBlock);

    currentSampleRate = sampleRate > 0.0 ? sampleRate : 48000.0;

    const double smoothTime = 0.02; // 20 ms

    inputGainSm.reset (currentSampleRate, smoothTime);
    outputGainSm.reset (currentSampleRate, smoothTime);
    mixSm.reset (currentSampleRate, smoothTime);
    autoGainSm.reset (currentSampleRate, smoothTime);
    proximitySm.reset (currentSampleRate, smoothTime);
    axisSm.reset (currentSampleRate, smoothTime);
    lowCutSm.reset (currentSampleRate, smoothTime);
    highCutSm.reset (currentSampleRate, smoothTime);

    inputGainSm.setCurrentAndTargetValue (1.0f);
    outputGainSm.setCurrentAndTargetValue (1.0f);
    mixSm.setCurrentAndTargetValue (1.0f);
    autoGainSm.setCurrentAndTargetValue (1.0f);
    proximitySm.setCurrentAndTargetValue (0.25f);
    axisSm.setCurrentAndTargetValue (0.0f);
    lowCutSm.setCurrentAndTargetValue (20.0f);
    highCutSm.setCurrentAndTargetValue (20000.0f);

    chainCount = 0;
    reset();
}

void MicSmithAudioProcessor::releaseResources()
{
}

void MicSmithAudioProcessor::reset()
{
    for (auto& ch : chains)
        for (auto& b : ch)
            b.reset();
}

bool MicSmithAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in  = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();

    if (in != out)
        return false;

    return in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo();
}

//==============================================================================
bool MicSmithAudioProcessor::isBypassed() const
{
    return bypassParam != nullptr && bypassParam->getValue() > 0.5f;
}

juce::AudioProcessorParameter* MicSmithAudioProcessor::getBypassParameter() const
{
    return bypassParam;
}

void MicSmithAudioProcessor::fillDisplayParams (micsmith::ModelParams& out) const
{
    auto read = [this] (const char* id)
    {
        if (auto* v = apvts.getRawParameterValue (id))
            return v->load();
        return 0.0f;
    };

    out.sourceIndex = (int) std::lround (read ("source"));
    out.targetIndex = (int) std::lround (read ("target"));
    out.proximity   = read ("proximity") * 0.01f;
    out.axis        = read ("axis") * 0.01f;
    out.lowCutHz    = read ("lowcut");
    out.highCutHz   = read ("highcut");
}

//==============================================================================
void MicSmithAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused (midiMessages);
    juce::ScopedNoDenormals noDenormals;

    const int totalIn  = getTotalNumInputChannels();
    const int totalOut = getTotalNumOutputChannels();
    const int numSamples = buffer.getNumSamples();

    for (int ch = totalIn; ch < totalOut; ++ch)
        buffer.clear (ch, 0, numSamples);

    if (numSamples == 0)
        return;

    inputPeak.store (buffer.getMagnitude (0, numSamples));

    if (isBypassed())
    {
        outputPeak.store (inputPeak.load());
        return;
    }

    auto* sourceValue  = apvts.getRawParameterValue ("source");
    auto* targetValue  = apvts.getRawParameterValue ("target");
    auto* proxValue    = apvts.getRawParameterValue ("proximity");
    auto* axisValue    = apvts.getRawParameterValue ("axis");
    auto* lowCutValue  = apvts.getRawParameterValue ("lowcut");
    auto* highCutValue = apvts.getRawParameterValue ("highcut");
    auto* mixValue     = apvts.getRawParameterValue ("mix");
    auto* inputValue   = apvts.getRawParameterValue ("input");
    auto* outputValue  = apvts.getRawParameterValue ("output");
    auto* phaseValue   = apvts.getRawParameterValue ("phase");

    const int   sourceIndex = (int) std::lround (sourceValue->load());
    const int   targetIndex = (int) std::lround (targetValue->load());
    const float mixAmount   = juce::jlimit (0.0f, 1.0f, mixValue->load() * 0.01f);
    const float phaseSign   = (phaseValue->load() > 0.5f) ? -1.0f : 1.0f;

    inputGainSm.setTargetValue (juce::Decibels::decibelsToGain (inputValue->load()));
    outputGainSm.setTargetValue (juce::Decibels::decibelsToGain (outputValue->load()));
    mixSm.setTargetValue (mixAmount);
    proximitySm.setTargetValue (juce::jlimit (0.0f, 1.0f, proxValue->load() * 0.01f));
    axisSm.setTargetValue (juce::jlimit (0.0f, 1.0f, axisValue->load() * 0.01f));
    lowCutSm.setTargetValue (lowCutValue->load());
    highCutSm.setTargetValue (highCutValue->load());

    const int numChannels = juce::jlimit (1, 2, totalIn);

    for (int pos = 0; pos < numSamples; )
    {
        const int chunk = juce::jmin (32, numSamples - pos);

        micsmith::ModelParams mp;
        mp.sourceIndex = sourceIndex;
        mp.targetIndex = targetIndex;
        mp.proximity   = nextChunkValue (proximitySm, chunk);
        mp.axis        = nextChunkValue (axisSm, chunk);
        mp.lowCutHz    = nextChunkValue (lowCutSm, chunk);
        mp.highCutHz   = nextChunkValue (highCutSm, chunk);

        micsmith::Band bands[micsmith::kMaxBands];
        const int count = micsmith::buildBands (mp, currentSampleRate, bands, micsmith::kMaxBands);

        micsmith::Coeffs coeffs[micsmith::kMaxBands];

        for (int i = 0; i < count; ++i)
            coeffs[i] = micsmith::makeCoeffs (bands[i], currentSampleRate);

        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < count; ++i)
                chains[ch][i].c = coeffs[i];

        chainCount = count;

        const double autoDb = micsmith::autoGainDb (mp, currentSampleRate);
        autoGainSm.setTargetValue (juce::Decibels::decibelsToGain ((float) autoDb));

        for (int i = 0; i < chunk; ++i)
        {
            const float inGain  = inputGainSm.getNextValue();
            const float outGain = outputGainSm.getNextValue() * autoGainSm.getNextValue();
            const float mix     = mixSm.getNextValue();
            const int   s       = pos + i;

            for (int ch = 0; ch < numChannels; ++ch)
            {
                float* d = buffer.getWritePointer (ch);

                const float dry = d[s] * inGain;
                float wet = dry;

                for (int b = 0; b < count; ++b)
                    wet = (float) chains[ch][b].process ((double) wet);

                d[s] = (dry + (wet - dry) * mix) * outGain * phaseSign;
            }
        }

        pos += chunk;
    }

    outputPeak.store (buffer.getMagnitude (0, numSamples));
}

//==============================================================================
void MicSmithAudioProcessor::copyCurrentToSlot (int slot)
{
    if (slot == 0) slotA = apvts.copyState();
    else           slotB = apvts.copyState();
}

void MicSmithAudioProcessor::storeSlot (int slot)
{
    copyCurrentToSlot (slot);
    activeSlot = slot;
}

void MicSmithAudioProcessor::recallSlot (int slot)
{
    const auto& tree = (slot == 0) ? slotA : slotB;

    if (tree.isValid())
    {
        apvts.replaceState (tree.createCopy());
        activeSlot = slot;
    }
}

//==============================================================================
void MicSmithAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void MicSmithAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));

    slotA = apvts.copyState();
    slotB = apvts.copyState();
    activeSlot = 0;
}

//==============================================================================
juce::AudioProcessorEditor* MicSmithAudioProcessor::createEditor()
{
    return new MicSmithAudioProcessorEditor (*this);
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MicSmithAudioProcessor();
}
