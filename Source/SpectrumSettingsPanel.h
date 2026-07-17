#pragma once

#include <JuceHeader.h>
#include "SpectrumDisplay.h"

//==============================================================================
// Popover of LIVE SPECTRUM settings (iZotope Insight set), shown from the gear.
//==============================================================================
class SpectrumSettingsPanel : public juce::Component
{
public:
    explicit SpectrumSettingsPanel (SpectrumDisplay&);

    void resized() override;
    void paint (juce::Graphics&) override;

private:
    SpectrumDisplay& spectrum;

    juce::Label     specTypeCaption, windowSizeCaption, windowTypeCaption,
                    averageCaption, overlapCaption, peakHoldCaption;
    juce::ComboBox  specTypeBox, windowSizeBox, windowTypeBox,
                    averageBox, overlapBox, peakHoldBox;
    juce::ToggleButton peakHoldToggle { "Show peak hold" };
    juce::TextButton   freezeButton   { "Freeze reference" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SpectrumSettingsPanel)
};
