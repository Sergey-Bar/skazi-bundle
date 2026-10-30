#include "PluginProcessor.h"
#include "../../Common/UpdateNotice.h"
#include "PluginEditor.h"

namespace
{
    struct Preset
    {
        const char* name;
        float width, spread, depth, phase, driftRate, driftDepth;
        bool lowOn;
        float lowFreq;
        bool monoBass;
        float mix;
        bool haasOn = false;
        float haasLeft = 0.0f, haasRight = 12.0f, haasAmount = 100.0f;
        bool haasSafe = true;
    };

    //                       name                 width spread depth phase rate  drift  low   freq  mono   mix
    const Preset presets[] {
        { "Init",                100,   0,    0,    0,  0.25f,   0, true, 120, false, 100 },
        { "Mono To Wide",        150,  60,    0,    0,  0.25f,   0, true, 150, false, 100 },
        { "200% Psy Width",      200, 100,    0,    0,  0.25f,   0, true, 120, true,  100 },
        { "Subtle Glue",         115,  20,    0,    0,  0.25f,   0, true, 100, false, 100 },
        { "Vocal Halo",          125,  35,  -25,   45,  0.20f,   0, true, 180, false,  80 },
        { "Psy Lead Spread",     180,  80,   15,   90,  0.25f,   0, true, 200, true,  100 },
        { "Pad Galaxy",          190,  90,  -60,    0,  0.15f,  60, true, 150, false, 100 },
        { "Swirl Motion",        160,  70,  -20,    0,  0.80f, 180, true, 150, false, 100 },
        { "Up Front",            100,  10,   80,    0,  0.25f,   0, true, 120, true,  100 },
        { "Far Away",            140,  40,  -90,   30,  0.10f,  20, true, 160, false, 100 },
        { "Bass Safe Master",    110,  15,    0,    0,  0.25f,   0, true, 100, true,  100 },
        { "Narrow Focus",         60,   0,   30,    0,  0.25f,   0, true, 120, true,  100 },
        { "Mono Collapse",         0,   0,    0,    0,  0.25f,   0, true, 120, true,  100 },
        { "Phase Spin",          170,  75,    0,  180,  2.00f, 180, true, 150, true,  100 },
        { "Wide Drums",          130,  25,   10,    0,  0.25f,   0, true,  90, true,  100 },
        { "Super Stereo",        200, 100,  -40,  270,  0.10f,  40, true, 140, true,  100 },
        //                                                                                      haas  L ms   R ms   amt  safe
        { "Haas Super Wide",     120,  30,    0,    0,  0.25f,   0, true, 150, true,  100,  true, 0.0f,  18.0f, 100, false },
        { "Safe Haas Lead",      140,  40,    0,    0,  0.25f,   0, true, 180, true,  100,  true, 0.23f, 11.0f, 100, true },
        { "Twin Delay Wide",     130,  20,  -15,    0,  0.25f,   0, true, 160, true,  100,  true, 7.3f,  13.1f,  85, true },
        { "Micro Haas Shimmer",  110,  20,   10,    0,  0.25f,   0, true, 200, true,  100,  true, 0.23f, 0.9f,  100, true },
        { "LEAD: Psy Saw Wall",        190,  85,   10,   0, 0.25f,   0, true, 180, true, 100 },
        { "LEAD: Goa Ribbon",          170,  70,    0,  60, 0.30f,  30, true, 200, true, 100 },
        { "LEAD: Acid Halo",           150,  50,  -10, 120, 0.50f,  40, true, 220, true, 100 },
        { "LEAD: Pluck Spray",         160,  60,   20,   0, 0.25f,   0, true, 250, true, 100, true, 0.23f, 6.50f,  70, true },
        { "LEAD: Supersaw Max",        200, 100,    0,   0, 0.25f,   0, true, 150, true, 100, true, 0.00f, 14.00f,  80, true },
        { "LEAD: Laser Focus",         120,  25,   40,   0, 0.25f,   0, true, 200, true, 100 },
        { "LEAD: Dual Delay Lead",     140,  30,    0,   0, 0.25f,   0, true, 200, true, 100, true, 4.20f, 9.80f,  90, true },
        { "PAD: Nebula",               200, 100,  -70,   0, 0.08f,  90, true, 140, false, 100 },
        { "PAD: Slow Spin",            180,  90,  -30,   0, 0.12f, 180, true, 150, false, 100 },
        { "PAD: Cathedral",            190,  80, -100,  45, 0.05f,  30, true, 160, false, 100 },
        { "PAD: Glass Wide",           170,  75,   10, 200, 0.20f,  20, true, 180, false, 100 },
        { "PAD: Deep Ocean",           160,  70,  -80,   0, 0.10f,  60, true, 120, true, 100, true, 9.00f, 21.00f,  60, true },
        { "PAD: Aurora Drift",         200,  95,  -50,  90, 0.30f, 120, true, 140, false, 100 },
        { "VOX: Lead Air",             115,  20,   25,   0, 0.25f,   0, true, 200, true, 100 },
        { "VOX: Double Track",         130,  30,    0,   0, 0.25f,   0, true, 180, true, 100, true, 0.00f, 11.00f,  70, true },
        { "VOX: Backing Wide",         170,  70,  -20,   0, 0.25f,   0, true, 200, true, 100 },
        { "VOX: Distant Choir",        160,  60,  -85,  30, 0.10f,  25, true, 220, true, 100 },
        { "VOX: Whisper Halo",         140,  45,  -40,  90, 0.20f,  30, true, 250, true,  80 },
        { "DRUMS: Overheads",          125,  10,    0,   0, 0.25f,   0, true, 250, true, 100 },
        { "DRUMS: Hats Spray",         160,  60,   10,   0, 0.25f,   0, true, 400, true, 100 },
        { "DRUMS: Room Bloom",         140,  30,  -60,   0, 0.25f,   0, true, 200, true, 100 },
        { "DRUMS: Clap Wide",          150,  50,    0,   0, 0.25f,   0, true, 300, true, 100, true, 0.00f, 8.00f,  60, true },
        { "DRUMS: Tight Center",        80,   0,   30,   0, 0.25f,   0, true, 200, true, 100 },
        { "DRUMS: Perc Swirl",         170,  70,    0,   0, 1.50f, 120, true, 350, true, 100 },
        { "BASS: Mono Lock",           100,   0,    0,   0, 0.25f,   0, true, 200, true, 100 },
        { "BASS: Top Wide Sub Mono",   160,  50,    0,   0, 0.25f,   0, true, 180, true, 100 },
        { "BASS: Reese Spread",        180,  70,    0,  90, 0.30f,  30, true, 150, true, 100 },
        { "BASS: Growl Edge",          140,  40,   20,   0, 0.25f,   0, true, 250, true, 100 },
        { "FX: Riser Explode",         200, 100,  -30,   0, 2.50f, 180, true, 200, true, 100, true, 0.00f, 25.00f, 100, false },
        { "FX: Haas Insane",           200,  60,    0,   0, 0.25f,   0, true, 150, true, 100, true, 0.00f, 30.00f, 100, false },
        { "FX: Wobble Image",          150,  80,    0,   0, 4.00f, 180, true, 150, true, 100 },
        { "FX: Phase Tunnel",          180, 100,  -60,   0, 6.00f, 180, true, 120, false, 100 },
        { "FX: Ping Field",            160,  40,    0,   0, 0.25f,   0, true, 200, true, 100, true, 17.00f, 3.00f, 100, false },
        { "FX: Space Throw",           200,  90, -100, 180, 0.05f,  90, true, 150, false, 100 },
        { "FX: Micro Shift",           110,  10,    0,   0, 0.25f,   0, false, 120, false, 100, true, 0.23f, 0.47f, 100, false },
        { "MIX: Gentle Master",        108,  10,    0,   0, 0.25f,   0, true, 120, true, 100 },
        { "MIX: Wide Master",          125,  25,    0,   0, 0.25f,   0, true, 140, true, 100 },
        { "MIX: Club Mono Safe",       115,  15,    0,   0, 0.25f,   0, true, 160, true, 100 },
        { "MIX: Bus Glue Wide",        120,  20,    5,   0, 0.25f,   0, true, 100, false,  70 },
        { "MIX: Narrow Check",          50,   0,    0,   0, 0.25f,   0, true, 120, true, 100 },
        { "MIX: Parallel Width",       200, 100,    0,   0, 0.25f,   0, true, 150, true,  40 },
        { "KEYS: Wide Piano",          140,  40,    0,   0, 0.25f,   0, true, 150, false, 100 },
        { "KEYS: Rhodes Swirl",        150,  60,    0,   0, 0.60f,  90, true, 150, false, 100 },
        { "GTR: Double Guitar",        140,  30,    0,   0, 0.25f,   0, true, 150, true, 100, true, 0.00f, 13.00f,  85, true },
        { "GTR: Acoustic Air",         125,  25,   15,   0, 0.25f,   0, true, 140, false, 100 },
        { "ARP: Spiral Arp",           170,  80,    0,   0, 1.00f, 150, true, 200, true, 100 },
        { "ARP: Haas Bounce",          150,  40,    0,   0, 0.25f,   0, true, 200, true, 100, true, 0.00f, 7.50f, 100, true },
        { "PSY: Full On Air",          180,  70,   10,  30, 0.25f,   0, true, 160, true, 100 },
        { "PSY: Darkpsy Void",         190,  90,  -70, 270, 0.40f, 100, true, 150, true, 100 },
        { "PSY: Morning Sun",          170,  60,   20,  45, 0.15f,  40, true, 170, true, 100, true, 0.23f, 9.00f,  60, true },
    };

