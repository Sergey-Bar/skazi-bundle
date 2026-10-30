#include "PluginEditor.h"

using namespace juce;
using namespace midigod;

namespace
{
    Rectangle<float> screen (Graphics& g, Rectangle<float> b)
    {
        skin::drawInsetPanel (g, b, 6.0f);
        auto in = b.reduced (7.0f);
        g.setColour (Colour (0xff070707));
        g.fillRoundedRectangle (in, 4.0f);
        return in;
    }

    void tag (Graphics& g, Rectangle<float> in, const String& text, Colour c = skin::red2)
    {
        g.setFont (skin::font (10.5f));
        const float tw = jmin (in.getWidth() - 12.0f, GlyphArrangement::getStringWidth (g.getCurrentFont(), text) + 14.0f);
        g.setColour (Colours::black.withAlpha (0.65f));
        g.fillRoundedRectangle (in.getX() + 6, in.getY() + 6, tw, 17, 4);
        g.setColour (c);
        g.drawText (text, (int) in.getX() + 13, (int) in.getY() + 6, (int) tw - 10, 17, Justification::left, true);
    }

    String noteName (int p) { return String (keyName (p)) + String (p / 12 - 2); } // C3 = 60

    String drumName (int p)
    {
        switch (p)
        {
            case KickNote: return "KICK";
            case RimNote: return "RIM";
            case SnareNote: return "SNARE";
            case ClapNote: return "CLAP";
            case HatNote: return "HAT";
            case OpenHatNote: return "OPEN";
            case RideNote: return "RIDE";
            case ShakerNote: return "SHAKE";
            default: return noteName (p);
        }
    }

    const String dot = String::fromUTF8 ("  \xc2\xb7  ");
}

