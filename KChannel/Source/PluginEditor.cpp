#include "PluginEditor.h"

#include "PluginProcessor.h"

#include <cmath>
#include <utility>

namespace
{
    constexpr int baseWidth = 1400;
    constexpr int baseHeight = 840;

    juce::String dbText (double v)
    {
        return (v > 0.0 ? "+" : "") + juce::String (v, 1) + " dB";
    }

    juce::String hzText (double v)
    {
        if (v < 1000.0)
            return juce::String (juce::roundToInt (v)) + " Hz";

        return juce::String (v / 1000.0, v < 10000.0 ? 2 : 1) + " kHz";
    }

    juce::String msText (double v)
    {
        if (v < 10.0)
            return juce::String (v, 1) + " ms";

        return juce::String (juce::roundToInt (v)) + " ms";
    }

    juce::String ratioText (double v)
    {
        return juce::String (v, 1) + ":1";
    }

    juce::String percentText (double v)
    {
        return juce::String (juce::roundToInt (v)) + " %";
    }

    juce::String qText (double v)
    {
        return juce::String (v, 2);
    }

    double parseValue (const juce::String& text)
    {
        return text.getDoubleValue();
    }
}

//==============================================================================
KChannelAudioProcessorEditor::KChannelAudioProcessorEditor (KChannelAudioProcessor& p)
    : juce::AudioProcessorEditor (&p), processor (p), display (p)
{
    setLookAndFeel (&lnf);
    setName ("K Channel");

    configureButton (filtersButton, "FILTERS", "Engage the high-pass and low-pass filters");
    configureButton (eqButton, "EQ", "Engage the four-band equalizer");
    configureButton (compButton, "COMP", "Engage the RMS compressor");
    configureButton (gateButton, "GATE", "Engage the downward gate");
    configureButton (quellButton, "QUELL", "Engage Quell resonance suppression");
    configureButton (quellPostButton, "Q POST", "Place Quell after the compressor; off is before it (default)");
    configureButton (phaseButton, "PHASE", "Invert the input polarity");
    configureButton (bypassButton, "BYPASS", "Hard bypass, with the same reported latency");

    addAndMakeVisible (display);

    //============================================================== input/filters
    addKnob (Group::input, "input", "Input", "Input trim before the channel", dbText);
    addKnob (Group::input, "hpf", "HPF", "High-pass filter frequency", hzText);
    addKnob (Group::input, "lpf", "LPF", "Low-pass filter frequency", hzText);

    //======================================================================= EQ
    addKnob (Group::eq, "lfFreq", "Freq", "Low shelf frequency", hzText);
    addKnob (Group::eq, "lfGain", "Gain", "Low shelf gain", dbText);

    addKnob (Group::eq, "lmFreq", "Freq", "Low-mid bell frequency", hzText);
    addKnob (Group::eq, "lmGain", "Gain", "Low-mid bell gain", dbText);
    addKnob (Group::eq, "lmQ", "Q", "Low-mid bell width", qText);

    addKnob (Group::eq, "hmFreq", "Freq", "High-mid bell frequency", hzText);
    addKnob (Group::eq, "hmGain", "Gain", "High-mid bell gain", dbText);
    addKnob (Group::eq, "hmQ", "Q", "High-mid bell width", qText);

    addKnob (Group::eq, "hfFreq", "Freq", "High shelf frequency", hzText);
    addKnob (Group::eq, "hfGain", "Gain", "High shelf gain", dbText);

    //=============================================================== compressor
    addKnob (Group::comp, "compThreshold", "Thresh", "Compressor threshold", dbText);
    addKnob (Group::comp, "compRatio", "Ratio", "Compressor ratio", ratioText);
    addKnob (Group::comp, "compAttack", "Attack", "Compressor attack time", msText);
    addKnob (Group::comp, "compRelease", "Release", "Compressor release time", msText);
    addKnob (Group::comp, "compMakeup", "Makeup", "Compressor make-up gain", dbText);

    //====================================================================== gate
    addKnob (Group::gate, "gateThreshold", "Thresh", "Gate threshold", dbText);
    addKnob (Group::gate, "gateRange", "Range", "Gate attenuation below threshold", dbText);
    addKnob (Group::gate, "gateAttack", "Attack", "Gate opening time", msText);
    addKnob (Group::gate, "gateRelease", "Release", "Gate closing time", msText);

    //===================================================================== Quell
    addKnob (Group::quell, "qDepth", "Depth", "How much resonance Quell removes", percentText);
    addKnob (Group::quell, "qSensitivity", "Sens", "How far a peak must rise before Quell acts", percentText);
    addKnob (Group::quell, "qSelectivity", "Select", "How tightly Quell focuses on narrow peaks", percentText);
    addKnob (Group::quell, "qSpeed", "Speed", "Quell attack/release speed", percentText);
    addKnob (Group::quell, "qLowTame", "Low", "Back Quell off in the low end", percentText);
    addKnob (Group::quell, "qHighTame", "High", "Back Quell off in the top end", percentText);

    //==================================================================== master
    addKnob (Group::master, "output", "Output", "Output trim after the channel", dbText);

    filtersAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        processor.apvts, "filtersOn", filtersButton);
    eqAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        processor.apvts, "eqOn", eqButton);
    compAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        processor.apvts, "compOn", compButton);
    gateAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        processor.apvts, "gateOn", gateButton);
    quellAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        processor.apvts, "quellOn", quellButton);
    quellPostAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        processor.apvts, "quellPost", quellPostButton);
    phaseAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        processor.apvts, "phase", phaseButton);
    bypassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        processor.apvts, "bypass", bypassButton);

    setResizable (true, true);

    constexpr int minWidth = 900;
    constexpr int minHeight = 540;
    constexpr double aspectRatio = (double) baseWidth / (double) baseHeight;

    // Open comfortably inside the actual screen. A 1400x840 default is taller
    // than the usable area of many laptops, which made the bottom controls
    // impossible to reach before the window could be resized.
    int initialWidth = baseWidth;

    if (auto* screen = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay())
    {
        const auto area = screen->userArea;
        const int maxWidth = juce::jmax (minWidth, juce::roundToInt ((double) area.getWidth() * 0.92));
        const int maxHeight = juce::jmax (minHeight, juce::roundToInt ((double) area.getHeight() * 0.92));

        initialWidth = juce::jmin (baseWidth, maxWidth);
        int initialHeight = juce::roundToInt ((double) initialWidth / aspectRatio);

        if (initialHeight > maxHeight)
        {
            initialHeight = maxHeight;
            initialWidth = juce::roundToInt ((double) initialHeight * aspectRatio);
        }

        initialWidth = juce::jmax (minWidth, initialWidth);
    }

    setResizeLimits (minWidth, minHeight, baseWidth * 2, baseHeight * 2);

    if (auto* boundsConstrainer = getConstrainer())
        boundsConstrainer->setFixedAspectRatio (aspectRatio);

    setSize (initialWidth, juce::roundToInt ((double) initialWidth / aspectRatio));
    startTimerHz (30);
}

