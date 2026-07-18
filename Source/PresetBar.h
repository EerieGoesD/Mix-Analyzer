#pragma once

#include <JuceHeader.h>
#include <functional>

//==============================================================================
// A small reusable strip for managing user presets in a settings popover:
// a dropdown of saved presets, a name box, a Save button and a Delete button.
// It is category-agnostic - the owner passes callbacks to read and apply the
// current settings as an opaque string, so the same bar drives Loudness and
// Spectrum. Presets are persisted through PresetStore.
//==============================================================================
class PresetBar : public juce::Component
{
public:
    PresetBar (juce::String category,
               std::function<juce::String()> getSettings,
               std::function<void (const juce::String&)> applySettings);

    void resized() override;

    int getPreferredHeight() const noexcept { return 2 * 22 + 8; }

    // Called after a preset is applied, so the owner can refresh its own controls.
    std::function<void()> onPresetApplied;

private:
    void refreshList();

    juce::String category;
    std::function<juce::String()> getSettings;
    std::function<void (const juce::String&)> applySettings;

    juce::String activePreset;   // last loaded/saved name (target for Delete)

    juce::Label      caption;
    juce::ComboBox   presetBox;
    juce::TextEditor nameBox;
    juce::TextButton saveButton   { "Save" };
    juce::TextButton deleteButton { "Delete" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PresetBar)
};
