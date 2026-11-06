#include "ResponseDisplay.h"

#include "../PluginProcessor.h"

namespace micsmith_ui
{
namespace
{
    juce::Path buildCurvePath (const micsmith::CurveEvaluator& ev,
                               juce::Rectangle<float> plot,
                               float minHz, float maxHz, double offsetDb)
    {
        juce::Path path;

        const int steps = 360;
        const double lo = std::log10 ((double) minHz);
        const double hi = std::log10 ((double) maxHz);

        for (int i = 0; i <= steps; ++i)
        {
            const double t = (double) i / (double) steps;
            const double hz = std::pow (10.0, lo + t * (hi - lo));

            float db = (float) (ev.dbAt (hz) + offsetDb);
            db = juce::jlimit (-ResponseDisplay::dbRange, ResponseDisplay::dbRange, db);

            const float x = plot.getX() + (float) t * plot.getWidth();
            const float y = plot.getCentreY() - (db / ResponseDisplay::dbRange) * (plot.getHeight() * 0.5f);

            if (i == 0) path.startNewSubPath (x, y);
            else        path.lineTo (x, y);
        }

        return path;
    }

    void drawLegendEntry (juce::Graphics& g, juce::Rectangle<int>& row, juce::Colour c,
                          const juce::String& label, bool dashed, float scale)
    {
        auto lineArea = row.removeFromLeft (juce::roundToInt (22.0f * scale)).toFloat();
        lineArea = lineArea.withSizeKeepingCentre (lineArea.getWidth(), 1.5f);

        if (dashed)
        {
            const float dashLens[] = { 3.0f, 3.0f };
            for (float x = lineArea.getX(); x < lineArea.getRight(); x += 6.0f)
                g.setColour (c),
                g.fillRect (juce::Rectangle<float> (x, lineArea.getY(),
                                                    juce::jmin (3.0f, lineArea.getRight() - x), 1.5f));
        }
        else
        {
            g.setColour (c);
            g.fillRect (lineArea);
        }

        g.setColour (colours::textDim);
        g.setFont (uiFont (9.5f * scale, true));
        g.drawText (label, row.removeFromLeft (juce::roundToInt (58.0f * scale)),
                    juce::Justification::centredLeft, false);
    }
}

ResponseDisplay::ResponseDisplay (MicSmithAudioProcessor& p)
    : processor (p)
{
    setInterceptsMouseClicks (false, false);
}

float ResponseDisplay::xForFreq (float hz, juce::Rectangle<float> plot)
{
    const float lo = std::log10 (minHz);
    const float hi = std::log10 (maxHz);
    const float t = (std::log10 (juce::jlimit (minHz, maxHz, hz)) - lo) / (hi - lo);
    return plot.getX() + t * plot.getWidth();
}

float ResponseDisplay::yForDb (float db, juce::Rectangle<float> plot)
{
    const float t = juce::jlimit (-1.0f, 1.0f, db / dbRange);
    return plot.getCentreY() - t * (plot.getHeight() * 0.5f);
}

void ResponseDisplay::drawCurve (juce::Graphics& g, const micsmith::CurveEvaluator& ev, double sr,
                                 juce::Rectangle<float> plot, juce::Colour colour,
                                 float thickness, bool dashed, double offsetDb)
{
    juce::ignoreUnused (sr);

    const auto path = buildCurvePath (ev, plot, minHz, maxHz, offsetDb);
    const juce::PathStrokeType stroke (thickness, juce::PathStrokeType::curved,
                                       juce::PathStrokeType::rounded);

    if (dashed)
    {
        juce::Path dashedPath;
        const float dashLens[] = { 2.5f, 3.0f };
        stroke.createDashedStroke (dashedPath, path, dashLens, 2);
        g.setColour (colour);
        g.strokePath (dashedPath, stroke);
    }
    else
    {
        g.setColour (colour);
        g.strokePath (path, stroke);
    }
}

void ResponseDisplay::paint (juce::Graphics& g)
{
    const float scale = juce::jlimit (0.85f, 1.7f, (float) getWidth() / 732.0f);

    auto bounds = getLocalBounds().toFloat();

    juce::ColourGradient panelGrad (colours::panel.brighter (0.03f), bounds.getCentreX(), bounds.getY(),
                                    colours::panel.darker (0.35f), bounds.getCentreX(), bounds.getBottom(), false);
    g.setGradientFill (panelGrad);
    g.fillRoundedRectangle (bounds, 6.0f);
    g.setColour (colours::outline);
    g.drawRoundedRectangle (bounds.reduced (0.5f), 6.0f, 1.0f);

    auto plot = bounds.reduced (10.0f * scale, 8.0f * scale);
    auto legendRow = plot.removeFromTop (16.0f * scale).toNearestInt();
    auto freqRow = plot.removeFromBottom (14.0f * scale);
    auto dbCol = plot.removeFromLeft (26.0f * scale);

    //============================================================== plot frame
    g.setColour (colours::outline.withAlpha (0.85f));
    g.drawRect (plot, 1.0f);

    const double sr = processor.getSampleRate() > 0.0 ? processor.getSampleRate() : 48000.0;

    micsmith::ModelParams params;
    processor.fillDisplayParams (params);

    //==================================================================== grid
    const int gridHz[] = { 30, 50, 100, 200, 300, 500, 1000, 2000, 3000, 5000, 10000, 20000 };

    for (int hz : gridHz)
    {
        const float x = xForFreq ((float) hz, plot);
        const bool major = hz == 100 || hz == 1000 || hz == 10000;

        g.setColour (colours::outline.withAlpha (major ? 0.75f : 0.35f));
        g.drawVerticalLine (juce::roundToInt (x), plot.getY(), plot.getBottom());
    }

    for (int db = -18; db <= 18; db += 6)
    {
        const float y = yForDb ((float) db, plot);

        g.setColour (db == 0 ? colours::outline.brighter (0.35f) : colours::outline.withAlpha (0.35f));
        g.drawHorizontalLine (juce::roundToInt (y), plot.getX(), plot.getRight());
    }

    //======================================================= curves and shading
    micsmith::CurveEvaluator sourceEv, targetEv, pairEv;
    sourceEv.buildSingleModel (params.sourceIndex, sr);
    targetEv.buildSingleModel (params.targetIndex, sr);
    pairEv.build (params, sr);

    const double autoGain = micsmith::autoGainDb (params, sr);

    {
        auto fillPath = buildCurvePath (pairEv, plot, minHz, maxHz, autoGain);
        fillPath.lineTo (plot.getRight(), plot.getCentreY());
        fillPath.lineTo (plot.getX(), plot.getCentreY());
        fillPath.closeSubPath();

        juce::ColourGradient fillGrad (colours::accent.withAlpha (0.22f), plot.getCentreX(), plot.getY(),
                                       colours::accent.withAlpha (0.0f), plot.getCentreX(), plot.getBottom(), false);
        g.setGradientFill (fillGrad);
        g.fillPath (fillPath);
    }

    drawCurve (g, sourceEv, sr, plot, colours::accentCool.withAlpha (0.75f), 1.2f * scale, true, 0.0);
    drawCurve (g, targetEv, sr, plot, colours::accent.withAlpha (0.55f), 1.2f * scale, true, 0.0);
    drawCurve (g, pairEv,   sr, plot, colours::accent, 2.0f * scale, false, autoGain);

    //================================================================== legends
    auto row = legendRow;
    row.setY (legendRow.getY() + 1);

    drawLegendEntry (g, row, colours::accent, "RESULT", false, scale);
    drawLegendEntry (g, row, colours::accent.withAlpha (0.6f), "TARGET", true, scale);
    drawLegendEntry (g, row, colours::accentCool.withAlpha (0.8f), "SOURCE", true, scale);

    {
        const auto txt = juce::String ("AUTO ") + (autoGain >= 0.0 ? "+" : "")
                       + juce::String (autoGain, 1) + " dB";
        g.setColour (colours::textDim);
        g.setFont (uiFont (9.5f * scale, true));
        g.drawText (txt, legendRow, juce::Justification::centredRight, false);
    }

    //============================================================= axis labels
    g.setFont (uiFont (9.0f * scale));
    g.setColour (colours::textDim.withAlpha (0.9f));

    struct FreqLabel { int hz; const char* txt; };
    const FreqLabel freqLabels[] = { { 30, "30" }, { 100, "100" }, { 300, "300" },
                                     { 1000, "1k" }, { 3000, "3k" }, { 10000, "10k" } };

    for (const auto& fl : freqLabels)
    {
        const float x = xForFreq ((float) fl.hz, plot);
        g.drawText (fl.txt,
                    juce::Rectangle<float> (x - 20.0f, freqRow.getY(), 40.0f, freqRow.getHeight()),
                    juce::Justification::centred, false);
    }

    for (int db = -12; db <= 12; db += 6)
    {
        const float y = yForDb ((float) db, plot);
        const auto txt = (db > 0 ? "+" : "") + juce::String (db);
        g.drawText (txt,
                    juce::Rectangle<float> (dbCol.getX(), y - 7.0f, dbCol.getWidth() - 4.0f, 14.0f),
                    juce::Justification::centredRight, false);
    }
}
} // namespace micsmith_ui
