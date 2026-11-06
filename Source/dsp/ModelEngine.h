#pragma once

#include <cmath>
#include <cstddef>

#include "MicModels.h"

namespace micsmith
{
/** Upper bound on the number of biquads in a cascade. */
constexpr int kMaxBands = 24;

/** Everything that shapes the transfer function at a given instant. */
struct ModelParams
{
    int   sourceIndex = 0;
    int   targetIndex = 1;
    float proximity   = 0.25f;   // 0..1
    float axis        = 0.0f;    // 0..1  (0 = on-axis, 1 = fully off-axis)
    float lowCutHz    = 20.0f;
    float highCutHz   = 20000.0f;
};

inline double clampd (double v, double lo, double hi) noexcept
{
    return v < lo ? lo : (v > hi ? hi : v);
}

inline int clampIndex (int i, int size) noexcept
{
    if (size <= 0) return 0;
    if (i < 0) return 0;
    if (i >= size) return size - 1;
    return i;
}

/**
    Builds the full cascade for a source/target pair.

    Order matters for nothing mathematically (LTI), but it reads top-to-bottom:
    the target microphone's own colouration, then the exact inverse of the
    source microphone, then the physical controls, then the user filters.
*/
inline int buildBands (const ModelParams& p, double sampleRate, Band* out, int maxBands)
{
    (void) sampleRate; // coefficients are clamped at makeCoeffs() time

    int n = 0;
    auto push = [&] (Band b)
    {
        if (n < maxBands)
            out[n++] = b;
    };

    const auto& mics = micModels();
    const int src = clampIndex (p.sourceIndex, (int) mics.size());
    const int tgt = clampIndex (p.targetIndex, (int) mics.size());

    for (const auto& b : mics[(size_t) tgt].bands)
        push (b);

    // Inverting the source: peak and shelf bands are exactly symmetric under
    // A -> 1/A, so negating the gain gives the stable, exact inverse.
    for (const auto& b : mics[(size_t) src].bands)
        push ({ b.type, b.freq, b.q, -b.gainDb });

    // Proximity effect: pressure build-up at close working distance.
    const double prox = clampd (p.proximity, 0.0, 1.0);
    if (prox > 0.001)
    {
        push ({ BandType::LowShelf, 140.0, 0.62,  7.0 * prox });
        push ({ BandType::Peak,     380.0, 0.90,  2.5 * prox });
    }

    // Moving off-axis loses top end and notches the upper midrange.
    const double ax = clampd (p.axis, 0.0, 1.0);
    if (ax > 0.001)
    {
        push ({ BandType::HighShelf, 4500.0, 0.70, -9.0 * ax });
        push ({ BandType::Peak,      2600.0, 1.10, -3.0 * ax });
    }

    if (p.lowCutHz > 25.0f)
        push ({ BandType::HighPass, (double) p.lowCutHz, 0.707, 0.0 });

    if (p.highCutHz < 19500.0f)
        push ({ BandType::LowPass, (double) p.highCutHz, 0.707, 0.0 });

    return n;
}

/** Precomputes a cascade once so many frequencies can be evaluated cheaply. */
struct CurveEvaluator
{
    Coeffs coeffs[kMaxBands];
    int    count = 0;
    double sr    = 48000.0;

    void build (const ModelParams& p, double sampleRate)
    {
        sr = sampleRate > 0.0 ? sampleRate : 48000.0;

        Band bands[kMaxBands];
        count = buildBands (p, sr, bands, kMaxBands);

        for (int i = 0; i < count; ++i)
            coeffs[i] = makeCoeffs (bands[i], sr);
    }

    /** Builds the raw response of a single microphone model (no pair correction). */
    void buildSingleModel (int modelIndex, double sampleRate)
    {
        sr = sampleRate > 0.0 ? sampleRate : 48000.0;
        count = 0;

        const auto& mics = micModels();
        const int idx = clampIndex (modelIndex, (int) mics.size());

        for (const auto& b : mics[(size_t) idx].bands)
            if (count < kMaxBands)
                coeffs[count++] = makeCoeffs (b, sr);
    }

    double magnitudeAt (double freq) const
    {
        double mag = 1.0;
        for (int i = 0; i < count; ++i)
            mag *= micsmith::magnitudeAt (coeffs[i], freq, sr);
        return mag;
    }

    double dbAt (double freq) const
    {
        const double mag = magnitudeAt (freq);
        return 20.0 * std::log10 (mag > 1.0e-12 ? mag : 1.0e-12);
    }
};

inline double pairResponseDb (const ModelParams& p, double sampleRate, double freq)
{
    CurveEvaluator ev;
    ev.build (p, sampleRate);
    return ev.dbAt (freq);
}

/**
    Broadband loudness compensation for the current pair, so switching models
    does not jump in level. Averaged over the speech/music band, clamped so a
    deliberately extreme setting cannot blow up the output.
*/
inline double autoGainDb (const ModelParams& p, double sampleRate)
{
    static const double probeHz[] = { 80.0, 160.0, 250.0, 400.0, 630.0, 1000.0,
                                      1600.0, 2500.0, 4000.0, 6300.0, 8000.0, 12000.0 };

    CurveEvaluator ev;
    ev.build (p, sampleRate);

    double sum = 0.0;
    int count = 0;
    for (double f : probeHz)
    {
        sum += ev.dbAt (f);
        ++count;
    }

    return clampd (-sum / (double) count, -18.0, 18.0);
}
} // namespace micsmith
