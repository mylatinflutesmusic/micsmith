#include "PluginEditor.h"

#include "PluginProcessor.h"
#include "ui/PresetBrowser.h"

namespace
{
    constexpr int baseWidth = 920;
    constexpr int baseHeight = 600;

    const char* const soundParameterIds[] = {
        "depth", "selectivity", "sensitivity", "speed", "lowtame", "hightame",
        "mix", "output", "link", "msmode", "sidechain"
    };
}

//==============================================================================
QuellAudioProcessorEditor::QuellAudioProcessorEditor (QuellAudioProcessor& p)
    : juce::AudioProcessorEditor (&p), processor (p), display (p)
{
    setLookAndFeel (&lnf);
    setName ("Quell");

    //==========================================================================
    presetButton.setTooltip ("Browse factory and user presets, and save your own");
    presetButton.setColour (juce::TextButton::textColourOffId, quell_ui::colours::text);
    presetButton.setComponentID ("presetButton");
    presetButton.onClick = [this] { openPresetBrowser(); };
    addAndMakeVisible (presetButton);

    const auto storedPresetName = processor.apvts.state.getProperty ("presetName").toString();
    setCurrentPresetName (storedPresetName.isNotEmpty() ? storedPresetName : "Init");

    for (auto* b : { &abA, &abB, &abCopy })
    {
        b->setColour (juce::TextButton::textColourOffId, quell_ui::colours::textDim);
        addAndMakeVisible (*b);
    }

    abA.onClick  = [this] { switchToSlot (0); };
    abB.onClick  = [this] { switchToSlot (1); };
    abCopy.onClick = [this]
    {
        processor.copyCurrentToSlot (1 - processor.getActiveSlot());
        refreshABButtons();
    };

    abA.setTooltip ("Recall slot A");
    abB.setTooltip ("Recall slot B");
    abCopy.setTooltip ("Copy the current settings to the other slot");

    //==========================================================================
    for (auto* b : { &linkButton, &msButton, &scButton, &deltaButton, &bypassButton })
    {
        b->setClickingTogglesState (true);
        b->setColour (juce::TextButton::textColourOffId, quell_ui::colours::textDim);
        addAndMakeVisible (*b);
    }

    linkButton.setTooltip ("Detect and cut both channels together, preserving the stereo image");
    msButton.setTooltip ("Process mid and side instead of left and right");
    scButton.setTooltip ("Detect from the sidechain input bus; falls back to the main input if the host has not activated the bus");
    deltaButton.setTooltip ("Audition only the material being removed");
    bypassButton.setTooltip ("Bypass processing, keeping latency and levels aligned");

    linkAttachment   = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (processor.apvts, "link", linkButton);
    msAttachment     = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (processor.apvts, "msmode", msButton);
    scAttachment     = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (processor.apvts, "sidechain", scButton);
    deltaAttachment  = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (processor.apvts, "delta", deltaButton);
    bypassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (processor.apvts, "bypass", bypassButton);

    addAndMakeVisible (display);

    //==========================================================================
    struct KnobSpec
    {
        const char* id;
        const char* caption;
        const char* tooltip;
    };

    const KnobSpec specs[] = {
        { "depth",       "Depth",       "Amount of resonance removal. Higher values cut deeper." },
        { "selectivity", "Selectivity", "How tightly the detector focuses on narrow resonances." },
        { "sensitivity", "Sensitivity", "How far a peak must rise above the local spectrum before it is touched." },
        { "speed",       "Speed",       "How quickly the cuts follow the music. Slower is more transparent." },
        { "lowtame",     "Low Tame",    "Back off processing in the low end (below roughly 700 Hz)." },
        { "hightame",    "High Tame",   "Back off processing in the top end (above roughly 5 kHz)." },
        { "mix",         "Mix",         "Parallel blend with the unprocessed signal." },
        { "output",      "Output",      "Output level trim." }
    };

    for (const auto& spec : specs)
    {
        auto knob = std::make_unique<quell_ui::LabelledKnob> (processor.apvts, spec.id, spec.caption);
        knob->slider.setTooltip (spec.tooltip);

        const juce::String id (spec.id);

        if (id == "output")
        {
            knob->setFormatter ([] (double v) { return juce::String (v, 1) + " dB"; },
                                [] (const juce::String& t) { return t.getDoubleValue(); });
        }
        else
        {
            knob->setFormatter ([] (double v) { return juce::String (juce::roundToInt (v)) + " %"; },
                                [] (const juce::String& t) { return t.getDoubleValue(); });
        }

        addAndMakeVisible (*knob);
        knobs.push_back (std::move (knob));
    }

    //==========================================================================
    presetBrowser = std::make_unique<quell_ui::PresetBrowser> (
        processor.apvts,
        [this] (const juce::String& name) { setCurrentPresetName (name); },
        [this] (bool applying) { ignoreParameterChanges.store (applying); });
    addChildComponent (*presetBrowser);

    for (auto* id : soundParameterIds)
        processor.apvts.addParameterListener (id, this);

    setResizable (true, true);
    setResizeLimits ((int) (baseWidth * 0.85), (int) (baseHeight * 0.85),
                     baseWidth * 2, baseHeight * 2);

    if (auto* boundsConstrainer = getConstrainer())
        boundsConstrainer->setFixedAspectRatio ((double) baseWidth / (double) baseHeight);

    setSize (baseWidth, baseHeight);

    refreshABButtons();
    startTimerHz (30);
}

