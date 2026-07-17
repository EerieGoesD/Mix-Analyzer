#include "WaveformDisplay.h"
#include "MixLookAndFeel.h"
#include <cmath>

//==============================================================================
WaveformDisplay::WaveformDisplay()
{
    formatManager.registerBasicFormats();
    thumbnail.addChangeListener (this);
}

WaveformDisplay::~WaveformDisplay()
{
    thumbnail.removeChangeListener (this);
}

//==============================================================================
void WaveformDisplay::setFile (const juce::File& file)
{
    hasSelection = false;
    selStart = selEnd = 0.0;
    playhead = -1.0;

    // setSource loads asynchronously and sends change messages as it scans, so
    // the waveform appears almost immediately and fills in.
    thumbnail.setSource (new juce::FileInputSource (file));
    lengthSeconds = thumbnail.getTotalLength();
    repaint();
}

void WaveformDisplay::clear()
{
    thumbnail.clear();
    lengthSeconds = 0.0;
    hasSelection = false;
    selStart = selEnd = 0.0;
    playhead = -1.0;
    repaint();
}

void WaveformDisplay::setPlayheadProportion (double proportion)
{
    if (std::abs (proportion - playhead) > 1.0e-4)
    {
        playhead = proportion;
        repaint();
    }
}

void WaveformDisplay::changeListenerCallback (juce::ChangeBroadcaster*)
{
    lengthSeconds = thumbnail.getTotalLength();
    repaint();
}

//==============================================================================
double WaveformDisplay::xToTime (float x) const
{
    auto b = getLocalBounds().toFloat();

    if (b.getWidth() <= 0.0f || lengthSeconds <= 0.0)
        return 0.0;

    const double p = juce::jlimit (0.0, 1.0, (double) ((x - b.getX()) / b.getWidth()));
    return p * lengthSeconds;
}

float WaveformDisplay::timeToX (double timeSeconds) const
{
    auto b = getLocalBounds().toFloat();

    if (lengthSeconds <= 0.0)
        return b.getX();

    return b.getX() + (float) (timeSeconds / lengthSeconds) * b.getWidth();
}

//==============================================================================
void WaveformDisplay::paint (juce::Graphics& g)
{
    auto area = getLocalBounds().toFloat();

    g.setColour (MixColours::surface);
    g.fillRoundedRectangle (area, 8.0f);

    if (thumbnail.getNumChannels() == 0)
    {
        g.setColour (MixColours::textDim);
        g.setFont (MixLookAndFeel::uiFont (13.0f, false));
        g.drawText ("Load a song to see its waveform",
                    getLocalBounds(), juce::Justification::centred);
    }
    else
    {
        // The waveform itself.
        g.setColour (MixColours::accentH);
        thumbnail.drawChannels (g, getLocalBounds().reduced (3),
                                0.0, thumbnail.getTotalLength(), 1.0f);

        // Selection: dim everything OUTSIDE the selected section so it pops.
        if (hasSelection && selEnd > selStart)
        {
            const float x1 = timeToX (selStart);
            const float x2 = timeToX (selEnd);

            g.setColour (MixColours::bg.withAlpha (0.72f));
            g.fillRect (juce::Rectangle<float> (area.getX(), area.getY(),
                                                x1 - area.getX(), area.getHeight()));
            g.fillRect (juce::Rectangle<float> (x2, area.getY(),
                                                area.getRight() - x2, area.getHeight()));

            // Bright edges on the section boundaries.
            g.setColour (MixColours::accent);
            g.drawLine (x1, area.getY(), x1, area.getBottom(), 1.5f);
            g.drawLine (x2, area.getY(), x2, area.getBottom(), 1.5f);
        }

        // Playhead.
        if (playhead >= 0.0 && playhead <= 1.0)
        {
            const float px = area.getX() + (float) playhead * area.getWidth();
            g.setColour (juce::Colours::white);
            g.drawLine (px, area.getY(), px, area.getBottom(), 1.0f);
        }
    }

    g.setColour (MixColours::border);
    g.drawRoundedRectangle (area, 8.0f, 1.0f);
}

//==============================================================================
void WaveformDisplay::mouseDown (const juce::MouseEvent& e)
{
    if (lengthSeconds <= 0.0)
        return;

    dragAnchor = xToTime ((float) e.x);
    selStart = selEnd = dragAnchor;
    hasSelection = false;
    repaint();
}

void WaveformDisplay::mouseDrag (const juce::MouseEvent& e)
{
    if (lengthSeconds <= 0.0)
        return;

    const double t = xToTime ((float) e.x);
    selStart = juce::jmin (dragAnchor, t);
    selEnd   = juce::jmax (dragAnchor, t);
    hasSelection = (selEnd - selStart) > 0.02;   // ignore tiny drags
    repaint();

    if (hasSelection && onSelectionChanged != nullptr)
        onSelectionChanged (selStart, selEnd);
}

void WaveformDisplay::mouseUp (const juce::MouseEvent&)
{
    if (! hasSelection)
    {
        // A plain click (no real drag) clears the selection: play the whole song.
        selStart = selEnd = 0.0;
        if (onSelectionCleared != nullptr)
            onSelectionCleared();
        repaint();
    }
}
