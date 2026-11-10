#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    juce::NormalisableRange<float> frequencyRange (float lo, float hi)
    {
        return juce::NormalisableRange<float> (lo, hi, 1.0f, 0.35f);
    }

    juce::NormalisableRange<float> logRange (float lo, float hi, float interval)
    {
        return juce::NormalisableRange<float> (lo, hi, interval, 0.45f);
    }

    juce::NormalisableRange<float> dbRange()
    {
        return juce::NormalisableRange<float> (-15.0f, 15.0f, 0.1f);
    }
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout KChannelAudioProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    auto addFloat = [&layout] (const char* id, const char* name,
                               juce::NormalisableRange<float> range, float defaultValue)
    {
        layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { id, 1 },
                                                                 name, range, defaultValue));
    };

    auto addBool = [&layout] (const char* id, const char* name, bool defaultValue)
    {
        layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { id, 1 },
                                                                name, defaultValue));
    };

    // Master / routing.
    addFloat ("input",  "Input",  juce::NormalisableRange<float> (-24.0f, 24.0f, 0.1f), 0.0f);
    addFloat ("output", "Output", juce::NormalisableRange<float> (-24.0f, 24.0f, 0.1f), 0.0f);
    addBool  ("phase",  "Phase Invert", false);
    addBool  ("bypass", "Bypass", false);

    // Filters.
    addBool  ("filtersOn", "Filters In", false);
    addFloat ("hpf", "HPF", frequencyRange (20.0f, 500.0f), 80.0f);
    addFloat ("lpf", "LPF", frequencyRange (3000.0f, 20000.0f), 20000.0f);

    // Four-band equalizer.
    addBool  ("eqOn", "EQ In", true);
    addFloat ("lfFreq", "LF Frequency", frequencyRange (30.0f, 450.0f), 80.0f);
    addFloat ("lfGain", "LF Gain", dbRange(), 0.0f);
    addFloat ("lmFreq", "LM Frequency", frequencyRange (80.0f, 1500.0f), 350.0f);
    addFloat ("lmGain", "LM Gain", dbRange(), 0.0f);
    addFloat ("lmQ",    "LM Q", logRange (0.3f, 4.0f, 0.01f), 0.70f);
    addFloat ("hmFreq", "HM Frequency", frequencyRange (1000.0f, 12000.0f), 3000.0f);
    addFloat ("hmGain", "HM Gain", dbRange(), 0.0f);
    addFloat ("hmQ",    "HM Q", logRange (0.3f, 4.0f, 0.01f), 0.70f);
    addFloat ("hfFreq", "HF Frequency", frequencyRange (2500.0f, 18000.0f), 10000.0f);
    addFloat ("hfGain", "HF Gain", dbRange(), 0.0f);

    // Compressor.
    addBool  ("compOn", "Compressor In", false);
    addFloat ("compThreshold", "Compressor Threshold",
              juce::NormalisableRange<float> (-48.0f, 0.0f, 0.1f), -18.0f);
    addFloat ("compRatio", "Compressor Ratio", logRange (1.0f, 20.0f, 0.1f), 2.5f);
    addFloat ("compAttack", "Compressor Attack", logRange (0.1f, 100.0f, 0.1f), 10.0f);
    addFloat ("compRelease", "Compressor Release", logRange (20.0f, 2000.0f, 1.0f), 200.0f);
    addFloat ("compMakeup", "Compressor Makeup",
              juce::NormalisableRange<float> (0.0f, 24.0f, 0.1f), 0.0f);

    // Gate.
    addBool  ("gateOn", "Gate In", false);
    addFloat ("gateThreshold", "Gate Threshold",
              juce::NormalisableRange<float> (-70.0f, 0.0f, 0.1f), -45.0f);
    addFloat ("gateRange", "Gate Range",
              juce::NormalisableRange<float> (0.0f, 60.0f, 0.1f), 30.0f);
    addFloat ("gateAttack", "Gate Attack", logRange (0.1f, 50.0f, 0.1f), 1.0f);
    addFloat ("gateRelease", "Gate Release", logRange (20.0f, 1000.0f, 1.0f), 150.0f);

    // Quell. These map directly onto the Quell engine's 0..1 controls.
    addBool  ("quellOn",  "Quell In", true);
    addBool  ("quellPost", "Quell Post Compressor", false);
    addFloat ("qDepth", "Quell Depth", juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 35.0f);
    addFloat ("qSensitivity", "Quell Sensitivity",
              juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 40.0f);
    addFloat ("qSelectivity", "Quell Selectivity",
              juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 55.0f);
    addFloat ("qSpeed", "Quell Speed", juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 50.0f);
    addFloat ("qLowTame", "Quell Low Tame",
              juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 10.0f);
    addFloat ("qHighTame", "Quell High Tame",
              juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 15.0f);

    return layout;
}

