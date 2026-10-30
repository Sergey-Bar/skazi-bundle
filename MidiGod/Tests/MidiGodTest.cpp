// Checks the generator over every genre / role / length, and audio-to-MIDI on rendered loops whose notes are known.
//   MidiGodTest [output folder]   writes every factory loop as .mid plus one audition .wav per genre
#include <juce_audio_formats/juce_audio_formats.h>
#include <cstring>
#include <set>
#include "../Source/AudioToMidi.h"
#include "../Source/Pattern.h"
#include "../Source/Presets.h"
#include "../Source/PreviewSynth.h"

using namespace midigod;

static int failures = 0;
static void check (bool ok, const juce::String& what)
{
    if (! ok) { ++failures; std::printf ("FAIL: %s\n", what.toRawUTF8()); }
}

static Settings settingsFor (const skazi::Preset& p, int key = 6)
{
    auto values = p.values;
    values["key"] = (float) key;
    return Settings::fromValues ([&] (const char* id) { return values[id]; });
}

// The loop played `loops` times on the preview synth
static juce::AudioBuffer<float> render (const Pattern& p, int role, double bpm, double sr, int loops)
{
    PreviewSynth synth;
    synth.prepare (sr);
    synth.setSound (role, p.drums);
    struct Ev { double t; bool on; int note, vel; bool bend = false; float value = 0.0f; };
    std::vector<Ev> ev;
    const double spb = 60.0 / bpm;
    for (int k = 0; k < loops; ++k)
        for (auto& n : p.notes)
        {
            ev.push_back ({ (n.start + k * p.lengthBeats) * spb, true, n.pitch, n.velocity });
            ev.push_back ({ (n.start + n.length + k * p.lengthBeats) * spb, false, n.pitch, 0 });
        }
    for (int k = 0; k < loops; ++k)
        for (auto& b : p.bends)
            ev.push_back ({ (b.beat + k * p.lengthBeats) * spb, false, 0, 0, true, b.value });
    std::stable_sort (ev.begin(), ev.end(), [] (const Ev& a, const Ev& b) { return a.t != b.t ? a.t < b.t : (! a.on && b.on); });
    const int total = (int) ((loops * p.lengthBeats * spb + 0.6) * sr);
    juce::AudioBuffer<float> buf (2, total);
    buf.clear();
    int pos = 0;
    for (auto& e : ev)
    {
        const int at = juce::jlimit (0, total, (int) (e.t * sr));
        if (at > pos) { synth.render (buf.getWritePointer (0, pos), buf.getWritePointer (1, pos), at - pos, 1.0f); pos = at; }
        if (e.bend) synth.setBend (e.value * kBendRangeSemis);
        else if (e.on) synth.noteOn (e.note, e.vel); else synth.noteOff (e.note);
    }
    if (total > pos) synth.render (buf.getWritePointer (0, pos), buf.getWritePointer (1, pos), total - pos, 1.0f);
    return buf;
}

static void writeWav (const juce::File& f, const juce::AudioBuffer<float>& buf, double sr)
{
    f.deleteFile();
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (new juce::FileOutputStream (f), sr, 2, 24, {}, 0));
    if (w != nullptr) w->writeFromAudioSampleBuffer (buf, 0, buf.getNumSamples());
}

struct Score { int truth = 0, found = 0, matched = 0; };

// Notes match when the pitch (or drum class) agrees and the start is within `tolerance` beats
static Score score (const std::vector<Note>& truth, const std::vector<Note>& found, double tolerance, std::function<int (int)> cls)
{
    // two hits of the same class at the same moment (a closed and an open hat) are one sound
    std::vector<Note> want;
    for (auto& t : truth)
        if (std::none_of (want.begin(), want.end(), [&] (const Note& w) { return cls (w.pitch) == cls (t.pitch) && std::abs (w.start - t.start) < 1.0e-6; }))
            want.push_back (t);
    Score s { (int) want.size(), (int) found.size(), 0 };
    std::vector<bool> used (found.size(), false);
    for (auto& t : want)
        for (size_t i = 0; i < found.size(); ++i)
            if (! used[i] && cls (found[i].pitch) == cls (t.pitch) && std::abs (found[i].start - t.start) <= tolerance)
            {
                used[i] = true;
                ++s.matched;
                break;
            }
    return s;
}

