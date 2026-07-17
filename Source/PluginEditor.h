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
    void showMeter (int meter);        // 0 Spectrum, 1 Sound Field, 2 Loudness
    void toggleSidebar();
    void updateNavHighlight();

    MixAnalyzerAudioProcessor& processorRef;

    // Controls.
    juce::TextButton loadButton { "Load song..." };
    TransportButton restartButton   { "Restart",    TransportButton::Icon::restart };
    TransportButton playPauseButton { "Play/Pause", TransportButton::Icon::play };
    TransportButton stopButton      { "Stop",       TransportButton::Icon::stop };
    juce::ToggleButton loopButton   { "Loop" };
    juce::TextButton settingsButton { "Settings" };
    juce::TextButton spectrumExportButton { "Snapshot" };
    juce::TextButton screenshotButton { "Screenshot" };
    juce::TextButton recordButton { "Record" };
    juce::ComboBox   recordIntervalBox;
    juce::TextButton liveTabButton    { "Live" };
    juce::TextButton historyTabButton { "History" };

    // Footer links (styled like the Size Scanner footer).
    juce::Label madeByLabel;
    juce::Label footerSep1, footerSep2, footerSep3, footerSep4;
    juce::HyperlinkButton coffeeLink   { "Support This Project", juce::URL ("https://buymeacoffee.com/eeriegoesd") };
    juce::HyperlinkButton eerieLink     { "EERIE", juce::URL ("https://eeriegoesd.com") };
    juce::HyperlinkButton reportLink    { "Report Issue",    juce::URL ("https://github.com/EerieGoesD/Mix-Analyzer/issues/new?template=bug-report.md") };
    juce::HyperlinkButton feedbackLink  { "Feedback",        juce::URL ("https://github.com/EerieGoesD/Mix-Analyzer/discussions") };
    juce::HyperlinkButton featureLink   { "Suggest Feature", juce::URL ("https://github.com/EerieGoesD/Mix-Analyzer/issues/new?template=feature-request.md") };
    juce::Label fileLabel;
    std::unique_ptr<juce::FileChooser> chooser;

    WaveformDisplay waveform;
    SpectrumDisplay spectrum;
    HistoryAnalyzer history;
    bool historyMode = false;

    // Left sidebar (like NoteStash): a hamburger toggles it; nav picks the meter.
    juce::TextButton menuButton { juce::String::fromUTF8 ("\xE2\x98\xB0") };   // hamburger
    juce::TextButton navSpectrum   { "Spectrum" };
    juce::TextButton navSoundField { "Sound Field" };
    juce::TextButton navLoudness   { "Loudness" };
    bool sidebarOpen = true;
    int  currentMeter = 0;
    juce::Rectangle<int> sidebarArea, meterArea;

    MixLookAndFeel lookAndFeel;
    juce::TooltipWindow tooltipWindow { this };

    juce::Rectangle<int> waveCaptionBounds;
    juce::Rectangle<int> spectrumCaptionBounds;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MixAnalyzerAudioProcessorEditor)
};
