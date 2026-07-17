#pragma once

#include <JuceHeader.h>
#include <vector>
#include "PluginProcessor.h"

//==============================================================================
// LIVE SPECTRUM meter (iZotope Insight style). Pulls the latest samples from the
// processor, runs (optionally overlap-averaged) FFTs with the chosen window, and
// shows the result as a raw line (Linear) or clean bars (1/3 / Critical / Full
// octave). Supports time-averaging, peak hold, freeze reference, and hover.
//==============================================================================
class SpectrumDisplay : public juce::Component,
                        private juce::Timer
{
public:
    // Live-meter window set (Insight): Hann, Hamming, Blackman, Bartlett, Kaiser.
    enum class WinType { hann, hamming, blackman, bartlett, kaiser };

    // Spectrum Type (Insight).
    enum class SpecType { linear, thirdOctave, critical, fullOctave };

    explicit SpectrumDisplay (MixAnalyzerAudioProcessor&);
    ~SpectrumDisplay() override;

    //== Settings ==============================================================
    void setSpectrumType (SpecType);
    SpecType getSpectrumType() const noexcept { return specType; }

    void setWindowOrder (int order);                 // 9..13 -> 512..8192
    int  getWindowOrder() const noexcept { return windowOrder; }

    void setWindowType (WinType);
    WinType getWindowType() const noexcept { return windowType; }

    void setAverageSeconds (float seconds);          // 0 = real-time, <0 = infinite
    float getAverageSeconds() const noexcept { return averageSeconds; }

    void setPeakHoldShown (bool shouldShow);
    bool isPeakHoldShown() const noexcept { return showPeakHold; }

    void setPeakHoldMs (float ms);                   // <0 = infinite hold
    float getPeakHoldMs() const noexcept { return peakHoldMs; }

    void setOverlapPercent (float percent);          // 0, 50, 75, 87.5
    float getOverlapPercent() const noexcept { return overlapPercent; }

    void freezeReference();
    void clearReference();
    bool hasReferenceCurve() const noexcept { return hasReference; }

    // The current live spectrum as "frequency<TAB>dB" lines, for a snapshot export.
    juce::String getExportText() const;

    // Record the live spectrum over time (one row per second) and export it as a
    // CSV time-series: first row = frequencies, each later row = time + dB values.
    void setRecordIntervalSeconds (int seconds);    // 1, 2, 5, 10, 30 ...
    void startRecording();
    void stopRecording();
    bool isRecording() const noexcept { return recording; }
    int  getRecordedCount() const noexcept { return (int) recordFrames.size(); }
    juce::String getRecordingText() const;

    //== Component =============================================================
    void paint (juce::Graphics&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;

private:
    void timerCallback() override;
    void rebuildFft();
    void fillWindowTable();
    void rebuildSlots (double sampleRate);           // slot = bin (linear) or band (octave)
    void computeFrame();
    void resetAveraging();

    juce::Rectangle<float> plotBounds() const;
    float freqToX (float freq, juce::Rectangle<float> plot) const;
    float xToFreq (float x, juce::Rectangle<float> plot) const;
    float levelToDb (float level) const;

    MixAnalyzerAudioProcessor& processorRef;

    static constexpr int maxFftSize = MixAnalyzerAudioProcessor::fftSize;  // 32768
    static constexpr float mindB = -100.0f;
    static constexpr float maxdB = 0.0f;

    // FFT config
    int windowOrder = 13;                       // default 8192
    int fftSize     = 1 << 13;
    WinType  windowType = WinType::hann;
    SpecType specType   = SpecType::thirdOctave;   // clean bands by default
    float overlapPercent = 50.0f;

    std::unique_ptr<juce::dsp::FFT> fft;
    std::vector<float> windowTable;             // sized fftSize
    std::vector<float> sampleBuf;               // sized maxFftSize
    std::vector<float> fftBuf;                  // sized 2 * fftSize
    std::vector<float> magSpec;                 // averaged magnitude per bin (fftSize/2+1)

    // Display slots: one per FFT bin (Linear) or one per band (octave modes).
    struct Slot { float loHz, hiHz, centreHz; };
    std::vector<Slot>  slots;
    std::vector<float> dispLevel;               // 0..1 shown level per slot
    std::vector<float> dispAvg;                 // running-average magnitude per slot
    std::vector<float> dispPeak;                // peak-hold level per slot
    std::vector<float> referenceData;           // frozen snapshot per slot

    // Cache to know when to rebuild slots.
    double lastSampleRate = 0.0;
    int    lastFftSize    = 0;
    SpecType lastSpecType = SpecType::linear;

    // Averaging / peak-hold state
    float averageSeconds = 0.0f;                // 0 real-time, <0 infinite
    long long avgCount   = 0;
    bool  showPeakHold   = false;
    float peakHoldMs     = 1000.0f;
    bool  hasReference   = false;

    float specFMin = 20.0f;
    float specFMax = 20000.0f;
    float specSampleRate = 44100.0f;

    // Hover
    bool hovering = false;
    juce::Point<float> mousePos;

    // Recording (time-series capture, one frame per second)
    void captureRecordFrame();
    bool recording = false;
    int  recordFrameCounter = 0;
    int  recordIntervalSec = 1;                     // capture every N seconds
    std::vector<float> recordFreqs;                 // column frequencies, fixed at start
    std::vector<std::vector<float>> recordFrames;   // rows of dB values

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SpectrumDisplay)
};
