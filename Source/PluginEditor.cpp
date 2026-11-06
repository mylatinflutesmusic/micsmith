#include "PluginEditor.h"

#include "PluginProcessor.h"
#include "Presets.h"

namespace
{
    constexpr int baseWidth = 760;
    constexpr int baseHeight = 560;

    int indexForMicName (const juce::String& name)
    {
        const auto& mics = micsmith::micModels();

        for (size_t i = 0; i < mics.size(); ++i)
            if (name == mics[i].name)
                return (int) i;

        return 0;
    }
}

//==============================================================================
MicSmithAudioProcessorEditor::MicSmithAudioProcessorEditor (MicSmithAudioProcessor& p)
    : juce::AudioProcessorEditor (&p), processor (p), display (p)
{
    setLookAndFeel (&lnf);

    const auto& mics = micsmith::micModels();

    for (size_t i = 0; i < mics.size(); ++i)
    {
        sourceBox.addItem (mics[i].name, (int) i + 1);
        targetBox.addItem (mics[i].name, (int) i + 1);
    }

    sourceBox.setTooltip ("The microphone the take was actually recorded with.");
    targetBox.setTooltip ("The microphone you want it to sound like.");

    addAndMakeVisible (sourceBox);
    addAndMakeVisible (targetBox);

    sourceAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (processor.apvts, "source", sourceBox);
    targetAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (processor.apvts, "target", targetBox);

    sourceBox.onChange = [this] { updateModelText(); };
    targetBox.onChange = [this] { updateModelText(); };

    sourceCaption.setText ("RECORDED WITH", juce::dontSendNotification);
    targetCaption.setText ("EMULATE", juce::dontSendNotification);

    for (auto* l : { &sourceCaption, &targetCaption })
    {
        l->setFont (micsmith_ui::uiFont (10.0f, true));
        l->setColour (juce::Label::textColourId, micsmith_ui::colours::textDim);
        l->setJustificationType (juce::Justification::centredLeft);
        addAndMakeVisible (*l);
    }

    arrowLabel.setText (juce::String::fromUTF8 ("\xe2\x86\x92"), juce::dontSendNotification);
    arrowLabel.setFont (micsmith_ui::uiFont (20.0f));
    arrowLabel.setColour (juce::Label::textColourId, micsmith_ui::colours::accent.withAlpha (0.8f));
    arrowLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (arrowLabel);

    modelText.setFont (micsmith_ui::uiFont (11.0f));
    modelText.setColour (juce::Label::textColourId, micsmith_ui::colours::textDim);
    modelText.setJustificationType (juce::Justification::topLeft);
    addAndMakeVisible (modelText);

    addAndMakeVisible (display);

    //==========================================================================
    presetBox.setTextWhenNothingSelected ("Factory Presets");
    presetBox.setTooltip ("Load a starting point");

    {
        int id = 1;
        for (const auto& preset : micsmith::factoryPresets())
            presetBox.addItem (preset.name, id++);
    }

    presetBox.onChange = [this] { applyPreset (presetBox.getSelectedItemIndex()); };
    addAndMakeVisible (presetBox);

    for (auto* b : { &abA, &abB, &abCopy })
    {
        b->setColour (juce::TextButton::textColourOffId, micsmith_ui::colours::textDim);
        addAndMakeVisible (*b);
    }

    abA.onClick  = [this] { switchToSlot (0); };
    abB.onClick  = [this] { switchToSlot (1); };
    abCopy.onClick = [this]
    {
        processor.copyCurrentToSlot (1 - processor.getActiveSlot());
        refreshABButtons();
    };

    phaseButton.setClickingTogglesState (true);
    bypassButton.setClickingTogglesState (true);

    addAndMakeVisible (phaseButton);
    addAndMakeVisible (bypassButton);

    phaseAttachment  = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (processor.apvts, "phase", phaseButton);
    bypassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (processor.apvts, "bypass", bypassButton);

    hintText.setText ("Record with the flattest mic you have for the most accurate result.", juce::dontSendNotification);
    hintText.setFont (micsmith_ui::uiFont (10.0f));
    hintText.setColour (juce::Label::textColourId, micsmith_ui::colours::textDim.withAlpha (0.8f));
    hintText.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (hintText);

    //==========================================================================
    struct KnobSpec { const char* id; const char* caption; };

    const KnobSpec specs[] = {
        { "input",     "Input" },
        { "proximity", "Proximity" },
        { "axis",      "Off-Axis" },
        { "lowcut",    "Low Cut" },
        { "highcut",   "High Cut" },
        { "mix",       "Mix" },
        { "output",    "Output" }
    };

    for (const auto& spec : specs)
    {
        auto knob = std::make_unique<micsmith_ui::LabelledKnob> (processor.apvts, spec.id, spec.caption);

        const juce::String id (spec.id);

        if (id == "lowcut")
        {
            knob->setFormatter ([] (double v) { return juce::String (juce::roundToInt (v)) + " Hz"; },
                                [] (const juce::String& t) { return t.getDoubleValue(); });
        }
        else if (id == "highcut")
        {
            knob->setFormatter ([] (double v)
                                {
                                    if (v >= 19500.0) return juce::String ("Off");
                                    return juce::String (v / 1000.0, 1) + " kHz";
                                },
                                [] (const juce::String& t)
                                {
                                    if (t.containsIgnoreCase ("off")) return 20000.0;
                                    return t.getDoubleValue() * (t.containsIgnoreCase ("k") ? 1000.0 : 1.0);
                                });
        }
        else if (id == "input" || id == "output")
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
    setResizable (true, true);
    setResizeLimits ((int) (baseWidth * 0.9), (int) (baseHeight * 0.9),
                     baseWidth * 2, baseHeight * 2);

    if (auto* aspectConstrainer = getConstrainer())
        aspectConstrainer->setFixedAspectRatio ((double) baseWidth / (double) baseHeight);

    setSize (baseWidth, baseHeight);

    updateModelText();
    refreshABButtons();

    startTimerHz (30);
}

MicSmithAudioProcessorEditor::~MicSmithAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

//==============================================================================
void MicSmithAudioProcessorEditor::timerCallback()
{
    const float in  = processor.inputPeak.load();
    const float out = processor.outputPeak.load();

    meterIn  = juce::jmax (in,  meterIn  * 0.87f);
    meterOut = juce::jmax (out, meterOut * 0.87f);

    auto read = [this] (const char* id)
    {
        if (auto* v = processor.apvts.getRawParameterValue (id))
            return v->load();
        return 0.0f;
    };

    const juce::String signature = juce::String (read ("source")) + "|" + juce::String (read ("target")) + "|"
                                 + juce::String (read ("proximity")) + "|" + juce::String (read ("axis")) + "|"
                                 + juce::String (read ("lowcut")) + "|" + juce::String (read ("highcut"));

    if (signature != lastSignature)
    {
        lastSignature = signature;
        display.repaint();
    }

    repaint();
}

//==============================================================================
void MicSmithAudioProcessorEditor::updateModelText()
{
    const auto& mics = micsmith::micModels();

    const int src = indexForMicName (sourceBox.getText());
    const int tgt = indexForMicName (targetBox.getText());

    const juce::String line1 = juce::String ("SOURCE: ") + mics[(size_t) src].family
                             + "    TARGET: " + mics[(size_t) tgt].family;

    modelText.setText (line1 + "\n" + mics[(size_t) tgt].blurb, juce::dontSendNotification);
}

void MicSmithAudioProcessorEditor::refreshABButtons()
{
    const bool a = processor.getActiveSlot() == 0;

    abA.setColour (juce::TextButton::buttonColourId, a ? micsmith_ui::colours::accent : micsmith_ui::colours::panelLight);
    abA.setColour (juce::TextButton::textColourOffId, a ? micsmith_ui::colours::background : micsmith_ui::colours::textDim);
    abB.setColour (juce::TextButton::buttonColourId, a ? micsmith_ui::colours::panelLight : micsmith_ui::colours::accent);
    abB.setColour (juce::TextButton::textColourOffId, a ? micsmith_ui::colours::textDim : micsmith_ui::colours::background);

    abA.repaint();
    abB.repaint();
}

void MicSmithAudioProcessorEditor::switchToSlot (int slot)
{
    if (processor.getActiveSlot() == slot)
        return;

    processor.storeSlot (processor.getActiveSlot());
    processor.recallSlot (slot);
    refreshABButtons();
}

void MicSmithAudioProcessorEditor::setParam (const juce::String& id, float value)
{
    if (auto* p = processor.apvts.getParameter (id))
        p->setValueNotifyingHost (p->convertTo0to1 (value));
}

void MicSmithAudioProcessorEditor::applyPreset (int presetIndex)
{
    const auto& presets = micsmith::factoryPresets();

    if (! juce::isPositiveAndBelow (presetIndex, (int) presets.size()))
        return;

    const auto& preset = presets[(size_t) presetIndex];

    setParam ("source",      (float) indexForMicName (preset.sourceName));
    setParam ("target",      (float) indexForMicName (preset.targetName));
    setParam ("proximity",   preset.proximity);
    setParam ("axis",        preset.axis);
    setParam ("lowcut",      preset.lowCutHz);
    setParam ("highcut",     preset.highCutHz);
    setParam ("mix",         preset.mix);
    setParam ("output",      preset.outputDb);

    updateModelText();
    display.repaint();
}

//==============================================================================
void MicSmithAudioProcessorEditor::resized()
{
    const float s = (float) getWidth() / (float) baseWidth;

    auto R = [s] (int x, int y, int w, int h)
    {
        return juce::Rectangle<int> (juce::roundToInt (x * s), juce::roundToInt (y * s),
                                     juce::roundToInt (w * s), juce::roundToInt (h * s));
    };

    presetBox.setBounds (R (400, 14, 210, 26));

    abA.setBounds    (R (616, 14, 32, 26));
    abB.setBounds    (R (652, 14, 32, 26));
    abCopy.setBounds (R (690, 14, 56, 26));

    sourceCaption.setBounds (R (14, 60, 200, 14));
    targetCaption.setBounds (R (396, 60, 200, 14));

    sourceBox.setBounds (R (14, 76, 350, 44));
    targetBox.setBounds (R (396, 76, 350, 44));
    arrowLabel.setBounds (R (362, 76, 34, 44));

    modelText.setBounds (R (14, 126, 732, 30));

    display.setBounds (R (14, 158, 732, 228));

    const int knobWidth = 100;
    const int knobHeight = 100;
    const int startX = 30;

    for (size_t i = 0; i < knobs.size(); ++i)
        knobs[i]->setBounds (R (startX + (int) i * knobWidth, 394, knobWidth, knobHeight));

    phaseButton.setBounds  (R (14, 504, 80, 30));
    bypassButton.setBounds (R (100, 504, 90, 30));
    hintText.setBounds     (R (206, 504, 300, 30));
}

//==============================================================================
void MicSmithAudioProcessorEditor::paint (juce::Graphics& g)
{
    const float s = (float) getWidth() / (float) baseWidth;

    juce::ColourGradient bg (micsmith_ui::colours::background.brighter (0.035f), 0.0f, 0.0f,
                             micsmith_ui::colours::background.darker (0.35f), 0.0f, (float) getHeight(), false);
    g.setGradientFill (bg);
    g.fillAll();

    // Title block
    {
        g.setColour (micsmith_ui::colours::text);
        g.setFont (micsmith_ui::uiFont (17.0f * s, true));
        g.drawText ("M I C S M I T H",
                    juce::Rectangle<int> (juce::roundToInt (14.0f * s), juce::roundToInt (9.0f * s),
                                          juce::roundToInt (300.0f * s), juce::roundToInt (22.0f * s)),
                    juce::Justification::centredLeft, false);

        g.setColour (micsmith_ui::colours::accent);
        g.fillRect (juce::Rectangle<float> (14.5f * s, 34.0f * s, 46.0f * s, 2.0f * s));

        g.setColour (micsmith_ui::colours::textDim);
        g.setFont (micsmith_ui::uiFont (9.5f * s, true));
        g.drawText ("MICROPHONE  MODELER", juce::Rectangle<int> (juce::roundToInt (66 * s), juce::roundToInt (22 * s),
                                                                juce::roundToInt (200 * s), juce::roundToInt (16 * s)),
                    juce::Justification::centredLeft, false);
    }

    // Control strip behind the knobs
    {
        const juce::Rectangle<float> strip (14.0f * s, 386.0f * s, 732.0f * s, 106.0f * s);

        juce::ColourGradient stripGrad (micsmith_ui::colours::panelLight.brighter (0.02f), strip.getCentreX(), strip.getY(),
                                        micsmith_ui::colours::panel.darker (0.2f), strip.getCentreX(), strip.getBottom(), false);
        g.setGradientFill (stripGrad);
        g.fillRoundedRectangle (strip, 6.0f);
        g.setColour (micsmith_ui::colours::outline);
        g.drawRoundedRectangle (strip.reduced (0.5f), 6.0f, 1.0f);
    }

    // Meters
    {
        auto drawMeter = [&] (juce::Rectangle<int> area, float peak, const juce::String& caption)
        {
            g.setColour (micsmith_ui::colours::textDim);
            g.setFont (micsmith_ui::uiFont (9.0f * s, true));
            g.drawText (caption, area.removeFromLeft (juce::roundToInt (24.0f * s)),
                        juce::Justification::centredLeft, false);

            g.setColour (micsmith_ui::colours::background.brighter (0.12f));
            g.fillRoundedRectangle (area.toFloat(), 2.0f);

            const float db = juce::Decibels::gainToDecibels (peak, -72.0f);
            const float norm = juce::jlimit (0.0f, 1.0f, (db + 60.0f) / 66.0f);

            if (norm > 0.0f)
            {
                auto bar = area.toFloat().withWidth (area.getWidth() * norm);
                g.setColour (db > -0.3f ? micsmith_ui::colours::accentWarm : micsmith_ui::colours::accent);
                g.fillRoundedRectangle (bar, 2.0f);
            }
        };

        auto meterArea = juce::Rectangle<int> (juce::roundToInt (516 * s), juce::roundToInt (506 * s),
                                               juce::roundToInt (228 * s), juce::roundToInt (10 * s));
        drawMeter (meterArea, meterIn, "IN");

        meterArea.translate (0, juce::roundToInt (16 * s));
        drawMeter (meterArea, meterOut, "OUT");
    }
}
