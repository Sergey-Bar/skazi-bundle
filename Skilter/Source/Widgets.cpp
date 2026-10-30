#include "Widgets.h"

namespace
{
    juce::Point<float> polar (juce::Point<float> centre, float radius, float angle)
    {
        return centre.translated (radius * std::sin (angle), -radius * std::cos (angle));
    }
}

//==============================================================================
NeonKnob::NeonKnob (const juce::String& labelText, juce::Colour accentColour, bool isBipolar)
    : juce::Slider (RotaryHorizontalVerticalDrag, NoTextBox),
      label (labelText.toUpperCase()), accent (accentColour), bipolar (isBipolar)
{
    setRotaryParameters (juce::degreesToRadians (-135.0f), juce::degreesToRadians (135.0f), true);
    setMouseDragSensitivity (240);
    setRepaintsOnMouseActivity (true);
}

void NeonKnob::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    const auto labelArea = bounds.withTop (bounds.getBottom() - 16.0f);
    const auto knobArea = bounds.withTrimmedBottom (18.0f);
    const float size = juce::jmin (knobArea.getWidth(), knobArea.getHeight());
    const auto centre = knobArea.getCentre();
    const float radius = size * 0.5f - 3.0f;

    const bool active = isEnabled();
    const bool hot = isMouseOverOrDragging() && active;
    const float alpha = active ? 1.0f : 0.3f;
    const auto rotary = getRotaryParameters();
    const float a0 = rotary.startAngleRadians, a1 = rotary.endAngleRadians;
    const float norm = (float) valueToProportionOfLength (getValue());
    const float angle = a0 + norm * (a1 - a0);
    const float origin = bipolar ? 0.5f : 0.0f;

    // Segmented outer ring.
    const int ticks = 41;
    for (int i = 0; i < ticks; ++i)
    {
        const float t = (float) i / (float) (ticks - 1);
        const float a = a0 + t * (a1 - a0);
        const bool lit = t >= juce::jmin (origin, norm) - 0.001f && t <= juce::jmax (origin, norm) + 0.001f && ! (bipolar && std::abs (norm - 0.5f) < 0.01f);
        const float inner = radius * 0.84f;
        const float outer = radius * (i % 5 == 0 ? 1.0f : 0.94f);
        g.setColour (lit ? accent.interpolatedWith (Theme::magenta, t * 0.5f).withAlpha (alpha)
                         : Theme::dim.withAlpha (0.28f * alpha));
        g.drawLine ({ polar (centre, inner, a), polar (centre, outer, a) }, lit ? 2.0f : 1.4f);
    }

    // Value arc.
    const float arcRadius = radius * 0.72f;
    juce::Path track;
    track.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f, a0, a1, true);
    g.setColour (Theme::edge.withAlpha (alpha));
    g.strokePath (track, juce::PathStrokeType (3.0f));

    const float arcFrom = a0 + origin * (a1 - a0);
    if (std::abs (angle - arcFrom) > 0.01f)
    {
        juce::Path arc;
        arc.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f, arcFrom, angle, true);
        Theme::glowStroke (g, arc, accent, 2.6f, (hot ? 1.0f : 0.75f) * alpha);
    }

    // Live modulation marker.
    if (modulation && active)
    {
        const float mod = juce::jlimit (0.0f, 1.0f, modulation());
        const float modAngle = a0 + mod * (a1 - a0);
        if (std::abs (modAngle - angle) > 0.02f)
        {
            juce::Path modArc;
            modArc.addCentredArc (centre.x, centre.y, radius * 0.62f, radius * 0.62f, 0.0f, angle, modAngle, true);
            g.setColour (Theme::magenta.withAlpha (0.8f));
            g.strokePath (modArc, juce::PathStrokeType (1.6f));
        }
        Theme::glowDot (g, polar (centre, radius * 0.62f, modAngle), 2.2f, Theme::magenta);
    }

    // Body.
    const float bodyRadius = radius * 0.54f;
    const auto body = juce::Rectangle<float> (bodyRadius * 2.0f, bodyRadius * 2.0f).withCentre (centre);
    g.setGradientFill (juce::ColourGradient (Theme::panelHi.brighter (0.25f), centre.translated (-bodyRadius * 0.5f, -bodyRadius * 0.7f),
                                             Theme::background, centre.translated (0.0f, bodyRadius), true));
    g.fillEllipse (body);
    g.setColour (accent.withAlpha ((hot ? 0.7f : 0.35f) * alpha));
    g.drawEllipse (body, 1.2f);

    // Pointer.
    juce::Path pointer;
    pointer.startNewSubPath (polar (centre, bodyRadius * 0.55f, angle));
    pointer.lineTo (polar (centre, bodyRadius * 0.95f, angle));
    Theme::glowStroke (g, pointer, accent, 2.0f, alpha);

    // Readout. Compact knobs show it above the knob while hovered, since the body is too small.
    if (! compact)
    {
        g.setColour (Theme::text.withAlpha (alpha));
        g.setFont (Theme::mono (juce::jlimit (9.0f, 11.5f, size * 0.1f), true));
        g.drawFittedText (getTextFromValue (getValue()), body.reduced (4.0f).toNearestInt(), juce::Justification::centred, 2, 0.7f);
    }
    else if (hot)
    {
        const auto tag = bounds.withHeight (14.0f);
        g.setColour (Theme::background.withAlpha (0.9f));
        g.fillRect (tag);
        g.setColour (accent);
        g.setFont (Theme::mono (10.0f, true));
        g.drawFittedText (getTextFromValue (getValue()), tag.toNearestInt(), juce::Justification::centred, 1, 0.6f);
    }

    g.setFont (Theme::mono (compact ? 10.0f : 11.0f, true));
    g.setColour ((hot ? accent : Theme::dim.brighter (0.5f)).withAlpha (alpha));
    g.drawText (label, labelArea, juce::Justification::centred);
}

