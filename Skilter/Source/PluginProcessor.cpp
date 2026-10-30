#include "PluginProcessor.h"
#include "../../Common/UpdateNotice.h"
#include "PluginEditor.h"
#include "Presets.h"

const juce::StringArray LowpassAudioProcessor::slopeNames { "12 dB/oct", "24 dB/oct", "36 dB/oct", "48 dB/oct" };
const juce::StringArray LowpassAudioProcessor::shapeNames { "Sine", "Triangle", "Saw", "Square", "S&H" };
const juce::StringArray LowpassAudioProcessor::divisionNames { "1/32", "1/16", "1/8T", "1/8", "1/4T", "1/4", "1/2", "1 Bar", "2 Bars", "4 Bars" };
const juce::StringArray LowpassAudioProcessor::driveTypeNames { "Tube", "Tape", "Hard" };
const juce::StringArray LowpassAudioProcessor::drivePosNames { "Pre", "Post" };
const juce::StringArray LowpassAudioProcessor::stereoModeNames { "Stereo", "Mid/Side", "Dual L/R" };
const juce::StringArray LowpassAudioProcessor::oversamplingNames { "1x", "2x", "4x" };

juce::String LowpassAudioProcessor::morphName (float morph)
{
    static const char* names[] { "LP", "BP", "HP", "NOTCH" };
    const int nearest = juce::jlimit (0, 3, juce::roundToInt (morph));
    if (std::abs (morph - (float) nearest) < 0.02f)
        return names[nearest];
    const int lower = juce::jlimit (0, 2, (int) std::floor (morph));
    return juce::String (names[lower]) + ">" + names[lower + 1] + " " + juce::String (juce::roundToInt ((morph - (float) lower) * 100.0f)) + "%";
}

namespace
{
    juce::NormalisableRange<float> logRange (float min, float max)
    {
        return { min, max,
                 [] (float start, float end, float t) { return start * std::pow (end / start, t); },
                 [] (float start, float end, float v) { return std::log (v / start) / std::log (end / start); },
                 [] (float start, float end, float v) { return juce::jlimit (start, end, v); } };
    }

    juce::String hzText (float v, int)
    {
        if (v >= 1000.0f)
            return juce::String (v / 1000.0f, 2) + " kHz";
        return (v < 100.0f ? juce::String (v, 1) : juce::String (juce::roundToInt (v))) + " Hz";
    }

    float hzValue (const juce::String& t)
    {
        const auto v = t.getFloatValue();
        return t.containsIgnoreCase ("k") ? v * 1000.0f : v;
    }

    juce::String percentText (float v, int) { return juce::String (juce::roundToInt (v * 100.0f)) + "%"; }

    juce::String bipolarText (float v, int)
    {
        const auto p = juce::roundToInt (v * 100.0f);
        return (p > 0 ? "+" : "") + juce::String (p) + "%";
    }

    float percentValue (const juce::String& t) { return t.getFloatValue() / 100.0f; }

    juce::String resonanceText (float v, int)
    {
        return isSelfOscillating (resonanceToDamping (v)) ? juce::String ("SELF-OSC") : percentText (v, 0);
    }

    juce::String dbText (float v, int) { return (v > 0.05f ? "+" : "") + juce::String (v, 1) + " dB"; }
    juce::String msText (float v, int) { return v < 10.0f ? juce::String (v, 1) + " ms" : juce::String (juce::roundToInt (v)) + " ms"; }
    juce::String rateText (float v, int) { return juce::String (v, v < 1.0f ? 2 : 1) + " Hz"; }
    juce::String degreeText (float v, int) { return juce::String (juce::roundToInt (v)) + " deg"; }
    juce::String octaveText (float v, int) { return (v > 0.005f ? "+" : "") + juce::String (v, 2) + " oct"; }
    juce::String morphText (float v, int) { return LowpassAudioProcessor::morphName (v); }

    std::unique_ptr<juce::AudioParameterFloat> floatParam (const juce::String& id, const juce::String& name, juce::NormalisableRange<float> range, float defaultValue,
                                                           std::function<juce::String (float, int)> toText,
                                                           std::function<float (const juce::String&)> fromText = nullptr)
    {
        auto attributes = juce::AudioParameterFloatAttributes().withStringFromValueFunction (std::move (toText));
        if (fromText)
            attributes = attributes.withValueFromStringFunction (std::move (fromText));
        return std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { id, 1 }, name, range, defaultValue, attributes);
    }

    std::unique_ptr<juce::AudioParameterChoice> choiceParam (const char* id, const char* name, const juce::StringArray& choices, int defaultIndex)
    {
        return std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { id, 1 }, name, choices, defaultIndex);
    }
}

