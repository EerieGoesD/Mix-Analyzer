#pragma once

#include <JuceHeader.h>
#include <atomic>
#include <vector>

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

    void copyLatestSamples (float* dest) const noexcept;   // mono/mid, dest holds fftSize
    void copyLatestSide (float* dest) const noexcept;      // side = (L-R)/2, dest holds fftSize

    // Stereo capture for the Sound Field meter (vectorscope + correlation). The
    // audio thread keeps the newest L/R pairs; the GUI grabs the last N each frame.
    static constexpr int sfSize = 8192;                    // must stay a power of two
    void copyLatestStereo (float* destL, float* destR, int numSamples) const noexcept;

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

    void setPlaybackGain (float gain);   // monitoring level for the loaded song (analysis unaffected)

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

    //==========================================================================
    // Loudness meter (EBU R128 / BS.1770 via libebur128). The audio thread feeds
    // samples and publishes these figures ~10 times a second. LUFS for the three
    // loudness readings, LU for the range, dBTP for the peak. A value of -300
    // means "no reading yet / silence".
    float getMomentaryLufs()   const noexcept { return momentaryLufs.load(); }
    float getShortTermLufs()   const noexcept { return shortTermLufs.load(); }
    float getIntegratedLufs()  const noexcept { return integratedLufs.load(); }
    float getLoudnessRangeLU() const noexcept { return loudnessRangeLU.load(); }
    float getTruePeakDb()      const noexcept { return truePeakDb.load(); }

    // Run the (heavy) EBU R128 read-outs and publish them to the atomics above.
    // Called from the GUI timer, NOT the audio thread, so a big query burst can
    // never overrun an audio block. Feeding samples stays on the audio thread.
    void updateLoudnessReadings();

    //==========================================================================
    // Offline loudness measurement of the loaded file, read straight from disk -
    // no playback needed. Measures the whole song, or the current selection when
    // wholeSong is false. Only the whole-region figures make sense this way, so
    // Momentary / Short-term are deliberately not reported.
    struct OfflineLoudness
    {
        bool         ok         = false;
        juce::String message;                 // set when ok == false
        float        integrated = -300.0f;    // LUFS
        float        range      = 0.0f;       // LU
        float        truePeak   = -300.0f;    // dBTP
        double       startSec   = 0.0;
        double       endSec     = 0.0;        // the region actually measured
        bool         wholeSong  = true;
    };

    OfflineLoudness analyzeFileLoudness (bool wholeSong);

private:
    //==========================================================================
    void pushSample (float mid, float side) noexcept;   // spectrum: mono/mid + side rings
    void pushStereo (float l, float r) noexcept;        // Sound Field capture
    void clearAnalysis() noexcept;                 // zero the ring buffer
    double sectionStartOrZero() const noexcept;    // selection start, or 0
    void resetLoudness();                          // (re)create the EBU R128 state

    float ringBuffer[fftSize] = {};   // mono / mid = (L+R)/2
    float sideBuffer[fftSize] = {};   // side = (L-R)/2 (for the Side spectrum)
    std::atomic<int> writePos { 0 };
    std::atomic<bool> loopEnabled { true };

    float sfLeft[sfSize]  = {};
    float sfRight[sfSize] = {};
    std::atomic<int> sfWritePos { 0 };

    //==========================================================================
    juce::AudioFormatManager formatManager;
    juce::TimeSliceThread readAheadThread { "fileReadAhead" };
    std::unique_ptr<juce::AudioFormatReaderSource> readerSource;   // message-thread only
    juce::AudioTransportSource transportSource;   // thread-safe on its own

    juce::String loadedFileName;
    juce::File   loadedFile;

    std::atomic<bool> fileIsLoaded { false };
    std::atomic<float> playbackGain { 1.0f };   // song monitoring volume (post-analysis)
    std::atomic<bool> requestStop  { false };
    std::atomic<double> selectionStart { 0.0 };
    std::atomic<double> selectionEnd   { 0.0 };

    //==========================================================================
    // Loudness state. loudnessState is an ebur128_state* held opaquely so the C
    // header stays out of this header. A spin lock guards it: the message thread
    // creates/destroys it, the audio thread feeds it with a try-lock (and simply
    // skips a block if the state is being rebuilt).
    void*  loudnessState = nullptr;
    juce::SpinLock loudnessLock;
    double currentSampleRate = 0.0;
    int    loudnessChannels = 2;
    std::vector<float> interleaveBuf;

    std::atomic<float> momentaryLufs   { -300.0f };
    std::atomic<float> shortTermLufs   { -300.0f };
    std::atomic<float> integratedLufs  { -300.0f };
    std::atomic<float> loudnessRangeLU { 0.0f };
    std::atomic<float> truePeakDb      { -300.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MixAnalyzerAudioProcessor)
};
