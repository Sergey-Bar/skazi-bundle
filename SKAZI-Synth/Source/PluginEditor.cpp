#include "PluginEditor.h"
#include "Presets.h"
#include <array>
#include <initializer_list>
#if SKAZI_HAS_LOGO
 #include "BinaryData.h"
#endif

namespace
{
    // The SKAZI logo on a black badge; false when the build has no logo image
    bool drawBrandLogo (juce::Graphics& g, juce::Rectangle<float> r)
    {
#if SKAZI_HAS_LOGO
        // Not a static: an image still alive when Windows unloads the plugin DLL frees its Direct2D
        // resources under the loader lock, and the graphics driver deadlocks the DAW on quit.
        // ImageCache hands back the same decoded image every paint.
        const auto logo = juce::ImageCache::getFromMemory (BinaryData::skazi_logo_png, BinaryData::skazi_logo_pngSize);
        g.setColour (juce::Colours::black);
        g.fillRoundedRectangle (r, 6.f);
        g.drawImage (logo, r.reduced (4.f, 3.f), juce::RectanglePlacement::centred);
        return true;
#else
        juce::ignoreUnused (g, r);
        return false;
#endif
    }

    const char* const noteNames[13] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B", "C" };
    bool isBlackKey (int n) { n %= 12; return n == 1 || n == 3 || n == 6 || n == 8 || n == 10; }

    float readParam (AcidanceProcessor& p, const juce::String& id)
    {
        auto* v = p.apvts.getRawParameterValue (id);
        return v != nullptr ? v->load() : 0.f;
    }
}

//==============================================================================
Knob::Knob (APVTS& state, const juce::String& paramId, const juce::String& caption, juce::Colour accent, bool bipolar)
    : text (caption)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    slider.setColour (juce::Slider::rotarySliderFillColourId, accent);
    slider.setPopupDisplayEnabled (true, true, nullptr, 900);
    slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
    slider.getProperties().set ("bipolar", bipolar);
    addAndMakeVisible (slider);
    attachment = std::make_unique<APVTS::SliderAttachment> (state, paramId, slider);
    if (auto* p = state.getParameter (paramId))
        slider.setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue()));
}

void Knob::resized()
{
    auto r = getLocalBounds();
    if (text.isNotEmpty()) r.removeFromBottom (15);
    slider.setBounds (r);
}

void Knob::paint (juce::Graphics& g)
{
    if (text.isEmpty()) return;
    g.setColour (Acid::dim);
    g.setFont (Acid::font (10.5f));
    g.drawText (text, getLocalBounds().removeFromBottom (15), juce::Justification::centred);
}

//==============================================================================
ChoiceButtons::ChoiceButtons (APVTS& state, const juce::String& paramId, juce::StringArray l, juce::Colour accent, int r)
    : param (state.getParameter (paramId)), labels (std::move (l)), colour (Acid::accentFor (accent)), rows (std::max (1, r))
{
    jassert (param != nullptr);
}

int ChoiceButtons::getIndex() const
{
    const float start = param->getNormalisableRange().start;
    return juce::jlimit (0, labels.size() - 1, (int) std::round (param->convertFrom0to1 (param->getValue()) - start));
}

void ChoiceButtons::paint (juce::Graphics& g)
{
    const int sel = getIndex();
    const int perRow = (labels.size() + rows - 1) / rows;
    const float w = getWidth() / (float) perRow, h = getHeight() / (float) rows;
    for (int i = 0; i < labels.size(); ++i)
    {
        auto r = juce::Rectangle<float> ((i % perRow) * w, (i / perRow) * h, w, h).reduced (2.f);
        if (Acid::skin == Acid::Hyper)
        {
            Acid::rubberKey (g, r, colour, i == sel, false);
            g.setColour (i == sel ? Acid::text : Acid::dim);
            g.setFont (Acid::font (std::min (12.f, h * 0.45f)));
            g.drawFittedText (labels[i], r.withTrimmedLeft (r.getWidth() > 34.f ? 10.f : 0.f).toNearestInt(), juce::Justification::centred, 1, 0.7f);
            continue;
        }
        if (i == sel) Acid::glowRect (g, r, colour, 4.f);
        else
        {
            if (Acid::skin == Acid::Robot)
            {
                auto p = Acid::chamfer (r, std::min (6.f, r.getHeight() * 0.3f));
                g.setColour (Acid::panel2);
                g.fillPath (p);
                g.setColour (Acid::line);
                g.strokePath (p, juce::PathStrokeType (1.f));
            }
            else
            {
            g.setColour (Acid::panel2);
            g.fillRoundedRectangle (r, 4.f);
            g.setColour (Acid::line);
            g.drawRoundedRectangle (r, 4.f, 1.f);
            }
        }
        g.setColour (i == sel ? juce::Colour (0xff101014) : Acid::text);
        g.setFont (Acid::font (std::min (12.f, h * 0.45f)));
        g.drawFittedText (labels[i], r.toNearestInt(), juce::Justification::centred, 1, 0.7f);
    }
}

void ChoiceButtons::mouseDown (const juce::MouseEvent& e)
{
    const int perRow = (labels.size() + rows - 1) / rows;
    const int col = juce::jlimit (0, perRow - 1, (int) (e.x / (getWidth() / (float) perRow)));
    const int row = juce::jlimit (0, rows - 1, (int) (e.y / (getHeight() / (float) rows)));
    const int i = juce::jlimit (0, labels.size() - 1, row * perRow + col);
    param->beginChangeGesture();
    param->setValueNotifyingHost (param->convertTo0to1 (param->getNormalisableRange().start + (float) i));
    param->endChangeGesture();
    repaint();
}

//==============================================================================
// Framed section with a title; owns its controls and their attachments.
class Panel : public juce::Component
{
public:
    Panel (const juce::String& t, juce::Colour c, APVTS& s) : accent (Acid::accentFor (c)), title (t), state (s) {}

    Knob* knob (const juce::String& id, const juce::String& cap, bool bipolar = false)
    {
        return add (new Knob (state, id, cap, accent, bipolar));
    }
    ChoiceButtons* choice (const juce::String& id, const juce::StringArray& labels, int rows = 1)
    {
        return add (new ChoiceButtons (state, id, labels, accent, rows));
    }
    juce::TextButton* toggle (const juce::String& id, const juce::String& text)
    {
        auto* b = add (new juce::TextButton (text));
        b->setClickingTogglesState (true);
        b->setColour (juce::TextButton::buttonOnColourId, accent);
        buttonAtts.add (new APVTS::ButtonAttachment (state, id, *b));
        return b;
    }
    juce::ComboBox* combo (const juce::String& id, const juce::StringArray& items)
    {
        auto* cb = add (new juce::ComboBox());
        cb->addItemList (items, 1);
        comboAtts.add (new APVTS::ComboBoxAttachment (state, id, *cb));
        return cb;
    }
    template <class T> T* add (T* c)
    {
        owned.add (c);
        addAndMakeVisible (c);
        return c;
    }

    // Lays out components left to right
    static void row (std::initializer_list<juce::Component*> items, int x, int y, int w = 64, int h = 80, int gap = 4)
    {
        for (auto* c : items) { c->setBounds (x, y, w, h); x += w + gap; }
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        if (Acid::skin == Acid::Bully)
        {
            // BULLY glass panel: dark violet glass, rainbow frame, neon dot + title
            auto body = r.reduced (1.f);
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xe0140c2c), 0.f, body.getY(), juce::Colour (0xd40a0618), 0.f, body.getBottom(), false));
            g.fillRoundedRectangle (body, 11.f);
            g.setGradientFill (Acid::rainbow (body.getX(), body.getY(), body.getRight(), body.getBottom()));
            g.drawRoundedRectangle (body.reduced (0.5f), 11.f, 1.4f);
            g.setColour (juce::Colours::white.withAlpha (0.05f));
            g.drawHorizontalLine ((int) body.getY() + 2, body.getX() + 10.f, body.getRight() - 10.f);
            g.setColour (accent.withAlpha (0.4f));
            g.fillEllipse (body.getX() + 9.f, 12.f, 13.f, 13.f);
            g.setColour (accent);
            g.fillEllipse (body.getX() + 12.f, 15.f, 7.f, 7.f);
            g.setColour (Acid::text);
            g.setFont (Acid::font (13.5f));
            g.drawText (title.toUpperCase(), 30, 8, 420, 22, juce::Justification::centredLeft);
            return;
        }
        if (Acid::skin == Acid::Vintage)
        {
            // Black anodised front panel, engraved cream title, corner screws
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xff1f1e1c), 0.f, 0.f, juce::Colour (0xff141312), 0.f, r.getHeight(), false));
            g.fillRect (r);
            g.setColour (juce::Colours::black);
            g.drawRect (r, 1.f);
            g.setColour (juce::Colours::white.withAlpha (0.05f));
            g.drawHorizontalLine (1, 1.f, r.getRight() - 1.f);
            g.setColour (Acid::text);
            g.setFont (Acid::font (13.f).withExtraKerningFactor (0.18f));
            g.drawText (title.toUpperCase(), 24, 9, 400, 22, juce::Justification::centredLeft);
            g.setColour (Acid::text.withAlpha (0.25f));
            g.drawHorizontalLine (34, 18.f, r.getRight() - 18.f);
            for (auto p : { juce::Point<float> (8.f, 8.f), juce::Point<float> (r.getRight() - 8.f, 8.f),
                            juce::Point<float> (8.f, r.getBottom() - 8.f), juce::Point<float> (r.getRight() - 8.f, r.getBottom() - 8.f) })
                Acid::screw (g, p.x, p.y, 3.2f);
            return;
        }
        if (Acid::skin == Acid::Hyper)
        {
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2e2e33), 0.f, 0.f, juce::Colour (0xff1e1e22), 0.f, r.getHeight(), false));
            g.fillRoundedRectangle (r, 4.f);
            g.setColour (juce::Colours::white.withAlpha (0.08f));
            g.drawHorizontalLine (1, 4.f, r.getRight() - 4.f);
            g.setColour (juce::Colours::black);
            g.drawRoundedRectangle (r.reduced (0.5f), 4.f, 1.2f);
            const auto t = title.toUpperCase();
            const auto f = Acid::font (13.f).italicised().withExtraKerningFactor (0.06f);
            const float tw = juce::GlyphArrangement::getStringWidth (f, t) + 22.f;
            juce::Rectangle<float> tab (10.f, 9.f, tw, 22.f);
            g.setColour (Acid::green);
            g.fillRoundedRectangle (tab, 2.f);
            g.setColour (juce::Colours::black);
            g.setFont (f);
            g.drawText (t, tab, juce::Justification::centred);
            g.setColour (Acid::green.withAlpha (0.6f));
            g.drawHorizontalLine (20, tab.getRight() + 6.f, r.getRight() - 12.f);
            return;
        }
        if (Acid::skin == Acid::Robot)
        {
            auto p = Acid::chamfer (r.reduced (0.5f), 14.f);
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xff18202a), 0.f, 0.f, juce::Colour (0xff10151b), 0.f, r.getHeight(), false));
            g.fillPath (p);
            g.saveState();
            g.reduceClipRegion (p);
            g.setColour (juce::Colours::white.withAlpha (0.022f));
            for (float x = -r.getHeight(); x < r.getWidth(); x += 9.f)
                g.drawLine (x, r.getHeight(), x + r.getHeight(), 0.f, 1.f);
            g.restoreState();
            g.setColour (Acid::line);
            g.strokePath (p, juce::PathStrokeType (1.2f));
            g.setColour (accent);
            g.drawLine (0.5f, 14.5f, 14.5f, 0.5f, 2.f);
            g.drawLine (r.getRight() - 0.5f, r.getBottom() - 14.5f, r.getRight() - 14.5f, r.getBottom() - 0.5f, 2.f);
            g.fillRect (18.f, 14.f, 3.f, 14.f);
            g.setColour (Acid::text);
            g.setFont (Acid::font (14.f).withExtraKerningFactor (0.16f));
            g.drawText (title.toUpperCase(), 28, 8, 420, 26, juce::Justification::centredLeft);
            g.setColour (Acid::line);
            g.drawHorizontalLine (36, 18.f, r.getRight() - 18.f);
            return;
        }
        if (Acid::skin == Acid::Chrome)
        {
            if (metal.getWidth() != getWidth() || metal.getHeight() != getHeight())
                metal = Acid::brushedMetal (getWidth(), getHeight());
            juce::Path plate;
            plate.addRoundedRectangle (r.reduced (0.5f), 6.f);
            g.saveState();
            g.reduceClipRegion (plate);
            g.drawImageAt (metal, 0, 0);
            g.restoreState();
            g.setColour (juce::Colours::white.withAlpha (0.8f));
            g.drawHorizontalLine (1, 6.f, r.getRight() - 6.f);
            g.setColour (juce::Colour (0xff6f7279));
            g.drawRoundedRectangle (r.reduced (0.5f), 6.f, 1.2f);
            Acid::glowRect (g, { 12.f, 13.f, 4.f, 16.f }, accent, 2.f);
            g.setColour (Acid::text);
            g.setFont (Acid::font (14.f).withExtraKerningFactor (0.12f));
            g.drawText (title.toUpperCase(), 24, 8, 400, 26, juce::Justification::centredLeft);
            g.setColour (juce::Colours::black.withAlpha (0.18f));
            g.drawHorizontalLine (35, 12.f, r.getRight() - 12.f);
            g.setColour (juce::Colours::white.withAlpha (0.6f));
            g.drawHorizontalLine (36, 12.f, r.getRight() - 12.f);
            return;
        }
        g.setColour (Acid::panel);
        g.fillRoundedRectangle (r, 8.f);
        g.setColour (Acid::line);
        g.drawRoundedRectangle (r.reduced (0.5f), 8.f, 1.f);
        Acid::glowRect (g, { 14.f, 13.f, 4.f, 16.f }, accent, 2.f);
        g.setColour (Acid::text);
        g.setFont (Acid::font (15.f));
        g.drawText (title, 26, 8, 300, 26, juce::Justification::centredLeft);
    }

    juce::Colour accent;

private:
    juce::String title;
    APVTS& state;
    juce::Image metal;
    juce::OwnedArray<juce::Component> owned;
    juce::OwnedArray<APVTS::ButtonAttachment> buttonAtts;
    juce::OwnedArray<APVTS::ComboBoxAttachment> comboAtts;
};

// A page repaints only its live displays on a timer
class Page : public juce::Component, public juce::Timer
{
public:
    void timerCallback() override { for (auto* c : live) c->repaint(); }
    juce::Array<juce::Component*> live;
};

//==============================================================================
class WaveDisplay : public juce::Component, public juce::FileDragAndDropTarget, public juce::SettableTooltipClient
{
public:
    WaveDisplay (AcidanceProcessor& p, int index, juce::Colour c) : proc (p), o (index), colour (Acid::accentFor (c))
    {
        setTooltip ("Drop a WAV file here (or right-click) to load your own wavetable: it plays as USER");
    }

