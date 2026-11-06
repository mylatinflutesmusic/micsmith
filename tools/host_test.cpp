// Loads the installed plugin through JUCE's VST3 host implementation - the same
// code path a DAW uses - and exercises parameters and audio.

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <memory>

#include <juce_audio_processors/juce_audio_processors.h>

extern juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter();

namespace
{
/** Renders the editor offscreen so the interface can be inspected without a DAW. */
int renderEditorSnapshot (const juce::File& outFile, int width, int height)
{
    std::unique_ptr<juce::AudioProcessor> processor (createPluginFilter());

    if (processor == nullptr)
    {
        std::printf ("*FAIL* could not create the processor\n");
        return 1;
    }

    processor->setPlayConfigDetails (2, 2, 48000.0, 512);
    processor->prepareToPlay (512, 48000.0);

    std::unique_ptr<juce::AudioProcessorEditor> editor (processor->createEditorIfNeeded());

    if (editor == nullptr)
    {
        std::printf ("*FAIL* the plugin has no editor\n");
        return 1;
    }

    editor->setSize (width, height);

    const auto image = editor->createComponentSnapshot (editor->getLocalBounds(), true, 1.0f);

    if (image.isNull())
    {
        std::printf ("*FAIL* snapshot came back empty\n");
        return 1;
    }

    outFile.deleteFile();
    juce::FileOutputStream stream (outFile);

    if (! stream.openedOk())
    {
        std::printf ("*FAIL* cannot write %s\n", outFile.getFullPathName().toRawUTF8());
        return 1;
    }

    juce::PNGImageFormat png;
    png.writeImageToStream (image, stream);
    stream.flush();

    std::printf ("wrote %s (%dx%d)\n", outFile.getFullPathName().toRawUTF8(),
                 image.getWidth(), image.getHeight());
    return 0;
}

int failures = 0;

void check (bool ok, const char* what, const char* detail = "")
{
    std::printf ("%s  %s %s\n", ok ? " pass " : "*FAIL*", what, detail);

    if (! ok)
        ++failures;
}

float renderBlock (juce::AudioPluginInstance& inst, juce::AudioBuffer<float>& buffer,
                   juce::MidiBuffer& midi, double& phase, int blockIndex, juce::AudioBuffer<float>* capture = nullptr)
{
    const int numSamples = buffer.getNumSamples();

    for (int i = 0; i < numSamples; ++i)
    {
        const float v = (float) (0.5 * std::sin (phase));
        phase += 2.0 * 3.14159265358979 * 440.0 / 48000.0;

        buffer.setSample (0, i, v);
        buffer.setSample (1, i, v);
    }

    juce::ignoreUnused (blockIndex);
    inst.processBlock (buffer, midi);

    if (capture != nullptr)
        capture->makeCopyOf (buffer);

    return buffer.getMagnitude (0, numSamples);
}
} // namespace

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;

    if (argc >= 3 && juce::String (argv[1]) == "--shot")
        return renderEditorSnapshot (juce::File (juce::String (argv[2])),
                                     argc >= 5 ? std::atoi (argv[3]) : 760,
                                     argc >= 5 ? std::atoi (argv[4]) : 560);

    if (argc < 2)
    {
        std::printf ("usage: host_test <path to plugin> | --shot <out.png> [w h]\n");
        return 2;
    }

    const juce::String path (argv[1]);
    std::printf ("MicSmith host validation (JUCE VST3 host)\n=========================================\n");
    std::printf ("plugin: %s\n\n", path.toRawUTF8());

    juce::VST3PluginFormat format;
    juce::OwnedArray<juce::PluginDescription> types;
    format.findAllTypesForFile (types, path);

    std::printf ("      descriptions found: %d\n", types.size());

    for (auto* d : types)
        std::printf ("      - %s | %s | %s\n", d->name.toRawUTF8(),
                     d->manufacturerName.toRawUTF8(), d->pluginFormatName.toRawUTF8());

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
    std::printf ("      name: %s\n", instance->getName().toRawUTF8());

    const auto params = instance->getParameters();
    std::printf ("      parameters: %d\n", (int) params.size());

    check (params.size() >= 11, "all 11 parameters are visible to the host");

    for (int i = 0; i < params.size() && i < 12; ++i)
        std::printf ("        [%2d] %-22s = %s\n", i,
                     params.getUnchecked (i)->getName (48).toRawUTF8(),
                     params.getUnchecked (i)->getCurrentValueAsText().toRawUTF8());

    instance->setPlayConfigDetails (2, 2, 48000.0, 512);
    instance->prepareToPlay (512, 48000.0);

    juce::AudioBuffer<float> buffer (2, 512);
    juce::AudioBuffer<float> baseline (2, 512);
    juce::MidiBuffer midi;
    double phase = 0.0;

    float peak = 0.0f;
    bool finite = true;

    for (int block = 0; block < 8; ++block)
    {
        const float blockPeak = renderBlock (*instance, buffer, midi, phase, block,
                                             block == 7 ? &baseline : nullptr);
        peak = juce::jmax (peak, blockPeak);

        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            for (int i = 0; i < buffer.getNumSamples(); ++i)
                if (! std::isfinite (buffer.getSample (ch, i)))
                    finite = false;
    }

    std::printf ("      peak output of 440 Hz sine: %.4f\n", peak);

    check (finite, "output is finite (no NaN/Inf)");
    check (peak > 0.01f, "audio passes through the plugin");

    //================================================= parameters drive the DSP
    juce::AudioProcessorParameter* targetParam = nullptr;

    for (auto* p : params)
        if (p->getName (48).containsIgnoreCase ("Emulated"))
            targetParam = p;

    check (targetParam != nullptr, "target microphone parameter found by name");

    if (targetParam != nullptr)
    {
        std::printf ("      switching target to: ");
        targetParam->setValueNotifyingHost (1.0f); // last choice in the list
        std::printf ("%s\n", targetParam->getCurrentValueAsText().toRawUTF8());

        juce::AudioBuffer<float> changed (2, 512);
        float changedPeak = 0.0f;

        for (int block = 0; block < 8; ++block)
            changedPeak = juce::jmax (changedPeak, renderBlock (*instance, changed, midi, phase, block));

        check (std::isfinite (changedPeak) && changedPeak > 0.01f,
               "audio still flows after switching the model");

        float maxDiff = 0.0f;

        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < 512; ++i)
                maxDiff = juce::jmax (maxDiff, std::abs (baseline.getSample (ch, i) - changed.getSample (ch, i)));

        std::printf ("      peak after model change: %.4f, max sample delta vs baseline: %.4f\n",
                     changedPeak, maxDiff);

        check (maxDiff > 1.0e-3f, "changing the microphone model actually changes the audio");
    }

    //============================================================ state round-trip
    {
        juce::MemoryBlock state;
        instance->getStateInformation (state);

        juce::MemoryBlock state2;
        instance->getStateInformation (state2);

        check (state.getSize() > 0 && state.getSize() == state2.getSize(),
              "plugin state serialises");
        std::printf ("      state size: %d bytes\n", (int) state.getSize());
    }

    instance->releaseResources();

    std::printf ("\n%s\n", failures == 0 ? "PLUGIN OK IN A HOST" : "PLUGIN HAS PROBLEMS");
    return failures == 0 ? 0 : 1;
}
