#pragma once

#include <JuceHeader.h>
#include <memory>
#include "LoudnessDisplay.h"
#include "PresetBar.h"

//==============================================================================
// Popover of LOUDNESS settings (iZotope Insight set), shown from the gear:
// a standard preset, meter range + scale, loudness/peak/range targets and gate.
//==============================================================================
class LoudnessSettingsPanel : public juce::Component
{
public:
    explicit LoudnessSettingsPanel (LoudnessDisplay&);

    void resized() override;
    void paint (juce::Graphics&) override;

private:
    void refreshFromDisplay();   // pull every control back from the meter's state

    LoudnessDisplay& loud;

    std::unique_ptr<PresetBar> presetBar;

    juce::Label    viewCaption, rangeCaption, scaleCaption,
                   loudCaption, peakCaption, lraCaption, gateCaption;
    juce::ComboBox viewBox, rangeBox, scaleBox, gateBox;
    juce::Slider   loudSlider, peakSlider, lraSlider;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LoudnessSettingsPanel)
};
