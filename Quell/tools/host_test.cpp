// Loads the built plugin through JUCE's VST3 host implementation (the same code
// path a DAW uses), exercises audio and state, and can render the editor to PNG.

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <vector>

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_audio_processors/juce_audio_processors.h>

#include "PluginEditor.h"
#include "PluginProcessor.h"
#include "PresetLibrary.h"
#include "dsp/Fft.h"

namespace
{
int failures = 0;

void check (bool ok, const char* what, const char* detail = "")
{
    std::printf ("%s  %s %s\n", ok ? " pass " : "*FAIL*", what, detail);

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

bool canCreateEditor (juce::AudioProcessor& processor, int& width, int& height)
{
    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditorIfNeeded());

    if (editor == nullptr)
        return false;

    width = editor->getWidth();
    height = editor->getHeight();
    return true;
}

int renderEditorSnapshot (const juce::File& outFile, int width, int height, bool showBrowser)
{
    QuellAudioProcessor processor;
    processor.setPlayConfigDetails (2, 2, 48000.0, 512);
    processor.prepareToPlay (512, 48000.0);

    setParameter (processor, "Depth", 0.62f);
    setParameter (processor, "Sensitivity", 0.66f);
    setParameter (processor, "Selectivity", 0.62f);
    setParameter (processor, "Speed", 0.55f);
    setParameter (processor, "High Tame", 0.2f);
    setParameter (processor, "Mix", 1.0f);

    juce::AudioBuffer<float> buffer (2, 512);
    juce::MidiBuffer midi;
    double phase1 = 0.0, phase2 = 0.0, phase3 = 0.0, phase4 = 0.0;

    for (int block = 0; block < 220; ++block)
    {
        for (int i = 0; i < 512; ++i)
        {
            const double noise = 0.012 * std::sin (i * 12.9898 + block * 78.233);
            const double value = 0.16 * std::sin (phase1)
                               + 0.10 * std::sin (phase2)
                               + 0.07 * std::sin (phase3)
                               + 0.045 * std::sin (phase4)
                               + noise;

            buffer.setSample (0, i, (float) value);
            buffer.setSample (1, i, (float) (value * 0.95));

            phase1 += 2.0 * 3.14159265358979 * 110.0 / 48000.0;
            phase2 += 2.0 * 3.14159265358979 * 440.0 / 48000.0;
            phase3 += 2.0 * 3.14159265358979 * 1900.0 / 48000.0;
            phase4 += 2.0 * 3.14159265358979 * 6100.0 / 48000.0;
        }

        processor.processBlock (buffer, midi);
    }

    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditorIfNeeded());

    if (editor == nullptr)
    {
        std::printf ("*FAIL* the plugin has no editor\n");
        return 1;
    }

    editor->setSize (width, height);

    // Let the editor timer pull a few analyser frames and smooth the display.
    juce::MessageManager::getInstance()->runDispatchLoopUntil (400);

    if (showBrowser)
    {
        if (auto* component = editor->findChildWithID ("presetButton"))
            if (auto* button = dynamic_cast<juce::TextButton*> (component))
                button->triggerClick();

        juce::MessageManager::getInstance()->runDispatchLoopUntil (250);
    }

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

struct StereoSignalGenerator
{
    double phase = 0.0;

    void fill (juce::AudioBuffer<float>& buffer, int block) const
    {
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            const double t = phase + 2.0 * 3.14159265358979 * 3000.0 * (double) i / 48000.0;
            const double noise = 0.02 * std::sin (i * 9.77 + block * 13.1);
            const float value = (float) (0.22 * std::sin (t) + noise);

            buffer.setSample (0, i, value);
            buffer.setSample (1, i, value);
        }
    }

    void advance (int numSamples)
    {
        phase += 2.0 * 3.14159265358979 * 3000.0 * (double) numSamples / 48000.0;
    }
};

