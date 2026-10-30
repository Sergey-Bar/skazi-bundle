#include "PluginProcessor.h"
#include "../../Common/UpdateNotice.h"
#include "PluginEditor.h"
#include "Presets.h"

SalatProcessor::SalatProcessor()
    : SkaziProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true),
                      skazi::buildLayout (salat::paramDefs()), "SALAT")
{
}

bool SalatProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return layouts.getMainInputChannelSet() == out;
}

void SalatProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate, samplesPerBlock);
    setLatencySamples (engine.latency()); // the 2x oversampled drive delays the sound slightly; the DAW compensates
}

void SalatProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    engine.process (buffer, settings(), skazi::hostTime (getPlayHead()));
    finishBlock (buffer); // smooth preset changes
}

juce::AudioProcessorEditor* SalatProcessor::createEditor() { return skazi::withUpdateNotice (new SalatEditor (*this)); }

const std::vector<skazi::Preset>& SalatProcessor::presetList() const { return salat::presets(); }
skazi::Preset SalatProcessor::randomPreset (juce::Random& rng) const { return salat::randomPreset (rng); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new SalatProcessor(); }
