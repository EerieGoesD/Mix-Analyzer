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

    addAndMakeVisible (screenshotButton);
    screenshotButton.setColour (juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    screenshotButton.setTooltip ("Save a PNG picture of the spectrum as it looks now.");
    screenshotButton.onClick = [this]
    {
        const auto img = spectrum.createComponentSnapshot (spectrum.getLocalBounds());
        chooser = std::make_unique<juce::FileChooser> ("Save spectrum image", juce::File{}, "*.png");
        chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
                              [img] (const juce::FileChooser& fc)
        {
            auto f = fc.getResult();
            if (f == juce::File{})
                return;

            f = f.withFileExtension ("png");   // always PNG

            f.deleteFile();
            if (auto os = f.createOutputStream())
            {
                juce::PNGImageFormat fmt;
                fmt.writeImageToStream (img, *os);
            }
        });
    };

    // Footer links (bigger and readable, with visible separators).
    const juce::Font footerFont = MixLookAndFeel::uiFont (15.0f, false);

    auto setupLink = [this, footerFont] (juce::HyperlinkButton& link, juce::Colour col)
    {
        addAndMakeVisible (link);
        link.setColour (juce::HyperlinkButton::textColourId, col);
        link.setFont (footerFont, false, juce::Justification::centred);
    };
    setupLink (coffeeLink,   juce::Colour (0xfffacc15));   // yellow
    setupLink (reportLink,   MixColours::textDim);
    setupLink (feedbackLink, MixColours::textDim);
    setupLink (featureLink,  MixColours::textDim);
    setupLink (eerieLink,    MixColours::accentH);

    // Separators are empty spacers; the vertical divider bar is drawn in paint().
    for (auto* s : { &footerSep1, &footerSep2, &footerSep3, &footerSep4 })
        addAndMakeVisible (*s);

    addAndMakeVisible (madeByLabel);
    madeByLabel.setText ("Made by", juce::dontSendNotification);
    madeByLabel.setFont (footerFont);
    madeByLabel.setColour (juce::Label::textColourId, MixColours::textDim);
    madeByLabel.setBorderSize (juce::BorderSize<int> (0));   // kill the default 5px side margin
    madeByLabel.setJustificationType (juce::Justification::centredRight);

    liveTabButton.setColour (juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    historyTabButton.setColour (juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    liveTabButton.onClick    = [this] { showHistory (false); };
    historyTabButton.onClick = [this] { showHistory (true);  };

    // Left sidebar menu (NoteStash pattern).
    addAndMakeVisible (menuButton);
    menuButton.setColour (juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    menuButton.setTooltip ("Show / hide the meter menu");
    menuButton.onClick = [this] { toggleSidebar(); };

    for (auto* n : { &navSpectrum, &navSoundField, &navLoudness })
    {
        addAndMakeVisible (*n);
        n->setColour (juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    }
    navSpectrum.onClick   = [this] { showMeter (0); };
    navSoundField.onClick = [this] { showMeter (1); };
    navLoudness.onClick   = [this] { showMeter (2); };

    setTransportEnabled (false);
    showHistory (false);
    showMeter (0);

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
    screenshotButton.setVisible (! showHist);
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

void MixAnalyzerAudioProcessorEditor::showMeter (int meter)
{
    currentMeter = meter;
    const bool spec = (meter == 0);

    liveTabButton.setVisible (spec);
    historyTabButton.setVisible (spec);

    if (spec)
    {
        showHistory (historyMode);   // restores spectrum/history + their controls
    }
    else
    {
        spectrum.setVisible (false);
        history.setVisible (false);
        settingsButton.setVisible (false);
        spectrumExportButton.setVisible (false);
        screenshotButton.setVisible (false);
        recordButton.setVisible (false);
        recordIntervalBox.setVisible (false);
    }

    updateNavHighlight();
    resized();
    repaint();
}

void MixAnalyzerAudioProcessorEditor::toggleSidebar()
{
    sidebarOpen = ! sidebarOpen;
    for (auto* n : { &navSpectrum, &navSoundField, &navLoudness })
        n->setVisible (sidebarOpen);
    resized();
    repaint();
}

void MixAnalyzerAudioProcessorEditor::updateNavHighlight()
{
    auto setNav = [] (juce::TextButton& b, bool active)
    {
        b.setColour (juce::TextButton::buttonColourId,
                     active ? MixColours::accent : juce::Colours::transparentBlack);
        b.repaint();
    };
    setNav (navSpectrum,   currentMeter == 0);
    setNav (navSoundField, currentMeter == 1);
    setNav (navLoudness,   currentMeter == 2);
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

    // Header: app name in indigo with a soft glow (shifted for the menu button).
    auto header = getLocalBounds().removeFromTop (48).reduced (16, 0).withTrimmedLeft (34);

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

    // Sidebar background + right divider.
    if (sidebarOpen && ! sidebarArea.isEmpty())
    {
        g.setColour (MixColours::surface);
        g.fillRect (sidebarArea);
        g.setColour (MixColours::border);
        g.drawVerticalLine (sidebarArea.getRight() - 1, (float) sidebarArea.getY(), (float) sidebarArea.getBottom());
    }

    // Placeholder panel for meters not built yet.
    if (currentMeter != 0 && ! meterArea.isEmpty())
    {
        g.setColour (MixColours::surface);
        g.fillRoundedRectangle (meterArea.toFloat(), 8.0f);
        g.setColour (MixColours::border);
        g.drawRoundedRectangle (meterArea.toFloat(), 8.0f, 1.0f);
        g.setColour (MixColours::textDim);
        g.setFont (MixLookAndFeel::uiFont (15.0f, false));
        g.drawText (currentMeter == 1 ? "Sound Field  -  coming next" : "Loudness  -  coming next",
                    meterArea, juce::Justification::centred);
    }

    // Footer divider bars (real vertical lines, centered in each spacer).
    g.setColour (MixColours::textDim);
    for (auto* s : { &footerSep1, &footerSep2, &footerSep3, &footerSep4 })
    {
        const auto b = s->getBounds().toFloat();
        const float cx = b.getCentreX();
        g.drawLine (cx, b.getY() + 2.0f, cx, b.getBottom() - 2.0f, 1.4f);
    }
}

//==============================================================================
void MixAnalyzerAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();

    menuButton.setBounds (10, 11, 30, 26);   // hamburger in the header
    bounds.removeFromTop (48);   // header (painted)

    // Footer links along the very bottom, right-aligned and tight.
    auto footerRow = bounds.removeFromBottom (30).reduced (16, 4);
    const juce::Font ff = MixLookAndFeel::uiFont (15.0f, false);
    const int fh = footerRow.getHeight();

    auto fitW = [&ff] (const juce::String& t) { return juce::GlyphArrangement::getStringWidthInt (ff, t) + 2; };
    coffeeLink.setSize   (fitW ("Support This Project"), fh);
    madeByLabel.setSize  (fitW ("Made by"),  fh);
    eerieLink.setSize    (fitW ("EERIE"),    fh);
    reportLink.setSize   (fitW ("Report Issue"),    fh);
    feedbackLink.setSize (fitW ("Feedback"),        fh);
    featureLink.setSize  (fitW ("Suggest Feature"), fh);
    for (auto* s : { &footerSep1, &footerSep2, &footerSep3, &footerSep4 })
        s->setSize (13, fh);   // spacer; a vertical bar is drawn here in paint()

    // { component, gap after it }. "Made by" -> "EERIE" is a tight 4px so it reads as one phrase.
    struct FI { juce::Component* c; int gapAfter; };
    FI seq[] = { { &coffeeLink, 12 }, { &footerSep1, 12 }, { &madeByLabel, 4 }, { &eerieLink, 12 },
                 { &footerSep2, 12 }, { &reportLink, 12 }, { &footerSep3, 12 }, { &feedbackLink, 12 },
                 { &footerSep4, 12 }, { &featureLink, 0 } };

    int total = 0;
    for (auto& e : seq) total += e.c->getWidth() + e.gapAfter;

    int fx = footerRow.getRight() - total;   // right-aligned
    for (auto& e : seq)
    {
        e.c->setBounds (fx, footerRow.getY(), e.c->getWidth(), fh);
        fx += e.c->getWidth() + e.gapAfter;
    }

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

    // Meter section: collapsible left sidebar (nav) + the meter area to its right.
    const int sbW = sidebarOpen ? 128 : 0;
    sidebarArea = bounds.removeFromLeft (sbW);
    if (sidebarOpen)
    {
        auto sb = sidebarArea.reduced (10, 6);
        navSpectrum.setBounds   (sb.removeFromTop (30)); sb.removeFromTop (4);
        navSoundField.setBounds (sb.removeFromTop (30)); sb.removeFromTop (4);
        navLoudness.setBounds   (sb.removeFromTop (30));
    }

    // Tab strip: Live / History on the left, Settings (live only) on the right.
    auto tabRow = bounds.removeFromTop (28).reduced (16, 0);
    liveTabButton.setBounds    (tabRow.removeFromLeft (56).withSizeKeepingCentre (56, 22));
    tabRow.removeFromLeft (4);
    historyTabButton.setBounds (tabRow.removeFromLeft (70).withSizeKeepingCentre (70, 22));
    settingsButton.setBounds       (tabRow.removeFromRight (82).withSizeKeepingCentre (82, 22));
    tabRow.removeFromRight (6);
    spectrumExportButton.setBounds (tabRow.removeFromRight (74).withSizeKeepingCentre (74, 22));
    tabRow.removeFromRight (5);
    screenshotButton.setBounds     (tabRow.removeFromRight (88).withSizeKeepingCentre (88, 22));
    tabRow.removeFromRight (6);
    recordButton.setBounds         (tabRow.removeFromRight (84).withSizeKeepingCentre (84, 22));
    tabRow.removeFromRight (4);
    recordIntervalBox.setBounds    (tabRow.removeFromRight (84).withSizeKeepingCentre (84, 22));
    spectrumCaptionBounds = {};   // tabs replace the caption

    auto viewArea = bounds.reduced (16, 2).withTrimmedBottom (12);
    spectrum.setBounds (viewArea);
    history.setBounds (viewArea);
    meterArea = viewArea;   // used to draw the placeholder for Sound Field / Loudness
}
