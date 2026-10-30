#include "Widgets.h"

namespace
{
    juce::Point<float> polar (juce::Point<float> centre, float radius, float angle)
    {
        return centre.translated (radius * std::sin (angle), -radius * std::cos (angle));
    }
}

//==============================================================================
Knob::Knob (const juce::String& labelText, bool isBipolar)
    : juce::Slider (RotaryHorizontalVerticalDrag, NoTextBox),
      label (labelText.toUpperCase()), bipolar (isBipolar)
{
    setRotaryParameters (juce::degreesToRadians (-135.0f), juce::degreesToRadians (135.0f), true);
    setMouseDragSensitivity (260);
    setVelocityBasedMode (false);
    setRepaintsOnMouseActivity (true);
}

void Knob::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    const auto labelArea = bounds.withHeight (16.0f);
    const auto valueArea = bounds.withTop (bounds.getBottom() - 15.0f);
    const auto knobArea = bounds.withTrimmedTop (18.0f).withTrimmedBottom (17.0f);
    const float size = juce::jmin (knobArea.getWidth() - 16.0f, knobArea.getHeight());
    const auto centre = knobArea.getCentre();
    const float radius = size * 0.5f - 1.0f;

    const bool active = isEnabled();
    const bool hot = isMouseOverOrDragging() && active;
    const auto rotary = getRotaryParameters();
    const float a0 = rotary.startAngleRadians, a1 = rotary.endAngleRadians;
    const float norm = (float) valueToProportionOfLength (getValue());
    const float angle = a0 + norm * (a1 - a0);
    const float origin = bipolar ? 0.5f : 0.0f;

    if (! active)
        g.beginTransparencyLayer (0.38f);

    // Printed scale.
    const int ticks = 11;
    for (int i = 0; i < ticks; ++i)
    {
        const float t = (float) i / (float) (ticks - 1);
        const float a = a0 + t * (a1 - a0);
        const bool major = i == 0 || i == ticks - 1 || (bipolar && i == ticks / 2);
        g.setColour ((major ? Theme::legend : Theme::legendDim).withAlpha (major ? 0.9f : 0.6f));
        g.drawLine ({ polar (centre, radius * (major ? 0.86f : 0.9f), a), polar (centre, radius, a) }, major ? 1.4f : 1.0f);
    }
    if (scaleMin.isNotEmpty() && size > 60.0f)
    {
        g.setFont (Theme::face (9.0f, false));
        g.setColour (Theme::legendDim);
        const auto pMin = polar (centre, radius * 0.95f, a0), pMax = polar (centre, radius * 0.95f, a1);
        g.drawText (scaleMin, juce::Rectangle<float> (36.0f, 10.0f).withCentre (pMin.translated (-8.0f, 9.0f)), juce::Justification::centred);
        g.drawText (scaleMax, juce::Rectangle<float> (36.0f, 10.0f).withCentre (pMax.translated (8.0f, 9.0f)), juce::Justification::centred);
    }

    // Position arc in the groove just inside the scale.
    const float arcRadius = radius * 0.8f;
    juce::Path track;
    track.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f, a0, a1, true);
    g.setColour (Theme::groove);
    g.strokePath (track, juce::PathStrokeType (3.0f));
    const float arcFrom = a0 + origin * (a1 - a0);
    if (std::abs (angle - arcFrom) > 0.01f)
    {
        juce::Path arc;
        arc.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f, juce::jmin (arcFrom, angle), juce::jmax (arcFrom, angle), true);
        g.setColour (Theme::amber.withAlpha (hot ? 1.0f : 0.8f));
        g.strokePath (arc, juce::PathStrokeType (2.0f));
    }

    // Knob: soft cast shadow, knurled black skirt that turns with the value, anodised cap with a machined aluminium insert.
    const float skirt = radius * 0.7f;
    const auto skirtBox = juce::Rectangle<float> (skirt * 2.0f, skirt * 2.0f).withCentre (centre);
    {
        // Soft cast shadow as a radial gradient: looks like a blur, costs a single fill.
        const auto shadowCentre = centre.translated (skirt * 0.1f, skirt * 0.24f);
        juce::ColourGradient shadow (juce::Colours::black.withAlpha (0.75f), shadowCentre, juce::Colours::transparentBlack,
                                     shadowCentre.translated (skirt * 1.4f, 0.0f), true);
        shadow.addColour (0.62, juce::Colours::black.withAlpha (0.55f));
        g.setGradientFill (shadow);
        g.fillEllipse (juce::Rectangle<float> (skirt * 2.8f, skirt * 2.8f).withCentre (shadowCentre));
    }

    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2e3034), skirtBox.getX(), skirtBox.getY(), juce::Colour (0xff0a0b0c), skirtBox.getX(), skirtBox.getBottom(), false));
    g.fillEllipse (skirtBox);
    const int knurls = juce::jlimit (30, 72, (int) (skirt * 1.6f));
    for (int i = 0; i < knurls; ++i)
    {
        const float a = angle + juce::MathConstants<float>::twoPi * (float) i / (float) knurls;
        const float light = juce::jmax (0.0f, std::cos (a + 0.7f));   // ridges facing the top-left light catch it
        g.setColour (juce::Colours::white.withAlpha (0.04f + 0.22f * light * light));
        g.drawLine ({ polar (centre, skirt * 0.86f, a), polar (centre, skirt * 0.99f, a) }, 1.0f);
        g.setColour (juce::Colours::black.withAlpha (0.6f));
        g.drawLine ({ polar (centre, skirt * 0.86f, a + 0.04f), polar (centre, skirt * 0.99f, a + 0.04f) }, 0.8f);
    }
    // Rim light on the skirt's upper edge.
    {
        juce::Path rim;
        rim.addCentredArc (centre.x, centre.y, skirt - 0.6f, skirt - 0.6f, 0.0f, -2.0f, 0.9f, true);
        g.setColour (juce::Colours::white.withAlpha (0.22f));
        g.strokePath (rim, juce::PathStrokeType (1.0f));
    }

    const float cap = skirt * 0.8f;
    const auto capBox = juce::Rectangle<float> (cap * 2.0f, cap * 2.0f).withCentre (centre);
    g.setColour (juce::Colours::black.withAlpha (0.7f));
    g.fillEllipse (capBox.expanded (1.0f).translated (0.0f, 1.0f));
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff34373b), capBox.getX(), capBox.getY(), juce::Colour (0xff0d0e10), capBox.getX(), capBox.getBottom(), false));
    g.fillEllipse (capBox);
    {
        juce::Path capRim;
        capRim.addCentredArc (centre.x, centre.y, cap - 0.6f, cap - 0.6f, 0.0f, -1.9f, 0.7f, true);
        g.setColour (juce::Colours::white.withAlpha (0.3f));
        g.strokePath (capRim, juce::PathStrokeType (1.0f));
    }

    const float insert = cap * 0.74f;
    Theme::machinedDisc (g, centre, insert, juce::Colour (0xff3e4146), hot ? juce::Colour (0xffe6e9ee) : juce::Colour (0xffc9ccd1));
    g.setColour (juce::Colours::black.withAlpha (0.65f));
    g.drawEllipse (juce::Rectangle<float> (insert * 2.0f, insert * 2.0f).withCentre (centre), 1.0f);

    // Pointer: an engraved line across the insert and a white index on the rim and skirt.
    const float lineWidth = juce::jmax (1.8f, skirt * 0.07f);
    g.setColour (juce::Colour (0xff15171a));
    g.drawLine ({ polar (centre, insert * 0.3f, angle), polar (centre, insert * 0.92f, angle) }, lineWidth);
    g.setColour (Theme::legend.brighter (hot ? 0.3f : 0.1f));
    g.drawLine ({ polar (centre, insert * 1.08f, angle), polar (centre, skirt * 0.97f, angle) }, lineWidth);

    Theme::legendText (g, label, labelArea, juce::Justification::centred, hot ? Theme::legend.brighter (0.3f) : Theme::legend, 0.12f, 11.0f);
    g.setFont (Theme::face (11.0f, false));
    g.setColour (hot ? Theme::amber : Theme::legendDim);
    g.drawFittedText (getTextFromValue (getValue()), valueArea.toNearestInt(), juce::Justification::centred, 1, 0.7f);

    if (! active)
        g.endTransparencyLayer();
}

