#include "PluginProcessor.h"
#include "../../Common/UpdateNotice.h"
#include "PluginEditor.h"
#include "Presets.h"

QuaziProcessor::QuaziProcessor()
    : SkaziProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true),
                      skazi::buildLayout (quazi::paramDefs()), "QUAZI")
{
}

bool QuaziProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return layouts.getMainInputChannelSet() == out;
}

void QuaziProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate, samplesPerBlock);
    setLatencySamples (engine.latency()); // the 4x oversampling delays the sound slightly; the DAW compensates
}

void QuaziProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    engine.process (buffer, settings());
    finishBlock (buffer); // smooth preset changes
}

juce::AudioProcessorEditor* QuaziProcessor::createEditor() { return skazi::withUpdateNotice (new QuaziEditor (*this)); }

const std::vector<skazi::Preset>& QuaziProcessor::presetList() const { return quazi::presets(); }
skazi::Preset QuaziProcessor::randomPreset (juce::Random& rng) const { return quazi::randomPreset (rng); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new QuaziProcessor(); }
