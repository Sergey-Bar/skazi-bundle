#include "PluginProcessor.h"
#include "../../Common/UpdateNotice.h"
#include "PluginEditor.h"
#include "Presets.h"

SubichProcessor::SubichProcessor()
    : SkaziProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true), createLayout(), "SUBICH")
{
}

juce::AudioProcessorValueTreeState::ParameterLayout SubichProcessor::createLayout()
{
    using P = subich::ParamDef;
    juce::AudioProcessorValueTreeState::ParameterLayout l;
    for (auto& d : subich::paramDefs())
    {
        switch (d.kind)
        {
            case P::Float:  l.add (skazi::floatParam (d.id, d.name, d.lo, d.hi, d.def, d.label, d.step, d.skew)); break;
            case P::Int:    l.add (skazi::intParam (d.id, d.name, (int) d.lo, (int) d.hi, (int) d.def)); break;
            case P::Choice: l.add (skazi::choiceParam (d.id, d.name, d.choices, (int) d.def)); break;
            case P::Bool:   l.add (skazi::boolParam (d.id, d.name, d.def > 0.5f)); break;
        }
    }
    return l;
}

bool SubichProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo();
}

void SubichProcessor::prepareToPlay (double sampleRate, int)
{
    engine.prepare (sampleRate);
}

subich::Settings SubichProcessor::settings() const
{
    return subich::Settings::fromValues ([this] (const char* id) { return v (id); });
}

void SubichProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    if (panicRequested.exchange (false))
    {
        engine.panic();
        midi.clear();
    }
    if (pitchMoved.exchange (false))
        midi.addEvent (juce::MidiMessage::pitchWheel (1, juce::jlimit (0, 16383, (int) std::round ((screenPitch.load() + 1.0f) * 8191.5f))), 0);
    if (modMoved.exchange (false))
        midi.addEvent (juce::MidiMessage::controllerEvent (1, 1, juce::jlimit (0, 127, (int) std::round (screenMod.load() * 127.0f))), 0);
    keyboard.processNextMidiBuffer (midi, 0, buffer.getNumSamples(), true);
    engine.render (buffer, midi, settings(), getPlayHead());
    finishBlock (buffer); // smooth preset changes
    midi.clear();
}

juce::AudioProcessorEditor* SubichProcessor::createEditor() { return skazi::withUpdateNotice (new SubichEditor (*this)); }

const std::vector<skazi::Preset>& SubichProcessor::presetList() const { return subich::presets(); }
skazi::Preset SubichProcessor::randomPreset (juce::Random& rng) const { return subich::randomPreset (rng); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new SubichProcessor(); }