    // your own wavetables: drag a WAV / AIFF onto the display, or right-click to pick one
    bool isInterestedInFileDrag (const juce::StringArray& files) override
    {
        return files.size() == 1 && juce::File (files[0]).hasFileExtension ("wav;aif;aiff;flac");
    }
    void fileDragEnter (const juce::StringArray&, int, int) override { dragOver = true; repaint(); }
    void fileDragExit (const juce::StringArray&) override { dragOver = false; repaint(); }
    void filesDropped (const juce::StringArray& files, int, int) override
    {
        dragOver = false;
        load (juce::File (files[0]));
    }
    void mouseDown (const juce::MouseEvent& e) override
    {
        if (! e.mods.isPopupMenu()) return;
        juce::PopupMenu m;
        m.addItem (1, "LOAD WAVETABLE (WAV)...");
        juce::Component::SafePointer<WaveDisplay> safe (this);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this), [safe] (int r)
        {
            if (safe == nullptr || r != 1) return;
            safe->chooser = std::make_unique<juce::FileChooser> ("Load a wavetable", juce::File(), "*.wav;*.aif;*.aiff;*.flac");
            safe->chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                                        [safe] (const juce::FileChooser& fc)
                                        {
                                            if (safe != nullptr && fc.getResult().existsAsFile()) safe->load (fc.getResult());
                                        });
        });
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (Acid::screen);
        g.fillRoundedRectangle (r, 6.f);
        g.setColour (Acid::line);
        g.drawRoundedRectangle (r.reduced (0.5f), 6.f, 1.f);
        g.drawHorizontalLine ((int) r.getCentreY(), r.getX() + 4.f, r.getRight() - 4.f);

        const juce::String pre = "osc" + juce::String (o + 1) + "_";
        if (readParam (proc, pre + "on") < 0.5f)
        {
            g.setColour (Acid::screenDim);
            g.setFont (Acid::font (14.f));
            g.drawText ("OFF", r, juce::Justification::centred);
            return;
        }
        const int wave = (int) readParam (proc, pre + "wave");
        const float pos = readParam (proc, pre + "wtpos");
        const auto* user = wave >= 8 ? proc.userTable (o) : nullptr;
        const float* table = wave >= 8 ? (user != nullptr ? user->frame (pos) : Wavetables::get().frame (0, pos))
                           : wave >= 4 ? Wavetables::get().frame (wave - 4, pos) : nullptr;

        juce::Path path;
        const int n = (int) r.getWidth() - 8;
        for (int i = 0; i <= n; ++i)
        {
            const float ph = (float) i / (float) n;
            float v;
            switch (wave)
            {
                case 0: v = std::sin (6.2831853f * ph); break;
                case 1: v = 2.f * ph - 1.f; break;
                case 2: v = ph < 0.5f ? 1.f : -1.f; break;
                case 3: v = 1.f - 4.f * std::abs (ph - 0.5f); break;
                default: v = table[(int) (ph * (Wavetables::N - 1))]; break;
            }
            const float x = r.getX() + 4.f + (float) i;
            const float y = r.getCentreY() - v * (r.getHeight() * 0.38f);
            if (i == 0) path.startNewSubPath (x, y); else path.lineTo (x, y);
        }
        Acid::glowStroke (g, path, colour, 1.8f);
        g.setColour (Acid::screenDim);
        g.setFont (Acid::font (10.f));
        const auto label = wave >= 8 ? (user != nullptr ? "USER: " + proc.userWavetableName[o].toUpperCase() : juce::String ("USER: DROP A WAV HERE"))
                                     : Lists::oscWaves[wave].toUpperCase();
        g.drawText (label, r.reduced (6.f, 4.f), juce::Justification::topLeft);
        if (dragOver)
        {
            g.setColour (colour.withAlpha (0.25f));
            g.fillRoundedRectangle (r, 6.f);
            g.setColour (colour);
            g.drawRoundedRectangle (r.reduced (1.f), 6.f, 2.f);
            g.setFont (Acid::font (13.f));
            g.drawText ("DROP TO LOAD WAVETABLE", r, juce::Justification::centred);
        }
        if (message.isNotEmpty())
        {
            g.setColour (Acid::panicRed);
            g.setFont (Acid::font (11.f));
            g.drawText (message, r.reduced (6.f, 4.f), juce::Justification::bottomLeft);
        }
    }

private:
    void load (const juce::File& f)
    {
        juce::String err;
        message = proc.loadUserWavetable (o, f, &err) ? juce::String() : err;
        repaint();
    }

    AcidanceProcessor& proc;
    int o;
    juce::Colour colour;
    bool dragOver = false;
    juce::String message;
    std::unique_ptr<juce::FileChooser> chooser;
};

//==============================================================================
class EnvDisplay : public juce::Component
{
public:
    EnvDisplay (AcidanceProcessor& p, int index, juce::Colour c) : proc (p), e (index), colour (Acid::accentFor (c)) {}

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (Acid::screen);
        g.fillRoundedRectangle (r, 6.f);
        const juce::String pre = "env" + juce::String (e + 1) + "_";
        const float a = readParam (proc, pre + "a"), d = readParam (proc, pre + "d");
        const float s = readParam (proc, pre + "s"), rel = readParam (proc, pre + "r");
        auto seg = [] (float t) { return std::log1p (t * 8.f) + 0.05f; };
        const float wa = seg (a), wd = seg (d), wr = seg (rel), ws = 0.6f;
        const float total = wa + wd + ws + wr;
        auto inner = r.reduced (10.f, 12.f);
        const float sx = inner.getWidth() / total;
        const float x0 = inner.getX(), yb = inner.getBottom(), yt = inner.getY();
        const float ys = yb - s * inner.getHeight();

        juce::Path p;
        p.startNewSubPath (x0, yb);
        p.lineTo (x0 + wa * sx, yt);
        p.quadraticTo (x0 + (wa + wd * 0.25f) * sx, ys, x0 + (wa + wd) * sx, ys);
        p.lineTo (x0 + (wa + wd + ws) * sx, ys);
        p.quadraticTo (x0 + (wa + wd + ws + wr * 0.25f) * sx, yb, x0 + total * sx, yb);

        juce::Path fill (p);
        fill.lineTo (x0, yb);
        fill.closeSubPath();
        g.setColour (colour.withAlpha (0.08f));
        g.fillPath (fill);
        Acid::glowStroke (g, p, colour, 2.f);
    }

private:
    AcidanceProcessor& proc;
    int e;
    juce::Colour colour;
};

//==============================================================================
// Shows the LFO shape; drawing on it edits the 16-step custom shape.
class LfoEditor : public juce::Component
{
public:
    LfoEditor (AcidanceProcessor& p, int index, juce::Colour c) : proc (p), l (index), colour (Acid::accentFor (c)) {}

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (Acid::screen);
        g.fillRoundedRectangle (r, 6.f);
        g.setColour (Acid::line);
        g.drawHorizontalLine ((int) r.getCentreY(), r.getX(), r.getRight());

        const int shape = (int) readParam (proc, "lfo" + juce::String (l + 1) + "_shape");
        auto inner = r.reduced (8.f, 10.f);
        float custom[kLfoSteps];
        for (int s = 0; s < kLfoSteps; ++s) custom[s] = proc.lfoCustom[l][s].load();

        if (shape == 6)
        {
            const float w = inner.getWidth() / kLfoSteps;
            for (int s = 0; s < kLfoSteps; ++s)
            {
                const float v = custom[s];
                const float y0 = inner.getCentreY(), y1 = y0 - v * inner.getHeight() * 0.5f;
                auto bar = juce::Rectangle<float> (inner.getX() + s * w + 1.f, std::min (y0, y1), w - 2.f, std::max (1.f, std::abs (y1 - y0)));
                Acid::glowRect (g, bar, colour.withAlpha (0.85f), 2.f);
            }
            g.setColour (Acid::screenDim);
            g.setFont (Acid::font (10.f));
            g.drawText ("DRAW TO EDIT", r.reduced (6.f, 3.f), juce::Justification::topRight);
            return;
        }

        juce::Path p;
        uint32_t rng = 12345u;
        float sh = 0.f;
        const int n = (int) inner.getWidth();
        for (int i = 0; i <= n; ++i)
        {
            const float ph = (float) i / (float) n;
            if (shape == 5 && (i % std::max (1, n / 8)) == 0)
            {
                rng = rng * 1664525u + 1013904223u;
                sh = (float) (rng >> 8) * (2.f / 16777216.f) - 1.f;
            }
            const float v = lfoShapeValue (shape, ph, sh, custom);
            const float x = inner.getX() + (float) i, y = inner.getCentreY() - v * inner.getHeight() * 0.5f;
            if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
        }
        Acid::glowStroke (g, p, colour, 2.f);
        g.setColour (Acid::screenDim);
        g.setFont (Acid::font (10.f));
        g.drawText ("CLICK TO DRAW A CUSTOM SHAPE", r.reduced (6.f, 3.f), juce::Justification::topRight);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (auto* p = proc.apvts.getParameter ("lfo" + juce::String (l + 1) + "_shape"))
            if ((int) p->convertFrom0to1 (p->getValue()) != 6)
                p->setValueNotifyingHost (p->convertTo0to1 (6.f));
        edit (e);
    }
    void mouseDrag (const juce::MouseEvent& e) override { edit (e); }

private:
    void edit (const juce::MouseEvent& e)
    {
        auto inner = getLocalBounds().toFloat().reduced (8.f, 10.f);
        const int s = juce::jlimit (0, kLfoSteps - 1, (int) ((e.position.x - inner.getX()) / (inner.getWidth() / kLfoSteps)));
        const float v = juce::jlimit (-1.f, 1.f, (inner.getCentreY() - e.position.y) / (inner.getHeight() * 0.5f));
        proc.lfoCustom[l][s] = std::abs (v) < 0.06f ? 0.f : v;
        repaint();
    }

    AcidanceProcessor& proc;
    int l;
    juce::Colour colour;
};

//==============================================================================
class OscPage : public Page
{
public:
    explicit OscPage (AcidanceProcessor& p)
    {
        auto& s = p.apvts;
        const juce::Colour cols[2] = { Acid::green, Acid::cyan };
        for (int o = 0; o < 2; ++o)
        {
            const juce::String pre = "osc" + juce::String (o + 1) + "_";
            auto* panel = add (new Panel ("OSC " + juce::String (o + 1), cols[o], s));
            auto& u = ui[o];
            u.panel = panel;
            u.on = panel->toggle (pre + "on", "ON");
            u.wave = panel->choice (pre + "wave", Lists::oscWavesShort, 2);
            u.display = panel->add (new WaveDisplay (p, o, cols[o]));
            u.knobs1 = { panel->knob (pre + "wtpos", "WT POS"), panel->knob (pre + "oct", "OCTAVE", true),
                         panel->knob (pre + "semi", "SEMI", true), panel->knob (pre + "fine", "FINE", true),
                         panel->knob (pre + "level", "LEVEL") };
            u.knobs2 = { panel->knob (pre + "uni", "UNISON"), panel->knob (pre + "detune", "DETUNE"),
                         panel->knob (pre + "blend", "BLEND"), panel->knob (pre + "phase", "PHASE"),
                         panel->knob (pre + "warpamt", "WARP AMT") };
            u.rand = panel->toggle (pre + "rphase", "RAND PHASE");
            u.lock = panel->toggle (pre + "lock", "PHASE LOCK");
            u.lock->setTooltip ("Restart the oscillator at exactly 0 degrees on every note");
            u.warp = panel->choice (pre + "warp", { "NO WARP", "FM", "AM", "RING" });
            u.shape = panel->choice (pre + "shape", { "OFF", "FOLD", "PWM", "ASYM" });
            u.shape->setTooltip ("Shapes the wave: FOLD folds it back on itself, PWM squeezes the cycle, ASYM adds even harmonics");
            u.shapeAmt = panel->knob (pre + "shapeamt", "SHAPE");
            u.wtStep = panel->toggle (pre + "wtstep", "WT STEP");
            u.wtStep->setTooltip ("Wavetable position jumps from frame to frame instead of morphing: 8-bit / chip sounds");
            u.uniMode = panel->choice (pre + "unimode", { "UNI", "PWR", "MIN", "MAJ" });
            u.uniMode->setTooltip ("Unison mode: classic detune, or the unison voices stacked as a power, minor or major chord");
            live.add (u.wave); live.add (u.display); live.add (u.warp); live.add (u.shape); live.add (u.uniMode);
        }

        sub = add (new Panel ("SUB", Acid::orange, s));
        subWave = sub->choice ("sub_wave", { "SIN", "TRI", "SQR" });
        subOct = sub->choice ("sub_oct", { "-1 OCT", "-2 OCT" });
        subLevel = sub->knob ("sub_level", "LEVEL");

        noise = add (new Panel ("NOISE", Acid::magenta, s));
        noiseType = noise->choice ("noise_type", { "WHITE", "PINK" });
        noiseKnobs = { noise->knob ("noise_level", "LEVEL"), noise->knob ("noise_pitch", "PITCH") };

        voice = add (new Panel ("VOICE", Acid::violet, s));
        voiceMode = voice->choice ("mode", { "POLY", "MONO", "LEGATO" });
        oversample = voice->choice ("oversample", { "1x", "2x", "4x", "8x" });
        oversample->setTooltip ("Internal oversampling of the synth engine (higher = cleaner, more CPU)");
        live.add (oversample);
        voiceKnobs = { voice->knob ("glide", "GLIDE"), voice->knob ("bend", "BEND RANGE"),
                       voice->knob ("vel_sens", "VELOCITY"), voice->knob ("accent", "ACCENT") };
        live.add (subWave); live.add (subOct); live.add (noiseType); live.add (voiceMode);
    }

    void resized() override
    {
        const int w = (getWidth() - 16) / 2;
        for (int o = 0; o < 2; ++o)
        {
            auto& u = ui[o];
            u.panel->setBounds (o * (w + 16), 0, w, 360);
            u.on->setBounds (w - 70, 8, 56, 26);
            u.wave->setBounds (14, 42, 330, 58);
            u.display->setBounds (356, 42, w - 370, 116);
            Panel::row ({ u.knobs1[0], u.knobs1[1], u.knobs1[2], u.knobs1[3], u.knobs1[4] }, 14, 108, 64, 84, 2);
            Panel::row ({ u.knobs2[0], u.knobs2[1], u.knobs2[2], u.knobs2[3], u.knobs2[4] }, 14, 200, 64, 84, 2);
            u.rand->setBounds (14, 298, 112, 30);
            u.lock->setBounds (130, 298, 112, 30);
            u.warp->setBounds (250, 298, w - 264, 30);
            u.shape->setBounds (356, 166, w - 370, 28);
            u.shapeAmt->setBounds (356, 198, 64, 84);
            u.wtStep->setBounds (426, 204, w - 440, 30);
            u.uniMode->setBounds (426, 244, w - 440, 30);
        }
        const int y = 376, h = getHeight() - y;
        sub->setBounds (0, y, 300, h);
        subWave->setBounds (14, 44, 272, 30);
        subOct->setBounds (14, 82, 272, 30);
        subLevel->setBounds (110, 124, 80, 96);
        noise->setBounds (316, y, 300, h);
        noiseType->setBounds (14, 44, 272, 30);
        Panel::row ({ noiseKnobs[0], noiseKnobs[1] }, 60, 100, 80, 96, 20);
        voice->setBounds (632, y, getWidth() - 632, h);
        voiceMode->setBounds (14, 44, 300, 30);
        oversample->setBounds (voice->getWidth() - 214, 44, 200, 30);
        Panel::row ({ voiceKnobs[0], voiceKnobs[1], voiceKnobs[2], voiceKnobs[3] }, 14, 100, 86, 100, 40);
    }

private:
    template <class T> T* add (T* c) { owned.add (c); addAndMakeVisible (c); return c; }

    struct OscUi
    {
        Panel* panel = nullptr;
        juce::TextButton* on = nullptr, *rand = nullptr, *lock = nullptr;
        ChoiceButtons* wave = nullptr, *warp = nullptr, *shape = nullptr, *uniMode = nullptr;
        Knob* shapeAmt = nullptr;
        juce::TextButton* wtStep = nullptr;
        WaveDisplay* display = nullptr;
        std::array<Knob*, 5> knobs1 {}, knobs2 {};
    } ui[2];

    Panel* sub, *noise, *voice;
    ChoiceButtons* subWave, *subOct, *noiseType, *voiceMode, *oversample;
    Knob* subLevel;
    std::array<Knob*, 2> noiseKnobs {};
    std::array<Knob*, 4> voiceKnobs {};
    juce::OwnedArray<juce::Component> owned;
};

//==============================================================================
class FilterPage : public Page
{
public:
    explicit FilterPage (AcidanceProcessor& p)
    {
        auto& s = p.apvts;
        const juce::Colour cols[2] = { Acid::green, Acid::cyan };
        for (int f = 0; f < 2; ++f)
        {
            const juce::String pre = "flt" + juce::String (f + 1) + "_";
            auto& u = ui[f];
            u.panel = add (new Panel ("FILTER " + juce::String (f + 1), cols[f], s));
            u.type = u.panel->choice (pre + "type", { "OFF", "ACID 24", "LP 12", "LP 24", "HP 12", "HP 24", "BP 12", "BP 24", "NOTCH", "COMB" }, 2);
            u.knobs = { u.panel->knob (pre + "cutoff", "CUTOFF"), u.panel->knob (pre + "reso", "RESONANCE"),
                        u.panel->knob (pre + "drive", "DRIVE"), u.panel->knob (pre + "key", "KEY TRACK"),
                        u.panel->knob (pre + "env", "ENV 2 AMT", true) };
            live.add (u.type);
        }
        routing = add (new Panel ("ROUTING", Acid::violet, s));
        routingChoice = routing->choice ("flt_routing", { "SERIAL", "PARALLEL", "SPLIT" });
        warmth = routing->knob ("warmth", "ANALOG WARMTH");
        drift = routing->knob ("drift", "ANALOG DRIFT");
        live.add (routingChoice);

        envPanel = add (new Panel ("FILTER ENVELOPE (ENV 2)", Acid::orange, s));
        envDisplay = envPanel->add (new EnvDisplay (p, 1, Acid::orange));
        envKnobs = { envPanel->knob ("env2_a", "ATTACK"), envPanel->knob ("env2_d", "DECAY"),
                     envPanel->knob ("env2_s", "SUSTAIN"), envPanel->knob ("env2_r", "RELEASE") };
        live.add (envDisplay);
        live.add (routing);
    }

