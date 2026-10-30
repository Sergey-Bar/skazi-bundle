#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

// Studio rack-unit look: anodised faceplate, machined knobs, silkscreened legends,
// a recessed amber display and real-looking LEDs. One accent colour, no glow.
namespace Theme
{
    const juce::Colour plateTop    { 0xff26282c };
    const juce::Colour plateBottom { 0xff1a1b1e };
    const juce::Colour ear         { 0xff141517 };
    const juce::Colour groove      { 0xff0c0d0e };
    const juce::Colour bevel       { 0xff3a3d42 };
    const juce::Colour legend      { 0xffd8d3c7 };   // silkscreen off-white
    const juce::Colour legendDim   { 0xff8e8a82 };
    const juce::Colour capTop      { 0xff3b3e43 };
    const juce::Colour capBottom   { 0xff232528 };
    const juce::Colour glass       { 0xff0a0806 };
    const juce::Colour amber       { 0xffffa63a };
    const juce::Colour amberDim    { 0xff6b4518 };
    const juce::Colour ledRed      { 0xffff4433 };
    const juce::Colour ledGreen    { 0xff4bd66a };
    const juce::Colour ledYellow   { 0xffffd23a };
    const juce::Colour ledIce      { 0xff8fd8ff };

    // DIN-style panel typeface: Bahnschrift on Windows, DIN Alternate on macOS, the system sans otherwise.
    inline juce::Font face (float size, bool bold = true)
    {
        static const juce::String family = []
        {
            const auto names = juce::Font::findAllTypefaceNames();
            for (auto* candidate : { "Bahnschrift", "DIN Alternate", "DIN Condensed" })
                if (names.contains (candidate))
                    return juce::String (candidate);
            return juce::Font::getDefaultSansSerifFontName();
        }();
        static const juce::StringArray styles = juce::Font::findAllTypefaceStyles (family);

        juce::String style = bold ? "Bold" : "Regular";
        if (bold && styles.contains ("SemiBold"))
            style = "SemiBold";
        if (! styles.contains (style))
            style = styles.isEmpty() ? juce::String ("Regular") : styles[0];
        return juce::Font (juce::FontOptions (family, style, size));
    }

