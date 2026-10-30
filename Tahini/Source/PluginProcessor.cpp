#include "PluginProcessor.h"
#include "../../Common/UpdateNotice.h"
#include "PluginEditor.h"
#include "Presets.h"

TahiniProcessor::TahiniProcessor()
    : SkaziProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true),
                      skazi::buildLayout (tahini::paramDefs()), "TAHINI")
{
}

bool TahiniProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return layouts.getMainInputChannelSet() == out;
}

void TahiniProcessor::prepareToPlay (double sampleRate, int)
{
    engine.prepare (sampleRate, getTotalNumOutputChannels());
}

void TahiniProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    engine.process (buffer, settings(), getPlayHead());
    finishBlock (buffer); // smooth preset changes
}

juce::AudioProcessorEditor* TahiniProcessor::createEditor() { return skazi::withUpdateNotice (new TahiniEditor (*this)); }

const std::vector<skazi::Preset>& TahiniProcessor::presetList() const { return tahini::presets(); }
skazi::Preset TahiniProcessor::randomPreset (juce::Random& rng) const { return tahini::randomPreset (rng); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new TahiniProcessor(); }
