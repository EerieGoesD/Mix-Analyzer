#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
MixAnalyzerAudioProcessorEditor::MixAnalyzerAudioProcessorEditor (MixAnalyzerAudioProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p)
{
    setSize (720, 480);
}

MixAnalyzerAudioProcessorEditor::~MixAnalyzerAudioProcessorEditor()
{
}

//==============================================================================
void MixAnalyzerAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff121212));

    g.setColour (juce::Colours::white);
    g.setFont (juce::FontOptions (28.0f, juce::Font::bold));
    g.drawFittedText ("EERIE - Mix Analyzer",
                      getLocalBounds().removeFromTop (getHeight() / 2),
                      juce::Justification::centred, 1);

    g.setColour (juce::Colours::grey);
    g.setFont (juce::FontOptions (15.0f));
    g.drawFittedText ("meters coming next: loudness | spectrum | sound field | plot spectrum",
                      getLocalBounds().removeFromBottom (getHeight() / 2),
                      juce::Justification::centred, 1);
}

void MixAnalyzerAudioProcessorEditor::resized()
{
}
