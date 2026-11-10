#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <cmath>
#include <cstdio>
#include <functional>
#include <memory>

namespace
{
int failures = 0;

void check (bool condition, const char* message)
{
    std::printf (" %s   %s\n", condition ? "pass" : "FAIL", message);

    if (! condition)
        ++failures;
}

bool allVisibleChildrenInside (juce::Component& parent)
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
}
} // namespace

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;

    if (argc < 2)
    {
        std::printf ("usage: KChannelVST2HostTest <K Channel.dll>\n");
        return 2;
    }

    const juce::File pluginFile { juce::String (argv[1]) };

    std::printf ("K Channel VST2 host validation\n");
    std::printf ("================================\n");
    std::printf ("plugin: %s\n\n", pluginFile.getFullPathName().toRawUTF8());

    juce::VSTPluginFormat vst;
    juce::OwnedArray<juce::PluginDescription> types;
    vst.findAllTypesForFile (types, pluginFile.getFullPathName());

    check (types.size() == 1, "VST2 host discovers the plugin");
    if (types.isEmpty())
        return 1;

    std::printf ("      name: %s\n", types[0]->name.toRawUTF8());
    std::printf ("      manufacturer: %s\n", types[0]->manufacturerName.toRawUTF8());

    juce::AudioPluginFormatManager formats;
    formats.addDefaultFormats();

    juce::String error;
    auto instance = formats.createPluginInstance (*types[0], 48000.0, 512, error);

    check (instance != nullptr, "VST2 host instantiates the plugin");
    if (instance == nullptr)
    {
        std::printf ("      error: %s\n", error.toRawUTF8());
        return 1;
    }

    check (instance->getParameters().size() == 37,
           "all 37 channel parameters are visible to a VST2 host");
    check (instance->getTotalNumInputChannels() == 2
               && instance->getTotalNumOutputChannels() == 2,
           "the VST2 plugin exposes stereo input and output");

    instance->prepareToPlay (48000.0, 512);

    std::printf ("      latency: %d samples\n", instance->getLatencySamples());
    check (instance->getLatencySamples() == 1024,
           "Quell's 1024-sample latency is reported");

    juce::AudioBuffer<float> buffer (2, 512);
    bool finite = true;

    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        const auto sample = 0.1f * std::sin (juce::MathConstants<float>::twoPi * 440.0f
                                             * (float) i / 48000.0f);
        buffer.setSample (0, i, sample);
        buffer.setSample (1, i, sample);
    }

    juce::MidiBuffer midi;
    instance->processBlock (buffer, midi);

    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        for (int i = 0; i < buffer.getNumSamples(); ++i)
            finite = finite && std::isfinite (buffer.getSample (ch, i));

    check (finite, "VST2 audio processing produces finite samples");

    std::unique_ptr<juce::AudioProcessorEditor> editor (instance->createEditorIfNeeded());

    check (editor != nullptr, "the VST2 editor can be created");
    if (editor != nullptr)
    {
        std::printf ("      editor: %d x %d\n", editor->getWidth(), editor->getHeight());
        check (editor->getWidth() >= 900 && editor->getHeight() >= 540,
               "the VST2 editor reports a usable size");

        editor->setSize (900, 540);
        check (editor->getWidth() == 900 && editor->getHeight() == 540,
               "the VST2 editor resizes down to 900x540");
        check (allVisibleChildrenInside (*editor),
               "all visible controls stay inside the minimum-size VST2 editor");

        instance->editorBeingDeleted (editor.get());
        editor.reset();
    }

    instance->releaseResources();

    std::printf ("\n%s\n", failures == 0 ? "ALL CHECKS PASSED" : "FAILURES PRESENT");
    return failures == 0 ? 0 : 1;
}
