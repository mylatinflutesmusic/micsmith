#pragma once

#include <algorithm>
#include <cmath>

namespace kchannel
{
constexpr double pi = 3.14159265358979323846;

/**
    A single RBJ biquad, run in transposed direct form II with double-precision
    state. The channel strip uses six of these per channel: high-pass, low-pass,
    low shelf, low-mid bell, high-mid bell and high shelf.
*/
struct Biquad
{
    double b0 = 1.0, b1 = 0.0, b2 = 0.0;
    double a1 = 0.0, a2 = 0.0;

    double z1 = 0.0, z2 = 0.0;

    void reset() noexcept { z1 = z2 = 0.0; }

    void setIdentity() noexcept
    {
        b0 = 1.0; b1 = b2 = a1 = a2 = 0.0;
    }

    static Biquad highPass (double sampleRate, double hz, double q = 0.7071067811865476);
    static Biquad lowPass  (double sampleRate, double hz, double q = 0.7071067811865476);
    static Biquad lowShelf (double sampleRate, double hz, double gainDb, double slope = 1.0);
    static Biquad highShelf(double sampleRate, double hz, double gainDb, double slope = 1.0);
    static Biquad peaking  (double sampleRate, double hz, double gainDb, double q);

    inline float process (float x) noexcept
    {
        const double input = (double) x;
        const double y = b0 * input + z1;

        z1 = b1 * input - a1 * y + z2;
        z2 = b2 * input - a2 * y;

        return (float) y;
    }

    /** Magnitude response of this section at a frequency, in dB. */
    double magnitudeDb (double hz, double sampleRate) const noexcept;
};

struct EqSettings
{
    bool  filtersOn = false;
    float hpfHz     = 80.0f;
    float lpfHz     = 20000.0f;

    bool  eqOn      = true;
    float lfFreq    = 80.0f;
    float lfGain    = 0.0f;
    float lmFreq    = 350.0f;
    float lmGain    = 0.0f;
    float lmQ       = 0.70f;
    float hmFreq    = 3000.0f;
    float hmGain    = 0.0f;
    float hmQ       = 0.70f;
    float hfFreq    = 10000.0f;
    float hfGain    = 0.0f;
};

struct CompressorSettings
{
    float thresholdDb = -18.0f;
    float ratio       = 2.5f;
    float attackMs    = 10.0f;
    float releaseMs   = 200.0f;
    float makeupDb    = 0.0f;
    float kneeDb      = 6.0f;
};

struct GateSettings
{
    float thresholdDb = -45.0f;
    float rangeDb     = 30.0f;
    float attackMs    = 1.0f;
    float releaseMs   = 150.0f;
};

/** Combined response of the enabled filter/EQ blocks, in dB. */
void eqResponseDb (const EqSettings& settings, double sampleRate,
                   const double* frequenciesHz, float* responseDb, int numPoints) noexcept;

/** The channel strip's filter and equalizer section. */
class Equalizer
{
public:
    static constexpr int numBands = 6;

    void prepare (double sampleRate, int channels);
    void reset();

    void setSettings (const EqSettings&);
    void process (float* const* channels, int numChannels, int numSamples) noexcept;

private:
    void resetBand (int band);

    double sampleRate = 48000.0;
    int activeChannels = 2;

    EqSettings current;
    Biquad filters[2][numBands];
};

/** Feed-forward RMS compressor with a soft knee and linked stereo detection. */
class Compressor
{
public:
    void prepare (double sampleRate);
    void reset();

    void setSettings (const CompressorSettings&);
    void process (float* const* channels, int numChannels, int numSamples) noexcept;

    float getGainReductionDb() const noexcept { return gainReductionDb; }

private:
    void updateCoefficients();

    double sampleRate = 48000.0;
    CompressorSettings settings;

    double attackCoeff = 0.01;
    double releaseCoeff = 0.001;
    double rms = 0.0;
    double gainDb = 0.0;
    float gainReductionDb = 0.0f;
};

/** Peak-detected downward gate with linked stereo detection. */
class Gate
{
public:
    void prepare (double sampleRate);
    void reset();

    void setSettings (const GateSettings&);
    void process (float* const* channels, int numChannels, int numSamples) noexcept;

    float getGainReductionDb() const noexcept { return gainReductionDb; }

private:
    void updateCoefficients();

    double sampleRate = 48000.0;
    GateSettings settings;

    double attackCoeff = 0.01;
    double releaseCoeff = 0.001;
    double envelope = 0.0;
    double gain = 1.0;
    float gainReductionDb = 0.0f;
};
} // namespace kchannel