LowpassAudioProcessor::LowpassAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createParameterLayout())
{
    morphParam        = raw (ParamIDs::morph);
    slopeParam        = raw (ParamIDs::slope);
    cutoffParam       = raw (ParamIDs::cutoff);
    resonanceParam    = raw (ParamIDs::resonance);
    glideParam        = raw (ParamIDs::glide);
    driveParam        = raw (ParamIDs::drive);
    driveTypeParam    = raw (ParamIDs::driveType);
    drivePosParam     = raw (ParamIDs::drivePos);
    lfoRateParam      = raw (ParamIDs::lfoRate);
    lfoDepthParam     = raw (ParamIDs::lfoDepth);
    lfoShapeParam     = raw (ParamIDs::lfoShape);
    lfoSyncParam      = raw (ParamIDs::lfoSync);
    lfoDivParam       = raw (ParamIDs::lfoDiv);
    lfoStereoParam    = raw (ParamIDs::lfoStereo);
    envAmountParam    = raw (ParamIDs::envAmount);
    envAttackParam    = raw (ParamIDs::envAttack);
    envReleaseParam   = raw (ParamIDs::envRelease);
    seqOnParam        = raw (ParamIDs::seqOn);
    seqRateParam      = raw (ParamIDs::seqRate);
    seqStepsParam     = raw (ParamIDs::seqSteps);
    seqDepthParam     = raw (ParamIDs::seqDepth);
    seqGlideParam     = raw (ParamIDs::seqGlide);
    macroParam        = raw (ParamIDs::macro);
    macroCutoffParam  = raw (ParamIDs::macroCutoff);
    macroResParam     = raw (ParamIDs::macroRes);
    macroDriveParam   = raw (ParamIDs::macroDrive);
    macroMixParam     = raw (ParamIDs::macroMix);
    macroMorphParam   = raw (ParamIDs::macroMorph);
    inGainParam       = raw (ParamIDs::inGain);
    outGainParam      = raw (ParamIDs::outGain);
    mixParam          = raw (ParamIDs::mix);
    spreadParam       = raw (ParamIDs::spread);
    stereoModeParam   = raw (ParamIDs::stereoMode);
    stereoOffsetParam = raw (ParamIDs::stereoOffset);
    oversamplingParam = raw (ParamIDs::oversampling);

    for (int i = 0; i < ParamIDs::numSteps; ++i)
        seqStepParams[(size_t) i] = apvts.getRawParameterValue (ParamIDs::seqStep (i));

    liveCutoff[0].store (cutoffParam->load());
    liveCutoff[1].store (cutoffParam->load());
}

