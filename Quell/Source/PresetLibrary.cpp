#include "PresetLibrary.h"

#include <algorithm>

namespace quell
{
namespace
{
    constexpr const char* presetExtension = ".quellpreset";
    constexpr const char* presetTag = "QuellPreset";

    juce::String cleanDisplayName (const juce::String& raw)
    {
        auto name = raw.trim();
        name = name.removeCharacters ("\r\n\t");
        return name.substring (0, 64);
    }

    juce::String legalFileStem (const juce::String& name)
    {
        return juce::File::createLegalFileName (cleanDisplayName (name)).trim();
    }

    juce::String defaultPresetDirectory()
    {
        return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                   .getChildFile ("Quell")
                   .getChildFile ("Presets")
                   .getFullPathName();
    }

    float plainValue (const juce::AudioProcessorValueTreeState& state,
                      const char* id, float fallback)
    {
        if (auto* parameter = dynamic_cast<const juce::RangedAudioParameter*> (state.getParameter (id)))
            return parameter->convertFrom0to1 (parameter->getValue());

        return fallback;
    }

    void setRanged (juce::AudioProcessorValueTreeState& state, const char* id, float value)
    {
        if (auto* parameter = dynamic_cast<juce::RangedAudioParameter*> (state.getParameter (id)))
        {
            const auto& range = parameter->getNormalisableRange();
            parameter->setValueNotifyingHost (parameter->convertTo0to1 (
                juce::jlimit (range.start, range.end, value)));
        }
    }
}

PresetLibrary::PresetLibrary (juce::File presetDirectory)
    : directory (presetDirectory.getFullPathName().isNotEmpty()
                     ? std::move (presetDirectory)
                     : juce::File (defaultPresetDirectory()))
{
    directory.createDirectory();
    refresh();
}

void PresetLibrary::refresh()
{
    userPresets.clear();

    juce::Array<juce::File> files;
    directory.findChildFiles (files, juce::File::findFiles, false,
                              juce::String ("*") + presetExtension);

    for (const auto& file : files)
    {
        juce::String name, error;
        PresetValues values;

        if (! readPreset (file, name, values, error))
            continue;

        const bool duplicate = std::any_of (userPresets.begin(), userPresets.end(),
                                            [&name] (const UserPreset& existing)
                                            {
                                                return existing.name.equalsIgnoreCase (name);
                                            });

        if (! duplicate)
            userPresets.push_back ({ name, file });
    }

    std::sort (userPresets.begin(), userPresets.end(),
               [] (const UserPreset& a, const UserPreset& b)
               {
                   return a.name.compareIgnoreCase (b.name) < 0;
               });
}

int PresetLibrary::findUserPreset (const juce::String& name) const
{
    const auto wantedName = cleanDisplayName (name);
    const auto wantedStem = legalFileStem (wantedName);

    if (wantedName.isEmpty())
        return -1;

    for (int i = 0; i < (int) userPresets.size(); ++i)
    {
        const auto& preset = userPresets[(size_t) i];

        if (preset.name.equalsIgnoreCase (wantedName)
            || preset.file.getFileNameWithoutExtension().equalsIgnoreCase (wantedStem))
            return i;
    }

    return -1;
}

bool PresetLibrary::saveUserPreset (const juce::String& requestedName,
                                    const juce::AudioProcessorValueTreeState& state,
                                    juce::String& error)
{
    error.clear();

    const auto name = cleanDisplayName (requestedName);

    if (name.isEmpty())
    {
        error = "Enter a preset name.";
        return false;
    }

    if (legalFileStem (name).isEmpty())
    {
        error = "That name cannot be used for a file.";
        return false;
    }

    const auto createResult = directory.createDirectory();

    if (createResult.failed())
    {
        error = createResult.getErrorMessage();
        return false;
    }

    const int existingIndex = findUserPreset (name);
    const auto file = existingIndex >= 0
                          ? userPresets[(size_t) existingIndex].file
                          : directory.getChildFile (legalFileStem (name) + presetExtension);

    juce::XmlElement xml (presetTag);
    xml.setAttribute ("name", name);
    xml.setAttribute ("version", 1);
    xml.setAttribute ("depth",       (double) plainValue (state, "depth",       35.0f));
    xml.setAttribute ("sensitivity", (double) plainValue (state, "sensitivity", 40.0f));
    xml.setAttribute ("selectivity", (double) plainValue (state, "selectivity", 55.0f));
    xml.setAttribute ("speed",       (double) plainValue (state, "speed",       50.0f));
    xml.setAttribute ("lowtame",     (double) plainValue (state, "lowtame",      0.0f));
    xml.setAttribute ("hightame",    (double) plainValue (state, "hightame",     0.0f));
    xml.setAttribute ("mix",         (double) plainValue (state, "mix",        100.0f));
    xml.setAttribute ("output",      (double) plainValue (state, "output",       0.0f));
    xml.setAttribute ("link",        plainValue (state, "link", 1.0f) > 0.5f);
    xml.setAttribute ("msmode",      plainValue (state, "msmode", 0.0f) > 0.5f);
    xml.setAttribute ("sidechain",   plainValue (state, "sidechain", 0.0f) > 0.5f);

    if (! xml.writeTo (file))
    {
        error = "Could not write " + file.getFullPathName();
        return false;
    }

    refresh();
    return true;
}

bool PresetLibrary::loadUserPreset (int index,
                                    juce::AudioProcessorValueTreeState& state,
                                    juce::String& error) const
{
    error.clear();

    if (index < 0 || index >= (int) userPresets.size())
    {
        error = "Select a user preset first.";
        return false;
    }

    juce::String name;
    PresetValues values;

    if (! readPreset (userPresets[(size_t) index].file, name, values, error))
        return false;

    apply (values, state);
    state.state.setProperty ("presetName", name, nullptr);
    return true;
}

bool PresetLibrary::deleteUserPreset (int index, juce::String& error)
{
    error.clear();

    if (index < 0 || index >= (int) userPresets.size())
    {
        error = "Select a user preset first.";
        return false;
    }

    const auto file = userPresets[(size_t) index].file;

    if (file.existsAsFile() && ! file.deleteFile())
    {
        error = "Could not delete " + file.getFileName();
        return false;
    }

    refresh();
    return true;
}

void PresetLibrary::apply (const PresetValues& values,
                           juce::AudioProcessorValueTreeState& state)
{
    setRanged (state, "depth",       values.depth);
    setRanged (state, "sensitivity", values.sensitivity);
    setRanged (state, "selectivity", values.selectivity);
    setRanged (state, "speed",       values.speed);
    setRanged (state, "lowtame",     values.lowTame);
    setRanged (state, "hightame",    values.highTame);
    setRanged (state, "mix",         values.mix);
    setRanged (state, "output",      values.outputDb);
    setRanged (state, "link",        values.link ? 1.0f : 0.0f);
    setRanged (state, "msmode",      values.midSide ? 1.0f : 0.0f);
    setRanged (state, "sidechain",   values.sidechain ? 1.0f : 0.0f);
}

void PresetLibrary::applyFactoryPreset (int index,
                                        juce::AudioProcessorValueTreeState& state)
{
    const auto& presets = factoryPresets();

    if (index < 0 || index >= (int) presets.size())
        return;

    apply (valuesForFactoryPreset (index), state);
    state.state.setProperty ("presetName", presets[(size_t) index].name, nullptr);
}

bool PresetLibrary::readPreset (const juce::File& file,
                                juce::String& name,
                                PresetValues& values,
                                juce::String& error)
{
    error.clear();
    values = {};

    auto xml = juce::XmlDocument::parse (file);

    if (xml == nullptr || ! xml->hasTagName (presetTag))
    {
        error = "Not a Quell preset: " + file.getFileName();
        return false;
    }

    name = cleanDisplayName (xml->getStringAttribute ("name",
                                                      file.getFileNameWithoutExtension()));

    if (name.isEmpty())
        name = file.getFileNameWithoutExtension();

    values.depth       = (float) xml->getDoubleAttribute ("depth",       35.0);
    values.sensitivity = (float) xml->getDoubleAttribute ("sensitivity", 40.0);
    values.selectivity = (float) xml->getDoubleAttribute ("selectivity", 55.0);
    values.speed       = (float) xml->getDoubleAttribute ("speed",       50.0);
    values.lowTame     = (float) xml->getDoubleAttribute ("lowtame",      0.0);
    values.highTame    = (float) xml->getDoubleAttribute ("hightame",     0.0);
    values.mix         = (float) xml->getDoubleAttribute ("mix",        100.0);
    values.outputDb    = (float) xml->getDoubleAttribute ("output",       0.0);
    values.link        = xml->getBoolAttribute ("link", true);
    values.midSide     = xml->getBoolAttribute ("msmode", false);
    values.sidechain   = xml->getBoolAttribute ("sidechain", false);

    return true;
}
} // namespace quell
