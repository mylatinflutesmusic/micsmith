#pragma once

#include <array>

#include <juce_gui_basics/juce_gui_basics.h>

#include "../dsp/SpectralSuppressor.h"
#include "UiTheme.h"

class QuellAudioProcessor;

namespace quell_ui
{
/**
    The signature view: live input spectrum with the dynamic reduction drawn as
    an amber region hanging beneath it.
*/
class SpectrumDisplay : public juce::Component
{
public:
    explicit SpectrumDisplay (QuellAudioProcessor&);

    void paint (juce::Graphics&) override;

    /** Pulls the latest analyser frame and advances the visual smoothing. */
    void update();

private:
    static constexpr int numPoints = quell::DisplaySnapshot::numPoints;

    static float xForHz (float hz, juce::Rectangle<float> plot);
    static float yForDb (float db, juce::Rectangle<float> plot);

    QuellAudioProcessor& processor;

    std::array<float, numPoints> shownInput {};
    std::array<float, numPoints> shownCut {};
    float shownMaxCut = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SpectrumDisplay)
};
} // namespace quell_ui
