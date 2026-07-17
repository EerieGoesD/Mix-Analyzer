#pragma once

#include <JuceHeader.h>

//==============================================================================
// Shared palette, translated from the Size Scanner app (futuristic dark theme).
//==============================================================================
namespace MixColours
{
    const juce::Colour bg        { 0xff0a0a0c };
    const juce::Colour surface   { 0xff111114 };
    const juce::Colour surface2  { 0xff18181c };
    const juce::Colour border    { 0xff222228 };
    const juce::Colour text      { 0xffd4d4d8 };
    const juce::Colour textDim   { 0xff9b9ba6 };
    const juce::Colour accent    { 0xff6366f1 };
    const juce::Colour accentH   { 0xff818cf8 };
}

//==============================================================================
// Flat, rounded, indigo/ghost buttons + modern type. A button whose
// buttonColourId is transparent renders as a "ghost" (outline) button; any
// other colour renders as a filled button using that colour.
//==============================================================================
class MixLookAndFeel : public juce::LookAndFeel_V4
{
public:
    MixLookAndFeel();

    static juce::Font uiFont (float height, bool bold = false);
    static juce::Font monoFont (float height);

    void drawButtonBackground (juce::Graphics&, juce::Button&,
                               const juce::Colour& backgroundColour,
                               bool shouldDrawButtonAsHighlighted,
                               bool shouldDrawButtonAsDown) override;

    void drawButtonText (juce::Graphics&, juce::TextButton&,
                         bool shouldDrawButtonAsHighlighted,
                         bool shouldDrawButtonAsDown) override;

    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;

    void drawTickBox (juce::Graphics&, juce::Component&,
                      float x, float y, float w, float h,
                      bool ticked, bool isEnabled,
                      bool shouldDrawButtonAsHighlighted,
                      bool shouldDrawButtonAsDown) override;
};