    void resized() override
    {
        const int w = (getWidth() - 16) / 2;
        for (int f = 0; f < 2; ++f)
        {
            auto& u = ui[f];
            u.panel->setBounds (f * (w + 16), 0, w, 300);
            u.type->setBounds (14, 44, w - 28, 64);
            Panel::row ({ u.knobs[0], u.knobs[1], u.knobs[2], u.knobs[3], u.knobs[4] }, 18, 130, 100, 116, 8);
        }
        routing->setBounds (0, 316, w, getHeight() - 316);
        routingChoice->setBounds (14, 44, w - 28, 36);
        warmth->setBounds (w - 124, 92, 110, 120);
        drift->setBounds (w - 124, 220, 110, 120);
        envPanel->setBounds (w + 16, 316, w, getHeight() - 316);
        envDisplay->setBounds (14, 42, w - 28, 120);
        Panel::row ({ envKnobs[0], envKnobs[1], envKnobs[2], envKnobs[3] }, 40, 172, 90, 100, 40);
    }

    void paintOverChildren (juce::Graphics& g) override
    {
        // Routing description
        auto r = routing->getBounds().withTrimmedTop (92).withTrimmedRight (140).reduced (20, 10);
        const int mode = routingChoice->getIndex();
        const char* text[] = {
            "OSC 1 + OSC 2 + SUB + NOISE  ->  FILTER 1  ->  FILTER 2",
            "OSC 1 + OSC 2 + SUB + NOISE  ->  FILTER 1  +  FILTER 2  (side by side)",
            "OSC 1 + SUB  ->  FILTER 1          OSC 2 + NOISE  ->  FILTER 2" };
        g.setColour (Acid::dim);
        g.setFont (Acid::font (13.f, false));
        g.drawFittedText (juce::String (text[mode]) + "\n\nSet a filter to OFF to bypass it. ENV 2 AMT sets how far the filter envelope moves each cutoff.\n\nANALOG WARMTH adds gentle saturation before and after the filters. ANALOG DRIFT lets every voice wander slightly in pitch and cutoff, like an analog poly.",
                          r, juce::Justification::topLeft, 6);
    }

private:
    template <class T> T* add (T* c) { owned.add (c); addAndMakeVisible (c); return c; }

    struct FilterUi { Panel* panel = nullptr; ChoiceButtons* type = nullptr; std::array<Knob*, 5> knobs {}; } ui[2];
    Panel* routing, *envPanel;
    ChoiceButtons* routingChoice;
    Knob* warmth, *drift;
    EnvDisplay* envDisplay;
    std::array<Knob*, 4> envKnobs {};
    juce::OwnedArray<juce::Component> owned;
};

//==============================================================================
class EnvPage : public Page
{
public:
    explicit EnvPage (AcidanceProcessor& p)
    {
        const char* titles[] = { "ENV 1  -  AMP", "ENV 2  -  FILTER", "ENV 3  -  MOD", "ENV 4  -  MOD" };
        const juce::Colour cols[] = { Acid::green, Acid::orange, Acid::cyan, Acid::magenta };
        for (int e = 0; e < kNumEnvs; ++e)
        {
            const juce::String pre = "env" + juce::String (e + 1) + "_";
            auto& u = ui[e];
            u.panel = add (new Panel (titles[e], cols[e], p.apvts));
            u.display = u.panel->add (new EnvDisplay (p, e, cols[e]));
            u.knobs = { u.panel->knob (pre + "a", "ATTACK"), u.panel->knob (pre + "d", "DECAY"),
                        u.panel->knob (pre + "s", "SUSTAIN"), u.panel->knob (pre + "r", "RELEASE") };
            live.add (u.display);
        }
    }

    void resized() override
    {
        const int w = (getWidth() - 16) / 2, h = (getHeight() - 16) / 2;
        for (int e = 0; e < kNumEnvs; ++e)
        {
            auto& u = ui[e];
            u.panel->setBounds ((e % 2) * (w + 16), (e / 2) * (h + 16), w, h);
            u.display->setBounds (14, 42, w - 28, h - 160);
            Panel::row ({ u.knobs[0], u.knobs[1], u.knobs[2], u.knobs[3] }, 40, h - 108, 84, 96, 46);
        }
    }

    void paintOverChildren (juce::Graphics& g) override
    {
        g.setColour (Acid::dim);
        g.setFont (Acid::font (11.f, false));
        g.drawText ("Route ENV 3 / ENV 4 in the MATRIX tab", ui[2].panel->getBounds().removeFromTop (34).withTrimmedLeft (200).withTrimmedRight (14),
                    juce::Justification::centredRight);
    }

private:
    template <class T> T* add (T* c) { owned.add (c); addAndMakeVisible (c); return c; }
    struct EnvUi { Panel* panel = nullptr; EnvDisplay* display = nullptr; std::array<Knob*, 4> knobs {}; } ui[kNumEnvs];
    juce::OwnedArray<juce::Component> owned;
};

//==============================================================================
class LfoPage : public Page
{
public:
    explicit LfoPage (AcidanceProcessor& p)
    {
        const juce::Colour cols[] = { Acid::green, Acid::cyan, Acid::violet, Acid::magenta };
        for (int l = 0; l < kNumLfos; ++l)
        {
            const juce::String pre = "lfo" + juce::String (l + 1) + "_";
            auto& u = ui[l];
            const auto c = cols[l % 4];
            u.panel = add (new Panel ("LFO " + juce::String (l + 1), c, p.apvts));
            u.shape = u.panel->choice (pre + "shape", Lists::lfoShapesShort, 2);
            u.editor = u.panel->add (new LfoEditor (p, l, c));
            u.knobs = { u.panel->knob (pre + "rate", "RATE"), u.panel->knob (pre + "phase", "PHASE"),
                        u.panel->knob (pre + "fade", "FADE IN") };
            u.sync = u.panel->toggle (pre + "sync", "SYNC");
            u.div = u.panel->combo (pre + "div", Lists::divisions);
            u.trig = u.panel->choice (pre + "trig", { "GLOBAL", "RETRIG" });
            live.add (u.shape); live.add (u.trig); live.add (u.editor);
        }
    }

    void resized() override
    {
        const int gap = 12;
        const int w = (getWidth() - gap * 3) / 4, h = (getHeight() - gap) / 2;
        for (int l = 0; l < kNumLfos; ++l)
        {
            auto& u = ui[l];
            u.panel->setBounds ((l % 4) * (w + gap), (l / 4) * (h + gap), w, h);
            u.shape->setBounds (12, 42, w - 24, 50);
            u.editor->setBounds (12, 98, w - 24, h - 266);
            Panel::row ({ u.knobs[0], u.knobs[1], u.knobs[2] }, 14, h - 162, (w - 36) / 3, 84, 4);
            u.sync->setBounds (12, h - 70, 62, 26);
            u.div->setBounds (80, h - 70, w - 92, 26);
            u.trig->setBounds (12, h - 38, w - 24, 26);
        }
    }

private:
    template <class T> T* add (T* c) { owned.add (c); addAndMakeVisible (c); return c; }
    struct LfoUi
    {
        Panel* panel = nullptr;
        ChoiceButtons* shape = nullptr, *trig = nullptr;
        juce::TextButton* sync = nullptr;
        juce::ComboBox* div = nullptr;
        std::array<Knob*, 3> knobs {};
        LfoEditor* editor = nullptr;
    } ui[kNumLfos];
    juce::OwnedArray<juce::Component> owned;
};

//==============================================================================
class MatrixPage : public Page
{
public:
    explicit MatrixPage (AcidanceProcessor& p)
    {
        panel = std::make_unique<Panel> ("MODULATION MATRIX  -  16 SLOTS, AUDIO RATE", Acid::violet, p.apvts);
        addAndMakeVisible (*panel);
        for (int m = 0; m < kNumModSlots; ++m)
        {
            const juce::String pre = "mod" + juce::String (m + 1) + "_";
            auto& r = rows[m];
            r.src = panel->combo (pre + "src", Lists::modSources);
            r.dst = panel->combo (pre + "dst", Lists::modDests);
            r.amt = panel->add (new juce::Slider (juce::Slider::LinearHorizontal, juce::Slider::NoTextBox));
            r.amt->setColour (juce::Slider::rotarySliderFillColourId, Acid::accentFor (Acid::violet));
            r.amt->setPopupDisplayEnabled (true, true, nullptr, 900);
            r.amt->setDoubleClickReturnValue (true, 0.0);
            amtAtts.add (new APVTS::SliderAttachment (p.apvts, pre + "amt", *r.amt));
        }
        live.add (panel.get());
    }

    void resized() override
    {
        panel->setBounds (getLocalBounds());
        for (int m = 0; m < kNumModSlots; ++m)
        {
            const auto b = slotBounds (m);
            rows[m].src->setBounds (b.getX() + 34, b.getY(), 160, 30);
            rows[m].dst->setBounds (b.getX() + 222, b.getY(), 168, 30);
            rows[m].amt->setBounds (b.getX() + 398, b.getY(), b.getWidth() - 450, 30);
        }
    }

    void paintOverChildren (juce::Graphics& g) override
    {
        for (int col = 0; col < 2; ++col)
        {
            const auto b = slotBounds (col * 8);
            g.setFont (Acid::font (10.5f));
            g.setColour (Acid::dim);
            g.drawText ("SOURCE", b.getX() + 34, b.getY() - 20, 160, 16, juce::Justification::centredLeft);
            g.drawText ("DESTINATION", b.getX() + 222, b.getY() - 20, 168, 16, juce::Justification::centredLeft);
            g.drawText ("AMOUNT", b.getX() + 398, b.getY() - 20, 120, 16, juce::Justification::centredLeft);
        }
        for (int m = 0; m < kNumModSlots; ++m)
        {
            const auto b = slotBounds (m);
            const bool active = rows[m].src->getSelectedItemIndex() > 0 && rows[m].dst->getSelectedItemIndex() > 0;
            g.setFont (Acid::font (14.f));
            g.setColour (active ? Acid::accentFor (Acid::violet) : Acid::dim);
            g.drawText (juce::String (m + 1), b.getX(), b.getY(), 28, 30, juce::Justification::centred);
            g.setColour (Acid::dim);
            g.drawText (">", b.getX() + 196, b.getY(), 24, 30, juce::Justification::centred);
            g.setColour (active ? Acid::text : Acid::dim);
            g.setFont (Acid::font (12.f));
            g.drawText (juce::String (juce::roundToInt (rows[m].amt->getValue() * 100.0)), b.getRight() - 46, b.getY(), 44, 30,
                        juce::Justification::centredRight);
        }
    }

private:
    juce::Rectangle<int> slotBounds (int m) const
    {
        const int colW = (getWidth() - 48) / 2;
        const int rowH = std::min (64, (getHeight() - 84) / 8);
        return { 16 + (m / 8) * (colW + 16), 74 + (m % 8) * rowH, colW, 30 };
    }

    std::unique_ptr<Panel> panel;
    struct Row { juce::ComboBox* src = nullptr, *dst = nullptr; juce::Slider* amt = nullptr; } rows[kNumModSlots];
    juce::OwnedArray<APVTS::SliderAttachment> amtAtts;
};

//==============================================================================
class FxPage : public Page
{
public:
    // The rack in signal order: DIST -> FX FILTER -> MOD FX -> DELAY -> REVERB -> COMP -> WIDTH -> EQ
    explicit FxPage (AcidanceProcessor& p)
    {
        auto& s = p.apvts;
        dist = add (new Panel ("DISTORTION", Acid::orange, s));
        distOn = dist->toggle ("dist_on", "ON");
        distType = dist->choice ("dist_type", Lists::distTypesShort, 2);
        distType->setTooltip ("TUBE / SOFT / HARD / CRUSH, FUZZ (asymmetric), FOLD (wavefolder), RECT (adds the octave), "
                              "DIODE, SCREAM (mid-push hard clip), DESTROY (fold + clip + crush, the hardest)");
        distKnobs = { dist->knob ("dist_drive", "DRIVE"), dist->knob ("dist_tone", "TONE"), dist->knob ("dist_mix", "MIX") };

        fxf = add (new Panel ("FX FILTER", Acid::orange, s));
        fxfOn = fxf->toggle ("fxf_on", "ON");
        fxfType = fxf->choice ("fxf_type", { "LP", "HP", "BP", "NOTCH", "ACID" });
        fxfKnobs = { fxf->knob ("fxf_cut", "CUTOFF"), fxf->knob ("fxf_res", "RESO"),
                     fxf->knob ("fxf_drive", "DRIVE"), fxf->knob ("fxf_mix", "MIX") };

        mfx = add (new Panel ("MOD FX", Acid::cyan, s));
        mfxOn = mfx->toggle ("mfx_on", "ON");
        mfxType = mfx->choice ("mfx_type", { "CHORUS", "PHASER", "FLANGER", "RING", "TREMOLO", "PAN" }, 2);
        mfxKnobs = { mfx->knob ("mfx_rate", "RATE"), mfx->knob ("mfx_depth", "DEPTH"),
                     mfx->knob ("mfx_fb", "FEEDBACK"), mfx->knob ("mfx_mix", "MIX") };

        dly = add (new Panel ("DELAY", Acid::green, s));
        dlyOn = dly->toggle ("dly_on", "ON");
        dlySync = dly->toggle ("dly_sync", "SYNC");
        dlyPP = dly->toggle ("dly_pp", "PING-PONG");
        dlyDiv = dly->combo ("dly_div", Lists::divisions);
        dlyKnobs = { dly->knob ("dly_ms", "TIME MS"), dly->knob ("dly_fb", "FEEDBACK"), dly->knob ("dly_mix", "MIX") };

        rev = add (new Panel ("REVERB", Acid::violet, s));
        revOn = rev->toggle ("rev_on", "ON");
        revKnobs = { rev->knob ("rev_size", "SIZE"), rev->knob ("rev_decay", "DECAY"),
                     rev->knob ("rev_damp", "HIGH CUT"), rev->knob ("rev_mix", "MIX") };

        comp = add (new Panel ("COMPRESSOR", Acid::magenta, s));
        compOn = comp->toggle ("comp_on", "ON");
        compMode = comp->choice ("comp_mode", { "NORMAL", "OTT" });
        compKnobs = { comp->knob ("comp_thr", "THRESH"), comp->knob ("comp_ratio", "RATIO"), comp->knob ("comp_att", "ATTACK"),
                      comp->knob ("comp_rel", "RELEASE"), comp->knob ("comp_makeup", "MAKEUP"), comp->knob ("comp_depth", "OTT DEPTH") };

        wid = add (new Panel ("WIDTH", Acid::magenta, s));
        widOn = wid->toggle ("wid_on", "ON");
        widKnobs = { wid->knob ("wid_amt", "WIDTH"), wid->knob ("wid_mono", "BASS MONO") };

        eq = add (new Panel ("MASTER EQ", Acid::yellow, s));
        eqOn = eq->toggle ("eq_on", "ON");
        eqKnobs = { eq->knob ("eq_lf", "LOW FREQ"), eq->knob ("eq_lg", "LOW GAIN", true), eq->knob ("eq_mf", "MID FREQ"),
                    eq->knob ("eq_mq", "MID Q"), eq->knob ("eq_mg", "MID GAIN", true), eq->knob ("eq_hf", "HIGH FREQ"),
                    eq->knob ("eq_hg", "HIGH GAIN", true) };
        live.add (distType); live.add (fxfType); live.add (mfxType); live.add (compMode);
    }

    void resized() override
    {
        const int w = (getWidth() - 48) / 4, h = (getHeight() - 16) / 2;
        Panel* panels[] = { dist, fxf, mfx, dly, rev, comp, wid, eq };
        juce::TextButton* ons[] = { distOn, fxfOn, mfxOn, dlyOn, revOn, compOn, widOn, eqOn };
        for (int i = 0; i < 8; ++i)
        {
            panels[i]->setBounds ((i % 4) * (w + 16), (i / 4) * (h + 16), w, h);
            ons[i]->setBounds (w - 64, 8, 52, 26);
        }
        // knobs centred in rows of `cols`
        auto grid = [w] (std::initializer_list<juce::Component*> items, int cols, int y, int kw, int kh, int gapY = 6)
        {
            const int gap = 6, x0 = (w - (cols * kw + (cols - 1) * gap)) / 2;
            int i = 0;
            for (auto* c : items)
            {
                c->setBounds (x0 + (i % cols) * (kw + gap), y + (i / cols) * (kh + gapY), kw, kh);
                ++i;
            }
        };
        distType->setBounds (14, 46, w - 28, 64);
        grid ({ distKnobs[0], distKnobs[1], distKnobs[2] }, 3, 136, 80, 110);
        fxfType->setBounds (14, 46, w - 28, 30);
        grid ({ fxfKnobs[0], fxfKnobs[1], fxfKnobs[2], fxfKnobs[3] }, 2, 92, 90, 110, 12);
        mfxType->setBounds (14, 46, w - 28, 60);
        grid ({ mfxKnobs[0], mfxKnobs[1], mfxKnobs[2], mfxKnobs[3] }, 2, 116, 90, 104, 8);
        dlySync->setBounds (14, 46, 70, 30);
        dlyDiv->setBounds (90, 46, w - 104, 30);
        dlyPP->setBounds (14, 84, w - 28, 28);
        grid ({ dlyKnobs[0], dlyKnobs[1], dlyKnobs[2] }, 3, 136, 80, 110);
        grid ({ revKnobs[0], revKnobs[1], revKnobs[2], revKnobs[3] }, 2, 48, 90, 130, 14);
        compMode->setBounds (14, 46, w - 28, 28);
        grid ({ compKnobs[0], compKnobs[1], compKnobs[2], compKnobs[3], compKnobs[4], compKnobs[5] }, 3, 88, 80, 110, 14);
        grid ({ widKnobs[0], widKnobs[1] }, 2, 70, 100, 130);
        grid ({ eqKnobs[0], eqKnobs[1], eqKnobs[2], eqKnobs[3] }, 4, 50, 60, 110, 14);
        grid ({ eqKnobs[4], eqKnobs[5], eqKnobs[6] }, 3, 174, 60, 110);
    }

