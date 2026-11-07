#include "PresetBrowser.h"

namespace quell_ui
{
PresetBrowser::PresetBrowser (juce::AudioProcessorValueTreeState& state,
                              std::function<void (const juce::String&)> onPresetApplied,
                              std::function<void (bool)> onPresetApplying)
    : apvts (state), onApplied (std::move (onPresetApplied)),
      onApplying (std::move (onPresetApplying))
{
    setComponentID ("presetBrowser");
    setWantsKeyboardFocus (true);

    list.setComponentID ("presetList");
    list.setRowHeight (25);
    list.setOutlineThickness (1);
    list.setColour (juce::ListBox::backgroundColourId, colours::panel);
    list.setColour (juce::ListBox::outlineColourId, colours::outline);
    list.setColour (juce::ListBox::textColourId, colours::text);
    list.getVerticalScrollBar().setColour (juce::ScrollBar::thumbColourId,
                                           colours::outline.brighter (0.3f));
    list.getVerticalScrollBar().setColour (juce::ScrollBar::trackColourId, colours::panel);
    addAndMakeVisible (list);

    nameLabel.setText ("NAME", juce::dontSendNotification);
    nameLabel.setFont (uiFont (9.0f, true));
    nameLabel.setColour (juce::Label::textColourId, colours::textDim);
    nameLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (nameLabel);

    nameEditor.setFont (uiFont (12.0f));
    nameEditor.setJustification (juce::Justification::centredLeft);
    nameEditor.setIndents (8, 0);
    nameEditor.setSelectAllWhenFocused (true);
    nameEditor.setTextToShowWhenEmpty ("Preset name", colours::textDim);
    nameEditor.setColour (juce::TextEditor::backgroundColourId, colours::panelLight);
    nameEditor.setColour (juce::TextEditor::outlineColourId, colours::outline);
    nameEditor.setColour (juce::TextEditor::focusedOutlineColourId, colours::accent.withAlpha (0.7f));
    nameEditor.setColour (juce::TextEditor::textColourId, colours::text);
    nameEditor.setColour (juce::TextEditor::highlightColourId, colours::accent.withAlpha (0.3f));
    nameEditor.onReturnKey = [this] { saveCurrent(); };
    nameEditor.onTextChange = [this]
    {
        pendingOverwrite.clear();

        if (status.isNotEmpty())
            setStatus ({}, false);
    };
    addAndMakeVisible (nameEditor);

    for (auto* button : { &loadButton, &saveButton, &deleteButton, &closeButton })
    {
        button->setColour (juce::TextButton::textColourOffId, colours::textDim);
        addAndMakeVisible (*button);
    }

    saveButton.setColour (juce::TextButton::buttonColourId, colours::accent.withAlpha (0.16f));
    saveButton.setColour (juce::TextButton::textColourOffId, colours::accent);
    closeButton.setColour (juce::TextButton::textColourOffId, colours::text);
    loadButton.setColour (juce::TextButton::textColourOffId, colours::text);

    loadButton.setComponentID ("presetLoadButton");
    loadButton.onClick = [this] { loadSelected(); };
    saveButton.onClick = [this] { saveCurrent(); };
    deleteButton.onClick = [this] { deleteSelected(); };
    closeButton.onClick = [this] { hideBrowser(); };

    loadButton.setTooltip ("Load the selected preset; double-clicking a row also loads it");
    saveButton.setTooltip ("Save the current sound as a user preset");
    deleteButton.setTooltip ("Delete the selected user preset");
    closeButton.setTooltip ("Close the preset browser");

    deleteButton.setEnabled (false);
    loadButton.setEnabled (false);
    setVisible (false);
}

void PresetBrowser::showBrowser (const juce::String& currentPreset)
{
    currentName = currentPreset.isNotEmpty() ? currentPreset : "Init";
    pendingOverwrite.clear();
    setStatus ({}, false);
    refreshRows();

    nameEditor.setText (currentName, false);
    nameEditor.selectAll();

    setVisible (true);
    toFront (true);
    list.grabKeyboardFocus();
}

void PresetBrowser::hideBrowser()
{
    pendingOverwrite.clear();
    setVisible (false);
}

void PresetBrowser::refreshRows()
{
    library.refresh();
    rows.clear();

    rows.push_back ({ Row::Kind::Header, "FACTORY PRESETS", -1 });

    const auto& factory = quell::factoryPresets();

    for (int i = 0; i < (int) factory.size(); ++i)
        rows.push_back ({ Row::Kind::Factory, factory[(size_t) i].name, i });

    rows.push_back ({ Row::Kind::Header,
                      "USER PRESETS  (" + juce::String ((int) library.getUserPresets().size()) + ")",
                      -1 });

    for (int i = 0; i < (int) library.getUserPresets().size(); ++i)
        rows.push_back ({ Row::Kind::User, library.getUserPresets()[(size_t) i].name, i });

    list.updateContent();
    selectCurrentPreset();
    updateButtons();
    list.repaint();
}

void PresetBrowser::selectCurrentPreset()
{
    for (int i = 0; i < (int) rows.size(); ++i)
    {
        if (rows[(size_t) i].kind != Row::Kind::Header
            && rows[(size_t) i].name.equalsIgnoreCase (currentName))
        {
            list.selectRow (i);
            return;
        }
    }

    list.deselectAllRows();
}

int PresetBrowser::selectedPresetRow() const
{
    const int row = list.getSelectedRow();

    if (row >= 0 && row < (int) rows.size()
        && rows[(size_t) row].kind != Row::Kind::Header)
        return row;

    return -1;
}

void PresetBrowser::updateButtons()
{
    const int row = selectedPresetRow();

    loadButton.setEnabled (row >= 0);
    deleteButton.setEnabled (row >= 0 && rows[(size_t) row].kind == Row::Kind::User);
}

void PresetBrowser::loadSelected()
{
    const int row = selectedPresetRow();

    if (row < 0)
        return;

    const auto& selected = rows[(size_t) row];

    if (onApplying != nullptr)
        onApplying (true);

    if (selected.kind == Row::Kind::Factory)
    {
        quell::PresetLibrary::applyFactoryPreset (selected.index, apvts);
        currentName = selected.name;
        setStatus ("Loaded \"" + currentName + "\".", false);
    }
    else if (selected.kind == Row::Kind::User)
    {
        juce::String error;

        if (! library.loadUserPreset (selected.index, apvts, error))
        {
            if (onApplying != nullptr)
                onApplying (false);

            setStatus (error, true);
            return;
        }

        currentName = selected.name;
        setStatus ("Loaded \"" + currentName + "\".", false);
    }
    else
    {
        if (onApplying != nullptr)
            onApplying (false);

        return;
    }

    nameEditor.setText (currentName, false);

    if (onApplied != nullptr)
        onApplied (currentName);

    if (onApplying != nullptr)
        onApplying (false);

    list.repaint();
}

void PresetBrowser::saveCurrent()
{
    auto requested = nameEditor.getText().trim();

    if (requested.isEmpty())
    {
        setStatus ("Enter a preset name.", true);
        return;
    }

    const int existing = library.findUserPreset (requested);

    if (existing >= 0
        && ! pendingOverwrite.equalsIgnoreCase (requested))
    {
        pendingOverwrite = requested;
        setStatus ("Press SAVE again to replace \""
                       + library.getUserPresets()[(size_t) existing].name + "\".",
                   false);
        return;
    }

    juce::String error;

    if (! library.saveUserPreset (requested, apvts, error))
    {
        setStatus (error, true);
        return;
    }

    const int savedIndex = library.findUserPreset (requested);
    currentName = savedIndex >= 0 ? library.getUserPresets()[(size_t) savedIndex].name
                                  : requested;
    pendingOverwrite.clear();
    refreshRows();
    nameEditor.setText (currentName, false);

    if (onApplied != nullptr)
        onApplied (currentName);

    setStatus ("Saved \"" + currentName + "\".", false);
}

void PresetBrowser::deleteSelected()
{
    const int row = selectedPresetRow();

    if (row < 0 || rows[(size_t) row].kind != Row::Kind::User)
        return;

    confirmDelete (rows[(size_t) row].name);
}

void PresetBrowser::confirmDelete (const juce::String& name)
{
    juce::Component::SafePointer<PresetBrowser> safe (this);

    juce::AlertWindow::showOkCancelBox (
        juce::MessageBoxIconType::QuestionIcon,
        "Delete user preset?",
        "Delete \"" + name + "\"? This cannot be undone.",
        "Delete", "Cancel", this,
        juce::ModalCallbackFunction::create ([safe, name] (int result)
        {
            if (result == 1 && safe != nullptr)
                safe->deleteUserPreset (name);
        }));
}

void PresetBrowser::deleteUserPreset (const juce::String& name)
{
    const int index = library.findUserPreset (name);

    if (index < 0)
    {
        setStatus ("That preset no longer exists.", true);
        refreshRows();
        return;
    }

    juce::String error;

    if (! library.deleteUserPreset (index, error))
    {
        setStatus (error, true);
        return;
    }

    const bool wasCurrent = currentName.equalsIgnoreCase (name);
    refreshRows();

    if (wasCurrent)
    {
        currentName = "Unsaved";
        list.deselectAllRows();
        nameEditor.setText ({}, false);

        if (onApplied != nullptr)
            onApplied (currentName);
    }

    setStatus ("Deleted \"" + name + "\".", false);
}

void PresetBrowser::setStatus (const juce::String& message, bool isError)
{
    status = message;
    statusIsError = isError;
    repaint();
}

void PresetBrowser::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black.withAlpha (0.62f));

    auto panel = panelBounds.toFloat();

    g.setColour (colours::panel);
    g.fillRoundedRectangle (panel, 8.0f);
    g.setColour (colours::outline);
    g.drawRoundedRectangle (panel.reduced (0.5f), 8.0f, 1.0f);

    auto title = panelBounds.reduced (18).removeFromTop (46);

    g.setColour (colours::accent);
    g.fillRoundedRectangle ((float) title.getX(), (float) (title.getY() + 4),
                            3.0f, 24.0f, 1.5f);

    g.setColour (colours::text);
    g.setFont (uiFont (16.0f, true));
    g.drawText ("PRESET BROWSER",
                title.withTrimmedLeft (12).removeFromTop (28),
                juce::Justification::centredLeft, false);

    g.setColour (colours::textDim);
    g.setFont (uiFont (8.5f, true));
    g.drawText ("FACTORY AND USER PRESETS",
                title.withTrimmedLeft (13).removeFromBottom (14),
                juce::Justification::centredLeft, false);

    g.setColour (colours::outline);
    g.drawHorizontalLine (title.getBottom(), (float) title.getX(), (float) title.getRight());

    auto footer = panelBounds.reduced (18).removeFromBottom (16);

    g.setFont (monoFont (8.5f));
    g.setColour (statusIsError ? colours::reduction : colours::textDim.withAlpha (0.8f));
    g.drawText (status.isNotEmpty()
                    ? status
                    : "USER  " + library.getDirectory().getFullPathName(),
                footer, juce::Justification::centredLeft, true);
}

