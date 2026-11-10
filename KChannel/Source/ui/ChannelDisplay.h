#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "UiTheme.h"

class KChannelAudioProcessor;

namespace kchannel_ui
{
/**
    The channel overview: live EQ curve, the actual processing order, and
    input/output/gain-reduction/Quell meters.
*/
class ChannelDisplay : public juce::Component
{
public:
    explicit ChannelDisplay (KChannelAudioProcessor&);

    void paint (juce::Graphics&) override;

    static constexpr float minHz = 20.0f;
    static constexpr float maxHz = 20000.0f;
    static constexpr float dbRange = 18.0f;

private:
    static float xForHz (float hz, juce::Rectangle<float> plot);
    static float yForDb (float db, juce::Rectangle<float> plot);

    KChannelAudioProcessor& processor;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChannelDisplay)
};
} // namespace kchannel_ui
