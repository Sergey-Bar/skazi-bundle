#include "PluginEditor.h"

namespace
{
    SegmentedSelector::IconPainter shapeIcon (int shape)
    {
        return [shape] (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour c)
        {
            static constexpr float steps[] { 0.2f, -0.7f, 0.9f, -0.2f };
            juce::Path p;
            for (int i = 0; i <= 40; ++i)
            {
                const float phase = (float) i / 40.0f;
                const float v = shape == 4 ? steps[juce::jmin (3, (int) (phase * 4.0f))] : lfoValue (shape, phase, 0.0f);
                const juce::Point<float> pt { r.getX() + r.getWidth() * phase, r.getCentreY() - v * r.getHeight() * 0.5f };
                if (i == 0)
                    p.startNewSubPath (pt);
                else
                    p.lineTo (pt);
            }
            g.setColour (c);
            g.strokePath (p, juce::PathStrokeType (1.6f, juce::PathStrokeType::mitered, juce::PathStrokeType::rounded));
        };
    }

    juce::RangedAudioParameter& param (LowpassAudioProcessor& p, const char* id)
    {
        return *p.apvts.getParameter (id);
    }
}

LowpassAudioProcessorEditor::LowpassAudioProcessorEditor (LowpassAudioProcessor& p)
    : AudioProcessorEditor (&p), proc (p),
      header (p), display (p), scope (p), meters (p), lfoPreview (p), envHistory (p), sequencer (p), presetBar (p),
      morphSelector (param (p, ParamIDs::morph), { "LP", "BP", "HP", "NOTCH" }, Theme::cyan),
      slopeSelector (param (p, ParamIDs::slope), { "12dB", "24dB", "36dB", "48dB" }, Theme::magenta),
      oversamplingSelector (param (p, ParamIDs::oversampling), { "1x", "2x", "4x" }, Theme::lime),
      shapeSelector (param (p, ParamIDs::lfoShape), LowpassAudioProcessor::shapeNames, Theme::magenta),
      driveTypeSelector (param (p, ParamIDs::driveType), { "TUBE", "TAPE", "HARD" }, Theme::red),
      drivePosSelector (param (p, ParamIDs::drivePos), { "PRE", "POST" }, Theme::red),
      stereoModeSelector (param (p, ParamIDs::stereoMode), { "STEREO", "MID/SIDE", "DUAL L/R" }, Theme::cyan)
{
    setLookAndFeel (&lookAndFeel);
    addAndMakeVisible (canvas);

    for (int i = 0; i < 5; ++i)
        shapeSelector.icons.push_back (shapeIcon (i));

    for (auto* c : std::initializer_list<juce::Component*> { &header, &display, &scope, &meters, &lfoPreview, &envHistory, &sequencer, &presetBar,
                                                             &morphSelector, &slopeSelector, &oversamplingSelector, &shapeSelector,
                                                             &driveTypeSelector, &drivePosSelector, &stereoModeSelector,
                                                             &syncToggle, &seqToggle, &divisionBox, &seqRateBox })
        canvas.addAndMakeVisible (c);

    attach (cutoff, ParamIDs::cutoff, proc.liveCutoff[0]);
    attach (resonance, ParamIDs::resonance, proc.liveResonance);
    attach (morph, ParamIDs::morph, proc.liveMorph);
    attach (glide, ParamIDs::glide);
    attach (drive, ParamIDs::drive, proc.liveDrive);
    attach (lfoRate, ParamIDs::lfoRate);
    attach (lfoDepth, ParamIDs::lfoDepth);
    attach (lfoStereo, ParamIDs::lfoStereo);
    attach (envAmount, ParamIDs::envAmount);
    attach (envAttack, ParamIDs::envAttack);
    attach (envRelease, ParamIDs::envRelease);
    attach (seqSteps, ParamIDs::seqSteps);
    attach (seqDepth, ParamIDs::seqDepth);
    attach (seqGlide, ParamIDs::seqGlide);
    attach (macro, ParamIDs::macro);
    attach (macroCutoff, ParamIDs::macroCutoff);
    attach (macroRes, ParamIDs::macroRes);
    attach (macroDrive, ParamIDs::macroDrive);
    attach (macroMix, ParamIDs::macroMix);
    attach (macroMorph, ParamIDs::macroMorph);
    attach (inGain, ParamIDs::inGain);
    attach (outGain, ParamIDs::outGain);
    attach (mix, ParamIDs::mix, proc.liveMix);
    attach (spread, ParamIDs::spread);
    attach (stereoOffset, ParamIDs::stereoOffset);

    for (auto* knob : { &macroCutoff, &macroRes, &macroDrive, &macroMix, &macroMorph })
        knob->setCompact (true);

    for (auto* box : { &divisionBox, &seqRateBox })
    {
        box->addItemList (LowpassAudioProcessor::divisionNames, 1);
        box->setJustificationType (juce::Justification::centred);
    }
    divisionAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (p.apvts, ParamIDs::lfoDiv, divisionBox);
    seqRateAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (p.apvts, ParamIDs::seqRate, seqRateBox);
    syncAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, ParamIDs::lfoSync, syncToggle);
    seqAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, ParamIDs::seqOn, seqToggle);

    canvas.setBounds (0, 0, designWidth, designHeight);
    layout();

    // Open at a size that fits the screen, then let the user drag the corner to scale it.
    float initialScale = 1.0f;
    if (auto* screen = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay())
        initialScale = juce::jmin (1.0f, (float) screen->userArea.getHeight() * 0.85f / (float) designHeight);

    setResizable (true, true);
    setResizeLimits (designWidth / 2, designHeight / 2, designWidth * 2, designHeight * 2);
    getConstrainer()->setFixedAspectRatio ((double) designWidth / (double) designHeight);
    setSize (juce::roundToInt ((float) designWidth * initialScale), juce::roundToInt ((float) designHeight * initialScale));

    startTimerHz (60);
}

