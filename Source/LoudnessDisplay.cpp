#include "LoudnessDisplay.h"
#include <cmath>

//==============================================================================
namespace
{
    // A reading at or below this counts as "no signal" (silence / not measured).
    constexpr float kNoSignal = -100.0f;

    juce::String fmtLufs (float v)
    {
        return v <= kNoSignal ? juce::String ("-inf") : juce::String (v, 1);
    }
}

//==============================================================================
LoudnessDisplay::LoudnessDisplay (MixAnalyzerAudioProcessor& p)
    : proc (p)
{
    shortHistory.reserve (maxHistory);
    momHistory.reserve (maxHistory);
    startTimerHz (30);
}

void LoudnessDisplay::clearHistory()
{
    shortHistory.clear();
    momHistory.clear();
    mLufs = sLufs = iLufs = tp = -300.0f;
    lra = 0.0f;

    // If a recording is running when the meter resets (new song / stop / restart),
    // restart its capture too so the exported CSV's time axis matches the graph.
    recordFrames.clear();
    recordFrameCounter = 0;

    repaint();
}

juce::String LoudnessDisplay::metaHeader() const
{
    const double s0 = proc.getSelectionStartSeconds();
    const double s1 = proc.getSelectionEndSeconds();
    const juce::String section = (s1 > s0) ? juce::String (s0, 2) + " - " + juce::String (s1, 2) + " s"
                                           : juce::String ("whole song");
    const juce::String file = proc.getLoadedFileName().isNotEmpty()
                                  ? proc.getLoadedFileName() : juce::String ("(none)");
    juce::String h;
    h << "# Source: "  << file << "\r\n";
    h << "# Section: " << section << "\r\n";
    h << "# Values are absolute LUFS / LU / dBTP (the Relative display scale does not change this file)\r\n";
    return h;
}

juce::String LoudnessDisplay::getSnapshotText() const
{
    juce::String t;
    t << "# EERIE - Mix Analyzer  |  Loudness (EBU R128 / BS.1770)\r\n";
    t << metaHeader();
    t << "Momentary\t"      << fmtLufs (mLufs) << " LUFS\r\n";
    t << "Short-term\t"     << fmtLufs (sLufs) << " LUFS\r\n";
    t << "Integrated\t"     << fmtLufs (iLufs) << " LUFS\r\n";
    t << "Loudness Range\t" << juce::String (lra, 1) << " LU\r\n";
    t << "True Peak\t"      << (tp <= kNoSignal ? juce::String ("-inf")
                                                : juce::String (tp, 1)) << " dBTP\r\n";
    return t;
}

//==============================================================================
juce::String LoudnessDisplay::getSettingsString() const
{
    juce::DynamicObject::Ptr o = new juce::DynamicObject();
    o->setProperty ("range", (int) rangeScale);
    o->setProperty ("scale", (int) scaleMode);
    o->setProperty ("gate",  (int) gateMode);
    o->setProperty ("view",  (int) viewMode);
    o->setProperty ("peak",  peakTarget);
    o->setProperty ("loud",  loudnessTarget);
    o->setProperty ("lra",   lraTarget);
    return juce::JSON::toString (juce::var (o.get()));
}

void LoudnessDisplay::applySettingsString (const juce::String& s)
{
    const auto v = juce::JSON::parse (s);
    auto* o = v.getDynamicObject();
    if (o == nullptr)
        return;

    // Validate everything: a hand-edited or older-format preset must not push an
    // enum out of range (that would feed a default-less switch) or a target past
    // the slider limits. Out-of-range / missing values keep the current setting.
    auto readEnum = [o] (const char* key, int lo, int hi, int fallback)
    {
        if (! o->hasProperty (key)) return fallback;
        const int v = (int) o->getProperty (key);
        return (v >= lo && v <= hi) ? v : fallback;
    };

    rangeScale = (RangeScale) readEnum ("range", 0, 4, (int) rangeScale);
    scaleMode  = (ScaleMode)  readEnum ("scale", 0, 1, (int) scaleMode);
    gateMode   = (GateMode)   readEnum ("gate",  0, 2, (int) gateMode);
    viewMode   = (ViewMode)   readEnum ("view",  0, 1, (int) viewMode);

    if (o->hasProperty ("peak")) peakTarget     = juce::jlimit (-9.0f,   0.0f, (float) (double) o->getProperty ("peak"));
    if (o->hasProperty ("loud")) loudnessTarget = juce::jlimit (-40.0f,  0.0f, (float) (double) o->getProperty ("loud"));
    if (o->hasProperty ("lra"))  lraTarget      = juce::jlimit (  0.0f, 30.0f, (float) (double) o->getProperty ("lra"));
    repaint();
}

