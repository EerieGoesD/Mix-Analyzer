#include "SoundFieldDisplay.h"
#include <cmath>

//==============================================================================
namespace
{
    juce::String fmtBalance (float b)   // -1 left .. +1 right
    {
        const int pct = juce::roundToInt (std::abs (b) * 100.0f);
        if (pct <= 1)  return "Centre";
        return (b < 0.0f ? "L " : "R ") + juce::String (pct) + "%";
    }
}

//==============================================================================
SoundFieldDisplay::SoundFieldDisplay (MixAnalyzerAudioProcessor& p)
    : proc (p)
{
    bufL.resize (drawN, 0.0f);
    bufR.resize (drawN, 0.0f);
    envBins.assign (numBins, 0.0f);
    startTimerHz (40);
}

void SoundFieldDisplay::clearHistory()
{
    correlation = balance = width = 0.0f;
    haveSignal = false;
    recordFrames.clear();
    recordFrameCounter = 0;
    scopeImg = juce::Image();                    // wipe the phosphor trail
    std::fill (envBins.begin(), envBins.end(), 0.0f);
    repaint();
}

juce::String SoundFieldDisplay::getSnapshotText() const
{
    juce::String t;
    t << "EERIE - Mix Analyzer  |  Sound Field\r\n";
    t << "Correlation\t" << juce::String (correlation, 2) << "\r\n";
    t << "Balance\t"     << fmtBalance (balance) << "\r\n";
    t << "Width\t"       << juce::String (width, 2) << "\r\n";
    return t;
}

//==============================================================================
void SoundFieldDisplay::setRecordIntervalSeconds (int seconds)  { recordIntervalSec = juce::jmax (1, seconds); }
void SoundFieldDisplay::startRecording()  { recordFrames.clear(); recordFrameCounter = 0; recording = true; }
void SoundFieldDisplay::stopRecording()   { recording = false; }

void SoundFieldDisplay::captureRecordFrame()
{
    recordFrames.push_back ({ correlation, balance, width });
}

juce::String SoundFieldDisplay::getRecordingText() const
{
    juce::String txt = "Time (s),Correlation,Balance,Width\n";
    for (size_t r = 0; r < recordFrames.size(); ++r)
    {
        const auto& f = recordFrames[r];
        txt << juce::String ((int) r * recordIntervalSec) << ","
            << juce::String (f.corr, 3) << ","
            << juce::String (f.bal, 3)  << ","
            << juce::String (f.wid, 3)  << "\n";
    }
    return txt;
}

//==============================================================================
juce::String SoundFieldDisplay::getSettingsString() const
{
    juce::DynamicObject::Ptr o = new juce::DynamicObject();
    o->setProperty ("vector", (int) vectorMode);
    o->setProperty ("detect", (int) detectMethod);
    return juce::JSON::toString (juce::var (o.get()));
}

void SoundFieldDisplay::applySettingsString (const juce::String& s)
{
    const auto v = juce::JSON::parse (s);
    auto* o = v.getDynamicObject();
    if (o == nullptr)
        return;

    auto readEnum = [o] (const char* key, int lo, int hi, int fallback)
    {
        if (! o->hasProperty (key)) return fallback;
        const int val = (int) o->getProperty (key);
        return (val >= lo && val <= hi) ? val : fallback;
    };

    vectorMode   = (VectorMode)   readEnum ("vector", 0, 2, (int) vectorMode);
    detectMethod = (DetectMethod) readEnum ("detect", 0, 2, (int) detectMethod);
    repaint();
}

//==============================================================================
void SoundFieldDisplay::timerCallback()
{
    proc.copyLatestStereo (bufL.data(), bufR.data(), drawN);

    double sLL = 0.0, sRR = 0.0, sLR = 0.0, sMid = 0.0, sSide = 0.0;
    for (int i = 0; i < drawN; ++i)
    {
        const float l = bufL[i], r = bufR[i];
        sLL += (double) l * l;
        sRR += (double) r * r;
        sLR += (double) l * r;
        const float mid  = 0.5f * (l + r);
        const float side = 0.5f * (l - r);
        sMid  += (double) mid * mid;
        sSide += (double) side * side;
    }

    haveSignal = (sLL + sRR) > 1.0e-7;

    float rawCorr = 0.0f;
    const double denom = std::sqrt (sLL * sRR);
    if (denom > 1.0e-9) rawCorr = (float) (sLR / denom);

    const float rmsL = (float) std::sqrt (sLL / drawN);
    const float rmsR = (float) std::sqrt (sRR / drawN);
    const float rawBal = (rmsL + rmsR > 1.0e-6f) ? (rmsR - rmsL) / (rmsR + rmsL) : 0.0f;

    const float rawWidth = (sMid > 1.0e-9) ? (float) std::sqrt (sSide / sMid) : 0.0f;

    // Readouts settle at a fixed, readable rate. (Detection Method affects the
    // Polar Level rays only, per the vectorscope spec - handled in paint.)
    const float a = 0.3f;
    correlation += (rawCorr  - correlation) * a;
    balance     += (rawBal   - balance)     * a;
    width       += (rawWidth - width)       * a;

    if (recording)
    {
        if (recordFrameCounter <= 0)
        {
            captureRecordFrame();
            recordFrameCounter = recordIntervalSec * 40;   // ~ every N seconds at 40 fps
        }
        --recordFrameCounter;
    }

    repaint();
}