    void paintOverChildren (juce::Graphics& g) override
    {
        // WIDTH caption under its knobs
        g.setColour (Acid::dim);
        g.setFont (Acid::font (10.5f, false));
        g.drawFittedText ("Sides below BASS MONO are folded to mono for a tight low end. WIDTH above 1 spreads the highs.",
                          wid->getBounds().reduced (18, 0).withTrimmedTop (230).withHeight (60), juce::Justification::centredTop, 3);
    }

private:
    template <class T> T* add (T* c) { owned.add (c); addAndMakeVisible (c); return c; }
    Panel* dist, *fxf, *mfx, *dly, *rev, *comp, *wid, *eq;
    juce::TextButton* distOn, *fxfOn, *mfxOn, *dlyOn, *revOn, *compOn, *widOn, *eqOn, *dlySync, *dlyPP;
    ChoiceButtons* distType, *fxfType, *mfxType, *compMode;
    juce::ComboBox* dlyDiv;
    std::array<Knob*, 3> distKnobs {}, dlyKnobs {};
    std::array<Knob*, 4> fxfKnobs {}, mfxKnobs {}, revKnobs {};
    std::array<Knob*, 2> widKnobs {};
    std::array<Knob*, 6> compKnobs {};
    std::array<Knob*, 7> eqKnobs {};
    juce::OwnedArray<juce::Component> owned;
};

//==============================================================================
class SeqGrid : public juce::Component
{
public:
    SeqGrid (AcidanceProcessor& p, ChoiceButtons& sel) : proc (p), selector (sel) {}

    static constexpr int numRows = 16, labelW = 64;

    juce::Rectangle<float> cell (int row, int col) const
    {
        const float cw = (getWidth() - labelW) / (float) kNumSteps;
        const float rh = getHeight() / (float) numRows;
        return { labelW + col * cw, row * rh, cw, rh };
    }

    void paint (juce::Graphics& g) override
    {
        const auto colour = Acid::green;
        auto& steps = proc.seq.steps[selector.getIndex()];
        const int playing = proc.currentStep.load();

        for (int row = 0; row < numRows; ++row)
        {
            const auto lr = cell (row, 0).withX (0).withWidth ((float) labelW - 6.f);
            juce::Colour rowBg = Acid::panel2;
            if (row < 13)
            {
                const int n = 12 - row;
                rowBg = isBlackKey (n) ? Acid::bg.brighter (0.03f) : Acid::panel2;
                g.setColour (isBlackKey (n) ? Acid::bg : Acid::text.withAlpha (0.85f));
                g.fillRoundedRectangle (lr.reduced (1.f), 2.f);
                g.setColour (isBlackKey (n) ? Acid::dim : Acid::bg);
                g.setFont (Acid::font (11.f));
                g.drawText (noteNames[n], lr.reduced (6.f, 0.f), juce::Justification::centredRight);
            }
            else
            {
                g.setColour (Acid::dim);
                g.setFont (Acid::font (11.f));
                g.drawText (row == 13 ? "OCT" : (row == 14 ? "ACCENT" : "SLIDE"), lr.reduced (6.f, 0.f), juce::Justification::centredRight);
            }
            for (int col = 0; col < kNumSteps; ++col)
            {
                g.setColour (row >= 13 ? Acid::bg.brighter (0.05f) : rowBg);
                g.fillRect (cell (row, col).reduced (1.5f));
            }
        }
        for (int col = 0; col <= kNumSteps; col += 4)
        {
            g.setColour (Acid::line.brighter (0.3f));
            g.drawVerticalLine ((int) cell (0, col).getX(), 0.f, (float) getHeight());
        }
        for (int col = 0; col < kNumSteps; ++col)
        {
            auto& s = steps[col];
            if (s.gate.load())
            {
                Acid::glowRect (g, cell (12 - s.note.load(), col).reduced (4.f, 3.f), s.accent.load() ? colour.brighter (0.4f) : colour, 3.f);
                const int oct = s.octave.load();
                if (oct != 1)
                {
                    g.setColour (colour);
                    g.setFont (Acid::font (11.f));
                    g.drawText (oct == 2 ? "UP" : "DN", cell (13, col), juce::Justification::centred);
                }
            }
            if (s.accent.load()) Acid::glowRect (g, cell (14, col).reduced (8.f, 7.f), Acid::yellow, 3.f);
            if (s.slide.load())
            {
                auto c = cell (15, col).reduced (0.f, 9.f);
                Acid::glowRect (g, c.withX (c.getCentreX()).getIntersection (getLocalBounds().toFloat()), Acid::magenta, 2.f);
            }
        }
        if (playing >= 0)
        {
            auto col = cell (0, playing).withHeight ((float) getHeight());
            g.setColour (juce::Colours::white.withAlpha (0.07f));
            g.fillRect (col);
            g.setColour (colour.withAlpha (0.6f));
            g.drawRect (col, 1.f);
        }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.x < labelW) return;
        const int col = juce::jlimit (0, kNumSteps - 1, (int) ((e.x - labelW) / ((getWidth() - labelW) / (float) kNumSteps)));
        const int row = juce::jlimit (0, numRows - 1, (int) (e.y / (getHeight() / (float) numRows)));
        auto& s = proc.seq.steps[selector.getIndex()][col];
        if (row < 13)
        {
            const int n = 12 - row;
            if (s.gate.load() && s.note.load() == n) s.gate = 0;
            else { s.gate = 1; s.note = (int8_t) n; }
        }
        else if (row == 13) { const int o = s.octave.load(); s.octave = (int8_t) (o == 1 ? 2 : (o == 2 ? 0 : 1)); }
        else if (row == 14) s.accent = (int8_t) ! s.accent.load();
        else s.slide = (int8_t) ! s.slide.load();
        repaint();
    }

private:
    AcidanceProcessor& proc;
    ChoiceButtons& selector;
};

//==============================================================================
// Step-lock lane under the step grid: pick a lane, drag the bars up / down, double-click to clear
class LockLane : public juce::Component
{
public:
    LockLane (AcidanceProcessor& p, ChoiceButtons& sel) : proc (p), selector (sel) {}

    static constexpr int labelW = 64;
    int lane = 0;

    void paint (juce::Graphics& g) override
    {
        static const char* const names[] = { "CUTOFF", "RESO", "DECAY", "DRIVE" };
        const juce::Colour cols[] = { Acid::cyan, Acid::magenta, Acid::yellow, Acid::orange };
        const float lh = getHeight() / 4.f;
        for (int i = 0; i < kNumLocks; ++i)
        {
            auto r = juce::Rectangle<float> (0.f, i * lh, (float) labelW - 6.f, lh).reduced (1.f);
            if (i == lane) Acid::glowRect (g, r, cols[i], 3.f);
            else { g.setColour (Acid::panel2); g.fillRoundedRectangle (r, 3.f); }
            g.setColour (i == lane ? juce::Colour (0xff101014) : Acid::dim);
            g.setFont (Acid::font (10.f));
            g.drawText (names[i], r, juce::Justification::centred);
        }
        auto& steps = proc.seq.steps[selector.getIndex()];
        const float cw = (getWidth() - labelW) / (float) kNumSteps, mid = getHeight() * 0.5f;
        g.setColour (Acid::line);
        g.drawHorizontalLine ((int) mid, (float) labelW, (float) getWidth());
        for (int c = 0; c < kNumSteps; ++c)
        {
            const auto cell = juce::Rectangle<float> (labelW + c * cw, 0.f, cw, (float) getHeight()).reduced (2.f, 2.f);
            g.setColour (Acid::bg.brighter (0.04f));
            g.fillRect (cell);
            const int v = steps[c].lock[lane].load();
            if (v != 0)
            {
                const float hgt = (getHeight() * 0.5f - 3.f) * std::abs (v) / 100.f;
                auto bar = juce::Rectangle<float> (cell.getX() + 3.f, v > 0 ? mid - hgt : mid, cell.getWidth() - 6.f, hgt);
                Acid::glowRect (g, bar, cols[lane], 2.f);
            }
            if (proc.currentStep.load() == c)
            {
                g.setColour (juce::Colours::white.withAlpha (0.08f));
                g.fillRect (cell);
            }
        }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.x < labelW) { lane = juce::jlimit (0, kNumLocks - 1, (int) (e.y / (getHeight() / 4.f))); repaint(); return; }
        mouseDrag (e);
    }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (e.getMouseDownX() < labelW) return;
        const int c = juce::jlimit (0, kNumSteps - 1, (int) ((e.x - labelW) / ((getWidth() - labelW) / (float) kNumSteps)));
        const int v = juce::jlimit (-100, 100, (int) std::round ((getHeight() * 0.5f - e.y) / (getHeight() * 0.5f - 3.f) * 100.f));
        proc.seq.steps[selector.getIndex()][c].lock[lane] = (int8_t) (std::abs (v) < 6 ? 0 : v);
        repaint();
    }
    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        if (e.x < labelW) return;
        const int c = juce::jlimit (0, kNumSteps - 1, (int) ((e.x - labelW) / ((getWidth() - labelW) / (float) kNumSteps)));
        proc.seq.steps[selector.getIndex()][c].lock[lane] = 0;
        repaint();
    }

private:
    AcidanceProcessor& proc;
    ChoiceButtons& selector;
};

class SeqPage : public Page
{
public:
    explicit SeqPage (AcidanceProcessor& p) : proc (p)
    {
        panel = std::make_unique<Panel> ("STEP SEQUENCER", Acid::green, p.apvts);
        addAndMakeVisible (*panel);
        onButton = panel->toggle ("seq_on", "SEQ ON");
        pattern = panel->choice ("seq_pattern", { "1", "2", "3", "4", "5", "6", "7", "8" });
        knobs = { panel->knob ("seq_transpose", "TRANSPOSE", true), panel->knob ("seq_gate", "GATE"),
                  panel->knob ("shuffle", "SHUFFLE") };
        random = panel->add (new juce::TextButton ("ACID RANDOM"));
        arp = panel->add (new juce::TextButton ("ARP RANDOM"));
        clear = panel->add (new juce::TextButton ("CLEAR"));
        for (auto* b : { random, arp, clear }) b->setColour (juce::TextButton::buttonOnColourId, Acid::green);
        random->onClick = [this] { proc.seq.randomise (pattern->getIndex(), (uint32_t) juce::Random::getSystemRandom().nextInt()); };
        arp->onClick = [this] { proc.seq.randomiseArp (pattern->getIndex(), (uint32_t) juce::Random::getSystemRandom().nextInt()); };
        clear->onClick = [this] { proc.seq.clear (pattern->getIndex()); };
        grid = panel->add (new SeqGrid (p, *pattern));
        lane = panel->add (new LockLane (p, *pattern));
        live.add (pattern); live.add (grid); live.add (lane);

        arpPanel = std::make_unique<Panel> ("ARPEGGIATOR", Acid::cyan, p.apvts);
        addAndMakeVisible (*arpPanel);
        arpOn = arpPanel->toggle ("arp_on", "ARP ON");
        arpMode = arpPanel->choice ("arp_mode", Lists::arpModesShort);
        arpDiv = arpPanel->combo ("arp_div", Lists::divisions);
        arpKnobs = { arpPanel->knob ("arp_oct", "OCTAVES"), arpPanel->knob ("arp_gate", "GATE"), arpPanel->knob ("arp_swing", "SWING") };
        live.add (arpMode);
    }

    void resized() override
    {
        const int arpH = 132;
        panel->setBounds (getLocalBounds().withTrimmedBottom (arpH + 14));
        arpPanel->setBounds (0, getHeight() - arpH, getWidth(), arpH);
        arpOn->setBounds (20, 50, 100, 34);
        arpMode->setBounds (136, 48, 470, 38);
        arpDiv->setBounds (622, 52, 110, 30);
        Panel::row ({ arpKnobs[0], arpKnobs[1], arpKnobs[2] }, 760, 34, 80, 92, 10);
        onButton->setBounds (20, 52, 100, 34);
        pattern->setBounds (136, 52, 330, 34);
        Panel::row ({ knobs[0], knobs[1], knobs[2] }, 490, 36, 74, 80, 6);
        random->setBounds (740, 52, 130, 34);
        arp->setBounds (876, 52, 130, 34);
        clear->setBounds (1012, 52, 130, 34);
        const int laneH = 92;
        grid->setBounds (20, 128, getWidth() - 40, panel->getHeight() - 144 - laneH - 8);
        lane->setBounds (20, panel->getHeight() - 16 - laneH, getWidth() - 40, laneH);
    }

    void paintOverChildren (juce::Graphics& g) override
    {
        g.setColour (Acid::dim);
        g.setFont (Acid::font (11.f, false));
        g.drawText ("Plays only while MIDI notes come in (DAW clip or keyboard), in time with the song or RUN. C3 = no transpose.",
                    200, 10, getWidth() - 220, 22, juce::Justification::centredRight);
        g.drawText ("Hold a chord: the ARP plays it on the song grid (or from the first key when stopped). SEQ ON takes the keys first.",
                    200, arpPanel->getY() + 10, getWidth() - 220, 22, juce::Justification::centredRight);
    }

private:
    AcidanceProcessor& proc;
    std::unique_ptr<Panel> panel, arpPanel;
    juce::TextButton* onButton, *random, *arp, *clear, *arpOn;
    ChoiceButtons* arpMode;
    juce::ComboBox* arpDiv;
    std::array<Knob*, 3> arpKnobs {};
    ChoiceButtons* pattern;
    std::array<Knob*, 3> knobs {};
    SeqGrid* grid;
    LockLane* lane;
};

//==============================================================================
// RANDOM: a neon yellow-to-pink button with a dice, gently pulsing, so it stands out in every skin
class RandomButton : public juce::Button, private juce::Timer
{
public:
    RandomButton() : juce::Button ("RANDOM") { startTimerHz (30); }

    void paintButton (juce::Graphics& g, bool over, bool down) override
    {
        auto r = getLocalBounds().toFloat().reduced (3.f);
        const float pulse = 0.5f + 0.5f * std::sin (phase);
        const auto hot = juce::Colour (0xffffe14d), pink = juce::Colour (0xffff2bd6);

        // glow
        for (int i = 3; i > 0; --i)
        {
            g.setColour (pink.withAlpha ((0.10f + 0.10f * pulse + (over ? 0.08f : 0.f)) / (float) i));
            g.fillRoundedRectangle (r.expanded ((float) i * 1.6f), 9.f + (float) i);
        }

        auto body = down ? r.reduced (1.5f) : r;
        g.setGradientFill (juce::ColourGradient (hot.brighter (over ? 0.15f : 0.f), body.getX(), body.getY(),
                                                 pink.brighter (over ? 0.15f : 0.f), body.getRight(), body.getBottom(), false));
        g.fillRoundedRectangle (body, 8.f);
        g.setColour (juce::Colours::white.withAlpha (0.28f));
        g.fillRoundedRectangle (body.reduced (4.f).withHeight (body.getHeight() * 0.38f), 6.f);
        g.setColour (juce::Colours::white.withAlpha (0.55f + 0.35f * pulse));
        g.drawRoundedRectangle (body.reduced (0.5f), 8.f, 1.4f);

        // dice
        const float ds = juce::jmin (body.getHeight() - 14.f, 24.f);
        auto die = juce::Rectangle<float> (body.getX() + 10.f, body.getCentreY() - ds * 0.5f, ds, ds);
        g.setColour (juce::Colour (0xff17040f));
        g.fillRoundedRectangle (die, 4.f);
        g.setColour (hot);
        const float p = ds * 0.13f;
        for (auto [fx, fy] : { std::pair { 0.27f, 0.27f }, { 0.73f, 0.27f }, { 0.5f, 0.5f }, { 0.27f, 0.73f }, { 0.73f, 0.73f } })
            g.fillEllipse (die.getX() + fx * ds - p, die.getY() + fy * ds - p, p * 2.f, p * 2.f);

        g.setColour (juce::Colour (0xff17040f));
        g.setFont (Acid::font (16.f).withExtraKerningFactor (0.08f));
        g.drawText ("RANDOM", body.withTrimmedLeft (ds + 14.f).reduced (4.f, 0.f), juce::Justification::centred);
    }

private:
    void timerCallback() override { phase += 0.12f; repaint(); }
    float phase = 0.f;
};

