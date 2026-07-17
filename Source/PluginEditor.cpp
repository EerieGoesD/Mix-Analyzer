#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>

//==============================================================================
MixAnalyzerAudioProcessorEditor::MixAnalyzerAudioProcessorEditor (MixAnalyzerAudioProcessor& p)
    : AudioProcessorEditor (&p),
      processorRef (p),
      forwardFFT (MixAnalyzerAudioProcessor::fftOrder),
      window (MixAnalyzerAudioProcessor::fftSize,
              juce::dsp::WindowingFunction<float>::hann)
{
    setLookAndFeel (&lookAndFeel);

    addAndMakeVisible (loadButton);
    addAndMakeVisible (restartButton);
    addAndMakeVisible (playPauseButton);
    addAndMakeVisible (stopButton);
    addAndMakeVisible (loopButton);
    addAndMakeVisible (fileLabel);
    addAndMakeVisible (waveform);

    fileLabel.setText ("No song loaded", juce::dontSendNotification);
    fileLabel.setFont (MixLookAndFeel::monoFont (12.5f));
    fileLabel.setColour (juce::Label::textColourId, MixColours::textDim);
    fileLabel.setJustificationType (juce::Justification::centredLeft);

    // Load = ghost (outline) button.
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

    setTransportEnabled (false);

    waveform.onSelectionChanged = [this] (double s, double e) { processorRef.setSelection (s, e); };
    waveform.onSelectionCleared = [this] { processorRef.clearSelection(); };

    setSize (820, 560);
    startTimerHz (60);
}

void MixAnalyzerAudioProcessorEditor::setTransportEnabled (bool enabled)
{
    restartButton.setEnabled (enabled);
    playPauseButton.setEnabled (enabled);
    stopButton.setEnabled (enabled);
}

MixAnalyzerAudioProcessorEditor::~MixAnalyzerAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

//==============================================================================
void MixAnalyzerAudioProcessorEditor::timerCallback()
{
    // The audio thread asks us to stop when a non-looping section reaches its end.
    if (processorRef.consumeStopRequest())
        processorRef.stopAndReset();

    computeSpectrumFrame();
    repaint (spectrumBounds);

    // Move the playhead on the waveform.
    const double len = processorRef.getLengthSeconds();
    if (len > 0.0)
        waveform.setPlayheadProportion (processorRef.getCurrentPositionSeconds() / len);
    else
        waveform.setPlayheadProportion (-1.0);

    updateTransportUI();
}

void MixAnalyzerAudioProcessorEditor::computeSpectrumFrame()
{
    // Grab the newest fftSize samples and zero the FFT scratch's upper half.
    processorRef.copyLatestSamples (fftInput);
    juce::FloatVectorOperations::clear (fftInput + fftSize, fftSize);

    window.multiplyWithWindowingTable (fftInput, (size_t) fftSize);
    forwardFFT.performFrequencyOnlyForwardTransform (fftInput);

    const double sr = processorRef.getSampleRate() > 0.0 ? processorRef.getSampleRate() : 44100.0;
    specFMin = 20.0f;
    specFMax = juce::jmin (20000.0f, (float) (sr * 0.5));

    const float logRange = std::log (specFMax / specFMin);
    const float binHz    = (float) (sr / (double) fftSize);

    constexpr float mindB = -100.0f;
    constexpr float maxdB = 0.0f;

    for (int i = 0; i < scopeSize; ++i)
    {
        // True log-frequency x axis (matches Audacity's "Log frequency").
        const float frac = (float) i / (float) (scopeSize - 1);
        const float freq = specFMin * std::exp (logRange * frac);
        const int   bin  = juce::jlimit (0, fftSize / 2, (int) std::round (freq / binHz));

        const float db = juce::Decibels::gainToDecibels (fftInput[bin])
                             - juce::Decibels::gainToDecibels ((float) fftSize);

        const float level = juce::jmap (juce::jlimit (mindB, maxdB, db), mindB, maxdB, 0.0f, 1.0f);

        // Fast attack, slow release so it glides instead of stuttering.
        const float prev = scopeData[i];
        scopeData[i] = level > prev ? level : prev * 0.82f + level * 0.18f;
    }
}

