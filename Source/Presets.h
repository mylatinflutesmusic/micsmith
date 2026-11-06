#pragma once

#include <vector>

namespace micsmith
{
/** Factory starting points. Microphones are referenced by name so the list
    survives any re-ordering of the model library. */
struct FactoryPreset
{
    const char* name;
    const char* sourceName;
    const char* targetName;
    float proximity;
    float axis;
    float lowCutHz;
    float highCutHz;
    float mix;
    float outputDb;
};

inline const std::vector<FactoryPreset>& factoryPresets()
{
    static const std::vector<FactoryPreset> presets
    {
        { "Phone to Studio Condenser",  "Smartphone Mic",        "LD Condenser Modern",   35.0f,  0.0f,  80.0f, 18000.0f, 100.0f,  0.0f },
        { "Laptop Rescue",              "Laptop Built-in Mic",   "LD Condenser Warm",     25.0f,  0.0f, 100.0f, 16000.0f, 100.0f,  0.0f },
        { "Headset Cleanup",            "Headset Lavalier",      "SD Condenser Natural",  20.0f,  0.0f,  90.0f, 17000.0f, 100.0f,  0.0f },
        { "Stage Mic to Broadcast",     "Handheld Dynamic",      "Broadcast Dynamic",     45.0f,  0.0f,  60.0f, 18000.0f, 100.0f,  0.0f },
        { "Dynamic Vocal Polish",       "Handheld Dynamic",      "Vintage Tube Condenser",40.0f,  0.0f,  70.0f, 17000.0f, 100.0f,  0.0f },
        { "Podcast Warmth",             "Studio Dynamic",        "Broadcast Dynamic",     50.0f,  0.0f,  70.0f, 16000.0f, 100.0f,  0.0f },
        { "Airy Pop Vocal",             "LD Condenser Warm",     "LD Condenser Modern",   20.0f,  0.0f, 100.0f, 20000.0f, 100.0f,  0.0f },
        { "Acoustic Guitar SD",         "Neutral Reference",     "SD Condenser Natural",  10.0f,  0.0f,  50.0f, 20000.0f, 100.0f,  0.0f },
        { "Bright Overheads",           "Neutral Reference",     "SD Condenser Bright",    0.0f,  0.0f, 120.0f, 20000.0f, 100.0f,  0.0f },
        { "Ribbon Guitar Cab",          "Instrument Dynamic",    "Vintage Ribbon",        30.0f, 10.0f,  80.0f, 12000.0f, 100.0f,  0.0f },
        { "Off-Axis Room Blend",        "Neutral Reference",     "Modern Ribbon",         15.0f, 55.0f,  90.0f, 14000.0f,  80.0f, -1.0f },
        { "Kick Sub Layer",             "Kick Dynamic",          "Sub-Kick",              60.0f,  0.0f,  30.0f, 10000.0f, 100.0f,  0.0f },
        { "Flat Reference",             "Neutral Reference",     "Neutral Reference",      0.0f,  0.0f,  20.0f, 20000.0f, 100.0f,  0.0f }
    };

    return presets;
}
} // namespace micsmith
