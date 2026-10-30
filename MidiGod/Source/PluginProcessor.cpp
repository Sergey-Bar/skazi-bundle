#include "PluginProcessor.h"
#include "../../Common/UpdateNotice.h"
#include "PluginEditor.h"
#include "Presets.h"

using namespace midigod;

namespace
{
    // Which parameters say what the loop is, how it is shaped, how audio notes are finished, and how audio is analysed
    const std::vector<const char*> kIdentity = { "genre", "role", "seed", "rbMut" };
    const std::vector<const char*> kShape = { "key", "scale", "bars", "grid", "octave", "density", "swing", "gate", "dyn", "human", "move",
                                              "rbStyle", "rbSync", "rbKick", "rbOct", "rbDip", "rbVar", "rbEvolve", "rbGlide", "rbGroove" };
    const std::vector<const char*> kFinish = { "a2mQuant", "a2mSnap", "key", "scale", "octave" };
    const std::vector<const char*> kAnalysis = { "a2mMode", "a2mSens" };
    const juce::String dot = juce::String::fromUTF8 (" \xc2\xb7 ");
}

MidiGodProcessor::MidiGodProcessor()
    : SkaziProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true),
                      skazi::buildLayout (paramDefs()), "MIDIGOD")
{
    events.reserve (4096);
    generated.ensureSize (8192);
    rememberWatched();
    regenerate();
    startTimerHz (15);
}

MidiGodProcessor::~MidiGodProcessor()
{
    stopTimer();
    alive->store (false);
    cancelAnalysis = true;
    pool.removeAllJobs (true, 10000);
}

bool MidiGodProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo();
}

void MidiGodProcessor::prepareToPlay (double sampleRate, int)
{
    synth.prepare (sampleRate);
    sounding.fill (0);
    wasRunning = false;
    expectedBeat = -1.0;
}

double MidiGodProcessor::tempo() const
{
    const double b = bpmNow.load();
    return b > 0.0 ? b : genreInfo ((int) v ("genre")).bpm;
}

