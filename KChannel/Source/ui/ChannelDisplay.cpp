#include "ChannelDisplay.h"

#include "../PluginProcessor.h"

#include <array>
#include <cmath>

namespace kchannel_ui
{
namespace
{
    constexpr float minHz = ChannelDisplay::minHz;
    constexpr float maxHz = ChannelDisplay::maxHz;
    constexpr float dbRange = ChannelDisplay::dbRange;

    float xForHz (float hz, juce::Rectangle<float> plot)
    {
        const float lo = std::log10 (minHz);
        const float hi = std::log10 (maxHz);
        const float t = (std::log10 (juce::jlimit (minHz, maxHz, hz)) - lo) / (hi - lo);
        return plot.getX() + t * plot.getWidth();
    }

    float yForDb (float db, juce::Rectangle<float> plot)
    {
        const float t = juce::jlimit (-1.0f, 1.0f, db / dbRange);
        return plot.getCentreY() - t * plot.getHeight() * 0.5f;
    }

    float peakToNorm (float peak)
    {
        const float db = 20.0f * std::log10 (juce::jmax (1.0e-6f, peak));
        return juce::jlimit (0.0f, 1.0f, (db + 60.0f) / 66.0f);
    }

    void drawMeter (juce::Graphics& g, juce::Rectangle<float> area, const juce::String& label,
                    float norm, juce::Colour colour, const juce::String& valueText, float scale)
    {
        auto row = area;
        auto labelArea = row.removeFromLeft (34.0f * scale);
        auto valueArea = row.removeFromRight (38.0f * scale);

        row.removeFromLeft (4.0f * scale);
        row.removeFromRight (4.0f * scale);

        g.setColour (colours::textDim);
        g.setFont (uiFont (8.0f * scale, true));
        g.drawText (label, labelArea, juce::Justification::centredLeft, false);

        g.setColour (colours::panelLight);
        g.fillRoundedRectangle (row.reduced (0.0f, 4.0f * scale), 2.0f);

        auto fill = row.reduced (0.0f, 4.0f * scale);
        fill.setWidth (fill.getWidth() * juce::jlimit (0.0f, 1.0f, norm));

        g.setColour (colour.withAlpha (0.9f));
        g.fillRoundedRectangle (fill, 2.0f);

        g.setColour (colours::textDim.withAlpha (0.75f));
        g.setFont (monoFont (8.0f * scale));
        g.drawText (valueText, valueArea, juce::Justification::centredRight, false);
    }