double renderRms (juce::AudioPluginInstance& instance, int blocks)
{
    instance.prepareToPlay (512, 48000.0);

    juce::AudioBuffer<float> buffer (2, 512);
    juce::MidiBuffer midi;
    StereoSignalGenerator generator;

    double sum = 0.0;
    long long count = 0;

    for (int block = 0; block < blocks; ++block)
    {
        generator.fill (buffer, block);
        instance.processBlock (buffer, midi);
        generator.advance (512);

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

double renderProcessorSidechainRms (QuellAudioProcessor& processor, int blocks, bool driven)
{
    processor.prepareToPlay (512, 48000.0);

    juce::AudioBuffer<float> buffer (4, 512);
    juce::MidiBuffer midi;

    StereoSignalGenerator generator;
    double sidechainPhase = 0.0;
    double sum = 0.0;
    long long count = 0;

    for (int block = 0; block < blocks; ++block)
    {
        generator.fill (buffer, block);

        for (int i = 0; i < 512; ++i)
        {
            const double value = driven
                ? 0.40 * std::sin (sidechainPhase + 2.0 * 3.14159265358979 * 3000.0 * (double) i / 48000.0)
                : 0.0;

            buffer.setSample (2, i, (float) value);
            buffer.setSample (3, i, (float) value);
        }

        processor.processBlock (buffer, midi);
        generator.advance (512);
        sidechainPhase += 2.0 * 3.14159265358979 * 3000.0 * 512.0 / 48000.0;

        if (block > blocks / 3)
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < 512; ++i)
                {
                    const double v = buffer.getSample (ch, i);
                    sum += v * v;
                    ++count;
                }
    }

    return std::sqrt (sum / (double) std::max (1LL, count));
}

bool readRealMix (const juce::File& file, juce::AudioBuffer<float>& buffer, double& sampleRate)
{
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();

    std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));

    if (reader == nullptr)
    {
        std::printf ("*FAIL* cannot read real-mix fixture: %s\n", file.getFullPathName().toRawUTF8());
        return false;
    }

    const int channels = juce::jmin (2, (int) reader->numChannels);
    const int numSamples = (int) juce::jmin<juce::int64> (reader->lengthInSamples, 48000LL * 40LL);

    if (channels < 1 || numSamples < 1 || reader->sampleRate <= 0.0)
    {
        std::printf ("*FAIL* real-mix fixture is empty or has an invalid format\n");
        return false;
    }

    buffer.setSize (channels, numSamples);

    if (! reader->read (&buffer, 0, numSamples, 0, true, channels > 1))
    {
        std::printf ("*FAIL* cannot decode real-mix fixture: %s\n", file.getFullPathName().toRawUTF8());
        return false;
    }

    sampleRate = reader->sampleRate;
    return true;
}

double bufferRms (const juce::AudioBuffer<float>& buffer, int start, int length)
{
    double sum = 0.0;
    long long count = 0;

    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        for (int i = 0; i < length; ++i)
        {
            const double value = buffer.getSample (ch, start + i);
            sum += value * value;
            ++count;
        }

    return std::sqrt (sum / (double) std::max (1LL, count));
}

double bandPower (const juce::AudioBuffer<float>& buffer, double sampleRate,
                  double lowHz, double highHz, int start, int length)
{
    constexpr int fftSize = 16384;

    if (length < fftSize)
        return 0.0;

    quell::Fft fft (fftSize);
    std::vector<float> re ((size_t) fftSize), im ((size_t) fftSize), window ((size_t) fftSize);

    for (int i = 0; i < fftSize; ++i)
        window[(size_t) i] = (float) (0.5 - 0.5 * std::cos (2.0 * 3.14159265358979 * (double) i
                                                                     / (double) fftSize));

    const int firstBin = (int) std::ceil (lowHz * (double) fftSize / sampleRate);
    const int lastBin = std::min (fftSize / 2 - 1,
                                  (int) std::floor (highHz * (double) fftSize / sampleRate));
    double power = 0.0;

    for (int offset = start; offset + fftSize <= start + length; offset += fftSize / 2)
    {
        for (int i = 0; i < fftSize; ++i)
        {
            double mono = 0.0;

            for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                mono += buffer.getSample (ch, offset + i);

            re[(size_t) i] = (float) (mono / (double) buffer.getNumChannels()) * window[(size_t) i];
            im[(size_t) i] = 0.0f;
        }

        fft.forward (re.data(), im.data());

        for (int bin = firstBin; bin <= lastBin; ++bin)
            power += (double) re[(size_t) bin] * (double) re[(size_t) bin]
                   + (double) im[(size_t) bin] * (double) im[(size_t) bin];
    }

    return power;
}

