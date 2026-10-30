#include "Displays.h"
#include "BinaryData.h"
#include "Presets.h"

namespace
{
    constexpr float minFreq = 20.0f, maxFreq = 20000.0f;
    constexpr float topDb = 30.0f, bottomDb = -54.0f;
    constexpr float specTopDb = 6.0f, specBottomDb = -90.0f;

    float rawParam (const LowpassAudioProcessor& p, const char* id)
    {
        return p.apvts.getRawParameterValue (id)->load();
    }

    juce::String freqLabel (float f)
    {
        return f >= 1000.0f ? juce::String (f / 1000.0f, 0) + "k" : juce::String ((int) f);
    }

    void drawFrame (juce::Graphics& g, juce::Rectangle<float> bounds, const juce::String& title, juce::Colour accent)
    {
        const auto shape = Theme::chamfered (bounds.reduced (0.5f), 12.0f);
        g.setGradientFill (juce::ColourGradient (Theme::panelHi, bounds.getTopLeft(), Theme::background, bounds.getBottomLeft(), false));
        g.fillPath (shape);
        g.setColour (Theme::edge);
        g.strokePath (shape, juce::PathStrokeType (1.0f));
        g.setColour (accent.withAlpha (0.8f));
        g.fillRect (bounds.getX(), bounds.getY(), 3.0f, 22.0f);
        g.setFont (Theme::mono (11.0f, true));
        g.setColour (accent);
        g.drawText (title, bounds.withHeight (22.0f).withTrimmedLeft (12.0f), juce::Justification::centredLeft);
    }
}

//==============================================================================
FilterDisplay::FilterDisplay (LowpassAudioProcessor& p)
    : proc (p),
      cutoffParam (*p.apvts.getParameter (ParamIDs::cutoff)),
      resonanceParam (*p.apvts.getParameter (ParamIDs::resonance))
{
    setMouseCursor (juce::MouseCursor::CrosshairCursor);
}

float FilterDisplay::xForFreq (float freq) const
{
    return plot.getX() + plot.getWidth() * std::log (juce::jmax (freq, 1.0f) / minFreq) / std::log (maxFreq / minFreq);
}

float FilterDisplay::freqForX (float x) const
{
    return minFreq * std::pow (maxFreq / minFreq, (x - plot.getX()) / plot.getWidth());
}

float FilterDisplay::yForDb (float db) const
{
    return plot.getY() + plot.getHeight() * (topDb - db) / (topDb - bottomDb);
}

float FilterDisplay::yForSpectrumDb (float db) const
{
    return plot.getY() + plot.getHeight() * (specTopDb - db) / (specTopDb - specBottomDb);
}

float FilterDisplay::yForResonance (float res) const
{
    return plot.getBottom() - (0.1f + 0.8f * res) * plot.getHeight();
}

float FilterDisplay::resonanceForY (float y) const
{
    return ((plot.getBottom() - y) / plot.getHeight() - 0.1f) / 0.8f;
}

void FilterDisplay::resized()
{
    plot = getLocalBounds().toFloat().reduced (1.0f).withTrimmedLeft (42.0f).withTrimmedBottom (36.0f).withTrimmedTop (32.0f).withTrimmedRight (14.0f);
    const auto columns = (size_t) (plot.getWidth() / 2.0f) + 1;
    spectrumIn.assign (columns, -120.0f);
    spectrumOut.assign (columns, -120.0f);
}

void FilterDisplay::analyse (const SampleRing& ring, std::vector<float>& smoothed, double sampleRate)
{
    ring.copyLatest (fftData.data(), fftSize);
    std::fill (fftData.begin() + fftSize, fftData.end(), 0.0f);
    window.multiplyWithWindowingTable (fftData.data(), (size_t) fftSize);
    fft.performFrequencyOnlyForwardTransform (fftData.data());

    const float binHz = (float) sampleRate / (float) fftSize;
    const int lastBin = fftSize / 2 - 1;

    for (size_t c = 0; c < smoothed.size(); ++c)
    {
        const float x0 = plot.getX() + (float) c * 2.0f;
        const float f0 = freqForX (x0), f1 = freqForX (x0 + 2.0f);
        const float b0 = f0 / binHz, b1 = f1 / binHz;
        float magnitude = 0.0f;

        if (b1 - b0 < 1.0f)
        {
            // Low end: several pixels share a bin, so interpolate between bins.
            const float centre = 0.5f * (b0 + b1);
            const int i = juce::jlimit (0, lastBin - 1, (int) centre);
            const float frac = centre - (float) i;
            magnitude = fftData[(size_t) i] + (fftData[(size_t) i + 1] - fftData[(size_t) i]) * frac;
        }
        else
        {
            for (int b = juce::jlimit (0, lastBin, (int) b0); b <= juce::jlimit (0, lastBin, (int) b1); ++b)
                magnitude = juce::jmax (magnitude, fftData[(size_t) b]);
        }

        // Normalise a full-scale sine to 0 dB and tilt +3 dB/oct so pink-ish material looks flat.
        const float db = juce::Decibels::gainToDecibels (magnitude / ((float) fftSize * 0.25f), -120.0f)
                       + 3.0f * std::log2 (juce::jmax (f0, minFreq) / 1000.0f);
        auto& s = smoothed[c];
        s = db > s ? s + (db - s) * 0.55f : juce::jmax (db, s - 0.9f);
    }
}

juce::Path FilterDisplay::spectrumPath (const std::vector<float>& values) const
{
    juce::Path path;
    for (size_t c = 0; c < values.size(); ++c)
    {
        const float x = plot.getX() + (float) c * 2.0f;
        const float y = juce::jmin (plot.getBottom() + 2.0f, yForSpectrumDb (values[c]));
        if (c == 0)
            path.startNewSubPath (x, y);
        else
            path.lineTo (x, y);
    }
    return path;
}

juce::Path FilterDisplay::responsePath (const MorphWeights& weights, int stages, float cutoff, float damping, double sampleRate) const
{
    juce::Path path;
    bool first = true;
    for (float x = plot.getX(); x <= plot.getRight() + 1.0f; x += 2.0f)
    {
        const double magnitude = svfMagnitude (weights, stages, freqForX (x), cutoff, damping, sampleRate);
        const float db = juce::Decibels::gainToDecibels ((float) magnitude, -90.0f);
        const float y = juce::jlimit (plot.getY() - 2.0f, plot.getBottom() + 2.0f, yForDb (db));
        if (first)
            path.startNewSubPath (x, y);
        else
            path.lineTo (x, y);
        first = false;
    }
    return path;
}

