#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "SpectrumSettingsPanel.h"
#include "LoudnessSettingsPanel.h"
#include "SoundFieldSettingsPanel.h"
#include "MeterLayout.h"
#include "LayoutPicker.h"
#include <cmath>
#include <vector>

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
    addChildComponent (history);     // hidden until History tab is chosen
    addChildComponent (loudness);    // hidden until the Loudness meter is chosen
    addChildComponent (soundField);  // hidden until the Sound Field meter is chosen

    fileLabel.setText ("No song loaded", juce::dontSendNotification);
    fileLabel.setFont (MixLookAndFeel::monoFont (12.5f));
    fileLabel.setColour (juce::Label::textColourId, MixColours::textDim);
    fileLabel.setJustificationType (juce::Justification::centredLeft);

    loadButton.setColour (juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    loadButton.onClick = [this] { openFileChooser(); };

    restartButton.onClick   = [this] { processorRef.restart();        spectrum.resetAveraging(); loudness.clearHistory(); soundField.clearHistory(); updateTransportUI(); };
    playPauseButton.onClick = [this] { processorRef.togglePlayback(); updateTransportUI(); };
    stopButton.onClick      = [this] { processorRef.stopAndReset();   spectrum.resetAveraging(); loudness.clearHistory(); soundField.clearHistory(); updateTransportUI(); };

    restartButton.setTooltip ("Start from the beginning");
    playPauseButton.setTooltip ("Play / Pause");
    stopButton.setTooltip ("Stop and reset");

    loopButton.setColour (juce::ToggleButton::textColourId, MixColours::textDim);
    loopButton.setToggleState (true, juce::dontSendNotification);
    processorRef.setLoopEnabled (true);
    loopButton.onClick = [this] { processorRef.setLoopEnabled (loopButton.getToggleState()); };

    // Waveform show / hide as a radio pair (Yes / No).
    addAndMakeVisible (waveformLabel);
    waveformLabel.setText ("Waveform", juce::dontSendNotification);
    waveformLabel.setFont (MixLookAndFeel::uiFont (12.5f));
    waveformLabel.setColour (juce::Label::textColourId, MixColours::textDim);
    waveformLabel.setJustificationType (juce::Justification::centredLeft);
    waveformLabel.setBorderSize (juce::BorderSize<int> (0));

    for (auto* r : { &waveformYes, &waveformNo })
    {
        addAndMakeVisible (*r);
        r->setColour (juce::ToggleButton::textColourId, MixColours::textDim);
        r->setRadioGroupId (1);
        r->setClickingTogglesState (true);
    }
    waveformYes.setToggleState (true, juce::dontSendNotification);
    waveformYes.setTooltip ("Show the waveform.");
    waveformNo.setTooltip ("Hide the waveform to give the meters more room.");
    waveformYes.onClick = [this] { if (waveformYes.getToggleState()) setWaveformVisible (true);  };
    waveformNo.onClick  = [this] { if (waveformNo.getToggleState())  setWaveformVisible (false); };

    settingsButton.setColour (juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    settingsButton.onClick = [this] { showSpectrumSettings(); };

    addChildComponent (loudnessSettingsButton);   // shown only for the Loudness meter
    loudnessSettingsButton.setColour (juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    loudnessSettingsButton.onClick = [this] { showLoudnessSettings(); };

    // Loudness: Snapshot (one moment, txt) ----------------------------------
    addChildComponent (loudnessSnapshotButton);
    loudnessSnapshotButton.setColour (juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    loudnessSnapshotButton.setTooltip ("Save the loudness readings exactly as they are right now - one single moment.");
    loudnessSnapshotButton.onClick = [this]
    {
        chooser = std::make_unique<juce::FileChooser> ("Export loudness snapshot", juce::File{}, "*.txt");
        chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
                              [this] (const juce::FileChooser& fc)
        {
            const auto f = fc.getResult();
            if (f != juce::File{})
                f.replaceWithText (loudness.getSnapshotText());
        });
    };

    // Loudness: record interval --------------------------------------------
    addChildComponent (loudnessIntervalBox);
    loudnessIntervalBox.addItem ("Every 1s",  1);
    loudnessIntervalBox.addItem ("Every 2s",  2);
    loudnessIntervalBox.addItem ("Every 5s",  3);
    loudnessIntervalBox.addItem ("Every 10s", 4);
    loudnessIntervalBox.addItem ("Every 30s", 5);
    loudnessIntervalBox.setSelectedId (1, juce::dontSendNotification);
    loudnessIntervalBox.setTooltip ("How often Record saves the loudness readings while it runs.");
    loudnessIntervalBox.onChange = [this]
    {
        const int secs[] = { 1, 2, 5, 10, 30 };
        loudness.setRecordIntervalSeconds (secs[juce::jlimit (0, 4, loudnessIntervalBox.getSelectedId() - 1)]);
    };

    // Loudness: Record over time (csv) -------------------------------------
    addChildComponent (loudnessRecordButton);
    setRecordVisual (loudnessRecordButton, false);
    loudnessRecordButton.setTooltip ("Record the loudness readings over TIME at the chosen interval, then write them all to one CSV file.");
    loudnessRecordButton.onClick = [this]
    {
        if (loudness.isRecording())
        {
            loudness.stopRecording();
            setRecordVisual (loudnessRecordButton, false);
            loudnessIntervalBox.setEnabled (true);

            chooser = std::make_unique<juce::FileChooser> ("Save recorded loudness", juce::File{}, "*.csv");
            chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
                                  [this] (const juce::FileChooser& fc)
            {
                const auto f = fc.getResult();
                if (f != juce::File{})
                    f.replaceWithText (loudness.getRecordingText());
            });
        }
        else
        {
            loudness.startRecording();
            setRecordVisual (loudnessRecordButton, true);
            loudnessIntervalBox.setEnabled (false);
        }
    };

    // Loudness: Screenshot (png) -------------------------------------------
    addChildComponent (loudnessScreenshotButton);
    loudnessScreenshotButton.setColour (juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    loudnessScreenshotButton.setTooltip ("Save a PNG picture of the loudness meter as it looks now.");
    loudnessScreenshotButton.onClick = [this]
    {
        const auto img = loudness.createComponentSnapshot (loudness.getLocalBounds());
        chooser = std::make_unique<juce::FileChooser> ("Save loudness image", juce::File{}, "*.png");
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

    // ---- Sound Field tools (mirror the loudness set) -----------------------
    addChildComponent (soundFieldSettingsButton);
    soundFieldSettingsButton.setColour (juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    soundFieldSettingsButton.onClick = [this] { showSoundFieldSettings(); };

    addChildComponent (soundFieldSnapshotButton);
    soundFieldSnapshotButton.setColour (juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    soundFieldSnapshotButton.setTooltip ("Save the sound field readings exactly as they are right now.");
    soundFieldSnapshotButton.onClick = [this]
    {
        chooser = std::make_unique<juce::FileChooser> ("Export sound field snapshot", juce::File{}, "*.txt");
        chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
                              [this] (const juce::FileChooser& fc)
        {
            const auto f = fc.getResult();
            if (f != juce::File{})
                f.replaceWithText (soundField.getSnapshotText());
        });
    };

    addChildComponent (soundFieldIntervalBox);
    soundFieldIntervalBox.addItem ("Every 1s",  1);
    soundFieldIntervalBox.addItem ("Every 2s",  2);
    soundFieldIntervalBox.addItem ("Every 5s",  3);
    soundFieldIntervalBox.addItem ("Every 10s", 4);
    soundFieldIntervalBox.addItem ("Every 30s", 5);
    soundFieldIntervalBox.setSelectedId (1, juce::dontSendNotification);
    soundFieldIntervalBox.setTooltip ("How often Record saves the sound field readings while it runs.");
    soundFieldIntervalBox.onChange = [this]
    {
        const int secs[] = { 1, 2, 5, 10, 30 };
        soundField.setRecordIntervalSeconds (secs[juce::jlimit (0, 4, soundFieldIntervalBox.getSelectedId() - 1)]);
    };

    addChildComponent (soundFieldRecordButton);
    setRecordVisual (soundFieldRecordButton, false);
    soundFieldRecordButton.setTooltip ("Record the sound field readings over TIME, then write them all to one CSV file.");
    soundFieldRecordButton.onClick = [this]
    {
        if (soundField.isRecording())
        {
            soundField.stopRecording();
            setRecordVisual (soundFieldRecordButton, false);
            soundFieldIntervalBox.setEnabled (true);

            chooser = std::make_unique<juce::FileChooser> ("Save recorded sound field", juce::File{}, "*.csv");
            chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
                                  [this] (const juce::FileChooser& fc)
            {
                const auto f = fc.getResult();
                if (f != juce::File{})
                    f.replaceWithText (soundField.getRecordingText());
            });
        }
        else
        {
            soundField.startRecording();
            setRecordVisual (soundFieldRecordButton, true);
            soundFieldIntervalBox.setEnabled (false);
        }
    };

    addChildComponent (soundFieldScreenshotButton);
    soundFieldScreenshotButton.setColour (juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    soundFieldScreenshotButton.setTooltip ("Save a PNG picture of the sound field meter as it looks now.");
    soundFieldScreenshotButton.onClick = [this]
    {
        const auto img = soundField.createComponentSnapshot (soundField.getLocalBounds());
        chooser = std::make_unique<juce::FileChooser> ("Save sound field image", juce::File{}, "*.png");
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
    setRecordVisual (recordButton, false);
    recordButton.setTooltip ("Record the spectrum over TIME - it keeps saving snapshots at the chosen interval until you stop, then writes them all to one file.");
    recordButton.onClick = [this]
    {
        if (spectrum.isRecording())
        {
            spectrum.stopRecording();
            setRecordVisual (recordButton, false);
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
            setRecordVisual (recordButton, true);
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

    // Layout picker (top-left) - a subtle pill so it reads as a button.
    addAndMakeVisible (layoutButton);
    layoutButton.setColour (juce::TextButton::buttonColourId, MixColours::surface2);
    layoutButton.setTooltip ("Arrange the meter panels. Turn on 2 or more meters (left menu) to split the view.");
    layoutButton.onClick = [this] { showLayoutPicker(); };

    // Per-meter "more" menus (shown when a tile is too narrow for the full toolbar).
    addChildComponent (spectrumMoreButton);
    addChildComponent (soundFieldMoreButton);
    addChildComponent (loudnessMoreButton);
    spectrumMoreButton.setColour   (juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    soundFieldMoreButton.setColour (juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    loudnessMoreButton.setColour   (juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    spectrumMoreButton.onClick   = [this] { showMeterMenu (0); };
    soundFieldMoreButton.onClick = [this] { showMeterMenu (1); };
    loudnessMoreButton.onClick   = [this] { showMeterMenu (2); };

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
    navSpectrum.onClick   = [this] { toggleMeter (0); };
    navSoundField.onClick = [this] { toggleMeter (1); };
    navLoudness.onClick   = [this] { toggleMeter (2); };

    setTransportEnabled (false);
    showHistory (false);       // spectrum Live view + highlight the Live tab
    updateNavHighlight();

    waveform.onSelectionChanged = [this] (double s, double e) { processorRef.setSelection (s, e); };
    waveform.onSelectionCleared = [this] { processorRef.clearSelection(); };

    // Resizable so the standalone window gets a maximize button and can be
    // dragged bigger; the layout stretches to fill.
    setResizable (true, false);
    setResizeLimits (700, 520, 4000, 3000);
    setSize (820, 580);
    startTimerHz (60);

    // Keep keyboard focus on the editor so the spacebar transport shortcuts work
    // no matter which control was last clicked. (Text entry lives in pop-over
    // windows, which have their own focus, so typing spaces there still works.)
    for (auto* ch : getChildren())
        ch->setWantsKeyboardFocus (false);
    setWantsKeyboardFocus (true);
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

    if (isShowing())
        grabKeyboardFocus();   // so the spacebar shortcuts work right away
}

bool MixAnalyzerAudioProcessorEditor::keyPressed (const juce::KeyPress& key)
{
    if (key.getKeyCode() == juce::KeyPress::spaceKey)
    {
        if (! processorRef.hasFileLoaded())
            return true;

        if (key.getModifiers().isCommandDown() || key.getModifiers().isCtrlDown())
        {
            // Ctrl + Space: start from the beginning.
            processorRef.restart();
            spectrum.resetAveraging();
            loudness.clearHistory();
            soundField.clearHistory();
        }
        else
        {
            processorRef.togglePlayback();   // Space: play / pause
        }

        updateTransportUI();
        return true;
    }

    return false;
}

void MixAnalyzerAudioProcessorEditor::setTransportEnabled (bool enabled)
{
    restartButton.setEnabled (enabled);
    playPauseButton.setEnabled (enabled);
    stopButton.setEnabled (enabled);
}

void MixAnalyzerAudioProcessorEditor::setRecordVisual (juce::TextButton& b, bool recording)
{
    // A record control: red with a filled dot when idle, brighter red "Stop" while
    // running. The "recordBtn" flag makes the look-and-feel give it a larger font.
    b.getProperties().set ("recordBtn", true);
    b.setButtonText (recording ? juce::String::fromUTF8 ("\xE2\x96\xA0 Stop")
                               : juce::String::fromUTF8 ("\xE2\x97\x8F Record"));
    b.setColour (juce::TextButton::buttonColourId,
                 recording ? juce::Colour (0xffef4444) : juce::Colour (0xffb91c1c));
    b.repaint();
}

void MixAnalyzerAudioProcessorEditor::setWaveformVisible (bool shouldShow)
{
    if (waveformVisible == shouldShow)
        return;

    waveformVisible = shouldShow;
    waveform.setVisible (shouldShow);
    resized();
    repaint();
}

//==============================================================================
void MixAnalyzerAudioProcessorEditor::timerCallback()
{
    // Animate the sidebar width (smooth slide in/out).
    if (std::abs (sidebarW - sidebarTargetW) > 0.5f)
    {
        sidebarW += (sidebarTargetW - sidebarW) * 0.25f;
        if (std::abs (sidebarW - sidebarTargetW) <= 0.5f)
            sidebarW = sidebarTargetW;
        resized();
        repaint();
    }

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

void MixAnalyzerAudioProcessorEditor::showLoudnessSettings()
{
    auto panel = std::make_unique<LoudnessSettingsPanel> (loudness);
    panel->setLookAndFeel (&lookAndFeel);
    juce::CallOutBox::launchAsynchronously (std::move (panel),
                                            loudnessSettingsButton.getScreenBounds(),
                                            nullptr);
}

void MixAnalyzerAudioProcessorEditor::showSoundFieldSettings()
{
    auto panel = std::make_unique<SoundFieldSettingsPanel> (soundField);
    panel->setLookAndFeel (&lookAndFeel);
    juce::CallOutBox::launchAsynchronously (std::move (panel),
                                            soundFieldSettingsButton.getScreenBounds(),
                                            nullptr);
}

void MixAnalyzerAudioProcessorEditor::showLayoutPicker()
{
    int n = 0;
    for (bool b : meterOn) if (b) ++n;
    n = juce::jmax (1, n);

    auto picker = std::make_unique<LayoutPicker> (n, layoutIndex, [this] (int idx)
    {
        layoutIndex = idx;
        resized();
        repaint();
    });
    picker->setLookAndFeel (&lookAndFeel);
    juce::CallOutBox::launchAsynchronously (std::move (picker),
                                            layoutButton.getScreenBounds(), nullptr);
}

void MixAnalyzerAudioProcessorEditor::showMeterMenu (int meter)
{
    juce::TextButton* settings   = &settingsButton;
    juce::TextButton* snapshot   = &spectrumExportButton;
    juce::TextButton* screenshot = &screenshotButton;
    juce::TextButton* record     = &recordButton;
    juce::ComboBox*   interval   = &recordIntervalBox;
    juce::TextButton* more       = &spectrumMoreButton;

    if (meter == 1)
    {
        settings = &soundFieldSettingsButton; snapshot = &soundFieldSnapshotButton;
        screenshot = &soundFieldScreenshotButton; record = &soundFieldRecordButton;
        interval = &soundFieldIntervalBox; more = &soundFieldMoreButton;
    }
    else if (meter == 2)
    {
        settings = &loudnessSettingsButton; snapshot = &loudnessSnapshotButton;
        screenshot = &loudnessScreenshotButton; record = &loudnessRecordButton;
        interval = &loudnessIntervalBox; more = &loudnessMoreButton;
    }

    auto fire = [] (juce::TextButton* b) { if (b->onClick) b->onClick(); };

    juce::PopupMenu m;
    m.addItem ("Settings...", [settings, fire] { fire (settings); });
    m.addItem ("Snapshot",    [snapshot, fire] { fire (snapshot); });
    m.addItem ("Screenshot",  [screenshot, fire] { fire (screenshot); });
    m.addItem ("Record / Stop & save", [record, fire] { fire (record); });

    juce::PopupMenu iv;
    const char* labels[] = { "Every 1s", "Every 2s", "Every 5s", "Every 10s", "Every 30s" };
    for (int i = 0; i < 5; ++i)
    {
        const int id = i + 1;
        iv.addItem (labels[i], true, interval->getSelectedId() == id,
                    [interval, id] { interval->setSelectedId (id, juce::sendNotification); });
    }
    m.addSubMenu ("Record interval", iv);

    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (more));
}

void MixAnalyzerAudioProcessorEditor::showHistory (bool showHist)
{
    historyMode = showHist;

    // The active tab is a filled indigo button; the inactive one is a ghost.
    liveTabButton.setColour    (juce::TextButton::buttonColourId,
                                showHist ? juce::Colours::transparentBlack : MixColours::accent);
    historyTabButton.setColour (juce::TextButton::buttonColourId,
                                showHist ? MixColours::accent : juce::Colours::transparentBlack);
    liveTabButton.repaint();
    historyTabButton.repaint();

    updateMeterVisibility();
    resized();
    repaint();
}

void MixAnalyzerAudioProcessorEditor::toggleMeter (int meter)
{
    int onCount = 0;
    for (bool b : meterOn) if (b) ++onCount;

    // Keep at least one meter visible - can't switch off the last one.
    if (meterOn[meter] && onCount == 1)
        return;

    meterOn[meter] = ! meterOn[meter];

    updateMeterVisibility();
    updateNavHighlight();
    resized();
    repaint();
}

void MixAnalyzerAudioProcessorEditor::updateMeterVisibility()
{
    // Displays + spectrum tabs follow which meters are on. The per-meter tool
    // buttons are shown and positioned by layoutMeterTile (full row or compact),
    // so hide them all here as a baseline (covers off meters + history mode).
    spectrum.setVisible   (meterOn[0] && ! historyMode);
    history.setVisible    (meterOn[0] &&   historyMode);
    soundField.setVisible (meterOn[1]);
    loudness.setVisible   (meterOn[2]);

    liveTabButton.setVisible    (meterOn[0]);
    historyTabButton.setVisible (meterOn[0]);

    for (auto* b : { &settingsButton, &spectrumExportButton, &screenshotButton, &recordButton,
                     &soundFieldSettingsButton, &soundFieldSnapshotButton, &soundFieldScreenshotButton, &soundFieldRecordButton,
                     &loudnessSettingsButton, &loudnessSnapshotButton, &loudnessScreenshotButton, &loudnessRecordButton,
                     &spectrumMoreButton, &soundFieldMoreButton, &loudnessMoreButton })
        b->setVisible (false);

    recordIntervalBox.setVisible (false);
    soundFieldIntervalBox.setVisible (false);
    loudnessIntervalBox.setVisible (false);

    // The layout picker only does something with 2+ panels; grey it out otherwise.
    int onCount = 0;
    for (bool b : meterOn) if (b) ++onCount;
    layoutButton.setEnabled (onCount >= 2);
}

void MixAnalyzerAudioProcessorEditor::toggleSidebar()
{
    sidebarOpen = ! sidebarOpen;
    sidebarTargetW = sidebarOpen ? 128.0f : 0.0f;   // collapse fully; the toggle lives in the first panel
}

void MixAnalyzerAudioProcessorEditor::updateNavHighlight()
{
    auto setNav = [] (juce::TextButton& b, bool active)
    {
        b.setColour (juce::TextButton::buttonColourId,
                     active ? MixColours::accent : juce::Colours::transparentBlack);
        b.repaint();
    };
    setNav (navSpectrum,   meterOn[0]);
    setNav (navSoundField, meterOn[1]);
    setNav (navLoudness,   meterOn[2]);
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
            spectrum.resetAveraging();   // fresh meters for the new song
            loudness.clearHistory();
            soundField.clearHistory();
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

    // Header: app name in indigo with a soft glow (shifted right for the layout button).
    auto header = getLocalBounds().removeFromTop (34).reduced (16, 0).withTrimmedLeft (84);

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
    drawCaption (soundFieldCaptionBounds, "Sound Field");
    drawCaption (loudnessCaptionBounds, "Loudness");

    // Sidebar background + right divider.
    if (sidebarArea.getWidth() > 2)
    {
        g.setColour (MixColours::surface);
        g.fillRect (sidebarArea);
        g.setColour (MixColours::border);
        g.drawVerticalLine (sidebarArea.getRight() - 1, (float) sidebarArea.getY(), (float) sidebarArea.getBottom());
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

    layoutButton.setBounds (12, 5, 78, 24);   // top-left of the header
    bounds.removeFromTop (34);   // header (painted)

    // Footer links along the very bottom, right-aligned and tight.
    auto footerRow = bounds.removeFromBottom (22).reduced (16, 2);
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

    // Control strip: Load, transport icons, Loop, waveform toggle, file name.
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
    // Waveform Yes/No radios right next to the song name (its caption, then the
    // radios, then the file name).
    waveformLabel.setBounds (controls.removeFromLeft (68));
    waveformYes.setBounds   (controls.removeFromLeft (52));
    waveformNo.setBounds    (controls.removeFromLeft (50));
    controls.removeFromLeft (14);
    fileLabel.setBounds (controls);

    // Waveform: caption strip + panel, only when shown. When hidden, that space
    // (130px) stays in `bounds` and flows down to the meter section instead.
    if (waveformVisible)
    {
        waveCaptionBounds = bounds.removeFromTop (20).reduced (16, 0);
        waveform.setBounds (bounds.removeFromTop (110).reduced (16, 2));
    }
    else
    {
        waveCaptionBounds = {};
    }

    // Meter section: collapsible left sidebar (nav) + the meter area to its right.
    // The menu button is pinned at the top-left of the sidebar column (constant
    // spot, below the waveform); the column width animates and the nav slides.
    const int sbW = (int) sidebarW;
    sidebarArea = bounds.removeFromLeft (sbW);

    const bool navVisible = sbW > 96;
    for (auto* n : { &navSpectrum, &navSoundField, &navLoudness })
        n->setVisible (navVisible);
    if (navVisible)
    {
        auto sb = sidebarArea.reduced (10, 8);
        navSpectrum.setBounds   (sb.removeFromTop (30)); sb.removeFromTop (4);
        navSoundField.setBounds (sb.removeFromTop (30)); sb.removeFromTop (4);
        navLoudness.setBounds   (sb.removeFromTop (30));
    }

    // Meter tiles: one per selected meter, stacked vertically. Each tile has a
    // compact header (that meter's own tools) and its display below.
    spectrumCaptionBounds   = {};
    soundFieldCaptionBounds = {};
    loudnessCaptionBounds   = {};

    std::vector<int> on;
    for (int m = 0; m < 3; ++m) if (meterOn[m]) on.push_back (m);
    if (on.empty()) { meterOn[0] = true; on.push_back (0); }   // safety

    const int n = (int) on.size();
    if (layoutIndex < 0 || layoutIndex >= MeterLayout::count (n)) layoutIndex = 0;

    const auto rects = MeterLayout::tiles (bounds, n, layoutIndex);
    for (int i = 0; i < n; ++i)
        layoutMeterTile (on[i], rects[i], i == 0);   // first tile carries the sidebar toggle
}

void MixAnalyzerAudioProcessorEditor::layoutMeterTile (int meter, juce::Rectangle<int> tile, bool isFirst)
{
    const bool compact = tile.getWidth() < 520;   // too narrow for the full toolbar
    auto hdr  = tile.removeFromTop (28).reduced (16, 0);
    auto body = tile.reduced (16, 2).withTrimmedBottom (4);

    // The sidebar collapse/expand toggle lives at the top-left of the first panel,
    // so collapsing the sidebar frees all of its width instead of leaving a strip.
    if (isFirst)
    {
        menuButton.setBounds (hdr.removeFromLeft (28).withSizeKeepingCentre (28, 24));
        hdr.removeFromLeft (6);
    }

    // Lay out the right-side tools: the full button row when wide, or just the
    // Record button + a "more" menu when the tile is narrow.
    auto layoutTools = [&hdr, compact] (juce::TextButton& settings, juce::TextButton& snapshot,
                                        juce::TextButton& screenshot, juce::TextButton& record,
                                        juce::ComboBox& interval, juce::TextButton& more)
    {
        if (compact)
        {
            settings.setVisible (false); snapshot.setVisible (false);
            screenshot.setVisible (false); interval.setVisible (false);
            more.setVisible (true);
            more.setBounds (hdr.removeFromRight (30).withSizeKeepingCentre (30, 22));
            hdr.removeFromRight (4);
            record.setVisible (true);
            record.setBounds (hdr.removeFromRight (88).withSizeKeepingCentre (88, 22));
        }
        else
        {
            more.setVisible (false);
            settings.setVisible (true); snapshot.setVisible (true); screenshot.setVisible (true);
            record.setVisible (true); interval.setVisible (true);
            settings.setBounds   (hdr.removeFromRight (82).withSizeKeepingCentre (82, 22));
            hdr.removeFromRight (6);
            snapshot.setBounds   (hdr.removeFromRight (74).withSizeKeepingCentre (74, 22));
            hdr.removeFromRight (5);
            screenshot.setBounds (hdr.removeFromRight (88).withSizeKeepingCentre (88, 22));
            hdr.removeFromRight (6);
            record.setBounds     (hdr.removeFromRight (86).withSizeKeepingCentre (86, 22));
            hdr.removeFromRight (2);
            interval.setBounds   (hdr.removeFromRight (84).withSizeKeepingCentre (84, 22));
        }
    };

    if (meter == 0)   // Spectrum: Live/History tabs (left) + tools (right)
    {
        liveTabButton.setBounds    (hdr.removeFromLeft (56).withSizeKeepingCentre (56, 22));
        hdr.removeFromLeft (4);
        historyTabButton.setBounds (hdr.removeFromLeft (70).withSizeKeepingCentre (70, 22));

        if (! historyMode)
        {
            layoutTools (settingsButton, spectrumExportButton, screenshotButton,
                         recordButton, recordIntervalBox, spectrumMoreButton);
        }
        else   // History has its own inline controls - no header tools.
        {
            settingsButton.setVisible (false); spectrumExportButton.setVisible (false);
            screenshotButton.setVisible (false); recordButton.setVisible (false);
            recordIntervalBox.setVisible (false); spectrumMoreButton.setVisible (false);
        }

        spectrum.setBounds (body);
        history.setBounds (body);
    }
    else if (meter == 1)   // Sound Field
    {
        layoutTools (soundFieldSettingsButton, soundFieldSnapshotButton, soundFieldScreenshotButton,
                     soundFieldRecordButton, soundFieldIntervalBox, soundFieldMoreButton);
        soundFieldCaptionBounds = hdr;   // name painted in the leftover left area
        soundField.setBounds (body);
    }
    else   // meter == 2, Loudness
    {
        layoutTools (loudnessSettingsButton, loudnessSnapshotButton, loudnessScreenshotButton,
                     loudnessRecordButton, loudnessIntervalBox, loudnessMoreButton);
        loudnessCaptionBounds = hdr;
        loudness.setBounds (body);
    }
}
