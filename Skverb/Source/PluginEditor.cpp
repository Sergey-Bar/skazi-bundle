#include "PluginEditor.h"

namespace
{
    juce::RangedAudioParameter& param (SkverbAudioProcessor& p, const char* id)
    {
        return *p.apvts.getParameter (id);
    }

    juce::StringArray modeNames()
    {
        juce::StringArray names;
        for (int m = 0; m < verb::numModes; ++m)
            names.add (verb::spec (m).name);
        return names;
    }
}

SkverbAudioProcessorEditor::SkverbAudioProcessorEditor (SkverbAudioProcessor& p)
    : AudioProcessorEditor (&p), proc (p),
      space (p), meters (p),
      modeSelector (param (p, ParamIDs::mode), modeNames()),
      filterSelector (param (p, ParamIDs::fltType), TailFilter::names, true)
{
    setLookAndFeel (&lookAndFeel);
    addAndMakeVisible (canvas);

    for (auto* c : std::initializer_list<juce::Component*> { &header, &space, &meters, &modeSelector, &filterSelector,
                                                             &presetBox, &prevPreset, &nextPreset, &randomButton })
        canvas.addAndMakeVisible (c);

    attach (decay, ParamIDs::decay, "0.1", "30");
    attach (preDelay, ParamIDs::preDelay, "0", "1000");
    attach (feedback, ParamIDs::feedback, "0", "95");
    attach (size, ParamIDs::size, "0", "100");
    attach (lowCut, ParamIDs::lowCut, "20", "1k");
    attach (highCut, ParamIDs::highCut, "1k", "20k");
    attach (damping, ParamIDs::damping, "0", "100");
    attach (bassMult, ParamIDs::bassMult, "x.25", "x4");
    attach (diffusion, ParamIDs::diffusion, "0", "100");
    attach (early, ParamIDs::early, "0", "100");
    attach (width, ParamIDs::width, "0", "200");
    attach (modRate, ParamIDs::modRate, "0.05", "5");
    attach (modDepth, ParamIDs::modDepth, "0", "100");
    attach (shimmerPitch, ParamIDs::shimmerPitch, "-24", "+24");
    attach (shimmerAmt, ParamIDs::shimmerAmt, "0", "100");
    attach (duck, ParamIDs::duck, "0", "100");
    attach (duckRelease, ParamIDs::duckRelease, "20", "1000");
    attach (drive, ParamIDs::drive, "0", "100");
    attach (mix, ParamIDs::mix, "DRY", "WET");
    attach (output, ParamIDs::output, "-24", "+12");
    attach (revAmount, ParamIDs::revAmount, "0", "100");
    attach (revTime, ParamIDs::revTime, "0.1", "2.5");
    attach (fltCutoff, ParamIDs::fltCutoff, "40", "18k");
    attach (fltReso, ParamIDs::fltReso, "0", "100");
    attach (fltEnv, ParamIDs::fltEnv, "-", "+");
    attach (freeze, ParamIDs::freeze);
    attach (sync, ParamIDs::sync);
    attach (killDry, ParamIDs::killDry);

    // With SYNC on, the pre-delay knob reads as a note value.
    preDelay.textFromValueFunction = [this] (double v)
    {
        if (proc.apvts.getRawParameterValue (ParamIDs::sync)->load() > 0.5f)
        {
            const float norm = param (proc, ParamIDs::preDelay).convertTo0to1 ((float) v);
            return Sync::names[juce::jlimit (0, Sync::count - 1, juce::roundToInt (norm * (float) (Sync::count - 1)))];
        }
        return juce::String (juce::roundToInt (v)) + " ms";
    };

    for (int i = 0; i < proc.getNumPrograms(); ++i)
        presetBox.addItem (proc.getProgramName (i), i + 1);
    presetBox.setJustificationType (juce::Justification::centred);
    presetBox.setSelectedId (proc.getCurrentProgram() + 1, juce::dontSendNotification);
    presetBox.onChange = [this]
    {
        const int index = presetBox.getSelectedId() - 1;
        if (index >= 0 && (index != proc.getCurrentProgram() || proc.randomised))
            proc.setCurrentProgram (index);
    };
    prevPreset.onClick = [this] { stepPreset (-1); };
    nextPreset.onClick = [this] { stepPreset (1); };
    randomButton.onClick = [this]
    {
        proc.randomise();
        randomButton.flash = 1.0f;
    };

    canvas.setBounds (0, 0, designWidth, designHeight);
    layout();

    float initialScale = 1.0f;
    if (auto* screen = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay())
        initialScale = juce::jmin (1.0f, (float) screen->userArea.getHeight() * 0.85f / (float) designHeight);

    setResizable (true, true);
    setResizeLimits (designWidth / 2, designHeight / 2, designWidth * 2, designHeight * 2);
    getConstrainer()->setFixedAspectRatio ((double) designWidth / (double) designHeight);
    setSize (juce::roundToInt ((float) designWidth * initialScale), juce::roundToInt ((float) designHeight * initialScale));

    startTimerHz (60);
}

