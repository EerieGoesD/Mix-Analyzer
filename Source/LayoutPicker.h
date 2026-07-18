#pragma once

#include <JuceHeader.h>
#include <functional>
#include "MeterLayout.h"
#include "MixLookAndFeel.h"

//==============================================================================
// A small pop-over that shows the available panel layouts (for the current
// number of active meters) as clickable mini-diagrams, like a trading terminal.
//==============================================================================
class LayoutPicker : public juce::Component
{
public:
    LayoutPicker (int numPanels, int currentIndex, std::function<void (int)> onPick)
        : n (numPanels), current (currentIndex), pick (std::move (onPick))
    {
        const int c = MeterLayout::count (n);
        setSize (pad * 2 + c * iconW + (c - 1) * gap, pad + titleH + iconH + 18 + pad);
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (MixColours::surface);
        const int c = MeterLayout::count (n);

        g.setColour (MixColours::textDim);
        g.setFont (MixLookAndFeel::uiFont (11.0f, true));
        g.drawText ("CHOOSE A PANEL LAYOUT", pad, pad, getWidth() - pad * 2, 14,
                    juce::Justification::centredLeft);

        for (int i = 0; i < c; ++i)
        {
            auto box = iconRect (i);

            g.setColour (i == current ? MixColours::accent : MixColours::border);
            g.drawRoundedRectangle (box.toFloat(), 4.0f, i == current ? 1.8f : 1.0f);

            for (auto& r : MeterLayout::tiles (box.reduced (5), n, i))
            {
                g.setColour (i == current ? MixColours::accentH : MixColours::textDim);
                g.fillRect (r.reduced (1));
            }

            g.setColour (MixColours::textDim);
            g.setFont (MixLookAndFeel::uiFont (9.0f));
            g.drawText (MeterLayout::name (n, i),
                        box.getX() - 6, box.getBottom() + 2, iconW + 12, 16,
                        juce::Justification::centred, false);
        }
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        const int c = MeterLayout::count (n);
        for (int i = 0; i < c; ++i)
        {
            if (iconRect (i).contains (e.getPosition()))
            {
                if (pick != nullptr) pick (i);
                if (auto* cb = findParentComponentOfClass<juce::CallOutBox>())
                    cb->dismiss();
                return;
            }
        }
    }

private:
    juce::Rectangle<int> iconRect (int i) const
    {
        return { pad + i * (iconW + gap), pad + titleH, iconW, iconH };
    }

    static constexpr int iconW = 64, iconH = 50, gap = 10, pad = 12, titleH = 22;

    int n, current;
    std::function<void (int)> pick;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LayoutPicker)
};