    constexpr int numPresets = (int) std::size (presets);
}

SkwideAudioProcessor::SkwideAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "SKWIDE", createParameterLayout())
{
    widthParam      = raw (ParamIDs::width);
    spreadParam     = raw (ParamIDs::spread);
    depthParam      = raw (ParamIDs::depth);
    phaseParam      = raw (ParamIDs::phase);
    driftRateParam  = raw (ParamIDs::driftRate);
    driftDepthParam = raw (ParamIDs::driftDepth);
    lowOnParam      = raw (ParamIDs::lowOn);
    lowFreqParam    = raw (ParamIDs::lowFreq);
    monoBassParam   = raw (ParamIDs::monoBass);
    mixParam        = raw (ParamIDs::mix);
    outputParam     = raw (ParamIDs::output);
    swapParam       = raw (ParamIDs::swap);
    monoCheckParam  = raw (ParamIDs::monoCheck);
    haasOnParam     = raw (ParamIDs::haasOn);
    haasLeftParam   = raw (ParamIDs::haasLeft);
    haasRightParam  = raw (ParamIDs::haasRight);
    haasAmountParam = raw (ParamIDs::haasAmount);
    haasSafeParam   = raw (ParamIDs::haasSafe);
}

juce::AudioProcessorValueTreeState::ParameterLayout SkwideAudioProcessor::createParameterLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    auto percent = [] (float v, int) { return String (roundToInt (v)) + "%"; };
    auto degrees = [] (float v, int) { return String (roundToInt (v)) + juce::String::fromUTF8 ("\xc2\xb0"); };

    auto addFloat = [&] (const char* id, const char* name, NormalisableRange<float> range, float def,
                         std::function<String (float, int)> toText)
    {
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, range, def,
                                                           AudioParameterFloatAttributes().withStringFromValueFunction (std::move (toText))));
    };

    addFloat (ParamIDs::width, "Width", { 0.0f, 200.0f, 0.1f }, 100.0f, percent);
    addFloat (ParamIDs::spread, "Mono Spread", { 0.0f, 100.0f, 0.1f }, 0.0f, percent);
    addFloat (ParamIDs::depth, "Depth", { -100.0f, 100.0f, 0.1f }, 0.0f,
              [] (float v, int) { return std::abs (v) < 0.5f ? String ("CENTER") : (v > 0 ? "NEAR " : "FAR ") + String (roundToInt (std::abs (v))); });
    addFloat (ParamIDs::phase, "Phase", { 0.0f, 360.0f, 0.1f }, 0.0f, degrees);
    addFloat (ParamIDs::driftRate, "Drift Rate", NormalisableRange<float> (0.02f, 8.0f, 0.0f, 0.35f), 0.25f,
              [] (float v, int) { return String (v, v < 1.0f ? 2 : 1) + " Hz"; });
    addFloat (ParamIDs::driftDepth, "Drift Depth", { 0.0f, 180.0f, 0.1f }, 0.0f, degrees);
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { ParamIDs::lowOn, 1 }, "Low Bypass", true));
    addFloat (ParamIDs::lowFreq, "Low Freq", NormalisableRange<float> (20.0f, 500.0f, 0.0f, 0.45f), 120.0f,
              [] (float v, int) { return String (roundToInt (v)) + " Hz"; });
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { ParamIDs::monoBass, 1 }, "Mono Bass", false));
    addFloat (ParamIDs::mix, "Mix", { 0.0f, 100.0f, 0.1f }, 100.0f, percent);
    addFloat (ParamIDs::output, "Output", { -18.0f, 18.0f, 0.1f }, 0.0f,
              [] (float v, int) { return (v > 0.05f ? "+" : "") + String (v, 1) + " dB"; });
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { ParamIDs::swap, 1 }, "Swap L/R", false));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { ParamIDs::monoCheck, 1 }, "Mono Check", false));

    auto ms = [] (float v, int) { return String (v, 2) + " ms"; };
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { ParamIDs::haasOn, 1 }, "Super Wide", false));
    addFloat (ParamIDs::haasLeft, "Delay L", NormalisableRange<float> (0.0f, 30.0f, 0.01f, 0.4f), 0.0f, ms);
    addFloat (ParamIDs::haasRight, "Delay R", NormalisableRange<float> (0.0f, 30.0f, 0.01f, 0.4f), 12.0f, ms);
    addFloat (ParamIDs::haasAmount, "Haas Amount", { 0.0f, 100.0f, 0.1f }, 100.0f, percent);
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { ParamIDs::haasSafe, 1 }, "Haas Mono Safe", true));

    return layout;
}

