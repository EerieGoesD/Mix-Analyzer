#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "MixLookAndFeel.h"
#include <vector>

//==============================================================================
// Loudness meter (EBU R128 / BS.1770). Shows the momentary, short-term and
// integrated loudness, the loudness range and the true peak, plus a scrolling
// history graph of loudness over time. It only reads the numbers the processor
// measures on the audio thread; it never touches the audio itself.
//==============================================================================
class LoudnessDisplay : public juce::Component,
                        private juce::Timer
{
public:
    explicit LoudnessDisplay (MixAnalyzerAudioProcessor&);

    void paint (juce::Graphics&) override;
    void resized() override;

    void clearHistory();                 // wipe the graph (new song / stop)
    juce::String getSnapshotText() const; // current readings as text

    // Record the readings over time and export them as a CSV time-series.
    void setRecordIntervalSeconds (int seconds);   // 1, 2, 5, 10, 30 ...
    void startRecording();
    void stopRecording();
    bool isRecording() const noexcept { return recording; }
    int  getRecordedCount() const noexcept { return (int) recordFrames.size(); }
    juce::String getRecordingText() const;

    //==========================================================================
    // Settings (driven by the settings popover / a chosen standard preset). These
    // change how the meter is shown - the loudness the processor measures is the
    // standard EBU R128 / BS.1770 value either way.
    enum class RangeScale { dbLinear, dbNonLinear, bs1771, ebuPlus9, ebuPlus18 };
    enum class ScaleMode  { absolute, relative };
    enum class GateMode   { off, dialogue, program };
    enum class ViewMode   { timeline, bars };   // loudness over time, or vertical bar meters

    RangeScale getRangeScale()    const noexcept { return rangeScale; }
    ScaleMode  getScaleMode()     const noexcept { return scaleMode; }
    GateMode   getGateMode()      const noexcept { return gateMode; }
    ViewMode   getViewMode()      const noexcept { return viewMode; }
    float      getPeakTarget()    const noexcept { return peakTarget; }
    float      getLoudnessTarget()const noexcept { return loudnessTarget; }
    float      getLraTarget()     const noexcept { return lraTarget; }

    void setRangeScale (RangeScale r) { rangeScale = r; repaint(); }
    void setScaleMode  (ScaleMode s)  { scaleMode  = s; repaint(); }
    void setGateMode   (GateMode gm)  { gateMode   = gm; }
    void setViewMode   (ViewMode v)   { viewMode   = v; repaint(); }
    void setPeakTarget (float v)      { peakTarget = v; repaint(); }
    void setLoudnessTarget (float v)  { loudnessTarget = v; repaint(); }
    void setLraTarget  (float v)      { lraTarget  = v; repaint(); }

    // Serialise / restore every setting above (for user presets).
    juce::String getSettingsString() const;
    void applySettingsString (const juce::String&);

private:
    void timerCallback() override;
    void getGraphWindow (float& topLufs, float& botLufs) const;
    float toDisplay (float lufs) const;   // absolute LUFS or LU relative to target
    void captureRecordFrame();

    MixAnalyzerAudioProcessor& proc;

    RangeScale rangeScale     = RangeScale::dbLinear;   // full view suits loud music
    ScaleMode  scaleMode      = ScaleMode::absolute;
    GateMode   gateMode       = GateMode::program;
    ViewMode   viewMode       = ViewMode::timeline;
    float      peakTarget     = -1.0f;    // dBTP
    float      loudnessTarget = -23.0f;   // LUFS (EBU R128 default)
    float      lraTarget      = 15.0f;    // LU

    // Rolling history for the graph (oldest at the front, newest at the back).
    std::vector<float> shortHistory, momHistory;
    static constexpr int maxHistory = 3600;   // ~2 minutes at 30 Hz

    // Latest readings.
    float mLufs = -300.0f, sLufs = -300.0f, iLufs = -300.0f;
    float lra   = 0.0f,    tp    = -300.0f;

    // Recording (time-series capture at the chosen interval).
    struct LoudRow { float m, s, i, lra, tp; };
    std::vector<LoudRow> recordFrames;
    bool recording = false;
    int  recordIntervalSec = 1;
    int  recordFrameCounter = 0;

    juce::Rectangle<int> readoutArea, graphArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LoudnessDisplay)
};