KChannelAudioProcessorEditor::~KChannelAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

//==============================================================================
void KChannelAudioProcessorEditor::configureButton (juce::TextButton& button,
                                                    const juce::String& text,
                                                    const juce::String& tooltip)
{
    button.setButtonText (text);
    button.setClickingTogglesState (true);
    button.setTooltip (tooltip);
    button.setColour (juce::TextButton::textColourOffId, kchannel_ui::colours::textDim);
    button.setColour (juce::TextButton::textColourOnId, kchannel_ui::colours::background);
    addAndMakeVisible (button);
}

void KChannelAudioProcessorEditor::addKnob (Group group, const char* id, const char* caption,
                                            const char* tooltip,
                                            std::function<juce::String (double)> formatter)
{
    auto knob = std::make_unique<kchannel_ui::Knob> (processor.apvts, id, caption);
    knob->slider.setTooltip (tooltip);
    knob->setFormatter (std::move (formatter), parseValue);

    addAndMakeVisible (*knob);

    int column = 0;
    int row = 0;

    switch (group)
    {
        case Group::input:
            if (juce::String (id) == "hpf") column = 1;
            else if (juce::String (id) == "lpf") column = 2;
            break;

        case Group::eq:
            if (juce::String (id).startsWith ("lf"))       { column = 0; row = 0; }
            else if (juce::String (id).startsWith ("lm"))  { column = 1; row = 0; }
            else if (juce::String (id).startsWith ("hm"))  { column = 2; row = 0; }
            else if (juce::String (id).startsWith ("hf"))  { column = 3; row = 0; }

            if (juce::String (id).endsWith ("Freq"))       row = 0;
            else if (juce::String (id).endsWith ("Gain"))  row = 1;
            else if (juce::String (id).endsWith ("Q"))     row = 2;
            break;

        case Group::comp:
            if (juce::String (id) == "compThreshold")      { column = 0; row = 0; }
            else if (juce::String (id) == "compRatio")     { column = 1; row = 0; }
            else if (juce::String (id) == "compAttack")    { column = 2; row = 0; }
            else if (juce::String (id) == "compRelease")   { column = 0; row = 1; }
            else if (juce::String (id) == "compMakeup")    { column = 1; row = 1; }
            break;

        case Group::gate:
            if (juce::String (id) == "gateThreshold")      { column = 0; row = 0; }
            else if (juce::String (id) == "gateRange")     { column = 1; row = 0; }
            else if (juce::String (id) == "gateAttack")    { column = 0; row = 1; }
            else if (juce::String (id) == "gateRelease")   { column = 1; row = 1; }
            break;

        case Group::quell:
            if (juce::String (id) == "qDepth")             { column = 0; row = 0; }
            else if (juce::String (id) == "qSensitivity")  { column = 1; row = 0; }
            else if (juce::String (id) == "qSelectivity")  { column = 2; row = 0; }
            else if (juce::String (id) == "qSpeed")        { column = 0; row = 1; }
            else if (juce::String (id) == "qLowTame")      { column = 1; row = 1; }
            else if (juce::String (id) == "qHighTame")     { column = 2; row = 1; }
            break;

        case Group::master:
            column = 0;
            row = 0;
            break;
    }

    controls.push_back (Control { std::move (knob), group, column, row });
}