//==============================================================================
void PianoRoll::paint (Graphics& g)
{
    auto in = screen (g, getLocalBounds().toFloat());
    const auto pat = proc.pattern();
    const double L = pat->lengthBeats;
    auto area = in.reduced (8.0f, 0.0f).withTrimmedTop (30.0f).withTrimmedBottom (8.0f);
    auto keys = area.removeFromLeft (40.0f);

    // rows: every pitch from lowest to highest, or one row per drum sound
    std::vector<int> rows;
    if (pat->drums)
    {
        for (auto& n : pat->notes) if (std::find (rows.begin(), rows.end(), n.pitch) == rows.end()) rows.push_back (n.pitch);
        std::sort (rows.rbegin(), rows.rend());
    }
    else if (! pat->notes.empty())
    {
        int lo = 127, hi = 0;
        for (auto& n : pat->notes) { lo = jmin (lo, n.pitch); hi = jmax (hi, n.pitch); }
        lo -= 2; hi += 2;
        while (hi - lo < 14) { --lo; ++hi; }
        for (int p = hi; p >= lo; --p) rows.push_back (p);
    }
    if (rows.empty())
    {
        tag (g, in, pat->title);
        g.setColour (skin::textDim);
        g.setFont (skin::font (13.0f));
        g.drawText ("NO NOTES", area, Justification::centred);
        return;
    }

    const float rowH = area.getHeight() / (float) rows.size();
    for (size_t r = 0; r < rows.size(); ++r)
    {
        const int p = rows[r];
        const auto row = Rectangle<float> (area.getX(), area.getY() + r * rowH, area.getWidth(), rowH);
        const int pc = p % 12;
        const bool isBlack = ! pat->drums && (pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10);
        g.setColour (isBlack ? Colour (0xff0c0c0e) : Colour (0xff121215));
        g.fillRect (row);
        g.setColour (Colours::white.withAlpha (0.04f));
        g.drawHorizontalLine ((int) row.getBottom(), area.getX(), area.getRight());
        if (pat->drums || pc == 0 || rowH >= 11.0f)
        {
            g.setColour (pat->drums || pc == 0 ? skin::textDim : skin::textDim.withAlpha (0.5f));
            g.setFont (skin::font (jlimit (7.5f, 9.5f, rowH * 0.8f)));
            g.drawText (pat->drums ? drumName (p) : noteName (p), keys.withY (row.getY()).withHeight (rowH).toNearestInt(), Justification::centredLeft);
        }
    }

    // beats and bars
    const float w = area.getWidth();
    const bool manyBars = L > 64.0;
    for (int b = 0; b <= (int) L; ++b)
    {
        if (manyBars && b % 4 != 0) continue;
        const float x = area.getX() + (float) (b / L) * w;
        g.setColour (Colours::white.withAlpha (b % 16 == 0 ? 0.22f : b % 4 == 0 ? 0.13f : 0.05f));
        g.drawVerticalLine ((int) x, area.getY(), area.getBottom());
    }

    auto rowY = [&] (int pitch)
    {
        const auto it = std::find (rows.begin(), rows.end(), pitch);
        return it == rows.end() ? -1.0f : area.getY() + (float) (it - rows.begin()) * rowH + rowH * 0.5f;
    };
    // slides: a line from the note into the one it glides to; dips: a tail falling off the note's end
    g.saveState();
    g.reduceClipRegion (area.toNearestInt());
    for (auto& n : pat->notes)
    {
        const float y0 = rowY (n.pitch);
        if (y0 < 0.0f) continue;
        const float x1 = area.getX() + (float) ((n.start + n.length) / L) * w;
        if (n.slide)
            for (auto& o : pat->notes)
                if (o.pitch != n.pitch && o.start > n.start && o.start < n.start + n.length)
                {
                    const float xs = area.getX() + (float) (o.start / L) * w, y1 = rowY (o.pitch);
                    g.setColour (Colour (0xffff8a7a).withAlpha (0.8f));
                    g.drawLine (xs - 3.0f, y0, xs + 2.0f, y1, 1.6f);
                    break;
                }
        if (n.dip > 0.0f)
        {
            const float xa = area.getX() + (float) ((n.start + n.length * 0.4) / L) * w;
            const float drop = n.dip * kBendRangeSemis * rowH;
            Path tail;
            tail.startNewSubPath (xa, y0);
            tail.quadraticTo (x1 - 1.0f, y0, x1, y0 + drop);
            g.setColour (Colour (0xffff8a7a).withAlpha (0.75f));
            g.strokePath (tail, PathStrokeType (1.5f));
        }
    }
    g.restoreState();

    for (auto& n : pat->notes)
    {
        const auto it = std::find (rows.begin(), rows.end(), n.pitch);
        if (it == rows.end()) continue;
        const float y = area.getY() + (float) (it - rows.begin()) * rowH;
        const float x = area.getX() + (float) (n.start / L) * w;
        const float nw = jmax (2.5f, (float) (n.length / L) * w - 1.0f);
        auto r = Rectangle<float> (x, y + 1.0f, nw, jmax (2.0f, rowH - 2.0f));
        const float a = 0.35f + 0.65f * n.velocity / 127.0f;
        g.setColour (skin::red2.withAlpha (a));
        g.fillRoundedRectangle (r, 2.0f);
        g.setColour (Colour (0xffff8a7a).withAlpha (a));
        g.drawRoundedRectangle (r, 2.0f, 0.8f);
    }

    const double beat = proc.uiBeat.load();
    if (beat >= 0.0)
    {
        const float x = area.getX() + (float) (beat / L) * w;
        g.setColour (Colours::white.withAlpha (0.85f));
        g.drawVerticalLine ((int) x, area.getY(), area.getBottom());
    }

    tag (g, in, pat->title);
    g.setFont (skin::font (10.0f));
    g.setColour (skin::textDim);
    g.drawText ((pat->detail.isNotEmpty() ? pat->detail + dot : String()) + String ((int) pat->notes.size()) + " NOTES" + dot + String (roundToInt (proc.tempo())) + " BPM",
                in.withTrimmedRight (12.0f).withHeight (28.0f).toNearestInt(), Justification::centredRight);
}

//==============================================================================
void GodButton::paintButton (Graphics& g, bool over, bool down)
{
    auto r = getLocalBounds().toFloat().reduced (3.0f);
    const float glow = jmax (flash, over ? 0.35f : 0.0f);
    if (glow > 0.0f)
    {
        g.setColour (skin::red.withAlpha (0.25f * glow + 0.1f));
        g.fillRoundedRectangle (r.expanded (3.0f), 12.0f);
    }
    g.setGradientFill (ColourGradient (skin::red2.brighter (0.25f * flash), r.getX(), r.getY(), Colour (0xff8a0400), r.getX(), r.getBottom(), false));
    g.fillRoundedRectangle (down ? r.reduced (1.5f) : r, 10.0f);
    g.setColour (Colours::white.withAlpha (0.18f));
    g.fillRoundedRectangle (r.reduced (5.0f).withHeight (r.getHeight() * 0.35f), 8.0f);
    g.setColour (skin::red2.brighter (0.4f));
    g.drawRoundedRectangle (r, 10.0f, 1.2f);

    g.setColour (Colours::white);
    g.setFont (skin::titleFont (46.0f));
    g.drawText ("GOD MODE", r.withTrimmedBottom (r.getHeight() * 0.28f), Justification::centredBottom);
    g.setColour (Colours::white.withAlpha (0.85f));
    g.setFont (skin::font (11.0f));
    g.drawText ("NEW VARIATION", r.withTrimmedTop (r.getHeight() * 0.72f).withTrimmedBottom (8.0f), Justification::centredTop);
}