bool SkwideAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::stereo())
        return false;
    const auto in = layouts.getMainInputChannelSet();
    return in == juce::AudioChannelSet::stereo() || in == juce::AudioChannelSet::mono();
}

void SkwideAudioProcessor::prepareToPlay (double newSampleRate, int)
{
    sampleRate = newSampleRate;
    liveSampleRate = newSampleRate;

    for (auto* s : { &width, &spread, &depth, &phase, &driftDepth, &lowAmount, &splitAmount, &bassKeep, &mix, &outGain, &sideSign, &monoAmount, &lowFreq,
                     &haasAmount, &haasSafe })
        s->reset (sampleRate, 0.03);
    phase.reset (sampleRate, 0.08);
    haasLeftSamples.reset (sampleRate, 0.08);
    haasRightSamples.reset (sampleRate, 0.08);

    width.setCurrentAndTargetValue (widthParam->load() * 0.01f);
    spread.setCurrentAndTargetValue (spreadParam->load() * 0.01f);
    depth.setCurrentAndTargetValue (depthParam->load() * 0.01f);
    phase.setCurrentAndTargetValue (phaseParam->load());
    driftDepth.setCurrentAndTargetValue (driftDepthParam->load());
    lowAmount.setCurrentAndTargetValue (lowOnParam->load() > 0.5f ? 1.0f : 0.0f);
    splitAmount.setCurrentAndTargetValue (splitTarget());
    bassKeep.setCurrentAndTargetValue (monoBassParam->load() > 0.5f ? 0.0f : 1.0f);
    mix.setCurrentAndTargetValue (mixParam->load() * 0.01f);
    outGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (outputParam->load()));
    sideSign.setCurrentAndTargetValue (swapParam->load() > 0.5f ? -1.0f : 1.0f);
    monoAmount.setCurrentAndTargetValue (monoCheckParam->load() > 0.5f ? 1.0f : 0.0f);
    lowFreq.setCurrentAndTargetValue (lowFreqParam->load());
    haasAmount.setCurrentAndTargetValue (haasTarget());
    haasSafe.setCurrentAndTargetValue (haasSafeParam->load() > 0.5f ? 1.0f : 0.0f);
    haasLeftSamples.setCurrentAndTargetValue (haasLeftParam->load() * 0.001f * (float) sampleRate);
    haasRightSamples.setCurrentAndTargetValue (haasRightParam->load() * 0.001f * (float) sampleRate);

    for (int ch = 0; ch < 2; ++ch)
    {
        haasDelay[ch].prepare (sampleRate);
        haasHighPass[ch].reset();
    }

    sideSplit.reset();
    midSplit.reset();
    decorrelator.prepare (sampleRate);
    decorrelator.reset();
    rotator.reset();
    diffuser.prepare (sampleRate);
    depthHigh.reset();
    depthPresence.reset();
    depthLow.reset();

    appliedDepth = 1000.0f;
    appliedLowFreq = -1.0f;
    driftPhase = 0.0;
}

