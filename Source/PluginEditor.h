#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "WaveformDisplay.h"
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

private:
    void timerCallback() override;
    void computeSpectrumFrame();
    void paintSpectrum (juce::Graphics&, juce::Rectangle<float> area);
    void openFileChooser();
    void updateTransportUI();

    MixAnalyzerAudioProcessor& processorRef;

    // Spectrum drawing state (runs on the GUI thread).
    static constexpr int fftSize   = MixAnalyzerAudioProcessor::fftSize;
    static constexpr int scopeSize = 512;

    juce::dsp::FFT forwardFFT;
    juce::dsp::WindowingFunction<float> window;

    float fftInput[2 * fftSize] = {};   // scratch: raw samples in, magnitudes out
    float scopeData[scopeSize]  = {};

    // Frequency range currently drawn (set each frame, used for axis labels).
    float specFMin = 20.0f;
    float specFMax = 20000.0f;

    // File playback controls (used mainly by the standalone).
    juce::TextButton loadButton { "Load song..." };
    TransportButton restartButton  { "Restart",    TransportButton::Icon::restart };
    TransportButton playPauseButton { "Play/Pause", TransportButton::Icon::play };
    TransportButton stopButton     { "Stop",       TransportButton::Icon::stop };
    juce::ToggleButton loopButton  { "Loop" };
    juce::Label fileLabel;
    std::unique_ptr<juce::FileChooser> chooser;

    void setTransportEnabled (bool enabled);

    WaveformDisplay waveform;

    MixLookAndFeel lookAndFeel;
    juce::TooltipWindow tooltipWindow { this };

    juce::Rectangle<int> spectrumBounds;
    juce::Rectangle<int> waveCaptionBounds;
    juce::Rectangle<int> spectrumCaptionBounds;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MixAnalyzerAudioProcessorEditor)
};