LowpassAudioProcessorEditor::~LowpassAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void LowpassAudioProcessorEditor::attach (NeonKnob& knob, const char* paramID)
{
    canvas.addAndMakeVisible (knob);
    sliderAttachments.push_back (std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, paramID, knob));
    auto& p = param (proc, paramID);
    knob.setDoubleClickReturnValue (true, p.convertFrom0to1 (p.getDefaultValue()));
}

void LowpassAudioProcessorEditor::attach (NeonKnob& knob, const char* paramID, std::atomic<float>& liveValue)
{
    attach (knob, paramID);
    // The knob also shows where modulation and the macro are pushing the value right now.
    auto* p = &param (proc, paramID);
    knob.modulation = [p, &liveValue] { return p->convertTo0to1 (liveValue.load()); };
}

void LowpassAudioProcessorEditor::timerCallback()
{
    header.tick();
    display.tick();
    scope.tick();
    meters.tick();
    lfoPreview.tick();
    envHistory.tick();
    sequencer.tick();
    presetBar.tick();

    for (auto* knob : { &cutoff, &resonance, &morph, &drive, &mix })
        knob->repaint();

    const bool synced = proc.apvts.getRawParameterValue (ParamIDs::lfoSync)->load() > 0.5f;
    if (lfoRate.isEnabled() == synced)
    {
        lfoRate.setEnabled (! synced);
        divisionBox.setEnabled (synced);
    }

    const bool routed = proc.apvts.getRawParameterValue (ParamIDs::stereoMode)->load() > 0.5f;
    if (stereoOffset.isEnabled() != routed)
        stereoOffset.setEnabled (routed);
}

void LowpassAudioProcessorEditor::layout()
{
    header.setBounds (0, 0, designWidth, 64);
    morphSelector.setBounds (360, 15, 240, 34);
    slopeSelector.setBounds (612, 15, 200, 34);
    oversamplingSelector.setBounds (824, 15, 126, 34);

    presetBar.setBounds (16, 72, 1208, 32);

    display.setBounds (16, 112, 860, 300);
    scope.setBounds (888, 112, 336, 170);
    meters.setBounds (888, 290, 336, 122);

    panels = {
        { { 16, 424, 470, 240 },  "01", "FILTER CORE", Theme::cyan },
        { { 498, 424, 250, 240 }, "02", "DRIVE", Theme::red },
        { { 760, 424, 464, 240 }, "03", "OUTPUT + STEREO", Theme::cyan },
        { { 16, 676, 290, 250 },  "04", "LFO", Theme::magenta },
        { { 318, 676, 240, 250 }, "05", "ENV FOLLOW", Theme::lime },
        { { 570, 676, 410, 250 }, "06", "STEP SEQUENCER", Theme::amber },
        { { 992, 676, 232, 250 }, "07", "MACRO", Theme::magenta },
    };

    auto content = [this] (size_t i) { return panels[i].bounds.reduced (8).withTrimmedTop (32); };

    auto row = [] (juce::Rectangle<int> area, std::initializer_list<NeonKnob*> knobs, int maxHeight)
    {
        const int w = area.getWidth() / (int) knobs.size();
        int i = 0;
        for (auto* k : knobs)
            k->setBounds (juce::Rectangle<int> (area.getX() + i++ * w, area.getY(), w, area.getHeight())
                              .withSizeKeepingCentre (w - 4, juce::jmin (area.getHeight(), maxHeight)));
    };

    row (content (0), { &cutoff, &resonance, &morph, &glide }, 150);

    auto driveArea = content (1);
    drive.setBounds (driveArea.removeFromTop (124).withSizeKeepingCentre (130, 124));
    driveArea.removeFromTop (6);
    driveTypeSelector.setBounds (driveArea.removeFromTop (28));
    driveArea.removeFromTop (6);
    drivePosSelector.setBounds (driveArea.removeFromTop (28));

    auto output = content (2);
    row (output.removeFromTop (132), { &inGain, &outGain, &mix, &spread, &stereoOffset }, 132);
    output.removeFromTop (12);
    stereoModeSelector.setBounds (output.removeFromTop (30));

    auto lfo = content (3);
    row (lfo.removeFromTop (94), { &lfoRate, &lfoDepth, &lfoStereo }, 94);
    lfo.removeFromTop (4);
    shapeSelector.setBounds (lfo.removeFromTop (26));
    lfo.removeFromTop (4);
    auto syncRow = lfo.removeFromTop (26);
    syncToggle.setBounds (syncRow.removeFromLeft (90));
    syncRow.removeFromLeft (6);
    divisionBox.setBounds (syncRow);
    lfo.removeFromTop (4);
    lfoPreview.setBounds (lfo);

    auto env = content (4);
    row (env.removeFromTop (94), { &envAmount, &envAttack, &envRelease }, 94);
    env.removeFromTop (6);
    envHistory.setBounds (env);

    auto seq = content (5);
    auto seqTop = seq.removeFromTop (94);
    auto seqControls = seqTop.removeFromLeft (110).reduced (0, 10);
    seqToggle.setBounds (seqControls.removeFromTop (30));
    seqControls.removeFromTop (8);
    seqRateBox.setBounds (seqControls.removeFromTop (30));
    row (seqTop, { &seqSteps, &seqDepth, &seqGlide }, 94);
    seq.removeFromTop (6);
    sequencer.setBounds (seq);

    auto macroArea = content (6);
    macro.setBounds (macroArea.removeFromTop (122).withSizeKeepingCentre (130, 122));
    macroArea.removeFromTop (8);
    row (macroArea, { &macroCutoff, &macroRes, &macroDrive, &macroMix, &macroMorph }, 72);
}

