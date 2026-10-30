#include "PluginProcessor.h"
#include "../../Common/UpdateNotice.h"
#include "PluginEditor.h"
#include <cstring>

namespace
{
juce::NormalisableRange<float> skewed (float lo, float hi, float centre)
{
    juce::NormalisableRange<float> r (lo, hi);
    r.setSkewForCentre (centre);
    return r;
}

std::unique_ptr<juce::AudioParameterFloat> param (const char* id, const char* name,
                                                  juce::NormalisableRange<float> range, float def,
                                                  const char* unit)
{
    return std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { id, 1 }, name, range, def, juce::AudioParameterFloatAttributes().withLabel (unit));
}
} // namespace

KickassProcessor::KickassProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "KICKASS", createLayout())
{
    // The parameter defaults are the Init Kick.
    setPresetLabel (0, "Init / Init Kick");
    markPresetClean();
}

juce::AudioProcessorValueTreeState::ParameterLayout KickassProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    layout.add (param ("start",  "Start",       skewed (100.0f, 8000.0f, 1500.0f), 3000.0f, "Hz"));
    layout.add (param ("punch",  "Punch",       skewed (40.0f, 1000.0f, 200.0f), 180.0f, "Hz"));
    layout.add (param ("end",    "End",         skewed (20.0f, 200.0f, 55.0f), 50.0f, "Hz"));
    layout.add (param ("snap",   "Snap",        skewed (0.3f, 30.0f, 4.0f), 3.0f, "ms"));
    layout.add (param ("sweep",  "Sweep",       skewed (2.0f, 300.0f, 40.0f), 35.0f, "ms"));
    layout.add (param ("hold",   "Hold",        skewed (0.0f, 500.0f, 80.0f), 40.0f, "ms"));
    layout.add (param ("decay",  "Decay",       skewed (20.0f, 2000.0f, 300.0f), 300.0f, "ms"));
    layout.add (param ("sub",    "Sub Level",   { 0.0f, 1.0f }, 1.0f, ""));
    // The sample layer keeps its original "click" ids so saved sessions and presets still load.
    layout.add (param ("click",  "Sample Level",  { 0.0f, 1.0f }, 0.3f, ""));
    layout.add (param ("offset", "Sample Offset", { -5.0f, 20.0f, 0.01f }, 0.0f, "ms"));
    layout.add (param ("ctune",  "Sample Tune",   { -24.0f, 24.0f, 0.01f }, 0.0f, "st"));
    layout.add (param ("chpf",   "Sample HPF",    skewed (20.0f, 20000.0f, 1000.0f), 20.0f, "Hz"));
    layout.add (param ("clpf",   "Sample LPF",    skewed (20.0f, 20000.0f, 1000.0f), 20000.0f, "Hz"));
    layout.add (param ("slength", "Sample Length", skewed (20.0f, 2000.0f, 300.0f), 2000.0f, "ms"));
    layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "keytrack", 1 }, "Key Track", false));
    layout.add (param ("drive",  "Drive",       { 0.0f, 1.0f }, 0.0f, ""));
    layout.add (param ("eqlow",  "EQ Low",      { -12.0f, 12.0f, 0.1f }, 0.0f, "dB"));
    layout.add (param ("eqmid",  "EQ Mid",      { -12.0f, 12.0f, 0.1f }, 0.0f, "dB"));
    layout.add (param ("eqmidf", "EQ Mid Freq", skewed (100.0f, 5000.0f, 700.0f), 500.0f, "Hz"));
    layout.add (param ("eqhigh", "EQ High",     { -12.0f, 12.0f, 0.1f }, 0.0f, "dB"));
    layout.add (param ("level",  "Level",       { -24.0f, 6.0f }, -3.0f, "dB"));
    return layout;
}

kick::Params KickassProcessor::currentParams() const
{
    auto get = [this] (const char* id) { return apvts.getRawParameterValue (id)->load(); };
    kick::Params p;
    p.startHz = get ("start");
    p.punchHz = get ("punch");
    p.endHz   = get ("end");
    p.snapMs  = get ("snap");
    p.pitchMs = get ("sweep");
    p.holdMs  = get ("hold");
    p.ampMs   = get ("decay");
    p.click   = get ("click");
    p.offsetMs = get ("offset");
    p.clickTune = get ("ctune");
    p.clickHpfHz = get ("chpf");
    p.clickLpfHz = get ("clpf");
    p.subLevel = get ("sub");
    p.sampleLenMs = get ("slength");
    p.drive   = get ("drive");
    p.eqLowDb  = get ("eqlow");
    p.eqMidDb  = get ("eqmid");
    p.eqMidHz  = get ("eqmidf");
    p.eqHighDb = get ("eqhigh");
    p.levelDb = get ("level");
    return p;
}

void KickassProcessor::prepareToPlay (double sr, int)
{
    sampleRate = sr;
}

bool KickassProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo();
}

void KickassProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();
    keyboardState.processNextMidiBuffer (midi, 0, buffer.getNumSamples(), true);
    engine.setParams (currentParams());
    engine.setClickSample (std::atomic_load (&clickSample));

    const float dt = (float) (1.0 / sampleRate);
    auto* left = buffer.getWritePointer (0);
    int pos = 0;

    auto renderTo = [&] (int end)
    {
        for (; pos < end; ++pos)
            left[pos] = engine.processSample (dt);
    };

    if (uiTrigger.exchange (false))
    {
        engine.trigger();
        ++hitCount;
    }

    for (const auto meta : midi)
    {
        const auto msg = meta.getMessage();
        if (msg.isNoteOn())
        {
            // Key track: C2 (MIDI 36, the GM kick note) plays the kick as designed.
            const bool track = apvts.getRawParameterValue ("keytrack")->load() > 0.5f;
            const float ratio = track ? std::pow (2.0f, (float) (msg.getNoteNumber() - 36) / 12.0f) : 1.0f;
            renderTo (juce::jmin (meta.samplePosition, buffer.getNumSamples()));
            engine.trigger (msg.getFloatVelocity(), ratio);
            ++hitCount;
        }
    }
    renderTo (buffer.getNumSamples());

    const float peak = buffer.getMagnitude (0, 0, buffer.getNumSamples());
    if (peak > outputPeak.load())
        outputPeak.store (peak);

    for (int ch = 1; ch < buffer.getNumChannels(); ++ch)
        buffer.copyFrom (ch, 0, buffer, 0, 0, buffer.getNumSamples());
}

bool KickassProcessor::loadClickSample (const juce::File& file)
{
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));
    if (reader == nullptr || reader->lengthInSamples <= 0)
        return false;

    // A click is a one-shot: cap it at 2 seconds and fold to mono.
    const int length = (int) juce::jmin<juce::int64> (reader->lengthInSamples, (juce::int64) (reader->sampleRate * 2.0));
    juce::AudioBuffer<float> buffer ((int) reader->numChannels, length);
    reader->read (&buffer, 0, length, 0, true, true);

    auto s = std::make_shared<kick::ClickSample>();
    s->sampleRate = reader->sampleRate;
    s->data.assign ((size_t) length, 0.0f);
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        for (int i = 0; i < length; ++i)
            s->data[(size_t) i] += buffer.getSample (ch, i) / (float) buffer.getNumChannels();

    // Peak-normalise, so LEVEL means the same thing for a quiet click and a loud Splice kick.
    float peak = 0.0f;
    for (auto v : s->data)
        peak = juce::jmax (peak, std::abs (v));
    if (peak > 1.0e-4f)
        for (auto& v : s->data)
            v /= peak;

    setClickSample (std::move (s));
    apvts.state.setProperty ("clickFile", file.getFullPathName(), nullptr);
    return true;
}

void KickassProcessor::clearClickSample()
{
    setClickSample (nullptr);
    apvts.state.removeProperty ("clickFile", nullptr);
}

void KickassProcessor::setClickSample (kick::ClickSamplePtr s)
{
    if (auto old = std::atomic_exchange (&clickSample, std::move (s)))
        retired.push_back (std::move (old));

    // Once only we hold a retired sample, the audio thread is done with it.
    retired.erase (std::remove_if (retired.begin(), retired.end(),
                                   [] (const kick::ClickSamplePtr& p) { return p.use_count() == 1; }),
                   retired.end());
}

juce::String KickassProcessor::getClickSampleName() const
{
    const auto path = apvts.state.getProperty ("clickFile").toString();
    return path.isEmpty() ? juce::String() : juce::File (path).getFileNameWithoutExtension();
}

void KickassProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, dest);
}

void KickassProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
        {
            apvts.replaceState (juce::ValueTree::fromXml (*xml));

            const auto path = apvts.state.getProperty ("clickFile").toString();
            if (path.isEmpty() || ! loadClickSample (juce::File (path)))
                setClickSample (nullptr);

            // Reopening a session isn't an edit: whatever was saved is the new baseline.
            markPresetClean();
        }
}

//==============================================================================
const juce::String KickassProcessor::getProgramName (int index)
{
    const auto& bank = kick::factoryPresets();
    if (! juce::isPositiveAndBelow (index, (int) bank.size()))
        return {};
    return juce::String (bank[(size_t) index].category) + " - " + bank[(size_t) index].name;
}

void KickassProcessor::setParam (const juce::String& id, float plainValue)
{
    if (auto* p = apvts.getParameter (id))
        p->setValueNotifyingHost (p->convertTo0to1 (plainValue));
}

void KickassProcessor::applyParams (const kick::Params& p)
{
    setParam ("start", p.startHz);
    setParam ("punch", p.punchHz);
    setParam ("end", p.endHz);
    setParam ("snap", p.snapMs);
    setParam ("sweep", p.pitchMs);
    setParam ("hold", p.holdMs);
    setParam ("decay", p.ampMs);
    setParam ("click", p.click);
    setParam ("offset", p.offsetMs);
    setParam ("drive", p.drive);
    setParam ("eqlow", p.eqLowDb);
    setParam ("eqmid", p.eqMidDb);
    setParam ("eqmidf", p.eqMidHz);
    setParam ("eqhigh", p.eqHighDb);
    setParam ("level", p.levelDb);
    setParam ("ctune", p.clickTune);
    setParam ("chpf", p.clickHpfHz);
    setParam ("clpf", p.clickLpfHz);
    setParam ("sub", p.subLevel);
    setParam ("slength", p.sampleLenMs);
}