void MixAnalyzerAudioProcessorEditor::updateTransportUI()
{
    playPauseButton.setIcon (processorRef.isFilePlaying() ? TransportButton::Icon::pause
                                                          : TransportButton::Icon::play);
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

    // Header strip: app name in indigo with a soft glow.
    auto header = getLocalBounds().removeFromTop (48).reduced (16, 0);

    g.setColour (MixColours::accent.withAlpha (0.35f));
    g.setFont (MixLookAndFeel::uiFont (19.0f, true));
    for (auto d : { juce::Point<int> (0, 1), juce::Point<int> (1, 0) })
        g.drawText ("EERIE - Mix Analyzer", header.translated (d.x, d.y),
                    juce::Justification::centredLeft);

    g.setColour (MixColours::accentH);
    g.drawText ("EERIE - Mix Analyzer", header, juce::Justification::centredLeft);

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

    if (! spectrumBounds.isEmpty())
        paintSpectrum (g, spectrumBounds.toFloat());
}

void MixAnalyzerAudioProcessorEditor::paintSpectrum (juce::Graphics& g, juce::Rectangle<float> area)
{
    g.setColour (MixColours::surface);
    g.fillRoundedRectangle (area, 8.0f);
    g.setColour (MixColours::border);
    g.drawRoundedRectangle (area, 8.0f, 1.0f);

    // Reserve gutters for the axis labels; the curve lives in "plot".
    const float leftGutter   = 34.0f;   // dB numbers
    const float bottomGutter = 16.0f;   // frequency numbers
    juce::Rectangle<float> plot (area.getX() + leftGutter, area.getY() + 6.0f,
                                 area.getWidth() - leftGutter - 8.0f,
                                 area.getHeight() - bottomGutter - 8.0f);

    g.setFont (MixLookAndFeel::monoFont (9.5f));

    // dB grid + labels (0 to -100).
    for (int db = 0; db >= -100; db -= 20)
    {
        const float y = juce::jmap ((float) db, 0.0f, -100.0f, plot.getY(), plot.getBottom());
        g.setColour (MixColours::border.withAlpha (0.6f));
        g.drawHorizontalLine ((int) y, plot.getX(), plot.getRight());
        g.setColour (MixColours::textDim);
        g.drawText (juce::String (db),
                    juce::Rectangle<float> (area.getX() + 2.0f, y - 6.0f, leftGutter - 7.0f, 12.0f),
                    juce::Justification::centredRight);
    }

    // Frequency grid + labels (log spaced).
    const float logRange = std::log (specFMax / specFMin);
    const int   freqs[]  = { 20, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000 };

    for (int f : freqs)
    {
        if ((float) f < specFMin || (float) f > specFMax)
            continue;

        const float frac = std::log ((float) f / specFMin) / logRange;
        const float x = plot.getX() + frac * plot.getWidth();

        g.setColour (MixColours::border.withAlpha (0.5f));
        g.drawVerticalLine ((int) x, plot.getY(), plot.getBottom());
        g.setColour (MixColours::textDim);
        const juce::String lbl = f >= 1000 ? juce::String (f / 1000) + "k" : juce::String (f);
        g.drawText (lbl,
                    juce::Rectangle<float> (x - 16.0f, plot.getBottom() + 2.0f, 32.0f, 12.0f),
                    juce::Justification::centred);
    }

    // Filled spectrum curve.
    juce::Path curve;
    curve.startNewSubPath (plot.getX(), plot.getBottom());

    for (int i = 0; i < scopeSize; ++i)
    {
        const float x = juce::jmap ((float) i, 0.0f, (float) (scopeSize - 1),
                                    plot.getX(), plot.getRight());
        const float y = juce::jmap (scopeData[i], 0.0f, 1.0f, plot.getBottom(), plot.getY());
        curve.lineTo (x, y);
    }

    curve.lineTo (plot.getRight(), plot.getBottom());
    curve.closeSubPath();

    juce::ColourGradient grad (MixColours::accent.withAlpha (0.85f), plot.getX(), plot.getY(),
                               MixColours::accent.withAlpha (0.12f), plot.getX(), plot.getBottom(),
                               false);
    g.setGradientFill (grad);
    g.fillPath (curve);

    // Bright top line.
    juce::Path line;
    for (int i = 0; i < scopeSize; ++i)
    {
        const float x = juce::jmap ((float) i, 0.0f, (float) (scopeSize - 1),
                                    plot.getX(), plot.getRight());
        const float y = juce::jmap (scopeData[i], 0.0f, 1.0f, plot.getBottom(), plot.getY());
        if (i == 0) line.startNewSubPath (x, y);
        else        line.lineTo (x, y);
    }
    g.setColour (MixColours::accentH);
    g.strokePath (line, juce::PathStrokeType (1.5f));
}

//==============================================================================
void MixAnalyzerAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();

    bounds.removeFromTop (48);   // header (painted, no child components)

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

    // Spectrum: caption strip, then the rest.
    spectrumCaptionBounds = bounds.removeFromTop (20).reduced (16, 0);
    spectrumBounds = bounds.reduced (16, 2).withTrimmedBottom (12);
}
