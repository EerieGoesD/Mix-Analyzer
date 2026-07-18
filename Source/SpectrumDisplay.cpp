#include "SpectrumDisplay.h"
#include "MixLookAndFeel.h"
#include <cmath>
#include <climits>

//==============================================================================
SpectrumDisplay::SpectrumDisplay (MixAnalyzerAudioProcessor& p)
    : processorRef (p)
{
    sampleBuf.assign ((size_t) maxFftSize, 0.0f);
    rebuildFft();
    startTimerHz (60);
}

SpectrumDisplay::~SpectrumDisplay()
{
    stopTimer();
}

//==============================================================================
void SpectrumDisplay::rebuildFft()
{
    fftSize = 1 << windowOrder;
    fft     = std::make_unique<juce::dsp::FFT> (windowOrder);
    fftBuf.assign ((size_t) (2 * fftSize), 0.0f);
    magSpec.assign ((size_t) (fftSize / 2 + 1), 0.0f);
    fillWindowTable();
    lastFftSize = 0;   // force a slot rebuild next frame
}

void SpectrumDisplay::fillWindowTable()
{
    windowTable.assign ((size_t) fftSize, 1.0f);

    using WM = juce::dsp::WindowingFunction<float>;
    switch (windowType)
    {
        case WinType::hann:     WM::fillWindowingTables (windowTable.data(), (size_t) fftSize, WM::hann,       false); break;
        case WinType::hamming:  WM::fillWindowingTables (windowTable.data(), (size_t) fftSize, WM::hamming,    false); break;
        case WinType::blackman: WM::fillWindowingTables (windowTable.data(), (size_t) fftSize, WM::blackman,   false); break;
        case WinType::bartlett: WM::fillWindowingTables (windowTable.data(), (size_t) fftSize, WM::triangular, false); break;
        case WinType::kaiser:   WM::fillWindowingTables (windowTable.data(), (size_t) fftSize, WM::kaiser,     false, 8.0f); break;
    }

    double sum = 0.0;
    for (float v : windowTable) sum += (double) v;
    if (sum > 0.0)
        juce::FloatVectorOperations::multiply (windowTable.data(),
                                               (float) ((double) fftSize / sum), fftSize);
}

void SpectrumDisplay::rebuildSlots (double sr)
{
    slots.clear();
    const float binHz = (float) (sr / (double) fftSize);
    const float nyq   = (float) (sr * 0.5);
    const float fLow  = juce::jmax (20.0f, binHz);

    if (specType == SpecType::linear)
    {
        const int numBins = fftSize / 2;
        slots.reserve ((size_t) numBins);
        for (int k = 1; k <= numBins; ++k)
        {
            const float f = (float) k * binHz;
            slots.push_back ({ f, f, f });
        }
    }
    else if (specType == SpecType::critical)
    {
        // Bark critical-band edges (Hz).
        static const float edges[] = { 20, 100, 200, 300, 400, 510, 630, 770, 920, 1080,
                                       1270, 1480, 1720, 2000, 2320, 2700, 3150, 3700, 4400,
                                       5300, 6400, 7700, 9500, 12000, 15500, 20500 };
        for (int i = 0; i + 1 < (int) (sizeof (edges) / sizeof (float)); ++i)
        {
            const float lo = juce::jmax (edges[i], fLow);
            const float hi = juce::jmin (edges[i + 1], nyq);
            if (hi > lo)
                slots.push_back ({ lo, hi, std::sqrt (lo * hi) });
        }
    }
    else // thirdOctave or fullOctave
    {
        const float step = (specType == SpecType::fullOctave) ? 1.0f : 1.0f / 3.0f;
        const float edgeMul = std::pow (2.0f, step * 0.5f);

        for (int n = -20; n <= 20; ++n)
        {
            const float centre = 1000.0f * std::pow (2.0f, (float) n * step);
            if (centre < fLow || centre > nyq)
                continue;

            const float lo = juce::jmax (centre / edgeMul, fLow);
            const float hi = juce::jmin (centre * edgeMul, nyq);
            if (hi > lo)
                slots.push_back ({ lo, hi, centre });
        }
    }

    const size_t n = slots.size();
    dispLevel.assign     (n, 0.0f);
    dispAvg.assign       (n, 0.0f);
    dispPeak.assign      (n, 0.0f);
    referenceData.assign (n, 0.0f);
    hasReference = false;
    avgCount = 0;
}

