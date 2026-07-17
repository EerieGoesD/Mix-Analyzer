#include "HistoryAnalyzer.h"
#include "MixLookAndFeel.h"
#include <cmath>
#include <complex>
#include <algorithm>

//==============================================================================
HistoryAnalyzer::HistoryAnalyzer (MixAnalyzerAudioProcessor& p)
    : processorRef (p)
{
    formatManager.registerBasicFormats();

    algoBox.addItem ("Spectrum",                 1);
    algoBox.addItem ("Standard Autocorrelation", 2);
    algoBox.addItem ("Cuberoot Autocorrelation", 3);
    algoBox.addItem ("Enhanced Autocorrelation", 4);
    algoBox.addItem ("Cepstrum",                 5);
    algoBox.setSelectedId (1, juce::dontSendNotification);

    const char* sizes[] = { "128", "256", "512", "1024", "2048", "4096", "8192",
                            "16384", "32768", "65536", "131072" };
    for (int i = 0; i < 11; ++i) sizeBox.addItem (sizes[i], i + 1);
    sizeBox.setSelectedId (7, juce::dontSendNotification);   // 8192

    const char* wins[] = { "Rectangular", "Bartlett", "Hamming", "Hann", "Blackman",
                           "Blackman-Harris", "Welch", "Gaussian (a=2.5)",
                           "Gaussian (a=3.5)", "Gaussian (a=4.5)" };
    for (int i = 0; i < 10; ++i) windowBox.addItem (wins[i], i + 1);
    windowBox.setSelectedId (4, juce::dontSendNotification);   // Hann

    axisBox.addItem ("Log frequency",    1);
    axisBox.addItem ("Linear frequency", 2);
    axisBox.setSelectedId (1, juce::dontSendNotification);

    // Settings only update the choice; analysis runs when Analyze is pressed
    // (so changing options never freezes the app). Axis is display-only -> repaint.
    algoBox.onChange   = [this] { algo       = (Algo) (algoBox.getSelectedId() - 1); };
    sizeBox.onChange   = [this] { fftSizeSel = 128 << (sizeBox.getSelectedId() - 1); };
    windowBox.onChange = [this] { windowType = windowBox.getSelectedId() - 1; };
    axisBox.onChange   = [this] { axis       = (Axis) (axisBox.getSelectedId() - 1); repaint(); };

    analyzeButton.onClick = [this]
    {
        // Show "Analyzing..." first, then run the heavy work on the next message
        // loop so the label actually paints instead of the app looking frozen.
        statusLabel.setText ("Analyzing...", juce::dontSendNotification);
        repaint();
        juce::Component::SafePointer<HistoryAnalyzer> safe (this);
        juce::MessageManager::callAsync ([safe] { if (safe != nullptr) safe->analyzeSelection(); });
    };
    exportButton.onClick = [this] { exportData(); };

    statusLabel.setFont (MixLookAndFeel::monoFont (11.5f));
    statusLabel.setColour (juce::Label::textColourId, MixColours::textDim);

    for (auto* c : { &algoBox, &sizeBox, &windowBox, &axisBox })
        addAndMakeVisible (c);
    addAndMakeVisible (analyzeButton);
    addAndMakeVisible (exportButton);
    addAndMakeVisible (statusLabel);
}

//==============================================================================
void HistoryAnalyzer::fillWindow (std::vector<float>& w, int size, int type) const
{
    w.assign ((size_t) size, 1.0f);
    using WM = juce::dsp::WindowingFunction<float>;

    switch (type)
    {
        case 0: /* Rectangular - leave as 1 */                                            break;
        case 1: WM::fillWindowingTables (w.data(), (size_t) size, WM::triangular,     false); break;
        case 2: WM::fillWindowingTables (w.data(), (size_t) size, WM::hamming,        false); break;
        case 3: WM::fillWindowingTables (w.data(), (size_t) size, WM::hann,           false); break;
        case 4: WM::fillWindowingTables (w.data(), (size_t) size, WM::blackman,       false); break;
        case 5: WM::fillWindowingTables (w.data(), (size_t) size, WM::blackmanHarris, false); break;
        default:
        {
            const float n1 = (float) (size - 1);
            for (int n = 0; n < size; ++n)
            {
                const float x = (2.0f * (float) n - n1) / n1;   // -1..1
                float v;
                if (type == 6) v = 1.0f - x * x;                // Welch
                else
                {
                    const float a = type == 7 ? 2.5f : type == 8 ? 3.5f : 4.5f;   // Gaussians
                    v = std::exp (-0.5f * (a * x) * (a * x));
                }
                w[(size_t) n] = v;
            }
            break;
        }
    }
}