void FilterDisplay::tick()
{
    const double fs = proc.liveSampleRate.load();
    analyse (proc.inputRing, spectrumIn, fs);
    analyse (proc.outputRing, spectrumOut, fs);

    time += 1.0f / 60.0f;
    scan += 0.005f;
    if (scan > 1.0f)
        scan -= 1.0f;

    for (auto& p : particles)
    {
        p.pos += p.vel;
        p.vel.y -= 0.035f;
        p.vel.x *= 0.98f;
        p.life -= 0.016f;
    }
    particles.erase (std::remove_if (particles.begin(), particles.end(), [] (const Particle& p) { return p.life <= 0.0f; }), particles.end());

    // Sparks fly off the live resonant peak; more resonance and more signal, more sparks.
    const int stages = juce::roundToInt (rawParam (proc, ParamIDs::slope)) + 1;
    const float res = proc.liveResonance.load();
    const float damping = proc.liveDamping.load();
    const float live = proc.liveCutoff[0].load();
    const float energy = isSelfOscillating (damping) ? 1.0f : juce::jlimit (0.0f, 1.0f, proc.liveEnvelope.load());
    const float peakDb = juce::Decibels::gainToDecibels ((float) svfMagnitude (morphWeights (proc.liveMorph.load()), stages, live, live, damping, fs), -90.0f);
    const juce::Point<float> origin { xForFreq (live), juce::jlimit (plot.getY(), plot.getBottom(), yForDb (peakDb)) };

    const int spawn = (int) (energy * (1.0f + res * 5.0f) + random.nextFloat());
    for (int i = 0; i < spawn && particles.size() < 400; ++i)
        particles.push_back ({ origin,
                               { (random.nextFloat() - 0.5f) * 3.0f, -0.5f - random.nextFloat() * 2.5f },
                               0.6f + random.nextFloat() * 0.4f,
                               random.nextBool() ? Theme::cyan : Theme::magenta });

    repaint();
}

void FilterDisplay::drawGrid (juce::Graphics& g) const
{
    g.setColour (Theme::background.withAlpha (0.65f));
    g.fillRect (plot);

    // Dot matrix texture.
    g.setColour (Theme::cyan.withAlpha (0.05f));
    for (float y = plot.getY() + 6.0f; y < plot.getBottom(); y += 12.0f)
        for (float x = plot.getX() + 6.0f; x < plot.getRight(); x += 12.0f)
            g.fillRect (x, y, 1.0f, 1.0f);

    g.setFont (Theme::mono (10.0f));
    for (float decade : { 10.0f, 100.0f, 1000.0f, 10000.0f })
        for (int m = 1; m <= 9; ++m)
        {
            const float f = decade * (float) m;
            if (f < minFreq || f > maxFreq)
                continue;
            const float x = xForFreq (f);
            g.setColour (Theme::edge.withAlpha (m == 1 ? 0.9f : 0.35f));
            g.drawVerticalLine ((int) x, plot.getY(), plot.getBottom());
        }

    g.setColour (Theme::dim);
    for (float f : { 20.0f, 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f, 20000.0f })
        g.drawText (freqLabel (f), juce::Rectangle<float> (40.0f, 14.0f).withCentre ({ juce::jlimit (plot.getX() + 12.0f, plot.getRight() - 12.0f, xForFreq (f)), plot.getBottom() + 12.0f }),
                    juce::Justification::centred);

    for (float db = 24.0f; db >= -48.0f; db -= 12.0f)
    {
        const float y = yForDb (db);
        g.setColour (db == 0.0f ? Theme::cyan.withAlpha (0.25f) : Theme::edge.withAlpha (0.6f));
        g.drawHorizontalLine ((int) y, plot.getX(), plot.getRight());
        g.setColour (Theme::dim);
        g.drawText ((db > 0 ? "+" : "") + juce::String ((int) db), juce::Rectangle<float> (plot.getX() - 40.0f, y - 7.0f, 34.0f, 14.0f), juce::Justification::centredRight);
    }
}