//==============================================================================
// What a macro is routed to in the matrix, shown under its knob
class MacroInfo : public juce::Component
{
public:
    MacroInfo (AcidanceProcessor& p, int m) : proc (p), macro (m) {}
    void paint (juce::Graphics& g) override
    {
        juce::StringArray to;
        for (int k = 1; k <= kNumModSlots; ++k)
        {
            const juce::String id = "mod" + juce::String (k) + "_";
            if ((int) proc.apvts.getRawParameterValue (id + "src")->load() == S_Macro1 + macro
                && std::abs (proc.apvts.getRawParameterValue (id + "amt")->load()) > 0.001f)
                to.addIfNotAlreadyThere (Lists::modDests[(int) proc.apvts.getRawParameterValue (id + "dst")->load()].toUpperCase());
        }
        g.setColour (to.isEmpty() ? Acid::dim.withAlpha (0.6f) : Acid::text);
        g.setFont (Acid::font (10.5f, false));
        g.drawFittedText (to.isEmpty() ? juce::String ("NOT ROUTED (use MATRIX: source MACRO " + juce::String (macro + 1) + ")")
                                       : "-> " + to.joinIntoString (", "),
                          getLocalBounds(), juce::Justification::centredTop, 3);
    }
private:
    AcidanceProcessor& proc;
    int macro;
};

// Kick duck shape preview
class DuckView : public juce::Component
{
public:
    explicit DuckView (AcidanceProcessor& p) : proc (p) {}
    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (Acid::screen);
        g.fillRoundedRectangle (r, 5.f);
        const float depth = proc.apvts.getRawParameterValue ("duck_depth")->load();
        const float rel = std::max (0.05f, proc.apvts.getRawParameterValue ("duck_rel")->load());
        const float power = 1.f + proc.apvts.getRawParameterValue ("duck_curve")->load() * 3.f;
        const bool on = proc.apvts.getRawParameterValue ("duck_on")->load() > 0.5f;
        juce::Path p;
        for (int x = 0; x <= (int) r.getWidth(); ++x)
        {
            float ph = (float) x / r.getWidth() * 2.f;   // two beats
            ph -= std::floor (ph);
            const float t = ph / rel;
            const float gn = 1.f - depth * (t >= 1.f ? 0.f : std::pow (1.f - t, power));
            const float y = r.getBottom() - 6.f - gn * (r.getHeight() - 12.f);
            if (x == 0) p.startNewSubPath (r.getX() + (float) x, y); else p.lineTo (r.getX() + (float) x, y);
        }
        Acid::glowStroke (g, p, on ? Acid::magenta : Acid::dim, 2.f);
    }
private:
    AcidanceProcessor& proc;
};

class PerformPage : public Page
{
public:
    explicit PerformPage (AcidanceProcessor& p) : proc (p)
    {
        auto& s = p.apvts;
        macros = add (new Panel ("MACROS", Acid::yellow, s));
        for (int m = 0; m < kNumMacros; ++m)
        {
            macroKnobs[(size_t) m] = macros->knob ("macro" + juce::String (m + 1), "MACRO " + juce::String (m + 1));
            macroInfo[(size_t) m] = macros->add (new MacroInfo (p, m));
            live.add (macroInfo[(size_t) m]);
        }

        morph = add (new Panel ("A / B MORPH", Acid::cyan, s));
        morphOn = morph->toggle ("morph_on", "MORPH ON");
        storeA = morph->add (new juce::TextButton ("STORE A"));
        storeB = morph->add (new juce::TextButton ("STORE B"));
        for (auto* b : { storeA, storeB }) b->setColour (juce::TextButton::buttonOnColourId, Acid::cyan);
        storeA->onClick = [this] { proc.storeMorph (0); repaint(); };
        storeB->onClick = [this] { proc.storeMorph (1); repaint(); };
        morphKnob = morph->knob ("morph", "MORPH  A <-> B");

        duck = add (new Panel ("KICK DUCK", Acid::magenta, s));
        duckOn = duck->toggle ("duck_on", "ON");
        duckDiv = duck->combo ("duck_div", Lists::divisions);
        duckKnobs = { duck->knob ("duck_depth", "DEPTH"), duck->knob ("duck_rel", "RELEASE"), duck->knob ("duck_curve", "CURVE") };
        duckView = duck->add (new DuckView (p));
        live.add (duckView);

        analog = add (new Panel ("ANALOG VOICE CARDS", Acid::green, s));
        analogKnobs = { analog->knob ("spread", "VOICE SPREAD"), analog->knob ("drift", "ANALOG DRIFT"), analog->knob ("warmth", "WARMTH") };
        live.add (this);
    }

    void resized() override
    {
        const int w = (getWidth() - 16) / 2, h = (getHeight() - 16) / 2;
        macros->setBounds (0, 0, w, h);
        morph->setBounds (w + 16, 0, w, h);
        duck->setBounds (0, h + 16, w, h);
        analog->setBounds (w + 16, h + 16, w, h);
        const int kw = (w - 40) / 4;
        for (int m = 0; m < kNumMacros; ++m)
        {
            macroKnobs[(size_t) m]->setBounds (20 + m * kw, 56, kw, 150);
            macroInfo[(size_t) m]->setBounds (20 + m * kw + 4, 212, kw - 8, 90);
        }
        morphOn->setBounds (20, 56, 130, 34);
        storeA->setBounds (20, 110, 130, 40);
        storeB->setBounds (20, 160, 130, 40);
        morphKnob->setBounds (200, 50, 200, 230);
        duckOn->setBounds (w - 64, 8, 52, 26);
        duckDiv->setBounds (20, 50, 120, 30);
        Panel::row ({ duckKnobs[0], duckKnobs[1], duckKnobs[2] }, 20, 96, 96, 120, 8);
        duckView->setBounds (330, 56, w - 350, h - 76);
        Panel::row ({ analogKnobs[0], analogKnobs[1], analogKnobs[2] }, 40, 70, 130, 160, 30);
    }

    void paintOverChildren (juce::Graphics& g) override
    {
        g.setFont (Acid::font (11.f, false));
        auto mb = morph->getBounds();
        g.setColour (proc.hasMorph (0) ? Acid::cyan : Acid::dim);
        g.drawText (proc.hasMorph (0) ? "A stored" : "A empty", mb.getX() + 20, mb.getY() + 204, 130, 16, juce::Justification::centred);
        g.setColour (proc.hasMorph (1) ? Acid::cyan : Acid::dim);
        g.drawText (proc.hasMorph (1) ? "B stored" : "B empty", mb.getX() + 20, mb.getY() + 220, 130, 16, juce::Justification::centred);
        g.setColour (Acid::dim);
        g.drawFittedText ("STORE A, change the sound, STORE B, then MORPH ON. The MORPH knob blends every sound setting "
                          "between the two (automate it for builds and drops). Your knobs keep their own values.",
                          mb.getX() + 420, mb.getY() + 60, mb.getWidth() - 440, 160, juce::Justification::topLeft, 8);
        auto db = duck->getBounds();
        g.drawFittedText ("A sidechain dip on every beat of the RATE, locked to the song. Leaves room for the kick.",
                          db.getX() + 20, db.getBottom() - 54, 300, 40, juce::Justification::topLeft, 3);
        auto ab = analog->getBounds();
        g.drawFittedText ("VOICE SPREAD gives each of the 16 voices its own small differences in cutoff, envelope speed, "
                          "tuning and pan, like the voice cards of an analog poly. DRIFT lets every note wander slightly.",
                          ab.getX() + 40, ab.getY() + 250, ab.getWidth() - 80, 80, juce::Justification::topLeft, 4);
    }

private:
    template <class T> T* add (T* c) { owned.add (c); addAndMakeVisible (c); return c; }
    AcidanceProcessor& proc;
    Panel* macros, *morph, *duck, *analog;
    std::array<Knob*, kNumMacros> macroKnobs {};
    std::array<MacroInfo*, kNumMacros> macroInfo {};
    juce::TextButton* morphOn, *storeA, *storeB, *duckOn;
    Knob* morphKnob;
    juce::ComboBox* duckDiv;
    std::array<Knob*, 3> duckKnobs {}, analogKnobs {};
    DuckView* duckView;
    juce::OwnedArray<juce::Component> owned;
};

//==============================================================================
class PresetBar : public juce::Component, private juce::Timer
{
public:
    PresetBar (AcidanceProcessor& p, std::function<void()> openBrowser) : proc (p), onOpen (std::move (openBrowser))
    {
        for (auto* b : { &prev, &next })
        {
            b->setColour (juce::TextButton::buttonOnColourId, Acid::violet);
            addAndMakeVisible (b);
        }
        prev.onClick = [this] { step (-1); };
        next.onClick = [this] { step (1); };
        rnd.setTooltip ("New random sound on every press: oscillators, filters, envelopes, LFOs, effects and arp, level-matched");
        rnd.onClick = [this] { proc.randomizeSound(); repaint(); };
        addAndMakeVisible (rnd);
        startTimerHz (10);
    }

    void resized() override
    {
        prev.setBounds (0, 0, 36, getHeight());
        rnd.setBounds (38, 0, 136, getHeight());
        next.setBounds (getWidth() - 36, 0, 36, getHeight());
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().withTrimmedLeft (180).withTrimmedRight (40).toFloat();
        g.setColour (Acid::screen);
        g.fillRoundedRectangle (r, 5.f);
        g.setColour (Acid::accentFor (Acid::violet).withAlpha (0.7f));
        g.drawRoundedRectangle (r.reduced (0.5f), 5.f, 1.f);
        g.setColour (Acid::screenDim);
        g.setFont (Acid::font (10.f));
        const bool user = proc.currentPresetFolder == "User";
        g.drawText (proc.currentPresetFolder.toUpperCase()
                        + (user ? juce::String() : "   " + juce::String (proc.getCurrentProgram() + 1) + " / " + juce::String (Presets::count())),
                    r.reduced (10.f, 3.f), juce::Justification::topLeft);
        g.drawText ("BROWSE", r.reduced (10.f, 3.f), juce::Justification::topRight);
        if (Acid::skin == Acid::Hyper)
        {
            g.setColour (Acid::green.withAlpha (0.35f));
            g.setFont (Acid::font (19.f));
            g.drawText (proc.currentPresetName.toUpperCase(), r.reduced (10.f, 2.f).translated (0.f, 0.5f), juce::Justification::centredBottom);
            g.setColour (juce::Colour (0xffff4a3a));
            g.drawText (proc.currentPresetName.toUpperCase(), r.reduced (10.f, 2.f), juce::Justification::centredBottom);
            g.setColour (juce::Colours::black.withAlpha (0.35f));
            for (float y = r.getY(); y < r.getBottom(); y += 2.f) g.drawHorizontalLine ((int) y, r.getX(), r.getRight());
            for (float x = r.getX(); x < r.getRight(); x += 2.f) g.drawVerticalLine ((int) x, r.getY(), r.getBottom());
            return;
        }
        g.setColour (Acid::skin == Acid::Chrome ? juce::Colour (0xffeef6ff) : Acid::text);
        g.setFont (Acid::font (17.f));
        g.drawText (proc.currentPresetName, r.reduced (10.f, 2.f), juce::Justification::centredBottom);
    }

    void mouseDown (const juce::MouseEvent&) override { if (onOpen) onOpen(); }

private:
    void step (int d)
    {
        const int n = Presets::count();
        const int cur = proc.currentPresetFolder == "User" ? 0 : proc.getCurrentProgram();
        proc.setCurrentProgram ((cur + d + n) % n);
        repaint();
    }

    void timerCallback() override
    {
        const int c = proc.programChanges.load();
        if (c != lastChange) { lastChange = c; repaint(); }
    }

    AcidanceProcessor& proc;
    std::function<void()> onOpen;
    juce::TextButton prev { "<" }, next { ">" };
    RandomButton rnd;
    int lastChange = -1;
};

//==============================================================================
// Tag-based browser over the 1000 factory presets plus the user's saved presets
class PresetBrowser : public juce::Component, private juce::ListBoxModel
{
public:
    PresetBrowser (AcidanceProcessor& p, std::function<void()> closeFn) : proc (p), onClose (std::move (closeFn))
    {
        search.setTextToShowWhenEmpty ("Search name or tag...", Acid::screenDim);
        search.setColour (juce::TextEditor::backgroundColourId, Acid::screen);
        search.setColour (juce::TextEditor::textColourId, juce::Colour (0xffeef6ff));
        search.setColour (juce::TextEditor::outlineColourId, Acid::line);
        search.setFont (Acid::font (15.f, false));
        search.onTextChange = [this] { applyFilter(); };
        addAndMakeVisible (search);

        for (auto* b : { &saveButton, &closeButton })
        {
            b->setColour (juce::TextButton::buttonOnColourId, Acid::accentFor (Acid::green));
            addAndMakeVisible (b);
        }
        saveButton.onClick = [this] { askToSave(); };
        closeButton.onClick = [this] { if (onClose) onClose(); };

        auto addFolder = [this] (const juce::String& label, const juce::String& key)
        {
            auto* b = folderButtons.add (new juce::TextButton (label));
            b->setRadioGroupId (2001);
            b->setClickingTogglesState (true);
            b->setColour (juce::TextButton::buttonOnColourId, Acid::accentFor (Acid::cyan));
            b->getProperties().set ("folder", key);
            b->onClick = [this, key] { folder = key; applyFilter(); };
            addAndMakeVisible (b);
        };
        addFolder ("ALL", {});
        for (auto& c : Presets::categories()) addFolder (c.toUpperCase(), c);
        addFolder ("USER", "User");
        folderButtons[0]->setToggleState (true, juce::dontSendNotification);

        const auto& tags = Presets::allTags();
        for (int i = 0; i < std::min (30, tags.size()); ++i)
        {
            auto* b = tagButtons.add (new juce::TextButton (tags[i].toUpperCase()));
            b->setClickingTogglesState (true);
            b->setColour (juce::TextButton::buttonOnColourId, Acid::accentFor (Acid::magenta));
            b->getProperties().set ("tag", tags[i]);
            b->onClick = [this] { applyFilter(); };
            addAndMakeVisible (b);
        }

        list.setModel (this);
        list.setRowHeight (30);
        list.setColour (juce::ListBox::backgroundColourId, Acid::screen);
        list.setColour (juce::ListBox::outlineColourId, Acid::line);
        list.setOutlineThickness (1);
        addAndMakeVisible (list);
        refresh();
    }

    void refresh()
    {
        all.clear();
        for (int i = 0; i < Presets::count(); ++i)
            all.push_back ({ false, i, {}, Presets::name (i), Presets::category (i), Presets::tags (i) });
        for (auto& u : Presets::userPresets())
            all.push_back ({ true, -1, u.file, u.name, "User", u.tags });
        applyFilter();
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        if (Acid::skin == Acid::Chrome)
        {
            if (metal.getWidth() != getWidth()) metal = Acid::brushedMetal (getWidth(), getHeight());
            g.drawImageAt (metal, 0, 0);
        }
        else
        {
            g.setColour (Acid::panel);
            g.fillRect (r);
        }
        g.setColour (Acid::line);
        g.drawRect (r, 1.f);
        g.setColour (Acid::text);
        g.setFont (Acid::font (18.f).withExtraKerningFactor (0.1f));
        g.drawText ("PRESET BROWSER", 20, 12, 300, 30, juce::Justification::centredLeft);
        g.setColour (Acid::dim);
        g.setFont (Acid::font (12.f, false));
        g.drawText (juce::String (shown.size()) + " presets", 20, 40, 200, 18, juce::Justification::centredLeft);
        g.setFont (Acid::font (11.f));
        g.drawText ("FOLDERS", 20, 64, 200, 16, juce::Justification::centredLeft);
        g.drawText ("TAGS (all selected must match)", 236, 64, 400, 16, juce::Justification::centredLeft);
    }

    void resized() override
    {
        search.setBounds (236, 14, getWidth() - 236 - 280, 34);
        saveButton.setBounds (getWidth() - 270, 14, 120, 34);
        closeButton.setBounds (getWidth() - 140, 14, 120, 34);
        int y = 84;
        for (auto* b : folderButtons) { b->setBounds (16, y, 204, 30); y += 34; }

        const int tagW = 112, tagH = 26, perRow = std::max (1, (getWidth() - 252) / (tagW + 6));
        for (int i = 0; i < tagButtons.size(); ++i)
            tagButtons[i]->setBounds (236 + (i % perRow) * (tagW + 6), 84 + (i / perRow) * (tagH + 6), tagW, tagH);
        const int rows = (tagButtons.size() + perRow - 1) / perRow;
        const int listTop = 84 + rows * (tagH + 6) + 8;
        list.setBounds (236, listTop, getWidth() - 252, getHeight() - listTop - 16);
    }

private:
    struct Entry { bool user; int index; juce::File file; juce::String name, folder; juce::StringArray tags; };