void SpectrumDisplay::resetAveraging()
{
    avgCount = 0;
    std::fill (dispAvg.begin(),   dispAvg.end(),   0.0f);
    std::fill (dispPeak.begin(),  dispPeak.end(),  0.0f);
    std::fill (dispLevel.begin(), dispLevel.end(), 0.0f);
}

//==============================================================================
void SpectrumDisplay::setSpectrumType (SpecType t)
{
    if (t != specType) { specType = t; lastFftSize = 0; }   // force slot rebuild
}

void SpectrumDisplay::setChannel (Channel c)
{
    if (c != channel) { channel = c; resetAveraging(); }    // fresh curve for the new signal
}

void SpectrumDisplay::setWindowOrder (int order)
{
    order = juce::jlimit (9, 15, order);   // 512 .. 32768
    if (order != windowOrder)
    {
        windowOrder = order;
        rebuildFft();
    }
}

void SpectrumDisplay::setWindowType (WinType type)
{
    if (type != windowType)
    {
        windowType = type;
        fillWindowTable();
        resetAveraging();
    }
}

void SpectrumDisplay::setAverageSeconds (float seconds)
{
    if (! juce::approximatelyEqual (seconds, averageSeconds))
    {
        averageSeconds = seconds;
        resetAveraging();
    }
}

void SpectrumDisplay::setPeakHoldShown (bool shouldShow)
{
    showPeakHold = shouldShow;
    if (! shouldShow)
        std::fill (dispPeak.begin(), dispPeak.end(), 0.0f);
}

void SpectrumDisplay::setPeakHoldMs (float ms)          { peakHoldMs = ms; }
void SpectrumDisplay::setOverlapPercent (float percent) { overlapPercent = percent; }

juce::String SpectrumDisplay::getSettingsString() const
{
    juce::DynamicObject::Ptr o = new juce::DynamicObject();
    o->setProperty ("specType",    (int) specType);
    o->setProperty ("channel",     (int) channel);
    o->setProperty ("windowOrder", windowOrder);
    o->setProperty ("windowType",  (int) windowType);
    o->setProperty ("average",     averageSeconds);
    o->setProperty ("overlap",     overlapPercent);
    o->setProperty ("peakHoldMs",  peakHoldMs);
    o->setProperty ("peakShown",   showPeakHold);
    return juce::JSON::toString (juce::var (o.get()));
}

void SpectrumDisplay::applySettingsString (const juce::String& s)
{
    const auto v = juce::JSON::parse (s);
    auto* o = v.getDynamicObject();
    if (o == nullptr)
        return;

    // Guard the enum casts against out-of-range ints from a hand-edited/older
    // preset. setWindowOrder already clamps to 9..15.
    if (o->hasProperty ("specType"))
    {
        const int t = (int) o->getProperty ("specType");
        if (t >= 0 && t <= 3) setSpectrumType ((SpecType) t);
    }
    if (o->hasProperty ("channel"))
    {
        const int c = (int) o->getProperty ("channel");
        if (c >= 0 && c <= 1) setChannel ((Channel) c);
    }
    if (o->hasProperty ("windowOrder")) setWindowOrder ((int) o->getProperty ("windowOrder"));
    if (o->hasProperty ("windowType"))
    {
        const int t = (int) o->getProperty ("windowType");
        if (t >= 0 && t <= 4) setWindowType ((WinType) t);
    }
    if (o->hasProperty ("average"))     setAverageSeconds ((float) (double) o->getProperty ("average"));
    if (o->hasProperty ("overlap"))     setOverlapPercent ((float) (double) o->getProperty ("overlap"));
    if (o->hasProperty ("peakHoldMs"))  setPeakHoldMs     ((float) (double) o->getProperty ("peakHoldMs"));
    if (o->hasProperty ("peakShown"))   setPeakHoldShown  ((bool) o->getProperty ("peakShown"));
}