//==============================================================================
KChannelAudioProcessor::KChannelAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
    bypassParameter = apvts.getParameter ("bypass");
}

KChannelAudioProcessor::~KChannelAudioProcessor() = default;

//==============================================================================
void KChannelAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused (samplesPerBlock);

    currentSampleRate = sampleRate > 0.0 ? sampleRate : 48000.0;
    activeChannels = juce::jlimit (1, 2, getChannelCountOfBus (true, 0));

    equalizer.prepare (currentSampleRate, activeChannels);
    compressor.prepare (currentSampleRate);
    gate.prepare (currentSampleRate);
    quell.prepare (currentSampleRate, activeChannels);
    setLatencySamples (quell.getLatencySamples());

    inputGainSm.reset (currentSampleRate, 0.02);
    outputGainSm.reset (currentSampleRate, 0.02);

    inputGainSm.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (read ("input")));
    outputGainSm.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (read ("output")));
}

void KChannelAudioProcessor::releaseResources()
{
}

void KChannelAudioProcessor::reset()
{
    equalizer.reset();
    compressor.reset();
    gate.reset();
    quell.reset();

    inputGainSm.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (read ("input")));
    outputGainSm.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (read ("output")));
}

bool KChannelAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in  = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();

    if (in != out)
        return false;

    return in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo();
}

//==============================================================================
float KChannelAudioProcessor::read (const char* id) const noexcept
{
    if (auto* value = apvts.getRawParameterValue (id))
        return value->load();

    return 0.0f;
}

kchannel::EqSettings KChannelAudioProcessor::getEqSettings() const noexcept
{
    kchannel::EqSettings s;

    s.filtersOn = read ("filtersOn") > 0.5f;
    s.hpfHz     = read ("hpf");
    s.lpfHz     = read ("lpf");

    s.eqOn      = read ("eqOn") > 0.5f;
    s.lfFreq    = read ("lfFreq");
    s.lfGain    = read ("lfGain");
    s.lmFreq    = read ("lmFreq");
    s.lmGain    = read ("lmGain");
    s.lmQ       = read ("lmQ");
    s.hmFreq    = read ("hmFreq");
    s.hmGain    = read ("hmGain");
    s.hmQ       = read ("hmQ");
    s.hfFreq    = read ("hfFreq");
    s.hfGain    = read ("hfGain");

    return s;
}

quell::SuppressorSettings KChannelAudioProcessor::readQuellSettings() const noexcept
{
    quell::SuppressorSettings s;

    s.depth       = read ("qDepth") * 0.01f;
    s.sensitivity = read ("qSensitivity") * 0.01f;
    s.selectivity = read ("qSelectivity") * 0.01f;
    s.speed       = read ("qSpeed") * 0.01f;
    s.lowTame     = read ("qLowTame") * 0.01f;
    s.highTame    = read ("qHighTame") * 0.01f;
    s.link        = true;
    s.midSide     = false;

    return s;
}

kchannel::CompressorSettings KChannelAudioProcessor::readCompressorSettings() const noexcept
{
    kchannel::CompressorSettings s;

    s.thresholdDb = read ("compThreshold");
    s.ratio       = read ("compRatio");
    s.attackMs    = read ("compAttack");
    s.releaseMs   = read ("compRelease");
    s.makeupDb    = read ("compMakeup");
    s.kneeDb      = 6.0f;

    return s;
}

kchannel::GateSettings KChannelAudioProcessor::readGateSettings() const noexcept
{
    kchannel::GateSettings s;

    s.thresholdDb = read ("gateThreshold");
    s.rangeDb     = read ("gateRange");
    s.attackMs    = read ("gateAttack");
    s.releaseMs   = read ("gateRelease");

    return s;
}