static void report (const char* name, const Score& s, double minRecall, double minPrecision, double ms)
{
    const double recall = s.truth > 0 ? s.matched / (double) s.truth : 0.0;
    const double precision = s.found > 0 ? s.matched / (double) s.found : 0.0;
    std::printf ("  %-28s %3d notes, found %3d, matched %3d  recall %3.0f%%  precision %3.0f%%  (%.0f ms)\n",
                 name, s.truth, s.found, s.matched, recall * 100.0, precision * 100.0, ms);
    check (recall >= minRecall, juce::String (name) + " recall");
    check (precision >= minPrecision, juce::String (name) + " precision");
}

// One loop of a factory preset, converted back from its own audio
static void roundTrip (const char* name, int genre, int role, int mode, double minRecall, double minPrecision, bool withKick = false)
{
    const auto& list = presets();
    const auto it = std::find_if (list.begin(), list.end(), [&] (const skazi::Preset& p)
    {
        return (int) p.values.at ("genre") == genre && (int) p.values.at ("role") == role;
    });
    const auto s = settingsFor (*it);
    const auto pat = generate (s);
    const double bpm = genreInfo (genre).bpm, sr = 44100.0;
    auto audio = render (pat, role, bpm, sr, 1);
    if (withKick)
    {
        Settings ds = s;
        ds.role = Drums;
        auto kit = generate (ds);
        kit.notes.erase (std::remove_if (kit.notes.begin(), kit.notes.end(), [] (const Note& n) { return n.pitch != KickNote; }), kit.notes.end());
        auto k = render (kit, Drums, bpm, sr, 1);
        for (int ch = 0; ch < 2; ++ch) audio.addFrom (ch, 0, k, ch, 0, juce::jmin (audio.getNumSamples(), k.getNumSamples()), 0.8f);
    }

    const auto t0 = juce::Time::getMillisecondCounterHiRes();
    auto res = convertAudio (audio, sr, mode, 0.5f, bpm);
    const auto ms = juce::Time::getMillisecondCounterHiRes() - t0;
    check (res.error.isEmpty(), juce::String (name) + ": " + res.error);
    Finish f;
    f.quantize = role == Drums ? 0.0 : 0.25;
    f.drums = res.drums;
    const auto raw = res.notes;
    finishNotes (res.notes, res.lengthBeats, f);

    // MG_DEBUG=<part of a test name> lists what was played and what was found
    if (auto* dbg = std::getenv ("MG_DEBUG"))
        if (std::strstr (name, dbg) != nullptr)
        {
            auto list = [] (const char* title, const std::vector<Note>& v)
            {
                std::printf ("    %s:", title);
                for (auto& n : v) std::printf (" %d@%.2f/%.2f", n.pitch, n.start, n.length);
                std::printf ("\n");
            };
            list ("played", pat.notes);
            list ("raw   ", raw);
            list ("found ", res.notes);
        }

    if (mode == ChordTrack)
    {
        // per bar: how many pitch classes of the chord played in it the detected chord shares
        int ok = 0, bars = (int) (pat.lengthBeats / 4.0);
        for (int b = 0; b < bars; ++b)
        {
            std::set<int> want, got;
            for (auto& n : pat.notes) if (n.start >= b * 4.0 && n.start < b * 4.0 + 4.0) want.insert (n.pitch % 12);
            for (auto& n : res.notes) if (n.start < b * 4.0 + 4.0 && n.start + n.length > b * 4.0) got.insert (n.pitch % 12);
            int shared = 0;
            for (int pc : got) shared += (int) want.count (pc);
            ok += shared >= 3 ? 1 : 0;
        }
        std::printf ("  %-28s %d of %d bars with the right triad  (%.0f ms)\n", name, ok, bars, ms);
        check (ok >= bars - 1, juce::String (name) + " chords");
        return;
    }

    auto drumClass = [] (int p)
    {
        if (p == KickNote) return 0;
        if (p == SnareNote || p == ClapNote) return 1;
        if (p == HatNote || p == OpenHatNote || p == RideNote) return 2;
        return 3;
    };
    std::vector<Note> truth = pat.notes;
    if (role == Drums)
        truth.erase (std::remove_if (truth.begin(), truth.end(), [&] (const Note& n) { return drumClass (n.pitch) == 3; }), truth.end());
    report (name, score (truth, res.notes, role == Drums ? 0.08 : 0.13, role == Drums ? std::function<int (int)> (drumClass) : [] (int p) { return p; }),
            minRecall, minPrecision, ms);
}

