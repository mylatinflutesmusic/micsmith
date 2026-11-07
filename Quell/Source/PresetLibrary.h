#pragma once

#include <vector>

#include <juce_audio_processors/juce_audio_processors.h>

#include "Presets.h"

namespace quell
{
/**
    Factory presets plus user presets stored as small XML files.

    User presets live in the application data directory by default. Bypass and
    delta are deliberately not stored: they are monitoring controls, not part of
    a saved sound.
*/
class PresetLibrary
{
public:
    struct UserPreset
    {
        juce::String name;
        juce::File file;
    };

    /** Pass an explicit directory for tests; empty means the per-user default. */
    explicit PresetLibrary (juce::File directory = {});

    void refresh();

    const juce::File& getDirectory() const noexcept { return directory; }
    const std::vector<UserPreset>& getUserPresets() const noexcept { return userPresets; }

    /** Returns the user preset with this name, or -1. */
    int findUserPreset (const juce::String& name) const;

    bool saveUserPreset (const juce::String& name,
                         const juce::AudioProcessorValueTreeState&,
                         juce::String& error);

    bool loadUserPreset (int index,
                         juce::AudioProcessorValueTreeState&,
                         juce::String& error) const;

    bool deleteUserPreset (int index, juce::String& error);

    static void apply (const PresetValues&, juce::AudioProcessorValueTreeState&);
    static void applyFactoryPreset (int index, juce::AudioProcessorValueTreeState&);

private:
    static bool readPreset (const juce::File&, juce::String& name,
                            PresetValues&, juce::String& error);

    juce::File directory;
    std::vector<UserPreset> userPresets;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PresetLibrary)
};
} // namespace quell