juce::AudioProcessorValueTreeState::ParameterLayout LowpassAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // Filter core.
    layout.add (floatParam (ParamIDs::morph, "Morph", { 0.0f, 3.0f }, 0.0f, morphText));
    layout.add (choiceParam (ParamIDs::slope, "Slope", slopeNames, 1));
    layout.add (floatParam (ParamIDs::cutoff, "Cutoff", logRange (20.0f, 20000.0f), 20000.0f, hzText, hzValue));
    layout.add (floatParam (ParamIDs::resonance, "Resonance", { 0.0f, 1.0f }, 0.0f, resonanceText, percentValue));
    layout.add (floatParam (ParamIDs::glide, "Glide", { 0.0f, 2000.0f, 0.0f, 0.35f }, 20.0f, msText));

    // Drive.
    layout.add (floatParam (ParamIDs::drive, "Drive", { 0.0f, 24.0f }, 0.0f, dbText));
    layout.add (choiceParam (ParamIDs::driveType, "Drive Type", driveTypeNames, 0));
    layout.add (choiceParam (ParamIDs::drivePos, "Drive Position", drivePosNames, 0));

    // LFO.
    layout.add (floatParam (ParamIDs::lfoRate, "LFO Rate", logRange (0.02f, 20.0f), 1.0f, rateText));
    layout.add (floatParam (ParamIDs::lfoDepth, "LFO Depth", { -1.0f, 1.0f }, 0.0f, bipolarText, percentValue));
    layout.add (choiceParam (ParamIDs::lfoShape, "LFO Shape", shapeNames, 0));
    layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { ParamIDs::lfoSync, 1 }, "LFO Sync", false));
    layout.add (choiceParam (ParamIDs::lfoDiv, "LFO Division", divisionNames, 5));
    layout.add (floatParam (ParamIDs::lfoStereo, "LFO Stereo Phase", { 0.0f, 180.0f }, 0.0f, degreeText));

    // Envelope follower.
    layout.add (floatParam (ParamIDs::envAmount, "Env Amount", { -1.0f, 1.0f }, 0.0f, bipolarText, percentValue));
    layout.add (floatParam (ParamIDs::envAttack, "Env Attack", logRange (0.1f, 200.0f), 5.0f, msText));
    layout.add (floatParam (ParamIDs::envRelease, "Env Release", logRange (5.0f, 3000.0f), 150.0f, msText));

    // Step sequencer. Every step is its own automatable parameter.
    layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { ParamIDs::seqOn, 1 }, "Seq On", false));
    layout.add (choiceParam (ParamIDs::seqRate, "Seq Rate", divisionNames, 1));
    layout.add (std::make_unique<juce::AudioParameterInt> (juce::ParameterID { ParamIDs::seqSteps, 1 }, "Seq Steps", 1, ParamIDs::numSteps, 16,
                                                           juce::AudioParameterIntAttributes().withStringFromValueFunction ([] (int v, int) { return juce::String (v) + " st"; })));
    layout.add (floatParam (ParamIDs::seqDepth, "Seq Depth", { -1.0f, 1.0f }, 0.5f, bipolarText, percentValue));
    layout.add (floatParam (ParamIDs::seqGlide, "Seq Glide", { 0.0f, 1.0f }, 0.15f, percentText, percentValue));

    static constexpr float pattern[] { 1.0f, 0.0f, 0.5f, 0.0f, -0.5f, 0.0f, 0.75f, 0.25f, 1.0f, 0.0f, 0.5f, -0.25f, -0.75f, 0.0f, 0.5f, 0.0f };
    for (int i = 0; i < ParamIDs::numSteps; ++i)
        layout.add (floatParam (ParamIDs::seqStep (i), "Seq Step " + juce::String (i + 1), { -1.0f, 1.0f }, pattern[i], bipolarText, percentValue));

    // Macro: one knob drives five destinations by their own bipolar amounts.
    layout.add (floatParam (ParamIDs::macro, "Macro", { 0.0f, 1.0f }, 0.0f, percentText, percentValue));
    layout.add (floatParam (ParamIDs::macroCutoff, "Macro > Cutoff", { -1.0f, 1.0f }, -0.75f, bipolarText, percentValue));
    layout.add (floatParam (ParamIDs::macroRes, "Macro > Resonance", { -1.0f, 1.0f }, 0.35f, bipolarText, percentValue));
    layout.add (floatParam (ParamIDs::macroDrive, "Macro > Drive", { -1.0f, 1.0f }, 0.25f, bipolarText, percentValue));
    layout.add (floatParam (ParamIDs::macroMix, "Macro > Mix", { -1.0f, 1.0f }, 0.0f, bipolarText, percentValue));
    layout.add (floatParam (ParamIDs::macroMorph, "Macro > Morph", { -1.0f, 1.0f }, 0.0f, bipolarText, percentValue));

    // Output and routing.
    layout.add (floatParam (ParamIDs::inGain, "Input Gain", { -24.0f, 24.0f }, 0.0f, dbText));
    layout.add (floatParam (ParamIDs::outGain, "Output Gain", { -24.0f, 24.0f }, 0.0f, dbText));
    layout.add (floatParam (ParamIDs::mix, "Mix", { 0.0f, 1.0f }, 1.0f, percentText, percentValue));
    layout.add (floatParam (ParamIDs::spread, "Stereo Spread", { 0.0f, 1.0f }, 0.0f, percentText, percentValue));
    layout.add (choiceParam (ParamIDs::stereoMode, "Stereo Mode", stereoModeNames, 0));
    layout.add (floatParam (ParamIDs::stereoOffset, "R/Side Offset", { -4.0f, 4.0f }, 0.0f, octaveText));
    layout.add (choiceParam (ParamIDs::oversampling, "Oversampling", oversamplingNames, 1));

    return layout;
}