void PresetBrowser::resized()
{
    const int width  = juce::jmin (460, getWidth() - 32);
    const int height = juce::jmin (540, getHeight() - 32);

    panelBounds = juce::Rectangle<int> (width, height).withCentre (getLocalBounds().getCentre());

    auto area = panelBounds.reduced (18);
    area.removeFromTop (46);

    area.removeFromBottom (16);
    area.removeFromBottom (6);

    auto buttonRow = area.removeFromBottom (28);
    area.removeFromBottom (10);

    auto nameRow = area.removeFromBottom (28);
    area.removeFromBottom (10);

    list.setBounds (area);

    nameLabel.setBounds (nameRow.removeFromLeft (48));

    saveButton.setBounds (nameRow.removeFromRight (72));
    nameRow.removeFromRight (8);
    nameEditor.setBounds (nameRow);

    loadButton.setBounds (buttonRow.removeFromLeft (64));
    buttonRow.removeFromLeft (6);
    deleteButton.setBounds (buttonRow.removeFromLeft (74));
    closeButton.setBounds (buttonRow.removeFromRight (64));
}

void PresetBrowser::paintListBoxItem (int rowNumber, juce::Graphics& g,
                                      int width, int height, bool rowIsSelected)
{
    if (rowNumber < 0 || rowNumber >= (int) rows.size())
        return;

    const auto& row = rows[(size_t) rowNumber];
    auto bounds = juce::Rectangle<int> (0, 0, width, height);

    if (row.kind == Row::Kind::Header)
    {
        g.setColour (colours::panel);
        g.fillRect (bounds);
        g.setColour (colours::outline);
        g.fillRect (bounds.removeFromTop (1));

        g.setColour (colours::textDim);
        g.setFont (uiFont (8.5f, true));
        g.drawText (row.name,
                    bounds.reduced (12, 0),
                    juce::Justification::centredLeft, true);
        return;
    }

    g.setColour (rowIsSelected ? colours::accent.withAlpha (0.15f)
                               : (row.kind == Row::Kind::User
                                      ? colours::panelLight.withAlpha (0.28f)
                                      : colours::panel));
    g.fillRect (bounds);

    if (rowIsSelected)
    {
        g.setColour (colours::accent);
        g.fillRect (bounds.removeFromLeft (2));
    }

    g.setColour (rowIsSelected ? colours::accent : colours::text);
    g.setFont (uiFont (12.0f));
    g.drawText (row.name, bounds.reduced (14, 0),
                juce::Justification::centredLeft, true);

    if (row.kind == Row::Kind::User)
    {
        g.setColour (colours::textDim);
        g.setFont (monoFont (8.0f));
        g.drawText ("USER", bounds.reduced (12, 0),
                    juce::Justification::centredRight, false);
    }

    if (row.name.equalsIgnoreCase (currentName))
    {
        g.setColour (colours::accent);
        g.fillEllipse ((float) bounds.getRight()
                           - (row.kind == Row::Kind::User ? 52.0f : 31.0f),
                       (float) bounds.getCentreY() - 2.0f, 4.0f, 4.0f);
    }
}