    void applyFilter()
    {
        juce::StringArray needTags;
        for (auto* b : tagButtons)
            if (b->getToggleState()) needTags.add (b->getProperties()["tag"].toString());
        const auto text = search.getText().trim().toLowerCase();

        shown.clear();
        for (int i = 0; i < (int) all.size(); ++i)
        {
            const auto& e = all[(size_t) i];
            if (folder.isNotEmpty() && e.folder != folder) continue;
            bool ok = true;
            for (auto& t : needTags) if (! e.tags.contains (t)) { ok = false; break; }
            if (! ok) continue;
            if (text.isNotEmpty() && ! e.name.toLowerCase().contains (text) && ! e.folder.toLowerCase().contains (text)
                && ! e.tags.joinIntoString (" ").toLowerCase().contains (text))
                continue;
            shown.add (i);
        }
        list.updateContent();
        list.repaint();
        repaint (0, 36, 230, 24);
    }

    int getNumRows() override { return shown.size(); }

    void paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool selected) override
    {
        if (! juce::isPositiveAndBelow (row, shown.size())) return;
        const auto& e = all[(size_t) shown[row]];
        const bool current = e.user ? proc.currentPresetName == e.name && proc.currentPresetFolder == "User"
                                    : proc.currentPresetFolder != "User" && proc.getCurrentProgram() == e.index;
        const auto accent = Acid::accentFor (Acid::cyan);
        if (selected || current)
        {
            g.setColour (accent.withAlpha (current ? 0.28f : 0.14f));
            g.fillRect (0, 0, w, h);
        }
        g.setColour (current ? accent : juce::Colour (0xffeef6ff));
        g.setFont (Acid::font (14.f));
        g.drawText (e.name, 12, 0, 240, h, juce::Justification::centredLeft);
        g.setColour (Acid::screenDim);
        g.setFont (Acid::font (11.f));
        g.drawText (e.folder.toUpperCase(), 260, 0, 140, h, juce::Justification::centredLeft);
        g.setFont (Acid::font (11.f, false));
        g.drawText (e.tags.joinIntoString ("  /  "), 410, 0, w - 420, h, juce::Justification::centredLeft);
        g.setColour (juce::Colours::white.withAlpha (0.05f));
        g.drawHorizontalLine (h - 1, 0.f, (float) w);
    }

    void listBoxItemClicked (int row, const juce::MouseEvent&) override { load (row); }
    void returnKeyPressed (int row) override { load (row); }

    void load (int row)
    {
        if (! juce::isPositiveAndBelow (row, shown.size())) return;
        const auto& e = all[(size_t) shown[row]];
        if (e.user) Presets::loadUser (proc, e.file);
        else proc.setCurrentProgram (e.index);
        list.repaint();
    }

    void askToSave()
    {
        auto* w = new juce::AlertWindow ("Save preset", "Saved to Documents/SKAZI/Presets", juce::MessageBoxIconType::NoIcon);
        w->addTextEditor ("name", proc.currentPresetName, "Name:");
        w->addTextEditor ("tags", "", "Tags (comma separated):");
        w->addButton ("SAVE", 1, juce::KeyPress (juce::KeyPress::returnKey));
        w->addButton ("CANCEL", 0, juce::KeyPress (juce::KeyPress::escapeKey));
        juce::Component::SafePointer<PresetBrowser> safe (this);
        w->enterModalState (true, juce::ModalCallbackFunction::create ([safe, w] (int result)
        {
            if (result != 1 || safe == nullptr) return;
            auto tags = juce::StringArray::fromTokens (w->getTextEditorContents ("tags"), ",", "");
            tags.trim();
            tags.removeEmptyStrings();
            if (Presets::saveUser (safe->proc, w->getTextEditorContents ("name"), tags))
                safe->refresh();
        }), true);
    }

    AcidanceProcessor& proc;
    std::function<void()> onClose;
    juce::TextEditor search;
    juce::TextButton saveButton { "SAVE AS..." }, closeButton { "CLOSE" };
    juce::OwnedArray<juce::TextButton> folderButtons, tagButtons;
    juce::ListBox list;
    std::vector<Entry> all;
    juce::Array<int> shown;
    juce::String folder;
    juce::Image metal;
};

//==============================================================================
// Centre console: oscilloscope with light trails, 3D wavetable view and spectrum
class Console : public juce::Component, private juce::Timer
{
public:
    explicit Console (AcidanceProcessor& p) : proc (p), fft (11)
    {
        spectrum.fill (0.f);
        startTimerHz (60);
    }

    void resized() override
    {
        const int w = getWidth() - 24, h = getHeight() - 20;
        scopeArea = juce::Rectangle<int> (12, 10, w * 32 / 100, h);
        wtArea = juce::Rectangle<int> (scopeArea.getRight() + 8, 10, w * 34 / 100, h);
        fftArea = juce::Rectangle<int> (wtArea.getRight() + 8, 10, getWidth() - 12 - wtArea.getRight() - 8, h);
        trail = juce::Image (juce::Image::RGB, std::max (1, scopeArea.getWidth()), std::max (1, scopeArea.getHeight()), true);
        juce::Graphics (trail).fillAll (Acid::screen);
        bezel = {};
    }

    void paint (juce::Graphics& g) override
    {
        // Bezel
        auto r = getLocalBounds().toFloat();
        if (Acid::skin == Acid::Hyper)
        {
            g.setColour (juce::Colour (0xff222226));
            g.fillRoundedRectangle (r, 5.f);
            g.setColour (juce::Colours::black);
            g.drawRoundedRectangle (r.reduced (0.5f), 5.f, 1.2f);
        }
        else
        if (Acid::skin == Acid::Robot)
        {
            auto p = Acid::chamfer (r.reduced (0.5f), 14.f);
            g.setColour (juce::Colour (0xff121820));
            g.fillPath (p);
            g.setColour (Acid::line);
            g.strokePath (p, juce::PathStrokeType (1.2f));
        }
        else
        if (Acid::skin == Acid::Chrome)
        {
            if (bezel.getWidth() != getWidth())
                bezel = Acid::brushedMetal (getWidth(), getHeight(), juce::Colour (0xff9c9fa5), juce::Colour (0xff6d7077));
            g.drawImageAt (bezel, 0, 0);
            g.setColour (juce::Colour (0xff3d3f45));
            g.drawRoundedRectangle (r.reduced (0.5f), 6.f, 1.2f);
        }
        else
        {
            g.setColour (Acid::panel);
            g.fillRoundedRectangle (r, 6.f);
            g.setColour (Acid::line);
            g.drawRoundedRectangle (r.reduced (0.5f), 6.f, 1.f);
        }
        for (auto a : { scopeArea, wtArea, fftArea })
        {
            g.setColour (Acid::screen);
            g.fillRoundedRectangle (a.toFloat(), 4.f);
        }

        g.drawImageAt (trail, scopeArea.getX(), scopeArea.getY());
        drawWavetable (g);
        drawSpectrum (g);

        g.setFont (Acid::font (10.f));
        g.setColour (Acid::screenDim);
        g.drawText ("SCOPE", scopeArea.reduced (8, 5), juce::Justification::topLeft);
        g.drawText ("WAVETABLE  3D", wtArea.reduced (8, 5), juce::Justification::topLeft);
        g.drawText ("SPECTRUM", fftArea.reduced (8, 5), juce::Justification::topLeft);
        if (Acid::skin == Acid::Hyper)
        {
            for (auto a : { scopeArea, wtArea, fftArea })
            {
                g.setColour (juce::Colours::black.withAlpha (0.3f));
                for (int y = a.getY(); y < a.getBottom(); y += 3) g.drawHorizontalLine (y, (float) a.getX(), (float) a.getRight());
                for (int x = a.getX(); x < a.getRight(); x += 3) g.drawVerticalLine (x, (float) a.getY(), (float) a.getBottom());
                g.setColour (juce::Colours::black);
                g.drawRoundedRectangle (a.toFloat(), 4.f, 2.f);
            }
            return;
        }
        if (Acid::skin == Acid::Robot)
        {
            for (auto a : { scopeArea, wtArea, fftArea })
            {
                g.setColour (juce::Colours::black.withAlpha (0.22f));
                for (int y = a.getY(); y < a.getBottom(); y += 3)
                    g.drawHorizontalLine (y, (float) a.getX(), (float) a.getRight());
                g.setColour (Acid::green);
                const float x0 = (float) a.getX(), y0 = (float) a.getY(), x1 = (float) a.getRight(), y1 = (float) a.getBottom();
                for (auto seg : { juce::Line<float> (x0, y0, x0 + 12.f, y0), juce::Line<float> (x0, y0, x0, y0 + 12.f),
                                  juce::Line<float> (x1, y1, x1 - 12.f, y1), juce::Line<float> (x1, y1, x1, y1 - 12.f) })
                    g.drawLine (seg, 2.f);
            }
            return;
        }
        // Glass glare
        for (auto a : { scopeArea, wtArea, fftArea })
        {
            g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.06f), 0.f, (float) a.getY(),
                                                     juce::Colours::transparentWhite, 0.f, (float) a.getCentreY(), false));
            g.fillRoundedRectangle (a.toFloat(), 4.f);
        }
    }

private:
    void timerCallback() override
    {
        // Pull the newest samples from the processor's ring buffer
        const int size = AcidanceProcessor::kScopeSize;
        const int w = proc.scopeWrite.load (std::memory_order_acquire);
        for (int i = 0; i < N; ++i)
            samples[(size_t) i] = proc.scope[(size_t) ((w - N + i + size) % size)].load (std::memory_order_relaxed);

        double sum = 0.0;
        for (int i = N - 1024; i < N; ++i) sum += samples[(size_t) i] * samples[(size_t) i];
        level = level * 0.8f + 0.2f * (float) std::sqrt (sum / 1024.0);

        updateTrail();
        updateSpectrum();
        repaint();
    }

    void updateTrail()
    {
        if (! trail.isValid()) return;
        juce::Graphics tg (trail);
        tg.setColour (Acid::screen.withAlpha (0.28f));   // fade older sweeps -> light trails
        tg.fillAll();
        tg.setColour (juce::Colours::white.withAlpha (0.05f));
        tg.drawHorizontalLine (trail.getHeight() / 2, 0.f, (float) trail.getWidth());

        // Trigger on a rising zero crossing for a steady picture
        const int span = 600;
        int start = N - span - 1;
        for (int i = N - span - 1; i > N - span - 1200 && i > 1; --i)
            if (samples[(size_t) i - 1] < 0.f && samples[(size_t) i] >= 0.f) { start = i; break; }

        juce::Path p;
        const float tw = (float) trail.getWidth(), th = (float) trail.getHeight();
        for (int i = 0; i < span; ++i)
        {
            const float x = tw * (float) i / (float) (span - 1);
            const float y = th * 0.5f - juce::jlimit (-1.f, 1.f, samples[(size_t) (start + i)] * 1.8f) * th * 0.42f;
            if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
        }
        tg.setColour (Acid::accentFor (Acid::green).withAlpha (0.35f));
        tg.strokePath (p, juce::PathStrokeType (4.f));
        tg.setColour (Acid::accentFor (Acid::green));
        tg.strokePath (p, juce::PathStrokeType (1.6f));
    }

    void updateSpectrum()
    {
        std::array<float, 2 * N> data {};
        for (int i = 0; i < N; ++i)
        {
            const float win = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * (float) i / (float) (N - 1));
            data[(size_t) i] = samples[(size_t) i] * win;
        }
        fft.performFrequencyOnlyForwardTransform (data.data(), true);
        const float sr = (float) std::max (8000.0, proc.getSampleRate());
        for (int b = 0; b < bands; ++b)
        {
            const float f0 = 30.f * std::pow (20000.f / 30.f, (float) b / bands);
            const float f1 = 30.f * std::pow (20000.f / 30.f, (float) (b + 1) / bands);
            const int i0 = juce::jlimit (1, N / 2 - 1, (int) (f0 / sr * N));
            const int i1 = juce::jlimit (i0 + 1, N / 2, (int) (f1 / sr * N));
            float m = 0.f;
            for (int i = i0; i < i1; ++i) m = std::max (m, data[(size_t) i]);
            const float db = juce::Decibels::gainToDecibels (m / (N * 0.25f), -90.f);
            const float v = juce::jlimit (0.f, 1.f, (db + 90.f) / 90.f);
            spectrum[(size_t) b] = std::max (v, spectrum[(size_t) b] * 0.9f);   // falling light trail
            peaks[(size_t) b] = std::max (v, peaks[(size_t) b] - 0.006f);
        }
    }

    void drawSpectrum (juce::Graphics& g)
    {
        auto a = fftArea.toFloat().reduced (6.f, 18.f).withTrimmedTop (2.f);
        juce::Path fill, line, peak;
        fill.startNewSubPath (a.getX(), a.getBottom());
        for (int b = 0; b < bands; ++b)
        {
            const float x = a.getX() + a.getWidth() * (float) b / (float) (bands - 1);
            const float y = a.getBottom() - spectrum[(size_t) b] * a.getHeight();
            const float py = a.getBottom() - peaks[(size_t) b] * a.getHeight();
            fill.lineTo (x, y);
            if (b == 0) { line.startNewSubPath (x, y); peak.startNewSubPath (x, py); }
            else { line.lineTo (x, y); peak.lineTo (x, py); }
        }
        fill.lineTo (a.getRight(), a.getBottom());
        fill.closeSubPath();
        const auto c1 = Acid::accentFor (Acid::magenta), c2 = Acid::accentFor (Acid::green);
        g.setGradientFill (juce::ColourGradient (c1.withAlpha (0.45f), a.getX(), 0.f, c2.withAlpha (0.45f), a.getRight(), 0.f, false));
        g.fillPath (fill);
        g.setColour (c2);
        g.strokePath (line, juce::PathStrokeType (1.5f));
        g.setColour (juce::Colours::white.withAlpha (0.35f));
        g.strokePath (peak, juce::PathStrokeType (1.f));
    }

    void drawWavetable (juce::Graphics& g)
    {
        const int wave = (int) proc.apvts.getRawParameterValue ("osc1_wave")->load();
        const float pos = proc.apvts.getRawParameterValue ("osc1_wtpos")->load();
        auto a = wtArea.toFloat().reduced (10.f, 18.f);
        const int frames = 16, pts = 96;
        const float fw = a.getWidth() * 0.62f, fh = a.getHeight() * 0.22f;
        const float dx = (a.getWidth() - fw) / (frames - 1), dy = (a.getHeight() - fh * 2.2f) / (frames - 1);
        const int active = juce::roundToInt (pos * (frames - 1));
        const auto c1 = Acid::accentFor (Acid::violet), c2 = Acid::accentFor (Acid::green);

        auto sampleAt = [wave] (int f, float ph)
        {
            if (wave >= 4)
                return Wavetables::get().frame (wave - 4, (float) f / 15.f)[(int) (ph * (Wavetables::N - 1))];
            switch (wave)
            {
                case 0: return std::sin (6.2831853f * ph);
                case 1: return 2.f * ph - 1.f;
                case 2: return ph < 0.5f ? 1.f : -1.f;
                default: return 1.f - 4.f * std::abs (ph - 0.5f);
            }
        };

        for (int f = frames - 1; f >= 0; --f)   // back to front
        {
            const float ox = a.getX() + dx * (float) f;
            const float oy = a.getBottom() - fh * 1.1f - dy * (float) f;
            juce::Path p;
            for (int i = 0; i < pts; ++i)
            {
                const float ph = (float) i / (float) (pts - 1);
                const float x = ox + fw * ph;
                const float y = oy - sampleAt (f, ph) * fh;
                if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
            }
            if (f == active)
            {
                const float bright = juce::jlimit (0.6f, 1.f, 0.6f + level * 4.f);
                g.setColour (c2.withAlpha (0.25f * bright));
                g.strokePath (p, juce::PathStrokeType (6.f));
                g.setColour (c2.withMultipliedBrightness (bright));
                g.strokePath (p, juce::PathStrokeType (2.f));
            }
            else
            {
                const float depth = 1.f - (float) f / (float) frames;
                g.setColour (c1.withAlpha (0.18f + 0.35f * depth));
                g.strokePath (p, juce::PathStrokeType (1.f));
            }
        }
        g.setColour (Acid::screenDim);
        g.setFont (Acid::font (10.f));
        g.drawText ("OSC 1  " + Lists::oscWaves[wave].toUpperCase(), wtArea.reduced (8, 5), juce::Justification::topRight);
    }

    static constexpr int N = 2048, bands = 96;
    AcidanceProcessor& proc;
    juce::dsp::FFT fft;
    std::array<float, N> samples {};
    std::array<float, bands> spectrum {}, peaks {};
    float level = 0.f;
    juce::Rectangle<int> scopeArea, wtArea, fftArea;
    juce::Image trail, bezel;
};