//==============================================================================
LedSelector::LedSelector (juce::RangedAudioParameter& parameter, juce::StringArray labelTexts, bool isVertical)
    : labels (std::move (labelTexts)), vertical (isVertical),
      attachment (parameter, [this] (float v) { value = v; repaint(); })
{
    attachment.sendInitialUpdate();
}

int LedSelector::indexAt (juce::Point<float> p) const
{
    if (labels.isEmpty())
        return -1;
    const float along = vertical ? p.y / (float) getHeight() : p.x / (float) getWidth();
    return juce::jlimit (0, labels.size() - 1, (int) (along * (float) labels.size()));
}

void LedSelector::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    const float cellWidth = bounds.getWidth() / (float) labels.size();
    const int selected = juce::roundToInt (value);

    if (vertical)
    {
        const float cellHeight = bounds.getHeight() / (float) labels.size();
        for (int i = 0; i < labels.size(); ++i)
        {
            const auto cell = juce::Rectangle<float> (bounds.getX(), bounds.getY() + cellHeight * (float) i, bounds.getWidth(), cellHeight).reduced (0.0f, 4.0f);
            const bool isOn = i == selected;
            Theme::led (g, { cell.getX() + 6.0f, cell.getCentreY() }, 3.3f, Theme::amber, isOn ? 1.0f : 0.0f);
            const auto capArea = cell.withTrimmedLeft (17.0f).reduced (2.0f, 0.0f);
            Theme::buttonCap (g, capArea, isOn || i == pressed, i == hovered);
            Theme::legendText (g, labels[i], capArea.translated (0.0f, (isOn || i == pressed) ? 1.0f : 0.0f), juce::Justification::centred,
                               isOn ? Theme::legend.brighter (0.25f) : Theme::legend.withAlpha (0.8f), 0.1f, 11.0f);
        }
        return;
    }

    for (int i = 0; i < labels.size(); ++i)
    {
        const auto cell = juce::Rectangle<float> (bounds.getX() + cellWidth * (float) i, bounds.getY(), cellWidth, bounds.getHeight()).reduced (5.0f, 0.0f);
        const bool isOn = i == selected;
        Theme::led (g, { cell.getCentreX(), cell.getY() + 6.0f }, 3.6f, Theme::amber, isOn ? 1.0f : 0.0f);

        const auto capArea = cell.withTrimmedTop (17.0f).withTrimmedBottom (2.0f);
        Theme::buttonCap (g, capArea, isOn || i == pressed, i == hovered);
        Theme::legendText (g, labels[i], capArea.translated (0.0f, (isOn || i == pressed) ? 1.0f : 0.0f), juce::Justification::centred,
                           isOn ? Theme::legend.brighter (0.25f) : Theme::legend.withAlpha (0.8f), 0.1f, 11.5f);
    }
}