int runDefaultMixTest (const juce::File& file, double maxOverallDb, double maxAirDb)
{
    juce::AudioBuffer<float> dry;
    double sampleRate = 0.0;

    if (! readRealMix (file, dry, sampleRate))
        return 2;

    const int channels = dry.getNumChannels();
    constexpr int blockSize = 512;

    QuellAudioProcessor processor;
    processor.setPlayConfigDetails (channels, channels, sampleRate, blockSize);
    processor.prepareToPlay (blockSize, sampleRate);

    auto raw = [&processor] (const char* id)
    {
        return processor.apvts.getRawParameterValue (id)->load();
    };

    std::printf ("      defaults: depth %.1f, sensitivity %.1f, selectivity %.1f\n",
                 raw ("depth"), raw ("sensitivity"), raw ("selectivity"));

    juce::AudioBuffer<float> wet (dry);
    juce::AudioBuffer<float> block (channels, blockSize);
    juce::MidiBuffer midi;

    for (int pos = 0; pos < dry.getNumSamples(); pos += blockSize)
    {
        const int n = std::min (blockSize, dry.getNumSamples() - pos);
        block.clear();

        for (int ch = 0; ch < channels; ++ch)
            block.copyFrom (ch, 0, dry, ch, pos, n);

        processor.processBlock (block, midi);

        for (int ch = 0; ch < channels; ++ch)
            wet.copyFrom (ch, pos, block, ch, 0, n);
    }

    processor.releaseResources();

    const int start = (int) (sampleRate * 0.5);
    const int length = dry.getNumSamples() - start;
    const double dryRms = bufferRms (dry, start, length);
    const double wetRms = bufferRms (wet, start, length);
    const double dryAir = bandPower (dry, sampleRate, 8000.0, 16000.0, start, length);
    const double wetAir = bandPower (wet, sampleRate, 8000.0, 16000.0, start, length);
    const double overallDb = 20.0 * std::log10 ((wetRms + 1.0e-12) / (dryRms + 1.0e-12));
    const double airDb = 10.0 * std::log10 ((wetAir + 1.0e-12) / (dryAir + 1.0e-12));

    std::printf ("      real mix: overall %.2f dB, 8-16 kHz %.2f dB\n", overallDb, airDb);
    std::printf ("      limits:   overall %.2f dB, 8-16 kHz %.2f dB\n", -maxOverallDb, -maxAirDb);

    const auto overallDetail = juce::String::formatted ("%.2f dB (limit %.2f dB)",
                                                         overallDb, -maxOverallDb);
    const auto airDetail = juce::String::formatted ("%.2f dB (limit %.2f dB)",
                                                     airDb, -maxAirDb);

    check (overallDb >= -maxOverallDb, "defaults stay within the real-mix overall threshold",
           overallDetail.toRawUTF8());
    check (airDb >= -maxAirDb, "defaults stay within the real-mix air threshold",
           airDetail.toRawUTF8());

    std::printf ("\n%s\n", failures == 0 ? "REAL-MIX DEFAULTS OK" : "REAL-MIX DEFAULTS FAILED");
    return failures == 0 ? 0 : 1;
}
} // namespace

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;

    if (argc >= 2 && juce::String (argv[1]) == "--defaults-mix")
    {
        if (argc < 3)
        {
            std::printf ("usage: host_test --defaults-mix <real-mix-dry.wav> [maxOverallDb maxAirDb]\n");
            return 2;
        }

        return runDefaultMixTest (juce::File (juce::String (argv[2])),
                                  argc >= 4 ? std::atof (argv[3]) : 2.0,
                                  argc >= 5 ? std::atof (argv[4]) : 5.0);
    }

    const bool browserShot = argc >= 3 && juce::String (argv[1]) == "--shot-browser";

    if (argc >= 3 && (juce::String (argv[1]) == "--shot" || browserShot))
        return renderEditorSnapshot (juce::File (juce::String (argv[2])),
                                     argc >= 5 ? std::atoi (argv[3]) : 920,
                                     argc >= 5 ? std::atoi (argv[4]) : 600,
                                     browserShot);

    if (argc < 2)
    {
        std::printf ("usage: host_test <path to plugin.vst3> | --defaults-mix <dry.wav> [maxOverallDb maxAirDb] | --shot <out.png> [w h] | --shot-browser <out.png> [w h]\n");
        return 2;
    }

    const juce::String path (argv[1]);

    std::printf ("Quell host validation (JUCE VST3 host)\n======================================\n");
    std::printf ("plugin: %s\n\n", path.toRawUTF8());

    juce::VST3PluginFormat format;
    juce::OwnedArray<juce::PluginDescription> types;
    format.findAllTypesForFile (types, path);

    std::printf ("      descriptions found: %d\n", types.size());

    for (auto* description : types)
        std::printf ("      - %s | %s | %s\n",
                     description->name.toRawUTF8(),
                     description->manufacturerName.toRawUTF8(),
                     description->pluginFormatName.toRawUTF8());

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

    instance->setPlayConfigDetails (2, 2, 48000.0, 512);
    instance->prepareToPlay (512, 48000.0);

    std::printf ("      latency: %d samples\n", instance->getLatencySamples());

    const auto parameters = instance->getParameters();
    std::printf ("      parameters: %d\n", (int) parameters.size());
    std::printf ("      buses: %d in, %d out\n", instance->getBusCount (true),
                 instance->getBusCount (false));

    check (parameters.size() == 13, "all 13 parameters are visible to the host");
    check (instance->getBusCount (true) >= 2, "the sidechain input bus is declared");
    check (instance->getLatencySamples() > 0, "the STFT latency is reported to the host");

    for (int i = 0; i < parameters.size(); ++i)
        std::printf ("        [%2d] %-22s = %s\n", i,
                     parameters.getUnchecked (i)->getName (48).toRawUTF8(),
                     parameters.getUnchecked (i)->getCurrentValueAsText().toRawUTF8());

    //====================================================== depth changes the audio
    {
        setParameter (*instance, "Depth", 0.0f);
        setParameter (*instance, "Sensitivity", 0.9f);
        setParameter (*instance, "Selectivity", 0.8f);
        setParameter (*instance, "Mix", 1.0f);

        const double shallow = renderRms (*instance, 160);

        setParameter (*instance, "Depth", 1.0f);
        const double deep = renderRms (*instance, 160);

        const double reductionDb = 20.0 * std::log10 ((deep + 1.0e-12) / (shallow + 1.0e-12));

        std::printf ("      RMS at depth 0: %.5f, at depth 100: %.5f (%.2f dB)\n",
                     shallow, deep, reductionDb);

        check (std::isfinite (shallow) && shallow > 0.1, "audio passes through the plugin");
        check (std::isfinite (deep), "output stays finite at full depth");
        check (reductionDb < -10.0, "raising depth actually suppresses the 3 kHz resonance");
    }

    //============================================================= delta listen
    {
        setParameter (*instance, "Delta", 1.0f);
        const double delta = renderRms (*instance, 80);

        setParameter (*instance, "Delta", 0.0f);

        std::printf ("      delta RMS: %.5f\n", delta);
        check (std::isfinite (delta) && delta > 1.0e-4, "delta monitoring produces the removed signal");
    }

    //============================================================= mid/side mode
    {
        setParameter (*instance, "Depth", 0.0f);
        setParameter (*instance, "Mid/Side", 1.0f);
        const double msRms = renderRms (*instance, 80);
        setParameter (*instance, "Mid/Side", 0.0f);

        std::printf ("      mid/side pass-through RMS: %.5f\n", msRms);
        check (std::isfinite (msRms) && msRms > 0.1, "mid/side mode passes audio");
    }

    //============================================================= sidechain mode
    {
        setParameter (*instance, "Sidechain", 1.0f);
        const double scRms = renderRms (*instance, 80);
        setParameter (*instance, "Sidechain", 0.0f);

        std::printf ("      sidechain-mode RMS: %.5f\n", scRms);
        check (std::isfinite (scRms) && scRms > 0.1,
               "sidechain mode falls back to internal detection when no bus is connected");
    }

    //=============================================== external sidechain bus routing
    {
        QuellAudioProcessor scProcessor;

        juce::AudioProcessor::BusesLayout layout;
        layout.inputBuses.add (juce::AudioChannelSet::stereo());
        layout.inputBuses.add (juce::AudioChannelSet::stereo());
        layout.outputBuses.add (juce::AudioChannelSet::stereo());

        const bool activated = scProcessor.setBusesLayout (layout);

        std::printf ("      processor sidechain layout: %s (%d in, %d out)\n",
                     activated ? "accepted" : "rejected",
                     scProcessor.getTotalNumInputChannels(),
                     scProcessor.getTotalNumOutputChannels());

        check (activated, "the processor activates the sidechain input bus");
        check (scProcessor.getChannelCountOfBus (true, 1) == 2,
               "the active sidechain bus exposes two channels");

        if (activated)
        {
            setParameter (scProcessor, "Depth", 1.0f);
            setParameter (scProcessor, "Sensitivity", 0.75f);
            setParameter (scProcessor, "Selectivity", 0.85f);
            setParameter (scProcessor, "Mix", 1.0f);
            setParameter (scProcessor, "Mid/Side", 0.0f);
            setParameter (scProcessor, "Sidechain", 1.0f);

            const double silent = renderProcessorSidechainRms (scProcessor, 160, false);
            const double driven = renderProcessorSidechainRms (scProcessor, 160, true);

            std::printf ("      main RMS with silent sidechain: %.5f, with 3 kHz sidechain: %.5f\n",
                         silent, driven);

            check (std::isfinite (silent) && silent > 0.1,
                   "the main input is audible with the sidechain bus active");
            check (driven < silent * 0.5,
                   "a signal on the sidechain bus suppresses the main input");
        }
    }

    //================================================================== editor
    {
        int editorWidth = 0, editorHeight = 0;
        const bool hasEditor = canCreateEditor (*instance, editorWidth, editorHeight);

        std::printf ("      editor: %dx%d\n", editorWidth, editorHeight);
        check (hasEditor, "the plugin creates its editor");
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

    //============================================================ user presets
    {
        const auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory)
                             .getChildFile ("QuellPresetTest_"
                                            + juce::String (juce::Time::currentTimeMillis())
                                            + "_"
                                            + juce::String (juce::Random::getSystemRandom().nextInt (100000)));
        dir.createDirectory();

        quell::PresetLibrary library (dir);
        check (library.getUserPresets().empty(), "user preset library starts empty");

        QuellAudioProcessor presetProcessor;

        quell::PresetLibrary::applyFactoryPreset (1, presetProcessor.apvts);
        check (std::abs (presetProcessor.apvts.getRawParameterValue ("depth")->load() - 55.0f) < 0.01f,
               "factory presets apply their full settings");

        setParameter (presetProcessor, "Depth", 0.70f);
        setParameter (presetProcessor, "Sensitivity", 0.60f);
        setParameter (presetProcessor, "Selectivity", 0.65f);
        setParameter (presetProcessor, "Speed", 0.55f);
        setParameter (presetProcessor, "Low Tame", 0.20f);
        setParameter (presetProcessor, "High Tame", 0.30f);
        setParameter (presetProcessor, "Mix", 0.80f);
        setParameter (presetProcessor, "Output", 0.5625f); // +3 dB
        setParameter (presetProcessor, "Stereo Link", 0.0f);
        setParameter (presetProcessor, "Mid/Side", 1.0f);
        setParameter (presetProcessor, "Sidechain", 1.0f);

        juce::String error;
        check (library.saveUserPreset ("  Test Preset  ", presetProcessor.apvts, error),
               "user preset can be saved", error.toRawUTF8());
        check (library.getUserPresets().size() == 1,
               "saved preset appears in the library");
        check (library.getUserPresets()[0].name == "Test Preset",
               "preset names are trimmed");

        setParameter (presetProcessor, "Depth", 0.05f);
        setParameter (presetProcessor, "Sensitivity", 0.10f);
        setParameter (presetProcessor, "Selectivity", 0.15f);
        setParameter (presetProcessor, "Speed", 0.20f);
        setParameter (presetProcessor, "Low Tame", 0.80f);
        setParameter (presetProcessor, "High Tame", 0.90f);
        setParameter (presetProcessor, "Mix", 0.30f);
        setParameter (presetProcessor, "Output", 0.25f);
        setParameter (presetProcessor, "Stereo Link", 1.0f);
        setParameter (presetProcessor, "Mid/Side", 0.0f);
        setParameter (presetProcessor, "Sidechain", 0.0f);

        check (library.loadUserPreset (0, presetProcessor.apvts, error),
               "user preset can be loaded", error.toRawUTF8());

        auto raw = [&presetProcessor] (const char* id)
        {
            return presetProcessor.apvts.getRawParameterValue (id)->load();
        };

        const bool valuesMatch = std::abs (raw ("depth") - 70.0f) < 0.01f
            && std::abs (raw ("sensitivity") - 60.0f) < 0.01f
            && std::abs (raw ("selectivity") - 65.0f) < 0.01f
            && std::abs (raw ("speed") - 55.0f) < 0.01f
            && std::abs (raw ("lowtame") - 20.0f) < 0.01f
            && std::abs (raw ("hightame") - 30.0f) < 0.01f
            && std::abs (raw ("mix") - 80.0f) < 0.01f
            && std::abs (raw ("output") - 3.0f) < 0.05f
            && raw ("link") < 0.5f
            && raw ("msmode") > 0.5f
            && raw ("sidechain") > 0.5f;

        std::printf ("      preset values restored: depth %.0f, output %.1f dB, modes %s/%s\n",
                     raw ("depth"), raw ("output"),
                     raw ("msmode") > 0.5f ? "M/S" : "L/R",
                     raw ("sidechain") > 0.5f ? "SC" : "internal");
        check (valuesMatch, "save/load restores every sound parameter");

        setParameter (presetProcessor, "Depth", 0.80f);
        check (library.saveUserPreset ("Test Preset", presetProcessor.apvts, error),
               "a user preset can be overwritten", error.toRawUTF8());
        check (library.getUserPresets().size() == 1,
               "overwriting does not duplicate the preset");

        quell::PresetLibrary reopened (dir);
        check (reopened.getUserPresets().size() == 1,
               "user presets persist across library instances");
        check (reopened.loadUserPreset (0, presetProcessor.apvts, error),
               "a persisted preset reloads", error.toRawUTF8());
        check (std::abs (raw ("depth") - 80.0f) < 0.01f,
               "the overwritten value is persisted");

        check (reopened.deleteUserPreset (0, error), "a user preset can be deleted",
               error.toRawUTF8());
        check (reopened.getUserPresets().empty(),
               "deleting removes the preset from the library");

        dir.deleteRecursively();
    }

    //============================================== preset name survives editor reopen
    {
        QuellAudioProcessor source;
        quell::PresetLibrary::applyFactoryPreset (1, source.apvts);

        const float expectedDepth = source.apvts.getRawParameterValue ("depth")->load();

        juce::MemoryBlock state;
        source.getStateInformation (state);

        QuellAudioProcessor restored;
        restored.setStateInformation (state.getData(), (int) state.getSize());

        check (std::abs (restored.apvts.getRawParameterValue ("depth")->load() - expectedDepth) < 0.01f,
               "preset parameters survive a state round-trip");

        std::unique_ptr<juce::AudioProcessorEditor> firstEditor (restored.createEditorIfNeeded());
        auto* firstButton = firstEditor != nullptr
                                ? dynamic_cast<juce::TextButton*> (firstEditor->findChildWithID ("presetButton"))
                                : nullptr;

        check (firstButton != nullptr && firstButton->getButtonText().startsWith ("Vocal Cleanup"),
               "selected preset name is shown after reopening the editor");

        firstEditor.reset();

        std::unique_ptr<juce::AudioProcessorEditor> secondEditor (restored.createEditorIfNeeded());
        auto* secondButton = secondEditor != nullptr
                                 ? dynamic_cast<juce::TextButton*> (secondEditor->findChildWithID ("presetButton"))
                                 : nullptr;

        check (secondButton != nullptr && secondButton->getButtonText().startsWith ("Vocal Cleanup"),
               "selected preset name survives closing and recreating the editor");

        setParameter (restored, "Depth", 0.47f);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (50);

        check (secondButton != nullptr && secondButton->getButtonText().startsWith ("Unsaved"),
               "editing a parameter marks the preset Unsaved");
    }

    //====================================== preset selection through the browser UI
    {
        QuellAudioProcessor browserProcessor;
        std::unique_ptr<juce::AudioProcessorEditor> browserEditor (
            browserProcessor.createEditorIfNeeded());

        auto* presetButton = browserEditor != nullptr
                                 ? dynamic_cast<juce::TextButton*> (
                                       browserEditor->findChildWithID ("presetButton"))
                                 : nullptr;

        check (presetButton != nullptr && presetButton->getButtonText().startsWith ("Init"),
               "the editor starts on the Init preset");

        if (presetButton != nullptr)
            presetButton->triggerClick();

        juce::MessageManager::getInstance()->runDispatchLoopUntil (100);

        auto* browser = browserEditor != nullptr
                            ? dynamic_cast<quell_ui::PresetBrowser*> (
                                  browserEditor->findChildWithID ("presetBrowser"))
                            : nullptr;
        auto* list = browser != nullptr
                         ? dynamic_cast<juce::ListBox*> (browser->findChildWithID ("presetList"))
                         : nullptr;
        auto* loadButton = browser != nullptr
                               ? dynamic_cast<juce::TextButton*> (
                                     browser->findChildWithID ("presetLoadButton"))
                               : nullptr;

        check (browser != nullptr && browser->isVisible(),
               "the preset button opens the preset browser");
        check (list != nullptr && loadButton != nullptr,
               "the browser exposes its preset list and Load button");

        // Row 0 is the factory header; row 1 is Init; row 2 is Vocal Cleanup.
        constexpr int vocalCleanupRow = 2;

        if (list != nullptr)
        {
            list->selectRow (vocalCleanupRow);
            juce::MessageManager::getInstance()->runDispatchLoopUntil (50);
        }

        check (list != nullptr && list->getSelectedRow() == vocalCleanupRow,
               "a factory preset can be selected in the browser");
        check (loadButton != nullptr && loadButton->isEnabled(),
               "the Load button enables for the selected preset");

        if (loadButton != nullptr)
        {
            loadButton->triggerClick();
            juce::MessageManager::getInstance()->runDispatchLoopUntil (100);
        }

        auto raw = [&browserProcessor] (const char* id)
        {
            return browserProcessor.apvts.getRawParameterValue (id)->load();
        };

        const bool valuesMatch = std::abs (raw ("depth") - 55.0f) < 0.01f
            && std::abs (raw ("sensitivity") - 60.0f) < 0.01f
            && std::abs (raw ("selectivity") - 60.0f) < 0.01f
            && std::abs (raw ("speed") - 55.0f) < 0.01f
            && std::abs (raw ("lowtame") - 20.0f) < 0.01f
            && std::abs (raw ("hightame") - 15.0f) < 0.01f
            && std::abs (raw ("mix") - 100.0f) < 0.01f
            && std::abs (raw ("output") - 0.0f) < 0.01f
            && raw ("link") > 0.5f;

        check (valuesMatch, "browser selection applies the factory preset values");
        check (browserProcessor.apvts.state.getProperty ("presetName").toString()
                   == "Vocal Cleanup",
               "browser selection stores the preset name");
        check (presetButton != nullptr
                   && presetButton->getButtonText().startsWith ("Vocal Cleanup"),
               "browser selection updates the editor preset name");
    }

    instance->releaseResources();

    std::printf ("\n%s\n", failures == 0 ? "PLUGIN OK IN A HOST" : "PLUGIN HAS PROBLEMS");
    return failures == 0 ? 0 : 1;
}
