// Numeric checks for the K Channel DSP. JUCE-free so it can run without a host.

#include "dsp/ChannelDsp.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <random>
#include <vector>

namespace
{
int failures = 0;

void check (bool ok, const char* what)
{
    std::printf ("%s  %s\n", ok ? " pass " : "*FAIL*", what);
    if (! ok)
        ++failures;
}

constexpr double pi = 3.14159265358979323846;

double rms (const std::vector<float>& x, int start, int length)
{
    double sum = 0.0;

    for (int i = 0; i < length; ++i)
    {
        const double v = x[(size_t) (start + i)];
        sum += v * v;
    }

    return std::sqrt (sum / (double) length);
}
}

int main()
{
    std::printf ("K Channel DSP checks\n====================\n\n");

    //================================================================== EQ curve
    {
        kchannel::EqSettings eq;
        eq.eqOn = true;
        eq.filtersOn = false;
        eq.lmFreq = 1000.0f;
        eq.lmGain = 6.0f;
        eq.lmQ = 1.0f;
        eq.lfGain = 0.0f;
        eq.hmGain = 0.0f;
        eq.hfGain = 0.0f;

        const double freqs[] = { 100.0, 1000.0, 10000.0 };
        float db[3] {};
        kchannel::eqResponseDb (eq, 48000.0, freqs, db, 3);

        std::printf ("peaking 1 kHz +6 dB: 100 Hz %.2f, 1 kHz %.2f, 10 kHz %.2f\n",
                     db[0], db[1], db[2]);

        check (db[1] > 5.5f && db[1] < 6.5f, "peaking band reaches its requested gain");
        check (std::abs (db[0]) < 1.0f && std::abs (db[2]) < 1.0f,
               "peaking band leaves distant frequencies alone");
    }

    //==========================================================================
    {
        kchannel::EqSettings eq;
        eq.eqOn = false;
        eq.filtersOn = true;
        eq.hpfHz = 100.0f;
        eq.lpfHz = 10000.0f;

        const double freqs[] = { 30.0, 1000.0, 18000.0 };
        float db[3] {};
        kchannel::eqResponseDb (eq, 48000.0, freqs, db, 3);

        std::printf ("filters: 30 Hz %.2f dB, 1 kHz %.2f dB, 18 kHz %.2f dB\n",
                     db[0], db[1], db[2]);

        check (db[0] < -12.0f, "high-pass rolls off the low end");
        check (std::abs (db[1]) < 0.2f, "filters are transparent in the middle");
        check (db[2] < -10.0f, "low-pass rolls off the top end");
    }

    //================================================================ compressor
    {
        kchannel::CompressorSettings settings;
        settings.thresholdDb = -20.0f;
        settings.ratio = 4.0f;
        settings.attackMs = 1.0f;
        settings.releaseMs = 100.0f;
        settings.makeupDb = 0.0f;
        settings.kneeDb = 0.0f;

        kchannel::Compressor compressor;
        compressor.prepare (48000.0);
        compressor.setSettings (settings);

        const int length = 48000 * 2;
        std::vector<float> input ((size_t) length), output ((size_t) length);

        for (int i = 0; i < length; ++i)
        {
            input[(size_t) i] = (float) (0.5 * std::sin (2.0 * pi * 220.0 * (double) i / 48000.0));
            output[(size_t) i] = input[(size_t) i];
        }

        float* channels[] = { output.data() };
        compressor.process (channels, 1, length);

        const double inRms = rms (input, length - 16384, 16384);
        const double outRms = rms (output, length - 16384, 16384);
        const double reduction = 20.0 * std::log10 ((outRms + 1.0e-12) / (inRms + 1.0e-12));

        std::printf ("compressor: %.1f dB level change, %.1f dB reported reduction\n",
                     reduction, compressor.getGainReductionDb());

        check (reduction < -4.0, "compressor reduces a signal above threshold");
        check (compressor.getGainReductionDb() > 4.0f, "compressor reports gain reduction");
    }

    //==========================================================================
    {
        kchannel::CompressorSettings settings;
        settings.thresholdDb = 0.0f;
        settings.ratio = 4.0f;
        settings.attackMs = 5.0f;
        settings.releaseMs = 100.0f;
        settings.makeupDb = 0.0f;

        kchannel::Compressor compressor;
        compressor.prepare (48000.0);
        compressor.setSettings (settings);

        const int length = 48000;
        std::vector<float> input ((size_t) length), output ((size_t) length);

        for (int i = 0; i < length; ++i)
        {
            input[(size_t) i] = (float) (0.02 * std::sin (2.0 * pi * 220.0 * (double) i / 48000.0));
            output[(size_t) i] = input[(size_t) i];
        }

        float* channels[] = { output.data() };
        compressor.process (channels, 1, length);

        const double inRms = rms (input, 1024, length - 1024);
        const double outRms = rms (output, 1024, length - 1024);
        const double change = 20.0 * std::log10 ((outRms + 1.0e-12) / (inRms + 1.0e-12));

        std::printf ("compressor below threshold: %.3f dB change\n", change);
        check (std::abs (change) < 0.1, "compressor is transparent below threshold");
    }

    //==========================================================================
    {
        kchannel::GateSettings settings;
        settings.thresholdDb = -30.0f;
        settings.rangeDb = 40.0f;
        settings.attackMs = 1.0f;
        settings.releaseMs = 100.0f;

        kchannel::Gate gate;
        gate.prepare (48000.0);
        gate.setSettings (settings);

        const int length = 48000;
        std::vector<float> input ((size_t) length), output ((size_t) length);

        for (int i = 0; i < length; ++i)
        {
            input[(size_t) i] = (float) (0.001 * std::sin (2.0 * pi * 220.0 * (double) i / 48000.0));
            output[(size_t) i] = input[(size_t) i];
        }

        float* channels[] = { output.data() };
        gate.process (channels, 1, length);

        const double inRms = rms (input, length - 16384, 16384);
        const double outRms = rms (output, length - 16384, 16384);
        const double reduction = 20.0 * std::log10 ((outRms + 1.0e-12) / (inRms + 1.0e-12));

        std::printf ("gate below threshold: %.1f dB, reported %.1f dB\n",
                     reduction, gate.getGainReductionDb());

        check (reduction < -25.0, "gate attenuates a signal below threshold");
        check (gate.getGainReductionDb() > 25.0f, "gate reports gain reduction");
    }

    //==========================================================================
    {
        kchannel::EqSettings eq;
        eq.eqOn = true;
        eq.filtersOn = true;
        eq.hpfHz = 20.0f;
        eq.lpfHz = 20000.0f;
        eq.lfFreq = 80.0f;
        eq.lfGain = 15.0f;
        eq.lmFreq = 400.0f;
        eq.lmGain = 15.0f;
        eq.lmQ = 4.0f;
        eq.hmFreq = 3000.0f;
        eq.hmGain = 15.0f;
        eq.hmQ = 4.0f;
        eq.hfFreq = 12000.0f;
        eq.hfGain = 15.0f;

        kchannel::Equalizer equalizer;
        equalizer.prepare (96000.0, 2);
        equalizer.setSettings (eq);

        const int length = 96000;
        std::vector<std::vector<float>> data (2, std::vector<float> ((size_t) length));

        std::mt19937 rng (7);
        std::uniform_real_distribution<float> dist (-0.2f, 0.2f);

        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < length; ++i)
                data[(size_t) ch][(size_t) i] = dist (rng);

        float* channels[] = { data[0].data(), data[1].data() };
        equalizer.process (channels, 2, length);

        bool finite = true;

        for (const auto& channel : data)
            for (float v : channel)
                finite = finite && std::isfinite (v);

        check (finite, "extreme EQ settings stay finite at 96 kHz");
    }

    std::printf ("\n%s\n", failures == 0 ? "ALL CHECKS PASSED" : "FAILURES PRESENT");
    return failures == 0 ? 0 : 1;
}