void LedSelector::mouseDown (const juce::MouseEvent& e)
{
    pressed = indexAt (e.position);
    if (pressed >= 0 && std::abs (value - (float) pressed) > 0.001f)
        attachment.setValueAsCompleteGesture ((float) pressed);
    repaint();
}

void LedSelector::mouseUp (const juce::MouseEvent&)
{
    pressed = -1;
    repaint();
}

void LedSelector::mouseMove (const juce::MouseEvent& e)
{
    const int index = indexAt (e.position);
    if (index != hovered)
    {
        hovered = index;
        repaint();
    }
}

void LedSelector::mouseExit (const juce::MouseEvent&)
{
    hovered = -1;
    repaint();
}

//==============================================================================
LedToggle::LedToggle (const juce::String& text, juce::Colour colour)
    : juce::ToggleButton (text), ledColour (colour)
{
}

void LedToggle::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    auto cap = getLocalBounds().toFloat().reduced (2.0f, 3.0f);
    const bool on = getToggleState();
    Theme::buttonCap (g, cap, on || down, highlighted);
    if (on || down)
        cap = cap.translated (0.0f, 1.0f);

    Theme::led (g, { cap.getX() + 13.0f, cap.getCentreY() }, 3.4f, ledColour, on ? 1.0f : 0.0f);
    Theme::legendText (g, getButtonText(), cap.withTrimmedLeft (20.0f), juce::Justification::centred,
                       on ? Theme::legend.brighter (0.25f) : Theme::legend.withAlpha (0.8f), 0.12f, 11.5f);
}

//==============================================================================
PanelButton::PanelButton (const juce::String& text) : juce::Button (text)
{
}

void PanelButton::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    auto cap = getLocalBounds().toFloat().reduced (2.0f, 3.0f);
    const bool sunk = down || flash > 0.6f;
    Theme::buttonCap (g, cap, sunk, highlighted);
    if (sunk)
        cap = cap.translated (0.0f, 1.0f);
    Theme::legendText (g, getButtonText(), cap, juce::Justification::centred,
                       Theme::legend.interpolatedWith (Theme::amber, juce::jlimit (0.0f, 1.0f, flash)), 0.12f, 11.5f);
}

//==============================================================================
PanelLookAndFeel::PanelLookAndFeel()
{
    setColour (juce::ComboBox::textColourId, Theme::amber);
    setColour (juce::ComboBox::arrowColourId, Theme::amber);
    setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff1c1d20));
    setColour (juce::PopupMenu::textColourId, Theme::legend);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, Theme::amber.withAlpha (0.22f));
    setColour (juce::PopupMenu::highlightedTextColourId, Theme::amber);
    setColour (juce::PopupMenu::headerTextColourId, Theme::legendDim);
}

void PanelLookAndFeel::drawComboBox (juce::Graphics& g, int width, int height, bool, int, int, int, int, juce::ComboBox& box)
{
    const auto bounds = juce::Rectangle<float> (0.0f, 0.0f, (float) width, (float) height).reduced (3.0f);
    Theme::recess (g, bounds, 2.0f);

    juce::Path arrow;
    const float cx = bounds.getRight() - 13.0f, cy = bounds.getCentreY();
    arrow.addTriangle (cx - 4.0f, cy - 2.0f, cx + 4.0f, cy - 2.0f, cx, cy + 3.0f);
    g.setColour ((box.isMouseOver (true) ? Theme::amber : Theme::amberDim));
    g.fillPath (arrow);
    Theme::glassSheen (g, bounds);
}

void PanelLookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (8, 3, box.getWidth() - 34, box.getHeight() - 6);
    label.setFont (getComboBoxFont (box));
}

juce::Font PanelLookAndFeel::getComboBoxFont (juce::ComboBox&)
{
    return Theme::face (14.0f).withExtraKerningFactor (0.06f);
}

juce::Font PanelLookAndFeel::getPopupMenuFont()
{
    return Theme::face (14.0f, false);
}

void PanelLookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int width, int height)
{
    g.fillAll (juce::Colour (0xff1c1d20));
    g.setColour (Theme::bevel);
    g.drawRect (0, 0, width, height, 1);
}