//==============================================================================
void MidiGodProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    buffer.clear();
    const double sr = getSampleRate() > 0.0 ? getSampleRate() : 44100.0;

    bool hostKnown = false, hostPlaying = false;
    double ppq = 0.0, hostBpm = 0.0;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm()) hostBpm = *b;
            if (auto q = pos->getPpqPosition()) { ppq = *q; hostKnown = true; hostPlaying = pos->getIsPlaying(); }
        }
    bpmNow = hostBpm > 0.0 ? juce::jlimit (20.0, 999.0, hostBpm) : 0.0;
    const double bpm = tempo();

    const auto pat = pattern();
    synth.setSound ((int) v ("role"), pat->drums);
    events.clear();
    generated.clear();

    // whatever is still sounding when the loop changes or the transport jumps gets its note-off now
    auto flush = [this]
    {
        if (bent)
        {
            generated.addEvent (juce::MidiMessage::pitchWheel (1, 8192), 0);
            synth.setBend (0.0f);
            bent = false;
        }
        for (int p = 0; p < 128; ++p)
            if (sounding[(size_t) p] > 0)
            {
                generated.addEvent (juce::MidiMessage::noteOff (1, p), 0);
                synth.noteOff (p);
                sounding[(size_t) p] = 0;
            }
    };
    if (pat.get() != playingPattern) { flush(); playingPattern = pat.get(); }

    // notes played into the plugin sound on the preview synth and pass through
    for (const auto meta : midi)
    {
        const auto m = meta.getMessage();
        if (m.isNoteOn()) events.push_back ({ meta.samplePosition, true, m.getNoteNumber(), m.getVelocity(), true });
        else if (m.isNoteOff()) events.push_back ({ meta.samplePosition, false, m.getNoteNumber(), 0, true });
        generated.addEvent (m, meta.samplePosition);
    }

    const bool run = hostPlaying || ! hostKnown || v ("freeRun") > 0.5f;
    if (! run)
    {
        if (wasRunning) flush();
        wasRunning = false;
        expectedBeat = -1.0;
        uiBeat = -1.0;
    }
    else
    {
        const double start = hostPlaying ? ppq : freeBeat;
        const double perSample = bpm / 60.0 / sr, end = start + n * perSample;
        if (! wasRunning || std::abs (start - expectedBeat) > 0.01) flush();
        wasRunning = true;
        expectedBeat = end;
        freeBeat = end;

        const double L = pat->lengthBeats;
        auto occurrences = [&] (double t, bool on, const Note& note)
        {
            for (double a = t + std::ceil ((start - t) / L) * L; a < end; a += L)
                events.push_back ({ juce::jlimit (0, n - 1, (int) ((a - start) / perSample)), on, note.pitch, note.velocity, false });
        };
        for (const auto& note : pat->notes)
        {
            occurrences (note.start, true, note);
            occurrences (note.start + note.length, false, note);
        }
        for (const auto& b : pat->bends)
            for (double a = b.beat + std::ceil ((start - b.beat) / L) * L; a < end; a += L)
                events.push_back ({ juce::jlimit (0, n - 1, (int) ((a - start) / perSample)), false, 0, 0, false, true, b.value });
        uiBeat = std::fmod (std::fmod (start, L) + L, L);
    }

    // in time order, note-offs before note-ons on the same sample
    std::stable_sort (events.begin(), events.end(), [] (const Event& a, const Event& b)
    {
        return a.sample != b.sample ? a.sample < b.sample : (! a.on && b.on);
    });

    const float vol = v ("vol");
    const float gain = vol <= -59.9f ? 0.0f : juce::Decibels::decibelsToGain (vol);
    float* left = buffer.getWritePointer (0);
    float* right = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : nullptr;

    int pos = 0;
    auto renderTo = [&] (int upTo)
    {
        if (upTo <= pos) return;
        if (gain > 0.0f)
        {
            if (right != nullptr) synth.render (left + pos, right + pos, upTo - pos, gain);
            else
            {
                // mono out: render both channels into the one buffer at half level
                synth.render (left + pos, left + pos, upTo - pos, gain * 0.5f);
            }
        }
        pos = upTo;
    };

    for (const auto& e : events)
    {
        renderTo (e.sample);
        if (e.bend)
        {
            generated.addEvent (juce::MidiMessage::pitchWheel (1, bendTo14Bit (e.bendValue)), e.sample);
            synth.setBend (e.bendValue * kBendRangeSemis);
            bent = e.bendValue != 0.0f;
            continue;
        }
        if (! e.fromHost)
        {
            auto& count = sounding[(size_t) e.note];
            if (e.on)
            {
                if (count > 0) { generated.addEvent (juce::MidiMessage::noteOff (1, e.note), e.sample); synth.noteOff (e.note); --count; }
                ++count;
                generated.addEvent (juce::MidiMessage::noteOn (1, e.note, (juce::uint8) juce::jlimit (1, 127, e.velocity)), e.sample);
            }
            else
            {
                if (count == 0) continue; // started before playback did
                --count;
                generated.addEvent (juce::MidiMessage::noteOff (1, e.note), e.sample);
            }
        }
        if (e.on) synth.noteOn (e.note, e.velocity);
        else synth.noteOff (e.note);
    }
    renderTo (n);

    if (v ("midiOut") > 0.5f) midi.swapWith (generated);
    else midi.clear();

    finishBlock (buffer); // smooth preset changes
}

//==============================================================================
std::vector<float> MidiGodProcessor::watched (const std::vector<const char*>& ids) const
{
    std::vector<float> out;
    for (auto* id : ids) out.push_back (v (id));
    return out;
}

void MidiGodProcessor::rememberWatched()
{
    lastIdentity = watched (kIdentity);
    lastShape = watched (kShape);
    lastFinish = watched (kFinish);
    lastAnalysis = watched (kAnalysis);
}