double LowpassAudioProcessor::divisionBeats (int index)
{
    static constexpr double beats[] { 0.125, 0.25, 1.0 / 3.0, 0.5, 2.0 / 3.0, 1.0, 2.0, 4.0, 8.0, 16.0 };
    return beats[juce::jlimit (0, 9, index)];
}

bool LowpassAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return layouts.getMainInputChannelSet() == out;
}

void LowpassAudioProcessor::prepareToPlay (double newSampleRate, int samplesPerBlock)
{
    baseRate = newSampleRate;
    liveSampleRate.store (newSampleRate);

    for (int i = 0; i < 2; ++i)
    {
        oversamplers[i] = std::make_unique<juce::dsp::Oversampling<float>> (2, (size_t) i + 1,
                                                                             juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, true);
        oversamplers[i]->initProcessing ((size_t) samplesPerBlock);
    }

    inGain.reset (newSampleRate, 0.02);
    inGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (inGainParam->load()));

    lfoPhase = 0.0;
    seqPosition = 0.0;
    seqValue = 0.0f;
    envelope = 0.0f;
    logCutoff = std::log2 (cutoffParam->load());

    oversamplingIndex = -1;
    configureOversampling (juce::roundToInt (oversamplingParam->load()));
}

void LowpassAudioProcessor::configureOversampling (int index)
{
    oversamplingIndex = juce::jlimit (0, 2, index);
    engineRate = baseRate * (double) (1 << oversamplingIndex);

    for (auto& os : oversamplers)
        if (os != nullptr)
            os->reset();
    for (auto& channel : stages)
        for (auto& stage : channel)
            stage.reset();

    auto prime = [this] (juce::SmoothedValue<float>& value, float target)
    {
        value.reset (engineRate, 0.02);
        value.setCurrentAndTargetValue (target);
    };
    prime (outGain, juce::Decibels::decibelsToGain (outGainParam->load()));
    prime (mix, mixParam->load());
    prime (driveDb, driveParam->load());
    prime (resonance, resonanceParam->load());
    prime (morph, morphParam->load());
    prime (lfoDepth, lfoDepthParam->load());
    prime (envAmount, envAmountParam->load());
    prime (spread, spreadParam->load());
    prime (offset, stereoOffsetParam->load());
    prime (macro, macroParam->load());
    prime (seqDepth, seqDepthParam->load());

    // The IIR half-band filters add a few samples of delay; tell the host so it can compensate.
    setLatencySamples (oversamplingIndex == 0 ? 0 : juce::roundToInt (oversamplers[oversamplingIndex - 1]->getLatencyInSamples()));
}

void LowpassAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    for (auto i = getTotalNumInputChannels(); i < getTotalNumOutputChannels(); ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    const int numChannels = juce::jmin (2, buffer.getNumChannels());
    const int numSamples = buffer.getNumSamples();
    if (numChannels == 0 || numSamples == 0)
        return;

    const int requestedOversampling = juce::roundToInt (oversamplingParam->load());
    if (requestedOversampling != oversamplingIndex)
        configureOversampling (requestedOversampling);

    auto* left = buffer.getWritePointer (0);
    auto* right = numChannels > 1 ? buffer.getWritePointer (1) : nullptr;

    // Input gain at the host rate, then feed the analyzer.
    inGain.setTargetValue (juce::Decibels::decibelsToGain (inGainParam->load()));
    float inPeak = 0.0f;
    for (int n = 0; n < numSamples; ++n)
    {
        const float g = inGain.getNextValue();
        left[n] *= g;
        if (right != nullptr)
            right[n] *= g;
        const float l = left[n], r = right != nullptr ? right[n] : l;
        inPeak = juce::jmax (inPeak, std::abs (l), std::abs (r));
        inputRing.push (0.5f * (l + r));
    }

    const int stereoMode = numChannels == 2 ? juce::roundToInt (stereoModeParam->load()) : 0;
    if (stereoMode == 1)
        for (int n = 0; n < numSamples; ++n)
        {
            const float mid = 0.5f * (left[n] + right[n]);
            const float side = 0.5f * (left[n] - right[n]);
            left[n] = mid;
            right[n] = side;
        }

    juce::dsp::AudioBlock<float> block (buffer.getArrayOfWritePointers(), (size_t) numChannels, (size_t) numSamples);
    if (oversamplingIndex > 0)
    {
        auto& os = *oversamplers[oversamplingIndex - 1];
        runEngine (os.processSamplesUp (block), stereoMode);
        os.processSamplesDown (block);
    }
    else
    {
        runEngine (block, stereoMode);
    }

    if (stereoMode == 1)
        for (int n = 0; n < numSamples; ++n)
        {
            const float mid = left[n], side = right[n];
            left[n] = mid + side;
            right[n] = mid - side;
        }

    // Final safety ceiling at the host rate: downsampling filters and the M/S decode can overshoot
    // the engine's own limiter, so round off anything past 0 dBFS once more here.
    float outPeak = 0.0f;
    for (int n = 0; n < numSamples; ++n)
    {
        left[n] = softLimit (left[n]);
        if (right != nullptr)
            right[n] = softLimit (right[n]);
        const float l = left[n], r = right != nullptr ? right[n] : l;
        outPeak = juce::jmax (outPeak, std::abs (l), std::abs (r));
        outputRing.push (0.5f * (l + r));
    }

    inputRing.publish();
    outputRing.publish();
    inputPeak.store (juce::jmax (inputPeak.load(), inPeak));
    outputPeak.store (juce::jmax (outputPeak.load(), outPeak));
}