SkverbAudioProcessorEditor::~SkverbAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void SkverbAudioProcessorEditor::attach (Knob& knob, const char* paramID, const juce::String& scaleMin, const juce::String& scaleMax)
{
    canvas.addAndMakeVisible (knob);
    knob.setScaleLabels (scaleMin, scaleMax);
    sliderAttachments.push_back (std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, paramID, knob));
    auto& p = param (proc, paramID);
    knob.setDoubleClickReturnValue (true, p.convertFrom0to1 (p.getDefaultValue()));
}

void SkverbAudioProcessorEditor::attach (LedToggle& toggle, const char* paramID)
{
    canvas.addAndMakeVisible (toggle);
    buttonAttachments.push_back (std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, paramID, toggle));
}

void SkverbAudioProcessorEditor::stepPreset (int delta)
{
    const int count = proc.getNumPrograms();
    proc.setCurrentProgram ((proc.getCurrentProgram() + delta + count) % count);
    (delta < 0 ? prevPreset : nextPreset).flash = 1.0f;
}

void SkverbAudioProcessorEditor::timerCallback()
{
    const float in = proc.inPeak.exchange (0.0f);
    const float wetLevel = proc.wetPeak.exchange (0.0f);
    const float out = proc.outPeak.exchange (0.0f);

    space.tick (in, wetLevel);
    meters.tick (in, wetLevel, out);
    // SYNC follows the host tempo, so the note readout can change by itself; repaint only when it does.
    if (const auto text = preDelay.getTextFromValue (preDelay.getValue()); text != shownPreDelay)
    {
        shownPreDelay = text;
        preDelay.repaint();
    }

    for (auto* b : { &prevPreset, &nextPreset, &randomButton })
        if (b->flash > 0.0f)
        {
            b->flash = juce::jmax (0.0f, b->flash - 0.06f);
            b->repaint();
        }

    if (proc.randomised)
    {
        if (presetBox.getText() != "RANDOM PATCH")
            presetBox.setText ("RANDOM PATCH", juce::dontSendNotification);
    }
    else if (presetBox.getSelectedId() != proc.getCurrentProgram() + 1)
        presetBox.setSelectedId (proc.getCurrentProgram() + 1, juce::dontSendNotification);

    // Controls that do nothing in the current state are dimmed.
    const int mode = (int) proc.apvts.getRawParameterValue (ParamIDs::mode)->load();
    const bool frozen = proc.apvts.getRawParameterValue (ParamIDs::freeze)->load() > 0.5f;
    const bool erOn = verb::spec (mode).early > 0.0f;
    if (early.isEnabled() != erOn)
        early.setEnabled (erOn);
    if (decay.isEnabled() == frozen)
        decay.setEnabled (! frozen);
    const bool duckOn = proc.apvts.getRawParameterValue (ParamIDs::duck)->load() > 0.05f;
    if (duckRelease.isEnabled() != duckOn)
        duckRelease.setEnabled (duckOn);
    const bool layerOn = mode != verb::reverse;   // the REVERSE algorithm is already fully reversed
    for (auto* k : { &revAmount, &revTime })
        if (k->isEnabled() != layerOn)
            k->setEnabled (layerOn);
    const bool filterOn = proc.apvts.getRawParameterValue (ParamIDs::fltType)->load() > 0.5f;
    for (auto* k : { &fltCutoff, &fltReso, &fltEnv })
        if (k->isEnabled() != filterOn)
            k->setEnabled (filterOn);
}

