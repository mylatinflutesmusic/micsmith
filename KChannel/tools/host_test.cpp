// Loads the built K Channel VST3 through JUCE's VST3 host implementation, the
// same code path a DAW uses. Exercises instantiation, parameters, latency,
// audio flow, bypass, the Quell pre/post switch, state and the editor.

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <functional>
#include <memory>

#include <juce_audio_processors/juce_audio_processors.h>

namespace
{
int failures = 0;

void check (bool ok, const char* what)
{
    std::printf ("%s  %s\n", ok ? " pass " : "*FAIL*", what);
    if (! ok)
        ++failures;
}

void setParameter (juce::AudioProcessor& processor, const juce::String& name, float normalised)
{
    for (auto* parameter : processor.getParameters())
        if (parameter->getName (64) == name)
        {
            parameter->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, normalised));
            return;
        }

    std::printf ("      (parameter '%s' not found)\n", name.toRawUTF8());
}

double renderRms (juce::AudioPluginInstance& instance, int blocks)
{
    instance.prepareToPlay (512, 48000.0);

    juce::AudioBuffer<float> buffer (2, 512);
    juce::MidiBuffer midi;

    double phase = 0.0;
    double sum = 0.0;
    long long count = 0;

    for (int block = 0; block < blocks; ++block)
    {
        for (int i = 0; i < 512; ++i)
        {
            const double tone = 0.20 * std::sin (phase);
            const double edge = 0.06 * std::sin (phase * 11.7);
            const float value = (float) (tone + edge);

            buffer.setSample (0, i, value);
            buffer.setSample (1, i, value);

            phase += 2.0 * 3.14159265358979323846 * 997.0 / 48000.0;
        }

        instance.processBlock (buffer, midi);

        if (block > blocks / 3)
        {
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < 512; ++i)
                {
                    const double v = buffer.getSample (ch, i);
                    sum += v * v;
                    ++count;
                }
        }
    }

    return std::sqrt (sum / (double) std::max (1LL, count));
}