void KChannelAudioProcessorEditor::timerCallback()
{
    display.repaint();
}

//==============================================================================
void KChannelAudioProcessorEditor::resized()
{
    const float scale = (float) getWidth() / (float) baseWidth;

    auto content = getLocalBounds().reduced (juce::roundToInt (20.0f * scale),
                                             juce::roundToInt (16.0f * scale));

    auto header = content.removeFromTop (juce::roundToInt (64.0f * scale));
    juce::ignoreUnused (header);

    content.removeFromTop (juce::roundToInt (8.0f * scale));
    display.setBounds (content.removeFromTop (juce::roundToInt (250.0f * scale)));
    content.removeFromTop (juce::roundToInt (8.0f * scale));

    content.removeFromBottom (juce::roundToInt (20.0f * scale));

    auto deck = content;

    inputPanel = deck.removeFromLeft (juce::roundToInt (180.0f * scale));
    deck.removeFromLeft (juce::roundToInt (8.0f * scale));
    eqPanel = deck.removeFromLeft (juce::roundToInt (520.0f * scale));
    deck.removeFromLeft (juce::roundToInt (8.0f * scale));
    dynPanel = deck.removeFromLeft (juce::roundToInt (280.0f * scale));
    deck.removeFromLeft (juce::roundToInt (8.0f * scale));
    quellPanel = deck.removeFromLeft (juce::roundToInt (220.0f * scale));
    deck.removeFromLeft (juce::roundToInt (8.0f * scale));
    masterPanel = deck;

    //================================================================ input
    {
        auto area = inputPanel.reduced (juce::roundToInt (8.0f * scale));
        area.removeFromTop (juce::roundToInt (28.0f * scale));
        area.removeFromBottom (juce::roundToInt (66.0f * scale));

        const int cellWidth = area.getWidth() / 3;
        const int knobHeight = juce::jmin (area.getHeight(), juce::roundToInt (112.0f * scale));

        for (auto& control : controls)
        {
            if (control.group != Group::input)
                continue;

            control.knob->setBounds (area.getX() + control.column * cellWidth, area.getY(),
                                     cellWidth, knobHeight);
        }

        auto buttons = inputPanel.reduced (juce::roundToInt (12.0f * scale))
                                  .removeFromBottom (juce::roundToInt (58.0f * scale));

        filtersButton.setBounds (buttons.removeFromTop (juce::roundToInt (24.0f * scale)));
        buttons.removeFromTop (juce::roundToInt (6.0f * scale));
        phaseButton.setBounds (buttons.removeFromTop (juce::roundToInt (24.0f * scale)));
    }

    //=================================================================== EQ
    {
        auto area = eqPanel.reduced (juce::roundToInt (8.0f * scale));
        area.removeFromTop (juce::roundToInt (28.0f * scale));

        auto buttonArea = area.removeFromBottom (juce::roundToInt (28.0f * scale));

        const int columnWidth = area.getWidth() / 4;
        const int rowHeight = area.getHeight() / 3;

        for (auto& control : controls)
        {
            if (control.group != Group::eq)
                continue;

            control.knob->setBounds (area.getX() + control.column * columnWidth,
                                     area.getY() + control.row * rowHeight,
                                     columnWidth, rowHeight);
        }

        eqButton.setBounds (buttonArea.getRight() - juce::roundToInt (82.0f * scale),
                            buttonArea.getY() + juce::roundToInt (2.0f * scale),
                            juce::roundToInt (82.0f * scale),
                            juce::roundToInt (24.0f * scale));
    }

    //============================================================= dynamics
    {
        auto area = dynPanel.reduced (juce::roundToInt (8.0f * scale));
        area.removeFromTop (juce::roundToInt (26.0f * scale));

        compAreaRect = area.removeFromTop (juce::roundToInt (205.0f * scale));
        area.removeFromTop (juce::roundToInt (4.0f * scale));
        gateAreaRect = area;

        auto compArea = compAreaRect;
        compArea.removeFromTop (juce::roundToInt (22.0f * scale));

        const int compWidth = compArea.getWidth() / 3;
        const int compHeight = compArea.getHeight() / 2;

        auto gateArea = gateAreaRect;
        gateArea.removeFromTop (juce::roundToInt (22.0f * scale));

        const int gateWidth = gateArea.getWidth() / 2;
        const int gateHeight = gateArea.getHeight() / 2;

        for (auto& control : controls)
        {
            if (control.group == Group::comp)
            {
                control.knob->setBounds (compArea.getX() + control.column * compWidth,
                                         compArea.getY() + control.row * compHeight,
                                         compWidth, compHeight);
            }
            else if (control.group == Group::gate)
            {
                control.knob->setBounds (gateArea.getX() + control.column * gateWidth,
                                         gateArea.getY() + control.row * gateHeight,
                                         gateWidth, gateHeight);
            }
        }

        compButton.setBounds (compAreaRect.getRight() - juce::roundToInt (58.0f * scale),
                              compAreaRect.getY() + juce::roundToInt (1.0f * scale),
                              juce::roundToInt (54.0f * scale),
                              juce::roundToInt (20.0f * scale));

        gateButton.setBounds (gateAreaRect.getRight() - juce::roundToInt (58.0f * scale),
                              gateAreaRect.getY() + juce::roundToInt (1.0f * scale),
                              juce::roundToInt (54.0f * scale),
                              juce::roundToInt (20.0f * scale));
    }

    //================================================================ Quell
    {
        auto area = quellPanel.reduced (juce::roundToInt (8.0f * scale));
        area.removeFromTop (juce::roundToInt (26.0f * scale));

        auto buttons = quellPanel.reduced (juce::roundToInt (10.0f * scale))
                                 .removeFromBottom (juce::roundToInt (28.0f * scale));

        const int buttonGap = juce::roundToInt (6.0f * scale);
        const int half = (buttons.getWidth() - buttonGap) / 2;

        quellButton.setBounds (buttons.getX(), buttons.getY(), half, buttons.getHeight());
        quellPostButton.setBounds (buttons.getX() + half + buttonGap, buttons.getY(),
                                   half, buttons.getHeight());

        area.removeFromBottom (juce::roundToInt (42.0f * scale));

        const int columnWidth = area.getWidth() / 3;
        const int rowHeight = area.getHeight() / 2;

        for (auto& control : controls)
        {
            if (control.group != Group::quell)
                continue;

            control.knob->setBounds (area.getX() + control.column * columnWidth,
                                     area.getY() + control.row * rowHeight,
                                     columnWidth, rowHeight);
        }
    }

    //=============================================================== master
    {
        auto area = masterPanel.reduced (juce::roundToInt (10.0f * scale));
        area.removeFromTop (juce::roundToInt (28.0f * scale));

        for (auto& control : controls)
        {
            if (control.group == Group::master)
                control.knob->setBounds (area.getX(), area.getY(), area.getWidth(),
                                         juce::jmin (area.getHeight() - juce::roundToInt (42.0f * scale),
                                                     juce::roundToInt (124.0f * scale)));
        }

        bypassButton.setBounds (masterPanel.reduced (juce::roundToInt (10.0f * scale))
                                          .removeFromBottom (juce::roundToInt (26.0f * scale)));
    }
}

