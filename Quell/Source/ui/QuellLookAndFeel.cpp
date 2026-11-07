#include "QuellLookAndFeel.h"

namespace quell_ui
{
QuellLookAndFeel::QuellLookAndFeel()
{
    setColour (juce::ResizableWindow::backgroundColourId, colours::background);

    setColour (juce::Label::textColourId, colours::text);

    setColour (juce::ComboBox::backgroundColourId, colours::panelLight);
    setColour (juce::ComboBox::textColourId, colours::text);
    setColour (juce::ComboBox::outlineColourId, colours::outline);
    setColour (juce::ComboBox::arrowColourId, colours::textDim);
    setColour (juce::ComboBox::buttonColourId, colours::panelLight);

    setColour (juce::PopupMenu::backgroundColourId, colours::panel);
    setColour (juce::PopupMenu::textColourId, colours::text);
    setColour (juce::PopupMenu::headerTextColourId, colours::textDim);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, colours::accent.withAlpha (0.22f));
    setColour (juce::PopupMenu::highlightedTextColourId, colours::text);

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

void QuellLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                         float pos, float startAngle, float endAngle,
                                         juce::Slider& slider)
{
    auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat().reduced (6.0f);

    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const float angle = startAngle + pos * (endAngle - startAngle);

    // Small tick marks around the travel make the arc easy to read at a glance.
    for (int i = 0; i <= 10; ++i)
    {
        const float t = (float) i / 10.0f;
        const float a = startAngle + t * (endAngle - startAngle);
        const float inner = radius - 2.0f;
        const float outer = radius - 6.0f;

        g.setColour ((t <= pos ? colours::accent : colours::outline).withAlpha (t <= pos ? 0.8f : 0.75f));
        g.drawLine (centre.x + std::sin (a) * inner, centre.y - std::cos (a) * inner,
                    centre.x + std::sin (a) * outer, centre.y - std::cos (a) * outer, 1.0f);
    }

    const float dialRadius = radius - 8.0f;

    juce::ColourGradient bodyGrad (colours::panelLight.brighter (0.08f), centre.x, centre.y - dialRadius,
                                   colours::panel, centre.x, centre.y + dialRadius, false);
    g.setGradientFill (bodyGrad);
    g.fillEllipse (juce::Rectangle<float> (dialRadius * 2.0f, dialRadius * 2.0f).withCentre (centre));

    g.setColour (colours::outline);
    g.drawEllipse (juce::Rectangle<float> (dialRadius * 2.0f, dialRadius * 2.0f).withCentre (centre), 1.0f);

    juce::Path pointer;
    const float inner = dialRadius * 0.20f;
    const float outer = dialRadius - 4.0f;
    pointer.startNewSubPath (centre.x + std::sin (angle) * inner, centre.y - std::cos (angle) * inner);
    pointer.lineTo          (centre.x + std::sin (angle) * outer, centre.y - std::cos (angle) * outer);
    g.setColour (slider.isEnabled() ? colours::text : colours::textDim);
    g.strokePath (pointer, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved,
                                                 juce::PathStrokeType::rounded));

    g.setColour (colours::accent.withAlpha (0.9f));
    g.fillEllipse (juce::Rectangle<float> (3.0f, 3.0f).withCentre (centre));
}

void QuellLookAndFeel::drawComboBox (juce::Graphics& g, int width, int height, bool,
                                     int, int, int, int, juce::ComboBox& box)
{
    auto bounds = juce::Rectangle<int> (0, 0, width, height).toFloat().reduced (0.5f);
    const float corner = 4.0f;

    g.setColour (box.findColour (juce::ComboBox::backgroundColourId));
    g.fillRoundedRectangle (bounds, corner);

    g.setColour (box.hasKeyboardFocus (true) ? colours::accent.withAlpha (0.7f)
                                             : colours::outline);
    g.drawRoundedRectangle (bounds, corner, 1.0f);

    const auto arrowArea = juce::Rectangle<float> (bounds.getRight() - 22.0f, bounds.getY(),
                                                   20.0f, bounds.getHeight());
    juce::Path arrow;
    arrow.startNewSubPath (arrowArea.getCentreX() - 4.0f, arrowArea.getCentreY() - 2.0f);
    arrow.lineTo (arrowArea.getCentreX(), arrowArea.getCentreY() + 3.0f);
    arrow.lineTo (arrowArea.getCentreX() + 4.0f, arrowArea.getCentreY() - 2.0f);
    g.setColour (box.findColour (juce::ComboBox::arrowColourId));
    g.strokePath (arrow, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded));
}

void QuellLookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (11, 1, box.getWidth() - 36, box.getHeight() - 2);
    label.setFont (getComboBoxFont (box));
    label.setJustificationType (juce::Justification::centredLeft);
}

void QuellLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button,
                                             const juce::Colour& backgroundColour,
                                             bool isHighlighted, bool isDown)
{
    auto bounds = button.getLocalBounds().toFloat().reduced (0.5f);
    const float corner = 4.0f;

    auto fill = backgroundColour;

    if (isDown)
        fill = fill.darker (0.2f);
    else if (isHighlighted)
        fill = fill.brighter (0.1f);

    g.setColour (fill);
    g.fillRoundedRectangle (bounds, corner);

    g.setColour (button.getToggleState() ? colours::accent.withAlpha (0.9f) : colours::outline);
    g.drawRoundedRectangle (bounds, corner, 1.0f);
}

juce::Font QuellLookAndFeel::getComboBoxFont (juce::ComboBox& box)
{
    return uiFont (juce::jlimit (11.0f, 15.0f, (float) box.getHeight() * 0.45f));
}

juce::Font QuellLookAndFeel::getLabelFont (juce::Label& label)
{
    return label.getFont();
}
} // namespace quell_ui