void FilterDisplay::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    const double fs = proc.liveSampleRate.load();
    const int stages = juce::roundToInt (rawParam (proc, ParamIDs::slope)) + 1;
    const float cutoff = rawParam (proc, ParamIDs::cutoff);
    const float res = rawParam (proc, ParamIDs::resonance);
    const float damping = resonanceToDamping (res);
    const auto weights = morphWeights (rawParam (proc, ParamIDs::morph));
    const float liveMorph = proc.liveMorph.load();
    const auto liveWeights = morphWeights (liveMorph);
    const float liveDamping = proc.liveDamping.load();
    const float liveL = proc.liveCutoff[0].load();
    const float liveR = proc.liveCutoff[1].load();

    const auto frame = Theme::chamfered (bounds.reduced (0.5f), 18.0f);
    g.setGradientFill (juce::ColourGradient (Theme::panelHi, bounds.getTopLeft(), Theme::background, bounds.getBottomLeft(), false));
    g.fillPath (frame);
    g.setColour (Theme::edge);
    g.strokePath (frame, juce::PathStrokeType (1.0f));

    drawGrid (g);

    {
        juce::Graphics::ScopedSaveState save (g);
        g.reduceClipRegion (plot.toNearestInt());

        // Input spectrum as a ghost, output spectrum as a filled glowing mountain.
        g.setColour (Theme::cyan.withAlpha (0.22f));
        g.strokePath (spectrumPath (spectrumIn), juce::PathStrokeType (1.0f));

        auto outLine = spectrumPath (spectrumOut);
        auto outFill = outLine;
        outFill.lineTo (plot.getRight(), plot.getBottom() + 4.0f);
        outFill.lineTo (plot.getX(), plot.getBottom() + 4.0f);
        outFill.closeSubPath();
        g.setGradientFill (juce::ColourGradient (Theme::magenta.withAlpha (0.3f), 0.0f, plot.getY(), Theme::cyan.withAlpha (0.03f), 0.0f, plot.getBottom(), false));
        g.fillPath (outFill);
        Theme::glowStroke (g, outLine, Theme::magenta, 1.0f, 0.45f);

        // Static curve from the knobs, dashed.
        juce::Path dashed;
        const float dashes[] { 5.0f, 5.0f };
        juce::PathStrokeType (1.2f).createDashedStroke (dashed, responsePath (weights, stages, cutoff, damping, fs), dashes, 2);
        g.setColour (Theme::text.withAlpha (0.3f));
        g.fillPath (dashed);

        // Live curves after LFO / envelope / spread.
        if (std::abs (std::log2 (liveR / liveL)) > 0.01f)
            Theme::glowStroke (g, responsePath (liveWeights, stages, liveR, liveDamping, fs), Theme::magenta, 1.6f, 0.85f);

        auto live = responsePath (liveWeights, stages, liveL, liveDamping, fs);
        auto liveFill = live;
        liveFill.lineTo (plot.getRight() + 2.0f, plot.getBottom() + 4.0f);
        liveFill.lineTo (plot.getX(), plot.getBottom() + 4.0f);
        liveFill.closeSubPath();
        g.setGradientFill (juce::ColourGradient (Theme::cyan.withAlpha (0.22f), 0.0f, plot.getY(), Theme::cyan.withAlpha (0.0f), 0.0f, plot.getBottom(), false));
        g.fillPath (liveFill);
        Theme::glowStroke (g, live, Theme::cyan, 2.2f);

        // Laser at the live cutoff.
        const float lx = xForFreq (liveL);
        g.setColour (Theme::cyan.withAlpha (0.06f));
        g.fillRect (lx - 8.0f, plot.getY(), 16.0f, plot.getHeight());
        g.setGradientFill (juce::ColourGradient (Theme::cyan.withAlpha (0.0f), lx, plot.getY(), Theme::cyan.withAlpha (0.7f), lx, plot.getBottom(), false));
        g.fillRect (lx - 0.75f, plot.getY(), 1.5f, plot.getHeight());

        // Sweeping scanline.
        const float sx = plot.getX() + scan * plot.getWidth();
        g.setGradientFill (juce::ColourGradient (Theme::cyan.withAlpha (0.0f), sx - 140.0f, 0.0f, Theme::cyan.withAlpha (0.07f), sx, 0.0f, false));
        g.fillRect (sx - 140.0f, plot.getY(), 140.0f, plot.getHeight());
        g.setColour (Theme::cyan.withAlpha (0.35f));
        g.fillRect (sx, plot.getY(), 1.0f, plot.getHeight());

        for (const auto& p : particles)
        {
            g.setColour (p.colour.withAlpha (p.life * 0.25f));
            g.fillEllipse (p.pos.x - 3.0f, p.pos.y - 3.0f, 6.0f, 6.0f);
            g.setColour (p.colour.brighter (0.5f).withAlpha (p.life));
            g.fillEllipse (p.pos.x - 1.0f, p.pos.y - 1.0f, 2.0f, 2.0f);
        }

        // Control node: crosshair, counter-rotating arcs and a hot core.
        const juce::Point<float> node { xForFreq (cutoff), yForResonance (res) };
        // Dash each hair separately: dashing one multi-subpath path bridges the gap between them.
        const float crossDashes[] { 2.0f, 4.0f };
        g.setColour (Theme::cyan.withAlpha (dragging ? 0.45f : 0.2f));
        for (const auto& hair : { juce::Line<float> (node.x, plot.getY(), node.x, plot.getBottom()),
                                  juce::Line<float> (plot.getX(), node.y, plot.getRight(), node.y) })
            g.drawDashedLine (hair, crossDashes, 2, 1.0f);

        const float ringRadius = dragging ? 20.0f : 16.0f;
        for (int i = 0; i < 3; ++i)
        {
            const float start = time * 1.6f + (float) i * juce::MathConstants<float>::twoPi / 3.0f;
            juce::Path arc;
            arc.addCentredArc (node.x, node.y, ringRadius, ringRadius, 0.0f, start, start + 1.3f, true);
            Theme::glowStroke (g, arc, Theme::cyan, 1.5f, 0.8f);

            const float back = -time * 2.4f + (float) i * juce::MathConstants<float>::twoPi / 3.0f;
            juce::Path inner;
            inner.addCentredArc (node.x, node.y, ringRadius * 0.6f, ringRadius * 0.6f, 0.0f, back, back + 0.8f, true);
            g.setColour (Theme::magenta.withAlpha (0.9f));
            g.strokePath (inner, juce::PathStrokeType (1.5f));
        }
        Theme::glowDot (g, node, 3.0f, juce::Colours::white);

        // Readout tag.
        const juce::String tag = cutoffParam.getCurrentValueAsText() + (isSelfOscillating (damping) ? juce::String ("  SELF-OSC") : "  Q " + juce::String (1.0f / damping, 2));
        g.setFont (Theme::mono (11.0f, true));
        const float tagWidth = (float) tag.length() * 7.2f + 16.0f;
        auto tagBox = juce::Rectangle<float> (tagWidth, 20.0f).withPosition (node.x + 24.0f, node.y - 32.0f);
        if (tagBox.getRight() > plot.getRight() - 4.0f)
            tagBox.setX (node.x - 24.0f - tagWidth);
        tagBox.setY (juce::jlimit (plot.getY() + 4.0f, plot.getBottom() - 24.0f, tagBox.getY()));
        g.setColour (Theme::background.withAlpha (0.85f));
        g.fillRect (tagBox);
        g.setColour (Theme::cyan.withAlpha (0.8f));
        g.drawRect (tagBox, 1.0f);
        g.setColour (Theme::text);
        g.drawText (tag, tagBox, juce::Justification::centred);
    }

    Theme::cornerBrackets (g, plot.expanded (4.0f), Theme::cyan.withAlpha (0.7f), 14.0f);

    // HUD header.
    const auto head = bounds.withHeight (30.0f).reduced (16.0f, 0.0f);
    g.setFont (Theme::mono (12.0f, true));
    g.setColour (Theme::cyan);
    g.drawText (">> SPECTRAL FIELD", head, juce::Justification::centredLeft);
    g.setColour (Theme::dim.brighter (0.4f));
    g.drawText ("MORPH " + LowpassAudioProcessor::morphName (liveMorph) + " / " + LowpassAudioProcessor::slopeNames[stages - 1].toUpperCase(),
                head.withTrimmedLeft (160.0f), juce::Justification::centredLeft);

    const bool blink = std::fmod (time, 1.0f) < 0.5f;
    g.setColour (blink ? Theme::red : Theme::red.withAlpha (0.3f));
    g.fillEllipse (juce::Rectangle<float> (6.0f, 6.0f).withCentre ({ head.getRight() - 230.0f, head.getCentreY() }));
    g.setColour (Theme::dim.brighter (0.4f));
    g.drawText ("LIVE   FS " + juce::String (fs / 1000.0, 1) + "k   MOD " + juce::String (juce::roundToInt (liveL)) + " Hz",
                head.withTrimmedLeft (head.getWidth() - 220.0f), juce::Justification::centredRight);

    g.setColour (Theme::dim);
    g.setFont (Theme::mono (10.0f));
    g.drawText ("DRAG: CUTOFF/RES   WHEEL: RES   DBL-CLICK: RESET", bounds.withTop (bounds.getBottom() - 15.0f).withTrimmedRight (20.0f).translated (0.0f, -2.0f),
                juce::Justification::centredRight);
}