QuellAudioProcessorEditor::~QuellAudioProcessorEditor()
{
    for (auto* id : soundParameterIds)
        processor.apvts.removeParameterListener (id, this);

    presetBrowser.reset();
    stopTimer();
    setLookAndFeel (nullptr);
}

//==============================================================================
void QuellAudioProcessorEditor::timerCallback()
{
    display.update();
    display.repaint();
}

//==============================================================================
void QuellAudioProcessorEditor::openPresetBrowser()
{
    if (presetBrowser == nullptr)
        return;

    presetBrowser->setBounds (getLocalBounds());
    presetBrowser->showBrowser (currentPresetName);
}

void QuellAudioProcessorEditor::parameterChanged (const juce::String& parameterID, float)
{
    if (ignoreParameterChanges.load() || parameterID == "bypass" || parameterID == "delta")
        return;

    if (juce::MessageManager::getInstance()->isThisTheMessageThread())
    {
        markPresetUnsaved();
    }
    else
    {
        juce::Component::SafePointer<QuellAudioProcessorEditor> safe (this);
        juce::MessageManager::callAsync ([safe]
        {
            if (safe != nullptr)
                safe->markPresetUnsaved();
        });
    }
}

void QuellAudioProcessorEditor::markPresetUnsaved()
{
    if (currentPresetName != "Unsaved")
        setCurrentPresetName ("Unsaved");
}

void QuellAudioProcessorEditor::setCurrentPresetName (const juce::String& name)
{
    currentPresetName = name.isNotEmpty() ? name : "Unsaved";

    processor.apvts.state.setProperty ("presetName", currentPresetName, nullptr);

    presetButton.setButtonText (currentPresetName
        + juce::String (juce::CharPointer_UTF8 ("  \xe2\x96\xbe")));
}

void QuellAudioProcessorEditor::refreshABButtons()
{
    const int slot = processor.getActiveSlot();

    abA.setToggleState (slot == 0, juce::dontSendNotification);
    abB.setToggleState (slot == 1, juce::dontSendNotification);
}

void QuellAudioProcessorEditor::switchToSlot (int slot)
{
    processor.recallSlot (slot);

    const auto storedName = processor.apvts.state.getProperty ("presetName").toString();
    setCurrentPresetName (storedName.isNotEmpty() ? storedName : "Unsaved");

    refreshABButtons();
}

