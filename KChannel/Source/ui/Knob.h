#pragma once

#include <functional>
#include <memory>

#include <juce_audio_processors/juce_audio_processors.h>

#include "UiTheme.h"

namespace kchannel_ui
{
/** A rotary control with a caption above and an editable value readout below. */
class Knob : public juce::Component
{
public:
    Knob (juce::AudioProcessorValueTreeState& state,
          const juce::String& parameterID,
          const juce::String& caption);

    ~Knob() override = default;

    void resized() override;
    void paint (juce::Graphics&) override;

    void setFormatter (std::function<juce::String (double)> toText,
                       std::function<double (const juce::String&)> fromText);

    juce::Slider slider;

private:
    juce::String captionText;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Knob)
};
} // namespace kchannel_ui
