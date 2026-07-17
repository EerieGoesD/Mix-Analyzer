#pragma once

#include <JuceHeader.h>
#include <atomic>

//==============================================================================
// EERIE - Mix Analyzer
// Analysis-only, pass-through audio plugin. It taps the audio going through it
// (the master bus in a DAW, or a loaded song in standalone) and drives the
// meters. It never changes the sound.
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

    //==========================================================================
    // Spectrum source. The audio thread keeps writing the latest mono samples
    // into a rolling ring buffer; the GUI grabs the newest fftSize samples each
    // frame. That means the display refresh follows the GUI timer (smooth),
    // not the audio block size (which was making it choppy).
    // Ring-buffer / max analysis size. 32768 = the largest live-meter window;
    // the display picks a smaller slice. (The Analyze-selection snapshot reads
    // the file directly, so its bigger sizes do not need this buffer.)
    static constexpr int fftOrder = 15;             // 2^15 = 32768
    static constexpr int fftSize  = 1 << fftOrder;

    void copyLatestSamples (float* dest) const noexcept;   // dest must hold fftSize

    //==========================================================================
    // File playback (standalone: load a song and play it through the analyzer).
    // With no file loaded the plugin just passes host audio through.
    void loadFile (const juce::File& file);
    void togglePlayback();     // play / pause (keeps position)
    void restart();            // jump to the start (of section or song) and play
    void stopAndReset();       // stop and reset to the start; flatten the meters
    bool isFilePlaying() const;
    bool hasFileLoaded() const;
    juce::String getLoadedFileName() const;

    // When looping is on, playback repeats (the section if one is selected,
    // otherwise the whole song). When off, it plays once and stops at the end.
    void setLoopEnabled (bool shouldLoop);

    // The audio thread can ask the message thread to stop (section end, no loop).
    // The editor polls this from its timer. Returns true once per request.
    bool consumeStopRequest() noexcept { return requestStop.exchange (false); }

    // Section selection, in seconds. end <= start means "whole file".
    void setSelection (double startSeconds, double endSeconds);
    void clearSelection();

    double getCurrentPositionSeconds() const;
    double getLengthSeconds() const;

    // For the History analyzer: the loaded file + current selection (seconds).
    juce::File getLoadedFile() const                    { return loadedFile; }
    double getSelectionStartSeconds() const noexcept    { return selectionStart.load(); }
    double getSelectionEndSeconds() const noexcept      { return selectionEnd.load(); }

private:
    //==========================================================================
    void pushSample (float sample) noexcept;
    void clearAnalysis() noexcept;                 // zero the ring buffer
    double sectionStartOrZero() const noexcept;    // selection start, or 0

    float ringBuffer[fftSize] = {};
    std::atomic<int> writePos { 0 };
    std::atomic<bool> loopEnabled { true };

    //==========================================================================
    juce::AudioFormatManager formatManager;
    juce::TimeSliceThread readAheadThread { "fileReadAhead" };
    std::unique_ptr<juce::AudioFormatReaderSource> readerSource;   // message-thread only
    juce::AudioTransportSource transportSource;   // thread-safe on its own

    juce::String loadedFileName;
    juce::File   loadedFile;

    std::atomic<bool> fileIsLoaded { false };
    std::atomic<bool> requestStop  { false };
    std::atomic<double> selectionStart { 0.0 };
    std::atomic<double> selectionEnd   { 0.0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MixAnalyzerAudioProcessor)
};