    // Display numerals.
    inline juce::Font mono (float size, bool bold = false)
    {
        return juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), size, bold ? juce::Font::bold : juce::Font::plain));
    }

    // Silkscreened text: a dark shadow one pixel below gives it the printed-on-metal look.
    inline void legendText (juce::Graphics& g, const juce::String& text, juce::Rectangle<float> area, juce::Justification just,
                            juce::Colour colour = legend, float tracking = 0.12f, float size = 11.0f)
    {
        g.setFont (face (size).withExtraKerningFactor (tracking));
        g.setColour (juce::Colours::black.withAlpha (0.55f));
        g.drawText (text, area.translated (0.0f, 1.0f), just, false);
        g.setColour (colour);
        g.drawText (text, area, just, false);
    }

    // Machined groove between sections: a dark cut with a lit lower edge.
    inline void grooveH (juce::Graphics& g, float x0, float x1, float y)
    {
        g.setColour (groove);
        g.fillRect (x0, y, x1 - x0, 1.0f);
        g.setColour (bevel.withAlpha (0.55f));
        g.fillRect (x0, y + 1.0f, x1 - x0, 1.0f);
    }

    inline void grooveV (juce::Graphics& g, float x, float y0, float y1)
    {
        g.setColour (groove);
        g.fillRect (x, y0, 1.0f, y1 - y0);
        g.setColour (bevel.withAlpha (0.55f));
        g.fillRect (x + 1.0f, y0, 1.0f, y1 - y0);
    }

    // Section heading: small tracked legend with a groove running off to the right.
    inline void sectionTitle (juce::Graphics& g, const juce::String& title, juce::Rectangle<float> area)
    {
        g.setFont (face (10.5f).withExtraKerningFactor (0.22f));
        const float w = (float) juce::GlyphArrangement::getStringWidthInt (g.getCurrentFont(), title) + 10.0f;
        legendText (g, title, area.withWidth (w + 4.0f), juce::Justification::centredLeft, legendDim, 0.22f, 10.5f);
        grooveH (g, area.getX() + w + 6.0f, area.getRight(), area.getCentreY());
    }

    // Recessed window (display, preset readout): inner shadow, dark glass, bevelled lip.
    inline void recess (juce::Graphics& g, juce::Rectangle<float> r, float corner = 3.0f)
    {
        g.setColour (juce::Colours::black.withAlpha (0.6f));
        g.fillRoundedRectangle (r.expanded (2.0f), corner + 2.0f);
        g.setColour (bevel.withAlpha (0.6f));
        g.drawRoundedRectangle (r.expanded (2.5f), corner + 2.5f, 1.0f);
        g.setColour (glass);
        g.fillRoundedRectangle (r, corner);
        g.setGradientFill (juce::ColourGradient (juce::Colours::black.withAlpha (0.7f), r.getX(), r.getY(),
                                                 juce::Colours::transparentBlack, r.getX(), r.getY() + 10.0f, false));
        g.fillRoundedRectangle (r, corner);
    }

    // Moulded display bezel: a raised frame with a chamfered inner edge, around a recessed window.
    inline void bezel (juce::Graphics& g, juce::Rectangle<float> window, float frame = 10.0f)
    {
        const auto outer = window.expanded (frame);
        juce::Path outerPath;
        outerPath.addRoundedRectangle (outer, 6.0f);
        juce::DropShadow (juce::Colours::black.withAlpha (0.75f), 14, { 0, 5 }).drawForPath (g, outerPath);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2f3236), outer.getX(), outer.getY(), juce::Colour (0xff111214), outer.getX(), outer.getBottom(), false));
        g.fillPath (outerPath);
        g.setColour (juce::Colours::white.withAlpha (0.14f));
        g.drawHorizontalLine ((int) outer.getY() + 1, outer.getX() + 6.0f, outer.getRight() - 6.0f);
        g.setColour (juce::Colours::black);
        g.drawRoundedRectangle (outer, 6.0f, 1.0f);

        // Chamfer falling into the window: lit bottom face, shaded top face.
        const auto chamfer = window.expanded (4.0f);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff060607), chamfer.getX(), chamfer.getY(), juce::Colour (0xff3a3d42), chamfer.getX(), chamfer.getBottom(), false));
        g.fillRoundedRectangle (chamfer, 4.0f);
        g.setColour (glass);
        g.fillRoundedRectangle (window, 3.0f);
    }

    // Glass reflection over a display, drawn last.
    inline void glassSheen (juce::Graphics& g, juce::Rectangle<float> r)
    {
        juce::Path sheen;
        sheen.startNewSubPath (r.getX(), r.getY());
        sheen.lineTo (r.getX() + r.getWidth() * 0.55f, r.getY());
        sheen.lineTo (r.getX() + r.getWidth() * 0.35f, r.getBottom());
        sheen.lineTo (r.getX(), r.getBottom());
        sheen.closeSubPath();
        g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.035f), r.getX(), r.getY(),
                                                 juce::Colours::white.withAlpha (0.0f), r.getX() + r.getWidth() * 0.4f, r.getBottom(), false));
        g.fillPath (sheen);
    }

    // A panel LED in a chrome bezel: dark lens when off, a lit lens with a hot core that spills onto the panel when on.
    inline void led (juce::Graphics& g, juce::Point<float> centre, float radius, juce::Colour colour, float level)
    {
        const auto lens = juce::Rectangle<float> (radius * 2.0f, radius * 2.0f).withCentre (centre);
        if (level > 0.02f)
        {
            g.setGradientFill (juce::ColourGradient (colour.withAlpha (0.3f * level), centre, colour.withAlpha (0.0f), centre.translated (radius * 4.0f, 0.0f), true));
            g.fillEllipse (lens.expanded (radius * 3.0f));
        }
        const auto bezelBox = lens.expanded (radius * 0.55f + 1.0f);
        g.setColour (juce::Colours::black.withAlpha (0.6f));
        g.fillEllipse (bezelBox.expanded (1.0f).translated (0.0f, 1.0f));
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xffb4b7bc), bezelBox.getX(), bezelBox.getY(), juce::Colour (0xff222427), bezelBox.getX(), bezelBox.getBottom(), false));
        g.fillEllipse (bezelBox);
        g.setColour (juce::Colours::black);
        g.fillEllipse (lens.expanded (0.6f));

        const auto off = colour.withSaturation (colour.getSaturation() * 0.7f).withMultipliedBrightness (0.2f);
        const auto on = off.interpolatedWith (colour, level);
        g.setGradientFill (juce::ColourGradient (on.brighter (0.9f * level + 0.15f), centre.translated (-radius * 0.15f, -radius * 0.2f),
                                                 on.darker (0.5f - 0.3f * level), centre.translated (radius, radius), true));
        g.fillEllipse (lens);
        // Specular glint on the domed lens.
        g.setColour (juce::Colours::white.withAlpha (0.3f + 0.4f * level));
        g.fillEllipse (juce::Rectangle<float> (radius * 0.8f, radius * 0.5f).withCentre (centre.translated (-radius * 0.28f, -radius * 0.45f)));
    }

    // Rubber-topped push button sitting in a cut-out in the panel. pressed sinks it into the hole.
    inline void buttonCap (juce::Graphics& g, juce::Rectangle<float> r, bool pressed, bool hover)
    {
        // The hole: dark, with its lower lip catching the light from above.
        const auto hole = r.expanded (2.5f);
        g.setColour (juce::Colours::black.withAlpha (0.9f));
        g.fillRoundedRectangle (hole, 5.0f);
        g.setColour (bevel.withAlpha (0.7f));
        g.drawHorizontalLine ((int) hole.getBottom(), hole.getX() + 4.0f, hole.getRight() - 4.0f);

        r = r.withTrimmedBottom (pressed ? 0.0f : 1.5f).translated (0.0f, pressed ? 1.0f : 0.0f);
        const float lift = (pressed ? -0.3f : 0.0f) + (hover ? 0.1f : 0.0f);
        const auto top = juce::Colour (0xff45484e).withMultipliedBrightness (1.0f + lift);
        const auto bottom = juce::Colour (0xff1e2023).withMultipliedBrightness (1.0f + lift);
        g.setGradientFill (juce::ColourGradient (top, r.getX(), r.getY(), bottom, r.getX(), r.getBottom(), false));
        g.fillRoundedRectangle (r, 3.5f);

        // Convex face: a soft sheen on the upper half and darker flanks.
        juce::ColourGradient flanks (juce::Colours::black.withAlpha (0.25f), r.getX(), 0.0f, juce::Colours::black.withAlpha (0.25f), r.getRight(), 0.0f, false);
        flanks.addColour (0.12, juce::Colours::transparentBlack);
        flanks.addColour (0.88, juce::Colours::transparentBlack);
        g.setGradientFill (flanks);
        g.fillRoundedRectangle (r, 3.5f);
        g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (pressed ? 0.05f : 0.13f), r.getX(), r.getY(),
                                                 juce::Colours::transparentWhite, r.getX(), r.getY() + r.getHeight() * 0.55f, false));
        g.fillRoundedRectangle (r.reduced (1.0f), 3.0f);
        g.setColour (juce::Colours::white.withAlpha (pressed ? 0.06f : 0.18f));
        g.drawHorizontalLine ((int) r.getY() + 1, r.getX() + 4.0f, r.getRight() - 4.0f);
        if (! pressed)
        {
            g.setColour (juce::Colours::black.withAlpha (0.8f));
            g.fillRect (r.getX() + 2.0f, r.getBottom(), r.getWidth() - 4.0f, 1.5f);
        }
        g.setColour (juce::Colours::black.withAlpha (0.85f));
        g.drawRoundedRectangle (r, 3.5f, 1.0f);
    }

    // Brushed, machined metal disc: a conic sheen whose highlights stay put while the knob turns.
    inline void machinedDisc (juce::Graphics& g, juce::Point<float> c, float radius, juce::Colour dark, juce::Colour light)
    {
        const int wedges = juce::jlimit (48, 144, (int) (radius * 4.0f));
        const float step = juce::MathConstants<float>::twoPi / (float) wedges;
        for (int i = 0; i < wedges; ++i)
        {
            const float a = (float) i * step;
            // Two reflections opposite each other plus a weaker pair, lit from the top left.
            const float k = 0.5f + 0.32f * std::cos (2.0f * (a + 0.75f)) + 0.12f * std::cos (4.0f * (a + 0.2f));
            juce::Path wedge;
            wedge.startNewSubPath (c);
            wedge.lineTo (c.translated (radius * std::sin (a), -radius * std::cos (a)));
            wedge.lineTo (c.translated (radius * std::sin (a + step * 1.15f), -radius * std::cos (a + step * 1.15f)));
            wedge.closeSubPath();
            g.setColour (dark.interpolatedWith (light, juce::jlimit (0.0f, 1.0f, k)));
            g.fillPath (wedge);
        }
        for (int ring = 2; ring < 10; ++ring)
        {
            const float rr = radius * (float) ring / 10.0f;
            g.setColour ((ring % 2 == 0 ? juce::Colours::white : juce::Colours::black).withAlpha (0.035f));
            g.drawEllipse (juce::Rectangle<float> (rr * 2.0f, rr * 2.0f).withCentre (c), 0.7f);
        }
    }

    // Phillips-free hex screw head for the rack ears and corners.
    inline void screw (juce::Graphics& g, juce::Point<float> c, float radius)
    {
        const auto head = juce::Rectangle<float> (radius * 2.0f, radius * 2.0f).withCentre (c);
        g.setColour (juce::Colours::black.withAlpha (0.7f));
        g.fillEllipse (head.expanded (1.5f).translated (0.0f, 1.0f));
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff8a8d92), c.translated (-radius * 0.5f, -radius * 0.6f),
                                                 juce::Colour (0xff2c2e31), c.translated (radius * 0.6f, radius), true));
        g.fillEllipse (head);
        juce::Path hex;
        hex.addPolygon (c, 6, radius * 0.48f, 0.3f);
        g.setColour (juce::Colour (0xff1a1b1d));
        g.fillPath (hex);
        g.setColour (juce::Colours::white.withAlpha (0.12f));
        g.strokePath (hex, juce::PathStrokeType (0.8f));
    }
}