// The LR4 split shifts the side's phase around the crossover, so it is only used when the lows need
// different treatment from the rest: LOW BYPASS on and either MONO BASS or a width other than 100%.
float SkwideAudioProcessor::splitTarget() const
{
    const bool needed = lowOnParam->load() > 0.5f && (monoBassParam->load() > 0.5f || std::abs (widthParam->load() - 100.0f) > 0.05f);
    return needed ? 1.0f : 0.0f;
}

float SkwideAudioProcessor::haasTarget() const
{
    return haasOnParam->load() > 0.5f ? haasAmountParam->load() * 0.01f : 0.0f;
}

// DEPTH is a psychoacoustic distance control. Nearer: brighter, more presence, a tighter side.
// Further: the air absorbs the top, the proximity bass fades, the direct sound drops and a
// diffuse early-reflection cloud grows in the side channel only (so mono stays clean).
void SkwideAudioProcessor::updateDepthFilters (float d)
{
    const float nearAmt = juce::jmax (0.0f, d), farAmt = juce::jmax (0.0f, -d);

    depthHigh.highShelf (sampleRate, nearAmt > 0.0f ? 5000.0 : 3200.0, 3.5 * nearAmt - 8.0 * farAmt);
    depthPresence.peak (sampleRate, 2600.0, 0.8, 2.5 * nearAmt - 1.5 * farAmt);
    depthLow.lowShelf (sampleRate, 220.0, 1.0 * nearAmt - 2.5 * farAmt);

    depthLevel = 1.0f - 0.3f * farAmt;
    depthSideFocus = 1.0f - 0.3f * nearAmt;
    depthDiffuse = 0.32f * farAmt;
}

void SkwideAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    const int numIn = getTotalNumInputChannels();
    if (buffer.getNumChannels() < 2)
        return;
    if (numIn == 1)
        buffer.copyFrom (1, 0, buffer, 0, 0, numSamples);

    width.setTargetValue (widthParam->load() * 0.01f);
    spread.setTargetValue (spreadParam->load() * 0.01f);
    depth.setTargetValue (depthParam->load() * 0.01f);
    phase.setTargetValue (phaseParam->load());
    driftDepth.setTargetValue (driftDepthParam->load());
    lowAmount.setTargetValue (lowOnParam->load() > 0.5f ? 1.0f : 0.0f);
    splitAmount.setTargetValue (splitTarget());
    bassKeep.setTargetValue (monoBassParam->load() > 0.5f ? 0.0f : 1.0f);
    mix.setTargetValue (mixParam->load() * 0.01f);
    outGain.setTargetValue (juce::Decibels::decibelsToGain (outputParam->load()));
    sideSign.setTargetValue (swapParam->load() > 0.5f ? -1.0f : 1.0f);
    monoAmount.setTargetValue (monoCheckParam->load() > 0.5f ? 1.0f : 0.0f);
    lowFreq.setTargetValue (lowFreqParam->load());
    haasAmount.setTargetValue (haasTarget());
    haasSafe.setTargetValue (haasSafeParam->load() > 0.5f ? 1.0f : 0.0f);
    haasLeftSamples.setTargetValue (haasLeftParam->load() * 0.001f * (float) sampleRate);
    haasRightSamples.setTargetValue (haasRightParam->load() * 0.001f * (float) sampleRate);

    const double driftInc = driftRateParam->load() / sampleRate;
    auto* left = buffer.getWritePointer (0);
    auto* right = buffer.getWritePointer (1);

    float angleDeg = 0.0f, effectiveSpread = 0.0f;
    constexpr int chunk = 32;

    for (int start = 0; start < numSamples; start += chunk)
    {
        const int end = juce::jmin (numSamples, start + chunk);

        // Filter coefficients follow the smoothed controls once per chunk.
        const float d = depth.getCurrentValue();
        if (std::abs (d - appliedDepth) > 0.002f)
        {
            appliedDepth = d;
            updateDepthFilters (d);
        }
        const float f = lowFreq.getCurrentValue();
        if (std::abs (f - appliedLowFreq) > 0.05f)
        {
            appliedLowFreq = f;
            sideSplit.set (sampleRate, f);
            midSplit.set (sampleRate, f);
            for (auto& hp : haasHighPass)
                hp.set (sampleRate, f);
        }

        for (int i = start; i < end; ++i)
        {
            const float dryL = left[i], dryR = right[i];
            inL.push (dryL);
            inR.push (dryR);

            const float w = width.getNextValue();
            const float low = lowAmount.getNextValue();
            const float split = splitAmount.getNextValue();
            const float keep = bassKeep.getNextValue();
            const float wet = mix.getNextValue();
            const float gain = outGain.getNextValue();
            const float sign = sideSign.getNextValue();
            const float mono = monoAmount.getNextValue();
            depth.getNextValue();
            lowFreq.getNextValue();

            // "Smart" width: past 100% it also drives the mono-spread engine, so even a mono
            // source opens up. Spread and the smart part together are capped below a full null.
            const float spreadAmt = juce::jlimit (0.0f, 1.0f, spread.getNextValue() + juce::jmax (0.0f, w - 1.0f));
            effectiveSpread = spreadAmt;

            const float drift = driftDepth.getNextValue() * (float) std::sin (wide::twoPi * driftPhase);
            driftPhase += driftInc;
            if (driftPhase >= 1.0)
                driftPhase -= 1.0;
            angleDeg = phase.getNextValue() + drift;
            const float angle = juce::degreesToRadians (angleDeg);

            const float mid = 0.5f * (dryL + dryR);
            const float side = 0.5f * (dryL - dryR);

            float sideLow, sideHigh, midLow, midHigh;
            sideSplit.process (side, sideLow, sideHigh);
            midSplit.process (mid, midLow, midHigh);

            // Below the crossover, LOW BYPASS leaves the side as it was (or removes it with MONO BASS).
            // The split only engages when it changes something, so the default settings are bit-transparent.
            float newSide = w * side + split * ((keep - w) * sideLow + w * (sideHigh + sideLow - side));
            newSide *= depthSideFocus;

            // The decorrelator only ever sees the mid above the crossover when LOW BYPASS is on.
            const float source = mid + (midHigh - mid) * low;
            const float decor = rotator.rotate (decorrelator.process (source), std::cos (angle), std::sin (angle));
            newSide += 0.9f * spreadAmt * decor;

            const float cloud = diffuser.process (source);
            newSide += depthDiffuse * cloud;
            newSide *= sign;

            const float newMid = depthLow.process (depthPresence.process (depthHigh.process (mid))) * depthLevel;

            float wetLeft = newMid + newSide, wetRight = newMid - newSide;

            // SUPER WIDE: Haas delays per side. Only the difference to the undelayed signal is
            // added, so LOW BYPASS can keep the delays out of the low end. MONO SAFE then puts the
            // original mid back and keeps only the side the delays create; off, it is classic Haas.
            const float haas = haasAmount.getNextValue();
            const float safe = haasSafe.getNextValue();
            const float delayedL = haasDelay[0].process (wetLeft, haasLeftSamples.getNextValue());
            const float delayedR = haasDelay[1].process (wetRight, haasRightSamples.getNextValue());
            if (haas > 0.0f)
            {
                float diffL = delayedL - wetLeft, diffR = delayedR - wetRight;
                diffL += (haasHighPass[0].process (diffL) - diffL) * low;
                diffR += (haasHighPass[1].process (diffR) - diffR) * low;
                wetLeft += haas * diffL;
                wetRight += haas * diffR;
                const float midShift = 0.5f * (wetLeft + wetRight) - newMid;
                wetLeft -= safe * midShift;
                wetRight -= safe * midShift;
            }

            float outLeft = dryL + (wetLeft - dryL) * wet;
            float outRight = dryR + (wetRight - dryR) * wet;

            const float monoSum = 0.5f * (outLeft + outRight);
            outLeft = (outLeft + (monoSum - outLeft) * mono) * gain;
            outRight = (outRight + (monoSum - outRight) * mono) * gain;

            left[i] = outLeft;
            right[i] = outRight;
            outL.push (outLeft);
            outR.push (outRight);
        }
    }

    for (auto* r : { &inL, &inR, &outL, &outR })
        r->publish();
    livePhase = std::fmod (angleDeg + 720.0f, 360.0f);
    liveSpread = effectiveSpread;

    for (int ch = 2; ch < buffer.getNumChannels(); ++ch)
        buffer.clear (ch, 0, numSamples);
}

