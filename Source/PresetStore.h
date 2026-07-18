#pragma once

#include <JuceHeader.h>

//==============================================================================
// A tiny on-disk store for user-named presets, shared by the meter settings
// panels. Presets are grouped by a category ("loudness", "spectrum"); each holds
// an opaque settings string that the panel itself serialises. Persisted to the
// user's app-data folder, so presets survive restarts (standalone and plugin).
//==============================================================================
class PresetStore
{
public:
    static PresetStore& getInstance();

    juce::StringArray getNames (const juce::String& category);
    juce::String      get (const juce::String& category, const juce::String& name) const;
    void              save (const juce::String& category, const juce::String& name, const juce::String& data);
    void              remove (const juce::String& category, const juce::String& name);

private:
    PresetStore();
    static juce::PropertiesFile::Options makeOptions();
    static juce::String makeKey (const juce::String& category, const juce::String& name);

    juce::PropertiesFile props;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PresetStore)
};
