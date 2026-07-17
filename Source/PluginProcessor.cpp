#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <utility>

//==============================================================================
MixAnalyzerAudioProcessor::MixAnalyzerAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
    formatManager.registerBasicFormats();
    readAheadThread.startThread();
}

MixAnalyzerAudioProcessor::~MixAnalyzerAudioProcessor()
{
    transportSource.setSource (nullptr);
    readAheadThread.stopThread (1000);
}

//==============================================================================
void MixAnalyzerAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    transportSource.prepareToPlay (samplesPerBlock, sampleRate);
}

void MixAnalyzerAudioProcessor::releaseResources()
{
    transportSource.releaseResources();
}

bool MixAnalyzerAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
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

    const int numChannels = buffer.getNumChannels();
    const int numSamples  = buffer.getNumSamples();

    // If a song file is loaded, play it into the buffer (so we both hear it and
    // analyze it). Otherwise leave the host audio untouched. No locks here: the
    // transport is thread-safe on its own, and whole-song looping is handled at
    // the reader level, so we never call start/stop from the audio thread.
    if (fileIsLoaded.load())
    {
        juce::AudioSourceChannelInfo info (&buffer, 0, numSamples);
        transportSource.getNextAudioBlock (info);

        // Loop (or end) the selected section.
        if (transportSource.isPlaying())
        {
            const double s = selectionStart.load();
            const double e = selectionEnd.load();

            if (e > s && transportSource.getCurrentPosition() >= e)
            {
                if (loopEnabled.load())
                    transportSource.setPosition (s);   // seek back: loop the section
                else
                    requestStop.store (true);          // ask the message thread to stop
            }
        }
    }

    // Tap a mono sum (L+R)/2 into the rolling buffer for the spectrum meter.
    if (numChannels > 0)
    {
        const float* left  = buffer.getReadPointer (0);
        const float* right = numChannels > 1 ? buffer.getReadPointer (1) : left;

        for (int i = 0; i < numSamples; ++i)
            pushSample (0.5f * (left[i] + right[i]));
    }
}

//==============================================================================
void MixAnalyzerAudioProcessor::pushSample (float sample) noexcept
{
    const int w = writePos.load (std::memory_order_relaxed);
    ringBuffer[w] = sample;
    writePos.store ((w + 1) & (fftSize - 1), std::memory_order_release);
}

void MixAnalyzerAudioProcessor::copyLatestSamples (float* dest) const noexcept
{
    // The buffer holds exactly fftSize samples, so the oldest of the most recent
    // fftSize samples is at the current write position; copy forward from there.
    const int w = writePos.load (std::memory_order_acquire);

    for (int i = 0; i < fftSize; ++i)
        dest[i] = ringBuffer[(w + i) & (fftSize - 1)];
}

//==============================================================================
void MixAnalyzerAudioProcessor::loadFile (const juce::File& file)
{
    auto* reader = formatManager.createReaderFor (file);

    if (reader == nullptr)
        return;   // unsupported or unreadable file

    auto newSource = std::make_unique<juce::AudioFormatReaderSource> (reader, true);
    newSource->setLooping (loopEnabled.load());   // whole-song looping at the reader
    const double fileSampleRate = reader->sampleRate;

    // setSource does its own internal locking, and disconnecting the old source
    // first makes it safe to then destroy the old reader.
    transportSource.stop();
    transportSource.setSource (nullptr);
    readerSource = std::move (newSource);
    transportSource.setSource (readerSource.get(), 32768, &readAheadThread,
                               fileSampleRate, 2);

    loadedFileName = file.getFileName();
    loadedFile     = file;
    selectionStart.store (0.0);
    selectionEnd.store (0.0);
    fileIsLoaded.store (true);
}

double MixAnalyzerAudioProcessor::sectionStartOrZero() const noexcept
{
    return (selectionEnd.load() > selectionStart.load()) ? selectionStart.load() : 0.0;
}

void MixAnalyzerAudioProcessor::togglePlayback()
{
    if (! fileIsLoaded.load())
        return;

    if (transportSource.isPlaying())
    {
        transportSource.stop();   // pause: keep the position
    }
    else
    {
        const double s = selectionStart.load();
        const double e = selectionEnd.load();
        const double pos = transportSource.getCurrentPosition();

        // Resume where we were, unless we're outside the section or at the end.
        if (e > s)
        {
            if (pos < s || pos >= e)
                transportSource.setPosition (s);
        }
        else if (pos >= transportSource.getLengthInSeconds())
        {
            transportSource.setPosition (0.0);
        }

        transportSource.start();
    }
}

void MixAnalyzerAudioProcessor::restart()
{
    if (! fileIsLoaded.load())
        return;

    transportSource.setPosition (sectionStartOrZero());
    transportSource.start();
}

void MixAnalyzerAudioProcessor::stopAndReset()
{
    if (! fileIsLoaded.load())
        return;

    transportSource.stop();
    transportSource.setPosition (sectionStartOrZero());
    clearAnalysis();   // flatten the spectrum immediately instead of decaying
}

void MixAnalyzerAudioProcessor::setLoopEnabled (bool shouldLoop)
{
    loopEnabled.store (shouldLoop);

    if (readerSource != nullptr)
        readerSource->setLooping (shouldLoop);   // whole-song loop on/off
}

void MixAnalyzerAudioProcessor::clearAnalysis() noexcept
{
    juce::FloatVectorOperations::clear (ringBuffer, fftSize);
}

bool MixAnalyzerAudioProcessor::isFilePlaying() const
{
    return transportSource.isPlaying();
}

bool MixAnalyzerAudioProcessor::hasFileLoaded() const
{
    return fileIsLoaded.load();
}

juce::String MixAnalyzerAudioProcessor::getLoadedFileName() const
{
    return loadedFileName;
}

void MixAnalyzerAudioProcessor::setSelection (double startSeconds, double endSeconds)
{
    if (endSeconds < startSeconds)
        std::swap (startSeconds, endSeconds);

    selectionStart.store (startSeconds);
    selectionEnd.store (endSeconds);

    // If we're already playing, jump into the new section straight away.
    if (fileIsLoaded.load() && transportSource.isPlaying())
    {
        const double pos = transportSource.getCurrentPosition();
        if (pos < startSeconds || pos >= endSeconds)
            transportSource.setPosition (startSeconds);
    }
}

void MixAnalyzerAudioProcessor::clearSelection()
{
    selectionStart.store (0.0);
    selectionEnd.store (0.0);
}

double MixAnalyzerAudioProcessor::getCurrentPositionSeconds() const
{
    return transportSource.getCurrentPosition();
}

double MixAnalyzerAudioProcessor::getLengthSeconds() const
{
    return transportSource.getLengthInSeconds();
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
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MixAnalyzerAudioProcessor();
}