//==============================================================================
void LoudnessDisplay::getGraphWindow (float& topLufs, float& botLufs) const
{
    switch (rangeScale)
    {
        case RangeScale::dbNonLinear:  topLufs = 0.0f;                botLufs = -60.0f;                break;
        case RangeScale::bs1771:       topLufs = loudnessTarget + 9;  botLufs = loudnessTarget - 18;   break;
        case RangeScale::ebuPlus9:     topLufs = loudnessTarget + 9;  botLufs = loudnessTarget - 18;   break;
        case RangeScale::ebuPlus18:    topLufs = loudnessTarget + 18; botLufs = loudnessTarget - 36;   break;
        case RangeScale::dbLinear:
        default:                       topLufs = 0.0f;                botLufs = -42.0f;                break;
    }
}

float LoudnessDisplay::toDisplay (float lufs) const
{
    if (lufs <= kNoSignal) return lufs;   // keep the "no signal" sentinel
    return scaleMode == ScaleMode::relative ? lufs - loudnessTarget : lufs;
}

void LoudnessDisplay::timerCallback()
{
    if (! isVisible())   // skip the EBU R128 queries when this meter is switched off
        return;

    proc.updateLoudnessReadings();   // do the heavy EBU R128 queries here, not on the audio thread

    mLufs = proc.getMomentaryLufs();
    sLufs = proc.getShortTermLufs();
    iLufs = proc.getIntegratedLufs();
    lra   = proc.getLoudnessRangeLU();
    tp    = proc.getTruePeakDb();

    shortHistory.push_back (sLufs);
    momHistory.push_back   (mLufs);
    if ((int) shortHistory.size() > maxHistory) shortHistory.erase (shortHistory.begin());
    if ((int) momHistory.size()   > maxHistory) momHistory.erase   (momHistory.begin());

    if (recording)
    {
        if (recordFrameCounter <= 0)
        {
            captureRecordFrame();
            recordFrameCounter = recordIntervalSec * 30;   // ~ every N seconds at 30 fps
        }
        --recordFrameCounter;
    }

    repaint();
}

//==============================================================================
void LoudnessDisplay::setRecordIntervalSeconds (int seconds)
{
    recordIntervalSec = juce::jmax (1, seconds);
}

void LoudnessDisplay::startRecording()
{
    recordFrames.clear();
    recordFrameCounter = 0;
    recording = true;
}

void LoudnessDisplay::stopRecording()
{
    recording = false;
}

void LoudnessDisplay::captureRecordFrame()
{
    recordFrames.push_back ({ mLufs, sLufs, iLufs, lra, tp });
}

juce::String LoudnessDisplay::getRecordingText() const
{
    auto cell = [] (float v) { return v <= kNoSignal ? juce::String ("-inf") : juce::String (v, 1); };

    juce::String txt;
    txt << "# EERIE - Mix Analyzer  |  Loudness recorded over time\r\n";
    txt << metaHeader();
    txt << "# Interval: every " << recordIntervalSec << " s\r\n";
    txt << "Time (s),Momentary (LUFS),Short-term (LUFS),"
           "Integrated (LUFS),Range (LU),True Peak (dBTP)\r\n";

    for (size_t r = 0; r < recordFrames.size(); ++r)
    {
        const auto& f = recordFrames[r];
        txt << juce::String ((int) r * recordIntervalSec) << ","
            << cell (f.m) << "," << cell (f.s) << "," << cell (f.i) << ","
            << juce::String (f.lra, 1) << "," << cell (f.tp) << "\r\n";
    }
    return txt;
}

//==============================================================================
void LoudnessDisplay::resized()
{
    auto b = getLocalBounds();
    readoutArea = b.removeFromTop (50);
    b.removeFromTop (6);
    graphArea = b;
}

