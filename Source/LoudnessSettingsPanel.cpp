#include "LoudnessSettingsPanel.h"
#include "MixLookAndFeel.h"

//==============================================================================
LoudnessSettingsPanel::LoudnessSettingsPanel (LoudnessDisplay& l)
    : loud (l)
{
    auto setupCaption = [this] (juce::Label& lab, const juce::String& text)
    {
        lab.setText (text.toUpperCase(), juce::dontSendNotification);
        lab.setFont (MixLookAndFeel::uiFont (10.5f, false));
        lab.setColour (juce::Label::textColourId, MixColours::textDim);
        addAndMakeVisible (lab);
    };

    // Preset bar (save / pick / delete user presets).
    presetBar = std::make_unique<PresetBar> (
        juce::String ("loudness"),
        [this] { return loud.getSettingsString(); },
        [this] (const juce::String& s) { loud.applySettingsString (s); });
    presetBar->onPresetApplied = [this] { refreshFromDisplay(); };
    addAndMakeVisible (*presetBar);

    setupCaption (viewCaption,   "View");
    setupCaption (rangeCaption,  "Range");
    setupCaption (scaleCaption,  "Scale");
    setupCaption (loudCaption,   "Loudness target");
    setupCaption (peakCaption,   "Peak target");
    setupCaption (lraCaption,    "LRA target");
    setupCaption (gateCaption,   "Gate");

    //== View (loudness-over-time timeline, or vertical bar meters) =============
    viewBox.addItem ("Timeline", 1);
    viewBox.addItem ("Bars",     2);
    viewBox.onChange = [this]
    {
        loud.setViewMode (viewBox.getSelectedId() == 2
                              ? LoudnessDisplay::ViewMode::bars
                              : LoudnessDisplay::ViewMode::timeline);
    };
    addAndMakeVisible (viewBox);

    //== Range (the meter's vertical scale) ====================================
    rangeBox.addItem ("dB (linear)",     1);
    rangeBox.addItem ("dB (non-linear)", 2);
    rangeBox.addItem ("BS.1771",         3);
    rangeBox.addItem ("EBU +9",          4);
    rangeBox.addItem ("EBU +18",         5);
    rangeBox.onChange = [this]
    {
        using R = LoudnessDisplay::RangeScale;
        switch (rangeBox.getSelectedId())
        {
            case 2:  loud.setRangeScale (R::dbNonLinear); break;
            case 3:  loud.setRangeScale (R::bs1771);      break;
            case 4:  loud.setRangeScale (R::ebuPlus9);    break;
            case 5:  loud.setRangeScale (R::ebuPlus18);   break;
            default: loud.setRangeScale (R::dbLinear);    break;
        }
    };
    addAndMakeVisible (rangeBox);

    //== Scale (absolute LUFS or relative LU) ==================================
    scaleBox.addItem ("Absolute", 1);
    scaleBox.addItem ("Relative", 2);
    scaleBox.onChange = [this]
    {
        loud.setScaleMode (scaleBox.getSelectedId() == 2
                               ? LoudnessDisplay::ScaleMode::relative
                               : LoudnessDisplay::ScaleMode::absolute);
    };
    addAndMakeVisible (scaleBox);

    //== Numeric targets =======================================================
    auto setupNumber = [this] (juce::Slider& s, double lo, double hi, double step,
                               const juce::String& suffix)
    {
        s.setSliderStyle (juce::Slider::IncDecButtons);
        s.setRange (lo, hi, step);
        s.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 96, 22);
        s.setTextValueSuffix (suffix);
        addAndMakeVisible (s);
    };

    setupNumber (loudSlider, -40.0, 0.0,  0.1, " LUFS");
    setupNumber (peakSlider,  -9.0, 0.0,  0.1, " dBTP");
    setupNumber (lraSlider,    0.0, 30.0, 0.1, " LU");

    loudSlider.onValueChange = [this] { loud.setLoudnessTarget ((float) loudSlider.getValue()); };
    peakSlider.onValueChange = [this] { loud.setPeakTarget     ((float) peakSlider.getValue()); };
    lraSlider.onValueChange  = [this] { loud.setLraTarget      ((float) lraSlider.getValue()); };

    //== Gate ==================================================================
    gateBox.addItem ("Off",      1);
    gateBox.addItem ("Dialogue", 2);
    gateBox.addItem ("Program",  3);
    gateBox.onChange = [this]
    {
        using G = LoudnessDisplay::GateMode;
        switch (gateBox.getSelectedId())
        {
            case 1:  loud.setGateMode (G::off);      break;
            case 2:  loud.setGateMode (G::dialogue); break;
            default: loud.setGateMode (G::program);  break;
        }
    };
    addAndMakeVisible (gateBox);

    refreshFromDisplay();
    setSize (280, 311);
}

void LoudnessSettingsPanel::refreshFromDisplay()
{
    viewBox.setSelectedId (loud.getViewMode() == LoudnessDisplay::ViewMode::bars ? 2 : 1,
                           juce::dontSendNotification);

    using R = LoudnessDisplay::RangeScale;
    int rangeId = 1;
    switch (loud.getRangeScale())
    {
        case R::dbLinear:     rangeId = 1; break;
        case R::dbNonLinear:  rangeId = 2; break;
        case R::bs1771:       rangeId = 3; break;
        case R::ebuPlus9:     rangeId = 4; break;
        case R::ebuPlus18:    rangeId = 5; break;
    }
    rangeBox.setSelectedId (rangeId, juce::dontSendNotification);

    scaleBox.setSelectedId (loud.getScaleMode() == LoudnessDisplay::ScaleMode::relative ? 2 : 1,
                            juce::dontSendNotification);

    using G = LoudnessDisplay::GateMode;
    int gateId = 3;
    switch (loud.getGateMode())
    {
        case G::off:      gateId = 1; break;
        case G::dialogue: gateId = 2; break;
        case G::program:  gateId = 3; break;
    }
    gateBox.setSelectedId (gateId, juce::dontSendNotification);

    loudSlider.setValue (loud.getLoudnessTarget(), juce::dontSendNotification);
    peakSlider.setValue (loud.getPeakTarget(),     juce::dontSendNotification);
    lraSlider.setValue  (loud.getLraTarget(),      juce::dontSendNotification);
}

//==============================================================================
void LoudnessSettingsPanel::paint (juce::Graphics& g)
{
    g.fillAll (MixColours::surface);
}

void LoudnessSettingsPanel::resized()
{
    auto b = getLocalBounds().reduced (14, 12);
    const int rowH = 22;
    const int gap  = 7;

    auto row = [&b, rowH, gap] (juce::Label& caption, juce::Component& control)
    {
        auto r = b.removeFromTop (rowH);
        caption.setBounds (r.removeFromLeft (104));
        control.setBounds (r);
        b.removeFromTop (gap);
    };

    presetBar->setBounds (b.removeFromTop (presetBar->getPreferredHeight()));
    b.removeFromTop (gap + 4);   // small gap under the preset bar

    row (viewCaption,   viewBox);
    row (rangeCaption,  rangeBox);
    row (scaleCaption,  scaleBox);
    row (loudCaption,   loudSlider);
    row (peakCaption,   peakSlider);
    row (lraCaption,    lraSlider);
    row (gateCaption,   gateBox);
}
