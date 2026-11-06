#pragma once

#include <vector>

#include "Biquad.h"

namespace micsmith
{
/**
    A microphone model expressed as a minimum-phase magnitude response.

    These curves are physically motivated approximations of the broad family
    characteristics of each mic type (capsule resonance, diaphragm damping,
    transformer bandwidth, proximity behaviour). They are original curves, not
    copies of any specific product's measured data.

    Only Peak / LowShelf / HighShelf bands are used, so that any model can be
    inverted exactly and stably.
*/
struct MicModel
{
    const char* name;
    const char* family;
    const char* blurb;
    std::vector<Band> bands;
};

inline const std::vector<MicModel>& micModels()
{
    static const std::vector<MicModel> models
    {
        { "Neutral Reference", "Reference",
          "Flat calibration reference. Use it as the source when you already have a clean, uncoloured capture.",
          {} },

        { "LD Condenser Warm", "Large-diaphragm condenser",
          "Full low end, gentle 2.5 kHz presence and a soft air lift. Classic tube-condenser smoothness.",
          { { BandType::LowShelf,   120.0, 0.70,  1.5 },
            { BandType::Peak,       400.0, 1.20, -1.0 },
            { BandType::Peak,      2500.0, 1.00,  1.0 },
            { BandType::Peak,      8000.0, 0.70,  2.0 },
            { BandType::HighShelf, 12000.0, 0.70,  1.5 } } },

        { "LD Condenser Modern", "Large-diaphragm condenser",
          "Tight lows, a firm 5 kHz push and open top. Bright, forward, radio-ready.",
          { { BandType::LowShelf,    90.0, 0.70,  1.0 },
            { BandType::Peak,       700.0, 1.00, -1.5 },
            { BandType::Peak,      5000.0, 1.20,  2.5 },
            { BandType::Peak,     11000.0, 0.80,  3.0 } } },

        { "Vintage Tube Condenser", "Large-diaphragm condenser",
          "Rounded top, thick low-mids, relaxed upper midrange. Softens harsh sources.",
          { { BandType::LowShelf,   100.0, 0.70,  2.5 },
            { BandType::Peak,       300.0, 1.20,  1.5 },
            { BandType::Peak,      3000.0, 1.00, -1.0 },
            { BandType::HighShelf, 8000.0, 0.70, -3.0 } } },

        { "SD Condenser Natural", "Small-diaphragm condenser",
          "Very even response with a clean high-shelf lift. The honest acoustic-instrument choice.",
          { { BandType::LowShelf,   100.0, 0.70, -0.5 },
            { BandType::Peak,      4000.0, 1.50,  1.0 },
            { BandType::HighShelf, 9000.0, 0.70,  2.0 } } },

        { "SD Condenser Bright", "Small-diaphragm condenser",
          "Scooped lows with a strong 6 kHz shelf. Detailed hats, strings and cymbals.",
          { { BandType::LowShelf,   150.0, 0.70, -2.0 },
            { BandType::HighShelf, 6000.0, 0.60,  3.5 },
            { BandType::Peak,     10000.0, 1.00,  2.0 } } },

        { "Studio Dynamic", "Moving coil dynamic",
          "Balanced broadcast-style dynamic: controlled proximity, mild 3 kHz articulation, soft top.",
          { { BandType::LowShelf,   100.0, 0.70,  1.0 },
            { BandType::Peak,       500.0, 1.20, -3.0 },
            { BandType::Peak,      3000.0, 1.20,  2.0 },
            { BandType::HighShelf, 9000.0, 0.70, -3.0 } } },

        { "Broadcast Dynamic", "Moving coil dynamic",
          "Big low-mid body, scooped 500 Hz and a rolled-off top. The voice-of-radio weight.",
          { { BandType::LowShelf,    80.0, 0.70,  2.0 },
            { BandType::Peak,       250.0, 1.00,  2.5 },
            { BandType::Peak,       500.0, 1.50, -1.5 },
            { BandType::Peak,      6500.0, 1.20,  2.0 },
            { BandType::HighShelf,10000.0, 0.70, -5.0 } } },

        { "Instrument Dynamic", "Moving coil dynamic",
          "The 5 kHz bite and low-mid punch of a stage instrument mic. Snare and guitar cabs.",
          { { BandType::LowShelf,   100.0, 0.70, -1.0 },
            { BandType::Peak,       350.0, 1.20,  2.0 },
            { BandType::Peak,       900.0, 1.50, -2.0 },
            { BandType::Peak,      5000.0, 1.20,  3.5 },
            { BandType::HighShelf,10000.0, 0.70, -3.0 } } },

        { "Handheld Dynamic", "Moving coil dynamic",
          "Presence peak plus a hard low rolloff. Built for close vocals in loud rooms.",
          { { BandType::LowShelf,   150.0, 0.70, -4.0 },
            { BandType::Peak,       300.0, 1.20,  2.0 },
            { BandType::Peak,      1000.0, 1.00, -2.0 },
            { BandType::Peak,      4000.0, 1.00,  3.0 },
            { BandType::HighShelf, 9000.0, 0.70, -4.0 } } },

        { "Kick Dynamic", "Instrument dynamic",
          "70 Hz thump, deep 400 Hz scoop and a 3.5 kHz beater click.",
          { { BandType::Peak,       70.0, 1.20,  4.0 },
            { BandType::Peak,      400.0, 1.50, -6.0 },
            { BandType::Peak,     3500.0, 1.20,  4.0 },
            { BandType::HighShelf, 8000.0, 0.70, -8.0 } } },

        { "Sub-Kick", "Boundary / subwoofer",
          "Almost no top end, enormous sub weight. Layer it under a normal kick mic.",
          { { BandType::LowShelf,   60.0, 0.70,  8.0 },
            { BandType::Peak,       300.0, 1.20, -4.0 },
            { BandType::HighShelf, 3000.0, 0.70,-14.0 } } },

        { "Vintage Ribbon", "Ribbon",
          "Dark, thick and unhurried: strong low-mid build-up and an early top-end rolloff.",
          { { BandType::LowShelf,   120.0, 0.70,  3.0 },
            { BandType::Peak,       400.0, 1.00,  2.0 },
            { BandType::Peak,      2000.0, 1.00, -1.0 },
            { BandType::HighShelf, 5000.0, 0.70, -8.0 } } },

        { "Modern Ribbon", "Ribbon",
          "A brighter, better-damped ribbon. Warm but still usable on voice.",
          { { BandType::LowShelf,   100.0, 0.70,  1.5 },
            { BandType::Peak,      3000.0, 1.20,  1.5 },
            { BandType::HighShelf, 8000.0, 0.70, -4.0 } } },

        { "Passive Ribbon Dark", "Ribbon",
          "Very low output with a steep 3.5 kHz shelf. Gutty guitar amps and room mics.",
          { { BandType::Peak,       200.0, 1.00,  2.5 },
            { BandType::HighShelf, 3500.0, 0.70,-10.0 } } },

        { "Headset Lavalier", "Lavalier / headset",
          "Thin low end, boxy 500 Hz bump and a bright, sibilant top. Common in video calls.",
          { { BandType::LowShelf,   200.0, 0.70, -6.0 },
            { BandType::Peak,       500.0, 1.50,  3.0 },
            { BandType::Peak,      6000.0, 1.20,  3.0 },
            { BandType::HighShelf,10000.0, 0.70, -5.0 } } },

        { "Camera Shotgun", "Shotgun condenser",
          "Off-axis colouration: thin lows, hollow 800 Hz, brittle 6 kHz lift.",
          { { BandType::LowShelf,   180.0, 0.70, -5.0 },
            { BandType::Peak,       800.0, 1.20,  2.5 },
            { BandType::Peak,      6000.0, 1.00,  3.0 },
            { BandType::HighShelf,12000.0, 0.70, -2.0 } } },

        { "Smartphone Mic", "Consumer device",
          "Bass-light, boxy and sibilant. Start here when the take came from a phone.",
          { { BandType::LowShelf,   250.0, 0.70, -9.0 },
            { BandType::Peak,       900.0, 1.50,  4.0 },
            { BandType::Peak,      3500.0, 1.20,  3.0 },
            { BandType::HighShelf, 9000.0, 0.70, -6.0 } } },

        { "Laptop Built-in Mic", "Consumer device",
          "Almost no low end, strong 1.2 kHz honk and a ragged top. The worst-case source.",
          { { BandType::LowShelf,   300.0, 0.70,-12.0 },
            { BandType::Peak,      1200.0, 1.50,  5.0 },
            { BandType::Peak,      4000.0, 1.20,  2.5 },
            { BandType::HighShelf, 8000.0, 0.70, -8.0 } } }
    };

    return models;
}

inline int numMics() noexcept
{
    return (int) micModels().size();
}
} // namespace micsmith
