#include "Displays.h"
#include "BinaryData.h"

namespace
{
    float toDb (float gain) { return juce::Decibels::gainToDecibels (gain, -60.0f); }

    // Display phosphor colours.
    const juce::Colour phosphor    { 0xffffa63a };
    const juce::Colour phosphorHot { 0xffffd49a };
    const juce::Colour phosphorDim { 0xff7a4f1e };
}

//==============================================================================
HeaderView::HeaderView()
    : logo (juce::ImageCache::getFromMemory (BinaryData::skazi_logo_png, BinaryData::skazi_logo_pngSize).createCopy())
{
    // The logo ships on solid black: brightness becomes alpha, and the white turns into the panel's silkscreen
    // colour so it reads as printed on the metal. The red ring stays red.
    logo = logo.convertedToFormat (juce::Image::ARGB);
    juce::Image::BitmapData pixels (logo, juce::Image::BitmapData::readWrite);
    for (int y = 0; y < logo.getHeight(); ++y)
        for (int x = 0; x < logo.getWidth(); ++x)
        {
            const auto c = pixels.getPixelColour (x, y);
            const float level = juce::jmax (c.getFloatRed(), c.getFloatGreen(), c.getFloatBlue());
            if (level <= 0.0f)
            {
                pixels.setPixelColour (x, y, juce::Colours::transparentBlack);
                continue;
            }
            const bool red = c.getFloatRed() > 0.5f && c.getFloatGreen() < 0.4f;
            const auto tint = red ? juce::Colour (0xffe0352b) : Theme::legend;
            pixels.setPixelColour (x, y, tint.withAlpha (juce::jmin (1.0f, level) * c.getFloatAlpha()));
        }

    setInterceptsMouseClicks (false, false);
}

void HeaderView::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    const auto logoBox = bounds.withWidth (92.0f).reduced (0.0f, 4.0f);
    g.drawImage (logo, logoBox, juce::RectanglePlacement::xLeft | juce::RectanglePlacement::yMid);

    Theme::grooveV (g, logoBox.getRight() + 12.0f, bounds.getY() + 8.0f, bounds.getBottom() - 8.0f);

    const auto text = bounds.withTrimmedLeft (logoBox.getRight() + 28.0f);
    g.setFont (Theme::face (30.0f).withExtraKerningFactor (0.14f));
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.drawText ("SKAZI REVERB", text.withHeight (38.0f).translated (0.0f, 2.0f), juce::Justification::bottomLeft);
    g.setColour (Theme::legend);
    g.drawText ("SKAZI REVERB", text.withHeight (38.0f).translated (0.0f, 1.0f), juce::Justification::bottomLeft);

    Theme::legendText (g, juce::String (juce::CharPointer_UTF8 ("MEGA REVERB MACHINE  \xc2\xb7  10 ALGORITHMS")),
                       text.withTrimmedTop (40.0f).withHeight (14.0f), juce::Justification::centredLeft, Theme::legendDim, 0.24f, 10.0f);
}

//==============================================================================
SpaceView::SpaceView (SkverbAudioProcessor& p) : proc (p)
{
    setInterceptsMouseClicks (false, false);
}

void SpaceView::tick (float inLevel, float wetLevel)
{
    inHistory[(size_t) historyPos] = inLevel;
    wetHistory[(size_t) historyPos] = wetLevel;
    historyPos = (historyPos + 1) % historySize;
    animPhase += 0.045f;
    repaint();
}