//==============================================================================
SegmentedSelector::SegmentedSelector (juce::RangedAudioParameter& parameter, juce::StringArray labelTexts, juce::Colour accentColour)
    : labels (std::move (labelTexts)), accent (accentColour),
      attachment (parameter, [this] (float v) { value = v; repaint(); })
{
    attachment.sendInitialUpdate();
}

int SegmentedSelector::indexAt (juce::Point<float> p) const
{
    if (labels.isEmpty())
        return -1;
    return juce::jlimit (0, labels.size() - 1, (int) (p.x / ((float) getWidth() / (float) labels.size())));
}

void SegmentedSelector::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat().reduced (0.5f);
    const float cellWidth = bounds.getWidth() / (float) labels.size();

    g.setColour (Theme::background.withAlpha (0.8f));
    g.fillRect (bounds);

    for (int i = 0; i < labels.size(); ++i)
    {
        const auto cell = juce::Rectangle<float> (bounds.getX() + cellWidth * (float) i, bounds.getY(), cellWidth, bounds.getHeight()).reduced (2.0f);
        const float weight = juce::jlimit (0.0f, 1.0f, 1.0f - std::abs (value - (float) i));
        const bool isOn = weight > 0.5f;
        const bool isHover = i == hovered;

        if (weight > 0.02f)
        {
            g.setGradientFill (juce::ColourGradient (accent.withAlpha (0.45f * weight), cell.getTopLeft(), accent.withAlpha (0.12f * weight), cell.getBottomLeft(), false));
            g.fillRect (cell);
            juce::Path outline;
            outline.addRectangle (cell);
            Theme::glowStroke (g, outline, accent, 1.0f, 0.7f * weight);
        }
        if (weight < 0.98f)
        {
            g.setColour ((isHover ? accent.withAlpha (0.12f) : Theme::panelHi).withMultipliedAlpha (1.0f - weight));
            g.fillRect (cell);
            g.setColour (Theme::edge);
            g.drawRect (cell, 1.0f);
        }

        const auto colour = isOn ? Theme::text : (isHover ? accent : Theme::dim.brighter (0.4f));
        if (i < (int) icons.size() && icons[(size_t) i] != nullptr)
            icons[(size_t) i] (g, cell.reduced (cell.getWidth() * 0.2f, cell.getHeight() * 0.28f), colour);
        else
        {
            g.setColour (colour);
            g.setFont (Theme::mono (12.0f, true));
            g.drawText (labels[i], cell, juce::Justification::centred);
        }
    }
}

void SegmentedSelector::mouseDown (const juce::MouseEvent& e)
{
    const int index = indexAt (e.position);
    if (index >= 0 && std::abs (value - (float) index) > 0.001f)
        attachment.setValueAsCompleteGesture ((float) index);
}

void SegmentedSelector::mouseMove (const juce::MouseEvent& e)
{
    const int index = indexAt (e.position);
    if (index != hovered)
    {
        hovered = index;
        repaint();
    }
}

void SegmentedSelector::mouseExit (const juce::MouseEvent&)
{
    hovered = -1;
    repaint();
}

//==============================================================================
NeonToggle::NeonToggle (const juce::String& text, juce::Colour accentColour)
    : juce::ToggleButton (text), accent (accentColour)
{
}

