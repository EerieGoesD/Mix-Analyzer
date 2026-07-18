#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "MixLookAndFeel.h"
#include <vector>

//==============================================================================
// Sound Field meter (Insight-style): a stereo vectorscope (goniometer) plus a
// phase-correlation meter, with Correlation / Balance / Width readouts. It reads
// the processor's stereo capture; it never changes the audio.
//==============================================================================
class SoundFieldDisplay : public juce::Component,
                          private juce::Timer
{
public:
    explicit SoundFieldDisplay (MixAnalyzerAudioProcessor&);

    void paint (juce::Graphics&) override;
    void resized() override;

    void clearHistory();
    juce::String getSnapshotText() const;

    // Record the readings over time and export them as a CSV time-series.
    void setRecordIntervalSeconds (int seconds);
    void startRecording();
    void stopRecording();
    bool isRecording() const noexcept { return recording; }
    int  getRecordedCount() const noexcept { return (int) recordFrames.size(); }
    juce::String getRecordingText() const;

    //==========================================================================
    // Settings. VectorMode = how the scope is drawn; DetectMethod = how fast the
    // readings respond. (Stereo only - Surround needs a multichannel input.)
    enum class VectorMode   { lissajous, polarSample, polarLevel };
    enum class DetectMethod { peak, rms, envelope };

    VectorMode   getVectorMode()   const noexcept { return vectorMode; }
    DetectMethod getDetectMethod() const noexcept { return detectMethod; }
    void setVectorMode   (VectorMode v)   { vectorMode = v; repaint(); }
    void setDetectMethod (DetectMethod d) { detectMethod = d; }

    juce::String getSettingsString() const;
    void applySettingsString (const juce::String&);

private:
    void timerCallback() override;
    juce::String metaHeader() const;   // source / section / scope settings for exports
    void captureRecordFrame();

    MixAnalyzerAudioProcessor& proc;

    static constexpr int drawN = 2048;    // stereo pairs used per frame
    static constexpr int numBins = 72;    // angle bins for the Polar Level rays
    std::vector<float> bufL, bufR;
    juce::Image scopeImg;                 // phosphor persistence for the dot modes
    std::vector<float> envBins;           // smoothed per-bin level (Envelope detection)

    // Smoothed metrics.
    float correlation = 0.0f;   // -1 .. +1
    float balance     = 0.0f;   // -1 (hard left) .. +1 (hard right)
    float width       = 0.0f;   // side / mid energy (0 = mono)
    bool  haveSignal  = false;

    VectorMode   vectorMode   = VectorMode::lissajous;
    DetectMethod detectMethod = DetectMethod::peak;

    // Recording.
    struct SFRow { float corr, bal, wid; };
    std::vector<SFRow> recordFrames;
    bool recording = false;
    int  recordIntervalSec = 1;
    int  recordFrameCounter = 0;

    juce::Rectangle<int> readoutArea, scopeArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SoundFieldDisplay)
};
