#include "Knob.h"

namespace kchannel_ui
{
Knob::Knob (juce::AudioProcessorValueTreeState& state,
            const juce::String& parameterID,
            const juce::String& caption)
    : captionText (caption)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f,
                                juce::MathConstants<float>::pi * 2.75f,
                                true);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    slider.setMouseDragSensitivity (180);
    slider.onValueChange = [this] { repaint(); };
    addAndMakeVisible (slider);

    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (state,
                                                                                        parameterID,
                                                                                        slider);
}

void Knob::setFormatter (std::function<juce::String (double)> toText,
                         std::function<double (const juce::String&)> fromText)
{
    slider.textFromValueFunction = std::move (toText);

    slider.valueFromTextFunction = std::move (fromText);

    if (! slider.valueFromTextFunction)
        slider.valueFromTextFunction = [] (const juce::String& t) { return t.getDoubleValue(); };

    slider.updateText();
}

void Knob::resized()
{
    auto area = getLocalBounds();
    area.removeFromTop (14);
    area.removeFromBottom (16);
    slider.setBounds (area);
}

void Knob::paint (juce::Graphics& g)
{
    g.setColour (colours::textDim);
    g.setFont (uiFont (9.5f, true));
    g.drawText (captionText.toUpperCase(), getLocalBounds().removeFromTop (13),
                juce::Justification::centred, false);

    g.setColour (colours::text);
    g.setFont (monoFont (10.5f));
    g.drawText (slider.getTextFromValue (slider.getValue()), getLocalBounds().removeFromBottom (15),
                juce::Justification::centred, false);
}
} // namespace kchannel_ui