int main (int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI init;
    const juce::File outDir = argc > 1 ? juce::File::getCurrentWorkingDirectory().getChildFile (argv[1]) : juce::File();

    // 1. every genre x role x length x grid x a few variations makes a valid loop
    int loops = 0;
    for (int g = 0; g < NumGenres; ++g)
        for (int r = 0; r < NumRoles; ++r)
            for (int bars : { 1, 2, 4, 8 })
                for (int grid = 0; grid < 2; ++grid)
                    for (int seed = 1; seed <= 6; ++seed)
                    {
                        Settings s;
                        s.genre = g; s.role = r; s.bars = bars; s.triplet = grid == 1; s.seed = seed * 37;
                        s.key = seed % 12; s.scale = seed % NumScales; s.density = 0.15f * seed; s.movement = 0.2f * (seed % 5);
                        s.swing = seed % 2 == 0 ? 0.58f : 0.5f; s.humanize = seed == 3 ? 0.5f : 0.0f;
                        const auto p = generate (s);
                        const juce::String where = juce::String (genreInfo (g).title) + " " + roleTitle (r) + " " + juce::String (bars) + " bars seed " + juce::String (s.seed);
                        check (! p.notes.empty(), where + ": no notes");
                        check (p.lengthBeats == bars * 4.0, where + ": length");
                        for (size_t i = 0; i < p.notes.size(); ++i)
                        {
                            const auto& n = p.notes[i];
                            check (n.pitch >= 0 && n.pitch < 128 && n.velocity >= 1 && n.velocity <= 127, where + ": note range");
                            check (n.start >= 0.0 && n.start < p.lengthBeats && n.length > 0.0 && n.start + n.length <= p.lengthBeats + 1.0e-6, where + ": timing");
                            for (size_t j = i + 1; j < p.notes.size(); ++j)
                                if (p.notes[j].pitch == n.pitch && p.notes[j].start < n.start + n.length - 1.0e-6 && p.notes[j].start >= n.start)
                                    check (false, where + ": same-pitch overlap");
                        }
                        const auto again = generate (s);
                        check (again.notes.size() == p.notes.size(), where + ": not repeatable");
                        ++loops;
                    }
    std::printf ("generator: %d loops checked\n", loops);

    // a typical loop of each role, to eyeball density
    for (int r = 0; r < NumRoles; ++r)
    {
        std::printf ("  %-8s", roleTitle (r));
        for (int g = 0; g < NumGenres; ++g)
        {
            Settings s; s.genre = g; s.role = r; s.bars = 4;
            std::printf (" %4d", (int) generate (s).notes.size());
        }
        std::printf ("\n");
    }

    // 2. MIDI file export
    {
        Settings s; s.genre = Trance; s.role = Lead;
        const auto p = generate (s);
        const auto mf = toMidiFile (p, 138.0);
        int ons = 0;
        for (auto* e : *mf.getTrack (0)) if (e->message.isNoteOn()) ++ons;
        check (ons == (int) p.notes.size(), "midi file note count");
        check (mf.getTimeFormat() == 960, "midi file resolution");
    }

    // 2b. ROLL BASS: every style with the knobs pushed around stays in key, off the kick and bends cleanly
    {
        int rolls = 0, dipped = 0, mutatedDiffer = 0, mutable_ = 0;
        for (int g = 0; g < NumGenres; ++g)
            for (int st = 0; st < NumRollStyles; ++st)
                for (int seed = 1; seed <= 8; ++seed)
                {
                    Settings s;
                    s.genre = g; s.role = Bass; s.rollStyle = st; s.seed = seed * 53; s.bars = seed % 2 == 0 ? 8 : 4;
                    s.key = seed % 12; s.scale = seed % NumScales; s.triplet = seed == 5;
                    s.syncopation = 0.12f * seed; s.octaveJump = 0.1f * seed; s.pitchDip = seed % 3 == 0 ? 0.0f : 0.11f * seed;
                    s.variation = 0.12f * seed; s.evolve = seed % 4 == 0 ? 0.8f : 0.0f; s.kickAvoid = seed == 7 ? 0.0f : 1.0f;
                    const auto p = generate (s);
                    const juce::String where = juce::String (genreInfo (g).title) + " roll " + rollStyleTitle (st) + " seed " + juce::String (s.seed);
                    check (! p.notes.empty(), where + ": no notes");
                    const auto& steps = scaleSteps (s.scale);
                    const bool house = (g == TechHouse || g == DeepHouse || g == BassHouse) && st == RollAuto && ! s.triplet;
                    for (auto& n : p.notes)
                    {
                        const int pc = ((n.pitch - s.key) % 12 + 12) % 12;
                        check (house || std::find (steps.begin(), steps.end(), pc) != steps.end(), where + ": note out of key");
                        // kick avoid: nothing starts on a beat (swing never moves beats)
                        if (s.kickAvoid > 0.5f && ! house)
                            check (std::abs (n.start - std::round (n.start)) > 1.0e-6, where + ": note on the kick");
                    }
                    double last = -1.0;
                    for (auto& b : p.bends)
                    {
                        check (b.value <= 0.0f && b.value >= -1.0f && b.beat >= last, where + ": bend order / range");
                        last = b.beat;
                    }
                    if (! p.bends.empty())
                    {
                        check (p.bends.back().value == 0.0f, where + ": bend not reset");
                        ++dipped;
                    }
                    check (s.pitchDip > 0.0f || p.bends.empty(), where + ": bends with PITCH DIP at zero");
                    check (p.detail.isNotEmpty(), where + ": no detail line");

                    auto m = s;
                    m.mutation = 77;
                    const auto mp = generate (m);
                    bool differs = mp.notes.size() != p.notes.size() || mp.bends.size() != p.bends.size();
                    for (size_t i = 0; ! differs && i < p.notes.size(); ++i)
                        differs = mp.notes[i].pitch != p.notes[i].pitch || std::abs (mp.notes[i].start - p.notes[i].start) > 1.0e-6;
                    if (! house) { mutatedDiffer += differs ? 1 : 0; ++mutable_; } // AUTO house grooves have no motif to mutate
                    ++rolls;
                }
        std::printf ("roll bass: %d loops, %d with dips, %d changed by MUTATE\n", rolls, dipped, mutatedDiffer);
        check (dipped > rolls / 3, "roll bass: too few loops with dips");
        std::printf ("  MUTATE changed %d of %d motifs\n", mutatedDiffer, mutable_);
        check (mutatedDiffer > mutable_ * 4 / 5, "roll bass: MUTATE changes too little");

        // GLIDE: slides overlap a different next note; GROOVE adds quiet ghost notes
        {
            Settings gs; gs.genre = PsyTech; gs.role = Bass; gs.glide = 1.0f; gs.groove = 1.0f; gs.octaveJump = 0.5f; gs.bars = 4;
            const auto gp = generate (gs);
            int slides = 0, ghosts = 0;
            for (size_t i = 0; i < gp.notes.size(); ++i)
            {
                const auto& n = gp.notes[i];
                ghosts += n.velocity < 50 ? 1 : 0;
                if (! n.slide) continue;
                ++slides;
                const bool overlaps = std::any_of (gp.notes.begin(), gp.notes.end(), [&] (const Note& o)
                    { return o.pitch != n.pitch && o.start > n.start && o.start < n.start + n.length; });
                check (overlaps, "glide: a slide that overlaps nothing");
            }
            std::printf ("  glide + groove loop: %d notes, %d slides, %d ghosts\n", (int) gp.notes.size(), slides, ghosts);
            check (slides > 0 && ghosts > 0, "glide / groove did nothing");
            Settings none = gs; none.glide = 0.0f;
            for (auto& n : generate (none).notes) check (! n.slide, "slide with GLIDE at zero");
        }

        // the dips reach the .mid as pitch wheel moves
        Settings s; s.genre = FullOn; s.role = Bass; s.pitchDip = 0.9f; s.bars = 4;
        const auto p = generate (s);
        int wheels = 0;
        const auto mf = toMidiFile (p, 145.0);
        for (auto* e : *mf.getTrack (0)) if (e->message.isPitchWheel()) ++wheels;
        check (wheels == (int) p.bends.size() && wheels > 0, "roll bass: pitch wheel in the midi file");
        std::printf ("  full-on dip loop: %d notes, %d bends  [%s]\n", (int) p.notes.size(), wheels, p.detail.toRawUTF8());
    }

    // 3. audio to MIDI on loops rendered from known notes
    std::printf ("audio to midi:\n");
    roundTrip ("full-on bass", FullOn, Bass, BassLine, 0.9, 0.9);
    roundTrip ("full-on bass + kick", FullOn, Bass, BassLine, 0.85, 0.85, true);
    roundTrip ("tech house bass", TechHouse, Bass, BassLine, 0.85, 0.85);
    roundTrip ("trance lead", Trance, Lead, Melody, 0.85, 0.85);
    roundTrip ("prog psy pluck", ProgPsy, Pluck, Melody, 0.8, 0.8);
    // the preview synth slides acid notes legato now, and the shortest notes after a long slide never settle on their pitch
    roundTrip ("psy-tech acid", PsyTech, Acid, BassLine, 0.65, 0.75);
    roundTrip ("trance drums", Trance, Drums, DrumHits, 0.75, 0.85);
    roundTrip ("tech house drums", TechHouse, Drums, DrumHits, 0.75, 0.75);
    roundTrip ("trance pad chords", Trance, Pad, ChordTrack, 0, 0);
    roundTrip ("deep house chords", DeepHouse, Chords, ChordTrack, 0, 0);

    // silence and noise do not crash and give nothing sensible
    {
        juce::AudioBuffer<float> silent (1, 44100);
        silent.clear();
        check (convertAudio (silent, 44100.0, Melody, 0.5f, 120.0).notes.empty(), "silence gives no notes");
        juce::AudioBuffer<float> tiny (1, 100);
        tiny.clear();
        for (int m = 0; m < NumModes; ++m) convertAudio (tiny, 44100.0, m, 0.5f, 120.0);
    }

    // 4. files to look at and listen to
    if (outDir != juce::File())
    {
        auto midiDir = outDir.getChildFile ("midi"), wavDir = outDir.getChildFile ("audio");
        midiDir.createDirectory();
        wavDir.createDirectory();
        for (auto& preset : presets())
        {
            if (preset.category == "FACTORY") continue;
            const auto p = generate (settingsFor (preset));
            const auto mf = toMidiFile (p, genreInfo ((int) preset.values.at ("genre")).bpm);
            auto f = midiDir.getChildFile (juce::File::createLegalFileName (preset.category + " - " + preset.name) + ".mid");
            f.deleteFile();
            juce::FileOutputStream os (f);
            mf.writeTo (os);
        }
        // per genre: drums + bass + lead (or arp) on the preview synth, two times round
        for (int g = 0; g < NumGenres; ++g)
        {
            const double bpm = genreInfo (g).bpm, sr = 44100.0;
            juce::AudioBuffer<float> mix;
            for (int r : { Drums, Bass, g == MelodicTechno || g == Trance ? Arp : Lead, Pad })
            {
                Settings s; s.genre = g; s.role = r; s.bars = 4; s.key = 6; s.seed = 7;
                s.swing = genreInfo (g).swing;
                if (r == Pad && g != Trance && g != MelodicTechno && g != DeepHouse) continue;
                auto a = render (generate (s), r, bpm, sr, 2);
                if (mix.getNumSamples() == 0) { mix.makeCopyOf (a); continue; }
                for (int ch = 0; ch < 2; ++ch) mix.addFrom (ch, 0, a, ch, 0, juce::jmin (mix.getNumSamples(), a.getNumSamples()), r == Pad ? 0.5f : 0.8f);
            }
            mix.applyGain (0.8f / juce::jmax (0.01f, mix.getMagnitude (0, mix.getNumSamples())));
            writeWav (wavDir.getChildFile (juce::String (g).paddedLeft ('0', 2) + " " + genreInfo (g).title + ".wav"), mix, sr);
        }
        // every ROLL BASS preset over its genre's drums, to hear the dips and fills
        int rollWavs = 0;
        for (auto& preset : presets())
        {
            if (preset.category != "ROLL BASS") continue;
            const int g = (int) preset.values.at ("genre");
            const double bpm = genreInfo (g).bpm, sr = 44100.0;
            Settings d; d.genre = g; d.role = Drums; d.bars = 4; d.seed = 7;
            auto mix = render (generate (d), Drums, bpm, sr, 2);
            const auto bassLoop = generate (settingsFor (preset));
            const auto b = render (bassLoop, Bass, bpm, sr, juce::jmax (1, (int) std::round (32.0 / bassLoop.lengthBeats))); // 8 bars, like the drums
            const int len = juce::jmin (mix.getNumSamples(), b.getNumSamples());
            juce::AudioBuffer<float> out (2, len);
            for (int ch = 0; ch < 2; ++ch) { out.copyFrom (ch, 0, mix, ch, 0, len); out.addFrom (ch, 0, b, ch, 0, len, 1.0f); }
            out.applyGain (0.8f / juce::jmax (0.01f, out.getMagnitude (0, len)));
            writeWav (wavDir.getChildFile ("ROLL BASS - " + juce::File::createLegalFileName (preset.name) + ".wav"), out, sr);
            ++rollWavs;
        }
        std::printf ("wrote %d .mid files and %d .wav files to %s\n", (int) presets().size() - 1, (int) NumGenres + rollWavs, outDir.getFullPathName().toRawUTF8());
    }

    std::printf (failures == 0 ? "ALL PASSED\n" : "%d FAILURES\n", failures);
    return failures == 0 ? 0 : 1;
}
