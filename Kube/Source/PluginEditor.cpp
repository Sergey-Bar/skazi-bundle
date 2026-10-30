#include "PluginEditor.h"
#include "Presets.h"

namespace kube
{
namespace col
{
    // acid lime on black, violet accents: no other SKAZI plugin is green
    const juce::Colour bgTop { 0xff030503 }, bgBottom { 0xff0d1709 }, panel { 0xff0e170c }, well { 0xff030602 };
    const juce::Colour glow { 0xffb6ff1a }, accent { 0xff9b5cff }, second { 0xff2be38f }, hot { 0xffeeff55 };
    const juce::Colour text { 0xffeaffd8 }, dim { 0xff7f9a70 };
}

juce::Font font (float size, bool bold = true) { return juce::Font (juce::FontOptions (size, bold ? juce::Font::bold : juce::Font::plain)); }

//==============================================================================
// 1. Custom neon psychedelic skin
class PsyLook : public juce::LookAndFeel_V4
{
public:
    PsyLook()
    {
        setColour (juce::Slider::rotarySliderFillColourId, col::glow);
        setColour (juce::Slider::thumbColourId, col::accent);
        setColour (juce::Slider::textBoxTextColourId, col::text);
        setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        setColour (juce::Slider::textBoxHighlightColourId, col::accent.withAlpha (0.4f));
        setColour (juce::Label::textColourId, col::glow);
        setColour (juce::ComboBox::backgroundColourId, col::well);
        setColour (juce::ComboBox::textColourId, col::text);
        setColour (juce::ComboBox::outlineColourId, col::glow.withAlpha (0.5f));
        setColour (juce::ComboBox::arrowColourId, col::accent);
        setColour (juce::PopupMenu::backgroundColourId, col::panel);
        setColour (juce::PopupMenu::textColourId, col::text);
        setColour (juce::PopupMenu::headerTextColourId, col::accent);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, col::accent.withAlpha (0.35f));
        setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
        setColour (juce::TextButton::buttonColourId, col::well);
        setColour (juce::TextButton::textColourOffId, col::glow);
        setColour (juce::TextButton::textColourOnId, col::accent);
        setColour (juce::TextEditor::backgroundColourId, col::well);
        setColour (juce::TextEditor::textColourId, col::text);
        setColour (juce::TextEditor::highlightColourId, col::accent.withAlpha (0.4f));
        setColour (juce::CaretComponent::caretColourId, col::glow);
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
                           float rotaryStartAngle, float rotaryEndAngle, juce::Slider& slider) override
    {
        const float radius = (float) juce::jmin (width / 2, height / 2) - 5.0f;
        const float cx = (float) x + (float) width * 0.5f, cy = (float) y + (float) height * 0.5f;
        const float angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);
        const bool bipolar = slider.getMinimum() < 0.0;
        const float from = bipolar ? (rotaryStartAngle + rotaryEndAngle) * 0.5f : rotaryStartAngle;

        g.setColour (col::panel.brighter (0.05f));
        g.fillEllipse (cx - radius, cy - radius, radius * 2.0f, radius * 2.0f);

        juce::Path track, arc;
        track.addCentredArc (cx, cy, radius, radius, 0.0f, rotaryStartAngle, rotaryEndAngle, true);
        g.setColour (col::glow.withAlpha (0.15f));
        g.strokePath (track, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        arc.addCentredArc (cx, cy, radius, radius, 0.0f, juce::jmin (from, angle), juce::jmax (from, angle), true);
        g.setColour (col::glow.withAlpha (0.25f)); // glow
        g.strokePath (arc, juce::PathStrokeType (7.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (col::glow);
        g.strokePath (arc, juce::PathStrokeType (2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        juce::Path p;
        p.addRoundedRectangle (-2.0f, -radius + 3.0f, 4.0f, radius * 0.55f, 2.0f);
        p.applyTransform (juce::AffineTransform::rotation (angle).translated (cx, cy));
        g.setColour (col::accent.withAlpha (slider.isMouseOverOrDragging() ? 1.0f : 0.85f));
        g.fillPath (p);
    }

    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool over, bool) override
    {
        auto r = b.getLocalBounds().toFloat().reduced (1.5f);
        const bool on = b.getToggleState();
        g.setColour (on ? col::accent.withAlpha (0.25f) : col::well);
        g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
        g.setColour (on ? col::accent : col::dim.withAlpha (over ? 0.9f : 0.5f));
        g.drawRoundedRectangle (r, r.getHeight() * 0.5f, 1.5f);
        auto dot = r.removeFromLeft (r.getHeight()).reduced (r.getHeight() * 0.3f);
        g.setColour (on ? col::accent : col::dim.withAlpha (0.5f));
        g.fillEllipse (dot);
        g.setColour (on ? juce::Colours::white : col::dim);
        g.setFont (font (12.0f));
        g.drawText (b.getButtonText(), r.withTrimmedRight (6.0f), juce::Justification::centred);
    }

    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down) override
    {
        auto r = b.getLocalBounds().toFloat().reduced (1.0f);
        const auto accent = b.getProperties().getWithDefault ("accent", false) ? col::hot : col::glow;
        g.setColour (down ? accent.withAlpha (0.3f) : over ? accent.withAlpha (0.12f) : col::well);
        g.fillRoundedRectangle (r, 6.0f);
        g.setColour (accent.withAlpha (over ? 0.9f : 0.5f));
        g.drawRoundedRectangle (r, 6.0f, 1.5f);
    }

    void drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool) override
    {
        const auto accent = b.getProperties().getWithDefault ("accent", false) ? col::hot : col::glow;
        g.setColour (accent);
        g.setFont (font (juce::jmin (14.0f, b.getHeight() * 0.5f)));
        g.drawFittedText (b.getButtonText(), b.getLocalBounds().reduced (4, 0), juce::Justification::centred, 1);
    }

    void drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box) override
    {
        auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (1.0f);
        g.setColour (col::well);
        g.fillRoundedRectangle (r, 5.0f);
        g.setColour (col::glow.withAlpha (box.isMouseOver (true) ? 0.9f : 0.45f));
        g.drawRoundedRectangle (r, 5.0f, 1.2f);
        juce::Path arrow;
        const float ax = (float) w - 14.0f, ay = (float) h * 0.5f;
        arrow.addTriangle (ax - 4.0f, ay - 2.0f, ax + 4.0f, ay - 2.0f, ax, ay + 3.0f);
        g.setColour (col::accent);
        g.fillPath (arrow);
    }

    juce::Font getComboBoxFont (juce::ComboBox&) override { return font (13.0f); }
    juce::Font getPopupMenuFont() override { return font (14.0f, false); }
    juce::Font getLabelFont (juce::Label& l) override { return l.getFont(); }