void SpectrumDisplay::freezeReference()
{
    referenceData = dispLevel;
    hasReference = true;
    repaint();
}

void SpectrumDisplay::clearReference()
{
    hasReference = false;
    repaint();
}

juce::String SpectrumDisplay::getExportText() const
{
    juce::String txt = "Frequency (Hz)\tLevel (dB)\n";
    for (size_t s = 0; s < slots.size(); ++s)
    {
        const float db = juce::jmap (juce::jlimit (0.0f, 1.0f, dispLevel[s]), 0.0f, 1.0f, mindB, maxdB);
        txt << juce::String (slots[s].centreHz, 2) << "\t" << juce::String (db, 2) << "\n";
    }
    return txt;
}

//==============================================================================
void SpectrumDisplay::timerCallback()
{
    computeFrame();

    if (recording)
    {
        if (recordFrameCounter <= 0)
        {
            captureRecordFrame();
            recordFrameCounter = recordIntervalSec * 60;   // ~ every N seconds at 60 fps
        }
        --recordFrameCounter;
    }

    repaint();
}

void SpectrumDisplay::setRecordIntervalSeconds (int seconds)
{
    recordIntervalSec = juce::jmax (1, seconds);
}

void SpectrumDisplay::startRecording()
{
    recordFreqs.clear();
    for (const auto& s : slots) recordFreqs.push_back (s.centreHz);
    recordFrames.clear();
    recordFrameCounter = 0;
    recording = true;
}

void SpectrumDisplay::stopRecording()
{
    recording = false;
}

void SpectrumDisplay::captureRecordFrame()
{
    std::vector<float> row;
    row.reserve (recordFreqs.size());
    for (size_t s = 0; s < recordFreqs.size(); ++s)
    {
        const float lvl = (s < dispLevel.size()) ? dispLevel[s] : 0.0f;
        row.push_back (juce::jmap (juce::jlimit (0.0f, 1.0f, lvl), 0.0f, 1.0f, mindB, maxdB));
    }
    recordFrames.push_back (std::move (row));
}

juce::String SpectrumDisplay::getRecordingText() const
{
    juce::String txt = "Time (s)";
    for (float f : recordFreqs) txt << "," << juce::String (f, 1);
    txt << "\n";

    for (size_t r = 0; r < recordFrames.size(); ++r)
    {
        txt << juce::String ((int) r * recordIntervalSec);
        for (float db : recordFrames[r]) txt << "," << juce::String (db, 1);
        txt << "\n";
    }
    return txt;
}

