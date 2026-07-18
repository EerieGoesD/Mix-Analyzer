#pragma once

#include <JuceHeader.h>
#include <memory>
#include "SpectrumDisplay.h"
#include "PresetBar.h"

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
    void refreshFromSpectrum();   // pull every control back from the meter's state

    SpectrumDisplay& spectrum;

    std::unique_ptr<PresetBar> presetBar;

    juce::Label     channelCaption, specTypeCaption, windowSizeCaption, windowTypeCaption,
                    averageCaption, overlapCaption, peakHoldCaption;
    juce::ComboBox  channelBox, specTypeBox, windowSizeBox, windowTypeBox,
                    averageBox, overlapBox, peakHoldBox;
    juce::ToggleButton peakHoldToggle { "Show peak hold" };
    juce::TextButton   freezeButton   { "Freeze reference" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SpectrumSettingsPanel)
};