    void positionComboBoxText (juce::ComboBox& box, juce::Label& label) override
    {
        label.setBounds (6, 1, box.getWidth() - 24, box.getHeight() - 2);
        label.setFont (getComboBoxFont (box));
        label.setJustificationType (juce::Justification::centredLeft);
    }

    juce::Label* createSliderTextBox (juce::Slider& s) override
    {
        auto* l = LookAndFeel_V4::createSliderTextBox (s);
        l->setFont (font (11.0f, false));
        l->setColour (juce::Label::textColourId, col::text.withAlpha (0.8f));
        l->setColour (juce::Label::outlineColourId, juce::Colours::transparentBlack);
        return l;
    }

    // Tabs: neon pills on a dark rail
    int getTabButtonBestWidth (juce::TabBarButton& b, int) override { return juce::jmax (150, b.getTabbedButtonBar().getWidth() / 4 - 8); }
    void drawTabbedButtonBarBackground (juce::TabbedButtonBar&, juce::Graphics&) override {}
    void drawTabAreaBehindFrontButton (juce::TabbedButtonBar& bar, juce::Graphics& g, int w, int h) override
    {
        juce::ignoreUnused (bar);
        g.setGradientFill (juce::ColourGradient (col::accent.withAlpha (0.0f), 0, 0, col::accent.withAlpha (0.7f), (float) w * 0.5f, 0, false));
        g.fillRect (0, h - 2, w / 2, 2);
        g.setGradientFill (juce::ColourGradient (col::glow.withAlpha (0.7f), (float) w * 0.5f, 0, col::glow.withAlpha (0.0f), (float) w, 0, false));
        g.fillRect (w / 2, h - 2, w - w / 2, 2);
    }

    void drawTabButton (juce::TabBarButton& b, juce::Graphics& g, bool over, bool) override
    {
        auto r = b.getActiveArea().toFloat().reduced (4.0f, 3.0f);
        const bool front = b.isFrontTab();
        if (front)
        {
            g.setGradientFill (juce::ColourGradient (col::accent.withAlpha (0.45f), r.getX(), r.getY(), col::second.withAlpha (0.35f), r.getRight(), r.getBottom(), false));
            g.fillRoundedRectangle (r, 8.0f);
            g.setColour (col::accent);
            g.drawRoundedRectangle (r, 8.0f, 1.5f);
        }
        else
        {
            g.setColour (col::panel.withAlpha (over ? 1.0f : 0.7f));
            g.fillRoundedRectangle (r, 8.0f);
            g.setColour (col::glow.withAlpha (over ? 0.6f : 0.25f));
            g.drawRoundedRectangle (r, 8.0f, 1.0f);
        }
        g.setColour (front ? juce::Colours::white : col::glow.withAlpha (over ? 1.0f : 0.7f));
        g.setFont (font (14.0f));
        g.drawText (b.getButtonText(), r, juce::Justification::centred);
    }
};

//==============================================================================
// Controls bound to parameters
struct Knob : juce::Component
{
    Knob (juce::AudioProcessorValueTreeState& vts, const juce::String& id, const juce::String& title)
    {
        slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 64, 14);
        slider.setPopupDisplayEnabled (false, false, nullptr);
        addAndMakeVisible (slider);
        label.setText (title, juce::dontSendNotification);
        label.setJustificationType (juce::Justification::centred);
        label.setFont (font (11.0f));
        label.setColour (juce::Label::textColourId, col::glow);
        label.setInterceptsMouseClicks (false, false);
        addAndMakeVisible (label);
        attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (vts, id, slider);
        slider.setDoubleClickReturnValue (true, (double) vts.getParameterRange (id).convertFrom0to1 (vts.getParameter (id)->getDefaultValue()));
    }
    void resized() override
    {
        auto r = getLocalBounds();
        label.setBounds (r.removeFromTop (14));
        const int size = juce::jmin (r.getWidth(), r.getHeight() - 14, 76);
        slider.setBounds (r.withSizeKeepingCentre (juce::jmax (size, 64), size + 14));
    }
    juce::Slider slider;
    juce::Label label;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

struct Choice : juce::Component
{
    Choice (juce::AudioProcessorValueTreeState& vts, const juce::String& id, const juce::String& title)
    {
        if (auto* p = dynamic_cast<juce::AudioParameterChoice*> (vts.getParameter (id)))
            box.addItemList (p->choices, 1);
        addAndMakeVisible (box);
        if (title.isNotEmpty())
        {
            label.setText (title, juce::dontSendNotification);
            label.setJustificationType (juce::Justification::centred);
            label.setFont (font (11.0f));
            label.setColour (juce::Label::textColourId, col::glow);
            addAndMakeVisible (label);
        }
        attachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (vts, id, box);
    }
    void resized() override
    {
        auto r = getLocalBounds();
        if (label.getText().isNotEmpty()) label.setBounds (r.removeFromTop (14));
        box.setBounds (r.withSizeKeepingCentre (r.getWidth(), juce::jmin (26, r.getHeight())));
    }
    juce::ComboBox box;
    juce::Label label;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> attachment;
};

struct Toggle : juce::ToggleButton
{
    Toggle (juce::AudioProcessorValueTreeState& vts, const juce::String& id, const juce::String& text) : juce::ToggleButton (text)
    {
        attachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (vts, id, *this);
    }
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attachment;
};

// Lays the items out side by side in equal columns
void row (juce::Rectangle<int> r, std::initializer_list<juce::Component*> items, int gap = 4)
{
    const int n = (int) items.size();
    const int w = (r.getWidth() - gap * (n - 1)) / n;
    for (auto* c : items)
    {
        c->setBounds (r.removeFromLeft (w));
        r.removeFromLeft (gap);
    }
}

// Rounded panel with a title
struct Panel : juce::Component
{
    explicit Panel (juce::String t, juce::Colour a = col::accent) : title (std::move (t)), accent (a) { setInterceptsMouseClicks (false, true); }
    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (1.0f);
        g.setGradientFill (juce::ColourGradient (col::panel.brighter (0.06f), 0, 0, col::panel.darker (0.2f), 0, r.getBottom(), false));
        g.fillRoundedRectangle (r, 10.0f);
        g.setColour (accent.withAlpha (0.35f));
        g.drawRoundedRectangle (r, 10.0f, 1.2f);
        g.setColour (accent);
        g.setFont (font (13.0f));
        g.drawText (title, 12, 6, 200, 18, juce::Justification::centredLeft);
    }
    juce::Rectangle<int> content() const { return getLocalBounds().reduced (10).withTrimmedTop (22); }
    juce::String title;
    juce::Colour accent;
};

