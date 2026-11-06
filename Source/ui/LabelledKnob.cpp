#include "LabelledKnob.h"

namespace micsmith_ui
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
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 76, 16);
    slider.setColour (juce::Slider::textBoxTextColourId, colours::text);
    slider.setMouseDragSensitivity (180);
    addAndMakeVisible (slider);

    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (state, parameterID, slider);
}

void LabelledKnob::setFormatter (std::function<juce::String (double)> toText,
                                 std::function<double (const juce::String&)> fromText)
{
    slider.textFromValueFunction = std::move (toText);

    slider.valueFromTextFunction = std::move (fromText);

    if (! slider.valueFromTextFunction)
        slider.valueFromTextFunction = [this] (const juce::String& t) { return t.getDoubleValue(); };

    slider.updateText();
}

void LabelledKnob::resized()
{
    auto area = getLocalBounds();
    area.removeFromTop (14);
    slider.setBounds (area);
}

void LabelledKnob::paint (juce::Graphics& g)
{
    g.setColour (colours::textDim);
    g.setFont (uiFont (10.0f, true));
    g.drawText (captionText.toUpperCase(), getLocalBounds().removeFromTop (13),
                juce::Justification::centred, false);
}
} // namespace micsmith_ui
