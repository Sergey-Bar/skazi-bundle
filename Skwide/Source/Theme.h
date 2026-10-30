#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace Theme
{
    const juce::Colour background { 0xff03050b };
    const juce::Colour panel      { 0xff070c17 };
    const juce::Colour panelHi    { 0xff0d1628 };
    const juce::Colour edge       { 0xff1a2a46 };
    const juce::Colour cyan       { 0xff00f0ff };
    const juce::Colour magenta    { 0xffff2bd6 };
    const juce::Colour lime       { 0xffb4ff39 };
    const juce::Colour amber      { 0xffffae1a };
    const juce::Colour red        { 0xffff3b4e };
    const juce::Colour text       { 0xffd6ecff };
    const juce::Colour dim        { 0xff4f6585 };

    inline juce::Font mono (float size, bool bold = false)
    {
        return juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), size, bold ? juce::Font::bold : juce::Font::plain));
    }

    inline juce::Font sans (float size, bool bold = false)
    {
        return juce::Font (juce::FontOptions (juce::Font::getDefaultSansSerifFontName(), size, bold ? juce::Font::bold : juce::Font::plain));
    }

    // Neon look: a few wide translucent passes under a bright core.
    inline void glowStroke (juce::Graphics& g, const juce::Path& p, juce::Colour c, float width, float intensity = 1.0f)
    {
        const float extra[] { 10.0f, 5.0f, 2.0f };
        const float alpha[] { 0.07f, 0.13f, 0.28f };
        for (int i = 0; i < 3; ++i)
        {
            g.setColour (c.withAlpha (alpha[i] * intensity));
            g.strokePath (p, juce::PathStrokeType (width + extra[i], juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
        g.setColour (c.brighter (0.35f).withAlpha (juce::jmin (1.0f, intensity)));
        g.strokePath (p, juce::PathStrokeType (width, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    inline void glowDot (juce::Graphics& g, juce::Point<float> centre, float radius, juce::Colour c, float intensity = 1.0f)
    {
        juce::ColourGradient halo (c.withAlpha (0.5f * intensity), centre, c.withAlpha (0.0f), centre.translated (radius * 4.0f, 0.0f), true);
        g.setGradientFill (halo);
        g.fillEllipse (juce::Rectangle<float> (radius * 8.0f, radius * 8.0f).withCentre (centre));
        g.setColour (c.brighter (0.7f).withAlpha (intensity));
        g.fillEllipse (juce::Rectangle<float> (radius * 2.0f, radius * 2.0f).withCentre (centre));
    }

    // Rectangle with the top-right and bottom-left corners cut off.
    inline juce::Path chamfered (juce::Rectangle<float> r, float cut)
    {
        juce::Path p;
        p.startNewSubPath (r.getX(), r.getY());
        p.lineTo (r.getRight() - cut, r.getY());
        p.lineTo (r.getRight(), r.getY() + cut);
        p.lineTo (r.getRight(), r.getBottom());
        p.lineTo (r.getX() + cut, r.getBottom());
        p.lineTo (r.getX(), r.getBottom() - cut);
        p.closeSubPath();
        return p;
    }

    inline void cornerBrackets (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour c, float len = 10.0f)
    {
        juce::Path p;
        p.startNewSubPath (r.getX(), r.getY() + len);           p.lineTo (r.getX(), r.getY());           p.lineTo (r.getX() + len, r.getY());
        p.startNewSubPath (r.getRight() - len, r.getY());       p.lineTo (r.getRight(), r.getY());       p.lineTo (r.getRight(), r.getY() + len);
        p.startNewSubPath (r.getRight(), r.getBottom() - len);  p.lineTo (r.getRight(), r.getBottom());  p.lineTo (r.getRight() - len, r.getBottom());
        p.startNewSubPath (r.getX() + len, r.getBottom());      p.lineTo (r.getX(), r.getBottom());      p.lineTo (r.getX(), r.getBottom() - len);
        g.setColour (c);
        g.strokePath (p, juce::PathStrokeType (1.5f));
    }

    // Module frame with an indexed title strip, e.g. "01 // FILTER CORE".
    inline void drawPanel (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& index, const juce::String& title, juce::Colour accent)
    {
        const auto shape = chamfered (r, 14.0f);
        g.setGradientFill (juce::ColourGradient (panelHi, r.getTopLeft(), panel, r.getBottomLeft(), false));
        g.fillPath (shape);

        // Fine scanlines give the panels a CRT texture.
        g.setColour (juce::Colours::white.withAlpha (0.012f));
        for (float y = r.getY() + 2.0f; y < r.getBottom(); y += 3.0f)
            g.drawHorizontalLine ((int) y, r.getX() + 1.0f, r.getRight() - 1.0f);

        g.setColour (edge);
        g.strokePath (shape, juce::PathStrokeType (1.0f));

        const auto head = r.withHeight (30.0f);
        g.setGradientFill (juce::ColourGradient (accent.withAlpha (0.16f), head.getTopLeft(), accent.withAlpha (0.0f), head.getTopRight(), false));
        g.fillRect (head.reduced (1.0f).withTrimmedRight (16.0f));

        juce::Path underline;
        underline.startNewSubPath (r.getX() + 1.0f, head.getBottom());
        underline.lineTo (r.getX() + 90.0f, head.getBottom());
        glowStroke (g, underline, accent, 1.2f, 0.8f);
        g.setColour (edge.brighter (0.2f));
        g.drawHorizontalLine ((int) head.getBottom(), r.getX() + 90.0f, r.getRight() - 1.0f);

        g.setFont (mono (12.0f, true));
        g.setColour (accent);
        g.drawText (index + " //", head.withTrimmedLeft (12.0f), juce::Justification::centredLeft);
        g.setColour (text);
        g.drawText (title, head.withTrimmedLeft (52.0f), juce::Justification::centredLeft);

        for (int i = 0; i < 4; ++i)
        {
            g.setColour (accent.withAlpha (0.2f + 0.2f * (float) i));
            g.fillRect (juce::Rectangle<float> (r.getRight() - 64.0f + (float) i * 9.0f, head.getCentreY() - 3.0f, 5.0f, 6.0f));
        }

        g.setColour (accent.withAlpha (0.7f));
        g.fillRect (juce::Rectangle<float> (r.getX(), r.getY(), 3.0f, 30.0f));
    }
}