void LowpassAudioProcessor::runEngine (juce::dsp::AudioBlock<float> block, int stereoMode)
{
    const int numChannels = (int) block.getNumChannels();
    const int numSamples = (int) block.getNumSamples();
    const bool stereo = numChannels == 2;
    const auto fs = static_cast<float> (engineRate);
    const float pi = juce::MathConstants<float>::pi;

    const int numStages = juce::roundToInt (slopeParam->load()) + 1;
    if (numStages > activeStages)
        for (auto& channel : stages)
            for (int s = activeStages; s < numStages; ++s)
                channel[s].reset();
    activeStages = numStages;

    outGain.setTargetValue (juce::Decibels::decibelsToGain (outGainParam->load()));
    mix.setTargetValue (mixParam->load());
    driveDb.setTargetValue (driveParam->load());
    resonance.setTargetValue (resonanceParam->load());
    morph.setTargetValue (morphParam->load());
    lfoDepth.setTargetValue (lfoDepthParam->load());
    envAmount.setTargetValue (envAmountParam->load());
    spread.setTargetValue (spreadParam->load());
    offset.setTargetValue (stereoOffsetParam->load());
    macro.setTargetValue (macroParam->load());
    seqDepth.setTargetValue (seqDepthParam->load());

    const float maxCutoff = juce::jmin (20000.0f, static_cast<float> (baseRate) * 0.45f);
    const float targetLog = std::log2 (cutoffParam->load());
    const float glideMs = glideParam->load();
    const float glideCoef = glideMs < 0.5f ? 0.0f : std::exp (-1.0f / (glideMs * 0.001f * fs));
    const float attackCoef = std::exp (-1.0f / (envAttackParam->load() * 0.001f * fs));
    const float releaseCoef = std::exp (-1.0f / (envReleaseParam->load() * 0.001f * fs));

    // Host tempo and position drive both the synced LFO and the sequencer.
    double bpm = 120.0, ppq = 0.0;
    bool playing = false;
    if (auto* playHead = getPlayHead())
        if (auto position = playHead->getPosition())
        {
            if (auto hostBpm = position->getBpm())
                bpm = *hostBpm;
            if (auto hostPpq = position->getPpqPosition())
            {
                ppq = *hostPpq;
                playing = position->getIsPlaying();
            }
        }

    double phaseInc = lfoRateParam->load() / engineRate;
    if (lfoSyncParam->load() > 0.5f)
    {
        const double beats = divisionBeats (juce::roundToInt (lfoDivParam->load()));
        if (playing)
        {
            lfoPhase = std::fmod (ppq / beats, 1.0);
            if (lfoPhase < 0.0)
                lfoPhase += 1.0;
        }
        phaseInc = bpm / 60.0 / beats / engineRate;
    }

    const bool seqEnabled = seqOnParam->load() > 0.5f;
    const int seqLength = juce::jlimit (1, ParamIDs::numSteps, juce::roundToInt (seqStepsParam->load()));
    const double beatsPerStep = divisionBeats (juce::roundToInt (seqRateParam->load()));
    const double stepInc = bpm / 60.0 / beatsPerStep / engineRate;
    if (playing)
        seqPosition = ppq / beatsPerStep;
    const float stepSeconds = static_cast<float> (60.0 / bpm * beatsPerStep);
    const float seqGlide = seqGlideParam->load();
    const float seqCoef = seqGlide < 0.001f ? 0.0f : std::exp (-1.0f / (seqGlide * stepSeconds * 0.33f * fs));
    float stepValues[ParamIDs::numSteps];
    for (int i = 0; i < ParamIDs::numSteps; ++i)
        stepValues[i] = seqStepParams[(size_t) i]->load();

    const int shape = juce::roundToInt (lfoShapeParam->load());
    const float stereoOffset = lfoStereoParam->load() / 360.0f;
    const int driveType = juce::roundToInt (driveTypeParam->load());
    const bool drivePost = drivePosParam->load() > 0.5f;
    const float macroCut = macroCutoffParam->load() * 8.0f;
    const float macroRes = macroResParam->load();
    const float macroDrive = macroDriveParam->load() * 24.0f;
    const float macroMix = macroMixParam->load();
    const float macroMorph = macroMorphParam->load() * 3.0f;

    float* channelData[2] { block.getChannelPointer (0), stereo ? block.getChannelPointer (1) : nullptr };
    float lastCutoff[2] { 1000.0f, 1000.0f };
    float lastLfo = 0.0f, env01 = 0.0f, res = 0.0f, damping = 1.0f, morphNow = 0.0f, driveNow = 0.0f, wetMix = 1.0f;
    int step = 0;

    for (int n = 0; n < numSamples; ++n)
    {
        const float x[2] { channelData[0][n], stereo ? channelData[1][n] : 0.0f };

        const float level = juce::jmax (std::abs (x[0]), std::abs (x[1]));
        envelope = level + (level > envelope ? attackCoef : releaseCoef) * (envelope - level);
        env01 = juce::jlimit (0.0f, 1.0f, (juce::Decibels::gainToDecibels (envelope, -60.0f) + 48.0f) / 48.0f);

        lfoPhase += phaseInc;
        if (lfoPhase >= 1.0)
            lfoPhase -= std::floor (lfoPhase);

        seqPosition += stepInc;
        step = ((int) std::floor (seqPosition)) % seqLength;
        if (step < 0)
            step += seqLength;
        const float seqTarget = seqEnabled ? stepValues[step] : 0.0f;
        seqValue = seqTarget + (seqValue - seqTarget) * seqCoef;

        logCutoff = targetLog + (logCutoff - targetLog) * glideCoef;

        const float m = macro.getNextValue();
        res = juce::jlimit (0.0f, 1.0f, resonance.getNextValue() + m * macroRes);
        damping = resonanceToDamping (res);
        morphNow = juce::jlimit (0.0f, 3.0f, morph.getNextValue() + m * macroMorph);
        const auto weights = morphWeights (morphNow);
        driveNow = juce::jlimit (0.0f, 36.0f, driveDb.getNextValue() + m * macroDrive);
        const float driveGain = std::exp (driveNow * 0.11512925f);
        const float driveBlend = juce::jmin (1.0f, (driveGain - 1.0f) * 2.0f);
        wetMix = juce::jlimit (0.0f, 1.0f, mix.getNextValue() + m * macroMix);
        const float outG = outGain.getNextValue();

        const float depthOct = lfoDepth.getNextValue() * 4.0f;
        const float sharedOct = envAmount.getNextValue() * 5.0f * env01 + seqValue * seqDepth.getNextValue() * 4.0f + m * macroCut;
        const float spreadOct = spread.getNextValue() * 0.5f;
        const float offsetOct = offset.getNextValue();

        for (int ch = 0; ch < numChannels; ++ch)
        {
            float phase = static_cast<float> (lfoPhase) + (ch == 1 ? stereoOffset : 0.0f);
            if (phase >= 1.0f)
                phase -= 1.0f;

            auto& hold = sampleHold[ch];
            if (phase < hold.lastPhase)
                hold.value = random.nextFloat() * 2.0f - 1.0f;
            hold.lastPhase = phase;

            const float lfo = lfoValue (shape, phase, hold.value);
            if (ch == 0)
                lastLfo = lfo;

            float octave = logCutoff + lfo * depthOct + sharedOct;
            if (stereo)
                octave += (ch == 0 ? -spreadOct : spreadOct) + (ch == 1 && stereoMode != 0 ? offsetOct : 0.0f);

            const float fc = juce::jlimit (16.0f, maxCutoff, std::exp2 (octave));
            lastCutoff[ch] = fc;
            const float g = std::tan (pi * fc / fs);

            float y = x[ch];
            if (! drivePost && driveBlend > 0.0f)
                y += (saturate (driveType, y, driveGain) - y) * driveBlend;

            for (int s = 0; s < numStages; ++s)
            {
                const bool resonant = s == numStages - 1;
                const float R2 = resonant ? damping : 1.41421356f;
                const float h = 1.0f / (1.0f + R2 * g + g * g);
                // A whisper of noise lets a self-oscillating filter start singing in silence.
                const float input = resonant && damping < 0.01f ? y + (random.nextFloat() - 0.5f) * 1.0e-4f : y;
                y = stages[ch][s].process (input, weights, g, R2, h, resonant);
            }

            if (drivePost && driveBlend > 0.0f)
                y += (saturate (driveType, y, driveGain) - y) * driveBlend;

            channelData[ch][n] = (x[ch] + (softLimit (y) - x[ch]) * wetMix) * outG;
        }
    }

    liveCutoff[0].store (lastCutoff[0]);
    liveCutoff[1].store (stereo ? lastCutoff[1] : lastCutoff[0]);
    liveEnvelope.store (env01);
    liveLfo.store (lastLfo);
    liveLfoPhase.store (static_cast<float> (lfoPhase));
    liveMorph.store (morphNow);
    liveResonance.store (res);
    liveDamping.store (damping);
    liveDrive.store (driveNow);
    liveMix.store (wetMix);
    liveSeqStep.store (step);
    liveSeqValue.store (seqValue);
}