// RANDOM: every control except MIX, OUTPUT and MONO CHECK, kept to musical ranges.
void SkwideAudioProcessor::randomise()
{
    auto& r = random;
    auto set = [this] (const char* id, float value)
    {
        if (auto* param = apvts.getParameter (id))
            param->setValueNotifyingHost (param->convertTo0to1 (value));
    };
    auto chance = [&r] (float p) { return r.nextFloat() < p; };
    auto logRange = [&r] (float lo, float hi) { return lo * std::pow (hi / lo, r.nextFloat()); };

    set (ParamIDs::width, chance (0.15f) ? 40.0f + r.nextFloat() * 60.0f : 100.0f + r.nextFloat() * 100.0f);
    set (ParamIDs::spread, r.nextFloat() * 100.0f);
    set (ParamIDs::depth, -90.0f + r.nextFloat() * 140.0f);
    set (ParamIDs::phase, r.nextFloat() * 360.0f);
    set (ParamIDs::driftRate, logRange (0.05f, 3.0f));
    set (ParamIDs::driftDepth, chance (0.5f) ? 0.0f : r.nextFloat() * 180.0f);
    set (ParamIDs::lowOn, chance (0.85f) ? 1.0f : 0.0f);
    set (ParamIDs::lowFreq, logRange (80.0f, 300.0f));
    set (ParamIDs::monoBass, chance (0.6f) ? 1.0f : 0.0f);
    set (ParamIDs::swap, chance (0.1f) ? 1.0f : 0.0f);
    set (ParamIDs::haasOn, chance (0.4f) ? 1.0f : 0.0f);
    set (ParamIDs::haasLeft, chance (0.5f) ? 0.0f : 15.0f * r.nextFloat() * r.nextFloat());
    set (ParamIDs::haasRight, 25.0f * std::sqrt (r.nextFloat()));
    set (ParamIDs::haasAmount, 40.0f + r.nextFloat() * 60.0f);
    set (ParamIDs::haasSafe, chance (0.75f) ? 1.0f : 0.0f);

    randomised = true;
}

