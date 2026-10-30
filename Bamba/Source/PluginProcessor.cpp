#include "PluginProcessor.h"
#include "../../Common/UpdateNotice.h"
#include "PluginEditor.h"
#include "Presets.h"

BambaProcessor::BambaProcessor()
    : SkaziProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true),
                      skazi::buildLayout (bamba::paramDefs()), "BAMBA")
{
}

bool BambaProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo();
}

void BambaProcessor::prepareToPlay (double sampleRate, int)
{
    engine.prepare (sampleRate);
}

void BambaProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    if (auditionRequest.exchange (false))
        midi.addEvent (juce::MidiMessage::noteOn (1, 60, 1.0f), 0);
    engine.setSample (std::atomic_load (&sampleData));
    engine.render (buffer, midi, settings(), getPlayHead());
    finishBlock (buffer); // smooth preset changes
    midi.clear();
}

void BambaProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    SkaziProcessor::setStateInformation (data, sizeInBytes);
    const auto path = getSampleFile();
    if (path == juce::File() || ! loadSample (path))
        setSample (nullptr); // a moved or deleted Splice file just leaves the synth playing
}

//==============================================================================
bool BambaProcessor::loadSample (const juce::File& file)
{
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));
    if (reader == nullptr || reader->lengthInSamples <= 0 || reader->sampleRate <= 0)
        return false;

    // Long builds are fine, but cap a stray hour-long file at two minutes.
    const int length = (int) juce::jmin<juce::int64> (reader->lengthInSamples, (juce::int64) (reader->sampleRate * 120.0));
    juce::AudioBuffer<float> audio ((int) juce::jlimit (1u, 2u, reader->numChannels), length);
    reader->read (&audio, 0, length, 0, true, true);

    auto s = bamba::makeSample (audio, reader->sampleRate);
    if (s == nullptr)
        return false;
    setSample (std::move (s));
    apvts.state.setProperty ("sampleFile", file.getFullPathName(), nullptr);
    return true;
}

void BambaProcessor::clearSample()
{
    setSample (nullptr);
    apvts.state.removeProperty ("sampleFile", nullptr);
}

void BambaProcessor::setSample (bamba::SamplePtr s)
{
    if (auto old = std::atomic_exchange (&sampleData, std::move (s)))
        retired.push_back (std::move (old));

    // Once only we hold a retired sample, the audio thread is done with it.
    retired.erase (std::remove_if (retired.begin(), retired.end(),
                                   [] (const bamba::SamplePtr& p) { return p.use_count() == 1; }),
                   retired.end());
}

void BambaProcessor::setBrowseFilter (int kind, int length)
{
    apvts.state.setProperty ("browseKind", kind, nullptr);
    apvts.state.setProperty ("browseLength", length, nullptr);
}

std::vector<RiserLibrary::Entry> BambaProcessor::browseList() const
{
    std::vector<RiserLibrary::Entry> list;
    const int kind = getBrowseKind(), length = getBrowseLength();
    for (auto& e : library->getEntries())
        if (e.matches (kind, length))
            list.push_back (e);
    return list;
}

void BambaProcessor::stepSample (int delta)
{
    const auto list = browseList();
    const int n = (int) list.size();
    if (n == 0)
        return;
    int current = -1;
    for (int i = 0; i < n; ++i)
        if (list[(size_t) i].file == getSampleFile())
            current = i;
    const int next = current < 0 ? (delta > 0 ? 0 : n - 1) : ((current + delta) % n + n) % n;
    if (loadSample (list[(size_t) next].file))
        audition(); // browsing is listening
}

// SPLICE presets name the kind of sample they want: keep the loaded one if it fits, otherwise pick one.
void BambaProcessor::presetLoaded (const skazi::Preset& p)
{
    const auto kindIt = p.values.find ("spliceKind");
    if (kindIt == p.values.end())
        return;
    const auto lenIt = p.values.find ("spliceLength");
    setBrowseFilter ((int) kindIt->second, lenIt != p.values.end() ? (int) lenIt->second : 0);

    auto list = browseList();
    if (list.empty())
        return; // no Splice samples (yet): the preset's synth part still plays
    for (auto& e : list)
        if (e.file == getSampleFile())
            return;

    // Prefer real FX over drum and vocal builds when there are any
    std::vector<RiserLibrary::Entry> fx;
    for (auto& e : list)
    {
        const auto name = e.file.getFileName().toLowerCase();
        bool other = false;
        for (auto* w : { "drum", "clap", "snare", "kick", "perc", "vocal", "vox", "hat", "loop" })
            other = other || name.contains (w);
        if (! other) fx.push_back (e);
    }
    if (! fx.empty())
        list = std::move (fx);
    const auto pick = (size_t) (p.name.hashCode() & 0x7fffffff) % list.size();
    loadSample (list[pick].file);
}

juce::AudioProcessorEditor* BambaProcessor::createEditor() { return skazi::withUpdateNotice (new BambaEditor (*this)); }

const std::vector<skazi::Preset>& BambaProcessor::presetList() const { return bamba::presets(); }
skazi::Preset BambaProcessor::randomPreset (juce::Random& rng) const { return bamba::randomPreset (rng); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new BambaProcessor(); }