//==============================================================================
// Displays
void fillWell (juce::Graphics& g, juce::Rectangle<float> r)
{
    g.setColour (col::well);
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (col::glow.withAlpha (0.12f));
    g.drawRoundedRectangle (r, 6.0f, 1.0f);
}

// A display that redraws only when what it shows has changed; the editor calls tick() at 30 Hz
struct LiveView : juce::Component
{
    virtual juce::uint64 state() = 0;
    void tick()
    {
        const auto s = state();
        if (s != last) { last = s; repaint(); }
    }
    juce::uint64 last = 0;
};

// Fingerprint of the values a display depends on
struct Sig
{
    juce::uint64 h = 1469598103934665603ull;
    Sig& operator<< (float v)
    {
        juce::uint32 b;
        std::memcpy (&b, &v, sizeof (b));
        h = (h ^ b) * 1099511628211ull;
        return *this;
    }
};

// 2. Real-time oscilloscope of KUBE's output, locked to rising zero crossings
struct Scope : LiveView
{
    explicit Scope (Engine& e) : engine (e) {}
    juce::uint64 state() override // silent: one flat frame, then nothing to redraw
    {
        return engine.uiLevel.load (std::memory_order_relaxed) < 1.0e-4f ? 0 : (juce::uint64) engine.scopeWrite.load (std::memory_order_relaxed) + 1;
    }
    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        fillWell (g, r);
        g.setColour (col::glow.withAlpha (0.13f));
        g.drawHorizontalLine ((int) r.getCentreY(), r.getX() + 4, r.getRight() - 4);

        constexpr int N = Engine::kScope;
        std::array<float, N> buf;
        const int w = engine.scopeWrite.load (std::memory_order_relaxed);
        for (int i = 0; i < N; ++i) buf[(size_t) i] = engine.scope[(size_t) ((w + i) % N)].load (std::memory_order_relaxed);
        int start = 0;
        for (int i = N / 2 - 1; i > 0; --i)
            if (buf[(size_t) i - 1] <= 0.0f && buf[(size_t) i] > 0.0f) { start = i; break; }
        const int len = N / 2;
        float peak = 0.05f;
        for (int i = 0; i < len; ++i) peak = juce::jmax (peak, std::abs (buf[(size_t) (start + i)]));
        const float gain = juce::jmin (6.0f, 0.9f / peak); // auto zoom, so quiet sounds still show their shape

        juce::Path p;
        for (int i = 0; i < len; ++i)
        {
            const float x = r.getX() + 4 + (r.getWidth() - 8) * (float) i / (float) (len - 1);
            const float y = r.getCentreY() - buf[(size_t) (start + i)] * gain * (r.getHeight() * 0.42f);
            if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
        }
        g.setColour (col::glow);
        g.strokePath (p, juce::PathStrokeType (1.8f));
    }
    Engine& engine;
};

// The wavetable as a stack of frames in 3D, with the playing position lit up
struct WaveView : LiveView
{
    WaveView (KubeProcessor& p, int o) : proc (p), prefix (o == 0 ? "a" : "b"), osc (o) {}
    juce::uint64 state() override
    {
        Sig sig;
        for (const char* id : { "Table", "On", "Warp1", "Warp1Amt", "Warp2", "Warp2Amt" })
            sig << proc.apvts.getRawParameterValue (prefix + id)->load();
        sig << std::round (proc.engine.uiPos[osc].load (std::memory_order_relaxed) * 400.0f)
            << (float) (juce::pointer_sized_int) proc.userTable (osc);
        return sig.h;
    }
    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        fillWell (g, r);
        const auto o = proc.settings().osc[(size_t) osc];
        const auto& table = proc.engine.tableFor (o, osc);
        const float pos = proc.engine.uiPos[osc].load (std::memory_order_relaxed);
        WarpState warp;
        warp.set (o);

        constexpr int shown = 16; // frames drawn in the stack, spread over however many the table has
        const float dx = r.getWidth() * 0.018f, dy = r.getHeight() * 0.032f;
        const float w = r.getWidth() - 24.0f - dx * (shown - 1), h = r.getHeight() * 0.3f;
        const float x0 = r.getX() + 12.0f, y0 = r.getBottom() - h - 12.0f;
        auto frame = [&] (float morph, float depth, bool warped)
        {
            juce::Path p;
            const int points = warped ? 192 : 96;
            for (int i = 0; i <= points; ++i)
            {
                const double ph = juce::jmin (0.9999, i / (double) points);
                const float v = warped ? warp.read (table, morph, ph, 0, 0.0f) : table.read (morph, ph, 0);
                const float x = x0 + depth * dx + w * (float) i / (float) points;
                const float y = y0 - depth * dy - juce::jlimit (-1.2f, 1.2f, v) * h * 0.9f;
                if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
            }
            return p;
        };
        // thin 1 px frames are cheap, and this only repaints when something it shows changes
        for (int f = shown - 1; f >= 0; --f)
        {
            const float m = (float) f / (shown - 1);
            const float near = juce::jmax (0.0f, 1.0f - std::abs (m - pos) * 3.0f);
            g.setColour (col::second.interpolatedWith (col::glow, near).withAlpha (o.on ? 0.3f + 0.35f * near : 0.12f));
            g.strokePath (frame (m, (float) f, false), juce::PathStrokeType (1.0f));
        }
        const auto lit = frame (pos, pos * (shown - 1), true); // the playing frame, warps and all
        g.setColour (o.on ? col::accent : col::dim);
        g.strokePath (lit, juce::PathStrokeType (2.2f));

        g.setColour (col::dim);
        g.setFont (font (11.0f));
        const bool missing = o.table >= kUserTable && proc.userTable (osc) == nullptr;
        g.drawText ((missing ? juce::String ("USER: LOAD OR DROP A .WAV") : table.getName()) + "  " + juce::String ((int) std::round (pos * 100.0f)) + "%",
                    r.reduced (8, 4), juce::Justification::topRight);
    }
    KubeProcessor& proc;
    juce::String prefix;
    int osc;
};