void HistoryAnalyzer::analyzeSelection()
{
    result.clear();

    auto setStatus = [this] (const juce::String& t)
    {
        statusLabel.setText (t, juce::dontSendNotification);
        repaint();
    };

    const auto file = processorRef.getLoadedFile();
    if (file == juce::File{} || ! file.existsAsFile())            { setStatus ("Load a song first."); return; }

    std::unique_ptr<juce::AudioFormatReader> reader (formatManager.createReaderFor (file));
    if (reader == nullptr || reader->sampleRate <= 0.0)          { setStatus ("Could not read the file."); return; }

    const double sr = reader->sampleRate;
    double s0 = processorRef.getSelectionStartSeconds();
    double s1 = processorRef.getSelectionEndSeconds();
    if (s1 <= s0) { s0 = 0.0; s1 = (double) reader->lengthInSamples / sr; }   // whole file

    const juce::int64 startSamp = (juce::int64) (s0 * sr);
    const juce::int64 endSamp   = juce::jmin ((juce::int64) (s1 * sr), reader->lengthInSamples);
    juce::int64 numSamps = endSamp - startSamp;
    numSamps = juce::jmin (numSamps, (juce::int64) (sr * 120.0));   // cap at 2 min

    if (numSamps < 64)                                           { setStatus ("Selection too short to analyze."); return; }

    // Read the region and sum to mono.
    const int chans = (int) reader->numChannels;
    juce::AudioBuffer<float> buf (juce::jmax (1, chans), (int) numSamps);
    reader->read (&buf, 0, (int) numSamps, startSamp, true, true);

    std::vector<float> mono ((size_t) numSamps, 0.0f);
    for (int ch = 0; ch < buf.getNumChannels(); ++ch)
    {
        const float* d = buf.getReadPointer (ch);
        for (int i = 0; i < (int) numSamps; ++i) mono[(size_t) i] += d[i];
    }
    if (buf.getNumChannels() > 1)
        for (auto& v : mono) v /= (float) buf.getNumChannels();

    // Block-average the FFT over the selection (50% overlap).
    const int size  = fftSizeSel;
    const int order = (int) std::round (std::log2 ((double) size));
    if ((int) mono.size() < size) mono.resize ((size_t) size, 0.0f);

    std::vector<float> win;
    fillWindow (win, size, windowType);

    const int hop = size / 2;
    const int numBlocks = juce::jmax (1, ((int) mono.size() - size) / hop + 1);

    juce::dsp::FFT fft (order);
    std::vector<std::complex<float>> in ((size_t) size), spec ((size_t) size);
    std::vector<double> avgPower  ((size_t) size, 0.0);
    std::vector<double> avgLogMag ((size_t) size, 0.0);

    for (int b = 0; b < numBlocks; ++b)
    {
        const int start = b * hop;
        for (int n = 0; n < size; ++n)
            in[(size_t) n] = { mono[(size_t) (start + n)] * win[(size_t) n], 0.0f };

        fft.perform (in.data(), spec.data(), false);

        for (int k = 0; k < size; ++k)
        {
            const double pw = (double) std::norm (spec[(size_t) k]);
            avgPower[(size_t) k]  += pw;
            avgLogMag[(size_t) k] += std::log (std::sqrt (pw) + 1.0e-9);
        }
    }
    for (int k = 0; k < size; ++k) { avgPower[(size_t) k] /= numBlocks; avgLogMag[(size_t) k] /= numBlocks; }

    const int half = size / 2;
    resFMin = (axis == Axis::logFreq) ? (float) (sr / size) : 20.0f;
    resFMax = (float) (sr * 0.5);

    if (algo == Algo::spectrum)
    {
        const float normDb = juce::Decibels::gainToDecibels ((float) size);
        for (int k = 1; k <= half; ++k)
        {
            const float freq = (float) (k * sr / size);
            const float mag  = (float) std::sqrt (avgPower[(size_t) k]);
            const float db   = juce::Decibels::gainToDecibels (mag) - normDb;
            const float lvl  = juce::jmap (juce::jlimit (mindB, maxdB, db), mindB, maxdB, 0.0f, 1.0f);
            result.push_back ({ freq, lvl, db });
        }
    }
    else
    {
        // Autocorrelation family / cepstrum via inverse FFT.
        std::vector<std::complex<float>> freqDom ((size_t) size), timeDom ((size_t) size);

        if (algo == Algo::cepstrum)
            for (int k = 0; k < size; ++k) freqDom[(size_t) k] = { (float) avgLogMag[(size_t) k], 0.0f };
        else if (algo == Algo::cubeAutocorr)
            for (int k = 0; k < size; ++k) freqDom[(size_t) k] = { std::pow ((float) avgPower[(size_t) k], 1.0f / 3.0f), 0.0f };
        else
            for (int k = 0; k < size; ++k) freqDom[(size_t) k] = { (float) avgPower[(size_t) k], 0.0f };

        fft.perform (freqDom.data(), timeDom.data(), true);

        const float norm = std::abs (timeDom[0].real()) > 1.0e-12f ? std::abs (timeDom[0].real()) : 1.0f;

        for (int lag = 2; lag <= half; ++lag)
        {
            const float freq = (float) (sr / lag);
            if (freq > resFMax) continue;
            float v = std::abs (timeDom[(size_t) lag].real()) / norm;
            if (algo == Algo::enhAutocorr) v = juce::jmax (0.0f, v - 0.2f) * 1.25f;   // simple emphasis
            const float lvl = juce::jlimit (0.0f, 1.0f, v);
            result.push_back ({ freq, lvl, juce::Decibels::gainToDecibels (juce::jmax (1.0e-4f, v)) });
        }
        std::reverse (result.begin(), result.end());   // ascending frequency
    }

    setStatus ("Analyzed " + juce::String (s1 - s0, 2) + " s  (" + juce::String (numBlocks) + " blocks, size " + juce::String (size) + ")");
}