void FilterDisplay::setFromMouse (juce::Point<float> p)
{
    const float freq = juce::jlimit (minFreq, maxFreq, freqForX (p.x));
    cutoffParam.setValueNotifyingHost (cutoffParam.convertTo0to1 (freq));
    resonanceParam.setValueNotifyingHost (resonanceParam.convertTo0to1 (juce::jlimit (0.0f, 1.0f, resonanceForY (p.y))));
}

void FilterDisplay::mouseDown (const juce::MouseEvent& e)
{
    dragging = true;
    cutoffParam.beginChangeGesture();
    resonanceParam.beginChangeGesture();
    setFromMouse (e.position);
}

void FilterDisplay::mouseDrag (const juce::MouseEvent& e)
{
    setFromMouse (e.position);
}

void FilterDisplay::mouseUp (const juce::MouseEvent&)
{
    dragging = false;
    cutoffParam.endChangeGesture();
    resonanceParam.endChangeGesture();
}

void FilterDisplay::mouseDoubleClick (const juce::MouseEvent&)
{
    for (auto* param : { &cutoffParam, &resonanceParam })
    {
        param->beginChangeGesture();
        param->setValueNotifyingHost (param->getDefaultValue());
        param->endChangeGesture();
    }
}

void FilterDisplay::mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& wheel)
{
    resonanceParam.beginChangeGesture();
    resonanceParam.setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, resonanceParam.getValue() + wheel.deltaY * 0.1f));
    resonanceParam.endChangeGesture();
}

//==============================================================================
ScopeView::ScopeView (LowpassAudioProcessor& p) : proc (p) {}

void ScopeView::tick()
{
    proc.outputRing.copyLatest (outBuffer.data(), history);
    proc.inputRing.copyLatest (inBuffer.data(), history);

    // Lock onto the latest rising zero crossing so periodic material stands still.
    int start = history - window;
    for (int i = history - window; i > window; --i)
        if (outBuffer[(size_t) i - 1] < 0.0f && outBuffer[(size_t) i] >= 0.0f)
        {
            start = i;
            break;
        }

    float peak = 1.0e-4f;
    for (int i = 0; i < window; ++i)
    {
        outTrace[(size_t) i] = outBuffer[(size_t) (start + i)];
        inTrace[(size_t) i] = inBuffer[(size_t) (start + i)];
        peak = juce::jmax (peak, std::abs (outTrace[(size_t) i]), std::abs (inTrace[(size_t) i]));
    }

    zoom += (juce::jlimit (1.0f, 16.0f, 0.85f / peak) - zoom) * 0.08f;
    repaint();
}

void ScopeView::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    drawFrame (g, bounds, "WAVEFORM // OSC", Theme::cyan);

    g.setFont (Theme::mono (11.0f, true));
    g.setColour (Theme::magenta);
    g.drawText ("ZOOM x" + juce::String (zoom, 1), bounds.withHeight (22.0f).withTrimmedRight (16.0f), juce::Justification::centredRight);

    const auto area = bounds.reduced (10.0f).withTrimmedTop (16.0f);
    g.setColour (Theme::background.withAlpha (0.7f));
    g.fillRect (area);
    g.setColour (Theme::edge.withAlpha (0.6f));
    for (int i = 1; i < 8; ++i)
        g.drawVerticalLine ((int) (area.getX() + area.getWidth() * (float) i / 8.0f), area.getY(), area.getBottom());
    for (int i = 1; i < 4; ++i)
        g.drawHorizontalLine ((int) (area.getY() + area.getHeight() * (float) i / 4.0f), area.getX(), area.getRight());
    g.setColour (Theme::cyan.withAlpha (0.3f));
    g.drawHorizontalLine ((int) area.getCentreY(), area.getX(), area.getRight());

    auto tracePath = [&] (const std::vector<float>& trace)
    {
        juce::Path path;
        for (int i = 0; i < window; ++i)
        {
            const float x = area.getX() + area.getWidth() * (float) i / (float) (window - 1);
            const float y = area.getCentreY() - juce::jlimit (-1.0f, 1.0f, trace[(size_t) i] * zoom) * area.getHeight() * 0.48f;
            if (i == 0)
                path.startNewSubPath (x, y);
            else
                path.lineTo (x, y);
        }
        return path;
    };

    juce::Graphics::ScopedSaveState save (g);
    g.reduceClipRegion (area.toNearestInt());
    g.setColour (Theme::magenta.withAlpha (0.35f));
    g.strokePath (tracePath (inTrace), juce::PathStrokeType (1.0f));
    Theme::glowStroke (g, tracePath (outTrace), Theme::cyan, 1.5f);
    Theme::cornerBrackets (g, area.reduced (1.0f), Theme::cyan.withAlpha (0.5f), 8.0f);
}

//==============================================================================
MeterView::MeterView (LowpassAudioProcessor& p) : proc (p) {}

void MeterView::tick()
{
    auto update = [] (float fresh, float& level, float& hold, int& holdFrames)
    {
        level = juce::jmax (fresh, level * 0.88f);
        if (fresh >= hold)
        {
            hold = fresh;
            holdFrames = 50;
        }
        else if (--holdFrames < 0)
            hold *= 0.94f;
    };
    update (proc.inputPeak.exchange (0.0f), inLevel, inHold, inHoldFrames);
    update (proc.outputPeak.exchange (0.0f), outLevel, outHold, outHoldFrames);
    repaint();
}