struct FilterView : LiveView
{
    explicit FilterView (KubeProcessor& p) : proc (p) {}
    juce::uint64 state() override { return (Sig() << proc.v ("fltType") << proc.v ("cutoff") << proc.v ("reso")).h; }
    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        fillWell (g, r);
        const int type = (int) proc.v ("fltType");
        const float cutoff = proc.v ("cutoff"), reso = proc.v ("reso") * 0.01f;
        g.setColour (col::glow.withAlpha (0.08f));
        for (float f : { 100.0f, 1000.0f, 10000.0f })
        {
            const float x = r.getX() + r.getWidth() * std::log (f / 20.0f) / std::log (1000.0f);
            g.drawVerticalLine ((int) x, r.getY() + 4, r.getBottom() - 4);
        }
        juce::Path p, fill;
        for (int i = 0; i <= 200; ++i)
        {
            const float t = (float) i / 200.0f, f = 20.0f * std::pow (1000.0f, t);
            const float db = juce::jlimit (-48.0f, 24.0f, juce::Decibels::gainToDecibels (filterMagnitude (type, cutoff, reso, f), -100.0f));
            const float x = r.getX() + r.getWidth() * t;
            const float y = r.getY() + r.getHeight() * (0.25f + (-db / 48.0f) * 0.7f);
            if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
        }
        fill = p;
        fill.lineTo (r.getRight(), r.getBottom()); fill.lineTo (r.getX(), r.getBottom()); fill.closeSubPath();
        g.setGradientFill (juce::ColourGradient (col::accent.withAlpha (0.35f), 0, r.getY(), col::accent.withAlpha (0.0f), 0, r.getBottom(), false));
        g.fillPath (fill);
        g.setColour (col::accent);
        g.strokePath (p, juce::PathStrokeType (2.0f));
    }
    KubeProcessor& proc;
};

struct EnvView : LiveView
{
    EnvView (KubeProcessor& p, juce::String pre, juce::Colour c) : proc (p), prefix (std::move (pre)), colour (c) {}
    juce::uint64 state() override
    {
        Sig sig;
        for (const char* s : { "A", "D", "S", "R" }) sig << proc.apvts.getRawParameterValue (prefix + s)->load();
        return sig.h;
    }
    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        fillWell (g, r);
        auto t = [&] (const char* s) { return std::sqrt (proc.apvts.getRawParameterValue (prefix + s)->load() / 5000.0f); };
        const float a = t ("A"), d = t ("D"), rel = t ("R"), s = proc.apvts.getRawParameterValue (prefix + "S")->load() * 0.01f;
        const float total = a + d + rel + 0.35f;
        auto in = r.reduced (8, 8);
        auto X = [&] (float v) { return in.getX() + in.getWidth() * v / total; };
        auto Y = [&] (float v) { return in.getBottom() - in.getHeight() * v; };
        juce::Path p;
        p.startNewSubPath (X (0), Y (0));
        p.lineTo (X (a), Y (1));
        p.quadraticTo (X (a + d * 0.3f), Y (s), X (a + d), Y (s));
        p.lineTo (X (a + d + 0.35f), Y (s));
        p.quadraticTo (X (a + d + 0.35f + rel * 0.3f), Y (0), X (total), Y (0));
        g.setColour (colour.withAlpha (0.25f));
        g.strokePath (p, juce::PathStrokeType (6.0f));
        g.setColour (colour);
        g.strokePath (p, juce::PathStrokeType (2.0f));
    }
    KubeProcessor& proc;
    juce::String prefix;
    juce::Colour colour;
};

struct LfoView : LiveView
{
    explicit LfoView (KubeProcessor& p) : proc (p) {}
    juce::uint64 state() override
    {
        return (Sig() << proc.v ("lfoOn") << proc.v ("lfoShape") << std::round (proc.engine.uiLfo.load (std::memory_order_relaxed) * 100.0f)).h;
    }
    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        fillWell (g, r);
        const int shape = (int) proc.v ("lfoShape");
        auto in = r.reduced (10, 10);
        juce::Path p;
        juce::Random rnd (7);
        float held = 0.0f;
        for (int i = 0; i <= 200; ++i)
        {
            const float ph = std::fmod ((float) i / 100.0f, 1.0f);
            float y;
            switch (shape)
            {
                case Sine:     y = std::sin (ph * juce::MathConstants<float>::twoPi); break;
                case Triangle: y = ph < 0.5f ? 4.0f * ph - 1.0f : 3.0f - 4.0f * ph; break;
                case Saw:      y = 1.0f - 2.0f * ph; break;
                case Square:   y = ph < 0.5f ? 1.0f : -1.0f; break;
                default:       if (i % 25 == 0) held = rnd.nextFloat() * 2.0f - 1.0f; y = held; break;
            }
            const float x = in.getX() + in.getWidth() * (float) i / 200.0f;
            if (i == 0) p.startNewSubPath (x, in.getCentreY() - y * in.getHeight() * 0.45f);
            else p.lineTo (x, in.getCentreY() - y * in.getHeight() * 0.45f);
        }
        const bool on = proc.v ("lfoOn") > 0.5f;
        g.setColour (col::second.withAlpha (on ? 1.0f : 0.25f));
        g.strokePath (p, juce::PathStrokeType (2.0f));
        if (! on)
        {
            g.setColour (col::second.withAlpha (0.7f));
            g.setFont (font (13.0f));
            g.drawText ("LFO OFF", r, juce::Justification::centred);
            return;
        }
        const float now = proc.engine.uiLfo.load (std::memory_order_relaxed);
        const float y = in.getCentreY() - now * in.getHeight() * 0.45f;
        g.setColour (col::accent.withAlpha (0.5f));
        g.drawHorizontalLine ((int) y, in.getX(), in.getRight());
        g.setColour (col::accent);
        g.fillEllipse (in.getRight() - 8.0f, y - 4.0f, 8.0f, 8.0f);
    }
    KubeProcessor& proc;
};

//==============================================================================
// Tab 1: oscillators and waveform
// One warp slot: the mode on top, its amount below
struct WarpSlot : juce::Component
{
    WarpSlot (juce::AudioProcessorValueTreeState& vts, const juce::String& pre, int n)
        : mode (vts, pre + "Warp" + juce::String (n), "WARP " + juce::String (n)), amount (vts, pre + "Warp" + juce::String (n) + "Amt", {})
    {
        addAndMakeVisible (mode);
        addAndMakeVisible (amount);
    }
    void resized() override
    {
        auto r = getLocalBounds();
        mode.setBounds (r.removeFromTop (40));
        amount.setBounds (r);
    }
    Choice mode;
    Knob amount;
};

