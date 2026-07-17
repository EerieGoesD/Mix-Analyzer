#pragma once

#include <JuceHeader.h>
#include <vector>
#include "PluginProcessor.h"

//==============================================================================
// HISTORY / FREQUENCY ANALYZER (Audacity Plot Spectrum style). Static snapshot:
// reads the selected section of the loaded song, chops it into blocks of the
// chosen size, windows + FFTs each, and averages them into one frozen curve.
// Supports the Audacity algorithms, window functions, sizes, and log/lin axis.
//==============================================================================
class HistoryAnalyzer : public juce::Component
{
public:
    enum class Algo { spectrum, autocorr, cubeAutocorr, enhAutocorr, cepstrum };
    enum class Axis { logFreq, linFreq };

    explicit HistoryAnalyzer (MixAnalyzerAudioProcessor&);

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;

private:
    void analyzeSelection();
    void exportData();
    void fillWindow (std::vector<float>& w, int size, int type) const;   // 0..9 Audacity

    juce::Rectangle<float> plotBounds() const;
    float freqToX (float freq, juce::Rectangle<float> plot) const;
    float xToFreq (float x, juce::Rectangle<float> plot) const;

    MixAnalyzerAudioProcessor& processorRef;

    juce::ComboBox   algoBox, sizeBox, windowBox, axisBox;
    juce::TextButton analyzeButton { "Analyze" };
    juce::TextButton exportButton  { "Export" };
    juce::Label      statusLabel;

    Algo algo   = Algo::spectrum;
    int  fftSizeSel = 8192;
    int  windowType = 3;              // Hann (Audacity order)
    Axis axis   = Axis::logFreq;

    struct Pt { float freq, level, db; };   // level 0..1, db for readout/export
    std::vector<Pt> result;
    float resFMin = 20.0f, resFMax = 20000.0f;

    bool hovering = false;
    juce::Point<float> mousePos;

    std::unique_ptr<juce::FileChooser> chooser;
    juce::AudioFormatManager formatManager;

    static constexpr float mindB = -100.0f;
    static constexpr float maxdB = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HistoryAnalyzer)
};