int LowpassAudioProcessor::getNumPrograms()
{
    return (int) factoryPresets().size();
}

const juce::String LowpassAudioProcessor::getProgramName (int index)
{
    const auto& presets = factoryPresets();
    return juce::isPositiveAndBelow (index, (int) presets.size()) ? presets[(size_t) index].name : juce::String();
}

void LowpassAudioProcessor::setCurrentProgram (int index)
{
    // Hosts re-send the current program when a project loads; ignore that so the restored state survives.
    if (index != currentPreset)
        loadPreset (index);
}

void LowpassAudioProcessor::setParameter (const juce::String& id, float value)
{
    if (auto* param = apvts.getParameter (id))
    {
        param->beginChangeGesture();
        param->setValueNotifyingHost (param->convertTo0to1 (value));
        param->endChangeGesture();
    }
}

// Presets and RANDOM leave oversampling alone: that is a quality setting, not part of the sound.
void LowpassAudioProcessor::resetParametersForPreset()
{
    for (auto* p : getParameters())
        if (auto* param = dynamic_cast<juce::RangedAudioParameter*> (p))
            if (param->getParameterID() != ParamIDs::oversampling)
            {
                param->beginChangeGesture();
                param->setValueNotifyingHost (param->getDefaultValue());
                param->endChangeGesture();
            }
}