bool KChannelAudioProcessor::isQuellPost() const noexcept
{
    return read ("quellPost") > 0.5f;
}

//==============================================================================
juce::AudioProcessorParameter* KChannelAudioProcessor::getBypassParameter() const
{
    return bypassParameter;
}

void KChannelAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused (midiMessages);
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    const int totalIn = getTotalNumInputChannels();
    const int totalOut = getTotalNumOutputChannels();

    for (int ch = totalIn; ch < totalOut; ++ch)
        buffer.clear (ch, 0, numSamples);

    if (numSamples <= 0)
        return;

    const int numChannels = juce::jlimit (1, 2, totalIn);

    if (numChannels <= 0)
        return;

    float* channelPointers[2] { buffer.getWritePointer (0),
                                numChannels > 1 ? buffer.getWritePointer (1) : nullptr };

    float peak = 0.0f;

    for (int ch = 0; ch < numChannels; ++ch)
        peak = juce::jmax (peak, buffer.getMagnitude (ch, 0, numSamples));

    inputPeak.store (peak);

    const bool bypass = read ("bypass") > 0.5f;
    const bool quellOn = read ("quellOn") > 0.5f;
    const bool quellPost = read ("quellPost") > 0.5f;

    const auto quellSettings = readQuellSettings();

    // The suppressor is always run, even when bypassed, because its one-frame
    // delay is the plug-in's reported latency. Bypass mode therefore produces a
    // latency-compensated dry signal rather than a shorter path.
    if (bypass)
    {
        quell.setSettings (quellSettings);
        quell.process (channelPointers, numChannels, numSamples, 1.0f, false, true);

        compGrDb.store (0.0f);
        gateGrDb.store (0.0f);
        quellCutDb.store (0.0f);
        outputPeak.store (buffer.getMagnitude (0, numSamples));
        return;
    }

    //================================================================ input trim
    const float phaseSign = read ("phase") > 0.5f ? -1.0f : 1.0f;

    inputGainSm.setTargetValue (juce::Decibels::decibelsToGain (read ("input")));
    outputGainSm.setTargetValue (juce::Decibels::decibelsToGain (read ("output")));

    for (int i = 0; i < numSamples; ++i)
    {
        const float gain = inputGainSm.getNextValue() * phaseSign;

        for (int ch = 0; ch < numChannels; ++ch)
            channelPointers[ch][i] *= gain;
    }

    //================================================================ filters/EQ
    equalizer.setSettings (getEqSettings());
    equalizer.process (channelPointers, numChannels, numSamples);

    //============================================================= Quell (pre)
    if (! quellPost)
    {
        quell.setSettings (quellSettings);
        quell.process (channelPointers, numChannels, numSamples, 1.0f, false, ! quellOn);
    }

    //==================================================================== gate
    if (read ("gateOn") > 0.5f)
    {
        gate.setSettings (readGateSettings());
        gate.process (channelPointers, numChannels, numSamples);
        gateGrDb.store (gate.getGainReductionDb());
    }
    else
    {
        gate.reset();
        gateGrDb.store (0.0f);
    }

    //============================================================== compressor
    if (read ("compOn") > 0.5f)
    {
        compressor.setSettings (readCompressorSettings());
        compressor.process (channelPointers, numChannels, numSamples);
        compGrDb.store (compressor.getGainReductionDb());
    }
    else
    {
        compressor.reset();
        compGrDb.store (0.0f);
    }

    //============================================================ Quell (post)
    if (quellPost)
    {
        quell.setSettings (quellSettings);
        quell.process (channelPointers, numChannels, numSamples, 1.0f, false, ! quellOn);
    }

    quellCutDb.store (quellOn ? quell.getMaxCutDb() : 0.0f);

    //=============================================================== output trim
    for (int i = 0; i < numSamples; ++i)
    {
        const float gain = outputGainSm.getNextValue();

        for (int ch = 0; ch < numChannels; ++ch)
            channelPointers[ch][i] *= gain;
    }

    outputPeak.store (buffer.getMagnitude (0, numSamples));
}

//==============================================================================
void KChannelAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void KChannelAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

//==============================================================================
juce::AudioProcessorEditor* KChannelAudioProcessor::createEditor()
{
    return new KChannelAudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new KChannelAudioProcessor();
}