void MidiGodProcessor::timerCallback()
{
    bool changed = false;
    const auto identity = watched (kIdentity), shape = watched (kShape), finish = watched (kFinish), analysis = watched (kAnalysis);
    if (identity != lastIdentity)
    {
        // a new genre, role or variation means the generator is wanted again
        if (fromAudio) { fromAudio = false; audioStatus = "BACK TO THE GENERATOR" + dot + "DROP AUDIO TO CONVERT AGAIN"; }
        changed = true;
    }
    if (! fromAudio && shape != lastShape) changed = true;
    if (fromAudio && finish != lastFinish) changed = true;
    if (analysis != lastAnalysis && fromAudio && audioFile.existsAsFile())
    {
        lastAnalysis = analysis;
        convertAudio (audioFile); // new mode or sensitivity: analyse the same file again
    }
    lastIdentity = identity;
    lastShape = shape;
    lastFinish = finish;
    lastAnalysis = analysis;
    if (changed) regenerate();
}

void MidiGodProcessor::regenerate()
{
    std::shared_ptr<Pattern> p;
    if (fromAudio)
    {
        p = std::make_shared<Pattern>();
        p->notes = audioNotes;
        Finish f;
        f.quantize = quantizeBeats ((int) v ("a2mQuant"));
        f.snap = v ("a2mSnap") > 0.5f;
        f.key = (int) v ("key");
        f.scale = (int) v ("scale");
        f.octave = (int) v ("octave");
        f.drums = audioDrums;
        finishNotes (p->notes, audioLength, f);
        p->lengthBeats = audioLength;
        p->drums = audioDrums;
        const int bars = juce::roundToInt (audioLength / 4.0);
        p->title = ("AUDIO" + dot + audioName + dot + modeTitle (audioMode) + dot + juce::String (bars) + (bars == 1 ? " BAR" : " BARS")).toUpperCase();
        p->fileStem = "MIDI GOD - " + audioName + " (" + modeTitle (audioMode) + ")";
    }
    else
        p = std::make_shared<Pattern> (generate (settings()));

    // the audio thread may still hold the last two loops for a block: they are freed here, never there
    retired[0] = retired[1];
    retired[1] = std::atomic_load (&live);
    std::atomic_store (&live, std::shared_ptr<const Pattern> (std::move (p)));
}

void MidiGodProcessor::setParam (const char* id, float value)
{
    if (auto* prm = apvts.getParameter (id))
    {
        prm->beginChangeGesture();
        prm->setValueNotifyingHost (prm->convertTo0to1 (value));
        prm->endChangeGesture();
    }
}

void MidiGodProcessor::godMode()
{
    const int now = (int) v ("seed");
    int next = now;
    while (next == now) next = 1 + juce::Random::getSystemRandom().nextInt (999);
    undo.beginNewTransaction();
    setParam ("seed", (float) next);
    setParam ("rbMut", 0.0f);
}

void MidiGodProcessor::mutate()
{
    const int now = (int) v ("rbMut");
    int next = now;
    while (next == now) next = 1 + juce::Random::getSystemRandom().nextInt (999);
    undo.beginNewTransaction();
    setParam ("rbMut", (float) next);
}

void MidiGodProcessor::backToGenerator()
{
    if (! fromAudio) return;
    fromAudio = false;
    audioStatus = "BACK TO THE GENERATOR" + dot + "DROP AUDIO TO CONVERT AGAIN";
    regenerate();
}

//==============================================================================
void MidiGodProcessor::convertAudio (const juce::File& file)
{
    if (analysing.load())
    {
        cancelAnalysis = true;
        pool.removeAllJobs (true, 5000);
    }
    cancelAnalysis = false;
    analysing = true;
    const int mode = (int) v ("a2mMode");
    const float sens = v ("a2mSens") / 100.0f;
    const double bpm = tempo();
    audioStatus = "LISTENING TO " + file.getFileName().toUpperCase() + "...";

    pool.addJob ([this, file, mode, sens, bpm, flag = alive]
    {
        auto res = convertFile (file, mode, sens, bpm, &cancelAnalysis);
        juce::MessageManager::callAsync ([this, flag, file, mode, res = std::move (res)]() mutable
        {
            if (flag->load()) installAudio (std::move (res), file, mode);
        });
    });
}