struct OscPanel : Panel, juce::FileDragAndDropTarget
{
    OscPanel (KubeProcessor& p, int o)
        : Panel (o == 0 ? "OSC A" : "OSC B", o == 0 ? col::accent : col::glow), proc (p), osc (o),
          pre (o == 0 ? "a" : "b"), on (p.apvts, pre + "On", "ON"), table (p.apvts, pre + "Table", {}), view (p, o),
          pos (p.apvts, pre + "Pos", "POSITION"), oct (p.apvts, pre + "Oct", "OCT"), semi (p.apvts, pre + "Semi", "SEMI"),
          fine (p.apvts, pre + "Fine", "FINE"), level (p.apvts, pre + "Level", "LEVEL"), rand (p.apvts, pre + "Rand", "RAND"),
          uni (p.apvts, pre + "Uni", "UNISON"), detune (p.apvts, pre + "Detune", "DETUNE"), blend (p.apvts, pre + "Blend", "BLEND"),
          width (p.apvts, pre + "Width", "WIDTH"), warp1 (p.apvts, pre, 1), warp2 (p.apvts, pre, 2)
    {
        for (juce::Component* c : { (juce::Component*) &on, (juce::Component*) &table, (juce::Component*) &load, (juce::Component*) &view,
                                    (juce::Component*) &pos, (juce::Component*) &oct, (juce::Component*) &semi, (juce::Component*) &fine,
                                    (juce::Component*) &level, (juce::Component*) &rand, (juce::Component*) &uni, (juce::Component*) &detune,
                                    (juce::Component*) &blend, (juce::Component*) &width, (juce::Component*) &warp1, (juce::Component*) &warp2 })
            addAndMakeVisible (c);
        load.setTooltip ("Load a Serum-style wavetable (.wav), or drop one on the oscillator");
        load.onClick = [this]
        {
            chooser = std::make_unique<juce::FileChooser> ("Load a wavetable", juce::File(), "*.wav");
            chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                                  [this] (const juce::FileChooser& fc) { if (fc.getResult().existsAsFile()) loadFile (fc.getResult()); });
        };
    }

    void loadFile (const juce::File& f)
    {
        juce::String error;
        if (! proc.loadUserTable (osc, f, error))
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "KUBE", "Couldn't load that wavetable: " + error);
        view.repaint();
    }

    bool isInterestedInFileDrag (const juce::StringArray& files) override { return files.size() == 1 && files[0].endsWithIgnoreCase (".wav"); }
    void fileDragEnter (const juce::StringArray&, int, int) override { dropping = true; repaint(); }
    void fileDragExit (const juce::StringArray&) override { dropping = false; repaint(); }
    void filesDropped (const juce::StringArray& files, int, int) override { dropping = false; repaint(); loadFile (juce::File (files[0])); }

    void paintOverChildren (juce::Graphics& g) override
    {
        if (! dropping) return;
        g.setColour (col::glow.withAlpha (0.15f));
        g.fillRoundedRectangle (getLocalBounds().toFloat().reduced (2.0f), 10.0f);
        g.setColour (col::glow);
        g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (2.0f), 10.0f, 2.0f);
        g.setFont (font (16.0f));
        g.drawText ("DROP TO LOAD INTO " + title, getLocalBounds(), juce::Justification::centred);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (10);
        auto top = r.removeFromTop (26);
        top.removeFromLeft (64);
        on.setBounds (top.removeFromLeft (64));
        top.removeFromLeft (8);
        load.setBounds (top.removeFromRight (64));
        top.removeFromRight (6);
        table.setBounds (top);
        r.removeFromTop (6);
        view.setBounds (r.removeFromTop (104));
        r.removeFromTop (6);
        row (r.removeFromTop (90), { &pos, &oct, &semi, &fine, &level, &rand }, 2);
        r.removeFromTop (4);
        warp1.setBounds (r.removeFromLeft (100));
        r.removeFromLeft (4);
        warp2.setBounds (r.removeFromLeft (100));
        r.removeFromLeft (4);
        row (r.withTrimmedTop (26), { &uni, &detune, &blend, &width }, 2); // lined up with the warp amounts
    }

    KubeProcessor& proc;
    int osc;
    juce::String pre;
    Toggle on;
    Choice table;
    juce::TextButton load { "LOAD" };
    WaveView view;
    Knob pos, oct, semi, fine, level, rand, uni, detune, blend, width;
    WarpSlot warp1, warp2;
    std::unique_ptr<juce::FileChooser> chooser;
    bool dropping = false;
};

struct OscTab : juce::Component
{
    explicit OscTab (KubeProcessor& p)
        : a (p, 0), b (p, 1), extras ("SUB / NOISE"), folder ("WAVEFOLD", col::hot), scopePanel ("SCOPE", col::glow),
          sub (p.apvts, "sub", "SUB"), subOct (p.apvts, "subOct", "SUB OCT"), noise (p.apvts, "noise", "NOISE"),
          fold (p.apvts, "fold", "WAVEFOLD"), scope (p.engine)
    {
        for (juce::Component* c : { (juce::Component*) &a, (juce::Component*) &b, (juce::Component*) &extras, (juce::Component*) &folder,
                                    (juce::Component*) &scopePanel })
            addAndMakeVisible (c);
        extras.addAndMakeVisible (sub); extras.addAndMakeVisible (subOct); extras.addAndMakeVisible (noise);
        folder.addAndMakeVisible (fold);
        scopePanel.addAndMakeVisible (scope);
    }
    void resized() override
    {
        auto r = getLocalBounds().reduced (12, 10);
        auto top = r.removeFromTop (r.getHeight() - 150);
        a.setBounds (top.removeFromLeft (top.getWidth() / 2 - 5));
        top.removeFromLeft (10);
        b.setBounds (top);
        r.removeFromTop (10);
        extras.setBounds (r.removeFromLeft (250));
        r.removeFromLeft (10);
        folder.setBounds (r.removeFromLeft (130));
        r.removeFromLeft (10);
        scopePanel.setBounds (r);
        auto e = extras.content();
        row (e, { &sub, &subOct, &noise }, 6);
        subOct.setBounds (subOct.getBounds().withSizeKeepingCentre (subOct.getWidth(), 44));
        fold.setBounds (folder.content().reduced (6, 0));
        scope.setBounds (scopePanel.content());
    }
    OscPanel a, b;
    Panel extras, folder, scopePanel;
    Knob sub; Choice subOct; Knob noise, fold;
    Scope scope;
};

