#include "PluginProcessor.h"
#include "../../Common/UpdateNotice.h"
#include "PluginEditor.h"
#include "Presets.h"

namespace
{
const char* const kFields[] = { "note", "oct", "gate", "acc", "sld" };
}

SkacidProcessor::SkacidProcessor()
    : SkaziProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true),
                      skazi::buildLayout (skacid::paramDefs()), "SKACID")
{
    for (int i = 0; i < skacid::NumSteps; ++i)
        for (int f = 0; f < 5; ++f)
            stepValues[(size_t) i][(size_t) f] = apvts.getRawParameterValue (skacid::stepId (i, kFields[f]));

    // Start on the Init preset's pattern rather than 16 identical notes
    applyValues (presetList().front().values);
    undo.clearUndoHistory();
}

bool SkacidProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo();
}

void SkacidProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate, samplesPerBlock);
}

skacid::Pattern SkacidProcessor::pattern() const
{
    skacid::Pattern p;
    for (int i = 0; i < skacid::NumSteps; ++i)
    {
        const auto& sv = stepValues[(size_t) i];
        auto& st = p[(size_t) i];
        st.note = (int) sv[0]->load();
        st.oct = (int) sv[1]->load();
        st.gate = (int) sv[2]->load();
        st.acc = sv[3]->load() > 0.5f;
        st.sld = sv[4]->load() > 0.5f;
    }
    return p;
}

void SkacidProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    engine.process (buffer, midi, settings(), pattern(), skazi::hostTime (getPlayHead()));
    finishBlock (buffer); // smooth preset changes
}

// Every change from the grid is wrapped in a change gesture. Without one, hosts such as Cubase
// may ignore the edit or put the old value back, so a dragged note would not move.
void SkacidProcessor::setStepField (int step, const char* field, float value)
{
    if (auto* prm = apvts.getParameter (skacid::stepId (step, field)))
    {
        const bool inDrag = editing.contains (prm);
        if (! inDrag) prm->beginChangeGesture();
        prm->setValueNotifyingHost (prm->convertTo0to1 (value));
        if (! inDrag) prm->endChangeGesture();
    }
}

void SkacidProcessor::beginStepEdit (int step, const char* field)
{
    if (auto* prm = apvts.getParameter (skacid::stepId (step, field)))
        if (! editing.contains (prm)) { editing.add (prm); prm->beginChangeGesture(); }
}

void SkacidProcessor::endStepEdit (int step, const char* field)
{
    if (auto* prm = apvts.getParameter (skacid::stepId (step, field)))
        if (editing.contains (prm)) { editing.removeFirstMatchingValue (prm); prm->endChangeGesture(); }
}

void SkacidProcessor::setPattern (const skacid::Pattern& p)
{
    undo.beginNewTransaction();
    std::map<juce::String, float> values;
    skacid::putPattern (values, p);
    for (auto& [id, value] : values)
        if (auto* prm = apvts.getParameter (id))
        {
            const float norm = prm->convertTo0to1 (value);
            if (std::abs (prm->getValue() - norm) > 1.0e-7f)
            {
                prm->beginChangeGesture();
                prm->setValueNotifyingHost (norm);
                prm->endChangeGesture();
            }
        }
}

void SkacidProcessor::patternTool (int tool)
{
    using namespace skacid;
    auto p = pattern();
    const int len = (int) v ("seqLen");
    juce::Random r;
    // the notes of the pattern, so random notes stay in its scale
    juce::Array<int> pool;
    for (int i = 0; i < len; ++i) if (p[(size_t) i].gate != Rest) pool.addIfNotAlreadyThere (p[(size_t) i].note);
    if (pool.size() < 3) pool.addArray ({ 0, 3, 5, 7, 10, 12 });

    switch (tool)
    {
        case NotesUp: case NotesDown:
            for (auto& st : p)
            {
                st.note += tool == NotesUp ? 1 : -1;
                if (st.note > 12) { if (st.oct < Up) { st.note -= 12; ++st.oct; } else st.note = 12; }
                if (st.note < 0)  { if (st.oct > Down) { st.note += 12; --st.oct; } else st.note = 0; }
            }
            break;
        case RandomNotes:   for (int i = 0; i < len; ++i) p[(size_t) i].note = pool[r.nextInt (pool.size())]; break;
        case RandomAccents: for (int i = 0; i < len; ++i) p[(size_t) i].acc = r.nextFloat() < 0.3f; break;
        case RandomGlides:  for (int i = 0; i < len; ++i) p[(size_t) i].sld = r.nextFloat() < 0.25f; break;
        case GlideAll:      for (int i = 0; i < len; ++i) p[(size_t) i].sld = true; break;
        case GlideNone:     for (auto& st : p) st.sld = false; break;
        case Reverse:       std::reverse (p.begin(), p.begin() + len); break;
        case Copy:          clipboard = p; hasClipboard = true; return;
        case Paste:         if (! hasClipboard) return; p = clipboard; break;
        default: return;
    }
    setPattern (p);
}

void SkacidProcessor::generatePattern (int style)
{
    juce::Random r;
    setPattern (skacid::patterns::generate (r, style));
}

void SkacidProcessor::mutatePattern()
{
    juce::Random r;
    setPattern (skacid::patterns::mutate (pattern(), r, 0.5f));
}

void SkacidProcessor::shiftPattern (int direction)
{
    setPattern (skacid::patterns::shift (pattern(), direction, (int) v ("seqLen")));
}

void SkacidProcessor::clearPattern()
{
    skacid::Pattern p;
    for (auto& st : p) st.gate = skacid::Rest;
    p[0].gate = skacid::Note;
    setPattern (p);
}

juce::AudioProcessorEditor* SkacidProcessor::createEditor() { return skazi::withUpdateNotice (new SkacidEditor (*this)); }

const std::vector<skazi::Preset>& SkacidProcessor::presetList() const { return skacid::presets(); }
skazi::Preset SkacidProcessor::randomPreset (juce::Random& rng) const { return skacid::randomPreset (rng); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new SkacidProcessor(); }