juce::Image LowpassAudioProcessorEditor::renderLattice() const
{
    // A software image at the design size: cheap to draw scaled, nothing for the graphics driver to free later.
    juce::Image image (juce::Image::ARGB, designWidth, designHeight, true, juce::SoftwareImageType());
    juce::Graphics g (image);
    const auto bounds = juce::Rectangle<float> (0.0f, 0.0f, (float) designWidth, (float) designHeight);
    g.setColour (Theme::cyan.withAlpha (0.035f));
    const float hx = 22.0f, hy = 19.0f;
    for (int r = 0; (float) r * hy < bounds.getHeight() + hy; ++r)
        for (int c = 0; (float) c * hx < bounds.getWidth() + hx; ++c)
        {
            const juce::Point<float> centre { (float) c * hx + (r % 2 ? hx * 0.5f : 0.0f), (float) r * hy };
            juce::Path hex;
            for (int k = 0; k < 6; ++k)
            {
                const float a = juce::MathConstants<float>::pi / 3.0f * (float) k + juce::MathConstants<float>::pi / 6.0f;
                const juce::Point<float> v { centre.x + 7.0f * std::cos (a), centre.y + 7.0f * std::sin (a) };
                if (k == 0)
                    hex.startNewSubPath (v);
                else
                    hex.lineTo (v);
            }
            hex.closeSubPath();
            g.strokePath (hex, juce::PathStrokeType (0.6f));
        }
    return image;
}

void LowpassAudioProcessorEditor::renderBackground()
{
    const float scale = juce::Component::getApproximateScaleFactorForComponent (&canvas);
    auto& image = canvas.background;
    image = juce::Image (juce::Image::ARGB, juce::roundToInt ((float) designWidth * scale), juce::roundToInt ((float) designHeight * scale), true);
    juce::Graphics g (image);
    g.addTransform (juce::AffineTransform::scale (scale));

    const auto bounds = juce::Rectangle<float> (0.0f, 0.0f, (float) designWidth, (float) designHeight);
    g.fillAll (Theme::background);

    // Faint hex lattice (thousands of paths, so it's drawn once and then only scaled) and a radial glow behind the analyzer.
    if (! lattice.isValid())
        lattice = renderLattice();
    g.drawImage (lattice, bounds);

    g.setGradientFill (juce::ColourGradient (Theme::cyan.withAlpha (0.07f), 450.0f, 220.0f, Theme::cyan.withAlpha (0.0f), 450.0f, 760.0f, true));
    g.fillRect (bounds);

    for (const auto& panel : panels)
        Theme::drawPanel (g, panel.bounds.toFloat(), panel.index, panel.title, panel.accent);

    g.setFont (Theme::mono (10.0f));
    g.setColour (Theme::dim);
    const auto footer = bounds.withTop (bounds.getBottom() - 16.0f).reduced (18.0f, 0.0f);
    g.drawText ("SKAZI // SKILTER v3.0 // BY ASHER SWISSA", footer, juce::Justification::centredLeft);
    g.drawText ("150 PRESETS  //  MORPH SVF  //  ANALOG FEEDBACK  //  OVS 4x", footer, juce::Justification::centredRight);
}

void LowpassAudioProcessorEditor::resized()
{
    const float scale = (float) getWidth() / (float) designWidth;
    canvas.setTransform (juce::AffineTransform::scale (scale));

    // Re-render the static background when the size changes so it stays sharp.
    if (std::abs (scale - renderedScale) > 0.001f)
    {
        renderedScale = scale;
        renderBackground();
        canvas.repaint();
    }
}