void KickassProcessor::setPresetLabel (int index, const juce::String& name, const juce::File& file)
{
    apvts.state.setProperty ("presetIndex", index, nullptr);
    apvts.state.setProperty ("presetName", name, nullptr);
    apvts.state.setProperty ("presetFile", file.getFullPathName(), nullptr);
}

void KickassProcessor::markPresetClean()
{
    loadedParams = currentParams();
    loadedSample = getClickSample();
}

bool KickassProcessor::isPresetModified() const
{
    // loadedParams was read back from the parameters, so range snapping can't show up as an edit.
    const auto now = currentParams();
    return std::memcmp (&now, &loadedParams, sizeof (now)) != 0 || getClickSample() != loadedSample;
}

void KickassProcessor::useSampleAsFullKick()
{
    setParam ("sub", 0.0f);
    setParam ("click", 0.8f);
    setParam ("offset", 0.0f);
    setParam ("ctune", 0.0f);
    setParam ("chpf", 20.0f);
    setParam ("clpf", 20000.0f);
    setParam ("slength", 2000.0f);
}

void KickassProcessor::useSampleAsTopLayer()
{
    // The sample keeps its punch and click above 150 Hz; the synth sub carries the low end.
    setParam ("sub", 1.0f);
    setParam ("click", 0.6f);
    setParam ("ctune", 0.0f);
    setParam ("chpf", 150.0f);
    setParam ("clpf", 20000.0f);
    setParam ("slength", 2000.0f);
}

void KickassProcessor::loadFactoryPreset (int index)
{
    const auto& bank = kick::factoryPresets();
    index = juce::jlimit (0, (int) bank.size() - 1, index);
    const auto& preset = bank[(size_t) index];

    applyParams (preset.params);
    clearClickSample();
    setPresetLabel (index, juce::String (preset.category) + " / " + preset.name);
    markPresetClean();
}

juce::File KickassProcessor::getUserPresetFolder()
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
        .getChildFile ("KICKASS").getChildFile ("Presets");
}

juce::Array<juce::File> KickassProcessor::getUserPresets()
{
    auto files = getUserPresetFolder().findChildFiles (juce::File::findFiles, false, "*.xml");
    files.sort();
    return files;
}

bool KickassProcessor::saveUserPreset (const juce::File& file)
{
    const auto name = "User / " + file.getFileNameWithoutExtension();
    auto state = apvts.copyState();
    state.setProperty ("presetIndex", -1, nullptr);
    state.setProperty ("presetName", name, nullptr);
    state.setProperty ("presetFile", file.getFullPathName(), nullptr);

    if (auto xml = state.createXml())
    {
        file.getParentDirectory().createDirectory();
        if (xml->writeTo (file))
        {
            setPresetLabel (-1, name, file);
            markPresetClean();
            return true;
        }
    }
    return false;
}

bool KickassProcessor::loadUserPreset (const juce::File& file)
{
    auto xml = juce::XmlDocument::parse (file);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType()))
        return false;

    // Go through the parameters (not replaceState) so the host sees every change.
    const auto tree = juce::ValueTree::fromXml (*xml);
    for (const auto& child : tree)
        if (child.hasProperty ("id") && child.hasProperty ("value"))
            setParam (child["id"].toString(), (float) child["value"]);

    const auto clickPath = tree.getProperty ("clickFile").toString();
    if (clickPath.isEmpty() || ! loadClickSample (juce::File (clickPath)))
        clearClickSample();

    setPresetLabel (-1, "User / " + file.getFileNameWithoutExtension(), file);
    markPresetClean();
    return true;
}

void KickassProcessor::stepPreset (int delta)
{
    // One ring: factory kicks first, then the user folder.
    const auto user = getUserPresets();
    const int numFactory = kick::numFactoryPresets();
    const int total = numFactory + user.size();

    int pos = getPresetIndex();
    if (pos < 0)
    {
        const int u = user.indexOf (getUserPresetFile());
        pos = u >= 0 ? numFactory + u : 0;
    }

    pos = ((pos + delta) % total + total) % total;
    if (pos < numFactory)
        loadFactoryPreset (pos);
    else if (! loadUserPreset (user[pos - numFactory]))
        loadFactoryPreset (0);
}

juce::String KickassProcessor::getPresetDisplayName() const
{
    auto name = apvts.state.getProperty ("presetName").toString();
    if (name.isEmpty())
        name = "Init / Init Kick";
    return isPresetModified() ? name + " *" : name;
}

juce::AudioProcessorEditor* KickassProcessor::createEditor() { return skazi::withUpdateNotice (new KickassEditor (*this)); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new KickassProcessor(); }