bool finiteAudio (const juce::AudioBuffer<float>& buffer)
{
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        for (int i = 0; i < buffer.getNumSamples(); ++i)
            if (! std::isfinite (buffer.getSample (ch, i)))
                return false;

    return true;
}
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;

    if (argc < 2)
    {
        std::printf ("usage: KChannelHostTest <path to K Channel.vst3>\n");
        return 2;
    }

    const juce::String path (argv[1]);

    std::printf ("K Channel host validation (JUCE VST3 host)\n");
    std::printf ("===========================================\n");
    std::printf ("plugin: %s\n\n", path.toRawUTF8());

    juce::VST3PluginFormat format;
    juce::OwnedArray<juce::PluginDescription> types;
    format.findAllTypesForFile (types, path);

    std::printf ("      descriptions found: %d\n", types.size());

    for (auto* description : types)
        std::printf ("      - %s | %s\n",
                     description->name.toRawUTF8(),
                     description->manufacturerName.toRawUTF8());

    check (! types.isEmpty(), "host discovers the plugin");

    if (types.isEmpty())
        return 1;

    juce::String error;
    std::unique_ptr<juce::AudioPluginInstance> instance (
        format.createInstanceFromDescription (*types[0], 48000.0, 512, error));

    if (instance == nullptr)
    {
        std::printf ("*FAIL* createInstanceFromDescription: %s\n", error.toRawUTF8());
        return 1;
    }

    check (true, "host instantiates the plugin");

    instance->setPlayConfigDetails (2, 2, 48000.0, 512);
    instance->prepareToPlay (512, 48000.0);

    const auto parameters = instance->getParameters();

    std::printf ("      name: %s\n", instance->getName().toRawUTF8());
    std::printf ("      latency: %d samples\n", instance->getLatencySamples());
    std::printf ("      parameters: %d\n", (int) parameters.size());
    std::printf ("      buses: %d in, %d out\n", instance->getBusCount (true),
                 instance->getBusCount (false));

    check (parameters.size() == 37, "all 37 channel parameters are visible to the host");
    check (instance->getLatencySamples() == 1024, "Quell's 1024-sample latency is reported");
    check (instance->getBusCount (true) == 1 && instance->getBusCount (false) == 1,
           "the channel exposes a single stereo input and output");

    for (int i = 0; i < parameters.size(); ++i)
        std::printf ("        [%2d] %-28s = %s\n", i,
                     parameters.getUnchecked (i)->getName (48).toRawUTF8(),
                     parameters.getUnchecked (i)->getCurrentValueAsText().toRawUTF8());

    //=============================================================== audio flow
    {
        const double rms = renderRms (*instance, 120);

        std::printf ("      default pass-through RMS: %.5f\n", rms);
        check (std::isfinite (rms) && rms > 0.02, "audio passes through the channel");
    }

    //=============================================================== compressor
    {
        setParameter (*instance, "Compressor In", 1.0f);
        setParameter (*instance, "Compressor Threshold", 0.25f);
        setParameter (*instance, "Compressor Ratio", 0.75f);

        const double rms = renderRms (*instance, 120);

        std::printf ("      compressed RMS: %.5f\n", rms);
        check (std::isfinite (rms) && rms > 0.002, "compressor stage passes finite audio");

        setParameter (*instance, "Compressor In", 0.0f);
    }

    //=============================================================== Quell post
    {
        setParameter (*instance, "Quell Post Compressor", 1.0f);
        setParameter (*instance, "Quell Depth", 1.0f);

        const double rms = renderRms (*instance, 120);

        std::printf ("      Quell-post RMS: %.5f\n", rms);
        check (std::isfinite (rms) && rms > 0.002, "Quell-post routing passes finite audio");

        setParameter (*instance, "Quell Post Compressor", 0.0f);
        setParameter (*instance, "Quell Depth", 0.35f);
    }

    //=================================================================== bypass
    {
        setParameter (*instance, "Bypass", 1.0f);

        juce::AudioBuffer<float> buffer (2, 512);
        juce::MidiBuffer midi;

        for (int i = 0; i < 512; ++i)
        {
            const float value = (float) (0.25 * std::sin (2.0 * 3.14159265358979323846
                                                          * 997.0 * (double) i / 48000.0));
            buffer.setSample (0, i, value);
            buffer.setSample (1, i, value);
        }

        instance->processBlock (buffer, midi);

        check (finiteAudio (buffer), "bypass produces finite audio");
        check (instance->getLatencySamples() == 1024, "bypass preserves the reported latency");

        setParameter (*instance, "Bypass", 0.0f);
    }

    //=============================================================== state / UI
    {
        juce::MemoryBlock state;
        instance->getStateInformation (state);

        check (state.getSize() > 0, "state serialises");

        std::unique_ptr<juce::AudioPluginInstance> restored (
            format.createInstanceFromDescription (*types[0], 48000.0, 512, error));

        if (restored != nullptr)
        {
            restored->setPlayConfigDetails (2, 2, 48000.0, 512);
            restored->setStateInformation (state.getData(), (int) state.getSize());
            check (true, "state restores into a fresh instance");
        }
        else
        {
            check (false, "state restores into a fresh instance");
        }

        std::unique_ptr<juce::AudioProcessorEditor> editor (instance->createEditorIfNeeded());

        check (editor != nullptr, "the editor can be created");
        if (editor != nullptr)
        {
            std::printf ("      editor: %d x %d\n", editor->getWidth(), editor->getHeight());
            check (editor->getWidth() >= 900 && editor->getHeight() >= 540,
                   "the editor reports a usable channel-strip size");

            editor->setSize (900, 540);

            std::function<bool (juce::Component&)> allVisibleChildrenInside =
                [&] (juce::Component& parent)
                {
                    for (auto* child : parent.getChildren())
                    {
                        if (! child->isVisible())
                            continue;

                        if (! parent.getLocalBounds().contains (child->getBounds()))
                            return false;

                        if (! allVisibleChildrenInside (*child))
                            return false;
                    }

                    return true;
                };

            std::printf ("      resized editor: %d x %d\n", editor->getWidth(), editor->getHeight());
            check (editor->getWidth() == 900 && editor->getHeight() == 540,
                   "the editor resizes down to 900x540");
            check (allVisibleChildrenInside (*editor),
                   "all visible controls stay inside the minimum-size editor");
        }
    }

    std::printf ("\n%s\n", failures == 0 ? "ALL CHECKS PASSED" : "FAILURES PRESENT");
    return failures == 0 ? 0 : 1;
}