void SkverbAudioProcessorEditor::layout()
{
    // The faceplate between the rack ears runs from x 50 to 1150.
    header.setBounds (50, 10, 600, 66);
    prevPreset.setBounds (722, 26, 40, 38);
    presetBox.setBounds (764, 24, 262, 42);
    nextPreset.setBounds (1028, 26, 40, 38);
    randomButton.setBounds (1072, 26, 80, 38);

    displayBounds = { 52, 102, 696, 304 };
    space.setBounds (displayBounds);

    sections = {
        { { 776, 96, 374, 16 },  "MAIN" },
        { { 50, 432, 1100, 16 }, "ALGORITHM" },
        { { 50, 532, 374, 16 },  "TONE" },
        { { 448, 532, 280, 16 }, "CHARACTER" },
        { { 752, 532, 187, 16 }, "MODULATION" },
        { { 963, 532, 187, 16 }, "SHIMMER" },
        { { 50, 706, 200, 16 },  "REVERSE" },
        { { 274, 706, 430, 16 }, "TAIL FILTER" },
        { { 728, 706, 422, 16 }, "DYNAMICS" },
        { { 50, 880, 386, 16 },  "OUTPUT" },
        { { 460, 880, 690, 16 }, "LEVEL" },
    };

    auto row = [] (juce::Rectangle<int> area, std::initializer_list<Knob*> knobs)
    {
        const int w = area.getWidth() / (int) knobs.size();
        int i = 0;
        for (auto* k : knobs)
            k->setBounds (area.getX() + i++ * w, area.getY(), w, area.getHeight());
    };

    row ({ 776, 118, 374, 124 }, { &decay, &preDelay });
    row ({ 776, 246, 374, 124 }, { &size, &feedback });
    freeze.setBounds (790, 372, 166, 36);
    sync.setBounds (970, 372, 166, 36);

    modeSelector.setBounds (50, 454, 1100, 56);

    row ({ 50, 554, 374, 128 }, { &lowCut, &highCut, &damping, &bassMult });
    row ({ 448, 554, 280, 128 }, { &diffusion, &early, &width });
    row ({ 752, 554, 187, 128 }, { &modRate, &modDepth });
    row ({ 963, 554, 187, 128 }, { &shimmerPitch, &shimmerAmt });

    row ({ 50, 728, 200, 128 }, { &revAmount, &revTime });
    filterSelector.setBounds (280, 730, 70, 124);
    row ({ 356, 728, 348, 128 }, { &fltCutoff, &fltReso, &fltEnv });
    row ({ 728, 728, 422, 128 }, { &duck, &duckRelease, &drive });

    row ({ 50, 902, 260, 128 }, { &mix, &output });
    killDry.setBounds (316, 948, 120, 36);
    meters.setBounds (meterWindow);
}

juce::Image SkverbAudioProcessorEditor::renderFaceTexture() const
{
    // A plain software image: fast to write pixel by pixel, and nothing for the graphics driver to free later.
    juce::Image texture (juce::Image::RGB, designWidth, designHeight, false, juce::SoftwareImageType());
    {
        juce::Graphics g (texture);
        g.setGradientFill (juce::ColourGradient (Theme::plateTop, 0.0f, 0.0f, Theme::plateBottom, 0.0f, (float) designHeight, false));
        g.fillAll();
        juce::Random brush (2024);
        for (float y = 0.0f; y < (float) designHeight; y += 1.0f)
        {
            float x = 0.0f;
            while (x < (float) designWidth)
            {
                const float len = 40.0f + brush.nextFloat() * 260.0f;
                g.setColour ((brush.nextBool() ? juce::Colours::white : juce::Colours::black).withAlpha (0.012f + brush.nextFloat() * 0.018f));
                g.fillRect (x, y, len, 1.0f);
                x += len + brush.nextFloat() * 30.0f;
            }
        }
    }

    // Fine powder-coat grain: nudge each pixel's brightness a little.
    juce::Image::BitmapData pixels (texture, juce::Image::BitmapData::readWrite);
    juce::Random noise (77);
    for (int y = 0; y < designHeight; ++y)
    {
        auto* p = pixels.getLinePointer (y);
        for (int x = 0; x < designWidth; ++x, p += pixels.pixelStride)
        {
            const int delta = (int) ((noise.nextFloat() - 0.5f) * 9.0f);
            for (int c = 0; c < 3; ++c)
                p[c] = (juce::uint8) juce::jlimit (0, 255, (int) p[c] + delta);
        }
    }
    return texture;
}