void NeonToggle::paintButton (juce::Graphics& g, bool highlighted, bool)
{
    const auto bounds = getLocalBounds().toFloat().reduced (2.0f);
    const bool on = getToggleState();

    g.setColour (on ? accent.withAlpha (0.3f) : (highlighted ? accent.withAlpha (0.1f) : Theme::panelHi));
    g.fillRect (bounds);

    juce::Path outline;
    outline.addRectangle (bounds);
    if (on)
        Theme::glowStroke (g, outline, accent, 1.0f, 0.8f);
    else
    {
        g.setColour (Theme::edge.brighter (0.2f));
        g.strokePath (outline, juce::PathStrokeType (1.0f));
    }

    const auto led = juce::Rectangle<float> (8.0f, 8.0f).withCentre ({ bounds.getX() + 12.0f, bounds.getCentreY() });
    if (on)
        Theme::glowDot (g, led.getCentre(), 3.0f, accent);
    else
    {
        g.setColour (Theme::dim);
        g.drawEllipse (led, 1.0f);
    }

    g.setColour (on ? Theme::text : Theme::dim.brighter (0.4f));
    g.setFont (Theme::mono (12.0f, true));
    g.drawText (getButtonText(), bounds.withTrimmedLeft (20.0f), juce::Justification::centred);
}

//==============================================================================
NeonButton::NeonButton (const juce::String& text, juce::Colour accentColour, bool drawDice)
    : juce::Button (text), accent (accentColour), dice (drawDice)
{
}

void NeonButton::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    const auto bounds = getLocalBounds().toFloat().reduced (2.0f);
    const float glow = juce::jlimit (0.0f, 1.0f, flash + (down ? 0.6f : highlighted ? 0.25f : 0.0f));

    g.setColour (Theme::panelHi.interpolatedWith (accent, 0.12f + 0.3f * glow));
    g.fillRect (bounds);
    juce::Path outline;
    outline.addRectangle (bounds);
    Theme::glowStroke (g, outline, accent, 1.0f, 0.35f + 0.65f * glow);

    auto content = bounds;
    if (dice)
    {
        // A tilted die with five pips; it spins when the button flashes.
        const auto die = content.removeFromLeft (bounds.getHeight()).reduced (5.0f);
        juce::Graphics::ScopedSaveState save (g);
        g.addTransform (juce::AffineTransform::rotation (0.25f + flash * 1.5f, die.getCentreX(), die.getCentreY()));
        g.setColour (accent.withAlpha (0.25f + 0.5f * glow));
        g.fillRoundedRectangle (die, 3.0f);
        g.setColour (accent);
        g.drawRoundedRectangle (die, 3.0f, 1.4f);
        const float pip = die.getWidth() * 0.16f;
        const float pips[5][2] { { 0.25f, 0.25f }, { 0.75f, 0.25f }, { 0.5f, 0.5f }, { 0.25f, 0.75f }, { 0.75f, 0.75f } };
        for (const auto& pp : pips)
            g.fillEllipse (juce::Rectangle<float> (pip, pip).withCentre ({ die.getX() + die.getWidth() * pp[0], die.getY() + die.getHeight() * pp[1] }));
    }

    g.setColour (glow > 0.3f ? juce::Colours::white : Theme::text);
    g.setFont (Theme::mono (13.0f, true));
    g.drawText (getButtonText(), content, juce::Justification::centred);
}

//==============================================================================
NeonLookAndFeel::NeonLookAndFeel()
{
    setColour (juce::ComboBox::textColourId, Theme::text);
    setColour (juce::ComboBox::arrowColourId, Theme::magenta);
    setColour (juce::PopupMenu::backgroundColourId, Theme::panel);
    setColour (juce::PopupMenu::textColourId, Theme::text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, Theme::magenta.withAlpha (0.35f));
    setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
}

void NeonLookAndFeel::drawComboBox (juce::Graphics& g, int width, int height, bool, int, int, int, int, juce::ComboBox& box)
{
    const auto bounds = juce::Rectangle<float> (0.0f, 0.0f, (float) width, (float) height).reduced (2.0f);
    const auto accent = Theme::magenta;
    const float alpha = box.isEnabled() ? 1.0f : 0.35f;

    g.setColour (Theme::panelHi.withAlpha (alpha));
    g.fillRect (bounds);
    g.setColour ((box.isMouseOver() ? accent : Theme::edge.brighter (0.2f)).withAlpha (alpha));
    g.drawRect (bounds, 1.0f);

    juce::Path arrow;
    const float cx = bounds.getRight() - 14.0f, cy = bounds.getCentreY();
    arrow.addTriangle (cx - 5.0f, cy - 3.0f, cx + 5.0f, cy - 3.0f, cx, cy + 4.0f);
    g.setColour (accent.withAlpha (alpha));
    g.fillPath (arrow);
}

juce::Font NeonLookAndFeel::getComboBoxFont (juce::ComboBox&)
{
    return Theme::mono (13.0f, true);
}

juce::Font NeonLookAndFeel::getPopupMenuFont()
{
    return Theme::mono (14.0f, true);
}

void NeonLookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int width, int height)
{
    g.fillAll (Theme::panel);
    g.setColour (Theme::magenta.withAlpha (0.6f));
    g.drawRect (0, 0, width, height, 1);
}
