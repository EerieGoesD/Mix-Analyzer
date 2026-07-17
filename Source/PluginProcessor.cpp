#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
MixAnalyzerAudioProcessor::MixAnalyzerAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
}

MixAnalyzerAudioProcessor::~MixAnalyzerAudioProcessor()
{
}

//==============================================================================
void MixAnalyzerAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused (sampleRate, samplesPerBlock);
}

void MixAnalyzerAudioProcessor::releaseResources()
{
}

bool MixAnalyzerAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    // Accept mono or stereo, as long as input and output match.
    const auto& mainOutput = layouts.getMainOutputChannelSet();
    const auto& mainInput  = layouts.getMainInputChannelSet();

    if (mainOutput != juce::AudioChannelSet::mono()
        && mainOutput != juce::AudioChannelSet::stereo())
        return false;

    return mainInput == mainOutput;
}

void MixAnalyzerAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                              juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused (midiMessages);
    juce::ScopedNoDenormals noDenormals;

    // Analysis-only: we leave the audio untouched and pass it straight through.
    // The meters will tap this buffer in a later step. Nothing to do yet.
    juce::ignoreUnused (buffer);
}

//==============================================================================
bool MixAnalyzerAudioProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor* MixAnalyzerAudioProcessor::createEditor()
{
    return new MixAnalyzerAudioProcessorEditor (*this);
}

//==============================================================================
const juce::String MixAnalyzerAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool MixAnalyzerAudioProcessor::acceptsMidi() const   { return false; }
bool MixAnalyzerAudioProcessor::producesMidi() const  { return false; }
bool MixAnalyzerAudioProcessor::isMidiEffect() const  { return false; }
double MixAnalyzerAudioProcessor::getTailLengthSeconds() const { return 0.0; }

//==============================================================================
int MixAnalyzerAudioProcessor::getNumPrograms()               { return 1; }
int MixAnalyzerAudioProcessor::getCurrentProgram()            { return 0; }
void MixAnalyzerAudioProcessor::setCurrentProgram (int index) { juce::ignoreUnused (index); }
const juce::String MixAnalyzerAudioProcessor::getProgramName (int index)
{
    juce::ignoreUnused (index);
    return {};
}
void MixAnalyzerAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
    juce::ignoreUnused (index, newName);
}

//==============================================================================
void MixAnalyzerAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    juce::ignoreUnused (destData);
}

void MixAnalyzerAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    juce::ignoreUnused (data, sizeInBytes);
}

//==============================================================================
// This creates new instances of the plugin.
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MixAnalyzerAudioProcessor();
}