//==============================================================================
void SoundFieldDisplay::resized()
{
    auto b = getLocalBounds();
    readoutArea = b.removeFromTop (50);
    b.removeFromTop (6);
    scopeArea = b;
}

void SoundFieldDisplay::paint (juce::Graphics& g)
{
    const juce::Colour red { 0xffef4444 };

    // ---- Readout cards -----------------------------------------------------
    struct Card { const char* label; juce::String value; juce::Colour col; };
    const Card cards[3] =
    {
        { "CORRELATION", juce::String (correlation, 2),
          correlation < 0.0f ? red : MixColours::accentH },
        { "BALANCE",     fmtBalance (balance), MixColours::text },
        { "WIDTH",       juce::String (width, 2), MixColours::text },
    };

    auto row = readoutArea;
    const int cardW = row.getWidth() / 3;
    for (int c = 0; c < 3; ++c)
    {
        auto cell = (c < 2 ? row.removeFromLeft (cardW) : row).reduced (4, 0);
        g.setColour (MixColours::surface);
        g.fillRoundedRectangle (cell.toFloat(), 6.0f);
        g.setColour (MixColours::border);
        g.drawRoundedRectangle (cell.toFloat().reduced (0.5f), 6.0f, 1.0f);

        auto inner = cell.reduced (12, 4);
        g.setColour (MixColours::textDim);
        g.setFont (MixLookAndFeel::uiFont (10.5f, true));
        g.drawText (cards[c].label, inner.removeFromTop (13), juce::Justification::centredLeft);
        g.setColour (cards[c].col);
        g.setFont (MixLookAndFeel::monoFont (22.0f));
        g.drawText (cards[c].value, inner, juce::Justification::centredLeft);
    }

    // ---- Scope panel -------------------------------------------------------
    g.setColour (MixColours::surface);
    g.fillRoundedRectangle (scopeArea.toFloat(), 6.0f);
    g.setColour (MixColours::border);
    g.drawRoundedRectangle (scopeArea.toFloat().reduced (0.5f), 6.0f, 1.0f);

    auto panel = scopeArea.reduced (10);
    auto corrBar = panel.removeFromBottom (24);   // correlation meter under the scope

    // Square scope area, centred.
    const int side = juce::jmin (panel.getWidth(), panel.getHeight());
    juce::Rectangle<float> sc ((float) (panel.getCentreX() - side / 2),
                               (float) panel.getY(), (float) side, (float) side);
    const float cx = sc.getCentreX();
    const float cy = sc.getCentreY();
    const float R  = side * 0.46f;
    const float k  = 0.70710678f;   // 45-degree rotation: mono -> vertical, L up-left, R up-right
    using MC = juce::MathConstants<float>;

    const bool polar = (vectorMode != VectorMode::lissajous);

    // ---- Guides ----
    if (polar)
    {
        auto semi = [cx, cy] (juce::Graphics& gg, float rad)
        {
            juce::Path a;
            a.addCentredArc (cx, cy, rad, rad, 0.0f, -MC::halfPi, MC::halfPi, true);
            gg.strokePath (a, juce::PathStrokeType (1.0f));
        };
        g.setColour (MixColours::border.withAlpha (0.7f));
        semi (g, R);
        g.drawLine (cx - R, cy, cx + R, cy, 1.0f);
        g.setColour (MixColours::border.withAlpha (0.28f));
        semi (g, R * 0.66f);
        semi (g, R * 0.33f);
        for (int deg = 0; deg <= 180; deg += 30)
        {
            const float th = juce::degreesToRadians ((float) deg);
            g.drawLine (cx, cy, cx + std::cos (th) * R, cy - std::sin (th) * R, 1.0f);
        }
    }
    else
    {
        g.setColour (MixColours::border.withAlpha (0.7f));
        g.drawEllipse (cx - R, cy - R, R * 2.0f, R * 2.0f, 1.0f);
        g.setColour (MixColours::border.withAlpha (0.45f));
        g.drawLine (cx, cy - R, cx, cy + R, 1.0f);   // mono (vertical)
        g.drawLine (cx - R, cy, cx + R, cy, 1.0f);   // side (horizontal)
    }

    // 45-degree safe lines + L / M / R labels.
    const float d = R * k;
    g.setColour (MixColours::textDim.withAlpha (0.35f));
    g.drawLine (cx - d, cy - d, cx, cy, 1.0f);
    g.drawLine (cx + d, cy - d, cx, cy, 1.0f);
    g.setColour (MixColours::textDim);
    g.setFont (MixLookAndFeel::uiFont (10.0f, true));
    g.drawText ("L", juce::Rectangle<float> (cx - d - 15, cy - d - 13, 13, 12), juce::Justification::centred);
    g.drawText ("M", juce::Rectangle<float> (cx - 6, cy - R - 14, 12, 12), juce::Justification::centred);
    g.drawText ("R", juce::Rectangle<float> (cx + d + 2, cy - d - 13, 13, 12), juce::Justification::centred);

    // ---- Plot ----
    if (vectorMode == VectorMode::polarLevel)
    {
        // Rays: per-angle level, filled envelope. Detection Method picks how the
        // per-angle level is measured (Peak / RMS / Envelope).
        std::vector<float> peak (numBins, 0.0f);
        std::vector<double> sq (numBins, 0.0);
        std::vector<int> cnt (numBins, 0);
        for (int i = 0; i < drawN; ++i)
        {
            const float X = k * (bufR[i] - bufL[i]);
            const float Y = k * (bufL[i] + bufR[i]);
            if (Y <= 0.0f) continue;                       // top semicircle only
            int b = (int) (std::atan2 (Y, X) / MC::pi * numBins);
            b = juce::jlimit (0, numBins - 1, b);
            const float mag = std::sqrt (X * X + Y * Y);
            peak[b] = juce::jmax (peak[b], mag);
            sq[b]  += (double) mag * mag;
            ++cnt[b];
        }

        juce::Path env;
        env.startNewSubPath (cx, cy);
        for (int b = 0; b < numBins; ++b)
        {
            float v = 0.0f;
            if (detectMethod == DetectMethod::peak)      v = peak[b];
            else if (detectMethod == DetectMethod::rms)  v = cnt[b] > 0 ? (float) std::sqrt (sq[b] / cnt[b]) : 0.0f;
            else { envBins[b] += (peak[b] - envBins[b]) * (peak[b] > envBins[b] ? 0.5f : 0.08f); v = envBins[b]; }

            v = juce::jlimit (0.0f, 1.0f, v);
            const float th = (b + 0.5f) / numBins * MC::pi;
            env.lineTo (cx + std::cos (th) * v * R, cy - std::sin (th) * v * R);
        }
        env.closeSubPath();
        g.setColour (MixColours::accentH.withAlpha (0.30f));
        g.fillPath (env);
        g.setColour (MixColours::accentH);
        g.strokePath (env, juce::PathStrokeType (1.3f));
    }
    else
    {
        // Lissajous / Polar Sample: the same per-sample dots (the grid above tells
        // them apart), drawn with a fading phosphor trail.
        const int iw = juce::jmax (1, (int) sc.getWidth());
        const int ih = juce::jmax (1, (int) sc.getHeight());
        if (scopeImg.getWidth() != iw || scopeImg.getHeight() != ih)
            scopeImg = juce::Image (juce::Image::ARGB, iw, ih, true);

        scopeImg.multiplyAllAlphas (0.80f);   // fade the previous frame
        {
            juce::Graphics ig (scopeImg);
            ig.setColour (MixColours::accentH.withAlpha (0.9f));
            const float ox = cx - sc.getX();
            const float oy = cy - sc.getY();
            for (int i = 0; i < drawN; ++i)
            {
                const float px = ox + k * (bufR[i] - bufL[i]) * R;
                const float py = oy - k * (bufL[i] + bufR[i]) * R;
                ig.fillRect (px - 0.7f, py - 0.7f, 1.4f, 1.4f);
            }
        }
        g.drawImageAt (scopeImg, (int) sc.getX(), (int) sc.getY());
    }

    // ---- Correlation meter (-1 left .. +1 right) ---------------------------
    corrBar.removeFromTop (6);
    auto track = corrBar.reduced (2, 0);
    g.setColour (MixColours::surface2);
    g.fillRoundedRectangle (track.toFloat(), 3.0f);

    const float midX = (float) track.getCentreX();
    g.setColour (MixColours::border);
    g.drawVerticalLine (track.getCentreX(), (float) track.getY(), (float) track.getBottom());   // 0 mark

    if (haveSignal)
    {
        const float hw = track.getWidth() * 0.5f;
        const float x  = midX + juce::jlimit (-1.0f, 1.0f, correlation) * hw;
        // green towards +1 (mono/in-phase), red towards -1 (out of phase)
        g.setColour (correlation < 0.0f ? red : juce::Colour (0xff22c55e));
        const float x0 = juce::jmin (midX, x);
        g.fillRoundedRectangle (x0, (float) track.getY(), std::abs (x - midX), (float) track.getHeight(), 3.0f);
    }

    if (recording)
    {
        auto tag = juce::Rectangle<int> (scopeArea.getRight() - 96, scopeArea.getY() + 8, 88, 16);
        g.setColour (red);
        g.fillEllipse ((float) tag.getX(), (float) tag.getY() + 4.0f, 8.0f, 8.0f);
        g.setFont (MixLookAndFeel::uiFont (11.0f, true));
        const int elapsed = juce::jmax (0, getRecordedCount() - 1) * recordIntervalSec;
        g.drawText ("REC " + juce::String (elapsed) + "s", tag.withTrimmedLeft (13),
                    juce::Justification::centredLeft);
    }
}