//==============================================================================
void DragTile::paint (Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    skin::drawInsetPanel (g, b, 6.0f);
    auto in = b.reduced (7.0f);
    const bool over = isMouseOver (true) || dragging;
    g.setColour (over ? Colour (0xff1e1012) : Colour (0xff0b0b0c));
    g.fillRoundedRectangle (in, 5.0f);
    g.setColour (over ? skin::red2 : skin::line);
    g.drawRoundedRectangle (in.reduced (0.5f), 5.0f, 1.2f);

    // a little MIDI clip with an arrow leaving it
    auto icon = in.removeFromLeft (in.getHeight()).reduced (14.0f);
    g.setColour (skin::red2);
    g.drawRoundedRectangle (icon, 3.0f, 1.5f);
    for (int i = 0; i < 4; ++i)
        g.fillRect (icon.getX() + 4.0f + i * icon.getWidth() * 0.21f, icon.getY() + icon.getHeight() * (0.25f + 0.15f * ((i * 3) % 4)), icon.getWidth() * 0.16f, 3.0f);
    Path arrow;
    arrow.addArrow ({ icon.getRight() - 4.0f, icon.getBottom() - 4.0f, icon.getRight() + 10.0f, icon.getBottom() + 8.0f }, 2.0f, 8.0f, 6.0f);
    g.setColour (skin::white);
    g.fillPath (arrow);

    g.setColour (skin::white);
    g.setFont (skin::titleFont (30.0f));
    g.drawText ("DRAG MIDI", in.withTrimmedBottom (in.getHeight() * 0.42f), Justification::centredLeft);
    g.setColour (skin::textDim);
    g.setFont (skin::font (10.0f));
    g.drawText ("ONTO A TRACK IN YOUR DAW", in.withTrimmedTop (in.getHeight() * 0.58f).withTrimmedBottom (10.0f), Justification::centredLeft);
}

void DragTile::mouseDrag (const MouseEvent& e)
{
    if (dragging || e.getDistanceFromDragStart() < 6) return;
    const auto file = proc.writeMidiFile();
    if (! file.existsAsFile()) return;
    dragging = true;
    repaint();
    SafePointer<DragTile> safe (this);
    DragAndDropContainer::performExternalDragDropOfFiles ({ file.getFullPathName() }, false, this, [safe]
    {
        if (safe != nullptr) { safe->dragging = false; safe->repaint(); }
    });
}

//==============================================================================
void DropZone::paint (Graphics& g)
{
    auto b = getLocalBounds().toFloat().reduced (2.0f);
    const bool busy = proc.isAnalysing();
    g.setColour (hover ? Colour (0xff1e1012) : Colour (0xff0b0b0c));
    g.fillRoundedRectangle (b, 6.0f);

    Path outline;
    outline.addRoundedRectangle (b, 6.0f);
    Path dashed;
    const float dashes[] = { 6.0f, 4.0f };
    PathStrokeType (1.4f).createDashedStroke (dashed, outline, dashes, 2);
    g.setColour (hover || busy ? skin::red2 : proc.isFromAudio() ? Colour (0xff22e3a5) : skin::textDim.withAlpha (0.6f));
    g.fillPath (dashed);

    auto in = b.reduced (12.0f, 8.0f);
    g.setColour (skin::white);
    g.setFont (skin::titleFont (26.0f));
    g.drawText (busy ? "LISTENING..." : hover ? "LET GO TO CONVERT" : "DROP AUDIO HERE", in.removeFromTop (32.0f), Justification::centred);

    g.setFont (skin::font (9.5f));
    g.setColour (proc.isFromAudio() && ! busy ? Colour (0xff22e3a5) : skin::textDim);
    g.drawFittedText (proc.getAudioStatus(), in.removeFromTop (30.0f).toNearestInt(), Justification::centred, 2);

    if (busy)
    {
        auto bar = in.removeFromTop (6.0f).reduced (40.0f, 0.0f);
        g.setColour (Colour (0xff141416));
        g.fillRoundedRectangle (bar, 3.0f);
        const float x = bar.getX() + (0.5f + 0.5f * std::sin (phase)) * bar.getWidth() * 0.7f;
        g.setColour (skin::red2);
        g.fillRoundedRectangle (x, bar.getY(), bar.getWidth() * 0.3f, bar.getHeight(), 3.0f);
    }
    else
    {
        g.setColour (skin::textDim.withAlpha (0.7f));
        g.drawText ("WAV  " + String::fromUTF8 ("\xc2\xb7") + "  MP3  " + String::fromUTF8 ("\xc2\xb7") + "  FLAC  " + String::fromUTF8 ("\xc2\xb7")
                        + "  AIFF  " + String::fromUTF8 ("\xc2\xb7") + "  OGG" + dot + "CLICK TO BROWSE",
                    in.removeFromTop (16.0f).toNearestInt(), Justification::centred);
    }
}

