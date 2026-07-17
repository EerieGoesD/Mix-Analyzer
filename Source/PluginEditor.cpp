#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "SpectrumSettingsPanel.h"

//==============================================================================
MixAnalyzerAudioProcessorEditor::MixAnalyzerAudioProcessorEditor (MixAnalyzerAudioProcessor& p)
    : AudioProcessorEditor (&p),
      processorRef (p),
      spectrum (p),
      history (p)
{
    setLookAndFeel (&lookAndFeel);

    addAndMakeVisible (loadButton);
    addAndMakeVisible (restartButton);
    addAndMakeVisible (playPauseButton);
    addAndMakeVisible (stopButton);
    addAndMakeVisible (loopButton);
    addAndMakeVisible (settingsButton);
    addAndMakeVisible (liveTabButton);
    addAndMakeVisible (historyTabButton);
    addAndMakeVisible (fileLabel);
    addAndMakeVisible (waveform);
    addAndMakeVisible (spectrum);
    addChildComponent (history);   // hidden until History tab is chosen

    fileLabel.setText ("No song loaded", juce::dontSendNotification);
    fileLabel.setFont (MixLookAndFeel::monoFont (12.5f));
    fileLabel.setColour (juce::Label::textColourId, MixColours::textDim);
    fileLabel.setJustificationType (juce::Justification::centredLeft);

    loadButton.setColour (juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    loadButton.onClick = [this] { openFileChooser(); };

    restartButton.onClick   = [this] { processorRef.restart();        updateTransportUI(); };
    playPauseButton.onClick = [this] { processorRef.togglePlayback(); updateTransportUI(); };
    stopButton.onClick      = [this] { processorRef.stopAndReset();   updateTransportUI(); };

    restartButton.setTooltip ("Start from the beginning");
    playPauseButton.setTooltip ("Play / Pause");
    stopButton.setTooltip ("Stop and reset");

    loopButton.setColour (juce::ToggleButton::textColourId, MixColours::textDim);
    loopButton.setToggleState (true, juce::dontSendNotification);
    processorRef.setLoopEnabled (true);
    loopButton.onClick = [this] { processorRef.setLoopEnabled (loopButton.getToggleState()); };

    settingsButton.setColour (juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    settingsButton.onClick = [this] { showSpectrumSettings(); };

    addAndMakeVisible (spectrumExportButton);
    spectrumExportButton.setColour (juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    spectrumExportButton.setTooltip ("Save the spectrum exactly as it looks right now - one single moment.");
    spectrumExportButton.onClick = [this]
    {
        chooser = std::make_unique<juce::FileChooser> ("Export live spectrum", juce::File{}, "*.txt");
        chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
                              [this] (const juce::FileChooser& fc)
        {
            const auto f = fc.getResult();
            if (f != juce::File{})
                f.replaceWithText (spectrum.getExportText());
        });
    };

    // Record interval dropdown (how often Record captures).
    addAndMakeVisible (recordIntervalBox);
    recordIntervalBox.addItem ("Every 1s",  1);
    recordIntervalBox.addItem ("Every 2s",  2);
    recordIntervalBox.addItem ("Every 5s",  3);
    recordIntervalBox.addItem ("Every 10s", 4);
    recordIntervalBox.addItem ("Every 30s", 5);
    recordIntervalBox.setSelectedId (1, juce::dontSendNotification);
    recordIntervalBox.setTooltip ("How often Record saves a snapshot while it runs.");
    recordIntervalBox.onChange = [this]
    {
        const int secs[] = { 1, 2, 5, 10, 30 };
        spectrum.setRecordIntervalSeconds (secs[juce::jlimit (0, 4, recordIntervalBox.getSelectedId() - 1)]);
    };

    addAndMakeVisible (recordButton);
    recordButton.setColour (juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    recordButton.setTooltip ("Record the spectrum over TIME - it keeps saving snapshots at the chosen interval until you stop, then writes them all to one file.");
    recordButton.onClick = [this]
    {
        if (spectrum.isRecording())
        {
            spectrum.stopRecording();
            recordButton.setButtonText ("Record");
            recordButton.setColour (juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
            recordIntervalBox.setEnabled (true);

            // Save the recorded time-series as CSV.
            chooser = std::make_unique<juce::FileChooser> ("Save recorded spectrum", juce::File{}, "*.csv");
            chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
                                  [this] (const juce::FileChooser& fc)
            {
                const auto f = fc.getResult();
                if (f != juce::File{})
                    f.replaceWithText (spectrum.getRecordingText());
            });
        }
        else
        {
            spectrum.startRecording();
            recordButton.setButtonText ("Stop & save");
            recordButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xffef4444));
            recordIntervalBox.setEnabled (false);
        }
    };

    liveTabButton.setColour (juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    historyTabButton.setColour (juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    liveTabButton.onClick    = [this] { showHistory (false); };
    historyTabButton.onClick = [this] { showHistory (true);  };

    setTransportEnabled (false);
    showHistory (false);

    waveform.onSelectionChanged = [this] (double s, double e) { processorRef.setSelection (s, e); };
    waveform.onSelectionCleared = [this] { processorRef.clearSelection(); };

    // Resizable so the standalone window gets a maximize button and can be
    // dragged bigger; the layout stretches to fill.
    setResizable (true, false);
    setResizeLimits (700, 520, 4000, 3000);
    setSize (820, 580);
    startTimerHz (60);
}

MixAnalyzerAudioProcessorEditor::~MixAnalyzerAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void MixAnalyzerAudioProcessorEditor::parentHierarchyChanged()
{
    // The standalone window ships with only minimise + close. Once we're inside
    // it, re-enable all three buttons so it gets a maximize button. In a plugin
    // the top-level isn't a DocumentWindow, so this safely does nothing.
    if (auto* dw = dynamic_cast<juce::DocumentWindow*> (getTopLevelComponent()))
        dw->setTitleBarButtonsRequired (juce::DocumentWindow::allButtons, false);
}

void MixAnalyzerAudioProcessorEditor::setTransportEnabled (bool enabled)
{
    restartButton.setEnabled (enabled);
    playPauseButton.setEnabled (enabled);
    stopButton.setEnabled (enabled);
}

//==============================================================================
void MixAnalyzerAudioProcessorEditor::timerCallback()
{
    // The audio thread asks us to stop when a non-looping section reaches its end.
    if (processorRef.consumeStopRequest())
        processorRef.stopAndReset();

    // Move the playhead on the waveform.
    const double len = processorRef.getLengthSeconds();
    if (len > 0.0)
        waveform.setPlayheadProportion (processorRef.getCurrentPositionSeconds() / len);
    else
        waveform.setPlayheadProportion (-1.0);

    updateTransportUI();
}

void MixAnalyzerAudioProcessorEditor::updateTransportUI()
{
    playPauseButton.setIcon (processorRef.isFilePlaying() ? TransportButton::Icon::pause
                                                          : TransportButton::Icon::play);
}

void MixAnalyzerAudioProcessorEditor::showSpectrumSettings()
{
    auto panel = std::make_unique<SpectrumSettingsPanel> (spectrum);
    panel->setLookAndFeel (&lookAndFeel);
    juce::CallOutBox::launchAsynchronously (std::move (panel),
                                            settingsButton.getScreenBounds(),
                                            nullptr);
}

void MixAnalyzerAudioProcessorEditor::showHistory (bool showHist)
{
    historyMode = showHist;
    spectrum.setVisible (! showHist);
    settingsButton.setVisible (! showHist);
    spectrumExportButton.setVisible (! showHist);
    recordButton.setVisible (! showHist);
    recordIntervalBox.setVisible (! showHist);
    history.setVisible (showHist);

    // The active tab is a filled indigo button; the inactive one is a ghost.
    liveTabButton.setColour    (juce::TextButton::buttonColourId,
                                showHist ? juce::Colours::transparentBlack : MixColours::accent);
    historyTabButton.setColour (juce::TextButton::buttonColourId,
                                showHist ? MixColours::accent : juce::Colours::transparentBlack);
    liveTabButton.repaint();
    historyTabButton.repaint();
}

//==============================================================================
void MixAnalyzerAudioProcessorEditor::openFileChooser()
{
    chooser = std::make_unique<juce::FileChooser> (
        "Choose a song to analyze", juce::File{},
        "*.wav;*.aiff;*.aif;*.flac;*.ogg;*.mp3");

    const auto chooserFlags = juce::FileBrowserComponent::openMode
                            | juce::FileBrowserComponent::canSelectFiles;

    chooser->launchAsync (chooserFlags, [this] (const juce::FileChooser& fc)
    {
        const auto file = fc.getResult();

        if (file == juce::File{})
            return;

        processorRef.loadFile (file);

        if (processorRef.hasFileLoaded())
        {
            fileLabel.setText (processorRef.getLoadedFileName(), juce::dontSendNotification);
            fileLabel.setColour (juce::Label::textColourId, MixColours::text);
            setTransportEnabled (true);
            waveform.setFile (file);
        }
        else
        {
            fileLabel.setText ("Could not open that file", juce::dontSendNotification);
            fileLabel.setColour (juce::Label::textColourId, juce::Colours::orangered);
            setTransportEnabled (false);
            waveform.clear();
        }

        updateTransportUI();
    });
}

//==============================================================================
void MixAnalyzerAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (MixColours::bg);

    // Header: app name in indigo with a soft glow.
    auto header = getLocalBounds().removeFromTop (48).reduced (16, 0);

    g.setColour (MixColours::accent.withAlpha (0.35f));
    g.setFont (MixLookAndFeel::uiFont (19.0f, true));
    for (auto d : { juce::Point<int> (0, 1), juce::Point<int> (1, 0) })
        g.drawText ("EERIE | Mix Analyzer", header.translated (d.x, d.y),
                    juce::Justification::centredLeft);

    g.setColour (MixColours::accentH);
    g.drawText ("EERIE | Mix Analyzer", header, juce::Justification::centredLeft);

    // Section captions (uppercase, dimmed).
    auto drawCaption = [&g] (juce::Rectangle<int> area, const juce::String& t)
    {
        if (area.isEmpty()) return;
        g.setColour (MixColours::textDim);
        g.setFont (MixLookAndFeel::uiFont (10.5f, false));
        g.drawText (t.toUpperCase(), area, juce::Justification::centredLeft);
    };

    drawCaption (waveCaptionBounds, "Waveform  -  drag to select a section");
    drawCaption (spectrumCaptionBounds, "Spectrum");
}

//==============================================================================
void MixAnalyzerAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();

    bounds.removeFromTop (48);   // header (painted)

    // Control strip: Load, transport icons, Loop, and the file name label.
    auto controls = bounds.removeFromTop (40).reduced (16, 5);
    loadButton.setBounds (controls.removeFromLeft (116));
    controls.removeFromLeft (12);
    restartButton.setBounds   (controls.removeFromLeft (34));
    controls.removeFromLeft (6);
    playPauseButton.setBounds (controls.removeFromLeft (34));
    controls.removeFromLeft (6);
    stopButton.setBounds      (controls.removeFromLeft (34));
    controls.removeFromLeft (14);
    loopButton.setBounds (controls.removeFromLeft (72));
    controls.removeFromLeft (12);
    fileLabel.setBounds (controls);

    // Waveform: caption strip, then the waveform panel.
    waveCaptionBounds = bounds.removeFromTop (20).reduced (16, 0);
    waveform.setBounds (bounds.removeFromTop (110).reduced (16, 2));

    // Tab strip: Live / History on the left, Settings (live only) on the right.
    auto tabRow = bounds.removeFromTop (28).reduced (16, 0);
    liveTabButton.setBounds    (tabRow.removeFromLeft (56).withSizeKeepingCentre (56, 22));
    tabRow.removeFromLeft (4);
    historyTabButton.setBounds (tabRow.removeFromLeft (70).withSizeKeepingCentre (70, 22));
    settingsButton.setBounds       (tabRow.removeFromRight (88).withSizeKeepingCentre (88, 22));
    tabRow.removeFromRight (6);
    spectrumExportButton.setBounds (tabRow.removeFromRight (78).withSizeKeepingCentre (78, 22));
    tabRow.removeFromRight (6);
    recordButton.setBounds         (tabRow.removeFromRight (88).withSizeKeepingCentre (88, 22));
    tabRow.removeFromRight (4);
    recordIntervalBox.setBounds    (tabRow.removeFromRight (88).withSizeKeepingCentre (88, 22));
    spectrumCaptionBounds = {};   // tabs replace the caption

    auto viewArea = bounds.reduced (16, 2).withTrimmedBottom (12);
    spectrum.setBounds (viewArea);
    history.setBounds (viewArea);
}