//==============================================================================
void KChannelAudioProcessorEditor::paint (juce::Graphics& g)
{
    const float scale = (float) getWidth() / (float) baseWidth;
    auto bounds = getLocalBounds();
    auto content = bounds.reduced (juce::roundToInt (20.0f * scale),
                                   juce::roundToInt (16.0f * scale));

    g.setColour (kchannel_ui::colours::background);
    g.fillAll();

    //==================================================================== header
    auto header = content.removeFromTop (juce::roundToInt (64.0f * scale));

    g.setColour (kchannel_ui::colours::accent);
    g.fillRoundedRectangle ((float) content.getX(), (float) header.getY() + 12.0f * scale,
                            3.0f * scale, 34.0f * scale, 1.5f);

    g.setColour (kchannel_ui::colours::text);
    g.setFont (kchannel_ui::uiFont (25.0f * scale, true));
    g.drawText ("K CHANNEL",
                content.getX() + juce::roundToInt (13.0f * scale),
                header.getY() + juce::roundToInt (3.0f * scale),
                juce::roundToInt (220.0f * scale),
                juce::roundToInt (33.0f * scale),
                juce::Justification::centredLeft, false);

    g.setColour (kchannel_ui::colours::textDim);
    g.setFont (kchannel_ui::uiFont (8.5f * scale, true));
    g.drawText ("9000K-STYLE CONSOLE CHANNEL",
                content.getX() + juce::roundToInt (14.0f * scale),
                header.getY() + juce::roundToInt (35.0f * scale),
                juce::roundToInt (300.0f * scale),
                juce::roundToInt (13.0f * scale),
                juce::Justification::centredLeft, false);

    {
        const bool post = processor.isQuellPost();
        const auto text = post ? "QUELL POST COMP" : "QUELL PRE COMP";
        auto chip = juce::Rectangle<int> (content.getRight() - juce::roundToInt (126.0f * scale),
                                          header.getY() + juce::roundToInt (17.0f * scale),
                                          juce::roundToInt (126.0f * scale),
                                          juce::roundToInt (24.0f * scale));

        g.setColour (kchannel_ui::colours::accentCool.withAlpha (0.14f));
        g.fillRoundedRectangle (chip.toFloat(), 4.0f);
        g.setColour (kchannel_ui::colours::accentCool.withAlpha (0.8f));
        g.drawRoundedRectangle (chip.toFloat().reduced (0.5f), 4.0f, 1.0f);
        g.setFont (kchannel_ui::uiFont (8.5f * scale, true));
        g.drawText (text, chip, juce::Justification::centred, false);
    }

    g.setColour (kchannel_ui::colours::outline);
    g.drawHorizontalLine (header.getBottom(), (float) content.getX(), (float) content.getRight());

    //==================================================================== panels
    auto drawPanel = [&g, scale] (juce::Rectangle<int> r, const juce::String& title,
                                  juce::Colour accent)
    {
        g.setColour (kchannel_ui::colours::panel);
        g.fillRoundedRectangle (r.toFloat(), 6.0f);
        g.setColour (kchannel_ui::colours::outline);
        g.drawRoundedRectangle (r.toFloat().reduced (0.5f), 6.0f, 1.0f);

        g.setColour (accent.withAlpha (0.9f));
        g.fillRoundedRectangle ((float) r.getX() + 10.0f * scale,
                                (float) r.getY() + 10.0f * scale,
                                2.0f * scale, 12.0f * scale, 1.0f);

        g.setColour (kchannel_ui::colours::textDim);
        g.setFont (kchannel_ui::uiFont (9.0f * scale, true));
        g.drawText (title,
                    r.getX() + juce::roundToInt (17.0f * scale),
                    r.getY() + juce::roundToInt (7.0f * scale),
                    r.getWidth() - juce::roundToInt (24.0f * scale),
                    juce::roundToInt (18.0f * scale),
                    juce::Justification::centredLeft, false);
    };

    drawPanel (inputPanel, "INPUT / FILTERS", kchannel_ui::colours::accent);
    drawPanel (eqPanel, "EQUALIZER", kchannel_ui::colours::accent);
    drawPanel (dynPanel, "DYNAMICS", kchannel_ui::colours::reduction);
    drawPanel (quellPanel, "QUELL", kchannel_ui::colours::accentCool);
    drawPanel (masterPanel, "MASTER", kchannel_ui::colours::accent);

    //=============================================================== EQ headers
    {
        auto area = eqPanel.reduced (juce::roundToInt (8.0f * scale));
        area.removeFromTop (juce::roundToInt (28.0f * scale));
        auto headerRow = area.removeFromTop (juce::roundToInt (16.0f * scale));

        const char* names[] = { "LF", "LM", "HM", "HF" };
        const int columnWidth = headerRow.getWidth() / 4;

        g.setFont (kchannel_ui::uiFont (8.5f * scale, true));

        for (int i = 0; i < 4; ++i)
        {
            g.setColour (i == 0 ? kchannel_ui::colours::accent
                                : (i == 1 ? kchannel_ui::colours::accentCool
                                          : (i == 2 ? kchannel_ui::colours::reduction
                                                    : kchannel_ui::colours::ok)));
            g.drawText (names[i],
                        headerRow.getX() + i * columnWidth,
                        headerRow.getY(),
                        columnWidth,
                        headerRow.getHeight(),
                        juce::Justification::centred, false);
        }
    }

    //=============================================================== dynamics
    {
        g.setFont (kchannel_ui::uiFont (8.0f * scale, true));
        g.setColour (kchannel_ui::colours::textDim);

        g.drawText ("COMPRESSOR", compAreaRect.getX(),
                    compAreaRect.getY(), compAreaRect.getWidth() - juce::roundToInt (64.0f * scale),
                    juce::roundToInt (20.0f * scale),
                    juce::Justification::centredLeft, false);

        g.drawText ("GATE", gateAreaRect.getX(),
                    gateAreaRect.getY(), gateAreaRect.getWidth() - juce::roundToInt (64.0f * scale),
                    juce::roundToInt (20.0f * scale),
                    juce::Justification::centredLeft, false);
    }

    //=============================================================== master
    {
        const double sr = processor.getSampleRate() > 0.0 ? processor.getSampleRate() : 48000.0;
        const double latencyMs = 1000.0 * (double) processor.getProcessingLatencySamples() / sr;

        g.setColour (kchannel_ui::colours::textDim.withAlpha (0.8f));
        g.setFont (kchannel_ui::monoFont (8.0f * scale));
        g.drawText ("LATENCY " + juce::String (latencyMs, 1) + " ms",
                    masterPanel.getX() + juce::roundToInt (10.0f * scale),
                    bypassButton.getY() - juce::roundToInt (16.0f * scale),
                    masterPanel.getWidth() - juce::roundToInt (20.0f * scale),
                    juce::roundToInt (14.0f * scale),
                    juce::Justification::centred, false);
    }

    //==================================================================== footer
    auto footer = bounds.reduced (juce::roundToInt (20.0f * scale),
                                  juce::roundToInt (16.0f * scale))
                        .removeFromBottom (juce::roundToInt (18.0f * scale));

    g.setColour (kchannel_ui::colours::textDim.withAlpha (0.8f));
    g.setFont (kchannel_ui::uiFont (8.5f * scale));
    g.drawText ("Quell is pre-compressor by default: narrow resonances are removed before they can drive gain reduction.",
                footer, juce::Justification::centredLeft, false);

    g.setFont (kchannel_ui::monoFont (8.5f * scale));
    g.setColour (kchannel_ui::colours::textDim.withAlpha (0.65f));
    g.drawText ("INPUT > FILTERS > EQ > QUELL > GATE > COMP > OUTPUT",
                footer, juce::Justification::centredRight, false);
}
