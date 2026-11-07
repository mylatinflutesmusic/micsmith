// Numeric checks for the resonance-suppressor engine. Deliberately JUCE-free so
// it builds and runs without a plugin host.

#include "dsp/SpectralSuppressor.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <random>
#include <vector>

namespace
{
int failures = 0;

void check (bool ok, const char* what, const char* detail = "")
{
    std::printf ("%s  %s %s\n", ok ? " pass " : "*FAIL*", what, detail);

    if (! ok)
        ++failures;
}

constexpr double pi = 3.14159265358979323846;

std::vector<float> makeSignal (double sampleRate, int numSamples, double toneHz, double toneAmp,
                               double noiseAmp, unsigned seed = 1)
{
    std::mt19937 rng (seed);
    std::uniform_real_distribution<double> dist (-1.0, 1.0);

    std::vector<float> signal ((size_t) numSamples, 0.0f);

    for (int i = 0; i < numSamples; ++i)
    {
        const double tone = toneAmp * std::sin (2.0 * pi * toneHz * (double) i / sampleRate);
        signal[(size_t) i] = (float) (tone + noiseAmp * dist (rng));
    }

    return signal;
}

struct Renderer
{
    quell::SpectralSuppressor suppressor;
    double sampleRate = 48000.0;
    int channels = 1;

    void prepare (double sr, int numChannels, const quell::SuppressorSettings& settings)
    {
        sampleRate = sr;
        channels = numChannels;
        suppressor.prepare (sr, numChannels);
        suppressor.setSettings (settings);
    }

    std::vector<std::vector<float>> run (const std::vector<std::vector<float>>& input,
                                         float mix = 1.0f, bool delta = false, bool bypass = false,
                                         const std::vector<std::vector<float>>* sidechain = nullptr)
    {
        auto output = input;
        const int total = (int) input[0].size();

        for (int pos = 0; pos < total; pos += 256)
        {
            const int block = std::min (256, total - pos);

            float* pointers[2] { output[0].data() + pos,
                                 channels > 1 ? output[1].data() + pos : nullptr };

            const float* sidechainPointers[2] { nullptr, nullptr };
            int sidechainChannels = 0;

            if (sidechain != nullptr)
            {
                sidechainChannels = std::min ((int) sidechain->size(), 2);

                for (int ch = 0; ch < sidechainChannels; ++ch)
                    sidechainPointers[ch] = (*sidechain)[(size_t) ch].data() + pos;
            }

            suppressor.process (pointers, channels, block, mix, delta, bypass,
                                sidechainChannels > 0 ? sidechainPointers : nullptr, sidechainChannels);
        }

        return output;
    }
};

/** Hann-windowed projection onto one frequency, plus the residual after it. */
void analyseTone (const std::vector<float>& x, double sampleRate, double freq,
                  int start, int length, double& amplitude, double& residualRms)
{
    std::vector<float> w ((size_t) length);
    double windowSum = 0.0;

    for (int i = 0; i < length; ++i)
    {
        w[(size_t) i] = (float) (0.5 - 0.5 * std::cos (2.0 * pi * (double) i / (double) (length - 1)));
        windowSum += w[(size_t) i];
    }

    double cosSum = 0.0, sinSum = 0.0;

    for (int i = 0; i < length; ++i)
    {
        const double phase = 2.0 * pi * freq * (double) i / sampleRate;
        const double v = (double) x[(size_t) (start + i)] * (double) w[(size_t) i];

        cosSum += v * std::cos (phase);
        sinSum += v * std::sin (phase);
    }

    const double a = 2.0 * cosSum / windowSum;
    const double b = 2.0 * sinSum / windowSum;

    amplitude = std::sqrt (a * a + b * b);

    double residual = 0.0;

    for (int i = 0; i < length; ++i)
    {
        const double phase = 2.0 * pi * freq * (double) i / sampleRate;
        const double modelled = a * std::cos (phase) + b * std::sin (phase);
        const double r = (double) x[(size_t) (start + i)] - modelled;

        residual += r * r;
    }

    residualRms = std::sqrt (residual / (double) length);
}

double rmsOf (const std::vector<float>& x, int start, int length)
{
    double sum = 0.0;

    for (int i = 0; i < length; ++i)
    {
        const double v = x[(size_t) (start + i)];
        sum += v * v;
    }

    return std::sqrt (sum / (double) length);
}

double maxDifference (const std::vector<float>& reference, const std::vector<float>& processed,
                      int start, int delay)
{
    double worst = 0.0;

    for (size_t i = (size_t) start; i < processed.size(); ++i)
        worst = std::max (worst, std::abs ((double) reference[i - (size_t) delay]
                                          - (double) processed[i]));

    return worst;
}
} // namespace