// Tab 2: filter and envelopes
struct FilterTab : juce::Component
{
    explicit FilterTab (KubeProcessor& p)
        : filter ("FILTER"), fenv ("FILTER ENVELOPE", col::second), aenv ("AMP ENVELOPE", col::glow),
          type (p.apvts, "fltType", {}), view (p),
          cutoff (p.apvts, "cutoff", "CUTOFF"), reso (p.apvts, "reso", "RESO"), env (p.apvts, "fltEnv", "ENV AMT"),
          key (p.apvts, "keyTrk", "KEY TRACK"), drive (p.apvts, "drive", "DRIVE"),
          fview (p, "flt", col::second), aview (p, "amp", col::glow),
          fA (p.apvts, "fltA", "ATTACK"), fD (p.apvts, "fltD", "DECAY"), fS (p.apvts, "fltS", "SUSTAIN"), fR (p.apvts, "fltR", "RELEASE"),
          aA (p.apvts, "ampA", "ATTACK"), aD (p.apvts, "ampD", "DECAY"), aS (p.apvts, "ampS", "SUSTAIN"), aR (p.apvts, "ampR", "RELEASE")
    {
        for (juce::Component* c : { (juce::Component*) &filter, (juce::Component*) &fenv, (juce::Component*) &aenv }) addAndMakeVisible (c);
        for (juce::Component* c : { (juce::Component*) &type, (juce::Component*) &view, (juce::Component*) &cutoff, (juce::Component*) &reso,
                                    (juce::Component*) &env, (juce::Component*) &key, (juce::Component*) &drive })
            filter.addAndMakeVisible (c);
        for (juce::Component* c : { (juce::Component*) &fview, (juce::Component*) &fA, (juce::Component*) &fD, (juce::Component*) &fS, (juce::Component*) &fR })
            fenv.addAndMakeVisible (c);
        for (juce::Component* c : { (juce::Component*) &aview, (juce::Component*) &aA, (juce::Component*) &aD, (juce::Component*) &aS, (juce::Component*) &aR })
            aenv.addAndMakeVisible (c);
    }
    void resized() override
    {
        auto r = getLocalBounds().reduced (12, 10);
        filter.setBounds (r.removeFromLeft (r.getWidth() / 2 - 5));
        r.removeFromLeft (10);
        fenv.setBounds (r.removeFromTop (r.getHeight() / 2 - 5));
        r.removeFromTop (10);
        aenv.setBounds (r);

        auto f = filter.getLocalBounds().reduced (10);
        type.setBounds (f.removeFromTop (26).withTrimmedLeft (80).withWidth (120));
        f.removeFromTop (8);
        view.setBounds (f.removeFromTop (f.getHeight() - 110));
        f.removeFromTop (10);
        row (f, { &cutoff, &reso, &env, &key, &drive }, 6);

        for (auto [panel, v, k1, k2, k3, k4] : { std::make_tuple (&fenv, (juce::Component*) &fview, &fA, &fD, &fS, &fR),
                                                std::make_tuple (&aenv, (juce::Component*) &aview, &aA, &aD, &aS, &aR) })
        {
            auto c = panel->content();
            auto knobs = c.removeFromRight (c.getWidth() * 3 / 5);
            v->setBounds (c.reduced (0, 6).withTrimmedRight (8));
            row (knobs, { k1, k2, k3, k4 }, 4);
        }
    }
    Panel filter, fenv, aenv;
    Choice type; FilterView view;
    Knob cutoff, reso, env, key, drive;
    EnvView fview, aview;
    Knob fA, fD, fS, fR, aA, aD, aS, aR;
};

// Tab 3: LFO, pitch envelope and voicing
struct ModTab : juce::Component
{
    explicit ModTab (KubeProcessor& p)
        : proc (p), lfo ("LFO", col::second), pitch ("PITCH ENVELOPE  /  ZAP", col::hot), voice ("VOICE", col::glow),
          lfoOn (p.apvts, "lfoOn", "ON"), shape (p.apvts, "lfoShape", "SHAPE"), sync (p.apvts, "lfoSync", "SYNC"), rate (p.apvts, "lfoRate", "RATE"), hz (p.apvts, "lfoHz", "RATE"),
          view (p), toCut (p.apvts, "lfoCut", "> CUTOFF"), toPos (p.apvts, "lfoPos", "> POSITION"), toFold (p.apvts, "lfoFold", "> FOLD"),
          pAmt (p.apvts, "pitchAmt", "AMOUNT"), pDec (p.apvts, "pitchDec", "DECAY"), envPos (p.apvts, "envPos", "F.ENV > POS"),
          mode (p.apvts, "mode", "MODE"), glide (p.apvts, "glide", "GLIDE"), vel (p.apvts, "vel", "VELOCITY")
    {
        for (juce::Component* c : { (juce::Component*) &lfo, (juce::Component*) &pitch, (juce::Component*) &voice }) addAndMakeVisible (c);
        for (juce::Component* c : { (juce::Component*) &lfoOn, (juce::Component*) &shape, (juce::Component*) &sync, (juce::Component*) &rate, (juce::Component*) &hz,
                                    (juce::Component*) &view, (juce::Component*) &toCut, (juce::Component*) &toPos, (juce::Component*) &toFold })
            lfo.addAndMakeVisible (c);
        for (juce::Component* c : { (juce::Component*) &pAmt, (juce::Component*) &pDec, (juce::Component*) &envPos }) pitch.addAndMakeVisible (c);
        for (juce::Component* c : { (juce::Component*) &mode, (juce::Component*) &glide, (juce::Component*) &vel }) voice.addAndMakeVisible (c);
        update();
    }
    void update() // synced LFOs show note lengths, free ones a Hz knob
    {
        const bool synced = proc.v ("lfoSync") > 0.5f;
        if (synced == rate.isVisible() && synced != hz.isVisible()) return;
        rate.setVisible (synced);
        hz.setVisible (! synced);
        resized();
    }
    void resized() override
    {
        auto r = getLocalBounds().reduced (12, 10);
        lfo.setBounds (r.removeFromLeft (r.getWidth() / 2 - 5));
        r.removeFromLeft (10);
        pitch.setBounds (r.removeFromTop (r.getHeight() / 2 - 5));
        r.removeFromTop (10);
        voice.setBounds (r);

        lfoOn.setBounds (lfo.getLocalBounds().reduced (10).removeFromTop (26).removeFromRight (64));
        auto l = lfo.content();
        auto top = l.removeFromTop (44);
        shape.setBounds (top.removeFromLeft (120));
        top.removeFromLeft (10);
        sync.setBounds (top.removeFromLeft (80).withTrimmedTop (16));
        top.removeFromLeft (10);
        rate.setBounds (top.removeFromLeft (100));
        hz.setBounds (rate.getBounds().withHeight (88).withWidth (70));
        l.removeFromTop (hz.isVisible() ? 50 : 8);
        view.setBounds (l.removeFromTop (l.getHeight() - 106));
        l.removeFromTop (8);
        row (l, { &toCut, &toPos, &toFold }, 10);

        row (pitch.content(), { &pAmt, &pDec, &envPos }, 10);
        auto v = voice.content();
        mode.setBounds (v.removeFromLeft (v.getWidth() / 3).withSizeKeepingCentre (110, 44));
        row (v, { &glide, &vel }, 10);
    }
    KubeProcessor& proc;
    Panel lfo, pitch, voice;
    Toggle lfoOn; Choice shape; Toggle sync; Choice rate; Knob hz;
    LfoView view;
    Knob toCut, toPos, toFold, pAmt, pDec, envPos;
    Choice mode; Knob glide, vel;
};