//==============================================================================
// Hardware-style neon red kill switch
class PanicButton : public juce::Component, public juce::SettableTooltipClient, private juce::Timer
{
public:
    explicit PanicButton (AcidanceProcessor& p) : proc (p)
    {
        setTooltip ("PANIC: all notes off, reset controllers, flush delay / reverb / filters");
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (4.f);
        const auto red = Acid::panicRed;
        juce::Path p;
        p.addRoundedRectangle (r, 5.f);
        juce::DropShadow (red.withAlpha (0.55f + 0.45f * flash), 14 + (int) (flash * 12.f), {}).drawForPath (g, p);
        g.setGradientFill (juce::ColourGradient (red.brighter (0.25f + flash * 0.6f), r.getX(), r.getY(),
                                                 red.darker (0.45f - flash * 0.3f), r.getX(), r.getBottom(), false));
        g.fillPath (p);
        g.setColour (juce::Colours::white.withAlpha (0.35f));
        g.drawHorizontalLine ((int) r.getY() + 2, r.getX() + 4.f, r.getRight() - 4.f);
        g.setColour (juce::Colours::black.withAlpha (0.6f));
        g.strokePath (p, juce::PathStrokeType (1.2f));
        g.setColour (juce::Colours::white);
        g.setFont (Acid::font (15.f).withExtraKerningFactor (0.15f));
        g.drawText ("PANIC", r, juce::Justification::centred);
    }

    void mouseDown (const juce::MouseEvent&) override
    {
        proc.panicRequest = true;
        flash = 1.f;
        startTimerHz (30);
        repaint();
    }

private:
    void timerCallback() override
    {
        flash *= 0.85f;
        if (flash < 0.02f) { flash = 0.f; stopTimer(); }
        repaint();
    }

    AcidanceProcessor& proc;
    float flash = 0.f;
};

//==============================================================================
// Hardware-style wheel for the left rail. PITCH springs back to the middle; both follow
// incoming MIDI wheels too.
class PerfWheel : public juce::Component, public juce::SettableTooltipClient
{
public:
    PerfWheel (juce::String label, bool bipolar, std::function<float()> current, std::function<void (float)> moved)
        : name (std::move (label)), centred (bipolar), read (std::move (current)), write (std::move (moved)) {}

    void refresh()
    {
        if (dragging) return;
        const float v = read();
        if (std::abs (v - value) > 0.002f) { value = v; repaint(); }
    }

    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat();
        g.setColour (Acid::screenDim);
        g.setFont (Acid::font (11.f));
        g.drawText (name, b.removeFromTop (16.f), juce::Justification::centred);
        g.setColour (juce::Colours::black);
        g.fillRoundedRectangle (b, 5.f);
        auto w = b.reduced (6.f, 4.f);
        g.setColour (juce::Colour (0xff0a0a0c));
        g.fillRoundedRectangle (w, 4.f);
        juce::ColourGradient shine (juce::Colour (0x00ffffff), w.getX(), w.getY(), juce::Colour (0x00ffffff), w.getX(), w.getBottom(), false);
        shine.addColour (0.5, juce::Colour (0x38ffffff));
        g.setGradientFill (shine);
        g.fillRoundedRectangle (w, 4.f);
        const float roll = (centred ? value : value * 2.f - 1.f) * 0.5f;
        for (int i = -10; i <= 10; ++i)
        {
            const float a = (i / 10.f + roll) * juce::MathConstants<float>::halfPi;
            if (std::abs (a) >= juce::MathConstants<float>::halfPi) continue;
            g.setColour (juce::Colours::white.withAlpha (0.12f * std::cos (a)));
            g.drawHorizontalLine ((int) (w.getCentreY() + std::sin (a) * w.getHeight() * 0.5f), w.getX() + 3, w.getRight() - 3);
        }
        const float pos = centred ? 0.5f - value * 0.5f : 1.f - value;
        g.setColour (Acid::accentFor (Acid::violet));
        g.fillRect (w.getX() + 1, w.getY() + pos * w.getHeight() - 2.5f, w.getWidth() - 2, 5.f);
        if (centred)
        {
            g.setColour (juce::Colours::white.withAlpha (0.35f));
            g.drawHorizontalLine ((int) w.getCentreY(), b.getX(), w.getX());
        }
    }

    void mouseDown (const juce::MouseEvent& e) override { dragging = true; mouseDrag (e); }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        auto w = getLocalBounds().toFloat().withTrimmedTop (16.f).reduced (6.f, 4.f);
        const float t = juce::jlimit (0.f, 1.f, (e.position.y - w.getY()) / w.getHeight());
        value = centred ? (0.5f - t) * 2.f : 1.f - t;
        write (value);
        repaint();
    }
    void mouseUp (const juce::MouseEvent&) override
    {
        dragging = false;
        if (centred) { value = 0.f; write (0.f); repaint(); }
    }

private:
    juce::String name;
    bool centred, dragging = false;
    float value = 0.f;
    std::function<float()> read;
    std::function<void (float)> write;
};

// Everything inside the window, laid out at a fixed 1200 x 1000 and scaled by the editor
class MainView : public juce::Component, private juce::Timer
{
public:
    MainView (AcidanceProcessor& p, AcidLookAndFeel& l) : proc (p), lnf (l)
    {
        runButton.setClickingTogglesState (true);
        addAndMakeVisible (runButton);
        runAttachment = std::make_unique<APVTS::ButtonAttachment> (proc.apvts, "run", runButton);

        skinButton.setTooltip ("Switch skin: Hyper / Robot / Chrome / Vintage / Neon");
        skinButton.onClick = [this]
        {
            juce::Component::SafePointer<MainView> safe (this);
            juce::MessageManager::callAsync ([safe] { if (safe != nullptr) safe->switchSkin(); });
        };
        addAndMakeVisible (skinButton);

        panicButton = std::make_unique<PanicButton> (proc);
        addAndMakeVisible (*panicButton);

        browseButton.onClick = [this] { openBrowser(); };
        addAndMakeVisible (browseButton);

        pitchWheel.setTooltip ("Pitch bend (springs back). Range: BEND RANGE in the OSC page");
        modWheel.setTooltip ("Mod wheel: every preset routes it to something (filter, wavetable, warp or detune) in the MATRIX");
        addAndMakeVisible (pitchWheel);
        addAndMakeVisible (modWheel);

        setSize (AcidanceEditor::baseWidth, AcidanceEditor::baseHeight);
        buildUi();
        startTimerHz (30);
    }

    ~MainView() override
    {
        for (auto* pg : pages)
            if (auto* tp = dynamic_cast<Page*> (pg)) tp->stopTimer();
        browser.reset();
        pages.clear();
    }

