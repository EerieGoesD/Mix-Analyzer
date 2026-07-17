#pragma once

#include <JuceHeader.h>
#include "MixLookAndFeel.h"

//==============================================================================
// A flat, ghost-style button that draws a transport icon (restart / play /
// pause / stop) instead of text. Matches the Size Scanner button look.
//==============================================================================
class TransportButton : public juce::Button
{
public:
    enum class Icon { restart, play, pause, stop };

    TransportButton (const juce::String& name, Icon initialIcon)
        : juce::Button (name), icon (initialIcon) {}

    void setIcon (Icon newIcon)
    {
        if (icon != newIcon)
        {
            icon = newIcon;
            repaint();
        }
    }

    Icon getIcon() const noexcept { return icon; }

    void paintButton (juce::Graphics& g, bool over, bool down) override
    {
        auto bounds = getLocalBounds().toFloat().reduced (0.5f);
        const float radius = 8.0f;

        if ((over || down) && isEnabled())
        {
            g.setColour (MixColours::surface2);
            g.fillRoundedRectangle (bounds, radius);
        }

        g.setColour (isEnabled() ? MixColours::border : MixColours::border.withAlpha (0.5f));
        g.drawRoundedRectangle (bounds, radius, 1.0f);

        const juce::Colour iconColour = ! isEnabled()
            ? MixColours::textDim.withAlpha (0.4f)
            : (over ? MixColours::accentH : MixColours::text);
        g.setColour (iconColour);

        const auto c = bounds.getCentre();
        const float s = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.28f;

        switch (icon)
        {
            case Icon::play:
            {
                juce::Path p;
                p.addTriangle (c.x - s * 0.7f, c.y - s,
                               c.x - s * 0.7f, c.y + s,
                               c.x + s,        c.y);
                g.fillPath (p);
                break;
            }

            case Icon::pause:
            {
                const float bw = s * 0.55f;
                g.fillRoundedRectangle (c.x - s * 0.75f, c.y - s, bw, s * 2.0f, 1.2f);
                g.fillRoundedRectangle (c.x + s * 0.2f,  c.y - s, bw, s * 2.0f, 1.2f);
                break;
            }

            case Icon::stop:
            {
                g.fillRoundedRectangle (c.x - s, c.y - s, s * 2.0f, s * 2.0f, 2.0f);
                break;
            }

            case Icon::restart:
            {
                // Vertical bar + left-pointing triangle (skip to start).
                const float barW = s * 0.42f;
                g.fillRoundedRectangle (c.x - s, c.y - s, barW, s * 2.0f, 1.0f);
                juce::Path p;
                p.addTriangle (c.x + s,       c.y - s,
                               c.x + s,       c.y + s,
                               c.x - s * 0.2f, c.y);
                g.fillPath (p);
                break;
            }
        }
    }

private:
    Icon icon;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TransportButton)
};