void LowpassAudioProcessor::loadPreset (int index)
{
    const auto& presets = factoryPresets();
    if (! juce::isPositiveAndBelow (index, (int) presets.size()))
        return;

    const auto& preset = presets[(size_t) index];
    resetParametersForPreset();
    for (const auto& [id, value] : preset.values)
        setParameter (id, value);

    currentPreset = index;
    presetName = preset.name;
    updateHostDisplay (ChangeDetails().withProgramChanged (true));
}

void LowpassAudioProcessor::randomise()
{
    juce::Random r;
    auto chance = [&r] (float p) { return r.nextFloat() < p; };
    auto between = [&r] (float lo, float hi) { return lo + (hi - lo) * r.nextFloat(); };
    auto logBetween = [&r] (float lo, float hi) { return lo * std::pow (hi / lo, r.nextFloat()); };

    // Keep the levels where they are so a roll never jumps in volume.
    const float keepIn = inGainParam->load(), keepOut = outGainParam->load();
    resetParametersForPreset();
    setParameter (ParamIDs::inGain, keepIn);
    setParameter (ParamIDs::outGain, keepOut);

    // Filter core: mostly the classic shapes, sometimes somewhere in between.
    static constexpr float morphWeightsTable[] { 0.0f, 0.0f, 0.0f, 1.0f, 2.0f, 2.0f, 3.0f };
    setParameter (ParamIDs::morph, chance (0.7f) ? morphWeightsTable[r.nextInt (7)] : between (0.0f, 3.0f));
    setParameter (ParamIDs::slope, (float) r.nextInt (4));
    setParameter (ParamIDs::cutoff, logBetween (150.0f, 8000.0f));
    setParameter (ParamIDs::resonance, between (0.0f, 0.85f));   // never self-oscillates by accident
    setParameter (ParamIDs::glide, logBetween (5.0f, 300.0f));

    if (chance (0.5f))
    {
        setParameter (ParamIDs::drive, between (3.0f, 18.0f));
        setParameter (ParamIDs::driveType, (float) r.nextInt (3));
        setParameter (ParamIDs::drivePos, (float) r.nextInt (2));
    }

    // Exactly one of LFO / sequencer usually leads, with the envelope as a sometimes-extra.
    const bool useSequencer = chance (0.35f);
    if (! useSequencer || chance (0.25f))
    {
        setParameter (ParamIDs::lfoShape, (float) r.nextInt (5));
        setParameter (ParamIDs::lfoDepth, (chance (0.5f) ? 1.0f : -1.0f) * between (0.2f, 0.8f));
        if (chance (0.6f))
        {
            setParameter (ParamIDs::lfoSync, 1.0f);
            setParameter (ParamIDs::lfoDiv, (float) (1 + r.nextInt (8)));
        }
        else
            setParameter (ParamIDs::lfoRate, logBetween (0.1f, 8.0f));
        if (chance (0.4f))
            setParameter (ParamIDs::lfoStereo, between (30.0f, 180.0f));
    }

    if (useSequencer)
    {
        setParameter (ParamIDs::seqOn, 1.0f);
        static constexpr float rates[] { 1.0f, 1.0f, 3.0f, 2.0f, 5.0f };
        setParameter (ParamIDs::seqRate, rates[r.nextInt (5)]);
        setParameter (ParamIDs::seqSteps, chance (0.7f) ? 16.0f : 8.0f);
        setParameter (ParamIDs::seqDepth, (chance (0.8f) ? 1.0f : -1.0f) * between (0.4f, 1.0f));
        setParameter (ParamIDs::seqGlide, chance (0.5f) ? between (0.0f, 0.1f) : between (0.1f, 0.7f));
        for (int i = 0; i < ParamIDs::numSteps; ++i)
            setParameter (ParamIDs::seqStep (i), (float) (r.nextInt (9) - 4) / 4.0f);
    }

    if (chance (0.35f))
    {
        setParameter (ParamIDs::envAmount, (chance (0.75f) ? 1.0f : -1.0f) * between (0.3f, 0.9f));
        setParameter (ParamIDs::envAttack, logBetween (0.5f, 30.0f));
        setParameter (ParamIDs::envRelease, logBetween (40.0f, 600.0f));
    }

    if (chance (0.4f))
        setParameter (ParamIDs::spread, between (0.2f, 0.8f));
    if (chance (0.15f))
    {
        setParameter (ParamIDs::stereoMode, (float) (1 + r.nextInt (2)));
        setParameter (ParamIDs::stereoOffset, between (-3.0f, 3.0f));
    }
    if (chance (0.2f))
        setParameter (ParamIDs::mix, between (0.4f, 0.9f));

    presetName = "Random " + juce::String::toHexString (r.nextInt (0xffff)).toUpperCase().paddedLeft ('0', 4);
}

juce::AudioProcessorEditor* LowpassAudioProcessor::createEditor()
{
    return skazi::withUpdateNotice (new LowpassAudioProcessorEditor (*this));
}

void LowpassAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("presetIndex", currentPreset, nullptr);
    state.setProperty ("presetName", presetName, nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void LowpassAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            const auto state = juce::ValueTree::fromXml (*xml);
            currentPreset = state.getProperty ("presetIndex", 0);
            presetName = state.getProperty ("presetName", "Init").toString();
            apvts.replaceState (state);
        }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new LowpassAudioProcessor();
}