// Tab 4: FX rack (chorus, delay, reverb)
struct FxTab : juce::Component
{
    explicit FxTab (KubeProcessor& p)
        : chorus ("CHORUS", col::glow), delay ("PING-PONG DELAY", col::accent), reverb ("REVERB", col::second), master ("OUTPUT", col::hot),
          chOn (p.apvts, "chOn", "ON"), chRate (p.apvts, "chRate", "RATE"), chDepth (p.apvts, "chDepth", "DEPTH"), chMix (p.apvts, "chMix", "MIX"),
          dlyOn (p.apvts, "dlyOn", "ON"), dlyTime (p.apvts, "dlyTime", "TIME"), dlyFb (p.apvts, "dlyFb", "FEEDBACK"),
          dlyTone (p.apvts, "dlyTone", "TONE"), dlyMix (p.apvts, "dlyMix", "MIX"),
          revOn (p.apvts, "revOn", "ON"), revSize (p.apvts, "revSize", "SIZE"), revDamp (p.apvts, "revDamp", "DAMP"), revMix (p.apvts, "revMix", "MIX"),
          out (p.apvts, "out", "VOLUME"), scope (p.engine)
    {
        for (juce::Component* c : { (juce::Component*) &chorus, (juce::Component*) &delay, (juce::Component*) &reverb, (juce::Component*) &master })
            addAndMakeVisible (c);
        for (juce::Component* c : { (juce::Component*) &chOn, (juce::Component*) &chRate, (juce::Component*) &chDepth, (juce::Component*) &chMix })
            chorus.addAndMakeVisible (c);
        for (juce::Component* c : { (juce::Component*) &dlyOn, (juce::Component*) &dlyTime, (juce::Component*) &dlyFb, (juce::Component*) &dlyTone,
                                    (juce::Component*) &dlyMix })
            delay.addAndMakeVisible (c);
        for (juce::Component* c : { (juce::Component*) &revOn, (juce::Component*) &revSize, (juce::Component*) &revDamp, (juce::Component*) &revMix })
            reverb.addAndMakeVisible (c);
        master.addAndMakeVisible (out);
        master.addAndMakeVisible (scope);
    }
    void resized() override
    {
        auto r = getLocalBounds().reduced (12, 10);
        auto bottom = r.removeFromBottom (150);
        const int w = (r.getWidth() - 20) / 3;
        chorus.setBounds (r.removeFromLeft (w)); r.removeFromLeft (10);
        delay.setBounds (r.removeFromLeft (w)); r.removeFromLeft (10);
        reverb.setBounds (r);
        bottom.removeFromTop (10);
        master.setBounds (bottom);

        auto lay = [] (Panel& panel, juce::Component& onButton, std::initializer_list<juce::Component*> knobs, juce::Component* extra)
        {
            auto c = panel.getLocalBounds().reduced (10);
            onButton.setBounds (c.removeFromTop (26).removeFromRight (64));
            c.removeFromTop (6);
            if (extra != nullptr) extra->setBounds (c.removeFromTop (44).withSizeKeepingCentre (110, 44));
            auto k = c.withSizeKeepingCentre (c.getWidth(), juce::jmin (c.getHeight(), 100));
            row (k, knobs, 4);
        };
        lay (chorus, chOn, { &chRate, &chDepth, &chMix }, nullptr);
        lay (delay, dlyOn, { &dlyFb, &dlyTone, &dlyMix }, &dlyTime);
        lay (reverb, revOn, { &revSize, &revDamp, &revMix }, nullptr);

        auto m = master.content();
        out.setBounds (m.removeFromLeft (110));
        m.removeFromLeft (10);
        scope.setBounds (m);
    }
    Panel chorus, delay, reverb, master;
    Toggle chOn; Knob chRate, chDepth, chMix;
    Toggle dlyOn; Choice dlyTime; Knob dlyFb, dlyTone, dlyMix;
    Toggle revOn; Knob revSize, revDamp, revMix;
    Knob out;
    Scope scope;
};

//==============================================================================
struct Ui
{
    explicit Ui (KubeProcessor& p)
        : osc (p), filter (p), mod (p), fx (p), headerScope (p.engine), keyboard (p.keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard)
    {
        tabs.addTab ("OSC / WAVE", juce::Colours::transparentBlack, &osc, false);
        tabs.addTab ("FILTER / ENV", juce::Colours::transparentBlack, &filter, false);
        tabs.addTab ("MOD / VOICE", juce::Colours::transparentBlack, &mod, false);
        tabs.addTab ("FX RACK", juce::Colours::transparentBlack, &fx, false);
        tabs.setTabBarDepth (38);
        tabs.setOutline (0);
        tabs.setIndent (0);
        tabs.setComponentID ("tabs");

        keyboard.setColour (juce::MidiKeyboardComponent::whiteNoteColourId, juce::Colour (0xffdcebd0));
        keyboard.setColour (juce::MidiKeyboardComponent::blackNoteColourId, col::bgTop);
        keyboard.setColour (juce::MidiKeyboardComponent::keySeparatorLineColourId, col::bgBottom);
        keyboard.setColour (juce::MidiKeyboardComponent::keyDownOverlayColourId, col::accent);
        keyboard.setColour (juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId, col::glow.withAlpha (0.4f));
        keyboard.setColour (juce::MidiKeyboardComponent::shadowColourId, juce::Colours::transparentBlack);
        keyboard.setColour (juce::MidiKeyboardComponent::upDownButtonBackgroundColourId, col::panel);
        keyboard.setColour (juce::MidiKeyboardComponent::upDownButtonArrowColourId, col::glow);
        keyboard.setAvailableRange (24, 108);
        keyboard.setLowestVisibleKey (36);
        keyboard.setOctaveForMiddleC (4);
        random.getProperties().set ("accent", true);
    }

    OscTab osc;
    FilterTab filter;
    ModTab mod;
    FxTab fx;
    juce::TabbedComponent tabs { juce::TabbedButtonBar::TabsAtTop };
    juce::TextButton prev { "<" }, next { ">" }, presetName, random { "RANDOM" };
    Scope headerScope;
    juce::MidiKeyboardComponent keyboard;
};
} // namespace kube

