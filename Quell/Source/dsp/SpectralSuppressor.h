#pragma once

#include <memory>
#include <vector>

#include "Fft.h"

namespace quell
{
/** Controls for the resonance suppressor. All values are 0..1 unless noted. */
struct SuppressorSettings
{
    float depth       = 0.35f;
    float sensitivity = 0.40f;
    float selectivity = 0.55f;
    float speed       = 0.50f;
    float lowTame     = 0.00f;
    float highTame    = 0.00f;
    bool  link        = true;
    bool  midSide     = false;
};

/** One frame of analyser data for the editor, already mapped to log frequency. */
struct DisplaySnapshot
{
    static constexpr int numPoints = 180;

    float inputDb[numPoints] {};
    float cutDb[numPoints] {};
};

/**
    Short-time spectral resonance suppressor.

    Each channel runs a weighted overlap-add STFT. Per frame the local spectral
    envelope is estimated by zero-phase, frequency-dependent smoothing; anything
    protruding above that envelope by more than the sensitivity threshold is
    attenuated, with depth scaling the amount and a fixed narrow smoothing pass
    keeping the notches musical. Gains are then smoothed in time (attack/release)
    so the processing follows the music rather than the FFT grid.

    The processor is causal with a latency of exactly one FFT frame, which is
    reported to the host and compensated in the dry path.
*/
class SpectralSuppressor
{
public:
    static constexpr int maxChannels = 2;

    SpectralSuppressor();
    ~SpectralSuppressor();

    void prepare (double sampleRate, int numChannels);
    void reset();

    void setSettings (const SuppressorSettings& newSettings) noexcept { settings = newSettings; }

    /**
        Processes in place. @a mix, @a delta and @a bypass correspond to the
        front-panel controls; mix is smoothed internally.

        When @a sidechain is non-null its spectrum drives detection while the
        channels themselves are still the signal being processed. A mono
        sidechain drives both detectors.
    */
    void process (float* const* channels, int numChannels, int numSamples,
                  float mix, bool delta, bool bypass,
                  const float* const* sidechain = nullptr,
                  int numSidechainChannels = 0) noexcept;

    int getLatencySamples() const noexcept { return fftSize; }
    int getFftSize() const noexcept { return fftSize; }

    /** Peak attenuation applied in the most recent frame, in dB (0..36). */
    float getMaxCutDb() const noexcept { return maxCutDb; }

    void readDisplay (DisplaySnapshot& out) const noexcept { out = display; }

private:
    void processFrame() noexcept;
    void computeDbAndEnvelope (int channel) noexcept;
    void updateDisplay() noexcept;
    void rebuildDisplayMap() noexcept;

    double sampleRate = 48000.0;
    int fftSize = 1024;
    int hop = 256;
    int numBins = 513;
    int activeChannels = 0;

    std::unique_ptr<Fft> fft;

    std::vector<float> window;
    float olaScale = 1.0f / 1.5f;

    // Per-channel state, indexed [channel][sample or bin].
    std::vector<std::vector<float>> history, overlap, dryLine;
    std::vector<std::vector<float>> fftRe, fftIm;
    std::vector<std::vector<float>> mag, db, envelope, cut, gainDb;
    std::vector<std::vector<float>> sideHistory, sideRe, sideIm;

    std::vector<float> gainLin;
    std::vector<float> cutSmooth;

    std::vector<int> displayLo, displayHi;
    DisplaySnapshot display;
    float maxCutDb = 0.0f;

    int historyPos = 0, overlapPos = 0, dryPos = 0, hopCounter = 0;

    bool sidechainActive = false;
    int sidechainChannels = 0;

    SuppressorSettings settings;
    float mixTarget = 1.0f;
    float mixCurrent = 1.0f;
    float mixCoefficient = 0.001f;
};
} // namespace quell
