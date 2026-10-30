#include "PluginProcessor.h"
#include "../../Common/UpdateNotice.h"
#include "PluginEditor.h"
#include "Presets.h"

BullyProcessor::BullyProcessor()
    : SkaziProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true),
                      skazi::buildLayout (bully::paramDefs()), "BULLY")
{
    for (auto& d : bully::paramDefs()) raw.push_back (apvts.getRawParameterValue (d.id));
    values.resize (raw.size());
    for (auto& [id, val] : bully::initPatch()) setParam (id, val); // open on the signature sound
    undo.clearUndoHistory();
    bully::WaveBank::get(); // build the tables now, not on the audio thread
}

bool BullyProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo();
}

void BullyProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate, samplesPerBlock);
    arp.prepare (sampleRate);
}

void BullyProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    for (size_t i = 0; i < raw.size(); ++i) values[i] = raw[i]->load();
    const auto& ix = bully::Ix::get();

    if (panicRequested.exchange (false))
    {
        engine.allNotesOff();
        arp.reset();
        midi.clear();
    }
    if (pitchMoved.exchange (false))
        midi.addEvent (juce::MidiMessage::pitchWheel (1, juce::jlimit (0, 16383, (int) std::round ((screenPitch.load() + 1.0f) * 8191.5f))), 0);
    if (modMoved.exchange (false))
        midi.addEvent (juce::MidiMessage::controllerEvent (1, 1, juce::jlimit (0, 127, (int) std::round (screenMod.load() * 127.0f))), 0);
    keyboard.processNextMidiBuffer (midi, 0, buffer.getNumSamples(), true);

    const auto ht = skazi::hostTime (getPlayHead());
    bully::Arp::Settings as;
    as.on = values[(size_t) ix.arpon] > 0.5f;
    as.mode = (int) values[(size_t) ix.arpmode];
    as.rate = (int) values[(size_t) ix.arprate];
    as.octaves = (int) values[(size_t) ix.arpoct];
    as.gate = values[(size_t) ix.arpgate] * 0.01f;
    as.swing = values[(size_t) ix.arpswing] * 0.01f;
    arp.process (midi, buffer.getNumSamples(), as, ht);

    engine.render (buffer, midi, values.data(), ht);
    finishBlock (buffer); // smooth preset changes
    midi.clear();
}

void BullyProcessor::setParam (const juce::String& id, float value)
{
    if (auto* p = apvts.getParameter (id))
        p->setValueNotifyingHost (p->convertTo0to1 (value));
}

int BullyProcessor::addModulation (int src, const juce::String& paramId, float amount)
{
    const int dst = bully::destForParam (paramId);
    if (dst <= 0 || src <= 0) return -1;
    undo.beginNewTransaction();
    int free = -1;
    for (int s = 1; s <= bully::kSlots; ++s)
    {
        const auto p = "x" + juce::String (s);
        const int a = (int) v ((p + "src").toRawUTF8()), d = (int) v ((p + "dst").toRawUTF8());
        if (a == src && d == dst) return s - 1; // already routed
        if (free < 0 && (a == 0 || d == 0)) free = s;
    }
    if (free < 0) return -1;
    const auto p = "x" + juce::String (free);
    setParam (p + "src", (float) src);
    setParam (p + "dst", (float) dst);
    setParam (p + "amt", amount);
    return free - 1;
}

void BullyProcessor::removeModulation (int slot)
{
    undo.beginNewTransaction();
    const auto p = "x" + juce::String (slot + 1);
    setParam (p + "src", 0);
    setParam (p + "dst", 0);
    setParam (p + "amt", 0);
}

std::vector<std::pair<int, float>> BullyProcessor::modsFor (const juce::String& paramId) const
{
    std::vector<std::pair<int, float>> out;
    const int dst = bully::destForParam (paramId);
    if (dst <= 0) return out;
    const auto& ix = bully::Ix::get();
    for (int s = 0; s < bully::kSlots; ++s)
        if ((int) raw[(size_t) ix.slot[s].dst]->load() == dst && (int) raw[(size_t) ix.slot[s].src]->load() > 0)
            out.push_back ({ s, raw[(size_t) ix.slot[s].amt]->load() });
    return out;
}

juce::AudioProcessorEditor* BullyProcessor::createEditor() { return skazi::withUpdateNotice (new BullyEditor (*this)); }

const std::vector<skazi::Preset>& BullyProcessor::presetList() const { return bully::presets(); }
skazi::Preset BullyProcessor::randomPreset (juce::Random& rng) const { return bully::randomPreset (rng); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new BullyProcessor(); }