int main()
{
    std::printf ("Quell DSP checks\n================\n\n");

    //==========================================================================
    // 1. FFT round-trip.
    {
        quell::Fft fft (1024);

        std::mt19937 rng (42);
        std::uniform_real_distribution<float> dist (-1.0f, 1.0f);

        std::vector<float> re (1024), im (1024), re0 (1024), im0 (1024);

        for (int i = 0; i < 1024; ++i)
        {
            re0[(size_t) i] = re[(size_t) i] = dist (rng);
            im0[(size_t) i] = im[(size_t) i] = dist (rng);
        }

        fft.forward (re.data(), im.data());
        fft.inverse (re.data(), im.data());

        double worst = 0.0;

        for (int i = 0; i < 1024; ++i)
        {
            worst = std::max (worst, (double) std::abs (re[(size_t) i] - re0[(size_t) i]));
            worst = std::max (worst, (double) std::abs (im[(size_t) i] - im0[(size_t) i]));
        }

        std::printf ("worst round-trip error: %.3e\n", worst);
        check (worst < 1.0e-5, "FFT forward/inverse round-trips");
    }

    //==========================================================================
    // 2. Zero depth must be bit-transparent apart from the reported latency.
    {
        const double rates[] = { 44100.0, 48000.0, 96000.0 };
        bool transparent = true;
        bool latencyCorrect = true;

        for (double sr : rates)
        {
            quell::SuppressorSettings settings;
            settings.depth = 0.0f;

            Renderer renderer;
            renderer.prepare (sr, 1, settings);

            const int latency = renderer.suppressor.getLatencySamples();

            const int length = (int) (sr * 3.0);
            const auto input = makeSignal (sr, length, 997.0, 0.25, 0.01);
            const auto output = renderer.run ({ input });

            bool finite = true;

            for (float v : output[0])
                if (! std::isfinite (v))
                    finite = false;

            const double error = maxDifference (input, output[0], latency + 64, latency);

            std::printf ("%6.0f Hz: latency %d, worst |wet - delayed dry| = %.3e\n",
                         sr, latency, error);

            if (error > 1.0e-4 || ! finite)
                transparent = false;
        }

        {
            quell::SuppressorSettings settings;
            settings.depth = 0.0f;

            Renderer renderer;
            renderer.prepare (48000.0, 1, settings);

            std::vector<float> impulse (8192, 0.0f);
            impulse[0] = 1.0f;

            const auto output = renderer.run ({ impulse });
            const int peakIndex = (int) (std::max_element (output[0].begin(), output[0].end())
                                         - output[0].begin());

            std::printf ("impulse peak lands at sample %d (reported latency %d)\n",
                         peakIndex, renderer.suppressor.getLatencySamples());

            if (peakIndex != renderer.suppressor.getLatencySamples()
                || std::abs (output[0][(size_t) peakIndex] - 1.0f) > 1.0e-4f)
                latencyCorrect = false;
        }

        check (transparent, "depth 0 is transparent at 44.1/48/96 kHz");
        check (latencyCorrect, "latency is exact and matches the dry delay");
    }

    //==========================================================================
    // 3. A resonance above a broadband bed must be cut more than the bed.
    {
        quell::SuppressorSettings settings;
        settings.depth = 1.0f;
        settings.sensitivity = 0.75f;
        settings.selectivity = 0.85f;
        settings.speed = 0.5f;

        Renderer renderer;
        renderer.prepare (48000.0, 1, settings);

        const int length = 48000 * 2;
        const double sr = 48000.0;
        const double resonanceHz = 3000.0;

        const auto input = makeSignal (sr, length, resonanceHz, 0.25, 0.04);
        const auto output = renderer.run ({ input });

        const int analysisLength = 16384;
        const int start = length - analysisLength;

        double inAmp = 0.0, inResidual = 0.0, outAmp = 0.0, outResidual = 0.0;

        analyseTone (input, sr, resonanceHz, start, analysisLength, inAmp, inResidual);
        analyseTone (output[0], sr, resonanceHz, start, analysisLength, outAmp, outResidual);

        const double resonanceDb = 20.0 * std::log10 ((outAmp + 1.0e-12) / (inAmp + 1.0e-12));
        const double bedDb = 20.0 * std::log10 ((outResidual + 1.0e-12) / (inResidual + 1.0e-12));

        std::printf ("resonance: %.1f dB, broadband bed: %.1f dB\n", resonanceDb, bedDb);

        check (resonanceDb < -8.0, "a 3 kHz resonance is cut by more than 8 dB");
        check (std::abs (bedDb) < 4.0, "the broadband bed is left mostly intact");
        check (renderer.suppressor.getMaxCutDb() > 1.0f, "maximum cut is reported for the display");
    }

    //==========================================================================
    // 4. Delta plus wet must reconstruct the delayed dry signal.
    {
        quell::SuppressorSettings settings;
        settings.depth = 0.8f;
        settings.sensitivity = 0.7f;

        Renderer wetRenderer, deltaRenderer;
        wetRenderer.prepare (48000.0, 1, settings);
        deltaRenderer.prepare (48000.0, 1, settings);

        const int length = 48000;
        const auto input = makeSignal (48000.0, length, 1770.0, 0.2, 0.03, 7);

        const auto wet = wetRenderer.run ({ input }, 1.0f, false);
        const auto delta = deltaRenderer.run ({ input }, 1.0f, true);

        const int latency = wetRenderer.suppressor.getLatencySamples();
        double worst = 0.0;

        for (int i = latency + 256; i < length; ++i)
        {
            const double delayed = (double) input[(size_t) (i - latency)];
            worst = std::max (worst, std::abs (delayed - ((double) wet[0][(size_t) i] + (double) delta[0][(size_t) i])));
        }

        std::printf ("worst |dry - (wet + delta)|: %.3e\n", worst);
        check (worst < 1.0e-4, "delta and wet sum back to the dry signal");
    }

    //==========================================================================
    // 5. Mid/side mode reconstructs the stereo signal exactly at depth 0.
    {
        quell::SuppressorSettings settings;
        settings.depth = 0.0f;
        settings.midSide = true;

        Renderer renderer;
        renderer.prepare (48000.0, 2, settings);

        const int length = 48000 * 2;
        std::vector<std::vector<float>> input (2, std::vector<float> ((size_t) length));

        for (int i = 0; i < length; ++i)
        {
            const double t = 2.0 * pi * (double) i / 48000.0;
            const double common = 0.06 * std::sin (431.0 * t);

            input[0][(size_t) i] = (float) (0.22 * std::sin (997.0 * t) + common);
            input[1][(size_t) i] = (float) (0.22 * std::sin (1499.0 * t) - common);
        }

        const auto output = renderer.run (input);
        const int latency = renderer.suppressor.getLatencySamples();

        const double errorL = maxDifference (input[0], output[0], latency + 64, latency);
        const double errorR = maxDifference (input[1], output[1], latency + 64, latency);

        std::printf ("mid/side depth-0 error: L %.3e, R %.3e\n", errorL, errorR);
        check (errorL < 1.0e-4 && errorR < 1.0e-4,
               "mid/side mode reconstructs the stereo signal exactly at depth 0");
    }

    //==========================================================================
    // 6. Mid/side processing still suppresses a panned resonance.
    {
        quell::SuppressorSettings settings;
        settings.depth = 1.0f;
        settings.sensitivity = 0.75f;
        settings.selectivity = 0.85f;
        settings.midSide = true;
        settings.link = false;

        Renderer renderer;
        renderer.prepare (48000.0, 2, settings);

        const int length = 48000 * 2;
        std::vector<std::vector<float>> input (2, std::vector<float> ((size_t) length));

        for (int i = 0; i < length; ++i)
        {
            const double t = 2.0 * pi * (double) i / 48000.0;

            input[0][(size_t) i] = (float) (0.30 * std::sin (3000.0 * t)
                                           + 0.04 * std::sin (8000.0 * t));
            input[1][(size_t) i] = (float) (0.15 * std::sin (900.0 * t)
                                           + 0.04 * std::sin (11000.0 * t));
        }

        const auto output = renderer.run (input);
        const int start = length - 16384;

        double inAmp = 0.0, inResidual = 0.0, outAmp = 0.0, outResidual = 0.0;
        analyseTone (input[0], 48000.0, 3000.0, start, 16384, inAmp, inResidual);
        analyseTone (output[0], 48000.0, 3000.0, start, 16384, outAmp, outResidual);

        const double db = 20.0 * std::log10 ((outAmp + 1.0e-12) / (inAmp + 1.0e-12));

        bool finite = true;

        for (const auto& channel : output)
            for (float value : channel)
                finite = finite && std::isfinite (value);

        std::printf ("mid/side 3 kHz resonance: %.1f dB\n", db);
        check (finite, "mid/side processing stays finite");
        check (db < -8.0, "mid/side processing suppresses a panned resonance");
    }

    //==========================================================================
    // 7. External sidechain drives detection; a silent one does nothing.
    {
        quell::SuppressorSettings settings;
        settings.depth = 1.0f;
        settings.sensitivity = 0.75f;
        settings.selectivity = 0.85f;

        Renderer withSidechain, withSilence;
        withSidechain.prepare (48000.0, 1, settings);
        withSilence.prepare (48000.0, 1, settings);

        const int length = 48000 * 2;
        const auto mainSignal = makeSignal (48000.0, length, 3000.0, 0.22, 0.04, 21);
        const auto sidechainTone = makeSignal (48000.0, length, 3000.0, 0.30, 0.01, 22);
        const std::vector<float> silence ((size_t) length, 0.0f);

        const std::vector<std::vector<float>> sidechain { sidechainTone };
        const std::vector<std::vector<float>> quiet { silence };

        const auto driven = withSidechain.run ({ mainSignal }, 1.0f, false, false, &sidechain);
        const auto undriven = withSilence.run ({ mainSignal }, 1.0f, false, false, &quiet);

        const int start = length - 16384;
        double inAmp = 0.0, inResidual = 0.0;
        double drivenAmp = 0.0, drivenResidual = 0.0;
        double undrivenAmp = 0.0, undrivenResidual = 0.0;

        analyseTone (mainSignal, 48000.0, 3000.0, start, 16384, inAmp, inResidual);
        analyseTone (driven[0], 48000.0, 3000.0, start, 16384, drivenAmp, drivenResidual);
        analyseTone (undriven[0], 48000.0, 3000.0, start, 16384, undrivenAmp, undrivenResidual);

        const double drivenDb = 20.0 * std::log10 ((drivenAmp + 1.0e-12) / (inAmp + 1.0e-12));
        const double undrivenDb = 20.0 * std::log10 ((undrivenAmp + 1.0e-12) / (inAmp + 1.0e-12));

        std::printf ("sidechain-driven 3 kHz: %.1f dB, silent sidechain: %.1f dB\n",
                     drivenDb, undrivenDb);

        check (drivenDb < -8.0, "the external sidechain drives suppression");
        check (std::abs (undrivenDb) < 0.5, "a silent sidechain leaves the main signal untouched");
    }

    //==========================================================================
    // 8. Stability and bounded output at extreme settings, stereo, 96 kHz.
    {
        quell::SuppressorSettings settings;
        settings.depth = 1.0f;
        settings.sensitivity = 1.0f;
        settings.selectivity = 1.0f;
        settings.speed = 1.0f;
        settings.lowTame = 0.0f;
        settings.highTame = 0.0f;
        settings.link = true;

        Renderer renderer;
        renderer.prepare (96000.0, 2, settings);

        std::mt19937 rng (99);
        std::uniform_real_distribution<float> dist (-1.0f, 1.0f);

        const int length = 96000 * 10;
        std::vector<std::vector<float>> input (2, std::vector<float> ((size_t) length));

        for (int i = 0; i < length; ++i)
        {
            input[0][(size_t) i] = 0.9f * dist (rng);
            input[1][(size_t) i] = 0.9f * std::sin (2.0 * pi * 40.0 * (double) i / 96000.0)
                                  + 0.1f * dist (rng);
        }

        const auto output = renderer.run (input);

        double peak = 0.0;
        bool finite = true;
        double worstCut = 0.0;

        for (int ch = 0; ch < 2; ++ch)
        {
            for (float v : output[(size_t) ch])
            {
                if (! std::isfinite (v))
                    finite = false;

                peak = std::max (peak, (double) std::abs (v));
            }
        }

        quell::DisplaySnapshot display;
        renderer.suppressor.readDisplay (display);

        for (int i = 0; i < quell::DisplaySnapshot::numPoints; ++i)
        {
            worstCut = std::max (worstCut, (double) display.cutDb[i]);

            if (display.cutDb[i] < -0.001f || display.cutDb[i] > 36.001f)
                finite = false;

            if (display.inputDb[i] < -111.0f || display.inputDb[i] > 6.001f)
                finite = false;
        }

        std::printf ("worst peak %.3f, worst displayed cut %.1f dB, latency %d\n",
                     peak, worstCut, renderer.suppressor.getLatencySamples());

        check (finite, "extreme settings stay finite and in range");
        check (peak < 4.0, "output remains bounded at extreme settings");
        check (renderer.suppressor.getLatencySamples() == 2048,
               "96 kHz uses a 2048-sample frame (twice the low-rate latency)");
    }

    //==========================================================================
    // 6. Mono and stereo link both run and agree with themselves.
    {
        quell::SuppressorSettings settings;
        settings.depth = 0.6f;
        settings.sensitivity = 0.6f;

        for (int channels = 1; channels <= 2; ++channels)
        {
            for (int link = 0; link <= 1; ++link)
            {
                settings.link = link != 0;

                Renderer renderer;
                renderer.prepare (48000.0, channels, settings);

                const int length = 48000;
                std::vector<std::vector<float>> input ((size_t) channels,
                                                       std::vector<float> ((size_t) length));

                for (int ch = 0; ch < channels; ++ch)
                {
                    const auto signal = makeSignal (48000.0, length, 1200.0 + 500.0 * ch, 0.2, 0.03,
                                                    (unsigned) (11 + ch));

                    input[(size_t) ch] = signal;
                }

                const auto output = renderer.run (input);
                bool finite = true;

                for (const auto& channel : output)
                    for (float v : channel)
                        if (! std::isfinite (v))
                            finite = false;

                check (finite, channels == 1 ? "mono processing is finite"
                                             : (link ? "stereo-linked processing is finite"
                                                     : "stereo-independent processing is finite"));
            }
        }
    }

    std::printf ("\n%s\n", failures == 0 ? "ALL CHECKS PASSED" : "FAILURES PRESENT");
    return failures == 0 ? 0 : 1;
}
