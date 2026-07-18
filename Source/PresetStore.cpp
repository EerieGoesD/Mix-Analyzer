#include "PresetStore.h"

//==============================================================================
juce::PropertiesFile::Options PresetStore::makeOptions()
{
    juce::PropertiesFile::Options o;
    o.applicationName     = "MixAnalyzerPresets";
    o.filenameSuffix      = "settings";
    o.folderName          = "EERIE - Mix Analyzer";
    o.osxLibrarySubFolder = "Application Support";
    return o;
}

PresetStore::PresetStore()
    : props (makeOptions())
{
}

PresetStore& PresetStore::getInstance()
{
    static PresetStore instance;
    return instance;
}

juce::String PresetStore::makeKey (const juce::String& category, const juce::String& name)
{
    return category + "::" + name;
}

juce::StringArray PresetStore::getNames (const juce::String& category)
{
    juce::StringArray names;
    const juce::String prefix = category + "::";

    for (const auto& k : props.getAllProperties().getAllKeys())
        if (k.startsWith (prefix))
            names.add (k.substring (prefix.length()));

    names.sort (true);   // case-insensitive
    return names;
}

juce::String PresetStore::get (const juce::String& category, const juce::String& name) const
{
    return props.getValue (makeKey (category, name));
}

void PresetStore::save (const juce::String& category, const juce::String& name, const juce::String& data)
{
    props.setValue (makeKey (category, name), data);
    props.saveIfNeeded();
}

void PresetStore::remove (const juce::String& category, const juce::String& name)
{
    props.removeValue (makeKey (category, name));
    props.saveIfNeeded();
}
