#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "../dsp/ModelEngine.h"
#include "UiTheme.h"

class MicSmithAudioProcessor;

namespace micsmith_ui
{
/**
    Live transfer-function plot: the resulting correction, plus the source and
    target microphone curves for reference.
*/
class ResponseDisplay : public juce::Component
{
public:
    explicit ResponseDisplay (MicSmithAudioProcessor&);

    void paint (juce::Graphics&) override;

    static constexpr float minHz = 20.0f;
    static constexpr float maxHz = 20000.0f;
    static constexpr float dbRange = 18.0f;

private:
    void drawCurve (juce::Graphics&, const micsmith::CurveEvaluator&, double sr,
                    juce::Rectangle<float> plot, juce::Colour colour, float thickness,
                    bool dashed, double offsetDb);

    static float xForFreq (float hz, juce::Rectangle<float> plot);
    static float yForDb (float db, juce::Rectangle<float> plot);

    MicSmithAudioProcessor& processor;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ResponseDisplay)
};
} // namespace micsmith_ui