void SpectrumDisplay::computeFrame()
{
    if (channel == Channel::side)
        processorRef.copyLatestSide (sampleBuf.data());
    else
        processorRef.copyLatestSamples (sampleBuf.data());

    const double sr = processorRef.getSampleRate() > 0.0 ? processorRef.getSampleRate() : 44100.0;

    if (sr != lastSampleRate || fftSize != lastFftSize || specType != lastSpecType)
    {
        rebuildSlots (sr);
        lastSampleRate = sr;
        lastFftSize    = fftSize;
        lastSpecType   = specType;
    }

    if (slots.empty())
        return;

    const float binHz = (float) (sr / (double) fftSize);
    specSampleRate = (float) sr;
    specFMin = slots.front().loHz;
    specFMax = slots.back().hiHz;

    // Overlap-averaged magnitude spectrum: average N overlapping FFT frames.
    const float overlapFrac = juce::jlimit (0.0f, 0.95f, overlapPercent / 100.0f);
    const int   numFrames = juce::jlimit (1, 16, (int) std::round (1.0f / (1.0f - overlapFrac)));
    const int   hop = juce::jmax (1, (int) ((float) fftSize * (1.0f - overlapFrac)));

    std::fill (magSpec.begin(), magSpec.end(), 0.0f);
    int framesUsed = 0;

    for (int f = 0; f < numFrames; ++f)
    {
        const int start = maxFftSize - fftSize - f * hop;
        if (start < 0)
            break;

        std::copy (sampleBuf.data() + start, sampleBuf.data() + start + fftSize, fftBuf.begin());
        std::fill (fftBuf.begin() + fftSize, fftBuf.end(), 0.0f);
        juce::FloatVectorOperations::multiply (fftBuf.data(), windowTable.data(), fftSize);
        fft->performFrequencyOnlyForwardTransform (fftBuf.data());

        for (int k = 0; k <= fftSize / 2; ++k)
            magSpec[(size_t) k] += fftBuf[(size_t) k];

        ++framesUsed;
    }

    if (framesUsed > 1)
        juce::FloatVectorOperations::multiply (magSpec.data(), 1.0f / (float) framesUsed, (int) magSpec.size());

    const float normDb = juce::Decibels::gainToDecibels ((float) fftSize);

    const bool  infinite = averageSeconds < 0.0f;
    const bool  timed    = averageSeconds > 0.0f;
    const float alpha    = timed ? 1.0f / (averageSeconds * 60.0f) : 1.0f;
    if (infinite) ++avgCount;

    const float peakDecay = (peakHoldMs > 0.0f) ? (1000.0f / 60.0f) / peakHoldMs : 0.0f;

    for (size_t s = 0; s < slots.size(); ++s)
    {
        // Aggregate the magnitude across the slot's bin range (mean).
        const int binLo = juce::jlimit (1, fftSize / 2, (int) std::floor (slots[s].loHz / binHz));
        const int binHi = juce::jlimit (1, fftSize / 2, (int) std::ceil  (slots[s].hiHz / binHz));

        float mag = 0.0f;
        int   cnt = 0;
        for (int b = binLo; b <= binHi; ++b) { mag += magSpec[(size_t) b]; ++cnt; }
        mag = (cnt > 0) ? mag / (float) cnt : 0.0f;

        float displayMag = mag;
        if (infinite)
        {
            dispAvg[s] += (mag - dispAvg[s]) / (float) juce::jmax (1LL, avgCount);
            displayMag = dispAvg[s];
        }
        else if (timed)
        {
            dispAvg[s] += alpha * (mag - dispAvg[s]);
            displayMag = dispAvg[s];
        }

        const float db    = juce::Decibels::gainToDecibels (displayMag) - normDb;
        const float level = juce::jmap (juce::jlimit (mindB, maxdB, db), mindB, maxdB, 0.0f, 1.0f);

        if (averageSeconds == 0.0f)
        {
            const float prev = dispLevel[s];
            dispLevel[s] = level > prev ? level : prev * 0.82f + level * 0.18f;
        }
        else
        {
            dispLevel[s] = level;
        }

        if (showPeakHold)
        {
            if (dispLevel[s] >= dispPeak[s]) dispPeak[s] = dispLevel[s];
            else                             dispPeak[s] = juce::jmax (dispLevel[s], dispPeak[s] - peakDecay);
        }
    }
}

//==============================================================================
juce::Rectangle<float> SpectrumDisplay::plotBounds() const
{
    auto area = getLocalBounds().toFloat();
    const float leftGutter   = 34.0f;
    const float bottomGutter = 16.0f;
    return { area.getX() + leftGutter, area.getY() + 6.0f,
             juce::jmax (1.0f, area.getWidth() - leftGutter - 8.0f),
             juce::jmax (1.0f, area.getHeight() - bottomGutter - 8.0f) };
}

float SpectrumDisplay::freqToX (float freq, juce::Rectangle<float> plot) const
{
    if (specFMax <= specFMin) return plot.getX();
    const float frac = std::log (juce::jmax (1.0f, freq) / specFMin) / std::log (specFMax / specFMin);
    return plot.getX() + juce::jlimit (0.0f, 1.0f, frac) * plot.getWidth();
}

float SpectrumDisplay::xToFreq (float x, juce::Rectangle<float> plot) const
{
    if (specFMax <= specFMin) return specFMin;
    const float frac = juce::jlimit (0.0f, 1.0f, (x - plot.getX()) / plot.getWidth());
    return specFMin * std::exp (std::log (specFMax / specFMin) * frac);
}

float SpectrumDisplay::levelToDb (float level) const
{
    return juce::jmap (level, 0.0f, 1.0f, mindB, maxdB);
}

