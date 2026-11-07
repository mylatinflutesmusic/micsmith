#pragma once

#include <vector>

namespace quell
{
/** Factory starting points. Percent values, output in dB. */
struct FactoryPreset
{
    const char* name;
    float depth;
    float sensitivity;
    float selectivity;
    float speed;
    float lowTame;
    float highTame;
    float mix;
    float outputDb;
    bool  link;
};

/** A complete sound setting; bypass and delta are monitoring states, not presets. */
struct PresetValues
{
    float depth       = 35.0f;
    float sensitivity = 40.0f;
    float selectivity = 55.0f;
    float speed       = 50.0f;
    float lowTame     = 0.0f;
    float highTame    = 0.0f;
    float mix         = 100.0f;
    float outputDb    = 0.0f;
    bool  link        = true;
    bool  midSide     = false;
    bool  sidechain   = false;
};

inline const std::vector<FactoryPreset>& factoryPresets()
{
    static const std::vector<FactoryPreset> presets
    {
        { "Init",                35.0f, 40.0f, 55.0f, 50.0f,  0.0f,  0.0f, 100.0f,  0.0f, true },
        { "Vocal Cleanup",       55.0f, 60.0f, 60.0f, 55.0f, 20.0f, 15.0f, 100.0f,  0.0f, true },
        { "De-Ess",              65.0f, 72.0f, 78.0f, 70.0f,  0.0f, 25.0f, 100.0f,  0.0f, true },
        { "Harsh Guitar",        60.0f, 62.0f, 65.0f, 50.0f, 10.0f,  0.0f, 100.0f,  0.0f, true },
        { "Acoustic Body",       50.0f, 55.0f, 45.0f, 45.0f, 35.0f, 20.0f, 100.0f,  0.0f, true },
        { "Drum Bus",            40.0f, 45.0f, 50.0f, 65.0f, 25.0f, 30.0f, 100.0f,  0.0f, true },
        { "Mastering Gentle",    30.0f, 40.0f, 55.0f, 40.0f, 20.0f, 20.0f, 100.0f,  0.0f, true },
        { "Aggressive Tame",     85.0f, 80.0f, 85.0f, 75.0f,  0.0f,  0.0f, 100.0f, -1.0f, true },
        { "Cymbal Wash",         55.0f, 65.0f, 70.0f, 60.0f,  0.0f, 10.0f, 100.0f,  0.0f, true },
        { "Bass Growl",          55.0f, 55.0f, 50.0f, 40.0f, 45.0f,  0.0f, 100.0f,  0.0f, true },
        { "Parallel Polish",     65.0f, 65.0f, 70.0f, 55.0f, 15.0f, 15.0f,  45.0f,  0.0f, true }
    };

    return presets;
}

inline PresetValues valuesForFactoryPreset (int index)
{
    PresetValues values;
    const auto& presets = factoryPresets();

    if (index < 0 || index >= (int) presets.size())
        return values;

    const auto& preset = presets[(size_t) index];

    values.depth       = preset.depth;
    values.sensitivity = preset.sensitivity;
    values.selectivity = preset.selectivity;
    values.speed       = preset.speed;
    values.lowTame     = preset.lowTame;
    values.highTame    = preset.highTame;
    values.mix         = preset.mix;
    values.outputDb    = preset.outputDb;
    values.link        = preset.link;

    return values;
}
} // namespace quell
