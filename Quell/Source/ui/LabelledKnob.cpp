#include "LabelledKnob.h"

namespace quell_ui
{
LabelledKnob::LabelledKnob (juce::AudioProcessorValueTreeState& state,
                            const juce::String& parameterID,
                            const juce::String& caption)
    : captionText (caption)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f,
                                juce::MathConstants<float>::pi * 2.75f,
                                true);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    slider.setMouseDragSensitivity (170);
    slider.onValueChange = [this] { repaint(); };
    addAndMakeVisible (slider);

    // The attachment owns the two-way sync; it also initialises the slider.
    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (state, parameterID, slider);
}

void LabelledKnob::setFormatter (std::function<juce::String (double)> toText,
                                 std::function<double (const juce::String&)> fromText)
{
    slider.textFromValueFunction = std::move (toText);

    slider.valueFromTextFunction = std::move (fromText);

    if (! slider.valueFromTextFunction)
        slider.valueFromTextFunction = [] (const juce::String& t) { return t.getDoubleValue(); };

    slider.updateText();
}

void LabelledKnob::resized()
{
    auto area = getLocalBounds();
    area.removeFromTop (14);
    area.removeFromBottom (17);
    slider.setBounds (area);
}

void LabelledKnob::paint (juce::Graphics& g)
{
    g.setColour (colours::textDim);
    g.setFont (uiFont (10.0f, true));
    g.drawText (captionText.toUpperCase(), getLocalBounds().removeFromTop (13),
                juce::Justification::centred, false);

    g.setColour (colours::text);
    g.setFont (monoFont (11.0f));
    g.drawText (slider.getTextFromValue (slider.getValue()), getLocalBounds().removeFromBottom (16),
                juce::Justification::centred, false);
}
} // namespace quell_ui
