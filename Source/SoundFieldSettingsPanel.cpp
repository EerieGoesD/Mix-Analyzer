#include "SoundFieldSettingsPanel.h"
#include "MixLookAndFeel.h"

//==============================================================================
SoundFieldSettingsPanel::SoundFieldSettingsPanel (SoundFieldDisplay& f)
    : field (f)
{
    auto setupCaption = [this] (juce::Label& l, const juce::String& text)
    {
        l.setText (text.toUpperCase(), juce::dontSendNotification);
        l.setFont (MixLookAndFeel::uiFont (10.5f, false));
        l.setColour (juce::Label::textColourId, MixColours::textDim);
        addAndMakeVisible (l);
    };

    setupCaption (vectorCaption, "Vectorscope");
    setupCaption (detectCaption, "Detection");

    presetBar = std::make_unique<PresetBar> (
        juce::String ("soundfield"),
        [this] { return field.getSettingsString(); },
        [this] (const juce::String& s) { field.applySettingsString (s); });
    presetBar->onPresetApplied = [this] { refreshFromDisplay(); };
    addAndMakeVisible (*presetBar);

    //== Vectorscope mode ======================================================
    vectorBox.addItem ("Lissajous",    1);
    vectorBox.addItem ("Polar sample", 2);
    vectorBox.addItem ("Polar level",  3);
    vectorBox.onChange = [this]
    {
        using V = SoundFieldDisplay::VectorMode;
        switch (vectorBox.getSelectedId())
        {
            case 2:  field.setVectorMode (V::polarSample); break;
            case 3:  field.setVectorMode (V::polarLevel);  break;
            default: field.setVectorMode (V::lissajous);   break;
        }
    };
    addAndMakeVisible (vectorBox);

    //== Detection method ======================================================
    detectBox.addItem ("Peak",     1);
    detectBox.addItem ("RMS",      2);
    detectBox.addItem ("Envelope", 3);
    detectBox.onChange = [this]
    {
        using D = SoundFieldDisplay::DetectMethod;
        switch (detectBox.getSelectedId())
        {
            case 2:  field.setDetectMethod (D::rms);      break;
            case 3:  field.setDetectMethod (D::envelope); break;
            default: field.setDetectMethod (D::peak);     break;
        }
    };
    addAndMakeVisible (detectBox);

    refreshFromDisplay();
    setSize (272, 160);
}

void SoundFieldSettingsPanel::refreshFromDisplay()
{
    using V = SoundFieldDisplay::VectorMode;
    int vid = 1;
    switch (field.getVectorMode())
    {
        case V::lissajous:   vid = 1; break;
        case V::polarSample: vid = 2; break;
        case V::polarLevel:  vid = 3; break;
    }
    vectorBox.setSelectedId (vid, juce::dontSendNotification);

    using D = SoundFieldDisplay::DetectMethod;
    int did = 1;
    switch (field.getDetectMethod())
    {
        case D::peak:     did = 1; break;
        case D::rms:      did = 2; break;
        case D::envelope: did = 3; break;
    }
    detectBox.setSelectedId (did, juce::dontSendNotification);
}

//==============================================================================
void SoundFieldSettingsPanel::paint (juce::Graphics& g)
{
    g.fillAll (MixColours::surface);
}

void SoundFieldSettingsPanel::resized()
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
    b.removeFromTop (gap + 4);

    row (vectorCaption, vectorBox);
    row (detectCaption, detectBox);
}
