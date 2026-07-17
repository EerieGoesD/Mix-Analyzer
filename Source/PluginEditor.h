#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

//==============================================================================
class MixAnalyzerAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit MixAnalyzerAudioProcessorEditor (MixAnalyzerAudioProcessor&);
    ~MixAnalyzerAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    MixAnalyzerAudioProcessor& processorRef;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MixAnalyzerAudioProcessorEditor)
};
