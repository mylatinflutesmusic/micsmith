#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace quell_ui
{
namespace colours
{
    inline const juce::Colour background  { 0xff07090b };
    inline const juce::Colour panel       { 0xff10151a };
    inline const juce::Colour panelLight  { 0xff192027 };
    inline const juce::Colour outline     { 0xff253038 };
    inline const juce::Colour text        { 0xffe6f1f2 };
    inline const juce::Colour textDim     { 0xff7f8e96 };
    inline const juce::Colour accent      { 0xff5fe0c0 };
    inline const juce::Colour accentCool  { 0xff5ab6e8 };
    inline const juce::Colour reduction   { 0xffffab52 };
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
} // namespace quell_ui
