#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "WaveformDisplay.h"
#include "SpectrumDisplay.h"
#include "HistoryAnalyzer.h"
#include "MixLookAndFeel.h"
#include "TransportButton.h"

//==============================================================================
class MixAnalyzerAudioProcessorEditor : public juce::AudioProcessorEditor,
                                        private juce::Timer
{
public:
    explicit MixAnalyzerAudioProcessorEditor (MixAnalyzerAudioProcessor&);
    ~MixAnalyzerAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void parentHierarchyChanged() override;

private:
    void timerCallback() override;
    void openFileChooser();
    void updateTransportUI();
    void setTransportEnabled (bool enabled);
    void showSpectrumSettings();
    void showHistory (bool showHist);

    MixAnalyzerAudioProcessor& processorRef;

    // Controls.
    juce::TextButton loadButton { "Load song..." };
    TransportButton restartButton   { "Restart",    TransportButton::Icon::restart };
    TransportButton playPauseButton { "Play/Pause", TransportButton::Icon::play };
    TransportButton stopButton      { "Stop",       TransportButton::Icon::stop };
    juce::ToggleButton loopButton   { "Loop" };
    juce::TextButton settingsButton { "Settings" };
    juce::TextButton spectrumExportButton { "Snapshot" };
    juce::TextButton recordButton { "Record" };
    juce::ComboBox   recordIntervalBox;
    juce::TextButton liveTabButton    { "Live" };
    juce::TextButton historyTabButton { "History" };
    juce::Label fileLabel;
    std::unique_ptr<juce::FileChooser> chooser;

    WaveformDisplay waveform;
    SpectrumDisplay spectrum;
    HistoryAnalyzer history;
    bool historyMode = false;

    MixLookAndFeel lookAndFeel;
    juce::TooltipWindow tooltipWindow { this };

    juce::Rectangle<int> waveCaptionBounds;
    juce::Rectangle<int> spectrumCaptionBounds;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MixAnalyzerAudioProcessorEditor)
};