    void paint (juce::Graphics& g) override
    {
        const int skin = Acid::skin;
        if (skin == Acid::Bully)
        {
            paintBully (g);
            return;
        }
        if (skin == Acid::Hyper)
        {
            paintHyper (g);
            return;
        }
        if (skin == Acid::Robot)
        {
            paintRobot (g);
            return;
        }
        if (skin == Acid::Chrome)
        {
            if (backdrop.getWidth() != getWidth())
                backdrop = Acid::brushedMetal (getWidth(), getHeight(), juce::Colour (0xffd2d3d6), juce::Colour (0xffa4a7ad));
            g.drawImageAt (backdrop, 0, 0);
            // Header plate
            auto header = juce::Rectangle<float> (0.f, 0.f, (float) getWidth(), 108.f);
            g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.35f), 0.f, 0.f, juce::Colours::transparentWhite, 0.f, 108.f, false));
            g.fillRect (header);
            g.setColour (juce::Colour (0xff6f7279));
            g.drawHorizontalLine (107, 0.f, (float) getWidth());
            g.setColour (juce::Colours::white.withAlpha (0.7f));
            g.drawHorizontalLine (108, 0.f, (float) getWidth());

            // Chrome wordmark with neon underline
            float logoRight = 132.f;
            if (! drawBrandLogo (g, { 28.f, 6.f, 104.f, 56.f }))
            {
                juce::GlyphArrangement ga;
                const auto logoFont = Acid::font (44.f).withExtraKerningFactor (0.16f);
                ga.addLineOfText (logoFont, "SKAZI", 30.f, 56.f);
                juce::Path logo;
                ga.createPath (logo);
                juce::DropShadow (juce::Colours::black.withAlpha (0.45f), 4, { 0, 2 }).drawForPath (g, logo);
                juce::ColourGradient chrome (juce::Colour (0xff2b2d33), 0.f, 18.f, juce::Colour (0xff2b2d33), 0.f, 58.f, false);
                chrome.addColour (0.45, juce::Colour (0xfff4f5f7));
                chrome.addColour (0.55, juce::Colour (0xff5a5d65));
                g.setGradientFill (chrome);
                g.fillPath (logo);
                logoRight = std::max (120.f, logo.getBounds().getRight());
            }
            juce::Path neon;
            neon.startNewSubPath (32.f, 66.f);
            neon.lineTo (logoRight, 66.f);
            Acid::glowStroke (g, neon, Acid::green, 2.f);
            g.setColour (Acid::text);
            g.setFont (Acid::font (16.5f).withExtraKerningFactor (0.16f));
            g.drawText ("BY ASHER SWISSA", 32, 70, 360, 22, juce::Justification::centredLeft);
            g.setColour (Acid::dim);
            g.setFont (Acid::font (9.5f).withExtraKerningFactor (0.22f));
            g.drawText ("PSY  /  ACID  /  WAVETABLE  SYNTHESIZER", 32, 90, 360, 14, juce::Justification::centredLeft);
        }
        else if (skin == Acid::Vintage)
        {
            g.fillAll (Acid::bg);
            Acid::wood (g, { 0.f, 0.f, 12.f, (float) getHeight() });
            Acid::wood (g, { (float) getWidth() - 12.f, 0.f, 12.f, (float) getHeight() });
            auto header = juce::Rectangle<float> (12.f, 0.f, (float) getWidth() - 24.f, 108.f);
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xff22201e), 0.f, 0.f, juce::Colour (0xff121110), 0.f, 108.f, false));
            g.fillRect (header);
            g.setColour (Acid::text.withAlpha (0.3f));
            g.drawHorizontalLine (107, 12.f, (float) getWidth() - 12.f);
            for (float x : { 22.f, (float) getWidth() - 22.f })
                for (float y : { 10.f, 98.f })
                    Acid::screw (g, x, y, 3.5f);
            if (! drawBrandLogo (g, { 32.f, 6.f, 92.f, 50.f }))
            {
                g.setColour (Acid::text);
                g.setFont (Acid::font (40.f).withExtraKerningFactor (0.22f));
                g.drawText ("SKAZI", 36, 10, 380, 50, juce::Justification::centredLeft);
            }
            g.setColour (Acid::green);
            g.fillRect (38.f, 70.f, 30.f, 2.f);
            g.setColour (Acid::text.withAlpha (0.9f));
            g.setFont (Acid::font (16.5f).withExtraKerningFactor (0.2f));
            g.drawText ("BY ASHER SWISSA", 76, 59, 340, 24, juce::Justification::centredLeft);
            g.setColour (Acid::dim);
            g.setFont (Acid::font (9.5f, false).withExtraKerningFactor (0.25f));
            g.drawText ("ANALOG  WAVETABLE  SYNTHESIZER", 38, 86, 360, 14, juce::Justification::centredLeft);
        }
        else
        {
            g.fillAll (Acid::bg);
            g.setColour (Acid::panel.withAlpha (0.6f));
            for (int x = 0; x < getWidth(); x += 24) g.drawVerticalLine (x, 0.f, (float) getHeight());
            for (int y = 0; y < getHeight(); y += 24) g.drawHorizontalLine (y, 0.f, (float) getWidth());
            if (! drawBrandLogo (g, { 28.f, 6.f, 104.f, 56.f }))
            {
                const juce::Rectangle<int> logo (28, 12, 360, 56);
                g.setFont (Acid::font (50.f));
                for (int i = 4; i >= 1; --i)
                {
                    g.setColour (Acid::green.withAlpha (0.05f * (5 - i)));
                    g.drawText ("SKAZI", logo.expanded (i, i), juce::Justification::centredLeft);
                }
                g.setGradientFill (juce::ColourGradient (Acid::green, 28.f, 0.f, Acid::magenta, 330.f, 0.f, false));
                g.drawText ("SKAZI", logo, juce::Justification::centredLeft);
            }
            g.setFont (Acid::font (18.f));
            g.setColour (Acid::text);
            g.drawText ("BY ASHER SWISSA", 30, 64, 360, 24, juce::Justification::centredLeft);
            g.setColour (Acid::dim);
            g.setFont (Acid::font (10.f));
            g.drawText ("WAVETABLE  /  ACID  /  SYNTHESIZER", 30, 88, 300, 14, juce::Justification::centredLeft);
        }

        g.setColour (Acid::dim);
        g.setFont (Acid::font (10.f).withExtraKerningFactor (0.2f));
        g.drawText ("PRESET", 360, 14, 400, 14, juce::Justification::centred);
    }

    // BULLY look: a blurred rainbow swirl under a dark veil with star dust, glass header, rainbow rule
    void paintBully (juce::Graphics& g)
    {
        if (backdrop.getWidth() != getWidth() || backdrop.getHeight() != getHeight())
        {
            const int sw = 96, sh = 72;
            juce::Image small (juce::Image::RGB, sw, sh, false);
            for (int y = 0; y < sh; ++y)
                for (int x = 0; x < sw; ++x)
                {
                    const float dx = (x - sw * 0.5f) / sw, dy = (y - sh * 0.45f) / sh;
                    const float a = std::atan2 (dy, dx) / juce::MathConstants<float>::twoPi + 0.5f;
                    const float swirl = std::fmod (a + std::sqrt (dx * dx + dy * dy) * 0.9f + 1.f, 1.f);
                    small.setPixelAt (x, y, juce::Colour::fromHSV (std::fmod (0.83f + swirl, 1.f), 0.95f, 1.f, 1.f));
                }
            backdrop = juce::Image (juce::Image::RGB, getWidth(), getHeight(), false);
            juce::Graphics bg (backdrop);
            bg.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
            bg.drawImage (small, 0, 0, getWidth(), getHeight(), 0, 0, sw, sh);
            bg.setColour (juce::Colour (0xff07040f).withAlpha (0.76f));
            bg.fillAll();
            juce::Random rnd (7);
            for (int i = 0; i < 320; ++i)
            {
                bg.setColour (juce::Colours::white.withAlpha (0.05f + rnd.nextFloat() * 0.18f));
                const float sz = 0.6f + rnd.nextFloat() * 1.4f;
                bg.fillEllipse (rnd.nextFloat() * getWidth(), rnd.nextFloat() * getHeight(), sz, sz);
            }
            bg.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.08f), 0.f, 0.f, juce::Colours::white.withAlpha (0.02f), 0.f, 100.f, false));
            bg.fillRect (0, 0, getWidth(), 100);
            bg.setGradientFill (Acid::rainbow (0.f, 0.f, (float) getWidth(), 0.f));
            bg.fillRect (0, 100, getWidth(), 2);
        }
        g.drawImageAt (backdrop, 0, 0);

        // logo in a rainbow frame, then the rainbow SKAZI wordmark and byline
        g.setColour (juce::Colours::black);
        g.fillRoundedRectangle (16.f, 10.f, 112.f, 62.f, 7.f);
        g.setGradientFill (Acid::rainbow (16.f, 10.f, 128.f, 72.f));
        g.drawRoundedRectangle (16.5f, 10.5f, 111.f, 61.f, 7.f, 1.2f);
        if (! drawBrandLogo (g, { 22.f, 13.f, 100.f, 56.f }))
        {
            g.setColour (juce::Colours::white);
            g.setFont (Acid::font (30.f));
            g.drawText ("SKAZI", 16, 10, 112, 62, juce::Justification::centred);
        }
        juce::GlyphArrangement ga;
        ga.addLineOfText (juce::Font (juce::FontOptions ("Impact", 44.f, juce::Font::plain)).withExtraKerningFactor (0.08f), "SKAZI", 140.f, 52.f);
        juce::Path word;
        ga.createPath (word);
        juce::DropShadow (juce::Colour (0xffff00c8).withAlpha (0.7f), 12, {}).drawForPath (g, word);
        g.setGradientFill (Acid::rainbow (140.f, 0.f, 140.f + word.getBounds().getWidth(), 0.f));
        g.fillPath (word);
        juce::GlyphArrangement by;
        by.addLineOfText (Acid::font (12.5f).withExtraKerningFactor (0.2f), "BY ASHER SWISSA", 141.f, 72.f);
        juce::Path byP;
        by.createPath (byP);
        g.setGradientFill (Acid::rainbow (141.f, 0.f, 141.f + byP.getBounds().getWidth(), 0.f));
        g.fillPath (byP);
        g.setColour (Acid::dim);
        g.setFont (Acid::font (9.f).withExtraKerningFactor (0.2f));
        g.drawText ("PSY / ACID / VA SYNTH", 141, 78, 190, 14, juce::Justification::centredLeft);
        g.drawText ("PRESET", 360, 14, 400, 14, juce::Justification::centred);
    }

    void paintHyper (juce::Graphics& g)
    {
        const auto red = Acid::green;
        g.fillAll (Acid::bg);

        // Red anodised side cheeks
        for (float x : { 0.f, (float) getWidth() - 10.f })
        {
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xffd01818), x, 0.f, juce::Colour (0xff7a0808), x + 10.f, 0.f, false));
            g.fillRect (x, 0.f, 10.f, (float) getHeight());
        }

        // Black top body
        juce::Rectangle<float> body (10.f, 0.f, (float) getWidth() - 20.f, 100.f);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff1c1c20), 0.f, 0.f, juce::Colour (0xff0b0b0d), 0.f, 100.f, false));
        g.fillRect (body);
        g.setColour (red);
        g.fillRect (10.f, 99.f, (float) getWidth() - 20.f, 2.f);

        // Italic wordmark with a red swoosh
        if (! drawBrandLogo (g, { 26.f, 5.f, 106.f, 57.f }))
        {
            juce::GlyphArrangement ga;
            ga.addLineOfText (Acid::font (58.f).italicised().withExtraKerningFactor (0.04f), "SKAZI", 26.f, 62.f);
            juce::Path logo;
            ga.createPath (logo);
            juce::DropShadow (juce::Colours::black, 6, { 2, 3 }).drawForPath (g, logo);
            g.setColour (juce::Colours::white);
            g.fillPath (logo);
        }
        juce::Path swoosh;
        swoosh.startNewSubPath (28.f, 74.f);
        swoosh.quadraticTo (140.f, 64.f, 236.f, 70.f);
        swoosh.lineTo (236.f, 74.f);
        swoosh.quadraticTo (140.f, 70.f, 28.f, 78.f);
        swoosh.closeSubPath();
        g.setColour (red);
        g.fillPath (swoosh);
        g.setColour (Acid::text);
        g.setFont (Acid::font (14.f).italicised().withExtraKerningFactor (0.12f));
        g.drawText ("BY ASHER SWISSA", 30, 78, 300, 18, juce::Justification::centredLeft);
        g.setColour (Acid::dim);
        g.setFont (Acid::font (9.5f).withExtraKerningFactor (0.2f));
        g.drawText ("HYPER WAVETABLE SYNTHESIZER", 186, 82, 240, 14, juce::Justification::centredLeft);

        // Rail
        juce::Rectangle<float> rail ((float) railX + 2.f, 108.f, (float) railW - 2.f, (float) getHeight() - 120.f);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff26262b), 0.f, rail.getY(), juce::Colour (0xff18181b), 0.f, rail.getBottom(), false));
        g.fillRoundedRectangle (rail, 4.f);
        g.setColour (juce::Colours::black);
        g.drawRoundedRectangle (rail, 4.f, 1.2f);
        juce::Rectangle<float> tab ((float) railX + 10.f, 116.f, (float) railW - 20.f, 18.f);
        g.setColour (red);
        g.fillRoundedRectangle (tab, 2.f);
        g.setColour (juce::Colours::black);
        g.setFont (Acid::font (11.f).italicised());
        g.drawText ("SECTIONS", tab, juce::Justification::centred);

        g.setColour (Acid::dim);
        g.setFont (Acid::font (10.f).withExtraKerningFactor (0.2f));
        g.drawText ("PROGRAM", 360, 14, 400, 14, juce::Justification::centred);
    }

    void paintRobot (juce::Graphics& g)
    {
        const auto cyan = Acid::green, orange = Acid::cyan;
        g.fillAll (Acid::bg);

        // Dot grid backdrop
        g.setColour (juce::Colours::white.withAlpha (0.035f));
        for (int y = 8; y < getHeight(); y += 16)
            for (int x = 8; x < getWidth(); x += 16)
                g.fillRect ((float) x, (float) y, 1.2f, 1.2f);

        // Header plate
        auto header = Acid::chamfer ({ 6.f, 4.f, (float) getWidth() - 12.f, 96.f }, 18.f);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff1a222b), 0.f, 0.f, juce::Colour (0xff0f1419), 0.f, 100.f, false));
        g.fillPath (header);
        g.setColour (Acid::line);
        g.strokePath (header, juce::PathStrokeType (1.2f));
        g.setColour (cyan);
        g.drawLine (6.f, 22.f, 24.f, 4.f, 2.f);

        // Hazard stripes next to PANIC
        {
            juce::Rectangle<float> hz (1168.f, 88.f, 160.f, 8.f);
            g.saveState();
            g.reduceClipRegion (hz.toNearestInt());
            g.setColour (juce::Colour (0xffffd400));
            g.fillRect (hz);
            g.setColour (juce::Colour (0xff0b0e12));
            for (float x = hz.getX() - 10.f; x < hz.getRight(); x += 14.f)
            {
                juce::Path p;
                p.addQuadrilateral (x, hz.getBottom(), x + 7.f, hz.getBottom(), x + 15.f, hz.getY(), x + 8.f, hz.getY());
                g.fillPath (p);
            }
            g.restoreState();
        }

        // Wordmark: SKAZI with cyan glow and a segmented underline
        if (! drawBrandLogo (g, { 28.f, 8.f, 102.f, 54.f }))
        {
            juce::GlyphArrangement ga;
            ga.addLineOfText (Acid::font (54.f).withExtraKerningFactor (0.28f), "SKAZI", 26.f, 58.f);
            juce::Path logo;
            ga.createPath (logo);
            juce::DropShadow (cyan.withAlpha (0.6f), 14, {}).drawForPath (g, logo);
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xffeaf8ff), 0.f, 14.f, juce::Colour (0xff8fd9ea), 0.f, 60.f, false));
            g.fillPath (logo);
        }
        for (int i = 0; i < 12; ++i)
        {
            g.setColour (i < 9 ? cyan : orange);
            g.fillRect (28.f + (float) i * 19.f, 66.f, 15.f, 3.f);
        }
        g.setColour (Acid::text);
        g.setFont (Acid::font (15.f).withExtraKerningFactor (0.22f));
        g.drawText ("BY ASHER SWISSA", 28, 72, 300, 20, juce::Justification::centredLeft);
        g.setColour (orange);
        g.fillEllipse (262.f, 79.f, 6.f, 6.f);
        g.setColour (Acid::dim);
        g.setFont (Acid::font (10.f).withExtraKerningFactor (0.2f));
        g.drawText ("SYS ONLINE", 272, 74, 80, 16, juce::Justification::centredLeft);

        // Module rail
        auto rail = Acid::chamfer ({ (float) railX, 108.f, (float) railW, (float) getHeight() - 120.f }, 14.f);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff161d25), 0.f, 108.f, juce::Colour (0xff0e1318), 0.f, (float) getHeight(), false));
        g.fillPath (rail);
        g.setColour (Acid::line);
        g.strokePath (rail, juce::PathStrokeType (1.2f));
        g.setColour (cyan);
        g.fillRect ((float) railX + 10.f, 120.f, 3.f, 12.f);
        g.setColour (Acid::text);
        g.setFont (Acid::font (12.f).withExtraKerningFactor (0.2f));
        g.drawText ("MODULES", railX + 18, 116, railW - 24, 20, juce::Justification::centredLeft);
        g.setColour (Acid::dim);
        g.setFont (Acid::font (10.f));
        for (int i = 0; i < tabs.size(); ++i)
            g.drawText (juce::String (i + 1).paddedLeft ('0', 2), railX + 14, 144 + i * 66, 30, 14, juce::Justification::centredLeft);

        g.setColour (Acid::dim);
        g.setFont (Acid::font (10.f).withExtraKerningFactor (0.2f));
        g.drawText ("PRESET", 360, 14, 400, 14, juce::Justification::centred);
    }

    void resized() override
    {
        if (presetBar == nullptr) return;
        // Header: logo | preset | run + tempo / master / pan | PANIC
        presetBar->setBounds (330, 28, 440, 54);
        runButton.setBounds (780, 30, 80, 44);
        bpmKnob->setBounds (872, 4, 96, 92);
        volumeKnob->setBounds (970, 4, 96, 92);
        panKnob->setBounds (1068, 4, 96, 92);
        panicButton->setBounds (1180, 16, 148, 68);

        // Left module rail
        for (int i = 0; i < tabs.size(); ++i)
            tabs[i]->setBounds (railX + 10, 140 + i * 66, railW - 20, 58);
        browseButton.setBounds (railX + 10, 140 + tabs.size() * 66 + 16, railW - 20, 44);
        const int wheelY = 140 + tabs.size() * 66 + 16 + 44 + 18;
        const int wheelH = juce::jmax (80, getHeight() - 64 - 16 - wheelY);
        pitchWheel.setBounds (railX + 12, wheelY, (railW - 32) / 2, wheelH);
        modWheel.setBounds (railX + 20 + (railW - 32) / 2, wheelY, (railW - 32) / 2, wheelH);
        skinButton.setBounds (railX + 10, getHeight() - 64, railW - 20, 40);

        // Work area with the console along the bottom
        const int x = railX + railW + 12, w = getWidth() - x - 12;
        for (auto* pg : pages)
            pg->setBounds (x, 108, w, 712);
        console->setBounds (x, 828, w, getHeight() - 840);
        if (browser != nullptr)
            browser->setBounds (x, 108, w, getHeight() - 120);
    }

    void openBrowser()
    {
        browser = std::make_unique<PresetBrowser> (proc, [this]
        {
            juce::Component::SafePointer<MainView> safe (this);
            juce::MessageManager::callAsync ([safe] { if (safe != nullptr) { safe->browser.reset(); } });
        });
        addAndMakeVisible (*browser);
        resized();
    }

    // Used by the offline snapshot tool
    void selectPage (int index)
    {
        if (! juce::isPositiveAndBelow (index, tabs.size())) return;
        tabs[index]->setToggleState (true, juce::dontSendNotification);
        showPage (index);
    }

private:
    void buildUi()
    {
        browser.reset();
        for (auto* pg : pages)
            if (auto* tp = dynamic_cast<Page*> (pg)) tp->stopTimer();
        pages.clear();
        tabs.clear();
        bpmKnob.reset(); volumeKnob.reset(); panKnob.reset(); presetBar.reset(); console.reset();

        runButton.setColour (juce::TextButton::buttonOnColourId, Acid::accentFor (Acid::green));
        skinButton.setColour (juce::TextButton::buttonOnColourId, Acid::violet);
        browseButton.setColour (juce::TextButton::buttonOnColourId, Acid::accentFor (Acid::cyan));
        static const char* const skinNames[] = { "SKIN: NEON", "SKIN: VINTAGE", "SKIN: CHROME", "SKIN: ROBOT", "SKIN: HYPER", "SKIN: BULLY" };
        skinButton.setButtonText (skinNames[juce::jlimit (0, Acid::numSkins - 1, Acid::skin)]);

        bpmKnob = std::make_unique<Knob> (proc.apvts, "bpm", "TEMPO", Acid::violet);
        volumeKnob = std::make_unique<Knob> (proc.apvts, "volume", "MASTER", Acid::violet);
        panKnob = std::make_unique<Knob> (proc.apvts, "pan", "PAN", Acid::violet, true);
        addAndMakeVisible (*bpmKnob);
        addAndMakeVisible (*volumeKnob);
        addAndMakeVisible (*panKnob);

        presetBar = std::make_unique<PresetBar> (proc, [this] { openBrowser(); });
        addAndMakeVisible (*presetBar);
        console = std::make_unique<Console> (proc);
        addAndMakeVisible (*console);

        const char* tabNames[] = { "OSC", "FILTER", "ENV", "LFO", "MATRIX", "FX", "SEQ / ARP", "PERFORM" };
        const juce::Colour tabCols[] = { Acid::green, Acid::cyan, Acid::orange, Acid::magenta, Acid::violet, Acid::yellow, Acid::green, Acid::magenta };
        for (int i = 0; i < 8; ++i)
        {
            auto* t = tabs.add (new juce::TextButton (tabNames[i]));
            t->setColour (juce::TextButton::buttonOnColourId, Acid::accentFor (tabCols[i]));
            t->setRadioGroupId (1001);
            t->setClickingTogglesState (true);
            t->onClick = [this, i] { showPage (i); };
            addAndMakeVisible (t);
        }

        pages.add (new OscPage (proc));
        pages.add (new FilterPage (proc));
        pages.add (new EnvPage (proc));
        pages.add (new LfoPage (proc));
        pages.add (new MatrixPage (proc));
        pages.add (new FxPage (proc));
        pages.add (new SeqPage (proc));
        pages.add (new PerformPage (proc));
        for (auto* pg : pages) addChildComponent (pg);

        tabs[currentPage]->setToggleState (true, juce::dontSendNotification);
        backdrop = {};
        resized();
        showPage (currentPage);
        repaint();
    }

    void switchSkin()
    {
        // Bully -> Hyper -> Robot -> Chrome -> Vintage -> Neon -> Bully
        const int next = Acid::skin == Acid::Bully ? Acid::Hyper
                       : Acid::skin == Acid::Hyper ? Acid::Robot
                       : Acid::skin == Acid::Robot ? Acid::Chrome
                       : Acid::skin == Acid::Chrome ? Acid::Vintage
                       : Acid::skin == Acid::Vintage ? Acid::Neon : Acid::Bully;
        proc.skin = next;
        Acid::applySkin (next);
        lnf.refreshColours();
        buildUi();
    }

    void showPage (int index)
    {
        // a section button always takes you to that section, even from the preset browser
        if (browser != nullptr)
            browser.reset();
        currentPage = index;
        if (juce::isPositiveAndBelow (index, tabs.size()))
            tabs[index]->setToggleState (true, juce::dontSendNotification);
        for (int i = 0; i < pages.size(); ++i)
        {
            pages[i]->setVisible (i == index);
            if (auto* tp = dynamic_cast<Page*> (pages[i]))
            {
                if (i == index) tp->startTimerHz (30);
                else tp->stopTimer();
            }
        }
    }

    void timerCallback() override
    {
        runButton.repaint();
        pitchWheel.refresh();
        modWheel.refresh();
    }

    AcidanceProcessor& proc;
    AcidLookAndFeel& lnf;
    juce::TooltipWindow tooltipWindow { this, 600 };
    juce::TextButton skinButton, runButton { "RUN" };
    juce::TextButton browseButton { "BROWSE" };
    PerfWheel pitchWheel { "PITCH", true, [this] { return proc.uiBend.load(); },
                           [this] (float v) { proc.screenPitch.store (v); proc.pitchMoved.store (true); } };
    PerfWheel modWheel { "MOD", false, [this] { return proc.uiMod.load(); },
                         [this] (float v) { proc.screenMod.store (v); proc.modMoved.store (true); } };
    static constexpr int railX = 12, railW = 136;
    std::unique_ptr<APVTS::ButtonAttachment> runAttachment;
    std::unique_ptr<PanicButton> panicButton;
    std::unique_ptr<Knob> bpmKnob, volumeKnob, panKnob;
    std::unique_ptr<PresetBar> presetBar;
    std::unique_ptr<Console> console;
    std::unique_ptr<PresetBrowser> browser;
    juce::OwnedArray<juce::TextButton> tabs;
    juce::OwnedArray<juce::Component> pages;
    int currentPage = 0;
    juce::Image backdrop;
};

//==============================================================================
AcidanceEditor::AcidanceEditor (AcidanceProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    Acid::applySkin (proc.skin.load());
    lnf.refreshColours();
    setLookAndFeel (&lnf);

    view = std::make_unique<MainView> (proc, lnf);
    addAndMakeVisible (*view);

    setResizable (true, true);
    setResizeLimits (baseWidth / 2, baseHeight / 2, baseWidth * 3 / 2, baseHeight * 3 / 2);
    if (auto* c = getConstrainer()) c->setFixedAspectRatio ((double) baseWidth / baseHeight);
    const float scale = juce::jlimit (0.5f, 1.5f, proc.uiScale.load());
    setSize (juce::roundToInt (baseWidth * scale), juce::roundToInt (baseHeight * scale));
}

AcidanceEditor::~AcidanceEditor()
{
    view.reset();
    setLookAndFeel (nullptr);
}

void AcidanceEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black);
}

void AcidanceEditor::resized()
{
    const float scale = (float) getWidth() / (float) baseWidth;
    proc.uiScale = scale;
    if (view != nullptr)
        view->setTransform (juce::AffineTransform::scale (scale));
}

void AcidanceEditor::showPresetBrowser()
{
    if (view != nullptr) view->openBrowser();
}
