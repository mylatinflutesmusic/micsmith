#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace kchannel_ui
{
namespace colours
{
    inline const juce::Colour background  { 0xff0a0c0e };
    inline const juce::Colour panel       { 0xff13181c };
    inline const juce::Colour panelLight  { 0xff1b2228 };
    inline const juce::Colour outline     { 0xff2b353d };
    inline const juce::Colour text        { 0xffe9e4d8 };
    inline const juce::Colour textDim     { 0xff8c9499 };
    inline const juce::Colour accent      { 0xffe2b15c };
    inline const juce::Colour accentCool  { 0xff78a9c9 };
    inline const juce::Colour reduction   { 0xffe07b4f };
    inline const juce::Colour ok          { 0xff7fc9a4 };
}

inline juce::Font uiFont (float height, bool bold = false)
{
   #if JUCE_MAJOR_VERSION >= 8
    auto opts = juce::FontOptions().withHeight (height);

    if (bold)
        opts = opts.withStyle ("Bold");

    return juce::Font (opts);
   #else
    return juce::Font (height, bold ? juce::Font::bold : juce::Font::plain);
   #endif
}

inline juce::Font monoFont (float height)
{
   #if JUCE_MAJOR_VERSION >= 8
    return juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(),
                                          height, juce::Font::plain));
   #else
    return juce::Font (juce::Font::getDefaultMonospacedFontName(),
                       height, juce::Font::plain);
   #endif
}
} // namespace kchannel_ui
