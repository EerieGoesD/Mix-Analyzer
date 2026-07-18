#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "WaveformDisplay.h"
#include "SpectrumDisplay.h"
#include "HistoryAnalyzer.h"
#include "LoudnessDisplay.h"
#include "SoundFieldDisplay.h"
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
    bool keyPressed (const juce::KeyPress&) override;   // space = play/pause, ctrl+space = restart

private:
    void timerCallback() override;
    void openFileChooser();
    void updateTransportUI();
    void setTransportEnabled (bool enabled);
    void showSpectrumSettings();
    void showLoudnessSettings();
    void showSoundFieldSettings();
    void setWaveformVisible (bool shouldShow);
    void showHistory (bool showHist);
    void toggleMeter (int meter);      // 0 Spectrum, 1 Sound Field, 2 Loudness (multi-select)
    void updateMeterVisibility();      // show/hide each meter's display + tools per meterOn[]
    void layoutMeterTile (int meter, juce::Rectangle<int> tile, bool isFirst);
    void toggleSidebar();
    void updateNavHighlight();
    void showLayoutPicker();
    void showMeterMenu (int meter);     // the "more" menu for a narrow tile
    static void setRecordVisual (juce::TextButton&, bool recording);   // red dot idle / red fill recording

    MixAnalyzerAudioProcessor& processorRef;

    // Controls.
    juce::TextButton loadButton { "Load song..." };
    TransportButton restartButton   { "Restart",    TransportButton::Icon::restart };
    TransportButton playPauseButton { "Play/Pause", TransportButton::Icon::play };
    TransportButton stopButton      { "Stop",       TransportButton::Icon::stop };
    juce::ToggleButton loopButton   { "Loop" };
    juce::Label        waveformLabel;                   // "Waveform" caption for the radios
    juce::ToggleButton waveformYes { "Yes" };           // radio group: show / hide the waveform
    juce::ToggleButton waveformNo  { "No" };
    juce::TextButton settingsButton { "Settings" };
    juce::TextButton loudnessSettingsButton { "Settings" };
    juce::TextButton spectrumExportButton { "Snapshot" };
    juce::TextButton screenshotButton { "Screenshot" };
    juce::TextButton recordButton { "Record" };
    juce::ComboBox   recordIntervalBox;

    // Loudness meter's own record / snapshot / screenshot (mirrors the spectrum).
    juce::TextButton loudnessSnapshotButton   { "Snapshot" };
    juce::TextButton loudnessScreenshotButton { "Screenshot" };
    juce::TextButton loudnessRecordButton     { "Record" };
    juce::ComboBox   loudnessIntervalBox;

    // Sound Field meter's own tools.
    juce::TextButton soundFieldSettingsButton   { "Settings" };
    juce::TextButton soundFieldSnapshotButton   { "Snapshot" };
    juce::TextButton soundFieldScreenshotButton { "Screenshot" };
    juce::TextButton soundFieldRecordButton     { "Record" };
    juce::ComboBox   soundFieldIntervalBox;
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
    LoudnessDisplay loudness { processorRef };
    SoundFieldDisplay soundField { processorRef };
    bool historyMode = false;

    // Layout picker (top-left) + per-meter "more" menus for narrow tiles.
    juce::TextButton layoutButton { juce::String::fromUTF8 ("\xE2\x8A\x9E  Layout") };   // grid glyph + label
    juce::TextButton spectrumMoreButton   { juce::String::fromUTF8 ("\xE2\x8B\xAF") };
    juce::TextButton soundFieldMoreButton { juce::String::fromUTF8 ("\xE2\x8B\xAF") };
    juce::TextButton loudnessMoreButton   { juce::String::fromUTF8 ("\xE2\x8B\xAF") };
    int layoutIndex = 0;

    // Left sidebar (like NoteStash): a hamburger toggles it; nav picks the meter.
    juce::TextButton menuButton { juce::String::fromUTF8 ("\xE2\x98\xB0") };   // hamburger
    juce::TextButton navSpectrum   { "Spectrum" };
    juce::TextButton navSoundField { "Sound Field" };
    juce::TextButton navLoudness   { "Loudness" };
    bool sidebarOpen = true;
    bool waveformVisible = true;
    bool meterOn[3] = { true, false, false };   // Spectrum on by default; multi-select
    float sidebarW = 128.0f;         // current animated width
    float sidebarTargetW = 128.0f;   // target (128 open, 40 collapsed = just the menu)
    juce::Rectangle<int> sidebarArea;
    juce::Rectangle<int> soundFieldCaptionBounds, loudnessCaptionBounds;   // per-tile name captions

    MixLookAndFeel lookAndFeel;
    juce::TooltipWindow tooltipWindow { this };

    juce::Rectangle<int> waveCaptionBounds;
    juce::Rectangle<int> spectrumCaptionBounds;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MixAnalyzerAudioProcessorEditor)
};
