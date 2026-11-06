#pragma once

#include <functional>
#include <memory>

#include <juce_audio_processors/juce_audio_processors.h>

#include "UiTheme.h"

namespace micsmith_ui
{
/** A rotary control with a small caption above and an editable value below. */
class LabelledKnob : public juce::Component
{
public:
    LabelledKnob (juce::AudioProcessorValueTreeState& state,
                  const juce::String& parameterID,
                  const juce::String& caption);

    ~LabelledKnob() override = default;

    void resized() override;
    void paint (juce::Graphics&) override;

    /** Optional custom value formatting (used for Hz / kHz readouts). */
    void setFormatter (std::function<juce::String (double)> toText,
                       std::function<double (const juce::String&)> fromText);

    juce::Slider slider;

private:
    juce::String captionText;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LabelledKnob)
};
} // namespace micsmith_ui
