#include "PluginProcessor.h"
#include "../../Common/UpdateNotice.h"
#include "PluginEditor.h"
#include "Presets.h"

namespace
{
    constexpr float kDefaultWeights[chop::NumFx] = { 80, 70, 60, 40, 30, 40, 40, 60, 50, 20, 40 };
}

ShaksukaProcessor::ShaksukaProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, &undo, "SHAKSUKA", createLayout())
{
}

juce::AudioProcessorValueTreeState::ParameterLayout ShaksukaProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout l;
    auto pct = [] (const char* id, const char* name, float def, float lo = 0.0f, float hi = 100.0f)
    {
        return std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, NormalisableRange<float> (lo, hi, 0.1f), def,
                                                      AudioParameterFloatAttributes().withLabel ("%"));
    };

    l.add (std::make_unique<AudioParameterBool> (ParameterID { "free", 1 }, "Free Mode", false));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "grid", 1 }, "Grid", chop::gridNames(), 4));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "freems", 1 }, "Free Slice",
                                                  NormalisableRange<float> (10.0f, 2000.0f, 1.0f, 0.4f), 125.0f,
                                                  AudioParameterFloatAttributes().withLabel ("ms")));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "groove", 1 }, "Groove",
                                                   StringArray { "Straight", "Swing", "Shuffle", "Drunk", "Push" }, 0));
    l.add (pct ("swing", "Swing", 58.0f, 50.0f, 80.0f));
    l.add (pct ("humanize", "Humanize", 0.0f));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "bars", 1 }, "Pattern Bars",
                                                   StringArray { "1 BAR", "2 BARS", "4 BARS" }, 0));
    l.add (pct ("lenvar", "Length Var", 30.0f));
    l.add (pct ("chaos", "Chaos", 60.0f));
    l.add (std::make_unique<AudioParameterInt> (ParameterID { "stack", 1 }, "Stack", 1, 5, 2));
    l.add (pct ("evolve", "Evolve", 20.0f));
    l.add (std::make_unique<AudioParameterInt> (ParameterID { "seed", 1 }, "Seed", 1, 9999, 427));
    l.add (pct ("mix", "Mix", 100.0f));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "gain", 1 }, "Gain", NormalisableRange<float> (-24.0f, 12.0f, 0.1f), 0.0f,
                                                  AudioParameterFloatAttributes().withLabel ("dB")));

    for (int i = 0; i < chop::NumFx; ++i)
    {
        const String name = String (chop::fxName (i)).toLowerCase();
        l.add (std::make_unique<AudioParameterBool> (ParameterID { enableId (i), 1 }, name + " on", true));
        l.add (std::make_unique<AudioParameterFloat> (ParameterID { weightId (i), 1 }, name + " amount",
                                                      NormalisableRange<float> (0.0f, 100.0f, 0.1f), kDefaultWeights[i],
                                                      AudioParameterFloatAttributes().withLabel ("%")));
    }
    return l;
}

bool ShaksukaProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return layouts.getMainInputChannelSet() == out;
}

void ShaksukaProcessor::prepareToPlay (double sampleRate, int)
{
    engine.prepare (sampleRate);
}

void ShaksukaProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    for (int c = getTotalNumInputChannels(); c < getTotalNumOutputChannels(); ++c)
        buffer.clear (c, 0, buffer.getNumSamples());

    auto v = [this] (const char* id) { return apvts.getRawParameterValue (id)->load(); };

    chop::Params p;
    p.freeMode  = v ("free") > 0.5f;
    p.grid      = (int) v ("grid");
    p.freeMs    = v ("freems");
    p.groove    = (int) v ("groove");
    p.swing     = v ("swing") * 0.01f;
    p.humanize  = v ("humanize") * 0.01f;
    p.bars      = 1 << (int) v ("bars");
    p.lengthVar = v ("lenvar") * 0.01f;
    p.chaos     = v ("chaos") * 0.01f;
    p.stack     = (int) v ("stack");
    p.evolve    = v ("evolve") * 0.01f;
    p.seed      = (int) v ("seed");
    p.mix       = v ("mix") * 0.01f;
    p.gain      = juce::Decibels::decibelsToGain (v ("gain"));
    for (int i = 0; i < chop::NumFx; ++i)
        p.weights[i] = apvts.getRawParameterValue (enableId (i))->load() > 0.5f
                           ? apvts.getRawParameterValue (weightId (i))->load() * 0.01f
                           : 0.0f;

    engine.process (buffer, p, getPlayHead());
    fader.process (buffer, getSampleRate());
}

juce::AudioProcessorEditor* ShaksukaProcessor::createEditor()
{
    return skazi::withUpdateNotice (new ShaksukaEditor (*this));
}

void ShaksukaProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void ShaksukaProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    fader.cancel(); // the saved sound wins over any preset change still waiting
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

void ShaksukaProcessor::setParam (const juce::String& id, float value)
{
    if (auto* p = apvts.getParameter (id))
        p->setValueNotifyingHost (p->convertTo0to1 (value));
}

void ShaksukaProcessor::applyValues (const std::map<juce::String, float>& values)
{
    undo.beginNewTransaction();
    for (auto& [id, value] : values)
        setParam (id, value);
}

void ShaksukaProcessor::loadPreset (int index)
{
    const auto& list = presets::all();
    if (list.empty())
        return;
    index = ((index % (int) list.size()) + (int) list.size()) % (int) list.size();
    const auto values = list[(size_t) index].values;
    fader.change ([this, values] { applyValues (values); });
    currentPreset = index;
    presetLabel = list[(size_t) index].category + juce::String::fromUTF8 (" \xc2\xb7 ") + list[(size_t) index].name;
}

void ShaksukaProcessor::randomizeAll()
{
    auto p = presets::makeRandom (juce::Random::getSystemRandom());
    fader.change ([this, values = p.values] { applyValues (values); });
    presetLabel = juce::String::fromUTF8 ("RANDOM \xc2\xb7 SEED ") + juce::String ((int) p.values["seed"]).paddedLeft ('0', 4);
}

void ShaksukaProcessor::switchAB (bool toB)
{
    if (toB == onB)
        return;
    (onB ? slotB : slotA) = apvts.copyState();
    const auto& target = toB ? slotB : slotA;
    if (target.isValid())
        apvts.replaceState (target.createCopy());
    onB = toB;
}

void ShaksukaProcessor::randomizeSeed()
{
    undo.beginNewTransaction();
    setParam ("seed", (float) juce::Random::getSystemRandom().nextInt ({ 1, 10000 }));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ShaksukaProcessor();
}