void PresetBrowser::listBoxItemClicked (int row, const juce::MouseEvent&)
{
    if (row < 0 || row >= (int) rows.size())
        return;

    if (rows[(size_t) row].kind == Row::Kind::Header)
    {
        list.deselectAllRows();
        updateButtons();
        return;
    }

    pendingOverwrite.clear();
    setStatus ({}, false);
}

void PresetBrowser::listBoxItemDoubleClicked (int row, const juce::MouseEvent&)
{
    if (row >= 0 && row < (int) rows.size()
        && rows[(size_t) row].kind != Row::Kind::Header)
        loadSelected();
}

void PresetBrowser::selectedRowsChanged (int)
{
    const int row = selectedPresetRow();

    if (row >= 0)
        nameEditor.setText (rows[(size_t) row].name, false);

    updateButtons();
}

void PresetBrowser::mouseDown (const juce::MouseEvent& event)
{
    if (! panelBounds.contains (event.getPosition()))
        hideBrowser();
}

bool PresetBrowser::keyPressed (const juce::KeyPress& key)
{
    if (key == juce::KeyPress::escapeKey)
    {
        hideBrowser();
        return true;
    }

    if (key == juce::KeyPress::returnKey)
    {
        loadSelected();
        return true;
    }

    return false;
}

int PresetBrowser::getNumRows()
{
    return (int) rows.size();
}
} // namespace quell_ui