void LoudnessDisplay::paint (juce::Graphics& g)
{
    const bool relative = (scaleMode == ScaleMode::relative);
    const char* loudUnit = relative ? "LU" : "LUFS";
    const juce::Colour red   { 0xffef4444 };
    const juce::Colour amber { 0xfff59e0b };

    // ---- Readout cards -----------------------------------------------------
    struct Card { const char* label; juce::String value; const char* unit; juce::Colour col; };

    const bool tpHot  = tp  > peakTarget && tp > kNoSignal;         // over the peak target
    const bool lraHot = lraTarget > 0.0f && lra > lraTarget;        // over the range target

    Card cards[5] =
    {
        { "MOMENTARY",  fmtLufs (toDisplay (mLufs)), loudUnit, MixColours::text    },
        { "SHORT-TERM", fmtLufs (toDisplay (sLufs)), loudUnit, MixColours::accentH },
        { "INTEGRATED", fmtLufs (toDisplay (iLufs)), loudUnit, MixColours::accent  },
        { "RANGE",      juce::String (lra, 1),       "LU",     lraHot ? amber : MixColours::text },
        { "TRUE PEAK",  (tp <= kNoSignal ? juce::String ("-inf") : juce::String (tp, 1)),
                                                     "dBTP",   tpHot ? red : MixColours::text },
    };

    auto row = readoutArea;
    const int cardW = row.getWidth() / 5;

    for (int c = 0; c < 5; ++c)
    {
        auto cell = (c < 4 ? row.removeFromLeft (cardW) : row).reduced (4, 0);

        g.setColour (MixColours::surface);
        g.fillRoundedRectangle (cell.toFloat(), 6.0f);
        g.setColour (MixColours::border);
        g.drawRoundedRectangle (cell.toFloat().reduced (0.5f), 6.0f, 1.0f);

        auto inner = cell.reduced (10, 4);

        g.setColour (MixColours::textDim);
        g.setFont (MixLookAndFeel::uiFont (10.5f, true));
        g.drawText (cards[c].label, inner.removeFromTop (13),
                    juce::Justification::centredLeft);

        auto valLine = inner;
        g.setColour (cards[c].col);
        g.setFont (MixLookAndFeel::monoFont (22.0f));
        const int vw = juce::GlyphArrangement::getStringWidthInt (
                           MixLookAndFeel::monoFont (22.0f), cards[c].value);
        g.drawText (cards[c].value, valLine.removeFromLeft (vw + 5),
                    juce::Justification::centredLeft);

        g.setColour (MixColours::textDim);
        g.setFont (MixLookAndFeel::uiFont (10.5f));
        g.drawText (cards[c].unit, valLine, juce::Justification::centredLeft);
    }

    // ---- History graph -----------------------------------------------------
    g.setColour (MixColours::surface);
    g.fillRoundedRectangle (graphArea.toFloat(), 6.0f);
    g.setColour (MixColours::border);
    g.drawRoundedRectangle (graphArea.toFloat().reduced (0.5f), 6.0f, 1.0f);

    auto plot = graphArea.reduced (1);
    auto gutter = plot.removeFromLeft (40);   // scale labels down the left

    juce::Rectangle<int> barValueRow, barLabelRow;
    if (viewMode == ViewMode::bars)
    {
        barValueRow = plot.removeFromTop (17);      // the value above each bar
        barLabelRow = plot.removeFromBottom (16);   // the name below each bar
    }

    float winTop, winBot;
    getGraphWindow (winTop, winBot);

    auto valToY = [plot, winTop, winBot] (float v)
    {
        v = juce::jlimit (winBot, winTop, v);
        return juce::jmap (v, winBot, winTop,
                           (float) plot.getBottom(), (float) plot.getY());
    };

    // Gridlines + scale labels every 6 units. Labels follow the chosen scale.
    g.setFont (MixLookAndFeel::uiFont (10.0f));
    const float start = std::floor (winTop / 6.0f) * 6.0f;
    for (float v = start; v >= winBot; v -= 6.0f)
    {
        const float y = valToY (v);
        g.setColour (MixColours::border.withAlpha (0.6f));
        g.drawHorizontalLine ((int) y, (float) plot.getX(), (float) plot.getRight());

        const int shown = (int) juce::roundToInt (relative ? v - loudnessTarget : v);
        g.setColour (MixColours::textDim);
        g.drawText (juce::String (shown),
                    juce::Rectangle<int> (gutter.getX(), (int) y - 7, gutter.getWidth() - 5, 14),
                    juce::Justification::centredRight);
    }

    // The loudness target line.
    if (loudnessTarget <= winTop && loudnessTarget >= winBot)
    {
        const float ty = valToY (loudnessTarget);
        g.setColour (MixColours::accent.withAlpha (0.75f));
        for (float x = (float) plot.getX(); x < (float) plot.getRight(); x += 8.0f)
            g.drawHorizontalLine ((int) ty, x, juce::jmin (x + 4.0f, (float) plot.getRight()));
    }

    if (viewMode == ViewMode::timeline)
    {
        // Scrolling lines of short-term (bright) + momentary (dim), skipping gaps.
        auto buildPath = [this, plot, &valToY] (const std::vector<float>& hist)
        {
            juce::Path p;
            bool started = false;
            const float dx = plot.getWidth() / (float) (maxHistory - 1);
            for (int k = 0; k < (int) hist.size(); ++k)
            {
                const float v = hist[k];
                if (v <= kNoSignal) { started = false; continue; }
                const float x = plot.getX() + k * dx;
                const float y = valToY (v);
                if (! started) { p.startNewSubPath (x, y); started = true; }
                else            p.lineTo (x, y);
            }
            return p;
        };

        juce::Graphics::ScopedSaveState clip (g);
        g.reduceClipRegion (plot);

        g.setColour (MixColours::textDim.withAlpha (0.55f));
        g.strokePath (buildPath (momHistory), juce::PathStrokeType (1.0f));
        g.setColour (MixColours::accentH);
        g.strokePath (buildPath (shortHistory), juce::PathStrokeType (1.6f));
    }
    else
    {
        // Vertical bar meters: Momentary / Short-term / Integrated, colours match
        // the readout cards. Each bar fills from its current value down to 0, with
        // the reading above it and the name below it so every bar is self-evident.
        struct Bar { float v; juce::Colour col; const char* name; };
        const Bar bars[3] =
        {
            { mLufs, MixColours::text,    "MOMENTARY"  },
            { sLufs, MixColours::accentH, "SHORT-TERM" },
            { iLufs, MixColours::accent,  "INTEGRATED" },
        };

        const float slotW  = plot.getWidth() / 3.0f;
        const float barW   = juce::jmin (54.0f, slotW * 0.5f);
        const float bottom = (float) plot.getBottom();

        for (int bi = 0; bi < 3; ++bi)
        {
            const float cx = plot.getX() + slotW * ((float) bi + 0.5f);
            const juce::Rectangle<int> col ((int) (cx - slotW * 0.5f), 0, (int) slotW, 0);
            juce::Rectangle<float> track (cx - barW * 0.5f, (float) plot.getY(), barW, (float) plot.getHeight());

            g.setColour (MixColours::surface2);
            g.fillRoundedRectangle (track, 3.0f);

            if (bars[bi].v > kNoSignal)
            {
                const float topY = valToY (bars[bi].v);
                g.setColour (bars[bi].col);
                g.fillRoundedRectangle (juce::Rectangle<float> (track.getX(), topY, barW, bottom - topY), 3.0f);
            }

            // Value above the bar (follows the Absolute / Relative scale).
            g.setColour (bars[bi].col);
            g.setFont (MixLookAndFeel::monoFont (13.0f));
            g.drawText (fmtLufs (toDisplay (bars[bi].v)),
                        col.withY (barValueRow.getY()).withHeight (barValueRow.getHeight()),
                        juce::Justification::centred);

            // Name below the bar.
            g.setColour (MixColours::textDim);
            g.setFont (MixLookAndFeel::uiFont (10.5f, true));
            g.drawText (bars[bi].name,
                        col.withY (barLabelRow.getY()).withHeight (barLabelRow.getHeight()),
                        juce::Justification::centred);
        }
    }

    if (recording)
    {
        auto tag = juce::Rectangle<int> (plot.getRight() - 92, plot.getY() + 6, 86, 16);
        g.setColour (juce::Colour (0xffef4444));
        g.fillEllipse ((float) tag.getX(), (float) tag.getY() + 4.0f, 8.0f, 8.0f);
        g.setFont (MixLookAndFeel::uiFont (11.0f, true));
        const int elapsed = juce::jmax (0, getRecordedCount() - 1) * recordIntervalSec;
        g.drawText ("REC " + juce::String (elapsed) + "s",
                    tag.withTrimmedLeft (13), juce::Justification::centredLeft);
    }
}
