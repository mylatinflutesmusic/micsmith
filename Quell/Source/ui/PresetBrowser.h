#pragma once

#include <functional>
#include <vector>

#include <juce_gui_basics/juce_gui_basics.h>

#include "../PresetLibrary.h"
#include "UiTheme.h"

namespace quell_ui
{
/** Full-editor overlay for browsing factory presets and saving user presets. */
class PresetBrowser : public juce::Component,
                      private juce::ListBoxModel
{
public:
    PresetBrowser (juce::AudioProcessorValueTreeState&,
                   std::function<void (const juce::String&)> onPresetApplied,
                   std::function<void (bool)> onPresetApplying = {});

    ~PresetBrowser() override = default;

    void showBrowser (const juce::String& currentPreset);
    void hideBrowser();

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    bool keyPressed (const juce::KeyPress&) override;

private:
    struct Row
    {
        enum class Kind { Header, Factory, User };

        Kind kind = Kind::Header;
        juce::String name;
        int index = -1;
    };

    int getNumRows() override;
    void paintListBoxItem (int rowNumber, juce::Graphics&, int width, int height,
                           bool rowIsSelected) override;
    void listBoxItemClicked (int row, const juce::MouseEvent&) override;
    void listBoxItemDoubleClicked (int row, const juce::MouseEvent&) override;
    void selectedRowsChanged (int lastRowSelected) override;

    void refreshRows();
    void selectCurrentPreset();
    void updateButtons();
    void loadSelected();
    void saveCurrent();
    void deleteSelected();
    void confirmDelete (const juce::String& name);
    void deleteUserPreset (const juce::String& name);
    void setStatus (const juce::String&, bool isError);
    int selectedPresetRow() const;

    juce::AudioProcessorValueTreeState& apvts;
    quell::PresetLibrary library;
    std::vector<Row> rows;

    juce::String currentName;
    juce::String pendingOverwrite;

    juce::ListBox list { "Presets", this };
    juce::Label nameLabel;
    juce::TextEditor nameEditor;
    juce::TextButton loadButton { "LOAD" }, saveButton { "SAVE" },
                     deleteButton { "DELETE" }, closeButton { "CLOSE" };

    juce::Rectangle<int> panelBounds;
    juce::String status;
    bool statusIsError = false;

    std::function<void (const juce::String&)> onApplied;
    std::function<void (bool)> onApplying;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PresetBrowser)
};
} // namespace quell_ui
