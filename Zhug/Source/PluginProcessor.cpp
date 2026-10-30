#include "PluginProcessor.h"
#include "../../Common/UpdateNotice.h"
#include "PluginEditor.h"
#include "Presets.h"

ZhugProcessor::ZhugProcessor()
    : SkaziProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true),
                      skazi::buildLayout (zhug::paramDefs()), "ZHUG")
{
}

bool ZhugProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return layouts.getMainInputChannelSet() == out;
}

void ZhugProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate, samplesPerBlock);
    setLatencySamples (engine.latency()); // the 4x oversampling filters delay the sound slightly; the DAW compensates
}

void ZhugProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    engine.process (buffer, settings());
    finishBlock (buffer); // smooth preset changes
}

juce::AudioProcessorEditor* ZhugProcessor::createEditor() { return skazi::withUpdateNotice (new ZhugEditor (*this)); }

const std::vector<skazi::Preset>& ZhugProcessor::presetList() const { return zhug::presets(); }
skazi::Preset ZhugProcessor::randomPreset (juce::Random& rng) const { return zhug::randomPreset (rng); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new ZhugProcessor(); }
