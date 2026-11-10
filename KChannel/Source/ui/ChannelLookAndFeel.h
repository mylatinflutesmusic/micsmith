#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "UiTheme.h"

namespace kchannel_ui
{
/** Dark console metal with a warm amber accent and cool blue for the Quell stage. */
class ChannelLookAndFeel : public juce::LookAndFeel_V4
{
public:
    ChannelLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPosProportional, float rotaryStartAngle,
                           float rotaryEndAngle, juce::Slider&) override;

    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour& backgroundColour,
                               bool shouldDrawButtonAsHighlighted,
                               bool shouldDrawButtonAsDown) override;

    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;
    juce::Font getLabelFont (juce::Label&) override;
};
} // namespace kchannel_ui