//==============================================================================
void QuellAudioProcessorEditor::resized()
{
    const float scale = (float) getWidth() / (float) baseWidth;

    auto content = getLocalBounds().reduced (juce::roundToInt (18.0f * scale),
                                             juce::roundToInt (14.0f * scale));

    auto header = content.removeFromTop (juce::roundToInt (56.0f * scale));

    const int buttonHeight = juce::roundToInt (25.0f * scale);
    const int buttonY = header.getCentreY() - buttonHeight / 2;

    presetButton.setBounds (content.getX() + juce::roundToInt (250.0f * scale), buttonY,
                            juce::roundToInt (190.0f * scale), buttonHeight);

    int x = presetButton.getRight() + juce::roundToInt (8.0f * scale);
    const int gap = juce::roundToInt (4.0f * scale);

    abA.setBounds (x, buttonY, juce::roundToInt (28.0f * scale), buttonHeight);
    x += abA.getWidth() + gap;
    abB.setBounds (x, buttonY, juce::roundToInt (28.0f * scale), buttonHeight);
    x += abB.getWidth() + gap;
    abCopy.setBounds (x, buttonY, juce::roundToInt (47.0f * scale), buttonHeight);

    const int rightGap = juce::roundToInt (6.0f * scale);
    int right = content.getRight();

    auto placeRight = [&] (juce::Component& component, float width)
    {
        const int w = juce::roundToInt (width * scale);
        component.setBounds (right - w, buttonY, w, buttonHeight);
        right -= w + rightGap;
    };

    placeRight (bypassButton, 74.0f);
    placeRight (deltaButton, 60.0f);
    placeRight (linkButton, 52.0f);
    placeRight (scButton, 42.0f);
    placeRight (msButton, 46.0f);

    content.removeFromTop (juce::roundToInt (2.0f * scale));

    display.setBounds (content.removeFromTop (juce::roundToInt (298.0f * scale)));
    content.removeFromTop (juce::roundToInt (8.0f * scale));

    auto knobRow = content.removeFromTop (juce::roundToInt (190.0f * scale));
    const int knobWidth = knobRow.getWidth() / (int) knobs.size();

    for (auto& knob : knobs)
        knob->setBounds (knobRow.removeFromLeft (knobWidth));

    if (presetBrowser != nullptr)
        presetBrowser->setBounds (getLocalBounds());
}

//==============================================================================
void QuellAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (quell_ui::colours::background);

    const float scale = (float) getWidth() / (float) baseWidth;

    auto content = getLocalBounds().reduced (juce::roundToInt (18.0f * scale),
                                             juce::roundToInt (14.0f * scale));

    auto header = content.removeFromTop (juce::roundToInt (56.0f * scale));

    //================================================================== masthead
    g.setColour (quell_ui::colours::accent);
    g.fillRoundedRectangle ((float) content.getX(),
                            (float) (header.getY() + juce::roundToInt (14.0f * scale)),
                            (float) juce::roundToInt (3.0f * scale),
                            (float) juce::roundToInt (28.0f * scale), 1.5f);

    g.setColour (quell_ui::colours::text);
    g.setFont (quell_ui::uiFont (23.0f * scale, true));
    g.drawText ("QUELL",
                content.getX() + juce::roundToInt (12.0f * scale), header.getY() + juce::roundToInt (5.0f * scale),
                juce::roundToInt (150.0f * scale), juce::roundToInt (30.0f * scale),
                juce::Justification::centredLeft, false);

    g.setColour (quell_ui::colours::textDim);
    g.setFont (quell_ui::uiFont (8.5f * scale, true));
    g.drawText ("DYNAMIC RESONANCE SUPPRESSOR",
                content.getX() + juce::roundToInt (13.0f * scale), header.getY() + juce::roundToInt (34.0f * scale),
                juce::roundToInt (230.0f * scale), juce::roundToInt (14.0f * scale),
                juce::Justification::centredLeft, false);

    g.setColour (quell_ui::colours::outline);
    g.drawHorizontalLine (header.getBottom(), (float) content.getX(), (float) content.getRight());

    //==================================================================== footer
    auto footer = getLocalBounds().reduced (juce::roundToInt (18.0f * scale),
                                            juce::roundToInt (14.0f * scale)).removeFromBottom (juce::roundToInt (16.0f * scale));

    g.setColour (quell_ui::colours::textDim.withAlpha (0.85f));
    g.setFont (quell_ui::uiFont (9.5f * scale));
    g.drawText ("Delta auditions only what is removed.  M/S processes mid/side.  SC detects from the sidechain bus.",
                footer, juce::Justification::centredLeft, false);

    const double sr = processor.getSampleRate() > 0.0 ? processor.getSampleRate() : 48000.0;
    const double latencyMs = 1000.0 * (double) processor.getProcessingLatencySamples() / sr;

    g.setFont (quell_ui::monoFont (9.5f * scale));
    g.setColour (quell_ui::colours::textDim.withAlpha (0.7f));
    g.drawText ("LATENCY " + juce::String (latencyMs, 1) + " ms",
                footer, juce::Justification::centredRight, false);
}
