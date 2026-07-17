#include "MixLookAndFeel.h"

//==============================================================================
MixLookAndFeel::MixLookAndFeel()
{
    setColour (juce::ResizableWindow::backgroundColourId, MixColours::bg);
    setColour (juce::TextButton::buttonColourId,   MixColours::accent);
    setColour (juce::TextButton::textColourOffId,  juce::Colours::white);
    setColour (juce::TextButton::textColourOnId,   juce::Colours::white);
    setColour (juce::Label::textColourId,          MixColours::text);
    setColour (juce::Label::backgroundColourId,    juce::Colours::transparentBlack);
}

juce::Font MixLookAndFeel::uiFont (float height, bool bold)
{
    // Segoe UI on Windows is a clean modern sans; JUCE falls back gracefully.
    return juce::Font (juce::FontOptions ("Segoe UI", height,
                                          bold ? juce::Font::bold : juce::Font::plain));
}

juce::Font MixLookAndFeel::monoFont (float height)
{
    return juce::Font (juce::FontOptions ("Consolas", height, juce::Font::plain));
}

//==============================================================================
void MixLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button,
                                           const juce::Colour& backgroundColour,
                                           bool over, bool down)
{
    auto bounds = button.getLocalBounds().toFloat().reduced (0.5f);
    const float radius = 8.0f;

    const bool ghost = backgroundColour.getFloatAlpha() < 0.5f;

    if (ghost)
    {
        if ((over || down) && button.isEnabled())
        {
            g.setColour (MixColours::surface2);
            g.fillRoundedRectangle (bounds, radius);
        }

        g.setColour (button.isEnabled() ? MixColours::border
                                        : MixColours::border.withAlpha (0.5f));
        g.drawRoundedRectangle (bounds, radius, 1.0f);
    }
    else
    {
        auto c = backgroundColour;

        if (! button.isEnabled())
            c = c.withMultipliedSaturation (0.35f).withMultipliedBrightness (0.5f);
        else if (down)
            c = c.darker (0.15f);
        else if (over)
            c = c.brighter (0.12f);

        g.setColour (c);
        g.fillRoundedRectangle (bounds, radius);
    }
}

void MixLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& button,
                                     bool over, bool /*down*/)
{
    g.setFont (getTextButtonFont (button, button.getHeight()));

    const bool ghost = button.findColour (juce::TextButton::buttonColourId).getFloatAlpha() < 0.5f;

    juce::Colour col = ghost ? MixColours::textDim : juce::Colours::white;

    if (! button.isEnabled())
        col = col.withAlpha (0.4f);
    else if (ghost && over)
        col = MixColours::text;

    g.setColour (col);
    g.drawText (button.getButtonText(), button.getLocalBounds(),
                juce::Justification::centred, true);
}

juce::Font MixLookAndFeel::getTextButtonFont (juce::TextButton&, int /*buttonHeight*/)
{
    return uiFont (13.5f, false);
}

void MixLookAndFeel::drawTickBox (juce::Graphics& g, juce::Component&,
                                  float x, float y, float w, float h,
                                  bool ticked, bool isEnabled,
                                  bool over, bool /*down*/)
{
    juce::Rectangle<float> box (x, y, w, h);
    box = box.reduced (1.0f);
    const float radius = 4.0f;

    if (ticked)
    {
        g.setColour (isEnabled ? MixColours::accent : MixColours::accent.withAlpha (0.4f));
        g.fillRoundedRectangle (box, radius);

        g.setColour (juce::Colours::white);
        juce::Path tick;
        tick.startNewSubPath (box.getX() + box.getWidth() * 0.24f, box.getCentreY());
        tick.lineTo          (box.getX() + box.getWidth() * 0.44f, box.getY() + box.getHeight() * 0.70f);
        tick.lineTo          (box.getX() + box.getWidth() * 0.78f, box.getY() + box.getHeight() * 0.30f);
        g.strokePath (tick, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved,
                                                  juce::PathStrokeType::rounded));
    }
    else
    {
        g.setColour (over ? MixColours::surface2 : MixColours::surface);
        g.fillRoundedRectangle (box, radius);
        g.setColour (MixColours::border);
        g.drawRoundedRectangle (box, radius, 1.0f);
    }
}