//==============================================================================
void SpectrumDisplay::paint (juce::Graphics& g)
{
    auto area = getLocalBounds().toFloat();

    g.setColour (MixColours::surface);
    g.fillRoundedRectangle (area, 8.0f);
    g.setColour (MixColours::border);
    g.drawRoundedRectangle (area, 8.0f, 1.0f);

    auto plot = plotBounds();
    g.setFont (MixLookAndFeel::monoFont (9.5f));

    // dB grid + labels.
    for (int db = 0; db >= -100; db -= 20)
    {
        const float y = juce::jmap ((float) db, 0.0f, -100.0f, plot.getY(), plot.getBottom());
        g.setColour (MixColours::border.withAlpha (0.6f));
        g.drawHorizontalLine ((int) y, plot.getX(), plot.getRight());
        g.setColour (MixColours::textDim);
        g.drawText (juce::String (db),
                    juce::Rectangle<float> (area.getX() + 2.0f, y - 6.0f, 27.0f, 12.0f),
                    juce::Justification::centredRight);
    }

    // Frequency grid + labels.
    const int freqs[] = { 20, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000 };
    float lastLabelX = -1000.0f;
    for (int f : freqs)
    {
        const float x = freqToX ((float) f, plot);
        if (x < plot.getX() - 0.5f || x > plot.getRight() + 0.5f)
            continue;

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

    if (slots.empty())
        return;

    auto levelToY = [&plot] (float level)
    {
        return juce::jmap (juce::jlimit (0.0f, 1.0f, level), 0.0f, 1.0f, plot.getBottom(), plot.getY());
    };

    if (specType == SpecType::linear)
    {
        // Raw line + fill, decimated to one point per pixel column (max).
        std::vector<juce::Point<float>> pts;
        pts.reserve ((size_t) juce::jmax (1.0f, plot.getWidth()) + 4);

        auto build = [this, &plot, &levelToY, &pts] (const std::vector<float>& data)
        {
            pts.clear();
            int lastPx = INT_MIN; float colMax = 0.0f;
            for (size_t s = 0; s < slots.size(); ++s)
            {
                const int px = (int) std::round (freqToX (slots[s].centreHz, plot));
                if (px != lastPx)
                {
                    if (lastPx != INT_MIN) pts.push_back ({ (float) lastPx, levelToY (colMax) });
                    lastPx = px; colMax = data[s];
                }
                else colMax = juce::jmax (colMax, data[s]);
            }
            if (lastPx != INT_MIN) pts.push_back ({ (float) lastPx, levelToY (colMax) });
        };

        if (hasReference)
        {
            build (referenceData);
            if (! pts.empty())
            {
                juce::Path r; r.startNewSubPath (pts[0]);
                for (size_t i = 1; i < pts.size(); ++i) r.lineTo (pts[i]);
                g.setColour (MixColours::textDim.withAlpha (0.7f));
                g.strokePath (r, juce::PathStrokeType (1.0f));
            }
        }

        build (dispLevel);
        if (! pts.empty())
        {
            juce::Path curve; curve.startNewSubPath (pts[0].x, plot.getBottom());
            for (const auto& pt : pts) curve.lineTo (pt);
            curve.lineTo (pts.back().x, plot.getBottom()); curve.closeSubPath();
            juce::ColourGradient grad (MixColours::accent.withAlpha (0.85f), plot.getX(), plot.getY(),
                                       MixColours::accent.withAlpha (0.12f), plot.getX(), plot.getBottom(), false);
            g.setGradientFill (grad); g.fillPath (curve);

            juce::Path line; line.startNewSubPath (pts[0]);
            for (size_t i = 1; i < pts.size(); ++i) line.lineTo (pts[i]);
            g.setColour (MixColours::accentH); g.strokePath (line, juce::PathStrokeType (1.5f));
        }

        if (showPeakHold)
        {
            build (dispPeak);
            if (! pts.empty())
            {
                juce::Path pk; pk.startNewSubPath (pts[0]);
                for (size_t i = 1; i < pts.size(); ++i) pk.lineTo (pts[i]);
                g.setColour (juce::Colours::white.withAlpha (0.65f));
                g.strokePath (pk, juce::PathStrokeType (1.0f));
            }
        }
    }
    else
    {
        // Octave / critical bands: clean filled bars.
        for (size_t s = 0; s < slots.size(); ++s)
        {
            const float x1 = freqToX (slots[s].loHz, plot) + 0.5f;
            const float x2 = freqToX (slots[s].hiHz, plot) - 0.5f;
            const float w  = juce::jmax (1.0f, x2 - x1);
            const float y  = levelToY (dispLevel[s]);

            juce::ColourGradient grad (MixColours::accent.withAlpha (0.9f), x1, plot.getY(),
                                       MixColours::accent.withAlpha (0.25f), x1, plot.getBottom(), false);
            g.setGradientFill (grad);
            g.fillRect (juce::Rectangle<float> (x1, y, w, plot.getBottom() - y));

            g.setColour (MixColours::accentH);
            g.fillRect (juce::Rectangle<float> (x1, y, w, 2.0f));

            if (showPeakHold)
            {
                const float py = levelToY (dispPeak[s]);
                g.setColour (juce::Colours::white.withAlpha (0.7f));
                g.fillRect (juce::Rectangle<float> (x1, py, w, 1.5f));
            }
        }

        if (hasReference)
        {
            juce::Path r; bool started = false;
            for (size_t s = 0; s < slots.size(); ++s)
            {
                const float x = freqToX (slots[s].centreHz, plot), y = levelToY (referenceData[s]);
                if (! started) { r.startNewSubPath (x, y); started = true; }
                else r.lineTo (x, y);
            }
            g.setColour (MixColours::textDim.withAlpha (0.8f));
            g.strokePath (r, juce::PathStrokeType (1.0f));
        }
    }

    // Recording indicator.
    if (recording)
    {
        g.setColour (juce::Colour (0xffef4444));
        g.fillEllipse (plot.getRight() - 60.0f, plot.getY() + 6.0f, 8.0f, 8.0f);
        g.setFont (MixLookAndFeel::monoFont (11.0f));
        g.drawText ("REC " + juce::String (getRecordedCount() * recordIntervalSec) + "s",
                    juce::Rectangle<float> (plot.getRight() - 48.0f, plot.getY() + 3.0f, 46.0f, 14.0f),
                    juce::Justification::centredLeft);
    }

    // Hover readout.
    if (hovering && plot.contains (mousePos))
    {
        const float freq = xToFreq (mousePos.x, plot);

        // Nearest slot by centre frequency.
        size_t best = 0; float bestD = 1.0e30f;
        for (size_t s = 0; s < slots.size(); ++s)
        {
            const float d = std::abs (slots[s].centreHz - freq);
            if (d < bestD) { bestD = d; best = s; }
        }
        const float db = levelToDb (dispLevel[best]);

        g.setColour (MixColours::accentH.withAlpha (0.5f));
        g.drawVerticalLine ((int) mousePos.x, plot.getY(), plot.getBottom());

        const juce::String freqStr = freq >= 1000.0f
            ? juce::String (freq / 1000.0f, 2) + " kHz"
            : juce::String (juce::roundToInt (freq)) + " Hz";
        const juce::String txt = freqStr + "   " + juce::String (db, 1) + " dB";

        g.setFont (MixLookAndFeel::monoFont (11.0f));
        const float tw = 128.0f, th = 18.0f;
        float tx = mousePos.x + 8.0f;
        if (tx + tw > plot.getRight()) tx = mousePos.x - 8.0f - tw;
        juce::Rectangle<float> box (tx, plot.getY() + 4.0f, tw, th);

        g.setColour (MixColours::bg.withAlpha (0.85f));
        g.fillRoundedRectangle (box, 4.0f);
        g.setColour (MixColours::border);
        g.drawRoundedRectangle (box, 4.0f, 1.0f);
        g.setColour (MixColours::text);
        g.drawText (txt, box, juce::Justification::centred);
    }
}

void SpectrumDisplay::mouseMove (const juce::MouseEvent& e)
{
    hovering = true;
    mousePos = e.position;
}

void SpectrumDisplay::mouseExit (const juce::MouseEvent&)
{
    hovering = false;
}