void MidiGodProcessor::installAudio (ConvertResult res, const juce::File& file, int mode)
{
    if (res.error == "CANCELLED") return; // a newer analysis replaced this one
    analysing = false;
    if (res.notes.empty())
    {
        audioStatus = (res.error.isNotEmpty() ? res.error : juce::String ("NO NOTES FOUND")) + dot + "TRY ANOTHER MODE OR MORE SENSITIVITY";
        return;
    }
    audioNotes = std::move (res.notes);
    audioLength = res.lengthBeats;
    audioDrums = res.drums;
    audioMode = mode;
    audioFile = file;
    audioName = file.getFileNameWithoutExtension();
    fromAudio = true;
    audioStatus = juce::String ((int) audioNotes.size()) + " NOTES FROM " + file.getFileName().toUpperCase()
                + dot + "AT " + juce::String (juce::roundToInt (tempo())) + " BPM" + (res.truncated ? dot + "FIRST 4 MINUTES" : juce::String());
    rememberWatched();
    regenerate();
}

//==============================================================================
juce::File MidiGodProcessor::writeMidiFile() const
{
    const auto pat = pattern();
    auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("MIDI GOD");
    dir.createDirectory();
    auto file = dir.getChildFile (juce::File::createLegalFileName (pat->fileStem) + ".mid");
    saveMidiFile (file);
    return file;
}

bool MidiGodProcessor::saveMidiFile (const juce::File& target) const
{
    const auto pat = pattern();
    auto midi = toMidiFile (*pat, tempo());
    target.deleteFile();
    juce::FileOutputStream out (target);
    return out.openedOk() && midi.writeTo (out);
}

//==============================================================================
void MidiGodProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    if (fromAudio)
    {
        juce::ValueTree a ("AUDIO");
        a.setProperty ("name", audioName, nullptr);
        a.setProperty ("file", audioFile.getFullPathName(), nullptr);
        a.setProperty ("length", audioLength, nullptr);
        a.setProperty ("drums", audioDrums, nullptr);
        a.setProperty ("mode", audioMode, nullptr);
        for (auto& note : audioNotes)
        {
            juce::ValueTree t ("N");
            t.setProperty ("p", note.pitch, nullptr);
            t.setProperty ("s", note.start, nullptr);
            t.setProperty ("l", note.length, nullptr);
            t.setProperty ("v", note.velocity, nullptr);
            a.appendChild (t, nullptr);
        }
        state.appendChild (a, nullptr);
    }
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void MidiGodProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    SkaziProcessor::setStateInformation (data, sizeInBytes);
    const auto a = apvts.state.getChildWithName ("AUDIO");
    fromAudio = a.isValid();
    if (fromAudio)
    {
        audioName = a.getProperty ("name").toString();
        audioFile = juce::File (a.getProperty ("file").toString());
        audioLength = (double) a.getProperty ("length", 16.0);
        audioDrums = (bool) a.getProperty ("drums", false);
        audioMode = (int) a.getProperty ("mode", 0);
        audioNotes.clear();
        for (const auto& t : a)
            audioNotes.push_back ({ (int) t.getProperty ("p"), (double) t.getProperty ("s"), (double) t.getProperty ("l"), (int) t.getProperty ("v") });
        audioStatus = juce::String ((int) audioNotes.size()) + " NOTES FROM " + audioName.toUpperCase() + " (SAVED WITH THE PROJECT)";
        apvts.state.removeChild (a, nullptr);
    }
    rememberWatched();
    regenerate();
}

juce::AudioProcessorEditor* MidiGodProcessor::createEditor() { return skazi::withUpdateNotice (new MidiGodWindow (*this)); }

const std::vector<skazi::Preset>& MidiGodProcessor::presetList() const { return presets(); }
skazi::Preset MidiGodProcessor::randomPreset (juce::Random& rng) const { return midigod::randomPreset (rng); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new MidiGodProcessor(); }