//==============================================================================
void HistoryAnalyzer::exportData()
{
    if (result.empty()) { statusLabel.setText ("Nothing to export - analyze first.", juce::dontSendNotification); return; }

    chooser = std::make_unique<juce::FileChooser> ("Export spectrum data", juce::File{}, "*.txt");
    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
                          [this] (const juce::FileChooser& fc)
    {
        auto f = fc.getResult();
        if (f == juce::File{}) return;

        juce::String txt = "Frequency (Hz)\tLevel (dB)\n";
        for (const auto& p : result)
            txt << juce::String (p.freq, 2) << "\t" << juce::String (p.db, 2) << "\n";

        f.replaceWithText (txt);
        statusLabel.setText ("Exported to " + f.getFileName(), juce::dontSendNotification);
    });
}

//==============================================================================
juce::Rectangle<float> HistoryAnalyzer::plotBounds() const
{
    auto area = getLocalBounds().toFloat();
    area.removeFromTop (40.0f);   // control strip
    area = area.reduced (12.0f);
    const float leftGutter = 34.0f, bottomGutter = 16.0f;
    return { area.getX() + leftGutter, area.getY() + 6.0f,
             juce::jmax (1.0f, area.getWidth() - leftGutter - 6.0f),
             juce::jmax (1.0f, area.getHeight() - bottomGutter - 8.0f) };
}

float HistoryAnalyzer::freqToX (float freq, juce::Rectangle<float> plot) const
{
    if (resFMax <= resFMin) return plot.getX();
    if (axis == Axis::linFreq)
        return plot.getX() + juce::jlimit (0.0f, 1.0f, (freq - resFMin) / (resFMax - resFMin)) * plot.getWidth();

    const float frac = std::log (juce::jmax (1.0f, freq) / resFMin) / std::log (resFMax / resFMin);
    return plot.getX() + juce::jlimit (0.0f, 1.0f, frac) * plot.getWidth();
}

float HistoryAnalyzer::xToFreq (float x, juce::Rectangle<float> plot) const
{
    const float frac = juce::jlimit (0.0f, 1.0f, (x - plot.getX()) / plot.getWidth());
    if (axis == Axis::linFreq)
        return resFMin + frac * (resFMax - resFMin);
    return resFMin * std::exp (std::log (resFMax / resFMin) * frac);
}

