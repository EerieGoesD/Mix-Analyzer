#pragma once

#include <JuceHeader.h>
#include <vector>

//==============================================================================
// Panel layouts for the meter tiles, trading-terminal style. Given how many
// meters are active (1, 2 or 3) and a layout index, it splits an area into that
// many tiles. The same function drives the real layout and the picker's icons,
// so they always agree.
//==============================================================================
namespace MeterLayout
{
    inline int count (int n) { return n <= 1 ? 1 : (n == 2 ? 2 : 6); }

    inline std::vector<juce::Rectangle<int>> tiles (juce::Rectangle<int> a, int n, int index)
    {
        std::vector<juce::Rectangle<int>> out;

        if (n <= 1) { out.push_back (a); return out; }

        if (n == 2)
        {
            if (index == 1) { auto l = a.removeFromLeft (a.getWidth() / 2); out = { l, a }; }   // side by side
            else            { auto t = a.removeFromTop  (a.getHeight() / 2); out = { t, a }; }   // stacked
            return out;
        }

        switch (index)   // n == 3
        {
            case 1: { auto x = a.removeFromLeft (a.getWidth() / 3); auto y = a.removeFromLeft (a.getWidth() / 2); out = { x, y, a }; break; }                       // columns
            case 2: { auto top = a.removeFromTop (a.getHeight() / 2); auto tl = top.removeFromLeft (top.getWidth() / 2); out = { tl, top, a }; break; }             // 2 top / 1 bottom
            case 3: { auto top = a.removeFromTop (a.getHeight() / 2); auto bl = a.removeFromLeft (a.getWidth() / 2); out = { top, bl, a }; break; }                 // 1 top / 2 bottom
            case 4: { auto left = a.removeFromLeft (a.getWidth() / 2); auto lt = left.removeFromTop (left.getHeight() / 2); out = { lt, left, a }; break; }         // 2 left / 1 right
            case 5: { auto left = a.removeFromLeft (a.getWidth() / 2); auto rt = a.removeFromTop (a.getHeight() / 2); out = { left, rt, a }; break; }               // 1 left / 2 right
            default:{ auto x = a.removeFromTop (a.getHeight() / 3); auto y = a.removeFromTop (a.getHeight() / 2); out = { x, y, a }; break; }                       // rows
        }
        return out;
    }

    inline const char* name (int n, int index)
    {
        if (n <= 1) return "Full";
        if (n == 2) return index == 1 ? "Side by side" : "Stacked";
        switch (index)
        {
            case 1:  return "Columns";
            case 2:  return "2 top / 1 bottom";
            case 3:  return "1 top / 2 bottom";
            case 4:  return "2 left / 1 right";
            case 5:  return "1 left / 2 right";
            default: return "Rows";
        }
    }
}
