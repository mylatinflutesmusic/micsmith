#include "SpectrumDisplay.h"

#include "../PluginProcessor.h"

namespace quell_ui
{
namespace
{
    constexpr float minHz = 20.0f;
    constexpr float maxHz = 20000.0f;
    constexpr float minDb = -96.0f;
    constexpr float maxDb = 0.0f;
}

SpectrumDisplay::SpectrumDisplay (QuellAudioProcessor& p)
    : processor (p)
{
    setInterceptsMouseClicks (false, false);

    shownInput.fill (-110.0f);
    shownCut.fill (0.0f);
}

float SpectrumDisplay::xForHz (float hz, juce::Rectangle<float> plot)
{
    const float lo = std::log10 (minHz);
    const float hi = std::log10 (maxHz);
    const float t = (std::log10 (juce::jlimit (minHz, maxHz, hz)) - lo) / (hi - lo);
    return plot.getX() + t * plot.getWidth();
}

float SpectrumDisplay::yForDb (float db, juce::Rectangle<float> plot)
{
    const float t = juce::jlimit (0.0f, 1.0f, (db - minDb) / (maxDb - minDb));
    return plot.getBottom() - t * plot.getHeight();
}

void SpectrumDisplay::update()
{
    quell::DisplaySnapshot snapshot;

    if (! processor.readDisplay (snapshot))
        return;

    for (int i = 0; i < numPoints; ++i)
    {
        const float inTarget = juce::jlimit (-110.0f, 3.0f, snapshot.inputDb[i]);
        const float cutTarget = juce::jlimit (0.0f, 36.0f, snapshot.cutDb[i]);

        shownInput[(size_t) i] += (inTarget - shownInput[(size_t) i]) * 0.35f;
        shownCut[(size_t) i] += (cutTarget - shownCut[(size_t) i]) * 0.25f;
    }

    const float targetMax = juce::jlimit (0.0f, 36.0f, processor.maxCutDb.load());
    shownMaxCut = targetMax > shownMaxCut ? targetMax
                                          : shownMaxCut * 0.90f + targetMax * 0.10f;
}

void SpectrumDisplay::paint (juce::Graphics& g)
{
    const float scale = juce::jlimit (0.8f, 1.6f, (float) getWidth() / 860.0f);
    auto bounds = getLocalBounds().toFloat();

    juce::ColourGradient panelGrad (colours::panel.brighter (0.03f), bounds.getCentreX(), bounds.getY(),
                                    colours::panel.darker (0.30f), bounds.getCentreX(), bounds.getBottom(), false);
    g.setGradientFill (panelGrad);
    g.fillRoundedRectangle (bounds, 7.0f);
    g.setColour (colours::outline);
    g.drawRoundedRectangle (bounds.reduced (0.5f), 7.0f, 1.0f);

    auto plot = bounds.reduced (13.0f * scale, 10.0f * scale);
    auto legendRow = plot.removeFromTop (17.0f * scale);
    auto freqRow = plot.removeFromBottom (15.0f * scale);
    auto dbColumn = plot.removeFromLeft (30.0f * scale);

    g.setColour (colours::outline.withAlpha (0.7f));
    g.drawRect (plot, 1.0f);

    //==================================================================== grid
    const int gridFreqs[] = { 30, 50, 100, 200, 300, 500, 1000, 2000, 3000, 5000, 10000, 20000 };

    for (int hz : gridFreqs)
    {
        const float x = xForHz ((float) hz, plot);
        const bool major = hz == 100 || hz == 1000 || hz == 10000;

        g.setColour (colours::outline.withAlpha (major ? 0.7f : 0.32f));
        g.drawVerticalLine (juce::roundToInt (x), plot.getY(), plot.getBottom());
    }

    for (int db = -96; db <= 0; db += 24)
    {
        const float y = yForDb ((float) db, plot);

        g.setColour (colours::outline.withAlpha (db == 0 ? 0.65f : 0.28f));
        g.drawHorizontalLine (juce::roundToInt (y), plot.getX(), plot.getRight());
    }

    const bool bypassed = processor.apvts.getRawParameterValue ("bypass")->load() > 0.5f;

    //================================================================ spectrum
    juce::Path inputPath, inputFill;

    for (int i = 0; i < numPoints; ++i)
    {
        const float x = plot.getX() + (float) i / (float) (numPoints - 1) * plot.getWidth();
        const float y = yForDb (shownInput[(size_t) i], plot);

        if (i == 0)
        {
            inputPath.startNewSubPath (x, y);
            inputFill.startNewSubPath (x, plot.getBottom());
            inputFill.lineTo (x, y);
        }
        else
        {
            inputPath.lineTo (x, y);
            inputFill.lineTo (x, y);
        }
    }

    inputFill.lineTo (plot.getRight(), plot.getBottom());
    inputFill.closeSubPath();

    {
        juce::ColourGradient fillGrad (colours::accent.withAlpha (0.16f), plot.getCentreX(), plot.getY(),
                                       colours::accent.withAlpha (0.0f), plot.getCentreX(), plot.getBottom(), false);
        g.setGradientFill (fillGrad);
        g.fillPath (inputFill);
    }

    g.setColour (colours::accent.withAlpha (bypassed ? 0.35f : 0.9f));
    g.strokePath (inputPath, juce::PathStrokeType (1.5f * scale, juce::PathStrokeType::curved,
                                                  juce::PathStrokeType::rounded));

    //================================================= reduction (amber region)
    const float cutScale = plot.getHeight() * 0.62f / 24.0f;

    juce::Path cutCurve, cutFill;

    for (int i = 0; i < numPoints; ++i)
    {
        const float x = plot.getX() + (float) i / (float) (numPoints - 1) * plot.getWidth();
        const float yIn = yForDb (shownInput[(size_t) i], plot);
        const float cutPixels = juce::jmin (plot.getHeight() * 0.62f,
                                           shownCut[(size_t) i] * cutScale);
        const float yCut = juce::jmin (plot.getBottom(), yIn + cutPixels);

        if (i == 0)
        {
            cutCurve.startNewSubPath (x, yCut);
            cutFill.startNewSubPath (x, yIn);
            cutFill.lineTo (x, yCut);
        }
        else
        {
            cutCurve.lineTo (x, yCut);
            cutFill.lineTo (x, yCut);
        }
    }

    for (int i = numPoints - 1; i >= 0; --i)
    {
        const float x = plot.getX() + (float) i / (float) (numPoints - 1) * plot.getWidth();
        cutFill.lineTo (x, yForDb (shownInput[(size_t) i], plot));
    }

    cutFill.closeSubPath();

    {
        juce::ColourGradient cutGrad (colours::reduction.withAlpha (0.45f), plot.getCentreX(), plot.getY(),
                                      colours::reduction.withAlpha (0.10f), plot.getCentreX(), plot.getBottom(), false);
        g.setGradientFill (cutGrad);
        g.fillPath (cutFill);
    }

    g.setColour (colours::reduction.withAlpha (bypassed ? 0.3f : 0.95f));
    g.strokePath (cutCurve, juce::PathStrokeType (1.3f * scale, juce::PathStrokeType::curved,
                                                  juce::PathStrokeType::rounded));

    //================================================================== legend
    auto legend = legendRow;
    auto swatch = legend.removeFromLeft (10.0f * scale).withSizeKeepingCentre (6.0f * scale, 6.0f * scale);
    g.setColour (colours::accent);
    g.fillEllipse (swatch);

    legend.removeFromLeft (5.0f * scale);
    g.setColour (colours::textDim);
    g.setFont (uiFont (9.0f * scale, true));
    g.drawText ("LIVE SPECTRUM", legend.removeFromLeft (94.0f * scale),
                juce::Justification::centredLeft, false);

    auto swatch2 = legend.removeFromLeft (10.0f * scale).withSizeKeepingCentre (6.0f * scale, 6.0f * scale);
    g.setColour (colours::reduction);
    g.fillEllipse (swatch2);

    legend.removeFromLeft (5.0f * scale);
    g.setColour (colours::textDim);
    g.drawText ("REDUCTION", legend.removeFromLeft (80.0f * scale),
                juce::Justification::centredLeft, false);

    if (bypassed)
    {
        g.setColour (colours::reduction.withAlpha (0.16f));
        g.fillRoundedRectangle (legendRow.removeFromRight (74.0f * scale).reduced (1.0f), 3.0f);
        g.setColour (colours::reduction);
        g.setFont (uiFont (9.0f * scale, true));
        g.drawText ("BYPASSED", legendRow, juce::Justification::centred, false);
    }
    else
    {
        const auto text = "MAX CUT  -" + juce::String (shownMaxCut, 1) + " dB";
        g.setColour (colours::reduction.withAlpha (0.9f));
        g.setFont (monoFont (9.5f * scale));
        g.drawText (text, legendRow, juce::Justification::centredRight, false);
    }

    //=============================================================== axis text
    g.setFont (monoFont (9.0f * scale));
    g.setColour (colours::textDim.withAlpha (0.9f));

    struct FreqLabel { int hz; const char* text; };
    const FreqLabel freqLabels[] = { { 30, "30" }, { 100, "100" }, { 300, "300" },
                                     { 1000, "1k" }, { 3000, "3k" }, { 10000, "10k" } };

    for (const auto& fl : freqLabels)
    {
        const float x = xForHz ((float) fl.hz, plot);
        g.drawText (fl.text,
                    juce::Rectangle<float> (x - 20.0f, freqRow.getY(), 40.0f, freqRow.getHeight()),
                    juce::Justification::centred, false);
    }

    for (int db = -96; db <= 0; db += 24)
    {
        const float y = yForDb ((float) db, plot);
        g.drawText (juce::String (db),
                    juce::Rectangle<float> (dbColumn.getX(), y - 7.0f, dbColumn.getWidth() - 5.0f, 14.0f),
                    juce::Justification::centredRight, false);
    }

    g.drawText ("Hz", freqRow.removeFromRight (22.0f * scale),
                juce::Justification::centredRight, false);
}
} // namespace quell_ui
