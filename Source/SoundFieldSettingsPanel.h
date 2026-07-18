#pragma once

#include <JuceHeader.h>
#include <memory>
#include "SoundFieldDisplay.h"
#include "PresetBar.h"

//==============================================================================
// Popover of SOUND FIELD settings: vectorscope mode + detection method, plus the
// shared user-preset bar.
//==============================================================================
class SoundFieldSettingsPanel : public juce::Component
{
public:
    explicit SoundFieldSettingsPanel (SoundFieldDisplay&);

    void resized() override;
    void paint (juce::Graphics&) override;

private:
    void refreshFromDisplay();

    SoundFieldDisplay& field;

    std::unique_ptr<PresetBar> presetBar;

    juce::Label    vectorCaption, detectCaption;
    juce::ComboBox vectorBox, detectBox;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SoundFieldSettingsPanel)
};