//==============================================================================
MidiGodEditor::MidiGodEditor (MidiGodProcessor& p)
    : EditorBase (p, "MIDI ", "GOD", String::fromUTF8 ("MIDI ARCHITECT + AUDIO TO MIDI  \xc2\xb7  BY ASHER SWISSA"),
                  String::fromUTF8 ("MIDI GOD v1.1  \xc2\xb7  ROLL BASS \xc2\xb7 AUDIO TO MIDI  \xc2\xb7  VST3 / AU  \xc2\xb7  INSTRUMENT + MIDI OUT")),
      mp (p), roll (p), dragTile (p), dropZone (p)
{
    setSubtitleSize (15.0f);

    addAndMakeVisible (roll);
    addAndMakeVisible (godBtn);
    godBtn.setTooltip ("A new variation of this genre and role");
    godBtn.onClick = [this] { mp.godMode(); godBtn.flash = 1.0f; };

    addAndMakeVisible (dragTile);
    dragTile.setTooltip ("Drag the loop onto a track in Ableton, FL Studio, Cubase, Logic, Bitwig or Reaper");

    addAndMakeVisible (saveBtn);
    saveBtn.setTooltip ("Save the loop as a .mid file");
    saveBtn.onClick = [this] { saveMidi(); };
    addAndMakeVisible (mutateBtn);
    mutateBtn.setTooltip ("Keeps the bass motif and changes a quarter of its steps. GOD MODE starts a new one");
    mutateBtn.onClick = [this] { mp.mutate(); };
    addAndMakeVisible (genBtn);
    genBtn.setTooltip ("Leave the converted audio and go back to the generator");
    genBtn.onClick = [this] { mp.backToGenerator(); };

    addAndMakeVisible (dropZone);
    dropZone.setTooltip ("Drop a WAV, MP3, FLAC, AIFF or OGG file, or click to browse. Uses the project tempo");
    dropZone.onClick = [this] { browseAudio(); };

    const auto red = skin::red2, amber = Colour (0xffffb000), green = Colour (0xff22e3a5), blue = Colour (0xff7aa2ff);

    genreRow = &addChoiceRow ("genre");
    roleRow = &addChoiceRow ("role", {}, amber);
    keyRow = &addChoiceRow ("key", {}, green);
    barsRow = &addChoiceRow ("bars");
    gridRow = &addChoiceRow ("grid");
    scaleBox = &addCombo ("scale");

    density = &addKnob ("density", "DENSITY", red, "%", 34);
    swing = &addKnob ("swing", "SWING", red, "%", 34);
    gate = &addKnob ("gate", "GATE", red, "%", 34);
    dyn = &addKnob ("dyn", "DYNAMICS", amber, "%", 34);
    move = &addKnob ("move", "MOVEMENT", amber, "%", 34);
    human = &addKnob ("human", "HUMANIZE", amber, "%", 34);
    octave = &addKnob ("octave", "OCTAVE", green, "oct", 34);
    seed = &addKnob ("seed", "VARIATION", green, "", 34);
    swing->setFormatter ([] (double v) { return v < 50.5 ? String ("STRAIGHT") : String (roundToInt (v)) + "%"; });
    seed->setFormatter ([] (double v) { return "#" + String (roundToInt (v)); });

    const auto pink = Colour (0xffff4fd8);
    styleRow = &addChoiceRow ("rbStyle", {}, pink);
    rbSync = &addKnob ("rbSync", "SYNCOPATION", pink, "%", 34);
    rbKick = &addKnob ("rbKick", "KICK AVOID", pink, "%", 34);
    rbOct = &addKnob ("rbOct", "OCTAVE JUMP", pink, "%", 34);
    rbDip = &addKnob ("rbDip", "PITCH DIP", blue, "%", 34);
    rbGlide = &addKnob ("rbGlide", "GLIDE", blue, "%", 34);
    rbGroove = &addKnob ("rbGroove", "GROOVE", blue, "%", 34);
    rbVar = &addKnob ("rbVar", "VARIATION", amber, "%", 34);
    rbEvolve = &addKnob ("rbEvolve", "EVOLVE", pink, "%", 56);

    modeRow = &addChoiceRow ("a2mMode", {}, green);
    quantRow = &addChoiceRow ("a2mQuant", {}, green);
    snap = &addToggle ("a2mSnap", "SNAP TO KEY");
    snap->setTooltip ("Moves converted notes onto the key and scale picked above");
    sens = &addKnob ("a2mSens", "SENSITIVITY", green, "%", 34);

    midiOut = &addToggle ("midiOut", "MIDI OUT");
    midiOut->setTooltip ("Sends the loop out as MIDI, so another instrument can play it (route this track's MIDI to it)");
    freeRun = &addToggle ("freeRun", "FREE RUN");
    freeRun->setTooltip ("Keeps playing with the DAW stopped, to audition patterns");
    vol = &addKnob ("vol", "PREVIEW", blue, "dB", 40);
    vol->setFormatter ([] (double v) { return v <= -59.9 ? String ("OFF") : skazi::formatValue (v, "dB"); });

    setSize (1200, 858);
}