    void drawStage (juce::Graphics& g, juce::Rectangle<float> box, const juce::String& line1,
                    const juce::String& line2, juce::Colour colour, bool filled, float scale)
    {
        g.setColour (filled ? colour.withAlpha (0.18f) : colours::panelLight);
        g.fillRoundedRectangle (box, 3.0f * scale);

        g.setColour (colour.withAlpha (filled ? 0.9f : 0.45f));
        g.drawRoundedRectangle (box.reduced (0.5f), 3.0f * scale, 1.0f);

        g.setColour (filled ? colour : colours::textDim);
        g.setFont (uiFont (8.0f * scale, true));
        g.drawText (line1, box.removeFromTop (box.getHeight() * 0.58f),
                    juce::Justification::centred, false);

        if (line2.isNotEmpty())
        {
            g.setColour (colours::textDim);
            g.setFont (monoFont (6.5f * scale));
            g.drawText (line2, box, juce::Justification::centred, false);
        }
    }
}

ChannelDisplay::ChannelDisplay (KChannelAudioProcessor& p)
    : processor (p)
{
    setInterceptsMouseClicks (false, false);
}

float ChannelDisplay::xForHz (float hz, juce::Rectangle<float> plot)
{
    return kchannel_ui::xForHz (hz, plot);
}

float ChannelDisplay::yForDb (float db, juce::Rectangle<float> plot)
{
    return kchannel_ui::yForDb (db, plot);
}

void ChannelDisplay::paint (juce::Graphics& g)
{
    const float scale = juce::jlimit (0.8f, 1.5f, (float) getWidth() / 1340.0f);
    auto bounds = getLocalBounds().toFloat();

    juce::ColourGradient panelGrad (colours::panel.brighter (0.035f), bounds.getCentreX(), bounds.getY(),
                                    colours::panel.darker (0.35f), bounds.getCentreX(), bounds.getBottom(), false);
    g.setGradientFill (panelGrad);
    g.fillRoundedRectangle (bounds, 7.0f);
    g.setColour (colours::outline);
    g.drawRoundedRectangle (bounds.reduced (0.5f), 7.0f, 1.0f);

    auto inner = bounds.reduced (11.0f * scale, 8.0f * scale);
    auto legend = inner.removeFromTop (18.0f * scale);
    auto flow = inner.removeFromBottom (39.0f * scale);
    auto axis = inner.removeFromBottom (14.0f * scale);
    auto dbCol = inner.removeFromLeft (28.0f * scale);

    inner.removeFromRight (7.0f * scale);
    auto meters = inner.removeFromRight (166.0f * scale);
    auto plot = inner;

    //==================================================================== legend
    g.setColour (colours::accent);
    g.fillEllipse (legend.removeFromLeft (6.0f * scale).withSizeKeepingCentre (5.0f * scale, 5.0f * scale));
    legend.removeFromLeft (5.0f * scale);

    g.setColour (colours::textDim);
    g.setFont (uiFont (8.5f * scale, true));
    g.drawText ("EQ RESPONSE", legend.removeFromLeft (94.0f * scale),
                juce::Justification::centredLeft, false);

    g.setColour (colours::accentCool);
    g.fillEllipse (legend.removeFromLeft (8.0f * scale).withSizeKeepingCentre (5.0f * scale, 5.0f * scale));
    legend.removeFromLeft (5.0f * scale);
    g.setColour (colours::textDim);
    g.drawText ("QUELL", legend.removeFromLeft (54.0f * scale),
                juce::Justification::centredLeft, false);

    const bool post = processor.isQuellPost();
    const auto chainLabel = post ? "QUELL POST COMP" : "QUELL PRE COMP";
    g.setColour (colours::accentCool);
    g.setFont (uiFont (8.5f * scale, true));
    g.drawText (chainLabel, legend, juce::Justification::centredRight, false);

    //====================================================================== grid
    g.setColour (colours::outline.withAlpha (0.8f));
    g.drawRect (plot, 1.0f);

    const int gridHz[] = { 30, 50, 100, 200, 300, 500, 1000, 2000, 3000, 5000, 10000, 20000 };

    for (int hz : gridHz)
    {
        const float x = xForHz ((float) hz, plot);
        const bool major = hz == 100 || hz == 1000 || hz == 10000;

        g.setColour (colours::outline.withAlpha (major ? 0.7f : 0.30f));
        g.drawVerticalLine (juce::roundToInt (x), plot.getY(), plot.getBottom());
    }

    for (int db = -18; db <= 18; db += 6)
    {
        const float y = yForDb ((float) db, plot);

        g.setColour (db == 0 ? colours::outline.brighter (0.35f) : colours::outline.withAlpha (0.32f));
        g.drawHorizontalLine (juce::roundToInt (y), plot.getX(), plot.getRight());
    }

    //================================================================= EQ curve
    const auto settings = processor.getEqSettings();
    const double sampleRate = processor.getSampleRate() > 0.0 ? processor.getSampleRate() : 48000.0;

    constexpr int numPoints = 240;
    std::array<double, numPoints> freqs {};
    std::array<float, numPoints> response {};

    for (int i = 0; i < numPoints; ++i)
    {
        const double t = (double) i / (double) (numPoints - 1);
        freqs[(size_t) i] = 20.0 * std::pow (1000.0, t);
    }

    kchannel::eqResponseDb (settings, sampleRate, freqs.data(), response.data(), numPoints);

    juce::Path curve, fill;
    fill.startNewSubPath (plot.getX(), plot.getCentreY());

    for (int i = 0; i < numPoints; ++i)
    {
        const float x = plot.getX() + (float) i / (float) (numPoints - 1) * plot.getWidth();
        const float y = yForDb (response[(size_t) i], plot);

        if (i == 0)
            curve.startNewSubPath (x, y);
        else
            curve.lineTo (x, y);

        fill.lineTo (x, y);
    }

    fill.lineTo (plot.getRight(), plot.getCentreY());
    fill.closeSubPath();

    {
        juce::ColourGradient fillGrad (colours::accent.withAlpha (0.20f), plot.getCentreX(), plot.getY(),
                                       colours::accent.withAlpha (0.0f), plot.getCentreX(), plot.getBottom(), false);
        g.setGradientFill (fillGrad);
        g.fillPath (fill);
    }

    g.setColour ((settings.filtersOn || settings.eqOn) ? colours::accent : colours::textDim);
    g.strokePath (curve, juce::PathStrokeType (1.8f * scale, juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded));

    // Band markers. These make the curve readable even when all bands are flat.
    struct BandMarker { const char* name; float hz; juce::Colour colour; };
    const BandMarker bands[] = {
        { "LF", settings.lfFreq, colours::accent },
        { "LM", settings.lmFreq, colours::accentCool },
        { "HM", settings.hmFreq, colours::reduction },
        { "HF", settings.hfFreq, colours::ok }
    };

    for (const auto& band : bands)
    {
        const float x = xForHz (band.hz, plot);

        g.setColour (band.colour.withAlpha (0.35f));
        g.drawVerticalLine (juce::roundToInt (x), plot.getY(), plot.getBottom());

        g.setColour (band.colour.withAlpha (0.9f));
        g.setFont (monoFont (7.0f * scale));
        g.drawText (band.name, juce::Rectangle<float> (x - 12.0f * scale, plot.getY() + 2.0f * scale,
                                                      24.0f * scale, 10.0f * scale),
                    juce::Justification::centred, false);
    }

    //=================================================================== meters
    const float meterHeight = meters.getHeight() / 5.0f;
    auto meterRow = [&meters, meterHeight] (int index)
    {
        return meters.withY (meters.getY() + (float) index * meterHeight).withHeight (meterHeight);
    };

    const auto inNorm = peakToNorm (processor.inputPeak.load());
    const auto outNorm = peakToNorm (processor.outputPeak.load());
    const auto compNorm = juce::jlimit (0.0f, 1.0f, processor.compGrDb.load() / 24.0f);
    const auto gateNorm = juce::jlimit (0.0f, 1.0f, processor.gateGrDb.load() / 60.0f);
    const auto quellNorm = juce::jlimit (0.0f, 1.0f, processor.quellCutDb.load() / 24.0f);

    drawMeter (g, meterRow (0), "IN", inNorm, colours::ok,
               juce::String (juce::Decibels::gainToDecibels (processor.inputPeak.load(), -60.0f), 1), scale);
    drawMeter (g, meterRow (1), "OUT", outNorm, colours::accent,
               juce::String (juce::Decibels::gainToDecibels (processor.outputPeak.load(), -60.0f), 1), scale);
    drawMeter (g, meterRow (2), "COMP", compNorm, colours::reduction,
               "-" + juce::String (processor.compGrDb.load(), 1), scale);
    drawMeter (g, meterRow (3), "GATE", gateNorm, colours::reduction,
               "-" + juce::String (processor.gateGrDb.load(), 1), scale);
    drawMeter (g, meterRow (4), "QUELL", quellNorm, colours::accentCool,
               "-" + juce::String (processor.quellCutDb.load(), 1), scale);

    //=============================================================== axis labels
    g.setFont (uiFont (8.0f * scale));
    g.setColour (colours::textDim.withAlpha (0.9f));

    struct FreqLabel { int hz; const char* text; };
    const FreqLabel freqLabels[] = { { 30, "30" }, { 100, "100" }, { 300, "300" },
                                     { 1000, "1k" }, { 3000, "3k" }, { 10000, "10k" } };

    for (const auto& fl : freqLabels)
    {
        const float x = xForHz ((float) fl.hz, plot);
        g.drawText (fl.text,
                    juce::Rectangle<float> (x - 18.0f * scale, axis.getY(), 36.0f * scale, axis.getHeight()),
                    juce::Justification::centred, false);
    }

    for (int db = -18; db <= 18; db += 6)
    {
        const float y = yForDb ((float) db, plot);
        const auto text = (db > 0 ? "+" : "") + juce::String (db);
        g.drawText (text,
                    juce::Rectangle<float> (dbCol.getX(), y - 6.0f * scale,
                                            dbCol.getWidth() - 5.0f * scale, 12.0f * scale),
                    juce::Justification::centredRight, false);
    }

    g.drawText ("Hz", axis.removeFromRight (20.0f * scale),
                juce::Justification::centredRight, false);

    //================================================================ signal flow
    std::array<juce::String, 7> stageNames { "IN", "FILTER", "EQ", "QUELL", "GATE", "COMP", "OUT" };

    if (post)
    {
        stageNames[3] = "GATE";
        stageNames[4] = "COMP";
        stageNames[5] = "QUELL";
    }

    const float gap = 7.0f * scale;
    const float boxWidth = juce::jmin (86.0f * scale,
                                       (flow.getWidth() - gap * 6.0f) / 7.0f);
    const float totalWidth = boxWidth * 7.0f + gap * 6.0f;
    float x = flow.getCentreX() - totalWidth * 0.5f;

    for (int i = 0; i < 7; ++i)
    {
        const bool isQuell = stageNames[(size_t) i] == "QUELL";
        const bool isComp = stageNames[(size_t) i] == "COMP";
        const auto colour = isQuell ? colours::accentCool : (isComp ? colours::accent : colours::textDim);

        if (i > 0)
        {
            juce::Path arrow;
            const float ax = x - gap + 1.0f;
            const float ay = flow.getCentreY();
            arrow.startNewSubPath (ax, ay - 2.5f * scale);
            arrow.lineTo (ax + 4.0f * scale, ay);
            arrow.lineTo (ax, ay + 2.5f * scale);
            arrow.closeSubPath();

            g.setColour (colours::textDim.withAlpha (0.6f));
            g.fillPath (arrow);
        }

        const auto box = juce::Rectangle<float> (x, flow.getY() + 4.0f * scale,
                                                 boxWidth, flow.getHeight() - 8.0f * scale);

        const auto sub = isQuell ? (post ? "POST" : "PRE") : (isComp ? "RMS" : "");
        drawStage (g, box, stageNames[(size_t) i], sub, colour, isQuell || isComp, scale);
        x += boxWidth + gap;
    }
}
} // namespace kchannel_ui
