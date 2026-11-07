#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    juce::NormalisableRange<float> percentRange()
    {
        return juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f);
    }
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout QuellAudioProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    auto addPercent = [&layout] (const char* id, const char* name, float defaultValue)
    {
        layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { id, 1 },
                                                                 name, percentRange(), defaultValue));
    };

    addPercent ("depth",       "Depth",       35.0f);
    addPercent ("selectivity", "Selectivity", 55.0f);
    addPercent ("sensitivity", "Sensitivity", 40.0f);
    addPercent ("speed",       "Speed",       50.0f);
    addPercent ("lowtame",     "Low Tame",     0.0f);
    addPercent ("hightame",    "High Tame",    0.0f);
    addPercent ("mix",         "Mix",        100.0f);

    layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "output", 1 },
                                                             "Output",
                                                             juce::NormalisableRange<float> (-24.0f, 24.0f, 0.1f),
                                                             0.0f));

    layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "link", 1 },
                                                            "Stereo Link", true));

    layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "delta", 1 },
                                                            "Delta", false));

    layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "bypass", 1 },
                                                            "Bypass", false));

    layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "msmode", 1 },
                                                            "Mid/Side", false));

    layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "sidechain", 1 },
                                                            "Sidechain", false));

    return layout;
}

//==============================================================================
QuellAudioProcessor::QuellAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withInput ("Sidechain", juce::AudioChannelSet::stereo(), false)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
    apvts.state.setProperty ("presetName", "Init", nullptr);

    bypassParameter = apvts.getParameter ("bypass");
    slotA = apvts.copyState();
    slotB = apvts.copyState();
}

QuellAudioProcessor::~QuellAudioProcessor() = default;

//==============================================================================
void QuellAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused (samplesPerBlock);

    const int mainChannels = juce::jlimit (1, 2, getChannelCountOfBus (true, 0));
    suppressor.prepare (sampleRate, mainChannels);
    setLatencySamples (suppressor.getLatencySamples());

    outputGainSm.reset (sampleRate > 0.0 ? sampleRate : 48000.0, 0.02);
    outputGainSm.setCurrentAndTargetValue (1.0f);

    displayStore.front.store (0);
    displayStore.buffers[0] = {};
    displayStore.buffers[1] = {};
}

void QuellAudioProcessor::releaseResources()
{
}

void QuellAudioProcessor::reset()
{
    suppressor.reset();

    if (auto* output = apvts.getRawParameterValue ("output"))
        outputGainSm.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (output->load()));
}

bool QuellAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in  = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();

    if (in != out)
        return false;

    if (in != juce::AudioChannelSet::mono() && in != juce::AudioChannelSet::stereo())
        return false;

    // The sidechain is optional, but when the host activates it, it must be
    // mono or stereo; mono is replicated across the detectors.
    if (layouts.inputBuses.size() > 1)
    {
        const auto sidechain = layouts.inputBuses[1];

        if (! sidechain.isDisabled()
            && sidechain != juce::AudioChannelSet::mono()
            && sidechain != juce::AudioChannelSet::stereo())
            return false;
    }

    return true;
}

//==============================================================================
juce::AudioProcessorParameter* QuellAudioProcessor::getBypassParameter() const
{
    // Nominating our own bypass parameter stops the VST3 wrapper creating a
    // second one, and routes the host's bypass control to the button in the UI.
    return bypassParameter;
}

bool QuellAudioProcessor::readDisplay (quell::DisplaySnapshot& out) const noexcept
{
    const auto index = displayStore.front.load (std::memory_order_acquire);
    out = displayStore.buffers[(size_t) juce::jlimit (0, 1, index)];
    return true;
}

