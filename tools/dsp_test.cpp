// Numeric checks for the microphone modelling maths. Deliberately JUCE-free so
// it can be built and run without a plugin host.

#include "../Source/dsp/ModelEngine.h"

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

struct Chain
{
    micsmith::Biquad biquads[micsmith::kMaxBands];
    int count = 0;

    void build (const micsmith::ModelParams& p, double sr)
    {
        micsmith::Band bands[micsmith::kMaxBands];
        count = micsmith::buildBands (p, sr, bands, micsmith::kMaxBands);

        for (int i = 0; i < count; ++i)
        {
            biquads[i].c = micsmith::makeCoeffs (bands[i], sr);
            biquads[i].reset();
        }
    }

    double process (double x)
    {
        for (int i = 0; i < count; ++i)
            x = biquads[i].process (x);

        return x;
    }
};

micsmith::ModelParams makePair (int source, int target,
                                float proximity = 0.0f, float axis = 0.0f,
                                float lowCut = 10.0f, float highCut = 21000.0f)
{
    micsmith::ModelParams p;
    p.sourceIndex = source;
    p.targetIndex = target;
    p.proximity = proximity;
    p.axis = axis;
    p.lowCutHz = lowCut;
    p.highCutHz = highCut;
    return p;
}
} // namespace

int main()
{
    const double rates[] = { 44100.0, 48000.0, 96000.0 };
    const double freqs[] = { 20.0, 50.0, 100.0, 200.0, 500.0, 1000.0,
                             2000.0, 4000.0, 8000.0, 12000.0, 16000.0, 20000.0 };
    const int numFreqs = (int) (sizeof (freqs) / sizeof (freqs[0]));

    std::printf ("MicSmith DSP checks\n===================\n\n");

    //==========================================================================
    // 1. Source == target must be an exact null: the correction is the exact
    //    inverse of the source model.
    {
        double worst = 0.0;

        for (double sr : rates)
        {
            for (int i = 0; i < micsmith::numMics(); ++i)
            {
                const auto p = makePair (i, i);

                for (int f = 0; f < numFreqs; ++f)
                    worst = std::max (worst, std::abs (micsmith::pairResponseDb (p, sr, freqs[f])));
            }
        }

        std::printf ("worst |dB| for source == target: %.3e\n", worst);
        check (worst < 1.0e-9, "source == target is a perfect null at 44.1/48/96 kHz");
    }

    //==========================================================================
    // 2. Every cascade must be stable for every pair and extreme control setting.
    {
        bool stable = true;
        int checked = 0;

        for (double sr : rates)
        {
            for (int s = 0; s < micsmith::numMics(); ++s)
            {
                for (int t = 0; t < micsmith::numMics(); ++t)
                {
                    for (int extreme = 0; extreme < 4; ++extreme)
                    {
                        const auto p = makePair (s, t,
                                                 (extreme & 1) ? 1.0f : 0.0f,
                                                 (extreme & 2) ? 1.0f : 0.0f,
                                                 (extreme & 1) ? 400.0f : 10.0f,
                                                 (extreme & 2) ? 2000.0f : 21000.0f);

                        micsmith::Band bands[micsmith::kMaxBands];
                        const int n = micsmith::buildBands (p, sr, bands, micsmith::kMaxBands);

                        for (int i = 0; i < n; ++i)
                            if (! micsmith::isStable (micsmith::makeCoeffs (bands[i], sr)))
                                stable = false;

                        ++checked;
                    }
                }
            }
        }

        std::printf ("cascades checked: %d\n", checked);
        check (stable, "all biquads are stable (poles inside the unit circle)");
    }

    //==========================================================================
    // 3. Processing must stay finite and decay: run noise and an impulse.
    {
        std::mt19937 rng (12345);
        std::uniform_real_distribution<double> dist (-1.0, 1.0);

        bool finite = true;
        double worstPeak = 0.0;
        double worstTail = 0.0;

        for (int s = 0; s < micsmith::numMics(); ++s)
        {
            for (int t = 0; t < micsmith::numMics(); ++t)
            {
                const auto p = makePair (s, t, 1.0f, 1.0f, 20.0f, 20000.0f);
                Chain chain;
                chain.build (p, 48000.0);

                double peak = 0.0;

                for (int i = 0; i < 96000; ++i)
                {
                    const double y = chain.process (dist (rng));

                    if (! std::isfinite (y))
                        finite = false;

                    peak = std::max (peak, std::abs (y));
                }

                worstPeak = std::max (worstPeak, peak);

                // Impulse response tail must have died away after ~1 s.
                Chain impulseChain;
                impulseChain.build (p, 48000.0);

                double tail = 0.0;
                double impulsePeak = 0.0;

                for (int i = 0; i < 48000; ++i)
                {
                    const double y = impulseChain.process (i == 0 ? 1.0 : 0.0);
                    impulsePeak = std::max (impulsePeak, std::abs (y));

                    if (i > 43000)
                        tail = std::max (tail, std::abs (y));
                }

                if (! std::isfinite (impulsePeak) || ! std::isfinite (tail))
                    finite = false;

                worstTail = std::max (worstTail, tail / std::max (impulsePeak, 1.0e-12));
            }
        }

        std::printf ("worst peak with noise input: %.3f   worst normalised impulse tail: %.3e\n",
                     worstPeak, worstTail);

        check (finite, "output stays finite (no NaN/Inf) for all pairs");
        check (worstTail < 1.0e-3, "impulse responses decay (no self-oscillation)");
    }

    //==========================================================================
    // 4. Report card: each model's colouration relative to the neutral reference.
    {
        std::printf ("\n%-24s %8s %8s %8s\n", "model", "100 Hz", "1 kHz", "10 kHz");
        std::printf ("%-24s %8s %8s %8s\n", "------------------------", "------", "-----", "------");

        for (int i = 0; i < micsmith::numMics(); ++i)
        {
            const auto p = makePair (0, i);
            std::printf ("%-24s %+7.1f  %+7.1f  %+7.1f\n",
                         micsmith::micModels()[(size_t) i].name,
                         micsmith::pairResponseDb (p, 48000.0, 100.0),
                         micsmith::pairResponseDb (p, 48000.0, 1000.0),
                         micsmith::pairResponseDb (p, 48000.0, 10000.0));
        }
    }

    //==========================================================================
    // 5. Auto-gain must be bounded.
    {
        bool bounded = true;

        for (int s = 0; s < micsmith::numMics(); ++s)
            for (int t = 0; t < micsmith::numMics(); ++t)
            {
                const double g = micsmith::autoGainDb (makePair (s, t, 1.0f, 0.0f), 48000.0);

                if (! std::isfinite (g) || std::abs (g) > 18.0)
                    bounded = false;
            }

        check (bounded, "auto gain is finite and within +/-18 dB");
    }

    std::printf ("\n%s\n", failures == 0 ? "ALL CHECKS PASSED" : "FAILURES PRESENT");
    return failures == 0 ? 0 : 1;
}
