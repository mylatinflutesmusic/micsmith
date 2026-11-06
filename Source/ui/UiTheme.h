#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace micsmith_ui
{
namespace colours
{
    inline const juce::Colour background  { 0xff0d0f12 };
    inline const juce::Colour panel       { 0xff171b21 };
    inline const juce::Colour panelLight  { 0xff1f242c };
    inline const juce::Colour outline     { 0xff2c333d };
    inline const juce::Colour text        { 0xffe2e7ee };
    inline const juce::Colour textDim     { 0xff89929f };
    inline const juce::Colour accent      { 0xffe0a84e };
    inline const juce::Colour accentCool  { 0xff5ab8e6 };
    inline const juce::Colour accentWarm  { 0xffe0684e };
}

inline juce::Font uiFont (float height, bool bold = false)
{
    auto opts = juce::FontOptions().withHeight (height);

    if (bold)
        opts = opts.withStyle ("Bold");

    return juce::Font (opts);
}

inline juce::Font monoFont (float height)
{
    return juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), height, juce::Font::plain));
}
} // namespace micsmith_ui