void MidiGodEditor::layout()
{
    roll.setBounds (20, 68, 870, 250);
    godBtn.setBounds (898, 68, 282, 110);
    dragTile.setBounds (898, 186, 282, 84);
    saveBtn.setBounds (898, 278, 137, 40);
    genBtn.setBounds (1043, 278, 137, 40);

    int y = 326;
    panel ({ 20, y, 1160, 178 }, "GENERATOR");
    genreRow->setBounds (110, y + 6, 1060, 22);
    label ({ 30, y + 35, 40, 20 }, "ROLE");
    roleRow->setBounds (70, y + 34, 600, 22);
    label ({ 684, y + 35, 40, 20 }, "BARS");
    barsRow->setBounds (718, y + 34, 250, 22);
    label ({ 980, y + 35, 40, 20 }, "GRID");
    gridRow->setBounds (1012, y + 34, 158, 22);
    label ({ 30, y + 63, 40, 20 }, "KEY");
    keyRow->setBounds (70, y + 62, 600, 22);
    label ({ 684, y + 63, 50, 20 }, "SCALE");
    scaleBox->setBounds (728, y + 61, 170, 24);
    int kx = 38;
    for (auto* k : { density, swing, gate, dyn, move, human, octave, seed }) { put (*k, kx, y + 100, 34, 78); kx += 142; }

    y = 512;
    panel ({ 20, y, 1160, 150 }, "ROLL BASS");
    styleRow->setBounds (120, y + 6, 560, 22);
    label ({ 700, y + 8, 300, 16 }, "PICK THE BASS ROLE - DRAG MIDI KEEPS GLIDE + DIPS");
    int rx = 38;
    for (auto* k : { rbSync, rbKick, rbOct, rbDip, rbGlide, rbGroove, rbVar }) { put (*k, rx, y + 44, 34, 84); rx += 112; }
    put (*rbEvolve, 838, y + 34, 56, 92);
    mutateBtn.setBounds (980, y + 44, 186, 44);
    label ({ 980, y + 98, 190, 14 }, "YOUR SYNTH: MONO + LEGATO GLIDE");
    label ({ 980, y + 112, 190, 14 }, "PITCH BEND RANGE 12");

    y = 670;
    panel ({ 20, y, 860, 144 }, "AUDIO TO MIDI");
    dropZone.setBounds (30, y + 30, 420, 106);
    label ({ 466, y + 32, 50, 20 }, "MODE");
    modeRow->setBounds (530, y + 30, 340, 24);
    label ({ 466, y + 64, 60, 20 }, "QUANTIZE");
    quantRow->setBounds (530, y + 62, 340, 24);
    snap->setBounds (466, y + 102, 110, 24);
    put (*sens, 640, y + 88, 30, 90);
    label ({ 742, y + 98, 140, 14 }, "USES THE PROJECT TEMPO");
    label ({ 742, y + 112, 140, 14 }, "CHANGE MODE TO RE-SCAN");

    panel ({ 888, y, 292, 144 }, "PLAYBACK");
    midiOut->setBounds (900, y + 34, 128, 26);
    freeRun->setBounds (900, y + 68, 128, 26);
    label ({ 900, y + 104, 160, 14 }, "PREVIEW = BUILT-IN SOUND");
    label ({ 900, y + 118, 160, 14 }, "MIDI OUT = YOUR SYNTH");
    put (*vol, 1074, y + 34, 40, 90);
}

