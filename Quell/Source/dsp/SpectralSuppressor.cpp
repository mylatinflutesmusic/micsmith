#include "SpectralSuppressor.h"

#include <algorithm>
#include <cmath>

namespace quell
{
namespace
{
    constexpr float twoPi = 6.28318530717958647692f;

    inline float smoothstep (float edge0, float edge1, float x) noexcept
    {
        const float t = std::min (1.0f, std::max (0.0f, (x - edge0) / (edge1 - edge0)));
        return t * t * (3.0f - 2.0f * t);
    }
}

SpectralSuppressor::SpectralSuppressor() = default;
SpectralSuppressor::~SpectralSuppressor() = default;

void SpectralSuppressor::prepare (double newSampleRate, int numChannels)
{
    sampleRate = newSampleRate > 0.0 ? newSampleRate : 48000.0;
    activeChannels = std::max (1, std::min (numChannels, maxChannels));

    // Keep roughly constant frequency resolution across sample rates while
    // keeping the latency as low as the technique allows.
    if (sampleRate <= 64000.0)
        fftSize = 1024;
    else if (sampleRate <= 128000.0)
        fftSize = 2048;
    else
        fftSize = 4096;

    hop = fftSize / 4;
    numBins = fftSize / 2 + 1;

    fft = std::make_unique<Fft> (fftSize);

    window.resize ((size_t) fftSize);

    for (int i = 0; i < fftSize; ++i)
        window[(size_t) i] = 0.5f - 0.5f * std::cos (twoPi * (float) i / (float) fftSize);

    // Periodic Hann analysis and synthesis windows at 75% overlap sum to 1.5.
    olaScale = 1.0f / 1.5f;

    history.assign ((size_t) maxChannels, std::vector<float> ((size_t) fftSize, 0.0f));
    overlap.assign ((size_t) maxChannels, std::vector<float> ((size_t) fftSize, 0.0f));
    dryLine.assign ((size_t) maxChannels, std::vector<float> ((size_t) fftSize, 0.0f));
    fftRe.assign ((size_t) maxChannels, std::vector<float> ((size_t) fftSize, 0.0f));
    fftIm.assign ((size_t) maxChannels, std::vector<float> ((size_t) fftSize, 0.0f));
    sideHistory.assign ((size_t) maxChannels, std::vector<float> ((size_t) fftSize, 0.0f));
    sideRe.assign ((size_t) maxChannels, std::vector<float> ((size_t) fftSize, 0.0f));
    sideIm.assign ((size_t) maxChannels, std::vector<float> ((size_t) fftSize, 0.0f));

    mag.assign ((size_t) maxChannels, std::vector<float> ((size_t) numBins, 0.0f));
    db.assign ((size_t) maxChannels, std::vector<float> ((size_t) numBins, -110.0f));
    envelope.assign ((size_t) maxChannels, std::vector<float> ((size_t) numBins, -110.0f));
    cut.assign ((size_t) maxChannels, std::vector<float> ((size_t) numBins, 0.0f));
    gainDb.assign ((size_t) maxChannels, std::vector<float> ((size_t) numBins, 0.0f));

    gainLin.assign ((size_t) numBins, 1.0f);
    cutSmooth.assign ((size_t) numBins, 0.0f);

    mixCoefficient = 1.0f - std::exp (-1.0f / (0.02f * (float) sampleRate));

    rebuildDisplayMap();
    reset();
}

void SpectralSuppressor::reset()
{
    for (auto& h : history)
        std::fill (h.begin(), h.end(), 0.0f);

    for (auto& o : overlap)
        std::fill (o.begin(), o.end(), 0.0f);

    for (auto& d : dryLine)
        std::fill (d.begin(), d.end(), 0.0f);

    for (auto& g : gainDb)
        std::fill (g.begin(), g.end(), 0.0f);

    for (auto& m : mag)
        std::fill (m.begin(), m.end(), 0.0f);

    for (auto& d : db)
        std::fill (d.begin(), d.end(), -110.0f);

    for (auto& e : envelope)
        std::fill (e.begin(), e.end(), -110.0f);

    for (auto& c : cut)
        std::fill (c.begin(), c.end(), 0.0f);

    for (auto& r : fftRe)
        std::fill (r.begin(), r.end(), 0.0f);

    for (auto& i : fftIm)
        std::fill (i.begin(), i.end(), 0.0f);

    for (auto& s : sideHistory)
        std::fill (s.begin(), s.end(), 0.0f);

    for (auto& s : sideRe)
        std::fill (s.begin(), s.end(), 0.0f);

    for (auto& s : sideIm)
        std::fill (s.begin(), s.end(), 0.0f);

    sidechainActive = false;
    sidechainChannels = 0;

    historyPos = overlapPos = dryPos = hopCounter = 0;
    mixCurrent = mixTarget;
    maxCutDb = 0.0f;

    display = {};
}

void SpectralSuppressor::rebuildDisplayMap() noexcept
{
    displayLo.assign ((size_t) DisplaySnapshot::numPoints, 0);
    displayHi.assign ((size_t) DisplaySnapshot::numPoints, 1);

    const double minHz = 20.0;
    const double maxHz = std::min (20000.0, sampleRate * 0.49);
    const double binHz = sampleRate / (double) fftSize;

    for (int i = 0; i < DisplaySnapshot::numPoints; ++i)
    {
        const double t0 = (double) i / (double) DisplaySnapshot::numPoints;
        const double t1 = (double) (i + 1) / (double) DisplaySnapshot::numPoints;

        const double f0 = minHz * std::pow (maxHz / minHz, t0);
        const double f1 = minHz * std::pow (maxHz / minHz, t1);

        int lo = (int) std::floor (f0 / binHz);
        int hi = (int) std::ceil (f1 / binHz);

        lo = std::max (0, std::min (lo, numBins - 1));
        hi = std::max (lo + 1, std::min (hi, numBins));

        displayLo[(size_t) i] = lo;
        displayHi[(size_t) i] = hi;
    }
}

void SpectralSuppressor::computeDbAndEnvelope (int channel) noexcept
{
    const int bins = numBins;

    auto& d = db[(size_t) channel];
    auto& e = envelope[(size_t) channel];
    auto& c = cut[(size_t) channel];
    auto& m = mag[(size_t) channel];

    for (int b = 0; b < bins; ++b)
    {
        const float value = 20.0f * std::log10 (m[(size_t) b] + 1.0e-7f);
        d[(size_t) b] = std::max (value, -110.0f);
        e[(size_t) b] = d[(size_t) b];
        c[(size_t) b] = 0.0f;
    }

    // Zero-phase, fractional-octave smoothing. The bandwidth grows with the bin
    // index, so the envelope follows the broad spectral shape at every octave
    // rather than the individual FFT bins.
    const float width = 0.18f + (1.0f - settings.selectivity) * 0.55f;

    for (int b = 1; b < bins; ++b)
    {
        const float a = 1.0f / (1.0f + width * (float) b);
        e[(size_t) b] = e[(size_t) (b - 1)] + a * (e[(size_t) b] - e[(size_t) (b - 1)]);
    }

    for (int b = bins - 2; b >= 0; --b)
    {
        const float a = 1.0f / (1.0f + width * (float) b);
        e[(size_t) b] = e[(size_t) (b + 1)] + a * (e[(size_t) b] - e[(size_t) (b + 1)]);
    }

    const float threshold = 0.5f + std::pow (1.0f - settings.sensitivity, 1.5f) * 16.0f;
    const float depth = settings.depth;
    const float binHz = (float) (sampleRate / (double) fftSize);

    for (int b = 2; b < bins - 2; ++b)
    {
        const float excess = d[(size_t) b] - e[(size_t) b];

        if (excess <= threshold)
            continue;

        const float hz = binHz * (float) b;

        const float lowWeight  = 1.0f - settings.lowTame * (1.0f - smoothstep (120.0f, 700.0f, hz));
        const float highWeight = 1.0f - settings.highTame * smoothstep (5000.0f, 14000.0f, hz);

        const float amount = (excess - threshold) * depth * 1.1f * lowWeight * highWeight;
        c[(size_t) b] = std::min (amount, 36.0f);
    }

    // A light symmetric pass keeps the notches from landing on single bins,
    // which would ring and sound like spectral holes rather than smoothing.
    for (int b = 0; b < bins; ++b)
    {
        const float left  = b > 0 ? c[(size_t) (b - 1)] : c[(size_t) b];
        const float right = b < bins - 1 ? c[(size_t) (b + 1)] : c[(size_t) b];

        cutSmooth[(size_t) b] = 0.25f * left + 0.5f * c[(size_t) b] + 0.25f * right;
    }

    std::copy (cutSmooth.begin(), cutSmooth.end(), c.begin());
}

void SpectralSuppressor::processFrame() noexcept
{
    const int n = fftSize;
    const int bins = numBins;
    const int channels = activeChannels;

    if (channels <= 0 || fft == nullptr)
        return;

    for (int ch = 0; ch < channels; ++ch)
    {
        auto& re = fftRe[(size_t) ch];
        auto& im = fftIm[(size_t) ch];
        const auto& hist = history[(size_t) ch];

        for (int k = 0; k < n; ++k)
        {
            const int index = historyPos + k;
            re[(size_t) k] = hist[(size_t) (index >= n ? index - n : index)] * window[(size_t) k];
            im[(size_t) k] = 0.0f;
        }

        fft->forward (re.data(), im.data());

        auto& m = mag[(size_t) ch];

        if (sidechainActive)
        {
            auto& sr = sideRe[(size_t) ch];
            auto& si = sideIm[(size_t) ch];
            const auto& scHist = sideHistory[(size_t) ch];

            for (int k = 0; k < n; ++k)
            {
                const int index = historyPos + k;
                sr[(size_t) k] = scHist[(size_t) (index >= n ? index - n : index)] * window[(size_t) k];
                si[(size_t) k] = 0.0f;
            }

            fft->forward (sr.data(), si.data());

            for (int b = 0; b < bins; ++b)
                m[(size_t) b] = std::sqrt (sr[(size_t) b] * sr[(size_t) b]
                                           + si[(size_t) b] * si[(size_t) b]);
        }
        else
        {
            for (int b = 0; b < bins; ++b)
                m[(size_t) b] = std::sqrt (re[(size_t) b] * re[(size_t) b]
                                           + im[(size_t) b] * im[(size_t) b]);
        }

        computeDbAndEnvelope (ch);
    }

    // Stereo-linked detection keeps the image stable. The cut curves are
    // already smoothed, so taking the larger one is well behaved.
    if (settings.link && channels == 2)
    {
        for (int b = 0; b < bins; ++b)
        {
            const float linked = std::max (cut[0][(size_t) b], cut[1][(size_t) b]);
            cut[0][(size_t) b] = linked;
            cut[1][(size_t) b] = linked;
        }
    }

    const double frameRate = sampleRate / (double) hop;
    const float attackMs = 3.0f + (1.0f - settings.speed) * 30.0f;
    const float releaseMs = 60.0f + (1.0f - settings.speed) * 540.0f;

    const float attack = 1.0f - std::exp (-1.0f / (float) (attackMs * 0.001 * frameRate));
    const float release = 1.0f - std::exp (-1.0f / (float) (releaseMs * 0.001 * frameRate));

    float frameMaxCut = 0.0f;

    for (int ch = 0; ch < channels; ++ch)
    {
        auto& gains = gainDb[(size_t) ch];
        auto& targets = cut[(size_t) ch];

        for (int b = 0; b < bins; ++b)
        {
            const float targetGainDb = -targets[(size_t) b];
            float& g = gains[(size_t) b];

            const float coefficient = targetGainDb < g ? attack : release;
            g += (targetGainDb - g) * coefficient;

            if (std::abs (g) < 1.0e-5f)
                g = 0.0f;

            gainLin[(size_t) b] = std::pow (10.0f, g * 0.05f);

            frameMaxCut = std::max (frameMaxCut, -g);
        }

        // Leave DC and Nyquist alone.
        gainLin[0] = 1.0f;
        gainLin[(size_t) (bins - 1)] = 1.0f;

        auto& re = fftRe[(size_t) ch];
        auto& im = fftIm[(size_t) ch];

        for (int b = 1; b < bins - 1; ++b)
        {
            const float g = gainLin[(size_t) b];
            const int mirror = n - b;

            re[(size_t) b] *= g;
            im[(size_t) b] *= g;
            re[(size_t) mirror] *= g;
            im[(size_t) mirror] *= g;
        }

        fft->inverse (re.data(), im.data());

        auto& o = overlap[(size_t) ch];

        for (int k = 0; k < n; ++k)
        {
            const int index = overlapPos + k;
            const int wrapped = index >= n ? index - n : index;
            o[(size_t) wrapped] += re[(size_t) k] * window[(size_t) k] * olaScale;
        }
    }

    maxCutDb = frameMaxCut;

    updateDisplay();
}

void SpectralSuppressor::updateDisplay() noexcept
{
    const int channels = activeChannels;

    for (int i = 0; i < DisplaySnapshot::numPoints; ++i)
    {
        const int lo = displayLo[(size_t) i];
        const int hi = displayHi[(size_t) i];

        float inputMax = -110.0f;
        float cutMax = 0.0f;

        for (int ch = 0; ch < channels; ++ch)
        {
            const auto& d = db[(size_t) ch];
            const auto& g = gainDb[(size_t) ch];

            for (int b = lo; b < hi; ++b)
            {
                inputMax = std::max (inputMax, d[(size_t) b]);
                cutMax = std::max (cutMax, -g[(size_t) b]);
            }
        }

        display.inputDb[(size_t) i] = std::min (6.0f, std::max (-110.0f, inputMax));
        display.cutDb[(size_t) i] = std::min (36.0f, std::max (0.0f, cutMax));
    }
}

void SpectralSuppressor::process (float* const* channels, int numChannels, int numSamples,
                                  float mix, bool delta, bool bypass,
                                  const float* const* sidechain, int numSidechainChannels) noexcept
{
    if (fft == nullptr || channels == nullptr || numSamples <= 0)
        return;

    const int channelsToProcess = std::min (std::min (numChannels, activeChannels), maxChannels);

    if (channelsToProcess <= 0)
        return;

    const bool hasSidechain = sidechain != nullptr && numSidechainChannels > 0;

    if (hasSidechain != sidechainActive)
    {
        for (auto& s : sideHistory)
            std::fill (s.begin(), s.end(), 0.0f);

        sidechainActive = hasSidechain;
    }

    sidechainChannels = hasSidechain ? std::min (numSidechainChannels, maxChannels) : 0;

    mixTarget = std::min (1.0f, std::max (0.0f, mix));

    const bool midSide = settings.midSide && channelsToProcess == 2;

    for (int s = 0; s < numSamples; ++s)
    {
        if (hopCounter == 0)
            processFrame();

        mixCurrent += (mixTarget - mixCurrent) * mixCoefficient;

        float inputs[2] { 0.0f, 0.0f };
        float outputs[2] { 0.0f, 0.0f };

        for (int ch = 0; ch < channelsToProcess; ++ch)
            inputs[ch] = channels[ch][s];

        if (midSide)
        {
            const float left = inputs[0];
            const float right = inputs[1];

            inputs[0] = 0.5f * (left + right);
            inputs[1] = 0.5f * (left - right);
        }

        for (int ch = 0; ch < channelsToProcess; ++ch)
        {
            const float delayedDry = dryLine[(size_t) ch][(size_t) dryPos];
            dryLine[(size_t) ch][(size_t) dryPos] = inputs[ch];

            float wet = overlap[(size_t) ch][(size_t) overlapPos];
            overlap[(size_t) ch][(size_t) overlapPos] = 0.0f;

            float output;

            if (bypass)
                output = delayedDry;
            else if (delta)
                output = delayedDry - wet;
            else
                output = delayedDry + (wet - delayedDry) * mixCurrent;

            outputs[ch] = output;

            history[(size_t) ch][(size_t) historyPos] = inputs[ch];
        }

        if (sidechainActive)
        {
            float scInputs[2] { 0.0f, 0.0f };

            for (int ch = 0; ch < channelsToProcess; ++ch)
                scInputs[ch] = sidechain[ch % sidechainChannels][s];

            if (midSide && sidechainChannels >= 2)
            {
                const float left = scInputs[0];
                const float right = scInputs[1];

                scInputs[0] = 0.5f * (left + right);
                scInputs[1] = 0.5f * (left - right);
            }

            for (int ch = 0; ch < channelsToProcess; ++ch)
                sideHistory[(size_t) ch][(size_t) historyPos] = scInputs[ch];
        }

        if (midSide)
        {
            channels[0][s] = outputs[0] + outputs[1];
            channels[1][s] = outputs[0] - outputs[1];
        }
        else
        {
            for (int ch = 0; ch < channelsToProcess; ++ch)
                channels[ch][s] = outputs[ch];
        }

        if (++dryPos >= fftSize)     dryPos = 0;
        if (++overlapPos >= fftSize) overlapPos = 0;
        if (++historyPos >= fftSize) historyPos = 0;

        if (++hopCounter >= hop)
            hopCounter = 0;
    }
}
} // namespace quell