void MeterView::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    drawFrame (g, bounds, "SIGNAL // TELEMETRY", Theme::lime);

    auto area = bounds.reduced (12.0f).withTrimmedTop (16.0f);
    const float rowHeight = area.getHeight() / 5.0f;
    constexpr int segments = 36;

    auto dbFraction = [] (float gain) { return juce::jmap (juce::jlimit (-60.0f, 6.0f, juce::Decibels::gainToDecibels (gain, -100.0f)), -60.0f, 6.0f, 0.0f, 1.0f); };

    auto drawRow = [&] (const juce::String& name, float from, float to, float hold, bool levelColours, juce::Colour accent, const juce::String& readout)
    {
        auto row = area.removeFromTop (rowHeight).reduced (0.0f, 3.0f);
        g.setFont (Theme::mono (11.0f, true));
        g.setColour (Theme::dim.brighter (0.5f));
        g.drawText (name, row.removeFromLeft (36.0f), juce::Justification::centredLeft);
        g.setColour (Theme::text);
        g.drawText (readout, row.removeFromRight (56.0f), juce::Justification::centredRight);

        const float segWidth = row.getWidth() / (float) segments;
        for (int s = 0; s < segments; ++s)
        {
            const float t = ((float) s + 0.5f) / (float) segments;
            const bool lit = t >= juce::jmin (from, to) && t <= juce::jmax (from, to);
            juce::Colour c = accent;
            if (levelColours)
                c = t < 0.6f ? Theme::cyan.interpolatedWith (Theme::lime, t / 0.6f)
                             : (t < 0.9f ? Theme::lime.interpolatedWith (Theme::amber, (t - 0.6f) / 0.3f) : Theme::red);
            const auto seg = juce::Rectangle<float> (row.getX() + segWidth * (float) s + 1.0f, row.getY(), segWidth - 2.0f, row.getHeight());
            g.setColour (lit ? c : c.withAlpha (0.08f));
            g.fillRect (seg);
            if (lit)
            {
                g.setColour (c.withAlpha (0.18f));
                g.fillRect (seg.expanded (1.0f, 2.0f));
            }
        }

        if (hold > 0.0f)
        {
            g.setColour (juce::Colours::white);
            g.fillRect (row.getX() + row.getWidth() * hold - 1.0f, row.getY() - 1.0f, 2.0f, row.getHeight() + 2.0f);
        }
    };

    auto dbText = [] (float gain) { return gain < 0.001f ? juce::String ("-inf") : juce::String (juce::Decibels::gainToDecibels (gain), 1); };

    drawRow ("IN", 0.0f, dbFraction (inLevel), inHold > 0.001f ? dbFraction (inHold) : 0.0f, true, Theme::cyan, dbText (inLevel));
    drawRow ("OUT", 0.0f, dbFraction (outLevel), outHold > 0.001f ? dbFraction (outHold) : 0.0f, true, Theme::cyan, dbText (outLevel));

    const float env = proc.liveEnvelope.load();
    drawRow ("ENV", 0.0f, env, 0.0f, false, Theme::lime, juce::String (juce::roundToInt (env * 100.0f)) + "%");

    const float lfo = proc.liveLfo.load();
    drawRow ("LFO", 0.5f, 0.5f + 0.5f * lfo, 0.0f, false, Theme::magenta, (lfo >= 0.0f ? "+" : "") + juce::String (lfo, 2));

    const float seq = proc.liveSeqValue.load();
    drawRow ("SEQ", 0.5f, 0.5f + 0.5f * seq, 0.0f, false, Theme::amber, (seq >= 0.0f ? "+" : "") + juce::String (seq, 2));
}

//==============================================================================
HeaderView::HeaderView (LowpassAudioProcessor& p)
    : proc (p), logo (juce::ImageCache::getFromMemory (BinaryData::skazi_logo_png, BinaryData::skazi_logo_pngSize).createCopy())
{
    // The logo ships on solid black; turn brightness into alpha so it floats on the header.
    logo = logo.convertedToFormat (juce::Image::ARGB);
    juce::Image::BitmapData pixels (logo, juce::Image::BitmapData::readWrite);
    for (int y = 0; y < logo.getHeight(); ++y)
        for (int x = 0; x < logo.getWidth(); ++x)
        {
            const auto c = pixels.getPixelColour (x, y);
            const float level = juce::jmax (c.getFloatRed(), c.getFloatGreen(), c.getFloatBlue());
            if (level < 0.999f)
                pixels.setPixelColour (x, y, level <= 0.0f ? juce::Colours::transparentBlack
                                                           : juce::Colour::fromFloatRGBA (c.getFloatRed() / level, c.getFloatGreen() / level, c.getFloatBlue() / level,
                                                                                          level * c.getFloatAlpha()));
        }

    setInterceptsMouseClicks (false, false);
}

void HeaderView::tick()
{
    time += 1.0f / 60.0f;
    if (glitchFrames > 0)
        --glitchFrames;
    else if (random.nextFloat() < 0.008f)
        glitchFrames = 4 + random.nextInt (8);
    glitchOffset = (random.nextFloat() - 0.5f) * 8.0f;
    repaint();
}