void MidiGodEditor::tick()
{
    roll.repaint();
    dropZone.phase += 0.15f;
    dropZone.repaint();
    if (godBtn.flash > 0.0f)
    {
        godBtn.flash = jmax (0.0f, godBtn.flash - 0.06f);
        godBtn.repaint();
    }

    // generator controls step back while the loop comes from audio
    const bool audio = mp.isFromAudio();
    const float a = audio ? 0.45f : 1.0f;
    for (Component* c : std::initializer_list<Component*> { barsRow, gridRow, density, swing, gate, dyn, move, human })
        c->setAlpha (a);
    // ROLL BASS works on the BASS role; AUTO in the house genres keeps the house grooves, where only PITCH DIP applies
    const bool bassRole = (int) mp.v ("role") == Bass && ! audio;
    const int genre = (int) mp.v ("genre");
    const bool houseGroove = (genre == TechHouse || genre == DeepHouse || genre == BassHouse) && (int) mp.v ("rbStyle") == RollAuto && (int) mp.v ("grid") == 0;
    styleRow->setAlpha (bassRole ? 1.0f : 0.45f);
    rbDip->setAlpha (bassRole ? 1.0f : 0.45f);
    for (Component* c : std::initializer_list<Component*> { rbSync, rbKick, rbOct, rbGlide, rbGroove, rbVar, rbEvolve })
        c->setAlpha (bassRole && ! houseGroove ? 1.0f : 0.45f);
    mutateBtn.setEnabled (bassRole && ! houseGroove);
    genBtn.setEnabled (audio);
    genBtn.setAlpha (audio ? 1.0f : 0.45f);
    const bool drums = (int) mp.v ("role") == Drums && ! audio;
    keyRow->setAlpha (drums ? 0.45f : 1.0f);
    scaleBox->setAlpha (drums ? 0.45f : 1.0f);
}

//==============================================================================
bool MidiGodEditor::isInterestedInFileDrag (const StringArray& files)
{
    for (auto& f : files) if (isAudioFile (f)) return true;
    return false;
}

void MidiGodEditor::filesDropped (const StringArray& files, int, int)
{
    dropZone.hover = false;
    for (auto& f : files)
        if (isAudioFile (f)) { mp.convertAudio (File (f)); return; }
}

void MidiGodEditor::browseAudio()
{
    chooser = std::make_unique<FileChooser> ("Audio to convert to MIDI", File::getSpecialLocation (File::userMusicDirectory),
                                             "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.ogg;*.m4a");
    SafePointer<MidiGodEditor> safe (this);
    chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles, [safe] (const FileChooser& fc)
    {
        if (safe != nullptr && fc.getResult().existsAsFile()) safe->mp.convertAudio (fc.getResult());
    });
}

void MidiGodEditor::saveMidi()
{
    const auto name = File::createLegalFileName (mp.pattern()->fileStem) + ".mid";
    chooser = std::make_unique<FileChooser> ("Save MIDI", File::getSpecialLocation (File::userMusicDirectory).getChildFile (name), "*.mid");
    SafePointer<MidiGodEditor> safe (this);
    chooser->launchAsync (FileBrowserComponent::saveMode | FileBrowserComponent::canSelectFiles | FileBrowserComponent::warnAboutOverwriting,
                          [safe] (const FileChooser& fc)
    {
        auto f = fc.getResult();
        if (safe == nullptr || f == File()) return;
        if (! f.hasFileExtension ("mid")) f = f.withFileExtension ("mid");
        safe->mp.saveMidiFile (f);
    });
}
