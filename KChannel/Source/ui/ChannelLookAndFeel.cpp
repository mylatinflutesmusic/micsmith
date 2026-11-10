#include "ChannelLookAndFeel.h"

namespace kchannel_ui
{
ChannelLookAndFeel::ChannelLookAndFeel()
{
    setColour (juce::ResizableWindow::backgroundColourId, colours::background);
    setColour (juce::Label::textColourId, colours::text);

    setColour (juce::TextButton::buttonColourId, colours::panelLight);
    setColour (juce::TextButton::buttonOnColourId, colours::accent);
    setColour (juce::TextButton::textColourOffId, colours::textDim);
    setColour (juce::TextButton::textColourOnId, colours::background);
    setColour (juce::ToggleButton::textColourId, colours::textDim);
    setColour (juce::ToggleButton::tickColourId, colours::accent);

    setColour (juce::Slider::textBoxTextColourId, colours::text);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::thumbColourId, colours::accent);

    setColour (juce::TooltipWindow::backgroundColourId, colours::panelLight);
    setColour (juce::TooltipWindow::textColourId, colours::text);
    setColour (juce::TooltipWindow::outlineColourId, colours::outline);
}

void ChannelLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                           float pos, float startAngle, float endAngle,
                                           juce::Slider& slider)
{
    auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat().reduced (5.0f);

    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const float angle = startAngle + pos * (endAngle - startAngle);

    for (int i = 0; i <= 10; ++i)
    {
        const float t = (float) i / 10.0f;
        const float a = startAngle + t * (endAngle - startAngle);
        const float inner = radius - 2.0f;
        const float outer = radius - 5.5f;

        g.setColour ((t <= pos ? colours::accent : colours::outline).withAlpha (t <= pos ? 0.9f : 0.8f));
        g.drawLine (centre.x + std::sin (a) * inner, centre.y - std::cos (a) * inner,
                    centre.x + std::sin (a) * outer, centre.y - std::cos (a) * outer, 1.0f);
    }

    const float dialRadius = radius - 7.0f;

    juce::ColourGradient bodyGrad (colours::panelLight.brighter (0.10f), centre.x, centre.y - dialRadius,
                                   colours::panel.darker (0.20f), centre.x, centre.y + dialRadius, false);
    g.setGradientFill (bodyGrad);
    g.fillEllipse (juce::Rectangle<float> (dialRadius * 2.0f, dialRadius * 2.0f).withCentre (centre));

    g.setColour (colours::outline);
    g.drawEllipse (juce::Rectangle<float> (dialRadius * 2.0f, dialRadius * 2.0f).withCentre (centre), 1.0f);

    juce::Path pointer;
    const float inner = dialRadius * 0.18f;
    const float outer = dialRadius - 3.5f;
    pointer.startNewSubPath (centre.x + std::sin (angle) * inner, centre.y - std::cos (angle) * inner);
    pointer.lineTo          (centre.x + std::sin (angle) * outer, centre.y - std::cos (angle) * outer);

    g.setColour (slider.isEnabled() ? colours::text : colours::textDim);
    g.strokePath (pointer, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved,
                                                 juce::PathStrokeType::rounded));

    g.setColour (colours::accent);
    g.fillEllipse (juce::Rectangle<float> (3.2f, 3.2f).withCentre (centre));
}

void ChannelLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button,
                                               const juce::Colour& backgroundColour,
                                               bool isHighlighted, bool isDown)
{
    auto bounds = button.getLocalBounds().toFloat().reduced (0.5f);
    const float corner = 4.0f;

    auto fill = backgroundColour;

    if (isDown)
        fill = fill.darker (0.22f);
    else if (isHighlighted)
        fill = fill.brighter (0.10f);

    g.setColour (fill);
    g.fillRoundedRectangle (bounds, corner);

    g.setColour (button.getToggleState() ? colours::accent.withAlpha (0.95f) : colours::outline);
    g.drawRoundedRectangle (bounds, corner, 1.0f);
}

juce::Font ChannelLookAndFeel::getTextButtonFont (juce::TextButton&, int buttonHeight)
{
    return uiFont (juce::jlimit (8.0f, 13.0f, (float) buttonHeight * 0.55f), true);
}

juce::Font ChannelLookAndFeel::getLabelFont (juce::Label& label)
{
    return label.getFont();
}
} // namespace kchannel_ui