void HeaderView::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    g.setGradientFill (juce::ColourGradient (Theme::panelHi, 0.0f, 0.0f, Theme::background, 0.0f, bounds.getBottom(), false));
    g.fillRect (bounds);

    // Glowing base line, cyan into magenta.
    const auto line = bounds.withTop (bounds.getBottom() - 2.0f);
    g.setGradientFill (juce::ColourGradient (Theme::cyan, line.getX(), 0.0f, Theme::magenta, line.getRight(), 0.0f, false));
    g.fillRect (line);
    g.setGradientFill (juce::ColourGradient (Theme::cyan.withAlpha (0.25f), line.getX(), 0.0f, Theme::magenta.withAlpha (0.25f), line.getRight(), 0.0f, false));
    g.fillRect (line.expanded (0.0f, 3.0f).translated (0.0f, -3.0f));

    // SKAZI logo; it tears into red and cyan copies along with the title when the header glitches.
    const auto logoBox = juce::Rectangle<float> (14.0f, 5.0f, 100.0f, 54.0f);
    if (glitchFrames > 0)
    {
        g.setOpacity (0.55f);
        g.drawImage (logo, logoBox.translated (glitchOffset, 0.0f), juce::RectanglePlacement::centred);
        g.drawImage (logo, logoBox.translated (-glitchOffset, 1.0f), juce::RectanglePlacement::centred);
    }
    g.setOpacity (1.0f);
    g.drawImage (logo, logoBox, juce::RectanglePlacement::centred);

    const auto titleArea = juce::Rectangle<float> (124.0f, 4.0f, 230.0f, 40.0f);
    g.setFont (Theme::sans (34.0f, true).withExtraKerningFactor (0.14f));

    if (glitchFrames > 0)
    {
        g.setColour (Theme::magenta.withAlpha (0.85f));
        g.drawText ("SKILTER", titleArea.translated (glitchOffset, 0.0f), juce::Justification::centredLeft);
        g.setColour (Theme::cyan.withAlpha (0.85f));
        g.drawText ("SKILTER", titleArea.translated (-glitchOffset, 1.0f), juce::Justification::centredLeft);
        g.setColour (juce::Colours::white);
        g.drawText ("SKILTER", titleArea, juce::Justification::centredLeft);
        for (int i = 0; i < 3; ++i)
        {
            g.setColour ((random.nextBool() ? Theme::cyan : Theme::background).withAlpha (0.9f));
            g.fillRect (logoBox.getX() + random.nextFloat() * 300.0f, titleArea.getY() + random.nextFloat() * 36.0f, 30.0f + random.nextFloat() * 90.0f, 2.0f);
        }
    }
    else
    {
        g.setColour (Theme::cyan.withAlpha (0.25f));
        for (auto offset : { juce::Point<float> (-1.5f, 0.0f), juce::Point<float> (1.5f, 0.0f), juce::Point<float> (0.0f, 1.5f) })
            g.drawText ("SKILTER", titleArea.translated (offset.x, offset.y), juce::Justification::centredLeft);
        g.setGradientFill (juce::ColourGradient (Theme::cyan, titleArea.getX(), 0.0f, Theme::magenta, titleArea.getX() + 210.0f, 0.0f, false));
        g.drawText ("SKILTER", titleArea, juce::Justification::centredLeft);
    }

    g.setFont (Theme::mono (10.0f, true));
    g.setColour (Theme::dim.brighter (0.4f));
    g.drawText ("FILTER PRO // HYPERDRIVE CORE", juce::Rectangle<float> (126.0f, 44.0f, 230.0f, 14.0f), juce::Justification::centredLeft);

    // Credit block on the right: the name big and glowing, engine status underneath.
    const auto credit = bounds.withTrimmedLeft (bounds.getWidth() - 278.0f).reduced (14.0f, 6.0f);
    const auto nameArea = credit.withHeight (32.0f);
    g.setFont (Theme::sans (25.0f, true).withExtraKerningFactor (0.08f));
    g.setColour (Theme::magenta.withAlpha (0.22f));
    for (auto offset : { juce::Point<float> (-1.5f, 0.0f), juce::Point<float> (1.5f, 0.0f), juce::Point<float> (0.0f, 1.5f) })
        g.drawText ("ASHER SWISSA", nameArea.translated (offset.x, offset.y), juce::Justification::centredRight);
    g.setGradientFill (juce::ColourGradient (Theme::magenta, nameArea.getX() + 20.0f, 0.0f, Theme::cyan, nameArea.getRight(), 0.0f, false));
    g.drawText ("ASHER SWISSA", nameArea, juce::Justification::centredRight);

    const auto statusLine = credit.withTrimmedTop (36.0f).withHeight (14.0f);
    const float pulse = 0.5f + 0.5f * std::sin (time * 5.0f);
    g.setFont (Theme::mono (10.0f, true));
    const juce::String status = "ENGINE ONLINE  //  FS " + juce::String (proc.liveSampleRate.load() / 1000.0, 1) + "k";
    g.setColour (Theme::lime);
    g.drawText (status, statusLine, juce::Justification::centredRight);
    const float textWidth = (float) status.length() * 6.0f;
    Theme::glowDot (g, { statusLine.getRight() - textWidth - 10.0f, statusLine.getCentreY() }, 2.5f, Theme::lime, 0.5f + 0.5f * pulse);

    g.setColour (Theme::edge.brighter (0.3f));
    g.drawVerticalLine ((int) credit.getX() - 6, credit.getY() + 4.0f, credit.getBottom() - 4.0f);
}

//==============================================================================
LfoPreview::LfoPreview (LowpassAudioProcessor& p) : proc (p) {}

void LfoPreview::tick() { repaint(); }

void LfoPreview::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    g.setColour (Theme::background.withAlpha (0.75f));
    g.fillRect (bounds);
    g.setColour (Theme::edge);
    g.drawRect (bounds, 1.0f);

    const auto area = bounds.reduced (8.0f, 16.0f);
    g.setColour (Theme::edge.withAlpha (0.7f));
    g.drawHorizontalLine ((int) area.getCentreY(), area.getX(), area.getRight());

    const int shape = juce::roundToInt (rawParam (proc, ParamIDs::lfoShape));
    const float depth = rawParam (proc, ParamIDs::lfoDepth);
    const float amplitude = juce::jmax (0.15f, std::abs (depth)) * (depth < 0.0f ? -1.0f : 1.0f);
    const float intensity = std::abs (depth) < 0.005f ? 0.35f : 1.0f;
    const float liveValue = proc.liveLfo.load();

    juce::Path wave;
    for (int i = 0; i <= 200; ++i)
    {
        const float phase = (float) i / 200.0f;
        const float v = shape == 4 ? liveValue : lfoValue (shape, phase, 0.0f);
        const float x = area.getX() + area.getWidth() * phase;
        const float y = area.getCentreY() - v * amplitude * area.getHeight() * 0.5f;
        if (i == 0)
            wave.startNewSubPath (x, y);
        else
            wave.lineTo (x, y);
    }
    Theme::glowStroke (g, wave, Theme::magenta, 1.6f, intensity);

    const float phase = proc.liveLfoPhase.load();
    const juce::Point<float> dot { area.getX() + area.getWidth() * phase, area.getCentreY() - liveValue * amplitude * area.getHeight() * 0.5f };
    g.setColour (Theme::magenta.withAlpha (0.3f * intensity));
    g.drawVerticalLine ((int) dot.x, bounds.getY() + 2.0f, bounds.getBottom() - 2.0f);
    Theme::glowDot (g, dot, 3.0f, juce::Colours::white, intensity);

    const bool synced = rawParam (proc, ParamIDs::lfoSync) > 0.5f;
    const auto rate = synced ? LowpassAudioProcessor::divisionNames[juce::roundToInt (rawParam (proc, ParamIDs::lfoDiv))]
                             : juce::String (rawParam (proc, ParamIDs::lfoRate), 2) + " Hz";
    g.setFont (Theme::mono (10.0f, true));
    g.setColour (Theme::dim.brighter (0.5f));
    g.drawText ((synced ? "SYNC " : "FREE ") + rate, bounds.reduced (6.0f, 2.0f).withHeight (12.0f), juce::Justification::centredLeft);
    g.drawText (LowpassAudioProcessor::shapeNames[shape].toUpperCase(), bounds.reduced (6.0f, 2.0f).withHeight (12.0f), juce::Justification::centredRight);
}

