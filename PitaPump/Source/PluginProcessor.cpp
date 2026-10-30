#include "PluginProcessor.h"
#include "../../Common/UpdateNotice.h"
#include "PluginEditor.h"
#include "Presets.h"

PitaPumpProcessor::PitaPumpProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                          .withInput ("Sidechain", juce::AudioChannelSet::stereo(), false)),
      apvts (*this, &undo, "PITAPUMP", createLayout())
{
    engine.setPoints (pump::defaultPoints());
    noteOns.reserve (256);
}

juce::AudioProcessorValueTreeState::ParameterLayout PitaPumpProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout l;
    auto flt = [] (const char* id, const char* name, float lo, float hi, float def, const char* label, float step = 0.1f, float skew = 1.0f)
    {
        return std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, NormalisableRange<float> (lo, hi, step, skew), def,
                                                      AudioParameterFloatAttributes().withLabel (label));
    };
    auto choice = [] (const char* id, const char* name, const StringArray& items, int def)
    {
        return std::make_unique<AudioParameterChoice> (ParameterID { id, 1 }, name, items, def);
    };

    StringArray shapes;
    for (int i = 0; i < pump::NumShapes; ++i)
        shapes.add (pump::shapeName (i));

    l.add (choice ("trigger", "Trigger", { "SYNC", "MIDI", "AUDIO" }, 0));
    l.add (choice ("rate", "Rate", pump::rateNames(), 4));
    l.add (choice ("snap", "Grid", pump::snapNames(), 3));
    l.add (flt ("phase", "Phase", 0.0f, 360.0f, 0.0f, "deg", 1.0f));
    l.add (flt ("swing", "Swing", 50.0f, 75.0f, 50.0f, "%"));
    l.add (flt ("thresh", "Threshold", -60.0f, 0.0f, -24.0f, "dB"));
    l.add (choice ("shape", "Shape", shapes, 0));
    l.add (flt ("depth", "Depth", 0.0f, 100.0f, 85.0f, "%"));
    l.add (flt ("curve", "Curve", 0.0f, 100.0f, 50.0f, "%"));
    l.add (flt ("attack", "Attack", 0.0f, 50.0f, 0.0f, "ms"));
    l.add (flt ("release", "Release", 10.0f, 100.0f, 100.0f, "%"));
    l.add (flt ("smooth", "Smooth", 0.0f, 20.0f, 2.0f, "ms"));
    l.add (flt ("pregain", "Pre Gain", -12.0f, 24.0f, 0.0f, "dB"));
    l.add (std::make_unique<AudioParameterBool> (ParameterID { "auto", 1 }, "Auto Pre-Amp", false));
    l.add (flt ("mix", "Mix", 0.0f, 100.0f, 100.0f, "%"));
    l.add (flt ("gain", "Output", -24.0f, 12.0f, 0.0f, "dB"));
    return l;
}

bool PitaPumpProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    if (layouts.getMainInputChannelSet() != out)
        return false;
    if (layouts.inputBuses.size() > 1)
    {
        const auto sc = layouts.inputBuses[1];
        if (! sc.isDisabled() && sc != juce::AudioChannelSet::mono() && sc != juce::AudioChannelSet::stereo())
            return false;
    }
    return true;
}

void PitaPumpProcessor::prepareToPlay (double sampleRate, int)
{
    engine.prepare (sampleRate);
}

pump::Params PitaPumpProcessor::readParams() const
{
    auto v = [this] (const char* id) { return apvts.getRawParameterValue (id)->load(); };

    pump::Params p;
    p.trigger  = (int) v ("trigger");
    p.rate     = (int) v ("rate");
    p.phase    = v ("phase");
    p.swing    = v ("swing") * 0.01f;
    p.threshDb = v ("thresh");
    p.shape    = (int) v ("shape");
    p.depth    = v ("depth") * 0.01f;
    p.curve    = v ("curve") * 0.01f;
    p.attackMs = v ("attack");
    p.release  = v ("release") * 0.01f;
    p.smoothMs = v ("smooth");
    p.preGain  = juce::Decibels::decibelsToGain (v ("pregain"));
    p.autoGain = v ("auto") > 0.5f;
    p.mix      = v ("mix") * 0.01f;
    p.outGain  = juce::Decibels::decibelsToGain (v ("gain"));
    return p;
}

void PitaPumpProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const auto p = readParams();

    noteOns.clear();
    for (const auto m : midi)
        if (m.getMessage().isNoteOn() && noteOns.size() < noteOns.capacity())
            noteOns.push_back (m.samplePosition);

    auto main = getBusBuffer (buffer, false, 0);
    const juce::AudioBuffer<float>* sc = nullptr;
    juce::AudioBuffer<float> scBuf;
    if (getBusCount (true) > 1 && getBus (true, 1)->isEnabled())
    {
        scBuf = getBusBuffer (buffer, true, 1);
        if (scBuf.getNumChannels() > 0)
            sc = &scBuf;
    }
    engine.process (main, sc, noteOns, p, getPlayHead());
    fader.process (buffer, getSampleRate());
}

juce::AudioProcessorEditor* PitaPumpProcessor::createEditor()
{
    return skazi::withUpdateNotice (new PitaPumpEditor (*this));
}

//==============================================================================
void PitaPumpProcessor::setPoints (const std::vector<pump::Point>& pts)
{
    engine.setPoints (pts);
    storePointsInState();
}

void PitaPumpProcessor::storePointsInState()
{
    juce::StringArray s;
    for (auto& pt : engine.getPoints())
        s.add (juce::String (pt.x, 4) + "," + juce::String (pt.y, 4));
    apvts.state.getOrCreateChildWithName ("POINTS", nullptr).setProperty ("data", s.joinIntoString (";"), nullptr);
}

void PitaPumpProcessor::loadPointsFromState()
{
    const auto data = apvts.state.getChildWithName ("POINTS").getProperty ("data").toString();
    std::vector<pump::Point> pts;
    for (auto& pair : juce::StringArray::fromTokens (data, ";", ""))
    {
        const auto xy = juce::StringArray::fromTokens (pair, ",", "");
        if (xy.size() == 2)
            pts.push_back ({ juce::jlimit (0.0f, 1.0f, xy[0].getFloatValue()), juce::jlimit (0.0f, 1.0f, xy[1].getFloatValue()) });
    }
    engine.setPoints (pts.size() >= 2 ? pts : pump::defaultPoints());
}

void PitaPumpProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    storePointsInState();
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void PitaPumpProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    fader.cancel(); // the saved sound wins over any preset change still waiting
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
            loadPointsFromState();
        }
}

void PitaPumpProcessor::applyValues (const std::map<juce::String, float>& values)
{
    undo.beginNewTransaction();
    for (auto& [id, value] : values)
        if (auto* prm = apvts.getParameter (id))
            prm->setValueNotifyingHost (prm->convertTo0to1 (value));
}

void PitaPumpProcessor::loadPreset (int index)
{
    const auto& list = presets::all();
    index = ((index % (int) list.size()) + (int) list.size()) % (int) list.size();
    const auto values = list[(size_t) index].values;
    fader.change ([this, values] { applyValues (values); });
    currentPreset = index;
    presetLabel = list[(size_t) index].category + juce::String::fromUTF8 (" \xc2\xb7 ") + list[(size_t) index].name;
}

void PitaPumpProcessor::randomizeAll()
{
    fader.change ([this, values = presets::makeRandom (juce::Random::getSystemRandom()).values] { applyValues (values); });
    presetLabel = juce::String::fromUTF8 ("RANDOM \xc2\xb7 ") + juce::String (juce::Random::getSystemRandom().nextInt ({ 100, 1000 }));
}

void PitaPumpProcessor::switchAB (bool toB)
{
    if (toB == onB)
        return;
    storePointsInState();
    (onB ? slotB : slotA) = apvts.copyState();
    const auto& target = toB ? slotB : slotA;
    if (target.isValid())
    {
        apvts.replaceState (target.createCopy());
        loadPointsFromState();
    }
    onB = toB;
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PitaPumpProcessor();
}