//==============================================================================
void HistoryAnalyzer::paint (juce::Graphics& g)
{
    auto area = getLocalBounds().toFloat();
    g.setColour (MixColours::surface);
    g.fillRoundedRectangle (area, 8.0f);
    g.setColour (MixColours::border);
    g.drawRoundedRectangle (area, 8.0f, 1.0f);

    auto plot = plotBounds();
    g.setFont (MixLookAndFeel::monoFont (9.5f));

    for (int db = 0; db >= -100; db -= 20)
    {
        const float y = juce::jmap ((float) db, 0.0f, -100.0f, plot.getY(), plot.getBottom());
        g.setColour (MixColours::border.withAlpha (0.6f));
        g.drawHorizontalLine ((int) y, plot.getX(), plot.getRight());
        g.setColour (MixColours::textDim);
        g.drawText (juce::String (db), juce::Rectangle<float> (plot.getX() - 32.0f, y - 6.0f, 28.0f, 12.0f),
                    juce::Justification::centredRight);
    }

    const int freqs[] = { 20, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000 };
    float lastLabelX = -1000.0f;
    for (int f : freqs)
    {
        const float x = freqToX ((float) f, plot);
        if (x < plot.getX() - 0.5f || x > plot.getRight() + 0.5f) continue;
        g.setColour (MixColours::border.withAlpha (0.5f));
        g.drawVerticalLine ((int) x, plot.getY(), plot.getBottom());
        if (x - lastLabelX >= 30.0f)
        {
            g.setColour (MixColours::textDim);
            const juce::String lbl = f >= 1000 ? juce::String (f / 1000) + "k" : juce::String (f);
            g.drawText (lbl, juce::Rectangle<float> (x - 16.0f, plot.getBottom() + 2.0f, 32.0f, 12.0f),
                        juce::Justification::centred);
            lastLabelX = x;
        }
    }

    if (result.empty())
    {
        g.setColour (MixColours::textDim);
        g.setFont (MixLookAndFeel::uiFont (13.0f, false));
        g.drawText ("Select a section on the waveform, then hit Analyze.",
                    plot.toNearestInt(), juce::Justification::centred);
        return;
    }

    auto levelToY = [&plot] (float lv) { return juce::jmap (juce::jlimit (0.0f, 1.0f, lv), 0.0f, 1.0f, plot.getBottom(), plot.getY()); };

    juce::Path curve;
    curve.startNewSubPath (plot.getX(), plot.getBottom());
    for (const auto& p : result) curve.lineTo (freqToX (p.freq, plot), levelToY (p.level));
    curve.lineTo (plot.getRight(), plot.getBottom());
    curve.closeSubPath();
    juce::ColourGradient grad (MixColours::accent.withAlpha (0.85f), plot.getX(), plot.getY(),
                               MixColours::accent.withAlpha (0.12f), plot.getX(), plot.getBottom(), false);
    g.setGradientFill (grad); g.fillPath (curve);

    juce::Path line;
    for (size_t i = 0; i < result.size(); ++i)
    {
        const float x = freqToX (result[i].freq, plot), y = levelToY (result[i].level);
        if (i == 0) line.startNewSubPath (x, y); else line.lineTo (x, y);
    }
    g.setColour (MixColours::accentH); g.strokePath (line, juce::PathStrokeType (1.3f));

    if (hovering && plot.contains (mousePos))
    {
        const float freq = xToFreq (mousePos.x, plot);
        size_t best = 0; float bestD = 1.0e30f;
        for (size_t i = 0; i < result.size(); ++i) { const float d = std::abs (result[i].freq - freq); if (d < bestD) { bestD = d; best = i; } }
        g.setColour (MixColours::accentH.withAlpha (0.5f));
        g.drawVerticalLine ((int) mousePos.x, plot.getY(), plot.getBottom());

        const juce::String fs = result[best].freq >= 1000.0f ? juce::String (result[best].freq / 1000.0f, 2) + " kHz"
                                                             : juce::String (juce::roundToInt (result[best].freq)) + " Hz";
        const juce::String txt = fs + "   " + juce::String (result[best].db, 1) + " dB";
        g.setFont (MixLookAndFeel::monoFont (11.0f));
        float tx = mousePos.x + 8.0f; const float tw = 130.0f;
        if (tx + tw > plot.getRight()) tx = mousePos.x - 8.0f - tw;
        juce::Rectangle<float> box (tx, plot.getY() + 4.0f, tw, 18.0f);
        g.setColour (MixColours::bg.withAlpha (0.85f)); g.fillRoundedRectangle (box, 4.0f);
        g.setColour (MixColours::border); g.drawRoundedRectangle (box, 4.0f, 1.0f);
        g.setColour (MixColours::text); g.drawText (txt, box, juce::Justification::centred);
    }
}

void HistoryAnalyzer::resized()
{
    auto row = getLocalBounds().removeFromTop (40).reduced (12, 6);
    auto cell = [&row] (int w) { auto r = row.removeFromLeft (w); row.removeFromLeft (6); return r; };

    algoBox.setBounds    (cell (150));
    sizeBox.setBounds    (cell (78));
    windowBox.setBounds  (cell (128));
    axisBox.setBounds    (cell (110));
    analyzeButton.setBounds (cell (84));
    exportButton.setBounds  (cell (72));
    statusLabel.setBounds (row);
}

void HistoryAnalyzer::mouseMove (const juce::MouseEvent& e) { hovering = true; mousePos = e.position; repaint(); }
void HistoryAnalyzer::mouseExit (const juce::MouseEvent&)   { hovering = false; repaint(); }
