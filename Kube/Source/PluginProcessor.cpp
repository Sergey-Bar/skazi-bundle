#include "PluginProcessor.h"
#include "../../Common/UpdateNotice.h"
#include "PluginEditor.h"
#include "Presets.h"

KubeProcessor::KubeProcessor()
    : SkaziProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true),
                      skazi::buildLayout (kube::paramDefs()), "KUBE")
{
}

bool KubeProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void KubeProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate, samplesPerBlock);
    keyboardState.reset();
}

void KubeProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    keyboardState.processNextMidiBuffer (midi, 0, buffer.getNumSamples(), true);
    engine.process (buffer, midi, settings(), skazi::hostTime (getPlayHead()));
    finishBlock (buffer); // smooth preset changes
}

juce::AudioProcessorEditor* KubeProcessor::createEditor() { return skazi::withUpdateNotice (new KubeEditor (*this)); }

void KubeProcessor::setUserTable (int osc, std::unique_ptr<kube::Wavetable> table)
{
    auto& slot = user[(size_t) osc];
    engine.userTables[(size_t) osc].store (table.get(), std::memory_order_release);
    if (slot != nullptr) retired.push_back (std::move (slot));
    slot = std::move (table);
}

bool KubeProcessor::loadUserTable (int osc, const juce::File& file, juce::String& error)
{
    auto table = kube::loadWavetable (file, error);
    if (table == nullptr) return false;
    setUserTable (osc, std::move (table));
    if (auto* p = apvts.getParameter (osc == 0 ? "aTable" : "bTable"))
        p->setValueNotifyingHost (p->convertTo0to1 ((float) kube::kUserTable));
    return true;
}

void KubeProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto xml = apvts.copyState().createXml();
    if (xml == nullptr) return;
    for (int o = 0; o < 2; ++o)
        if (auto* t = user[(size_t) o].get())
        {
            juce::MemoryOutputStream packed;
            {
                juce::GZIPCompressorOutputStream zip (packed, 9);
                zip.write (t->source().data(), t->source().size() * sizeof (float));
            }
            auto* e = xml->createNewChildElement ("USERWT");
            e->setAttribute ("osc", o);
            e->setAttribute ("name", t->getName());
            e->setAttribute ("frames", t->numFrames());
            e->setAttribute ("data", packed.getMemoryBlock().toBase64Encoding());
        }
    copyXmlToBinary (*xml, destData);
}

void KubeProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr) return;

    std::array<std::unique_ptr<kube::Wavetable>, 2> loaded;
    while (auto* e = xml->getChildByName ("USERWT"))
    {
        const int o = juce::jlimit (0, 1, e->getIntAttribute ("osc"));
        const int frames = juce::jlimit (1, kube::kMaxUserFrames, e->getIntAttribute ("frames", 1));
        juce::MemoryBlock packed;
        if (packed.fromBase64Encoding (e->getStringAttribute ("data")))
        {
            juce::MemoryInputStream in (packed, false);
            juce::GZIPDecompressorInputStream unzip (in);
            std::vector<float> cycles ((size_t) frames * kube::kTableSize);
            const auto bytes = (int) (cycles.size() * sizeof (float));
            if (unzip.read (cycles.data(), bytes) == bytes)
                loaded[(size_t) o] = std::make_unique<kube::Wavetable> (cycles, frames, e->getStringAttribute ("name"), false);
        }
        xml->removeChildElement (e, true);
    }
    for (int o = 0; o < 2; ++o)
        setUserTable (o, std::move (loaded[(size_t) o])); // a state without a table clears the slot

    juce::MemoryBlock rest; // the parameters, as the shared base expects them
    copyXmlToBinary (*xml, rest);
    SkaziProcessor::setStateInformation (rest.getData(), (int) rest.getSize());
}

const std::vector<skazi::Preset>& KubeProcessor::presetList() const { return kube::presets(); }
skazi::Preset KubeProcessor::randomPreset (juce::Random& rng) const { return kube::randomPreset (rng); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new KubeProcessor(); }