//==============================================================================
SequencerGrid::SequencerGrid (LowpassAudioProcessor& p) : proc (p)
{
    for (int i = 0; i < ParamIDs::numSteps; ++i)
        steps[(size_t) i] = p.apvts.getParameter (ParamIDs::seqStep (i));
    setMouseCursor (juce::MouseCursor::CrosshairCursor);
}

juce::Rectangle<float> SequencerGrid::lane() const
{
    return getLocalBounds().toFloat().reduced (6.0f).withTrimmedTop (10.0f);
}

int SequencerGrid::stepAt (float x) const
{
    const auto area = lane();
    return juce::jlimit (0, ParamIDs::numSteps - 1, (int) ((x - area.getX()) / area.getWidth() * (float) ParamIDs::numSteps));
}

void SequencerGrid::tick()
{
    pulse = std::fmod (pulse + 0.08f, juce::MathConstants<float>::twoPi);
    repaint();
}

void SequencerGrid::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    g.setColour (Theme::background.withAlpha (0.75f));
    g.fillRect (bounds);
    g.setColour (Theme::edge);
    g.drawRect (bounds, 1.0f);

    const auto area = lane();
    const bool running = rawParam (proc, ParamIDs::seqOn) > 0.5f;
    const int length = juce::roundToInt (rawParam (proc, ParamIDs::seqSteps));
    const int current = proc.liveSeqStep.load();
    const float cellWidth = area.getWidth() / (float) ParamIDs::numSteps;

    g.setColour (Theme::edge.withAlpha (0.8f));
    g.drawHorizontalLine ((int) area.getCentreY(), area.getX(), area.getRight());

    for (int i = 0; i < ParamIDs::numSteps; ++i)
    {
        const auto cell = juce::Rectangle<float> (area.getX() + cellWidth * (float) i, area.getY(), cellWidth, area.getHeight()).reduced (2.0f, 0.0f);
        const bool inLoop = i < length;
        const bool isCurrent = running && inLoop && i == current;
        const float v = proc.apvts.getRawParameterValue (ParamIDs::seqStep (i))->load();
        const auto colour = Theme::amber.interpolatedWith (Theme::magenta, (float) i / (float) (ParamIDs::numSteps - 1));
        const float alpha = inLoop ? (running ? 1.0f : 0.55f) : 0.15f;

        if (i % 4 == 0)
        {
            g.setColour (Theme::edge.withAlpha (0.35f));
            g.fillRect (cell);
        }

        const float top = juce::jmin (area.getCentreY(), area.getCentreY() - v * area.getHeight() * 0.5f);
        const auto bar = juce::Rectangle<float> (cell.getX() + 1.0f, top, cell.getWidth() - 2.0f, std::abs (v) * area.getHeight() * 0.5f);
        g.setGradientFill (juce::ColourGradient (colour.withAlpha ((isCurrent ? 0.9f : 0.5f) * alpha), bar.getX(), area.getCentreY() - (v >= 0 ? area.getHeight() * 0.5f : -area.getHeight() * 0.5f),
                                                 colour.withAlpha (0.12f * alpha), bar.getX(), area.getCentreY(), false));
        g.fillRect (bar);

        // Value cap with glow.
        const float capY = area.getCentreY() - v * area.getHeight() * 0.5f;
        juce::Path cap;
        cap.startNewSubPath (cell.getX() + 1.0f, capY);
        cap.lineTo (cell.getRight() - 1.0f, capY);
        Theme::glowStroke (g, cap, colour, isCurrent ? 2.2f : 1.4f, (isCurrent ? 1.0f : 0.6f) * alpha);

        // Step LED.
        const juce::Point<float> led { cell.getCentreX(), bounds.getY() + 7.0f };
        if (isCurrent)
            Theme::glowDot (g, led, 2.4f, colour, 0.75f + 0.25f * std::sin (pulse));
        else
        {
            g.setColour (colour.withAlpha (0.25f * alpha));
            g.fillEllipse (juce::Rectangle<float> (3.0f, 3.0f).withCentre (led));
        }
    }

    // Loop end marker.
    if (length < ParamIDs::numSteps)
    {
        const float x = area.getX() + cellWidth * (float) length;
        g.setColour (Theme::amber.withAlpha (0.7f));
        g.drawVerticalLine ((int) x, area.getY(), area.getBottom());
    }
}

void SequencerGrid::drawTo (juce::Point<float> p)
{
    const auto area = lane();
    const int step = stepAt (p.x);
    const float value = juce::jlimit (-1.0f, 1.0f, (area.getCentreY() - p.y) / (area.getHeight() * 0.5f));

    // Fast drags skip cells, so fill every step between the last one and this one.
    const int from = lastStep < 0 ? step : lastStep;
    for (int i = juce::jmin (from, step); i <= juce::jmax (from, step); ++i)
    {
        auto* param = steps[(size_t) i];
        if (! touched[(size_t) i])
        {
            param->beginChangeGesture();
            touched[(size_t) i] = true;
        }
        param->setValueNotifyingHost (param->convertTo0to1 (value));
    }
    lastStep = step;
}

void SequencerGrid::mouseDown (const juce::MouseEvent& e)
{
    lastStep = -1;
    drawTo (e.position);
}

void SequencerGrid::mouseDrag (const juce::MouseEvent& e)
{
    drawTo (e.position);
}

void SequencerGrid::mouseUp (const juce::MouseEvent&)
{
    for (int i = 0; i < ParamIDs::numSteps; ++i)
        if (touched[(size_t) i])
        {
            steps[(size_t) i]->endChangeGesture();
            touched[(size_t) i] = false;
        }
    lastStep = -1;
}

