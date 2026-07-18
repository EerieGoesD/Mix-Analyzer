#include "PresetBar.h"
#include "PresetStore.h"
#include "MixLookAndFeel.h"

//==============================================================================
PresetBar::PresetBar (juce::String cat,
                      std::function<juce::String()> getS,
                      std::function<void (const juce::String&)> applyS)
    : category (std::move (cat)),
      getSettings (std::move (getS)),
      applySettings (std::move (applyS))
{
    caption.setText ("PRESET", juce::dontSendNotification);
    caption.setFont (MixLookAndFeel::uiFont (10.5f, false));
    caption.setColour (juce::Label::textColourId, MixColours::textDim);
    addAndMakeVisible (caption);

    presetBox.setTextWhenNothingSelected ("Saved presets...");
    presetBox.onChange = [this]
    {
        const auto name = presetBox.getText();
        if (name.isNotEmpty())
        {
            const auto data = PresetStore::getInstance().get (category, name);
            if (data.isNotEmpty())
            {
                applySettings (data);
                activePreset = name;
                if (onPresetApplied != nullptr)
                    onPresetApplied();
            }
        }

        // Reset the selection so picking the SAME preset again re-applies it -
        // JUCE's ComboBox suppresses onChange when the selected id is unchanged.
        presetBox.setSelectedId (0, juce::dontSendNotification);
    };
    addAndMakeVisible (presetBox);

    nameBox.setTextToShowWhenEmpty ("New preset name", MixColours::textDim);
    nameBox.setColour (juce::TextEditor::backgroundColourId, MixColours::surface2);
    nameBox.setColour (juce::TextEditor::textColourId,       MixColours::text);
    nameBox.setColour (juce::TextEditor::outlineColourId,    MixColours::border);
    nameBox.setColour (juce::TextEditor::focusedOutlineColourId, MixColours::accent);
    nameBox.setJustification (juce::Justification::centredLeft);
    nameBox.setFont (MixLookAndFeel::uiFont (13.0f));
    addAndMakeVisible (nameBox);

    saveButton.setColour (juce::TextButton::buttonColourId, MixColours::accent);
    saveButton.setTooltip ("Save the current settings as a preset with the name typed on the left.");
    saveButton.onClick = [this]
    {
        const auto name = nameBox.getText().trim();
        if (name.isEmpty())
            return;

        PresetStore::getInstance().save (category, name, getSettings());
        activePreset = name;
        nameBox.clear();
        refreshList();
    };
    addAndMakeVisible (saveButton);

    deleteButton.setColour (juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    deleteButton.setTooltip ("Delete the preset you last loaded or saved.");
    deleteButton.onClick = [this]
    {
        if (activePreset.isEmpty())
            return;

        PresetStore::getInstance().remove (category, activePreset);
        activePreset.clear();
        presetBox.setSelectedId (0, juce::dontSendNotification);
        refreshList();
    };
    addAndMakeVisible (deleteButton);

    refreshList();
}

void PresetBar::refreshList()
{
    presetBox.clear (juce::dontSendNotification);
    int id = 1;
    for (const auto& n : PresetStore::getInstance().getNames (category))
        presetBox.addItem (n, id++);
}

void PresetBar::resized()
{
    auto b = getLocalBounds();
    const int rowH = 22;
    const int gap  = 8;

    auto r1 = b.removeFromTop (rowH);
    caption.setBounds (r1.removeFromLeft (52));
    presetBox.setBounds (r1);

    b.removeFromTop (gap);

    auto r2 = b.removeFromTop (rowH);
    deleteButton.setBounds (r2.removeFromRight (58));
    r2.removeFromRight (5);
    saveButton.setBounds (r2.removeFromRight (52));
    r2.removeFromRight (6);
    nameBox.setBounds (r2);
}