void SpaceView::paint (juce::Graphics& g)
{
    using namespace verb;
    const auto screen = getLocalBounds().toFloat();
    g.setColour (Theme::glass);
    g.fillRoundedRectangle (screen, 3.0f);

    const int mode = (int) value (ParamIDs::mode);
    const auto& m = spec (mode);
    const bool frozen = value (ParamIDs::freeze) > 0.5f;
    const float decay = value (ParamIDs::decay);
    const float rt = mode == gated ? decay : decay * m.decayMul;
    const float pre = proc.livePreDelayMs.load() * 0.001f;
    const float fb = value (ParamIDs::feedback) * 0.01f * juce::jlimit (0.0f, 1.0f, pre * 1000.0f / 5.0f);
    const float early = value (ParamIDs::early) * 0.01f * m.early;
    const float shimmerAmt = mode == shimmer ? 0.35f + 0.65f * value (ParamIDs::shimmerAmt) * 0.01f : value (ParamIDs::shimmerAmt) * 0.01f;
    const float modDepth = value (ParamIDs::modDepth) * 0.01f * m.mod;
    const float sizeMul = sizeFactor (value (ParamIDs::size) * 0.01f) * m.scale;

    auto bounds = screen.reduced (12.0f, 8.0f);
    auto status = bounds.removeFromTop (36.0f);
    auto live = bounds.removeFromBottom (40.0f);
    bounds.removeFromBottom (8.0f);
    auto plot = bounds.withTrimmedLeft (30.0f).withTrimmedBottom (14.0f).withTrimmedTop (8.0f);

    // Status line: algorithm on the left, decay and pre-delay readouts on the right.
    g.setFont (Theme::face (24.0f).withExtraKerningFactor (0.1f));
    g.setColour (phosphor.withAlpha (0.12f));
    for (auto offset : { juce::Point<float> (-1.5f, 0.0f), juce::Point<float> (1.5f, 0.0f), juce::Point<float> (0.0f, -1.5f), juce::Point<float> (0.0f, 1.5f) })
        g.drawText (m.name, status.translated (offset.x, offset.y), juce::Justification::centredLeft);
    g.setColour (phosphor);
    g.drawText (m.name, status, juce::Justification::centredLeft);
    const float nameWidth = (float) juce::GlyphArrangement::getStringWidthInt (g.getCurrentFont(), m.name);

    juce::String line1, line2;
    if (frozen)
        line1 = "DECAY  INF  FROZEN";
    else if (mode == gated)
        line1 = "GATE  " + juce::String (juce::jlimit (0.05f, 1.5f, decay), 2) + " s";
    else
        line1 = "RT60  " + juce::String (rt, rt < 10.0f ? 2 : 1) + " s";
    const int syncIndex = proc.liveSyncIndex.load();
    line2 = "PRE  " + juce::String (juce::roundToInt (pre * 1000.0f)) + " ms" + (syncIndex >= 0 ? "  " + Sync::names[syncIndex] : juce::String());
    if (fb > 0.01f)
        line2 << "   FB " << juce::roundToInt (value (ParamIDs::feedback)) << "%";
    const int filterType = (int) value (ParamIDs::fltType);
    if (filterType > 0)
    {
        const float cut = value (ParamIDs::fltCutoff);
        line1 << "   " << TailFilter::names[filterType] << " " << (cut >= 1000.0f ? juce::String (cut / 1000.0f, 1) + "k" : juce::String (juce::roundToInt (cut)));
    }
    const float reverseAmt = mode == reverse ? 0.0f : value (ParamIDs::revAmount) * 0.01f;
    if (reverseAmt > 0.01f)
        line2 << "   REV " << juce::roundToInt (reverseAmt * 100.0f) << "%";
    const auto readouts = status.withTrimmedLeft (nameWidth + 20.0f);
    g.setFont (Theme::mono (13.0f, true));
    g.setColour (phosphorHot);
    g.drawText (line1, readouts.withHeight (18.0f), juce::Justification::centredRight);
    g.setColour (phosphor);
    g.drawText (line2, readouts.withTrimmedTop (18.0f), juce::Justification::centredRight);
    g.setColour (phosphorDim.withAlpha (0.6f));
    g.fillRect (status.getX(), status.getBottom() + 2.0f, status.getWidth(), 1.0f);

    const float span = juce::jlimit (0.3f, 30.0f, pre + (mode == gated ? rt + 0.25f : (mode == reverse ? juce::jlimit (0.15f, 2.5f, decay * 0.5f) * 2.5f : rt * 1.1f)));
    auto xFor = [&] (float t) { return plot.getX() + juce::jlimit (0.0f, 1.0f, t / span) * plot.getWidth(); };
    auto yFor = [&] (float db) { return plot.getY() + juce::jlimit (0.0f, 1.0f, -db / 60.0f) * plot.getHeight(); };

    // Dotted graticule.
    g.setFont (Theme::mono (9.5f));
    for (int db = 0; db >= -60; db -= 12)
    {
        const float y = yFor ((float) db);
        g.setColour (phosphor.withAlpha (0.16f));
        for (float x = plot.getX(); x < plot.getRight(); x += 4.0f)
            g.fillRect (x, y, 1.0f, 1.0f);
        g.setColour (phosphorDim);
        g.drawText (juce::String (db), juce::Rectangle<float> (plot.getX() - 32.0f, y - 6.0f, 27.0f, 12.0f), juce::Justification::centredRight);
    }
    const float steps[] { 0.05f, 0.1f, 0.25f, 0.5f, 1.0f, 2.0f, 5.0f, 10.0f };
    float step = 10.0f;
    for (auto st : steps)
        if (span / st <= 8.0f) { step = st; break; }
    for (float t = 0.0f; t <= span + 0.0001f; t += step)
    {
        const float x = xFor (t);
        g.setColour (phosphor.withAlpha (0.11f));
        for (float y = plot.getY(); y < plot.getBottom(); y += 4.0f)
            g.fillRect (x, y, 1.0f, 1.0f);
        g.setColour (phosphorDim);
        g.drawText (step < 1.0f ? juce::String (juce::roundToInt (t * 1000.0f)) + "ms" : juce::String (t, 0) + "s",
                    juce::Rectangle<float> (x - 30.0f, plot.getBottom() + 2.0f, 60.0f, 12.0f), juce::Justification::centred);
    }

    // Pre-delay zone.
    if (pre > 0.0f)
    {
        const auto zone = plot.withRight (xFor (pre));
        g.setColour (phosphor.withAlpha (0.05f));
        g.fillRect (zone);
        juce::Path marker;
        marker.startNewSubPath (zone.getRight(), plot.getY());
        marker.lineTo (zone.getRight(), plot.getBottom());
        const float dashes[] { 3.0f, 3.0f };
        juce::PathStrokeType (1.0f).createDashedStroke (marker, marker, dashes, 2);
        g.setColour (phosphorDim);
        g.fillPath (marker);
    }

    // Envelope of one reverb "hit" starting at time t0, scaled by levelDb.
    auto envelope = [&] (float t0, float levelDb, float wobble)
    {
        juce::Path p;
        const int points = 160;
        bool started = false;
        for (int i = 0; i <= points; ++i)
        {
            const float t = span * (float) i / (float) points;
            const float local = t - t0;
            if (local < 0.0f)
                continue;
            float db;
            if (frozen)
                db = local < 0.03f ? -40.0f + 40.0f * local / 0.03f : -3.0f;
            else if (mode == reverse)
            {
                const float window = juce::jlimit (0.15f, 2.5f, decay * 0.5f);
                db = local < window ? -48.0f + 45.0f * std::pow (local / window, 0.6f) : -60.0f;
            }
            else if (mode == gated)
                db = local < rt ? -4.0f - 3.0f * local / rt : -60.0f;
            else
            {
                const float build = 0.012f + 0.04f * sizeMul * (mode == blackhole ? 3.0f : 1.0f);
                db = local < build ? -30.0f + 27.0f * local / build : -3.0f - 60.0f * (local - build) / rt;
            }
            db += levelDb + wobble * std::sin (animPhase * 2.0f + t * 9.0f);
            const auto pt = juce::Point<float> (xFor (t), yFor (db));
            if (! started) { p.startNewSubPath (xFor (t0), plot.getBottom()); started = true; }
            p.lineTo (pt);
        }
        if (started)
            p.lineTo (plot.getRight(), plot.getBottom());
        return p;
    };

    // Feedback echoes, each one a fainter copy.
    if (fb > 0.01f && pre > 0.004f)
    {
        float level = fb;
        for (int k = 1; k < 24 && level > 0.03f; ++k, level *= fb)
        {
            if (pre * (float) (k + 1) > span)
                break;
            auto echo = envelope (pre * (float) (k + 1), toDb (level), 0.0f);
            g.setColour (phosphor.withAlpha (0.05f));
            g.fillPath (echo);
            g.setColour (phosphor.withAlpha (0.3f * level + 0.1f));
            g.strokePath (echo, juce::PathStrokeType (1.0f));
        }
    }

    auto main = envelope (pre, 0.0f, modDepth * 1.2f);
    g.setGradientFill (juce::ColourGradient (phosphor.withAlpha (0.3f), plot.getX(), plot.getY(), phosphor.withAlpha (0.03f), plot.getX(), plot.getBottom(), false));
    g.fillPath (main);
    // Phosphor bloom under the trace.
    g.setColour (phosphor.withAlpha (0.07f));
    g.strokePath (main, juce::PathStrokeType (7.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (phosphor.withAlpha (0.16f));
    g.strokePath (main, juce::PathStrokeType (3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (phosphorHot);
    g.strokePath (main, juce::PathStrokeType (1.4f, juce::PathStrokeType::curved));

    // Reverse layer: a dashed swell rising into each window.
    if (reverseAmt > 0.01f && ! frozen)
    {
        const float window = juce::jlimit (0.1f, 2.5f, value (ParamIDs::revTime));
        juce::Path swell;
        const float topDb = toDb (reverseAmt) - 4.0f;
        for (int i = 0; i <= 60; ++i)
        {
            const float t = pre + window * (float) i / 60.0f;
            const auto pt = juce::Point<float> (xFor (t), yFor (-48.0f + (topDb + 48.0f) * std::pow ((float) i / 60.0f, 0.6f)));
            if (i == 0) swell.startNewSubPath (pt); else swell.lineTo (pt);
        }
        swell.lineTo (xFor (pre + window), plot.getBottom());
        const float dashes[] { 4.0f, 3.0f };
        juce::Path dashed;
        juce::PathStrokeType (1.4f).createDashedStroke (dashed, swell, dashes, 2);
        g.setColour (phosphorHot.withAlpha (0.85f));
        g.fillPath (dashed);
    }

    // Early-reflection spikes.
    if (early > 0.01f && ! frozen)
    {
        const float spanMs[3] { 45.0f, 110.0f, 240.0f };
        for (int t = 0; t < 16; ++t)
        {
            const float pos = std::pow (((float) t + 0.6f) / 16.0f, 1.6f);
            const float time = pre + (1.5f + pos * spanMs[m.erPattern]) * 0.001f;
            const float level = early * std::exp (-2.5f * pos) * (0.7f + 0.3f * std::sin ((float) t * 2.3f));
            const float x = xFor (time), top = yFor (toDb (level * 0.9f));
            g.setColour (phosphorHot.withAlpha (0.85f));
            g.drawLine (x, plot.getBottom(), x, top, 1.2f);
            g.fillRect (x - 1.5f, top - 1.5f, 3.0f, 3.0f);
        }
    }

    // Shimmer: pixels drifting up off the tail.
    if (shimmerAmt > 0.01f && ! frozen)
    {
        random.setSeed (7);
        const int count = (int) (40.0f * shimmerAmt);
        for (int i = 0; i < count; ++i)
        {
            const float t = pre + random.nextFloat() * (span - pre);
            const float rise = std::fmod (animPhase * (0.2f + random.nextFloat() * 0.3f) + random.nextFloat(), 1.0f);
            const float x = xFor (t);
            const float y = yFor (-3.0f - 60.0f * (t - pre) / juce::jmax (0.1f, rt)) - rise * 50.0f;
            g.setColour (phosphorHot.withAlpha (0.9f * (1.0f - rise)));
            g.fillRect (x - 1.0f, juce::jmax (plot.getY(), y) - 1.0f, 2.0f, 2.0f);
        }
    }

    // Live strip: input above the centre line, wet below it.
    live = live.withTrimmedLeft (30.0f);
    g.setColour (phosphorDim.withAlpha (0.5f));
    g.drawRect (live, 1.0f);
    const float mid = live.getCentreY();
    juce::Path inPath, wetPath;
    inPath.startNewSubPath (live.getX(), mid);
    wetPath.startNewSubPath (live.getX(), mid);
    for (int i = 0; i < historySize; ++i)
    {
        const auto idx = (size_t) ((historyPos + i) % historySize);
        const float x = live.getX() + live.getWidth() * (float) i / (float) (historySize - 1);
        const float a = juce::jlimit (0.0f, 1.0f, (toDb (inHistory[idx]) + 60.0f) / 60.0f);
        const float b = juce::jlimit (0.0f, 1.0f, (toDb (wetHistory[idx]) + 60.0f) / 60.0f);
        inPath.lineTo (x, mid - a * (live.getHeight() * 0.5f - 2.0f));
        wetPath.lineTo (x, mid + b * (live.getHeight() * 0.5f - 2.0f));
    }
    inPath.lineTo (live.getRight(), mid);
    wetPath.lineTo (live.getRight(), mid);
    g.setColour (phosphorHot.withAlpha (0.3f));
    g.fillPath (inPath);
    g.setColour (phosphor.withAlpha (0.55f));
    g.fillPath (wetPath);
    g.setColour (phosphorDim);
    g.drawHorizontalLine ((int) mid, live.getX(), live.getRight());
    g.setFont (Theme::mono (9.5f, true));
    g.setColour (phosphorDim.brighter (0.4f));
    g.drawText ("IN", juce::Rectangle<float> (live.getX() - 30.0f, live.getY(), 25.0f, live.getHeight() * 0.5f), juce::Justification::centredRight);
    g.drawText ("WET", juce::Rectangle<float> (live.getX() - 30.0f, mid, 25.0f, live.getHeight() * 0.5f), juce::Justification::centredRight);

    // LCD pixel matrix and the darker corners of a backlit panel, then the glass.
    if (pixelGrid.getWidth() != getWidth() || pixelGrid.getHeight() != getHeight())
    {
        pixelGrid = juce::Image (juce::Image::ARGB, juce::jmax (1, getWidth()), juce::jmax (1, getHeight()), true);
        juce::Graphics pg (pixelGrid);
        pg.setColour (juce::Colours::black.withAlpha (0.2f));
        for (int x = 0; x < getWidth(); x += 3)
            pg.fillRect (x, 0, 1, getHeight());
        for (int y = 0; y < getHeight(); y += 3)
            pg.fillRect (0, y, getWidth(), 1);
        const auto c = screen.getCentre();
        juce::ColourGradient vignette (juce::Colours::transparentBlack, c.x, c.y, juce::Colours::black.withAlpha (0.4f), screen.getX(), screen.getY(), true);
        vignette.addColour (0.6, juce::Colours::transparentBlack);
        pg.setGradientFill (vignette);
        pg.fillRect (screen);
    }
    g.drawImageAt (pixelGrid, 0, 0);
    Theme::glassSheen (g, screen);
}

//==============================================================================
MeterView::MeterView (SkverbAudioProcessor& p) : proc (p)
{
    setInterceptsMouseClicks (false, false);
}

void MeterView::tick (float inLevel, float wetLevel, float outLevel)
{
    auto fall = [] (float& v, float& hold, float level)
    {
        v = juce::jmax (level, v * 0.86f);
        hold = juce::jmax (level, hold * 0.985f);
    };
    fall (in, inHold, inLevel);
    fall (wet, wetHold, wetLevel);
    fall (out, outHold, outLevel);
    duck = proc.duckGain.load();
    repaint();
}

void MeterView::paint (juce::Graphics& g)
{
    // Four horizontal LED ladders behind the window, with a printed dB scale along the top.
    const auto bounds = getLocalBounds().toFloat().reduced (10.0f, 6.0f);
    auto norm = [] (float gain) { return juce::jlimit (0.0f, 1.0f, (juce::Decibels::gainToDecibels (gain, -60.0f) + 60.0f) / 66.0f); };

    struct Bar { const char* label; float level, hold; bool reduction; };
    const Bar bars[] {
        { "IN",   norm (in),  norm (inHold),  false },
        { "WET",  norm (wet), norm (wetHold), false },
        { "OUT",  norm (out), norm (outHold), false },
        { "DUCK", 1.0f - duck, 0.0f,          true },
    };

    const float labelWidth = 42.0f;
    auto scale = bounds.withHeight (14.0f);
    auto ladders = bounds.withTrimmedTop (16.0f);
    const float barArea = ladders.getWidth() - labelWidth;
    auto xFor = [&] (float t) { return ladders.getX() + labelWidth + t * barArea; };

    g.setFont (Theme::mono (9.5f));
    for (int db : { -60, -48, -36, -24, -18, -12, -6, -3, 0, 6 })
    {
        const float x = xFor ((float) (db + 60) / 66.0f);
        g.setColour (Theme::legendDim);
        g.drawText (juce::String (db), juce::Rectangle<float> (x - 15.0f, scale.getY(), 30.0f, 12.0f), juce::Justification::centred);
        g.setColour (Theme::legendDim.withAlpha (0.35f));
        g.fillRect (x, scale.getBottom(), 1.0f, 2.0f);
    }

    const float rowHeight = ladders.getHeight() / 4.0f;
    const int segments = 48;
    const float segW = barArea / (float) segments;
    for (int i = 0; i < 4; ++i)
    {
        const auto& b = bars[i];
        const auto rowArea = juce::Rectangle<float> (ladders.getX(), ladders.getY() + rowHeight * (float) i, ladders.getWidth(), rowHeight).reduced (0.0f, 3.0f);
        g.setFont (Theme::face (10.5f).withExtraKerningFactor (0.12f));
        g.setColour (Theme::amber.withAlpha (0.75f));
        g.drawText (b.label, rowArea.withWidth (labelWidth - 6.0f), juce::Justification::centredLeft);

        for (int sIdx = 0; sIdx < segments; ++sIdx)
        {
            const float t = ((float) sIdx + 0.5f) / (float) segments;
            const bool lit = t <= b.level;
            const auto seg = juce::Rectangle<float> (xFor ((float) sIdx / (float) segments) + 0.75f, rowArea.getY(), segW - 1.5f, rowArea.getHeight());
            const float db = t * 66.0f - 60.0f;
            const auto colour = b.reduction ? Theme::amber : (db > -3.0f ? Theme::ledRed : db > -12.0f ? Theme::ledYellow : Theme::ledGreen);
            if (lit)
            {
                g.setColour (colour.withAlpha (0.16f));
                g.fillRoundedRectangle (seg.expanded (1.5f, 2.0f), 2.0f);
            }
            const auto base = lit ? colour : colour.withMultipliedBrightness (0.16f).withMultipliedSaturation (0.6f);
            g.setGradientFill (juce::ColourGradient (base.brighter (lit ? 0.5f : 0.1f), seg.getX(), seg.getY(), base.darker (0.3f), seg.getX(), seg.getBottom(), false));
            g.fillRoundedRectangle (seg, 1.0f);
            g.setColour (juce::Colours::white.withAlpha (lit ? 0.35f : 0.06f));
            g.fillRect (seg.reduced (0.5f, 0.0f).withHeight (1.0f).translated (0.0f, 1.0f));
        }
        if (b.hold > 0.01f && ! b.reduction)
        {
            g.setColour (Theme::legend);
            g.fillRect (xFor (b.hold), rowArea.getY() - 1.0f, 1.5f, rowArea.getHeight() + 2.0f);
        }
    }

    // LCD-style pixel grid and glass, matching the main display.
    Theme::glassSheen (g, getLocalBounds().toFloat());
}