void SequencerGrid::mouseDoubleClick (const juce::MouseEvent& e)
{
    auto* param = steps[(size_t) stepAt (e.position.x)];
    param->beginChangeGesture();
    param->setValueNotifyingHost (param->convertTo0to1 (0.0f));
    param->endChangeGesture();
}

//==============================================================================
EnvHistory::EnvHistory (LowpassAudioProcessor& p) : proc (p) {}

void EnvHistory::tick()
{
    values[head] = proc.liveEnvelope.load();
    head = (head + 1) % values.size();
    repaint();
}

void EnvHistory::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    g.setColour (Theme::background.withAlpha (0.75f));
    g.fillRect (bounds);
    g.setColour (Theme::edge);
    g.drawRect (bounds, 1.0f);

    const auto area = bounds.reduced (6.0f).withTrimmedTop (12.0f);
    g.setColour (Theme::edge.withAlpha (0.6f));
    for (int i = 1; i < 4; ++i)
        g.drawHorizontalLine ((int) (area.getY() + area.getHeight() * (float) i / 4.0f), area.getX(), area.getRight());

    juce::Path line;
    const auto count = values.size();
    for (size_t i = 0; i < count; ++i)
    {
        const float v = values[(head + i) % count];
        const float x = area.getX() + area.getWidth() * (float) i / (float) (count - 1);
        const float y = area.getBottom() - v * area.getHeight();
        if (i == 0)
            line.startNewSubPath (x, y);
        else
            line.lineTo (x, y);
    }

    auto fill = line;
    fill.lineTo (area.getRight(), area.getBottom());
    fill.lineTo (area.getX(), area.getBottom());
    fill.closeSubPath();
    g.setGradientFill (juce::ColourGradient (Theme::lime.withAlpha (0.35f), 0.0f, area.getY(), Theme::lime.withAlpha (0.0f), 0.0f, area.getBottom(), false));
    g.fillPath (fill);
    Theme::glowStroke (g, line, Theme::lime, 1.4f, 0.9f);

    const float amount = rawParam (proc, ParamIDs::envAmount);
    g.setFont (Theme::mono (10.0f, true));
    g.setColour (Theme::dim.brighter (0.5f));
    g.drawText ("FOLLOWER", bounds.reduced (6.0f, 2.0f).withHeight (12.0f), juce::Justification::centredLeft);
    g.drawText ("MOD " + juce::String (amount * 5.0f, 1) + " OCT", bounds.reduced (6.0f, 2.0f).withHeight (12.0f), juce::Justification::centredRight);
}

//==============================================================================
PresetBar::PresetBar (LowpassAudioProcessor& p) : proc (p)
{
    for (auto* b : { &previous, &next, &random })
        addAndMakeVisible (b);

    previous.onClick = [this] { step (-1); };
    next.onClick = [this] { step (1); };
    random.onClick = [this]
    {
        proc.randomise();
        random.flash = 1.0f;
    };
}

void PresetBar::step (int delta)
{
    const int count = (int) factoryPresets().size();
    proc.loadPreset ((proc.getCurrentProgram() + delta + count) % count);
}

void PresetBar::showMenu()
{
    juce::PopupMenu menu;
    const auto& presets = factoryPresets();
    juce::String category;
    juce::PopupMenu sub;

    for (int i = 0; i <= (int) presets.size(); ++i)
    {
        if (i == (int) presets.size() || presets[(size_t) i].category != category)
        {
            if (category.isNotEmpty())
                menu.addSubMenu (category.toUpperCase(), sub);
            if (i == (int) presets.size())
                break;
            category = presets[(size_t) i].category;
            sub = {};
        }
        sub.addItem (i + 1, presets[(size_t) i].name, true, i == proc.getCurrentProgram() && proc.getPresetName() == presets[(size_t) i].name);
    }

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMinimumWidth (nameArea.getWidth()),
                        [safe = juce::Component::SafePointer<PresetBar> (this)] (int result)
                        {
                            if (safe != nullptr && result > 0)
                                safe->proc.loadPreset (result - 1);
                        });
}

void PresetBar::tick()
{
    if (random.flash > 0.0f)
    {
        random.flash = juce::jmax (0.0f, random.flash - 0.05f);
        random.repaint();
    }
    const float target = isMouseOver (false) ? 1.0f : 0.0f;
    const auto name = proc.getPresetName();
    if (name != shownName || std::abs (target - hover) > 0.01f)
    {
        hover += (target - hover) * 0.2f;
        shownName = name;
        repaint();
    }
}

void PresetBar::resized()
{
    auto area = getLocalBounds();
    random.setBounds (area.removeFromRight (150));
    area.removeFromRight (10);
    previous.setBounds (area.removeFromLeft (36));
    next.setBounds (area.removeFromRight (36));
    nameArea = area.reduced (4, 0);
}

void PresetBar::paint (juce::Graphics& g)
{
    const auto box = nameArea.toFloat().reduced (0.0f, 2.0f);
    g.setColour (Theme::background.withAlpha (0.85f));
    g.fillRect (box);
    juce::Path outline;
    outline.addRectangle (box);
    Theme::glowStroke (g, outline, Theme::cyan, 1.0f, 0.3f + 0.4f * hover);

    const auto& presets = factoryPresets();
    const int index = proc.getCurrentProgram();
    const bool isFactory = juce::isPositiveAndBelow (index, (int) presets.size()) && presets[(size_t) index].name == shownName;

    auto text = box.reduced (12.0f, 0.0f);
    g.setFont (Theme::mono (11.0f, true));
    g.setColour (Theme::magenta);
    const auto category = isFactory ? presets[(size_t) index].category.toUpperCase() : juce::String ("RANDOMISED");
    g.drawText (category, text.removeFromLeft (170.0f), juce::Justification::centredLeft);

    g.setColour (Theme::dim.brighter (0.4f));
    g.drawText (isFactory ? juce::String (index + 1).paddedLeft ('0', 3) + " / " + juce::String ((int) presets.size()) : juce::String ("--- / 150"),
                text.removeFromRight (100.0f), juce::Justification::centredRight);

    g.setColour (Theme::text);
    g.setFont (Theme::mono (15.0f, true));
    g.drawText (shownName.toUpperCase() + "  v", text, juce::Justification::centred);
}

void PresetBar::mouseDown (const juce::MouseEvent& e)
{
    if (nameArea.contains (e.getPosition()))
        showMenu();
}