void SkverbAudioProcessorEditor::renderBackground()
{
    const float scale = juce::Component::getApproximateScaleFactorForComponent (&canvas);
    auto& image = canvas.background;
    image = juce::Image (juce::Image::ARGB, juce::roundToInt ((float) designWidth * scale), juce::roundToInt ((float) designHeight * scale), true);
    juce::Graphics g (image);
    g.addTransform (juce::AffineTransform::scale (scale));

    const auto bounds = juce::Rectangle<float> (0.0f, 0.0f, (float) designWidth, (float) designHeight);
    const float earWidth = 34.0f;

    // Anodised faceplate: the brush and grain are rendered once per window and only scaled on resize.
    if (! faceTexture.isValid())
        faceTexture = renderFaceTexture();
    g.drawImage (faceTexture, bounds);
    // Light falling from above.
    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.06f), 0.0f, 0.0f, juce::Colours::transparentWhite, 0.0f, 260.0f, false));
    g.fillRect (bounds);
    g.setGradientFill (juce::ColourGradient (juce::Colours::black.withAlpha (0.25f), 0.0f, bounds.getBottom(), juce::Colours::transparentBlack, 0.0f, bounds.getBottom() - 160.0f, false));
    g.fillRect (bounds);

    // Rack ears with mounting slots.
    for (float x : { 0.0f, bounds.getRight() - earWidth })
    {
        const auto ear = juce::Rectangle<float> (x, 0.0f, earWidth, bounds.getHeight());
        const bool left = x < 1.0f;
        g.setGradientFill (juce::ColourGradient (Theme::ear.brighter (0.08f), ear.getX(), 0.0f, Theme::ear, ear.getRight(), 0.0f, false));
        g.fillRect (ear);
        g.setColour (juce::Colours::black.withAlpha (0.8f));
        g.fillRect (left ? ear.getRight() - 1.0f : ear.getX(), 0.0f, 1.0f, ear.getHeight());
        g.setColour (Theme::bevel.withAlpha (0.4f));
        g.fillRect (left ? ear.getRight() : ear.getX() - 1.0f, 0.0f, 1.0f, ear.getHeight());
        for (float y : { 52.0f, bounds.getBottom() - 52.0f })
        {
            const auto slot = juce::Rectangle<float> (16.0f, 26.0f).withCentre ({ ear.getCentreX(), y });
            g.setColour (juce::Colours::black);
            g.fillRoundedRectangle (slot, 8.0f);
            g.setColour (Theme::bevel.withAlpha (0.35f));
            g.drawRoundedRectangle (slot.translated (0.0f, 1.0f), 8.0f, 0.8f);
            Theme::screw (g, slot.getCentre().translated (0.0f, -3.0f), 6.5f);
        }

        // Front handle: a polished tube between two mounting blocks.
        const float hx = ear.getCentreX(), top = 112.0f, bottom = bounds.getBottom() - 112.0f;
        juce::Path tube;
        tube.addRoundedRectangle (juce::Rectangle<float> (hx - 7.0f, top, 14.0f, bottom - top), 7.0f);
        juce::DropShadow (juce::Colours::black.withAlpha (0.85f), 10, { 4, 6 }).drawForPath (g, tube);
        juce::ColourGradient chrome (juce::Colour (0xff2a2c30), hx - 7.0f, 0.0f, juce::Colour (0xff1a1b1d), hx + 7.0f, 0.0f, false);
        chrome.addColour (0.28, juce::Colour (0xffd9dce1));
        chrome.addColour (0.45, juce::Colour (0xff8e9196));
        chrome.addColour (0.75, juce::Colour (0xff3c3f43));
        g.setGradientFill (chrome);
        g.fillPath (tube);
        for (float y : { top, bottom - 30.0f })
        {
            const auto block = juce::Rectangle<float> (hx - 12.0f, y - 2.0f, 24.0f, 34.0f);
            juce::Path b;
            b.addRoundedRectangle (block, 4.0f);
            juce::DropShadow (juce::Colours::black.withAlpha (0.8f), 8, { 2, 4 }).drawForPath (g, b);
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3a3d41), block.getX(), block.getY(), juce::Colour (0xff141517), block.getX(), block.getBottom(), false));
            g.fillPath (b);
            g.setColour (juce::Colours::white.withAlpha (0.15f));
            g.drawHorizontalLine ((int) block.getY() + 1, block.getX() + 3.0f, block.getRight() - 3.0f);
            Theme::screw (g, block.getCentre(), 4.5f);
        }
    }
    g.setColour (Theme::bevel.withAlpha (0.8f));
    g.fillRect (earWidth, 0.0f, bounds.getWidth() - earWidth * 2.0f, 1.0f);
    g.setColour (juce::Colours::black);
    g.fillRect (0.0f, bounds.getBottom() - 1.0f, bounds.getWidth(), 1.0f);

    // Power indicator.
    Theme::led (g, { 682.0f, 40.0f }, 3.6f, Theme::ledGreen, 1.0f);
    Theme::legendText (g, "POWER", { 652.0f, 52.0f, 60.0f, 12.0f }, juce::Justification::centred, Theme::legendDim, 0.2f, 9.0f);

    // Meter window.
    Theme::bezel (g, meterWindow.toFloat(), 6.0f);

    Theme::legendText (g, "PROGRAM", { 768.0f, 8.0f, 200.0f, 14.0f }, juce::Justification::centredLeft, Theme::legendDim, 0.22f, 10.0f);

    // Section grooves.
    Theme::grooveH (g, 50.0f, 1150.0f, 84.0f);
    Theme::bezel (g, displayBounds.toFloat());
    Theme::grooveV (g, 762.0f, 98.0f, 410.0f);
    Theme::grooveH (g, 50.0f, 1150.0f, 422.0f);
    Theme::grooveH (g, 50.0f, 1150.0f, 522.0f);
    Theme::grooveH (g, 50.0f, 1150.0f, 696.0f);
    for (float x : { 436.0f, 740.0f, 951.0f })
        Theme::grooveV (g, x, 536.0f, 684.0f);
    for (float x : { 262.0f, 716.0f })
        Theme::grooveV (g, x, 710.0f, 858.0f);
    Theme::grooveH (g, 50.0f, 1150.0f, 870.0f);
    Theme::grooveV (g, 448.0f, 884.0f, 1032.0f);

    for (const auto& section : sections)
        Theme::sectionTitle (g, section.title, section.bounds.toFloat());

    const auto footer = juce::Rectangle<float> (50.0f, bounds.getBottom() - 22.0f, 1100.0f, 14.0f);
    Theme::legendText (g, "SKAZI REVERB  V1.1", footer, juce::Justification::centredLeft, Theme::legendDim, 0.22f, 9.5f);
    Theme::legendText (g, "DESIGNED BY ASHER SWISSA", footer, juce::Justification::centredRight, Theme::legendDim, 0.22f, 9.5f);
}

void SkverbAudioProcessorEditor::resized()
{
    const float scale = (float) getWidth() / (float) designWidth;
    canvas.setTransform (juce::AffineTransform::scale (scale));

    if (std::abs (scale - renderedScale) > 0.001f)
    {
        renderedScale = scale;
        renderBackground();
        canvas.repaint();
    }
}