//==============================================================================
// 5. Main editor: header, tabs, keyboard
KubeEditor::KubeEditor (KubeProcessor& p)
    : AudioProcessorEditor (&p), proc (p), look (std::make_unique<kube::PsyLook>())
{
    setLookAndFeel (look.get());
    ui = std::make_unique<kube::Ui> (p);
    for (juce::Component* c : { (juce::Component*) &ui->tabs, (juce::Component*) &ui->prev, (juce::Component*) &ui->next,
                                (juce::Component*) &ui->presetName, (juce::Component*) &ui->random, (juce::Component*) &ui->headerScope,
                                (juce::Component*) &ui->keyboard })
        addAndMakeVisible (c);

    ui->prev.onClick = [this] { proc.loadPreset (proc.getCurrentPreset() - 1); };
    ui->next.onClick = [this] { proc.loadPreset (proc.getCurrentPreset() + 1); };
    ui->random.onClick = [this] { proc.randomizeAll(); };
    ui->presetName.onClick = [this] { showPresetMenu(); };
    for (juce::Component* tab : { (juce::Component*) &ui->osc, (juce::Component*) &ui->filter, (juce::Component*) &ui->mod, (juce::Component*) &ui->fx })
        tab->setLookAndFeel (look.get()); // the tabs were built before they had a parent to inherit the skin from

    setSize (1000, 780);
    startTimerHz (30);
}

KubeEditor::~KubeEditor()
{
    stopTimer();
    for (juce::Component* tab : { (juce::Component*) &ui->osc, (juce::Component*) &ui->filter, (juce::Component*) &ui->mod, (juce::Component*) &ui->fx })
        tab->setLookAndFeel (nullptr);
    ui.reset();
    setLookAndFeel (nullptr);
}

void KubeEditor::showPresetMenu()
{
    juce::PopupMenu menu;
    std::map<juce::String, juce::PopupMenu> sub;
    juce::StringArray order;
    const auto& list = proc.presetList();
    for (int i = 0; i < (int) list.size(); ++i)
    {
        const auto& c = list[(size_t) i].category;
        if (! order.contains (c)) order.add (c);
        sub[c].addItem (i + 1, list[(size_t) i].name, true, i == proc.getCurrentPreset());
    }
    for (auto& c : order) menu.addSubMenu (c, sub[c]);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&ui->presetName),
                        [safe = juce::Component::SafePointer<KubeEditor> (this)] (int r)
                        {
                            if (safe != nullptr && r > 0) safe->proc.loadPreset (r - 1);
                        });
}

void KubeEditor::paint (juce::Graphics& g)
{
    g.setGradientFill (juce::ColourGradient (kube::col::bgTop, 0, 0, kube::col::bgBottom, 0, (float) getHeight(), false));
    g.fillAll();

    // the spinning neon cube
    const float cx = 46.0f, cy = 40.0f, s = 17.0f;
    const float ay = spin, ax = 0.6f + 0.15f * std::sin (spin * 0.7f);
    juce::Point<float> pts[8];
    for (int i = 0; i < 8; ++i)
    {
        float x = (i & 1) ? 1.0f : -1.0f, y = (i & 2) ? 1.0f : -1.0f, z = (i & 4) ? 1.0f : -1.0f;
        const float x1 = x * std::cos (ay) + z * std::sin (ay), z1 = -x * std::sin (ay) + z * std::cos (ay);
        const float y1 = y * std::cos (ax) - z1 * std::sin (ax);
        pts[i] = { cx + x1 * s, cy + y1 * s };
    }
    juce::Path edges;
    for (int a = 0; a < 8; ++a)
        for (int b = a + 1; b < 8; ++b)
        {
            const int d = a ^ b;
            if (d == 1 || d == 2 || d == 4) { edges.startNewSubPath (pts[a]); edges.lineTo (pts[b]); }
        }
    g.setColour (kube::col::accent.withAlpha (0.3f));
    g.strokePath (edges, juce::PathStrokeType (6.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setGradientFill (juce::ColourGradient (kube::col::glow, cx - s, cy - s, kube::col::accent, cx + s, cy + s, false));
    g.strokePath (edges, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    g.setGradientFill (juce::ColourGradient (kube::col::glow, 86, 0, kube::col::accent, 200, 0, false));
    g.setFont (kube::font (40.0f));
    g.drawText ("KUBE", 84, 8, 140, 44, juce::Justification::centredLeft);
    g.setColour (kube::col::dim);
    g.setFont (kube::font (12.0f, false));
    g.drawText ("by Asher Swissa", 86, 50, 140, 16, juce::Justification::centredLeft);
    g.drawText (juce::String ("VOICES ") + juce::String (proc.engine.uiVoices.load()), 236, 50, 90, 16, juce::Justification::centredLeft);
}

void KubeEditor::resized()
{
    auto r = getLocalBounds();
    auto header = r.removeFromTop (80).reduced (16, 12);
    header.removeFromLeft (320);
    auto presets = header.removeFromLeft (390).withSizeKeepingCentre (390, 34);
    ui->prev.setBounds (presets.removeFromLeft (34));
    presets.removeFromLeft (4);
    ui->random.setBounds (presets.removeFromRight (92));
    presets.removeFromRight (4);
    ui->next.setBounds (presets.removeFromRight (34));
    presets.removeFromRight (4);
    ui->presetName.setBounds (presets);
    header.removeFromLeft (16);
    ui->headerScope.setBounds (header);

    auto keys = r.removeFromBottom (78).reduced (16, 8);
    ui->keyboard.setBounds (keys);
    ui->keyboard.setKeyWidth ((float) keys.getWidth() / 43.0f); // six octaves of white keys
    ui->tabs.setBounds (r.reduced (6, 0));
}

void KubeEditor::timerCallback()
{
    // Only what moves gets redrawn: a full repaint 30 times a second froze hosts on HiDPI screens
    const bool cubeFrame = (++frame & 1) == 0; // the cube spins at 15 fps
    if (cubeFrame) spin += 0.05f;
    if (ui->presetName.getButtonText() != proc.getPresetLabel())
        ui->presetName.setButtonText (proc.getPresetLabel());
    ui->mod.update();
    ui->headerScope.tick();
    std::function<void (juce::Component&)> tickAll = [&] (juce::Component& c)
    {
        for (auto* child : c.getChildren())
        {
            if (auto* live = dynamic_cast<kube::LiveView*> (child)) live->tick();
            else tickAll (*child);
        }
    };
    if (auto* tab = ui->tabs.getCurrentContentComponent())
        tickAll (*tab);
    if (cubeFrame) repaint (14, 10, 66, 64);
    const int voices = proc.engine.uiVoices.load();
    if (voices != shownVoices) { shownVoices = voices; repaint (236, 50, 90, 16); }
}