//==============================================================================
void QuellAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused (midiMessages);
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();

    if (numSamples == 0)
        return;

    auto mainInput  = getBusBuffer (buffer, true, 0);
    auto mainOutput = getBusBuffer (buffer, false, 0);

    const int numChannels = juce::jlimit (1, 2, juce::jmin (mainOutput.getNumChannels(),
                                                          mainInput.getNumChannels()));

    inputPeak.store (mainInput.getMagnitude (0, numSamples));

    // Main output normally aliases main input in the process buffer. Copy only
    // if a host has given us distinct channel sets.
    for (int ch = 0; ch < numChannels; ++ch)
        if (mainOutput.getWritePointer (ch) != mainInput.getReadPointer (ch))
            mainOutput.copyFrom (ch, 0, mainInput, ch, 0, numSamples);

    if (auto* v = apvts.getRawParameterValue ("output"))
        outputGainSm.setTargetValue (juce::Decibels::decibelsToGain (v->load()));

    quell::SuppressorSettings settings;
    settings.depth       = apvts.getRawParameterValue ("depth")->load() * 0.01f;
    settings.sensitivity = apvts.getRawParameterValue ("sensitivity")->load() * 0.01f;
    settings.selectivity = apvts.getRawParameterValue ("selectivity")->load() * 0.01f;
    settings.speed       = apvts.getRawParameterValue ("speed")->load() * 0.01f;
    settings.lowTame     = apvts.getRawParameterValue ("lowtame")->load() * 0.01f;
    settings.highTame    = apvts.getRawParameterValue ("hightame")->load() * 0.01f;
    settings.link        = apvts.getRawParameterValue ("link")->load() > 0.5f;
    settings.midSide     = apvts.getRawParameterValue ("msmode")->load() > 0.5f;

    suppressor.setSettings (settings);

    const float mix = apvts.getRawParameterValue ("mix")->load() * 0.01f;
    const bool delta = apvts.getRawParameterValue ("delta")->load() > 0.5f;
    const bool bypass = apvts.getRawParameterValue ("bypass")->load() > 0.5f;

    float* channelPointers[2] { mainOutput.getWritePointer (0),
                                numChannels > 1 ? mainOutput.getWritePointer (1) : nullptr };

    int sidechainChannels = 0;
    float* sidechainPointers[2] { nullptr, nullptr };

    if (apvts.getRawParameterValue ("sidechain")->load() > 0.5f
        && getChannelCountOfBus (true, 1) > 0)
    {
        auto sidechain = getBusBuffer (buffer, true, 1);
        sidechainChannels = juce::jmin (2, sidechain.getNumChannels());

        for (int ch = 0; ch < sidechainChannels; ++ch)
            sidechainPointers[ch] = sidechain.getWritePointer (ch);
    }

    suppressor.process (channelPointers, numChannels, numSamples, mix, delta, bypass,
                        sidechainChannels > 0 ? sidechainPointers : nullptr, sidechainChannels);

    if (! outputGainSm.isSmoothing() && std::abs (outputGainSm.getTargetValue() - 1.0f) < 1.0e-6f)
    {
        // Unity gain is by far the common case; skip the per-sample work.
    }
    else
    {
        for (int i = 0; i < numSamples; ++i)
        {
            const float g = outputGainSm.getNextValue();

            for (int ch = 0; ch < numChannels; ++ch)
                mainOutput.getWritePointer (ch)[i] *= g;
        }
    }

    // Publish analyser data with a double buffer: the audio thread fills the
    // back buffer and then flips the front index.
    const int back = 1 - displayStore.front.load (std::memory_order_relaxed);
    suppressor.readDisplay (displayStore.buffers[back]);
    displayStore.front.store (back, std::memory_order_release);

    maxCutDb.store (suppressor.getMaxCutDb());
    outputPeak.store (mainOutput.getMagnitude (0, numSamples));
}

//==============================================================================
void QuellAudioProcessor::copyCurrentToSlot (int slot)
{
    if (slot == 0) slotA = apvts.copyState();
    else           slotB = apvts.copyState();
}

void QuellAudioProcessor::storeSlot (int slot)
{
    copyCurrentToSlot (slot);
    activeSlot = slot;
}

void QuellAudioProcessor::recallSlot (int slot)
{
    const auto& tree = (slot == 0) ? slotA : slotB;

    if (tree.isValid())
    {
        apvts.replaceState (tree.createCopy());
        activeSlot = slot;
    }
}

//==============================================================================
void QuellAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void QuellAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));

    slotA = apvts.copyState();
    slotB = apvts.copyState();
    activeSlot = 0;
}

//==============================================================================
juce::AudioProcessorEditor* QuellAudioProcessor::createEditor()
{
    return new QuellAudioProcessorEditor (*this);
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new QuellAudioProcessor();
}
