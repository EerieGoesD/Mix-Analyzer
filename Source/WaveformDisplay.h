#pragma once

#include <JuceHeader.h>

//==============================================================================
// Shows the loaded song's waveform, lets the user click-drag a section, and
// draws a moving playhead. Selection is reported back in seconds via callbacks.
//==============================================================================
class WaveformDisplay : public juce::Component,
                        private juce::ChangeListener
{
public:
    WaveformDisplay();
    ~WaveformDisplay() override;

    void setFile (const juce::File& file);
    void clear();

    // 0..1 along the song; negative hides the playhead.
    void setPlayheadProportion (double proportion);

    std::function<void (double startSeconds, double endSeconds)> onSelectionChanged;
    std::function<void()> onSelectionCleared;

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp   (const juce::MouseEvent&) override;

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    double xToTime (float x) const;
    float  timeToX (double timeSeconds) const;

    juce::AudioFormatManager formatManager;
    juce::AudioThumbnailCache thumbnailCache { 4 };
    juce::AudioThumbnail thumbnail { 512, formatManager, thumbnailCache };

    double lengthSeconds = 0.0;
    bool   hasSelection  = false;
    double selStart = 0.0;
    double selEnd   = 0.0;
    double dragAnchor = 0.0;
    double playhead = -1.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WaveformDisplay)
};
