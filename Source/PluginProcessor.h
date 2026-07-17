#pragma once

#include <JuceHeader.h>

//==============================================================================
// EERIE - Mix Analyzer
// Analysis-only, pass-through audio plugin. It taps the audio going through it
// (the master bus in a DAW, or the input device in standalone) and will drive
// the meters. It never changes the sound.
//==============================================================================
class MixAnalyzerAudioProcessor : public juce::AudioProcessor
{
public:
    MixAnalyzerAudioProcessor();
    ~MixAnalyzerAudioProcessor() override;

    //==========================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==========================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==========================================================================
    const juce::String getName() const override;
    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==========================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    //==========================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

private:
    //==========================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MixAnalyzerAudioProcessor)
};