int SkwideAudioProcessor::getNumPrograms() { return numPresets; }

const juce::String SkwideAudioProcessor::getProgramName (int index)
{
    return juce::isPositiveAndBelow (index, numPresets) ? presets[index].name : "";
}

void SkwideAudioProcessor::setCurrentProgram (int index)
{
    if (! juce::isPositiveAndBelow (index, numPresets))
        return;

    currentPreset = index;
    randomised = false;
    const auto& p = presets[index];
    auto set = [this] (const char* id, float value)
    {
        if (auto* param = apvts.getParameter (id))
            param->setValueNotifyingHost (param->convertTo0to1 (value));
    };
    set (ParamIDs::width, p.width);
    set (ParamIDs::spread, p.spread);
    set (ParamIDs::depth, p.depth);
    set (ParamIDs::phase, p.phase);
    set (ParamIDs::driftRate, p.driftRate);
    set (ParamIDs::driftDepth, p.driftDepth);
    set (ParamIDs::lowOn, p.lowOn ? 1.0f : 0.0f);
    set (ParamIDs::lowFreq, p.lowFreq);
    set (ParamIDs::monoBass, p.monoBass ? 1.0f : 0.0f);
    set (ParamIDs::mix, p.mix);
    set (ParamIDs::haasOn, p.haasOn ? 1.0f : 0.0f);
    set (ParamIDs::haasLeft, p.haasLeft);
    set (ParamIDs::haasRight, p.haasRight);
    set (ParamIDs::haasAmount, p.haasAmount);
    set (ParamIDs::haasSafe, p.haasSafe ? 1.0f : 0.0f);
}

void SkwideAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("preset", currentPreset, nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void SkwideAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto state = juce::ValueTree::fromXml (*xml);
            currentPreset = juce::jlimit (0, numPresets - 1, (int) state.getProperty ("preset", 0));
            apvts.replaceState (state);
        }
}

juce::AudioProcessorEditor* SkwideAudioProcessor::createEditor()
{
    return skazi::withUpdateNotice (new SkwideAudioProcessorEditor (*this));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SkwideAudioProcessor();
}
