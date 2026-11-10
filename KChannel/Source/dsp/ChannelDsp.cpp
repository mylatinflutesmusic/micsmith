#include "ChannelDsp.h"

#include <algorithm>
#include <cmath>

namespace kchannel
{
namespace
{
    inline double safeFrequency (double hz, double sampleRate) noexcept
    {
        return std::min (std::max (hz, 10.0), sampleRate * 0.49);
    }

    inline double safeQ (double q) noexcept
    {
        return std::min (std::max (q, 0.05), 20.0);
    }

    inline double millisecondsToCoefficient (double milliseconds, double sampleRate) noexcept
    {
        const double seconds = std::max (0.01, milliseconds) * 0.001;
        return 1.0 - std::exp (-1.0 / (seconds * std::max (1.0, sampleRate)));
    }

    inline double decibelsToGain (double db) noexcept
    {
        return std::pow (10.0, db / 20.0);
    }
}

Biquad Biquad::highPass (double sampleRate, double hz, double q)
{
    Biquad b;
    const double w0 = 2.0 * pi * safeFrequency (hz, sampleRate) / sampleRate;
    const double cw = std::cos (w0);
    const double alpha = std::sin (w0) / (2.0 * safeQ (q));
    const double a0 = 1.0 + alpha;

    b.b0 = (1.0 + cw) * 0.5 / a0;
    b.b1 = -(1.0 + cw) / a0;
    b.b2 = (1.0 + cw) * 0.5 / a0;
    b.a1 = -2.0 * cw / a0;
    b.a2 = (1.0 - alpha) / a0;
    return b;
}

Biquad Biquad::lowPass (double sampleRate, double hz, double q)
{
    Biquad b;
    const double w0 = 2.0 * pi * safeFrequency (hz, sampleRate) / sampleRate;
    const double cw = std::cos (w0);
    const double alpha = std::sin (w0) / (2.0 * safeQ (q));
    const double a0 = 1.0 + alpha;

    b.b0 = (1.0 - cw) * 0.5 / a0;
    b.b1 = (1.0 - cw) / a0;
    b.b2 = (1.0 - cw) * 0.5 / a0;
    b.a1 = -2.0 * cw / a0;
    b.a2 = (1.0 - alpha) / a0;
    return b;
}

Biquad Biquad::lowShelf (double sampleRate, double hz, double gainDb, double slope)
{
    Biquad b;
    const double A = std::pow (10.0, gainDb / 40.0);
    const double w0 = 2.0 * pi * safeFrequency (hz, sampleRate) / sampleRate;
    const double cw = std::cos (w0);
    const double sw = std::sin (w0);
    const double S = std::max (0.1, slope);
    const double alpha = sw * 0.5 * std::sqrt (std::max (0.0, (A + 1.0 / A) * (1.0 / S - 1.0) + 2.0));
    const double beta = 2.0 * std::sqrt (A) * alpha;
    const double a0 = (A + 1.0) + (A - 1.0) * cw + beta;

    b.b0 = A * ((A + 1.0) - (A - 1.0) * cw + beta) / a0;
    b.b1 = 2.0 * A * ((A - 1.0) - (A + 1.0) * cw) / a0;
    b.b2 = A * ((A + 1.0) - (A - 1.0) * cw - beta) / a0;
    b.a1 = -2.0 * ((A - 1.0) + (A + 1.0) * cw) / a0;
    b.a2 = ((A + 1.0) + (A - 1.0) * cw - beta) / a0;
    return b;
}

Biquad Biquad::highShelf (double sampleRate, double hz, double gainDb, double slope)
{
    Biquad b;
    const double A = std::pow (10.0, gainDb / 40.0);
    const double w0 = 2.0 * pi * safeFrequency (hz, sampleRate) / sampleRate;
    const double cw = std::cos (w0);
    const double sw = std::sin (w0);
    const double S = std::max (0.1, slope);
    const double alpha = sw * 0.5 * std::sqrt (std::max (0.0, (A + 1.0 / A) * (1.0 / S - 1.0) + 2.0));
    const double beta = 2.0 * std::sqrt (A) * alpha;
    const double a0 = (A + 1.0) - (A - 1.0) * cw + beta;

    b.b0 = A * ((A + 1.0) + (A - 1.0) * cw + beta) / a0;
    b.b1 = -2.0 * A * ((A - 1.0) + (A + 1.0) * cw) / a0;
    b.b2 = A * ((A + 1.0) + (A - 1.0) * cw - beta) / a0;
    b.a1 = 2.0 * ((A - 1.0) - (A + 1.0) * cw) / a0;
    b.a2 = ((A + 1.0) - (A - 1.0) * cw - beta) / a0;
    return b;
}

Biquad Biquad::peaking (double sampleRate, double hz, double gainDb, double q)
{
    Biquad b;
    const double A = std::pow (10.0, gainDb / 40.0);
    const double w0 = 2.0 * pi * safeFrequency (hz, sampleRate) / sampleRate;
    const double cw = std::cos (w0);
    const double alpha = std::sin (w0) / (2.0 * safeQ (q));
    const double a0 = 1.0 + alpha / A;

    b.b0 = (1.0 + alpha * A) / a0;
    b.b1 = -2.0 * cw / a0;
    b.b2 = (1.0 - alpha * A) / a0;
    b.a1 = -2.0 * cw / a0;
    b.a2 = (1.0 - alpha / A) / a0;
    return b;
}

double Biquad::magnitudeDb (double hz, double sampleRate) const noexcept
{
    const double w = 2.0 * pi * std::min (std::max (hz, 0.0), sampleRate * 0.499) / sampleRate;
    const double cw = std::cos (w);
    const double sw = std::sin (w);
    const double c2 = std::cos (2.0 * w);
    const double s2 = std::sin (2.0 * w);

    const double numR = b0 + b1 * cw + b2 * c2;
    const double numI = -(b1 * sw + b2 * s2);
    const double denR = 1.0 + a1 * cw + a2 * c2;
    const double denI = -(a1 * sw + a2 * s2);

    const double num = numR * numR + numI * numI;
    const double den = std::max (1.0e-30, denR * denR + denI * denI);

    return 10.0 * std::log10 (std::max (1.0e-30, num / den));
}

void eqResponseDb (const EqSettings& settings, double sampleRate,
                   const double* frequenciesHz, float* responseDb, int numPoints) noexcept
{
    if (frequenciesHz == nullptr || responseDb == nullptr || numPoints <= 0)
        return;

    Biquad bands[6];

    if (settings.filtersOn)
    {
        bands[0] = Biquad::highPass (sampleRate, (double) settings.hpfHz);
        bands[1] = Biquad::lowPass (sampleRate, (double) settings.lpfHz);
    }

    if (settings.eqOn)
    {
        bands[2] = Biquad::lowShelf (sampleRate, (double) settings.lfFreq, (double) settings.lfGain);
        bands[3] = Biquad::peaking (sampleRate, (double) settings.lmFreq, (double) settings.lmGain,
                                    (double) settings.lmQ);
        bands[4] = Biquad::peaking (sampleRate, (double) settings.hmFreq, (double) settings.hmGain,
                                    (double) settings.hmQ);
        bands[5] = Biquad::highShelf (sampleRate, (double) settings.hfFreq, (double) settings.hfGain);
    }

    for (int i = 0; i < numPoints; ++i)
    {
        double db = 0.0;

        if (settings.filtersOn)
            db += bands[0].magnitudeDb (frequenciesHz[i], sampleRate)
                + bands[1].magnitudeDb (frequenciesHz[i], sampleRate);

        if (settings.eqOn)
            for (int b = 2; b < 6; ++b)
                db += bands[b].magnitudeDb (frequenciesHz[i], sampleRate);

        responseDb[i] = (float) db;
    }
}

void Equalizer::prepare (double newSampleRate, int channels)
{
    sampleRate = newSampleRate > 0.0 ? newSampleRate : 48000.0;
    activeChannels = std::max (1, std::min (channels, 2));
    current = EqSettings {};
    reset();
}

void Equalizer::reset()
{
    for (auto& channel : filters)
        for (auto& filter : channel)
            filter.reset();
}

void Equalizer::resetBand (int band)
{
    for (int ch = 0; ch < 2; ++ch)
        filters[ch][band].reset();
}

void Equalizer::setSettings (const EqSettings& settings)
{
    if (current.filtersOn != settings.filtersOn)
    {
        resetBand (0);
        resetBand (1);
    }

    if (current.eqOn != settings.eqOn)
    {
        for (int b = 2; b < numBands; ++b)
            resetBand (b);
    }

    current = settings;

    Biquad coefficients[numBands];

    if (settings.filtersOn)
    {
        coefficients[0] = Biquad::highPass (sampleRate, (double) settings.hpfHz);
        coefficients[1] = Biquad::lowPass (sampleRate, (double) settings.lpfHz);
    }

    if (settings.eqOn)
    {
        coefficients[2] = Biquad::lowShelf (sampleRate, (double) settings.lfFreq, (double) settings.lfGain);
        coefficients[3] = Biquad::peaking (sampleRate, (double) settings.lmFreq, (double) settings.lmGain,
                                           (double) settings.lmQ);
        coefficients[4] = Biquad::peaking (sampleRate, (double) settings.hmFreq, (double) settings.hmGain,
                                           (double) settings.hmQ);
        coefficients[5] = Biquad::highShelf (sampleRate, (double) settings.hfFreq, (double) settings.hfGain);
    }

    for (int ch = 0; ch < activeChannels; ++ch)
    {
        for (int b = 0; b < numBands; ++b)
        {
            filters[ch][b].b0 = coefficients[b].b0;
            filters[ch][b].b1 = coefficients[b].b1;
            filters[ch][b].b2 = coefficients[b].b2;
            filters[ch][b].a1 = coefficients[b].a1;
            filters[ch][b].a2 = coefficients[b].a2;
        }
    }
}

void Equalizer::process (float* const* channels, int numChannels, int numSamples) noexcept
{
    if (channels == nullptr || numSamples <= 0)
        return;

    const int count = std::min (numChannels, activeChannels);

    for (int ch = 0; ch < count; ++ch)
    {
        auto* data = channels[ch];

        if (data == nullptr)
            continue;

        for (int i = 0; i < numSamples; ++i)
        {
            float x = data[i];

            if (current.filtersOn)
            {
                x = filters[ch][0].process (x);
                x = filters[ch][1].process (x);
            }

            if (current.eqOn)
            {
                x = filters[ch][2].process (x);
                x = filters[ch][3].process (x);
                x = filters[ch][4].process (x);
                x = filters[ch][5].process (x);
            }

            data[i] = x;
        }
    }
}

void Compressor::prepare (double newSampleRate)
{
    sampleRate = newSampleRate > 0.0 ? newSampleRate : 48000.0;
    updateCoefficients();
    reset();
}

void Compressor::reset()
{
    rms = 0.0;
    gainDb = 0.0;
    gainReductionDb = 0.0f;
}

void Compressor::updateCoefficients()
{
    attackCoeff = millisecondsToCoefficient ((double) settings.attackMs, sampleRate);
    releaseCoeff = millisecondsToCoefficient ((double) settings.releaseMs, sampleRate);
}

void Compressor::setSettings (const CompressorSettings& newSettings)
{
    settings = newSettings;
    updateCoefficients();
}

void Compressor::process (float* const* channels, int numChannels, int numSamples) noexcept
{
    if (channels == nullptr || numSamples <= 0 || numChannels <= 0)
        return;

    const double threshold = (double) settings.thresholdDb;
    const double ratio = std::max (1.0, (double) settings.ratio);
    const double knee = std::max (0.01, (double) settings.kneeDb);
    const double makeup = (double) settings.makeupDb;

    for (int i = 0; i < numSamples; ++i)
    {
        double sumSquares = 0.0;

        for (int ch = 0; ch < numChannels; ++ch)
        {
            const double x = (double) channels[ch][i];
            sumSquares += x * x;
        }

        const double power = sumSquares / (double) numChannels;
        const double detectorCoeff = power > rms ? attackCoeff : releaseCoeff;
        rms += (power - rms) * detectorCoeff;

        const double levelDb = 10.0 * std::log10 (rms + 1.0e-12);
        const double over = levelDb - threshold;

        double targetReductionDb = 0.0;

        if (over > knee * 0.5)
        {
            targetReductionDb = over * (1.0 - 1.0 / ratio);
        }
        else if (over > -knee * 0.5)
        {
            const double t = (over + knee * 0.5) / knee;
            targetReductionDb = (1.0 - 1.0 / ratio) * over * t;
        }

        const double gainCoeff = targetReductionDb > gainDb ? attackCoeff : releaseCoeff;
        gainDb += (targetReductionDb - gainDb) * gainCoeff;

        if (std::abs (gainDb) < 1.0e-10)
            gainDb = 0.0;

        const double gain = decibelsToGain (-gainDb + makeup);

        for (int ch = 0; ch < numChannels; ++ch)
            channels[ch][i] = (float) ((double) channels[ch][i] * gain);

        gainReductionDb = (float) gainDb;
    }
}

void Gate::prepare (double newSampleRate)
{
    sampleRate = newSampleRate > 0.0 ? newSampleRate : 48000.0;
    updateCoefficients();
    reset();
}

void Gate::reset()
{
    envelope = 0.0;
    gain = 1.0;
    gainReductionDb = 0.0f;
}

void Gate::updateCoefficients()
{
    attackCoeff = millisecondsToCoefficient ((double) settings.attackMs, sampleRate);
    releaseCoeff = millisecondsToCoefficient ((double) settings.releaseMs, sampleRate);
}

void Gate::setSettings (const GateSettings& newSettings)
{
    settings = newSettings;
    updateCoefficients();
}

void Gate::process (float* const* channels, int numChannels, int numSamples) noexcept
{
    if (channels == nullptr || numSamples <= 0 || numChannels <= 0)
        return;

    const double floorGain = decibelsToGain (-(double) settings.rangeDb);

    for (int i = 0; i < numSamples; ++i)
    {
        double peak = 0.0;

        for (int ch = 0; ch < numChannels; ++ch)
            peak = std::max (peak, std::abs ((double) channels[ch][i]));

        const double detectorCoeff = peak > envelope ? attackCoeff : releaseCoeff;
        envelope += (peak - envelope) * detectorCoeff;

        const double levelDb = 20.0 * std::log10 (envelope + 1.0e-12);

        // A little hysteresis stops the gate chattering on a signal hovering at
        // the threshold. Once open, it closes only 2 dB further down.
        const double threshold = (double) settings.thresholdDb + (gain > 0.5 ? -2.0 : 0.0);
        const double targetGain = levelDb > threshold ? 1.0 : floorGain;
        const double gainCoeff = targetGain > gain ? attackCoeff : releaseCoeff;
        gain += (targetGain - gain) * gainCoeff;

        if (std::abs (gain - 1.0) < 1.0e-7)
            gain = 1.0;

        for (int ch = 0; ch < numChannels; ++ch)
            channels[ch][i] = (float) ((double) channels[ch][i] * gain);

        gainReductionDb = (float) (-20.0 * std::log10 (std::max (1.0e-6, gain)));
    }
}
} // namespace kchannel
