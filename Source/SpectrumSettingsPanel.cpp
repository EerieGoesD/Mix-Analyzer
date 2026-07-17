#include "SpectrumSettingsPanel.h"
#include "MixLookAndFeel.h"

//==============================================================================
SpectrumSettingsPanel::SpectrumSettingsPanel (SpectrumDisplay& s)
    : spectrum (s)
{
    auto setupCaption = [this] (juce::Label& l, const juce::String& text)
    {
        l.setText (text.toUpperCase(), juce::dontSendNotification);
        l.setFont (MixLookAndFeel::uiFont (10.5f, false));
        l.setColour (juce::Label::textColourId, MixColours::textDim);
        addAndMakeVisible (l);
    };

    setupCaption (specTypeCaption,   "Spectrum type");
    setupCaption (windowSizeCaption, "Window size");
    setupCaption (windowTypeCaption, "Window type");
    setupCaption (averageCaption,    "Averaging");
    setupCaption (overlapCaption,    "Overlap");
    setupCaption (peakHoldCaption,   "Peak hold time");

    //== Spectrum type =========================================================
    specTypeBox.addItem ("Linear",       1);
    specTypeBox.addItem ("1/3 Octave",   2);
    specTypeBox.addItem ("Critical",     3);
    specTypeBox.addItem ("Full Octave",  4);
    {
        using ST = SpectrumDisplay::SpecType;
        int id = 2;
        switch (spectrum.getSpectrumType())
        {
            case ST::linear:      id = 1; break;
            case ST::thirdOctave: id = 2; break;
            case ST::critical:    id = 3; break;
            case ST::fullOctave:  id = 4; break;
        }
        specTypeBox.setSelectedId (id, juce::dontSendNotification);
    }
    specTypeBox.onChange = [this]
    {
        using ST = SpectrumDisplay::SpecType;
        switch (specTypeBox.getSelectedId())
        {
            case 1:  spectrum.setSpectrumType (ST::linear);      break;
            case 3:  spectrum.setSpectrumType (ST::critical);    break;
            case 4:  spectrum.setSpectrumType (ST::fullOctave);  break;
            default: spectrum.setSpectrumType (ST::thirdOctave); break;
        }
    };
    addAndMakeVisible (specTypeBox);

    //== Window size: 512..32768 -> order 9..15 (order = id + 8) ===============
    const char* sizes[] = { "512", "1024", "2048", "4096", "8192",
                            "16384 (Slow)", "32768 (Slow)" };
    for (int i = 0; i < 7; ++i)
        windowSizeBox.addItem (sizes[i], i + 1);
    windowSizeBox.setSelectedId (spectrum.getWindowOrder() - 8, juce::dontSendNotification);
    windowSizeBox.onChange = [this]
    {
        spectrum.setWindowOrder (windowSizeBox.getSelectedId() + 8);
    };
    addAndMakeVisible (windowSizeBox);

    //== Window type (Insight set) =============================================
    windowTypeBox.addItem ("Hann",     1);
    windowTypeBox.addItem ("Hamming",  2);
    windowTypeBox.addItem ("Blackman", 3);
    windowTypeBox.addItem ("Bartlett", 4);
    windowTypeBox.addItem ("Kaiser",   5);
    {
        using WT = SpectrumDisplay::WinType;
        int id = 1;
        switch (spectrum.getWindowType())
        {
            case WT::hann:     id = 1; break;
            case WT::hamming:  id = 2; break;
            case WT::blackman: id = 3; break;
            case WT::bartlett: id = 4; break;
            case WT::kaiser:   id = 5; break;
        }
        windowTypeBox.setSelectedId (id, juce::dontSendNotification);
    }
    windowTypeBox.onChange = [this]
    {
        using WT = SpectrumDisplay::WinType;
        switch (windowTypeBox.getSelectedId())
        {
            case 2:  spectrum.setWindowType (WT::hamming);  break;
            case 3:  spectrum.setWindowType (WT::blackman); break;
            case 4:  spectrum.setWindowType (WT::bartlett); break;
            case 5:  spectrum.setWindowType (WT::kaiser);   break;
            default: spectrum.setWindowType (WT::hann);     break;
        }
    };
    addAndMakeVisible (windowTypeBox);

    //== Averaging =============================================================
    averageBox.addItem ("Real-time", 1);
    averageBox.addItem ("1 sec",     2);
    averageBox.addItem ("3 sec",     3);
    averageBox.addItem ("5 sec",     4);
    averageBox.addItem ("10 sec",    5);
    averageBox.addItem ("Infinite",  6);
    {
        const float avg = spectrum.getAverageSeconds();
        int id = 1;
        if (avg < 0.0f)                                  id = 6;
        else if (juce::approximatelyEqual (avg, 1.0f))   id = 2;
        else if (juce::approximatelyEqual (avg, 3.0f))   id = 3;
        else if (juce::approximatelyEqual (avg, 5.0f))   id = 4;
        else if (juce::approximatelyEqual (avg, 10.0f))  id = 5;
        averageBox.setSelectedId (id, juce::dontSendNotification);
    }
    averageBox.onChange = [this]
    {
        switch (averageBox.getSelectedId())
        {
            case 2:  spectrum.setAverageSeconds (1.0f);   break;
            case 3:  spectrum.setAverageSeconds (3.0f);   break;
            case 4:  spectrum.setAverageSeconds (5.0f);   break;
            case 5:  spectrum.setAverageSeconds (10.0f);  break;
            case 6:  spectrum.setAverageSeconds (-1.0f);  break;
            default: spectrum.setAverageSeconds (0.0f);   break;
        }
    };
    addAndMakeVisible (averageBox);

    //== Overlap ===============================================================
    overlapBox.addItem ("0%",    1);
    overlapBox.addItem ("50%",   2);
    overlapBox.addItem ("75%",   3);
    overlapBox.addItem ("87.5%", 4);
    {
        const float ov = spectrum.getOverlapPercent();
        int id = 2;
        if      (juce::approximatelyEqual (ov, 0.0f))   id = 1;
        else if (juce::approximatelyEqual (ov, 75.0f))  id = 3;
        else if (juce::approximatelyEqual (ov, 87.5f))  id = 4;
        overlapBox.setSelectedId (id, juce::dontSendNotification);
    }
    overlapBox.onChange = [this]
    {
        switch (overlapBox.getSelectedId())
        {
            case 1:  spectrum.setOverlapPercent (0.0f);   break;
            case 3:  spectrum.setOverlapPercent (75.0f);  break;
            case 4:  spectrum.setOverlapPercent (87.5f);  break;
            default: spectrum.setOverlapPercent (50.0f);  break;
        }
    };
    addAndMakeVisible (overlapBox);

    //== Peak hold time ========================================================
    peakHoldBox.addItem ("5 ms",    1);
    peakHoldBox.addItem ("250 ms",  2);
    peakHoldBox.addItem ("500 ms",  3);
    peakHoldBox.addItem ("1 sec",   4);
    peakHoldBox.addItem ("5 sec",   5);
    peakHoldBox.addItem ("10 sec",  6);
    peakHoldBox.addItem ("Infinite",7);
    {
        const float ph = spectrum.getPeakHoldMs();
        int id = 4;
        if (ph < 0.0f)                                     id = 7;
        else if (juce::approximatelyEqual (ph, 5.0f))      id = 1;
        else if (juce::approximatelyEqual (ph, 250.0f))    id = 2;
        else if (juce::approximatelyEqual (ph, 500.0f))    id = 3;
        else if (juce::approximatelyEqual (ph, 5000.0f))   id = 5;
        else if (juce::approximatelyEqual (ph, 10000.0f))  id = 6;
        peakHoldBox.setSelectedId (id, juce::dontSendNotification);
    }
    peakHoldBox.onChange = [this]
    {
        switch (peakHoldBox.getSelectedId())
        {
            case 1:  spectrum.setPeakHoldMs (5.0f);     break;
            case 2:  spectrum.setPeakHoldMs (250.0f);   break;
            case 3:  spectrum.setPeakHoldMs (500.0f);   break;
            case 5:  spectrum.setPeakHoldMs (5000.0f);  break;
            case 6:  spectrum.setPeakHoldMs (10000.0f); break;
            case 7:  spectrum.setPeakHoldMs (-1.0f);    break;
            default: spectrum.setPeakHoldMs (1000.0f);  break;
        }
    };
    peakHoldBox.setEnabled (spectrum.isPeakHoldShown());
    addAndMakeVisible (peakHoldBox);

    //== Show peak hold + freeze ==============================================
    peakHoldToggle.setColour (juce::ToggleButton::textColourId, MixColours::text);
    peakHoldToggle.setToggleState (spectrum.isPeakHoldShown(), juce::dontSendNotification);
    peakHoldToggle.onClick = [this]
    {
        spectrum.setPeakHoldShown (peakHoldToggle.getToggleState());
        peakHoldBox.setEnabled (peakHoldToggle.getToggleState());
    };
    addAndMakeVisible (peakHoldToggle);

    freezeButton.setButtonText (spectrum.hasReferenceCurve() ? "Clear reference" : "Freeze reference");
    freezeButton.onClick = [this]
    {
        if (spectrum.hasReferenceCurve())
        {
            spectrum.clearReference();
            freezeButton.setButtonText ("Freeze reference");
        }
        else
        {
            spectrum.freezeReference();
            freezeButton.setButtonText ("Clear reference");
        }
    };
    addAndMakeVisible (freezeButton);

    setSize (262, 322);
}

//==============================================================================
void SpectrumSettingsPanel::paint (juce::Graphics& g)
{
    g.fillAll (MixColours::surface);
}

void SpectrumSettingsPanel::resized()
{
    auto b = getLocalBounds().reduced (14, 12);
    const int rowH = 22;
    const int gap  = 7;

    auto row = [&b, rowH, gap] (juce::Label& caption, juce::Component& control)
    {
        auto r = b.removeFromTop (rowH);
        caption.setBounds (r.removeFromLeft (98));
        control.setBounds (r);
        b.removeFromTop (gap);
    };

    row (specTypeCaption,   specTypeBox);
    row (windowSizeCaption, windowSizeBox);
    row (windowTypeCaption, windowTypeBox);
    row (averageCaption,    averageBox);
    row (overlapCaption,    overlapBox);

    peakHoldToggle.setBounds (b.removeFromTop (rowH));
    b.removeFromTop (gap);

    row (peakHoldCaption, peakHoldBox);

    freezeButton.setBounds (b.removeFromTop (28));
}
