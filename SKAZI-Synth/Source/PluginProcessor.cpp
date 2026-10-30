#include "PluginProcessor.h"
#include "../../Common/UpdateNotice.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include "PluginEditor.h"
#include "Presets.h"

namespace
{
    // Transparent below 0.8, then rounds off smoothly towards 1.0
    inline float softLimit (float x)
    {
        const float a = std::abs (x);
        if (a <= 0.8f) return x;
        return std::copysign (0.8f + 0.2f * std::tanh ((a - 0.8f) / 0.2f), x);
    }

    using APF = juce::AudioParameterFloat;
    using APB = juce::AudioParameterBool;
    using API = juce::AudioParameterInt;
    using APC = juce::AudioParameterChoice;
    using Layout = juce::AudioProcessorValueTreeState::ParameterLayout;

    juce::ParameterID pid (const juce::String& s) { return { s, 2 }; }

    void addF (Layout& l, const juce::String& id, const juce::String& name, float lo, float hi, float def, float centre = 0.f)
    {
        juce::NormalisableRange<float> r (lo, hi);
        if (centre > 0.f) r.setSkewForCentre (centre);
        l.add (std::make_unique<APF> (pid (id), name, r, def));
    }
    void addC (Layout& l, const juce::String& id, const juce::String& name, const juce::StringArray& items, int def)
    {
        l.add (std::make_unique<APC> (pid (id), name, items, def));
    }
    void addB (Layout& l, const juce::String& id, const juce::String& name, bool def)
    {
        l.add (std::make_unique<APB> (pid (id), name, def));
    }
    void addI (Layout& l, const juce::String& id, const juce::String& name, int lo, int hi, int def)
    {
        l.add (std::make_unique<API> (pid (id), name, lo, hi, def));
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout AcidanceProcessor::createLayout()
{
    Layout l;

    for (int o = 0; o < 2; ++o)
    {
        const juce::String p = "osc" + juce::String (o + 1) + "_";
        const juce::String n = "Osc " + juce::String (o + 1) + " ";
        addB (l, p + "on", n + "On", o == 0);
        addC (l, p + "wave", n + "Wave", Lists::oscWaves, 1);
        addF (l, p + "wtpos", n + "WT Position", 0.f, 1.f, 0.f);
        addI (l, p + "oct", n + "Octave", -3, 3, 0);
        addI (l, p + "semi", n + "Semi", -12, 12, 0);
        addF (l, p + "fine", n + "Fine", -100.f, 100.f, 0.f);
        addF (l, p + "level", n + "Level", 0.f, 1.f, 0.8f);
        addI (l, p + "uni", n + "Unison", 1, kMaxUnison, 1);
        addF (l, p + "detune", n + "Detune", 0.f, 1.f, 0.25f);
        addF (l, p + "blend", n + "Blend", 0.f, 1.f, 0.5f);
        addF (l, p + "phase", n + "Phase", 0.f, 1.f, 0.f);
        addB (l, p + "rphase", n + "Random Phase", true);
        addB (l, p + "lock", n + "Phase Lock", false);
        addC (l, p + "warp", n + "Warp Mode", Lists::warpModes, 0);
        addF (l, p + "warpamt", n + "Warp Amount", 0.f, 1.f, 0.f);
    }
    addC (l, "sub_wave", "Sub Wave", Lists::subWaves, 0);
    addC (l, "sub_oct", "Sub Octave", Lists::subOcts, 0);
    addF (l, "sub_level", "Sub Level", 0.f, 1.f, 0.f);
    addC (l, "noise_type", "Noise Type", Lists::noiseTypes, 0);
    addF (l, "noise_level", "Noise Level", 0.f, 1.f, 0.f);
    addF (l, "noise_pitch", "Noise Pitch", 0.f, 1.f, 1.f);

    for (int f = 0; f < 2; ++f)
    {
        const juce::String p = "flt" + juce::String (f + 1) + "_";
        const juce::String n = "Filter " + juce::String (f + 1) + " ";
        addC (l, p + "type", n + "Type", Lists::filterTypes, f == 0 ? 3 : 0);
        addF (l, p + "cutoff", n + "Cutoff", 20.f, 20000.f, f == 0 ? 2500.f : 800.f, 1000.f);
        addF (l, p + "reso", n + "Resonance", 0.f, 1.f, 0.2f);
        addF (l, p + "drive", n + "Drive", 0.f, 1.f, 0.f);
        addF (l, p + "key", n + "Key Track", 0.f, 1.f, 0.3f);
        addF (l, p + "env", n + "Env Amount", -1.f, 1.f, f == 0 ? 0.3f : 0.f);
    }
    addC (l, "flt_routing", "Filter Routing", Lists::routings, 0);

    const float envDef[4][4] = { { 0.003f, 0.6f, 0.8f, 0.25f }, { 0.001f, 0.4f, 0.2f, 0.3f },
                                 { 0.01f, 0.5f, 0.f, 0.3f }, { 0.5f, 1.f, 1.f, 0.5f } };
    for (int e = 0; e < kNumEnvs; ++e)
    {
        const juce::String p = "env" + juce::String (e + 1) + "_";
        const juce::String n = "Env " + juce::String (e + 1) + " ";
        addF (l, p + "a", n + "Attack", 0.001f, 8.f, envDef[e][0], 0.3f);
        addF (l, p + "d", n + "Decay", 0.005f, 8.f, envDef[e][1], 0.5f);
        addF (l, p + "s", n + "Sustain", 0.f, 1.f, envDef[e][2]);
        addF (l, p + "r", n + "Release", 0.005f, 10.f, envDef[e][3], 0.6f);
    }

    for (int i = 0; i < kNumLfos; ++i)
    {
        const juce::String p = "lfo" + juce::String (i + 1) + "_";
        const juce::String n = "LFO " + juce::String (i + 1) + " ";
        addC (l, p + "shape", n + "Shape", Lists::lfoShapes, 0);
        addF (l, p + "rate", n + "Rate", 0.02f, 500.f, 2.f, 2.f);   // up to audio rate
        addB (l, p + "sync", n + "Sync", false);
        addC (l, p + "div", n + "Division", Lists::divisions, 8);
        addC (l, p + "trig", n + "Trigger", Lists::lfoTrig, 0);
        addF (l, p + "phase", n + "Phase", 0.f, 1.f, 0.f);
        addF (l, p + "fade", n + "Fade In", 0.f, 5.f, 0.f, 0.5f);
    }

    for (int m = 0; m < kNumModSlots; ++m)
    {
        const juce::String p = "mod" + juce::String (m + 1) + "_";
        const juce::String n = "Mod " + juce::String (m + 1) + " ";
        addC (l, p + "src", n + "Source", Lists::modSources, 0);
        addC (l, p + "dst", n + "Destination", Lists::modDests, 0);
        addF (l, p + "amt", n + "Amount", -1.f, 1.f, 0.f);
    }

    addC (l, "mode", "Voice Mode", Lists::voiceModes, 0);
    addF (l, "glide", "Glide", 0.f, 2.f, 0.f, 0.2f);
    addI (l, "bend", "Pitch Bend Range", 0, 24, 2);
    addF (l, "vel_sens", "Velocity", 0.f, 1.f, 0.5f);
    addF (l, "warmth", "Analog Warmth", 0.f, 1.f, 0.3f);
    addC (l, "oversample", "Oversampling", juce::StringArray { "1x", "2x", "4x", "8x" }, 1);
    addF (l, "accent", "Accent", 0.f, 1.f, 0.5f);

    addB (l, "seq_on", "Sequencer On", false);
    addI (l, "seq_pattern", "Seq Pattern", 1, kNumPatterns, 1);
    addI (l, "seq_transpose", "Seq Transpose", -24, 24, 0);
    addF (l, "seq_gate", "Seq Gate", 0.1f, 1.f, 0.5f);
    l.add (std::make_unique<APF> (pid ("bpm"), "Tempo", juce::NormalisableRange<float> (60.f, 200.f, 0.1f), 132.f));
    addB (l, "run", "Run", false);
    addF (l, "shuffle", "Shuffle", 0.f, 1.f, 0.f);

    addB (l, "dist_on", "Dist On", false);
    addC (l, "dist_type", "Dist Type", Lists::distTypes, 0);
    addF (l, "dist_drive", "Dist Drive", 0.f, 1.f, 0.3f);
    addF (l, "dist_mix", "Dist Mix", 0.f, 1.f, 1.f);

    addB (l, "mfx_on", "Mod FX On", false);
    addC (l, "mfx_type", "Mod FX Type", Lists::modFxTypes, 0);
    addF (l, "mfx_rate", "Mod FX Rate", 0.02f, 8.f, 0.6f, 0.8f);
    addF (l, "mfx_depth", "Mod FX Depth", 0.f, 1.f, 0.5f);
    addF (l, "mfx_fb", "Mod FX Feedback", 0.f, 1.f, 0.2f);
    addF (l, "mfx_mix", "Mod FX Mix", 0.f, 1.f, 0.5f);

    addB (l, "dly_on", "Delay On", false);
    addB (l, "dly_sync", "Delay Sync", true);
    addC (l, "dly_div", "Delay Division", Lists::divisions, 6);
    addF (l, "dly_ms", "Delay Time", 1.f, 2000.f, 350.f, 300.f);
    addB (l, "dly_pp", "Delay Ping-Pong", true);
    addF (l, "dly_fb", "Delay Feedback", 0.f, 0.95f, 0.4f);
    addF (l, "dly_mix", "Delay Mix", 0.f, 1.f, 0.3f);

    addB (l, "rev_on", "Reverb On", false);
    addF (l, "rev_size", "Reverb Size", 0.f, 1.f, 0.6f);
    addF (l, "rev_decay", "Reverb Decay", 0.2f, 20.f, 2.5f, 2.5f);
    addF (l, "rev_damp", "Reverb High Cut", 1000.f, 20000.f, 7000.f, 5000.f);
    addF (l, "rev_mix", "Reverb Mix", 0.f, 1.f, 0.3f);

    addB (l, "comp_on", "Comp On", false);
    addC (l, "comp_mode", "Comp Mode", Lists::compModes, 0);
    addF (l, "comp_thr", "Comp Threshold", -60.f, 0.f, -18.f);
    addF (l, "comp_ratio", "Comp Ratio", 1.f, 20.f, 4.f, 4.f);
    addF (l, "comp_att", "Comp Attack", 0.1f, 100.f, 5.f, 10.f);
    addF (l, "comp_rel", "Comp Release", 10.f, 1000.f, 120.f, 150.f);
    addF (l, "comp_makeup", "Comp Makeup", 0.f, 24.f, 0.f);
    addF (l, "comp_depth", "OTT Depth", 0.f, 1.f, 0.5f);

    addB (l, "eq_on", "EQ On", false);
    addF (l, "eq_lf", "EQ Low Freq", 20.f, 1000.f, 120.f, 150.f);
    addF (l, "eq_lg", "EQ Low Gain", -18.f, 18.f, 0.f);
    addF (l, "eq_mf", "EQ Mid Freq", 100.f, 10000.f, 1000.f, 1000.f);
    addF (l, "eq_mq", "EQ Mid Q", 0.3f, 8.f, 1.f, 1.5f);
    addF (l, "eq_mg", "EQ Mid Gain", -18.f, 18.f, 0.f);
    addF (l, "eq_hf", "EQ High Freq", 1000.f, 20000.f, 8000.f, 6000.f);
    addF (l, "eq_hg", "EQ High Gain", -18.f, 18.f, 0.f);

    addF (l, "volume", "Master Volume", 0.f, 2.f, 0.8f);
    addF (l, "pan", "Master Pan", -1.f, 1.f, 0.f);

    // Added in 1.3: always appended, so existing automation and projects keep working
    addF (l, "dist_tone", "Dist Tone", 0.f, 1.f, 1.f);
    addB (l, "fxf_on", "FX Filter On", false);
    addC (l, "fxf_type", "FX Filter Type", Lists::fxFilterTypes, 0);
    addF (l, "fxf_cut", "FX Filter Cutoff", 20.f, 20000.f, 2000.f, 1000.f);
    addF (l, "fxf_res", "FX Filter Resonance", 0.f, 1.f, 0.2f);
    addF (l, "fxf_drive", "FX Filter Drive", 0.f, 1.f, 0.f);
    addF (l, "fxf_mix", "FX Filter Mix", 0.f, 1.f, 1.f);
    addB (l, "wid_on", "Width On", false);
    addF (l, "wid_amt", "Stereo Width", 0.f, 2.f, 1.3f);
    addF (l, "wid_mono", "Bass Mono Below", 20.f, 500.f, 150.f, 120.f);
    addB (l, "arp_on", "Arp On", false);
    addC (l, "arp_mode", "Arp Mode", Lists::arpModes, 0);
    addC (l, "arp_div", "Arp Rate", Lists::divisions, 2);
    addI (l, "arp_oct", "Arp Octaves", 1, 4, 1);
    addF (l, "arp_gate", "Arp Gate", 0.05f, 1.f, 0.6f);
    addF (l, "arp_swing", "Arp Swing", 0.f, 1.f, 0.f);
    addF (l, "drift", "Analog Drift", 0.f, 1.f, 0.4f);
    addF (l, "spread", "Voice Spread", 0.f, 1.f, 0.35f);
    for (int o = 0; o < 2; ++o)
    {
        const juce::String p = "osc" + juce::String (o + 1) + "_";
        const juce::String n = "Osc " + juce::String (o + 1) + " ";
        addC (l, p + "shape", n + "Shape", Lists::oscShapes, 0);
        addF (l, p + "shapeamt", n + "Shape Amount", 0.f, 1.f, 0.5f);
        addB (l, p + "wtstep", n + "WT Step", false);
        addC (l, p + "unimode", n + "Unison Mode", Lists::uniModes, 0);
    }
    for (int m = 0; m < kNumMacros; ++m)
        addF (l, "macro" + juce::String (m + 1), "Macro " + juce::String (m + 1), 0.f, 1.f, 0.f);
    addB (l, "morph_on", "Morph On", false);
    addF (l, "morph", "A/B Morph", 0.f, 1.f, 0.f);
    addB (l, "duck_on", "Kick Duck On", false);
    addC (l, "duck_div", "Kick Duck Rate", Lists::divisions, 8);
    addF (l, "duck_depth", "Kick Duck Depth", 0.f, 1.f, 0.7f);
    addF (l, "duck_rel", "Kick Duck Release", 0.05f, 1.f, 0.45f);
    addF (l, "duck_curve", "Kick Duck Curve", 0.f, 1.f, 0.6f);
    return l;
}

AcidanceProcessor::AcidanceProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "ACIDANCE", createLayout())
{
    // These never morph: transport, structure and the morph / macro controls themselves
    static const juce::StringArray noMorph { "bpm", "run", "seq_on", "seq_pattern", "arp_on", "mode", "oversample",
                                             "morph", "morph_on", "macro1", "macro2", "macro3", "macro4" };
    const int numParams = getParameters().size();
    shadow = std::make_unique<std::atomic<float>[]> ((size_t) numParams);
    morphA = std::make_unique<std::atomic<float>[]> ((size_t) numParams);
    morphB = std::make_unique<std::atomic<float>[]> ((size_t) numParams);
    shadowMap.reserve ((size_t) numParams);
    auto get = [this] (const juce::String& id)
    {
        auto* raw = apvts.getRawParameterValue (id);
        auto* param = apvts.getParameter (id);
        jassert (raw != nullptr && param != nullptr);
        const bool stepped = dynamic_cast<juce::AudioParameterFloat*> (param) == nullptr;
        const size_t i = shadowMap.size();
        shadowMap.push_back ({ raw, param, ! noMorph.contains (id), stepped });
        shadow[i] = raw->load();
        return &shadow[i];
    };

    for (int o = 0; o < 2; ++o)
    {
        const juce::String p = "osc" + juce::String (o + 1) + "_";
        oscP[o] = { get (p + "on"), get (p + "wave"), get (p + "wtpos"), get (p + "oct"), get (p + "semi"), get (p + "fine"),
                    get (p + "level"), get (p + "uni"), get (p + "detune"), get (p + "blend"), get (p + "phase"),
                    get (p + "rphase"), get (p + "warp"), get (p + "warpamt"), get (p + "lock"),
                    get (p + "shape"), get (p + "shapeamt"), get (p + "wtstep"), get (p + "unimode") };
    }
    for (int f = 0; f < 2; ++f)
    {
        const juce::String p = "flt" + juce::String (f + 1) + "_";
        fltP[f] = { get (p + "type"), get (p + "cutoff"), get (p + "reso"), get (p + "drive"), get (p + "key"), get (p + "env") };
    }
    for (int e = 0; e < kNumEnvs; ++e)
    {
        const juce::String p = "env" + juce::String (e + 1) + "_";
        envP[e] = { get (p + "a"), get (p + "d"), get (p + "s"), get (p + "r") };
    }
    for (int i = 0; i < kNumLfos; ++i)
    {
        const juce::String p = "lfo" + juce::String (i + 1) + "_";
        lfoP[i] = { get (p + "shape"), get (p + "rate"), get (p + "sync"), get (p + "div"), get (p + "trig"), get (p + "phase"), get (p + "fade") };
        for (int s = 0; s < kLfoSteps; ++s) lfoCustom[i][s] = (s % 4 == 0) ? 1.f : -0.5f;
    }
    for (int m = 0; m < kNumModSlots; ++m)
    {
        const juce::String p = "mod" + juce::String (m + 1) + "_";
        modP[m] = { get (p + "src"), get (p + "dst"), get (p + "amt") };
    }
    pSubWave = get ("sub_wave"); pSubOct = get ("sub_oct"); pSubLevel = get ("sub_level");
    pNoiseType = get ("noise_type"); pNoiseLevel = get ("noise_level"); pNoisePitch = get ("noise_pitch");
    pRouting = get ("flt_routing");
    pMode = get ("mode"); pGlide = get ("glide"); pBend = get ("bend"); pVelSens = get ("vel_sens"); pAccent = get ("accent");
    pSeqOn = get ("seq_on"); pSeqPattern = get ("seq_pattern"); pSeqTranspose = get ("seq_transpose");
    pSeqGate = get ("seq_gate"); pBpm = get ("bpm"); pRun = get ("run"); pShuffle = get ("shuffle");
    pDistOn = get ("dist_on"); pDistType = get ("dist_type"); pDistDrive = get ("dist_drive"); pDistMix = get ("dist_mix");
    pDistTone = get ("dist_tone");
    pFxfOn = get ("fxf_on"); pFxfType = get ("fxf_type"); pFxfCut = get ("fxf_cut"); pFxfRes = get ("fxf_res");
    pFxfDrive = get ("fxf_drive"); pFxfMix = get ("fxf_mix");
    pWidOn = get ("wid_on"); pWidAmt = get ("wid_amt"); pWidMono = get ("wid_mono");
    pArpOn = get ("arp_on"); pArpMode = get ("arp_mode"); pArpDiv = get ("arp_div"); pArpOct = get ("arp_oct");
    pArpGate = get ("arp_gate"); pArpSwing = get ("arp_swing");
    pMfxOn = get ("mfx_on"); pMfxType = get ("mfx_type"); pMfxRate = get ("mfx_rate"); pMfxDepth = get ("mfx_depth");
    pMfxFb = get ("mfx_fb"); pMfxMix = get ("mfx_mix");
    pDlyOn = get ("dly_on"); pDlySync = get ("dly_sync"); pDlyDiv = get ("dly_div"); pDlyMs = get ("dly_ms");
    pDlyPP = get ("dly_pp"); pDlyFb = get ("dly_fb"); pDlyMix = get ("dly_mix");
    pRevOn = get ("rev_on"); pRevSize = get ("rev_size"); pRevDecay = get ("rev_decay"); pRevDamp = get ("rev_damp"); pRevMix = get ("rev_mix");
    pCompOn = get ("comp_on"); pCompMode = get ("comp_mode"); pCompThr = get ("comp_thr"); pCompRatio = get ("comp_ratio");
    pCompAtt = get ("comp_att"); pCompRel = get ("comp_rel"); pCompMakeup = get ("comp_makeup"); pCompDepth = get ("comp_depth");
    pEqOn = get ("eq_on"); pEqLf = get ("eq_lf"); pEqLg = get ("eq_lg"); pEqMf = get ("eq_mf"); pEqMq = get ("eq_mq");
    pEqMg = get ("eq_mg"); pEqHf = get ("eq_hf"); pEqHg = get ("eq_hg");
    pVolume = get ("volume"); pPan = get ("pan");
    pOversample = get ("oversample"); pWarmth = get ("warmth"); pDrift = get ("drift"); pSpread = get ("spread");
    for (int m = 0; m < kNumMacros; ++m) pMacro[m] = get ("macro" + juce::String (m + 1));
    pMorphOn = get ("morph_on"); pMorph = get ("morph");
    morphOnRaw = apvts.getRawParameterValue ("morph_on"); morphRaw = apvts.getRawParameterValue ("morph");
    pDuckOn = get ("duck_on"); pDuckDiv = get ("duck_div"); pDuckDepth = get ("duck_depth");
    pDuckRel = get ("duck_rel"); pDuckCurve = get ("duck_curve");

    for (int v = 0; v <= kNumVoices; ++v) voices[v].seed ((uint32_t) (v + 1) * 7919u);
    for (int p = 0; p < kNumPatterns; ++p) seq.randomise (p, (uint32_t) (1234 + p * 77));

    Wavetables::get();   // build tables up front, not on the audio thread
    setCurrentProgram (0);
}

int AcidanceProcessor::getNumPrograms() { return Presets::count(); }

const juce::String AcidanceProcessor::getProgramName (int index)
{
    return "[" + Presets::category (index) + "] " + Presets::name (index);
}

void AcidanceProcessor::setParam (const juce::String& id, float value)
{
    if (auto* p = apvts.getParameter (id))
        p->setValueNotifyingHost (p->convertTo0to1 (value));
    else
        jassertfalse;
}

void AcidanceProcessor::resetAllParams()
{
    static const juce::StringArray keep { "bpm", "run" };
    for (auto* p : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
            if (! keep.contains (rp->getParameterID()))
                rp->setValueNotifyingHost (rp->getDefaultValue());
}

void AcidanceProcessor::setCurrentProgram (int index)
{
    index = juce::jlimit (0, Presets::count() - 1, index);
    // Hosts may switch programs from the audio thread (VST3 program-change parameter). Loading a
    // preset sets hundreds of parameters, so that work moves to the message thread and this returns
    // at once; the audio keeps running.
    if (auto* mm = juce::MessageManager::getInstanceWithoutCreating(); mm != nullptr && ! mm->isThisTheMessageThread())
    {
        currentProgram = index;
        pendingProgram = index;
        triggerAsyncUpdate();
        return;
    }
    currentProgram = index;
    currentPresetName = Presets::name (index);
    currentPresetFolder = Presets::category (index);
    fader.change ([this, index] // fade out, switch, fade in: no clicks between presets
    {
        Presets::apply (index, *this);
        ++programChanges;
        updateHostDisplay (ChangeDetails().withProgramChanged (true));
    });
}

namespace
{
    // Plays a sound offline in a scratch instance (no host) and returns its RMS and peak
    // at volume `vol`. RANDOM uses it so a new sound is never silent and never slams the limiter.
    struct Probe { double rms = 0.0; float peak = 0.f; bool ok = true; };

    Probe probeSound (const juce::XmlElement& state, double sampleRate, float vol, double seconds)
    {
        AcidanceProcessor q;
        q.applyStateXml (state);
        q.setParam ("volume", vol);
        // at most 2x for the test: close enough for loudness, and a press stays quick on the UI thread
        if (q.apvts.getRawParameterValue ("oversample")->load() > 1.5f) q.setParam ("oversample", 1.f);
        const bool seq = q.apvts.getRawParameterValue ("seq_on")->load() > 0.5f;
        const bool arp = q.apvts.getRawParameterValue ("arp_on")->load() > 0.5f;
        const bool poly = q.apvts.getRawParameterValue ("mode")->load() < 0.5f;
        q.setParam ("run", seq ? 1.f : 0.f);   // the pattern needs a clock to play
        constexpr int block = 512;
        q.setPlayConfigDetails (0, 2, sampleRate, block);
        q.prepareToPlay (sampleRate, block);

        const int total = (int) (sampleRate * seconds), offAt = (int) (sampleRate * seconds * 0.78);
        juce::AudioBuffer<float> buf (2, block);
        Probe r;
        double sum = 0.0;
        for (int pos = 0; pos < total; pos += block)
        {
            juce::MidiBuffer midi;
            const int chord[] = { 57, 60, 64 };
            const int* notes = seq ? chord + 1 : (poly || arp) ? chord : nullptr;
            const int numNotes = seq ? 1 : (poly || arp) ? 3 : 1;
            for (int k = 0; k < numNotes; ++k)
            {
                const int n = notes != nullptr ? notes[k] : 48;
                if (pos == 0) midi.addEvent (juce::MidiMessage::noteOn (1, n, (juce::uint8) 100), 0);
                if (offAt >= pos && offAt < pos + block) midi.addEvent (juce::MidiMessage::noteOff (1, n), offAt - pos);
            }
            q.processBlock (buf, midi);
            for (int c = 0; c < 2; ++c)
                for (int i = 0; i < block; ++i)
                {
                    const float v = buf.getSample (c, i);
                    if (! std::isfinite (v)) r.ok = false;
                    r.peak = std::max (r.peak, std::abs (v));
                    sum += (double) v * v;
                }
        }
        r.rms = std::sqrt (sum / (2.0 * (total / block) * block));
        return r;
    }
}

void AcidanceProcessor::handleAsyncUpdate()
{
    const int index = pendingProgram.exchange (-1);
    if (index >= 0) setCurrentProgram (index);
}

void AcidanceProcessor::randomizeSound()
{
    fader.change ([this]
    {
        // Roll a new sound, test-play it, and keep it only if it is clearly audible; then set the
        // master volume so every RANDOM press comes out about as loud as the factory presets.
        constexpr float probeVol = 0.1f;   // low enough that the output limiter never touches the test
        constexpr double target = 0.12, silent = 0.004;
        const double rate = sr > 0.0 ? sr : 48000.0;
        juce::String style;
        Probe pr;
        // A time budget keeps the UI responsive: if no audible roll turns up quickly, a random
        // factory preset (all of them are measured audible) is used instead.
        const auto t0 = juce::Time::getMillisecondCounterHiRes();
        bool found = false;
        for (int attempt = 0; attempt < 8 && ! found; ++attempt)
        {
            style = Presets::randomSound (*this);
            pr = probeSound (*createStateXml(), rate, probeVol, attempt == 0 ? 1.8 : 1.2);
            found = pr.ok && pr.rms > silent;
            if (! found && juce::Time::getMillisecondCounterHiRes() - t0 > 350.0) break;
        }
        if (! found)
        {
            const int index = juce::Random::getSystemRandom().nextInt (Presets::count());
            Presets::apply (index, *this);
            style = Presets::category (index);
            pr = probeSound (*createStateXml(), rate, probeVol, 1.2);
        }
        float vol = (float) (probeVol * target / std::max (pr.rms, silent));
        if (pr.peak > 0.f) vol = std::min (vol, probeVol * 0.9f / pr.peak);   // peaks stay under the limiter, with margin
        setParam ("volume", juce::jlimit (0.05f, 1.6f, vol));
        currentPresetName = "Random " + style + " #" + juce::String (juce::Random::getSystemRandom().nextInt ({ 1000, 10000 }));
        currentPresetFolder = "Random";
        ++programChanges;
    });
}

bool AcidanceProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void AcidanceProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    // Voices allocate for the highest oversampled rate once, then just switch rates
    for (auto& v : voices) v.prepare (sampleRate * 8.0);
    activeOversample = -1;
    oversampleBlock = std::max (samplesPerBlock, 512);
    for (int k = 0; k < 3; ++k)
    {
        oversamplers[k] = std::make_unique<juce::dsp::Oversampling<float>> (
            2, (size_t) (k + 1), juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, false);
        oversamplers[k]->initProcessing ((size_t) oversampleBlock);
    }
    // The IIR half-band filters delay the sound by a few samples. Their nominal figure doesn't match
    // what you hear, so measure each one: an oversampled saw goes down, cross-correlation finds the delay.
    // Then every factor is padded to the largest, which is what the host is told.
    latencyTotal = 0;
    int lat[4] { 0, 0, 0, 0 };
    {
        const int n = std::min (oversampleBlock, 2048);
        juce::AudioBuffer<float> test (2, n);
        std::vector<float> src ((size_t) n);
        float ph = 0.f;
        for (int i = 0; i < n; ++i)
        {
            src[(size_t) i] = 2.f * ph - 1.f;
            ph += 130.81f / (float) sampleRate;
            if (ph >= 1.f) ph -= 1.f;
        }
        for (int k = 0; k < 3; ++k)
        {
            // the voices write straight into the oversampled buffer, so only the way down delays them
            const int f = 2 << k;
            test.clear();
            juce::dsp::AudioBlock<float> blk (test);
            auto up = oversamplers[k]->processSamplesUp (blk);
            float uph = 0.f;
            for (int i = 0; i < n * f; ++i)
            {
                for (int c = 0; c < 2; ++c) up.setSample (c, i, 2.f * uph - 1.f);
                uph += 130.81f / (float) (sampleRate * f);
                if (uph >= 1.f) uph -= 1.f;
            }
            oversamplers[k]->processSamplesDown (blk);
            oversamplers[k]->reset();
            const float* y = test.getReadPointer (0);
            double bestDot = -1.0e30;
            for (int d = 0; d <= 16; ++d)
            {
                double dot = 0.0;
                for (int i = n / 4; i < n - 16; ++i) dot += (double) src[(size_t) i] * y[i + d];
                if (dot > bestDot) { bestDot = dot; lat[k + 1] = d; }
            }
            latencyTotal = std::max (latencyTotal, lat[k + 1]);
        }
    }
    for (int k = 0; k < 4; ++k) latencyPad[k] = latencyTotal - lat[k];
    for (auto& b : padBuf) b.assign (64, 0.f);
    padPos = 0;
    setLatencySamples (latencyTotal);
    numHeld = 0;
    modFx.prepare (sampleRate);
    fxFilter.prepare (sampleRate);
    width.prepare (sampleRate);
    delay.prepare (sampleRate);
    reverb.prepare (sampleRate);
    comp.prepare (sampleRate, samplesPerBlock);
    eq.prepare (sampleRate);
    clockStep = std::numeric_limits<int64_t>::min();
    wasRunning = false;
    arpStop();
    arpHeldCount = 0;
}

int AcidanceProcessor::findVoice()
{
    for (int v = 0; v < kNumVoices; ++v)
        if (! voices[v].isActive()) return v;
    int best = 0;
    uint32_t oldest = 0;
    for (int v = 0; v < kNumVoices; ++v)
        if (voices[v].isReleased() && voices[v].getAge() >= oldest) { oldest = voices[v].getAge(); best = v; }
    if (oldest > 0) return best;
    for (int v = 0; v < kNumVoices; ++v)
        if (voices[v].getAge() >= oldest) { oldest = voices[v].getAge(); best = v; }
    return best;
}

void AcidanceProcessor::resetLfoPhasesIfIdle()
{
    bool anyHeld = false;
    for (auto& v : voices) anyHeld |= (v.isActive() && ! v.isReleased());
    if (! anyHeld)
        for (int i = 0; i < kNumLfos; ++i)
            if (lfoP[i].sync->load() < 0.5f) globalLfo[i].phase = 0.f;
}

void AcidanceProcessor::noteOn (int note, float vel, bool accent)
{
    resetLfoPhasesIfIdle();
    lastVelocity = vel;
    lastKey = (note - 60.f) / 24.f;

    const int mode = (int) pMode->load();
    const bool glideOn = pGlide->load() > 0.0005f;

    if (mode == 0)
    {
        lastVoice = findVoice();
        voices[lastVoice].start (note, vel, accent, true, glideOn, vp);
        return;
    }

    const bool legato = numHeld > 0;
    if (numHeld < 16) heldNotes[numHeld++] = note;
    lastVoice = 0;
    if (mode == 1) voices[0].start (note, vel, accent, true, glideOn, vp);
    else           voices[0].start (note, vel, accent, ! legato, legato, vp);
}

void AcidanceProcessor::noteOff (int note)
{
    if ((int) pMode->load() == 0)
    {
        for (int v = 0; v < kNumVoices; ++v)
            if (voices[v].getNote() == note && ! voices[v].isReleased()) voices[v].release();
        return;
    }

    int idx = -1;
    for (int i = 0; i < numHeld; ++i) if (heldNotes[i] == note) idx = i;
    if (idx < 0)
    {
        // a note that never made the held list (more than 16 keys down): still let go of it
        if (numHeld == 0 && voices[0].getNote() == note && ! voices[0].isReleased()) voices[0].release();
        return;
    }
    const bool wasTop = idx == numHeld - 1;
    for (int i = idx; i < numHeld - 1; ++i) heldNotes[i] = heldNotes[i + 1];
    --numHeld;

    if (numHeld == 0) voices[0].release();
    else if (wasTop) voices[0].start (heldNotes[numHeld - 1], lastVelocity, false, false, true, vp);
}

void AcidanceProcessor::allNotesOff()
{
    for (auto& v : voices) v.release();
    numHeld = 0;
    lastSlide = false;
    arpHeldCount = 0;
    arpSoundingCount = 0;
    arpDirty = true;
}

void AcidanceProcessor::resetControllers()
{
    gm.modWheel = 0.f;
    gm.aftertouch = 0.f;
    gm.bend = 0.f;
    uiBend.store (0.f);
    uiMod.store (0.f);
}

// Kill switch: silences every voice instantly and flushes all DSP state
void AcidanceProcessor::panic()
{
    for (auto& v : voices) v.hardReset();
    numHeld = 0;
    arpHeldCount = 0;
    arpSoundingCount = 0;
    arpDirty = true;
    lastSlide = false;
    seqKeyOffset = 0;
    seqKeys.fill (false);
    seqKeyCount = 0;
    resetControllers();
    dist.reset();
    modFx.reset();
    delay.reset();
    reverb.reset();
    comp.reset();
    eq.reset();
    fxFilter.reset();
    width.reset();
    for (auto& b : padBuf) std::fill (b.begin(), b.end(), 0.f);
}

bool AcidanceProcessor::seqGateOpen() const
{
    // The pattern never plays by itself: only while MIDI notes are held (a DAW clip or a keyboard).
    // RUN just provides the clock when no song is playing.
    return seqKeyCount > 0;
}

void AcidanceProcessor::seqStepStart (int step)
{
    currentStep.store (step);
    if (pSeqOn->load() < 0.5f) return;
    if (! seqGateOpen())
    {
        auto& sv = voices[kNumVoices];
        if (! sv.isReleased()) sv.release();
        lastSlide = false;
        return;
    }

    auto& sv = voices[kNumVoices];
    const int pat = juce::jlimit (0, kNumPatterns - 1, (int) pSeqPattern->load() - 1);
    auto& s = seq.steps[pat][step];
    if (s.gate.load())
    {
        const int note = s.midiNote() + (int) pSeqTranspose->load() + seqKeyOffset;
        const bool slide = lastSlide && sv.isActive() && ! sv.isReleased();
        if (! slide) resetLfoPhasesIfIdle();
        sv.start (note, 1.f, s.accent.load() != 0, ! slide, slide, vp);
        sv.setLocks (s.lock[0].load() * 0.01f, s.lock[1].load() * 0.01f, s.lock[2].load() * 0.01f, s.lock[3].load() * 0.01f);
        lastVoice = kNumVoices;
        lastSlide = s.slide.load() != 0;
    }
    else
    {
        if (! sv.isReleased()) sv.release();
        lastSlide = false;
    }
}

// A key that lands a little after a step boundary (a DAW note a few samples late, or a key played
// by hand) plays the current step at once instead of waiting a whole 16th for the next one.
bool AcidanceProcessor::seqCanCatchUp() const
{
    return clockPlaying && stepStarted && ! stepEndDone && stepFrac - stepOffset < 0.5;
}

void AcidanceProcessor::seqStepEnd()
{
    auto& sv = voices[kNumVoices];
    if (! lastSlide && ! sv.isReleased()) sv.release();
}

void AcidanceProcessor::handleMidi (const juce::MidiMessage& m, bool seqArmed)
{
    if (m.isPitchWheel())
    {
        const float norm = juce::jlimit (-1.f, 1.f, (float) (m.getPitchWheelValue() - 8192) / 8192.f);
        gm.bend = norm * pBend->load();
        uiBend.store (norm);
        return;
    }
    if (m.isController())
    {
        const int cc = m.getControllerNumber();
        if (cc == 120) { panic(); return; }
        if (cc == 121) { resetControllers(); return; }
        if (cc == 123) { allNotesOff(); return; }
    }
    if (m.isController() && m.getControllerNumber() == 1)
    {
        gm.modWheel = m.getControllerValue() / 127.f;
        uiMod.store (gm.modWheel);
        return;
    }
    if (m.isChannelPressure()) { gm.aftertouch = m.getChannelPressureValue() / 127.f; return; }
    if (m.isAftertouch()) { gm.aftertouch = m.getAfterTouchValue() / 127.f; return; }

    if (seqArmed)
    {
        // While the sequencer is on, holding keys plays the pattern and transposes it (C3 = no transpose);
        // letting go of the last key stops it
        if (m.isNoteOn())
        {
            seqKeyOffset = m.getNoteNumber() - 60;
            const bool opens = seqKeyCount == 0;
            if (! seqKeys[(size_t) m.getNoteNumber()]) { seqKeys[(size_t) m.getNoteNumber()] = true; ++seqKeyCount; }
            if (opens && seqCanCatchUp()) seqStepStart ((int) (clockStep % kNumSteps));
        }
        else if (m.isNoteOff() && seqKeys[(size_t) m.getNoteNumber()])
        {
            seqKeys[(size_t) m.getNoteNumber()] = false;
            if (--seqKeyCount == 0 && ! seqGateOpen())
            {
                auto& sv = voices[kNumVoices];
                if (! sv.isReleased()) sv.release();
                lastSlide = false;
            }
        }
        else if (m.isAllNotesOff() || m.isAllSoundOff())
        {
            seqKeys.fill (false);
            seqKeyCount = 0;
            allNotesOff();
        }
        return;
    }

    if (arpWasActive)
    {
        if (m.isNoteOn()) arpKeyDown (m.getNoteNumber(), m.getFloatVelocity(), clockPlaying);
        else if (m.isNoteOff()) arpKeyUp (m.getNoteNumber());
        else if (m.isAllNotesOff() || m.isAllSoundOff()) { arpStop(); arpHeldCount = 0; allNotesOff(); }
        return;
    }

    if (m.isNoteOn()) noteOn (m.getNoteNumber(), m.getFloatVelocity(), m.getVelocity() >= 100);
    else if (m.isNoteOff()) noteOff (m.getNoteNumber());
    else if (m.isAllNotesOff() || m.isAllSoundOff()) allNotesOff();
}

//==============================================================================
void AcidanceProcessor::arpKeyDown (int note, float vel, bool playing)
{
    for (int i = 0; i < arpHeldCount; ++i)
        if (arpHeld[i] == note) return;
    if (arpHeldCount >= 32) return;
    const bool first = arpHeldCount == 0;
    arpHeld[arpHeldCount] = note;
    arpOrder[arpHeldCount] = note;
    ++arpHeldCount;
    arpVel = vel;
    arpDirty = true;
    if (! first) return;

    arpIndex = 0;
    if (! playing)
    {
        // no song running: a free clock that starts right on the key
        arpFreePos = 0.0;
        arpStep = 0;
        arpFrac = arpOffset = 0.0;
        arpStarted = arpGateDone = false;   // fires on this very sample
    }
    else if (arpStarted && ! arpGateDone && arpFrac - arpOffset < 0.5)
        arpFire();   // just after a step boundary: play this step now, not one step late
}

void AcidanceProcessor::arpKeyUp (int note)
{
    int idx = -1;
    for (int i = 0; i < arpHeldCount; ++i) if (arpHeld[i] == note) idx = i;
    if (idx >= 0)
    {
        for (int i = idx; i < arpHeldCount - 1; ++i) arpHeld[i] = arpHeld[i + 1];
        --arpHeldCount;
        int o = -1;
        for (int i = 0; i <= arpHeldCount; ++i) if (arpOrder[i] == note) o = i;
        if (o >= 0) for (int i = o; i < arpHeldCount; ++i) arpOrder[i] = arpOrder[i + 1];
        arpDirty = true;
    }
    if (arpHeldCount == 0) arpReleaseSounding();   // last key up: the arp stops at once
}

void AcidanceProcessor::arpRebuild()
{
    arpDirty = false;
    arpSeqLen = 0;
    if (arpHeldCount == 0) return;
    int asc[32];
    for (int i = 0; i < arpHeldCount; ++i) asc[i] = arpHeld[i];
    std::sort (asc, asc + arpHeldCount);
    const int mode = (int) pArpMode->load();
    const int octs = juce::jlimit (1, 4, (int) pArpOct->load());
    const int* src = mode == ArpPlayed ? arpOrder : asc;
    int up[128];
    int n = 0;
    for (int o = 0; o < octs; ++o)
        for (int i = 0; i < arpHeldCount; ++i)
            up[n++] = std::min (127, src[i] + 12 * o);
    auto push = [this] (int v) { if (arpSeqLen < 256) arpSeq[arpSeqLen++] = v; };
    switch (mode)
    {
        case ArpDown: for (int i = n - 1; i >= 0; --i) push (up[i]); break;
        case ArpUpDown:
            for (int i = 0; i < n; ++i) push (up[i]);
            for (int i = n - 2; i >= 1; --i) push (up[i]);
            break;
        case ArpDownUp:
            for (int i = n - 1; i >= 0; --i) push (up[i]);
            for (int i = 1; i <= n - 2; ++i) push (up[i]);
            break;
        case ArpConverge:
            for (int lo = 0, hi = n - 1; lo <= hi; ++lo, --hi)
            {
                push (up[lo]);
                if (hi != lo) push (up[hi]);
            }
            break;
        default: for (int i = 0; i < n; ++i) push (up[i]); break;   // Up, Random, Played, Chord
    }
}

void AcidanceProcessor::arpReleaseSounding()
{
    for (int i = 0; i < arpSoundingCount; ++i) noteOff (arpSounding[i]);
    arpSoundingCount = 0;
}

void AcidanceProcessor::arpStop()
{
    arpReleaseSounding();
    arpStep = std::numeric_limits<int64_t>::min();
    arpStarted = arpGateDone = false;
}

void AcidanceProcessor::arpFire()
{
    arpReleaseSounding();
    if (arpHeldCount == 0) return;
    if (arpDirty) arpRebuild();
    if (arpSeqLen == 0) return;
    const int mode = (int) pArpMode->load();
    auto play = [this] (int note)
    {
        noteOn (note, arpVel, false);
        if (arpSoundingCount < 32) arpSounding[arpSoundingCount++] = note;
    };
    if (mode == ArpChord)
    {
        // the whole chord on every step, climbing one octave per step through the octave range
        const int octs = juce::jlimit (1, 4, (int) pArpOct->load());
        const int o = arpIndex % octs;
        for (int i = 0; i < arpHeldCount; ++i) play (std::min (127, arpHeld[i] + 12 * o));
    }
    else if (mode == ArpRandom)
    {
        arpRng = arpRng * 1664525u + 1013904223u;
        play (arpSeq[(arpRng >> 8) % (uint32_t) arpSeqLen]);
    }
    else play (arpSeq[arpIndex % arpSeqLen]);
    ++arpIndex;
}

//==============================================================================
// WAV wavetables: the file is cut into 2048-sample frames (Serum / Pigments style); 16 of them,
// spread evenly through the file, become the table. A short single-cycle file is stretched to 2048.
bool AcidanceProcessor::loadUserWavetable (int osc, const juce::File& file, juce::String* error)
{
    juce::AudioFormatManager fm;
    fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader (fm.createReaderFor (file));
    if (reader == nullptr) { if (error) *error = "Not an audio file SKAZI can read"; return false; }
    const int len = (int) std::min<juce::int64> (reader->lengthInSamples, 2048 * 256);
    if (len < 64) { if (error) *error = "The file is too short"; return false; }
    juce::AudioBuffer<float> in ((int) reader->numChannels, len);
    reader->read (&in, 0, len, 0, true, true);
    std::vector<float> mono ((size_t) len);
    for (int c = 0; c < in.getNumChannels(); ++c)
        for (int i = 0; i < len; ++i) mono[(size_t) i] += in.getSample (c, i) / (float) in.getNumChannels();

    constexpr int N = Wavetables::N, F = Wavetables::kFrames;
    std::vector<float> frames ((size_t) F * N);
    const int srcFrames = len >= N ? len / N : 0;
    for (int f = 0; f < F; ++f)
    {
        float* dst = frames.data() + (size_t) f * N;
        if (srcFrames == 0)
            for (int i = 0; i < N; ++i)   // one short cycle: stretch it
            {
                const float x = (float) i * (float) len / (float) N;
                const int a = (int) x, b = (a + 1) % len;
                dst[i] = mono[(size_t) a] + (mono[(size_t) b] - mono[(size_t) a]) * (x - (float) a);
            }
        else
        {
            const int src = srcFrames == 1 ? 0 : (int) std::round ((float) f * (float) (srcFrames - 1) / (float) (F - 1));
            for (int i = 0; i < N; ++i) dst[i] = mono[(size_t) (src * N + i)];
        }
    }
    return setUserWavetable (osc, std::move (frames), file.getFileNameWithoutExtension());
}

bool AcidanceProcessor::setUserWavetable (int osc, std::vector<float> frames, const juce::String& name)
{
    osc = juce::jlimit (0, 1, osc);
    if ((int) frames.size() != Wavetables::kFrames * Wavetables::N) return false;
    // the audio thread may still read the previously active buffer during this block
    const auto now = juce::Time::getMillisecondCounter();
    if (userActive[osc].load() >= 0 && now - lastUserLoad[osc] < 250) juce::Thread::sleep (250);
    lastUserLoad[osc] = juce::Time::getMillisecondCounter();
    const int next = userActive[osc].load() == 0 ? 1 : 0;
    if (userTab[osc][next] == nullptr) userTab[osc][next] = std::make_unique<Wavetables::UserTable>();
    userTab[osc][next]->build (frames);
    userFrames[osc] = std::move (frames);
    userWavetableName[osc] = name;
    userActive[osc] = next;
    setParam ("osc" + juce::String (osc + 1) + "_wave", 8.f);
    ++programChanges;
    return true;
}

const Wavetables::UserTable* AcidanceProcessor::userTable (int osc) const
{
    const int a = userActive[juce::jlimit (0, 1, osc)].load();
    return a >= 0 ? userTab[osc][a].get() : nullptr;
}

void AcidanceProcessor::storeMorph (int which)
{
    auto& snap = which == 0 ? morphA : morphB;
    for (size_t i = 0; i < shadowMap.size(); ++i) snap[i] = shadowMap[i].raw->load();
    morphValid[which] = true;
}

void AcidanceProcessor::refreshShadow()
{
    const bool morphing = morphValid[0].load() && morphValid[1].load();
    const bool active = morphing && morphOnRaw->load() > 0.5f;
    const float k = morphRaw->load();
    for (size_t i = 0; i < shadowMap.size(); ++i)
    {
        const auto& m = shadowMap[i];
        float v = m.raw->load();
        if (active && m.morphs)
        {
            const float a = morphA[i].load(), b = morphB[i].load();
            v = m.stepped ? (k < 0.5f ? a : b) : a + (b - a) * k;
        }
        shadow[i].store (v, std::memory_order_relaxed);
    }
}

void AcidanceProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    refreshShadow();
    const int numSamples = buffer.getNumSamples();
    buffer.clear();

    const bool panicNow = panicRequest.exchange (false);
    if (panicNow) panic();
    if (pitchMoved.exchange (false))
        midi.addEvent (juce::MidiMessage::pitchWheel (1, juce::jlimit (0, 16383, (int) std::round ((screenPitch.load() + 1.f) * 8191.5f))), 0);
    if (modMoved.exchange (false))
        midi.addEvent (juce::MidiMessage::controllerEvent (1, 1, juce::jlimit (0, 127, (int) std::round (screenMod.load() * 127.f))), 0);

    // --- Transport -------------------------------------------------------
    bool playing = false, hostRunning = false;
    double bpm = pBpm->load(), pos16 = 0.0, ppq = 0.0;
    if (auto* ph = getPlayHead())
        if (auto info = ph->getPosition())
        {
            if (auto b = info->getBpm()) bpm = *b;
            if (info->getIsPlaying())
            {
                hostRunning = true;
                if (auto q = info->getPpqPosition()) { ppq = *q; pos16 = ppq * 4.0; }
            }
        }
    hostPlaying.store (hostRunning);

    if (hostRunning) playing = true;
    else if (pRun->load() > 0.5f)
    {
        if (! wasRunning) internalPos = -0.0001;
        playing = true;
        pos16 = internalPos;
    }
    clockPlaying = playing;

    const bool seqOn = pSeqOn->load() > 0.5f;
    if (! playing && wasRunning)
    {
        voices[kNumVoices].release();
        lastSlide = false;
        clockStep = std::numeric_limits<int64_t>::min();
        currentStep.store (-1);
    }
    wasRunning = playing;
    const bool seqRunning = playing && seqOn; // armed: keys go to the pattern, which plays only while one is held
    if (! seqRunning && seqKeyCount > 0) { seqKeys.fill (false); seqKeyCount = 0; }

    const double inc16 = bpm / 60.0 * 4.0 / sr;
    const double shuffleOff = pShuffle->load() * 0.33;
    const double gateLen = pSeqGate->load();

    // Arpeggiator: active when switched on and the step sequencer isn't taking the keys
    const bool arpActive = pArpOn->load() > 0.5f && ! seqRunning;
    if (arpActive != arpWasActive)
    {
        // switching between arp and direct play: let go of everything so no note can hang
        arpStop();
        allNotesOff();
        arpDirty = true;
        arpWasActive = arpActive;
    }
    const double arpBeats = Lists::divisionBeats[juce::jlimit (0, 13, (int) pArpDiv->load())];
    const double arpInc = bpm / 60.0 / arpBeats / sr;          // arp steps per sample
    const double arpSwingOff = pArpSwing->load() * 0.33;
    const double arpGate = juce::jlimit (0.05, 0.98, (double) pArpGate->load());
    if (arpActive)
    {
        // mode / octave changes rebuild the note order at the next step
        const int modeOct = (int) pArpMode->load() * 8 + (int) pArpOct->load();
        if (modeOct != lastArpModeOct) { lastArpModeOct = modeOct; arpDirty = true; }
    }

    // --- Voice parameters for this block ---------------------------------
    for (int o = 0; o < 2; ++o)
    {
        auto& os = vp.osc[o];
        const auto& p = oscP[o];
        os.on = p.on->load() > 0.5f;
        os.wave = (int) p.wave->load();
        os.wtPos = p.wtpos->load();
        os.semis = p.oct->load() * 12.f + p.semi->load() + p.fine->load() * 0.01f;
        os.level = p.level->load();
        os.uni = (int) p.uni->load();
        os.detune = p.detune->load();
        os.blend = p.blend->load();
        os.phase = p.phase->load();
        os.randPhase = p.rphase->load() > 0.5f;
        os.phaseLock = p.lock->load() > 0.5f;
        os.warp = (int) p.warp->load();
        os.warpAmt = p.warpamt->load();
        os.shape = (int) p.shape->load();
        os.shapeAmt = p.shapeamt->load();
        os.wtStep = p.wtstep->load() > 0.5f;
        os.uniMode = (int) p.unimode->load();
    }
    vp.subWave = (int) pSubWave->load(); vp.subOct = (int) pSubOct->load(); vp.subLevel = pSubLevel->load();
    vp.noiseType = (int) pNoiseType->load(); vp.noiseLevel = pNoiseLevel->load(); vp.noisePitch = pNoisePitch->load();
    for (int f = 0; f < 2; ++f)
        vp.flt[f] = { (int) fltP[f].type->load(), fltP[f].cutoff->load(), fltP[f].reso->load(),
                      fltP[f].drive->load(), fltP[f].key->load(), fltP[f].env->load() };
    vp.routing = (int) pRouting->load();
    for (int e = 0; e < kNumEnvs; ++e)
        vp.env[e] = { envP[e].a->load(), envP[e].d->load(), envP[e].s->load(), envP[e].r->load() };
    float lfoInc[kNumLfos];
    for (int i = 0; i < kNumLfos; ++i)
    {
        auto& ls = vp.lfo[i];
        ls.shape = (int) lfoP[i].shape->load();
        const bool synced = lfoP[i].sync->load() > 0.5f;
        const double beats = Lists::divisionBeats[juce::jlimit (0, 13, (int) lfoP[i].div->load())];
        ls.rateHz = synced ? (float) (bpm / 60.0 / beats) : lfoP[i].rate->load();
        ls.retrig = (int) lfoP[i].trig->load() == 1;
        ls.phase = lfoP[i].phase->load();
        ls.fade = lfoP[i].fade->load();
        for (int s = 0; s < kLfoSteps; ++s) ls.custom[s] = lfoCustom[i][s].load();
        lfoInc[i] = ls.rateHz / (float) sr;
        if (synced && playing && ! ls.retrig)
        {
            // locked to the song position (or the RUN clock), so it sits on the grid
            const double songPpq = hostRunning ? ppq : pos16 * 0.25;
            const double ph = songPpq / beats;
            globalLfo[i].phase = (float) (ph - std::floor (ph));
        }
    }
    vp.numActiveMods = 0;
    vp.lfoUsed = 0;
    for (int m = 0; m < kNumModSlots; ++m)
    {
        const ModSlot slot { (int) modP[m].src->load(), (int) modP[m].dst->load(), modP[m].amt->load() };
        if (slot.src <= 0 || slot.dst <= 0 || slot.amt == 0.f) continue;
        vp.mods[vp.numActiveMods++] = slot;
        if (slot.src >= S_Lfo1 && slot.src < S_Lfo1 + kNumLfos) vp.lfoUsed |= 1u << (slot.src - S_Lfo1);
    }
    vp.accent = pAccent->load(); vp.velSens = pVelSens->load();
    vp.warmth = pWarmth->load();
    vp.drift = pDrift->load();
    vp.spread = pSpread->load();
    vp.poly = (int) pMode->load() == 0;
    for (int o = 0; o < 2; ++o) vp.user[o] = userTable (o);
    for (int m = 0; m < kNumMacros; ++m) gm.macro[m] = pMacro[m]->load();
    const float glide = pGlide->load();
    const float seqGlide = glide > 0.001f ? glide : 0.06f;

    // --- Global modulation (FX targets and global LFO rates), once per block
    float gmod[D_Count] {};
    {
        const auto& lv = voices[lastVoice];
        float src[S_Count] {};
        for (int i = 0; i < kNumLfos; ++i) src[S_Lfo1 + i] = gm.lfo[i];
        src[S_Env2] = lv.envOut[1]; src[S_Env3] = lv.envOut[2]; src[S_Env4] = lv.envOut[3];
        src[S_ModWheel] = gm.modWheel; src[S_Velocity] = lastVelocity;
        src[S_Aftertouch] = gm.aftertouch; src[S_Keytrack] = lastKey;
        for (int m = 0; m < kNumMacros; ++m) src[S_Macro1 + m] = gm.macro[m];
        for (int k = 0; k < vp.numActiveMods; ++k)
            gmod[vp.mods[k].dst] += src[vp.mods[k].src] * vp.mods[k].amt;
    }

    auto* outL = buffer.getWritePointer (0);
    auto* outR = buffer.getWritePointer (1);
    auto midiIt = midi.cbegin();

    // --- Oversampling: voices run at F x the host rate, then filter back down.
    // Host blocks bigger than prepared are processed in chunks (never a rate switch mid-song).
    const int factorIndex = oversamplers[0] == nullptr ? 0 : juce::jlimit (0, 3, (int) pOversample->load());
    const int F = 1 << factorIndex;
    if (F != activeOversample)
    {
        activeOversample = F;
        for (auto& v : voices) v.setSampleRate (sr * F);
        for (auto& o : oversamplers) if (o != nullptr) o->reset();
        dist.reset();
    }
    juce::dsp::AudioBlock<float> hostBlock (buffer);
    const float invF = 1.f / (float) F;
    bool distOn = pDistOn->load() > 0.5f;
    const int distType = (int) pDistType->load();
    const float distDrive = juce::jlimit (0.f, 1.f, pDistDrive->load() + gmod[D_Dist]);
    const float distMix = juce::jlimit (0.f, 1.f, pDistMix->load() + gmod[D_DistMix]), distTone = pDistTone->load();
    if (distMix < 0.0001f) distOn = false;   // mix 0 (the DRIVE macro at rest): nothing to do
    const int chunk = std::max (1, oversampleBlock);

    for (int c0 = 0; c0 < numSamples; c0 += chunk)
    {
        const int len = std::min (chunk, numSamples - c0);
        auto sub = hostBlock.getSubBlock ((size_t) c0, (size_t) len);
        float* vL = outL + c0;
        float* vR = outR + c0;
        juce::dsp::AudioBlock<float> osBlock;
        if (F > 1)
        {
            osBlock = oversamplers[factorIndex - 1]->processSamplesUp (sub);
            vL = osBlock.getChannelPointer (0);
            vR = osBlock.getChannelPointer (1);
        }

        for (int k = 0; k < len; ++k)
        {
            const int i = c0 + k;
            double pos = -1.0;
            if (playing)
            {
                pos = pos16 + i * inc16;
                if (pos >= 0.0)
                {
                    const int64_t st = (int64_t) std::floor (pos);
                    stepFrac = pos - (double) st;
                    stepOffset = (st & 1) ? shuffleOff : 0.0;
                    if (st != clockStep) { clockStep = st; stepStarted = false; stepEndDone = false; }
                }
            }
            if (arpActive)
            {
                // arp clock: the song grid when playing, else a free clock from the first key
                const double ap = playing ? (pos16 + i * inc16) * 0.25 / arpBeats : arpFreePos;
                const int64_t as = (int64_t) std::floor (ap);
                arpFrac = ap - (double) as;
                arpOffset = (as & 1) ? arpSwingOff : 0.0;
                if (as != arpStep) { arpStep = as; arpStarted = false; arpGateDone = false; }
            }

            while (midiIt != midi.cend() && (*midiIt).samplePosition <= i)
            {
                handleMidi ((*midiIt).getMessage(), seqRunning);
                ++midiIt;
            }

            if (playing && pos >= 0.0)
            {
                if (! stepStarted && stepFrac >= stepOffset) { stepStarted = true; seqStepStart ((int) (clockStep % kNumSteps)); }
                if (stepStarted && ! stepEndDone && stepFrac >= stepOffset + gateLen * 0.98)
                {
                    stepEndDone = true;
                    if (seqOn) seqStepEnd();
                }
            }
            if (arpActive)
            {
                if (! arpStarted && arpFrac >= arpOffset)
                {
                    // the step starts even with no keys down, so a late key can catch it (see arpKeyDown)
                    arpStarted = true;
                    if (arpHeldCount > 0) arpFire();
                }
                if (arpStarted && ! arpGateDone && arpFrac >= arpOffset + arpGate * (1.0 - arpOffset))
                {
                    arpGateDone = true;
                    arpReleaseSounding();
                }
                if (! playing && arpHeldCount > 0) arpFreePos += arpInc;
            }

            for (int j = 0; j < F; ++j)
            {
                // Global LFOs tick at the oversampled rate so audio-rate modulation stays smooth
                for (int l = 0; l < kNumLfos; ++l)
                {
                    if ((vp.lfoUsed & (1u << l)) == 0) continue;
                    globalLfo[l].advance (lfoInc[l] * invF * std::exp2 (gmod[D_Lfo1Rate + l] * 3.f));
                    float ph = globalLfo[l].phase + vp.lfo[l].phase;
                    ph -= std::floor (ph);
                    gm.lfo[l] = lfoShapeValue (vp.lfo[l].shape, ph, globalLfo[l].sh, vp.lfo[l].custom);
                }

                float L = 0.f, R = 0.f;
                for (int v = 0; v < kNumVoices; ++v)
                    if (voices[v].isActive()) voices[v].process (vp, gm, glide, L, R);
                if (voices[kNumVoices].isActive()) voices[kNumVoices].process (vp, gm, seqGlide, L, R);
                vL[k * F + j] = L * 0.5f;
                vR[k * F + j] = R * 0.5f;
            }
        }

        // distortion runs at the oversampled rate: far less aliasing on the hard types
        if (distOn)
            dist.process (vL, vR, len * F, distType, distDrive, distMix, distTone, F, (float) (sr * F));
        if (F > 1)
            oversamplers[factorIndex - 1]->processSamplesDown (sub);
    }

    // pad to the common latency (the same for every oversampling factor)
    if (latencyTotal > 0 && ! padBuf[0].empty())
    {
        const int pad = latencyPad[factorIndex];
        const int size = (int) padBuf[0].size();
        for (int i = 0; i < numSamples; ++i)
        {
            float* io[2] = { outL + i, outR + i };
            for (int c = 0; c < 2; ++c)
            {
                padBuf[c][(size_t) padPos] = *io[c];
                int rp = padPos - pad;
                if (rp < 0) rp += size;
                *io[c] = padBuf[c][(size_t) rp];
            }
            padPos = (padPos + 1) % size;
        }
    }

    while (midiIt != midi.cend())
    {
        handleMidi ((*midiIt).getMessage(), seqRunning);
        ++midiIt;
    }
    midi.clear();
    if (panicNow)
        for (int ch = 1; ch <= 16; ++ch)
        {
            midi.addEvent (juce::MidiMessage::allNotesOff (ch), 0);
            midi.addEvent (juce::MidiMessage::controllerEvent (ch, 121, 0), 0);
        }

    // --- FX rack -----------------------------------------------------------
    auto clamp01 = [] (float x) { return juce::jlimit (0.f, 1.f, x); };
    if (pFxfOn->load() > 0.5f)
        fxFilter.process (outL, outR, numSamples, (int) pFxfType->load(),
                          pFxfCut->load() * std::exp2 (juce::jlimit (-1.f, 1.f, gmod[D_FxCut]) * 6.f),
                          pFxfRes->load(), pFxfDrive->load(), pFxfMix->load());
    if (pMfxOn->load() > 0.5f)
        modFx.process (outL, outR, numSamples, (int) pMfxType->load(), pMfxRate->load(), pMfxDepth->load(),
                       pMfxFb->load(), clamp01 (pMfxMix->load() + gmod[D_ModFx]));
    if (pDlyOn->load() > 0.5f)
    {
        const float t = pDlySync->load() > 0.5f
                          ? (float) (Lists::divisionBeats[juce::jlimit (0, 13, (int) pDlyDiv->load())] * 60.0 / bpm)
                          : pDlyMs->load() * 0.001f;
        delay.process (outL, outR, numSamples, t, pDlyPP->load() > 0.5f, pDlyFb->load(),
                       clamp01 (pDlyMix->load() + gmod[D_Delay]));
    }
    if (pRevOn->load() > 0.5f)
        reverb.process (outL, outR, numSamples, pRevSize->load(), pRevDecay->load(), pRevDamp->load(),
                        clamp01 (pRevMix->load() + gmod[D_Reverb]));
    if (pCompOn->load() > 0.5f)
        comp.process (outL, outR, numSamples, (int) pCompMode->load(), pCompThr->load(), pCompRatio->load(),
                      pCompAtt->load(), pCompRel->load(), pCompMakeup->load(), pCompDepth->load());
    if (pWidOn->load() > 0.5f)
        width.process (outL, outR, numSamples, pWidAmt->load(), pWidMono->load());
    if (pEqOn->load() > 0.5f)
        eq.process (outL, outR, numSamples, pEqLf->load(), pEqLg->load(), pEqMf->load(), pEqMq->load(),
                    pEqMg->load(), pEqHf->load(), pEqHg->load());

    // Kick duck: a sidechain-style dip on every beat of the chosen rate, locked to the song (or RUN,
    // or a free clock when nothing plays). 1.5 ms attack so the dip itself never clicks.
    if (pDuckOn->load() > 0.5f)
    {
        const double divBeats = Lists::divisionBeats[juce::jlimit (0, 13, (int) pDuckDiv->load())];
        const float depth = pDuckDepth->load(), rel = std::max (0.05f, pDuckRel->load());
        const float power = 1.f + pDuckCurve->load() * 3.f;
        const double beatsPerSample = bpm / 60.0 / sr;
        const double atk = 0.0015 * bpm / 60.0 / divBeats;   // attack as a share of one duck period
        for (int i = 0; i < numSamples; ++i)
        {
            const double beats = playing ? (pos16 + i * inc16) * 0.25 : duckFreePos + i * beatsPerSample;
            double ph = beats / divBeats;
            ph -= std::floor (ph);
            float dip;
            if (ph < atk) dip = (float) (ph / atk);
            else
            {
                const float t = (float) ((ph - atk) / rel);
                dip = t >= 1.f ? 0.f : std::pow (1.f - t, power);
            }
            const float gn = 1.f - depth * dip;
            outL[i] *= gn;
            outR[i] *= gn;
        }
    }
    if (! playing) duckFreePos += numSamples * bpm / 60.0 / sr;

    const float vol = pVolume->load(), pan = pPan->load();
    const float gl = vol * std::min (1.f, 1.f - pan), gr = vol * std::min (1.f, 1.f + pan);
    for (int i = 0; i < numSamples; ++i)
    {
        outL[i] = softLimit (outL[i] * gl);
        outR[i] = softLimit (outR[i] * gr);
    }

    int w = scopeWrite.load (std::memory_order_relaxed);
    for (int i = 0; i < numSamples; ++i)
    {
        scope[(size_t) w].store ((outL[i] + outR[i]) * 0.5f, std::memory_order_relaxed);
        w = (w + 1) % kScopeSize;
    }
    scopeWrite.store (w, std::memory_order_release);
    fader.process (buffer, getSampleRate());

    if (playing && ! hostRunning)
        internalPos += numSamples * inc16;
}

std::unique_ptr<juce::XmlElement> AcidanceProcessor::createStateXml()
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    auto* extra = xml->createNewChildElement ("ACIDANCE_DATA");
    extra->setAttribute ("seq", seq.serialise());
    extra->setAttribute ("seqLocks", seq.serialiseLocks());
    for (int o = 0; o < 2; ++o)
        if (! userFrames[o].empty())
        {
            // 16-bit samples, base64: about 90 kB per table
            juce::MemoryBlock mb (userFrames[o].size() * 2);
            auto* d = static_cast<juce::int16*> (mb.getData());
            for (size_t i = 0; i < userFrames[o].size(); ++i)
                d[i] = (juce::int16) juce::jlimit (-32767, 32767, (int) std::round (userFrames[o][i] * 32767.f));
            extra->setAttribute ("userWt" + juce::String (o + 1), mb.toBase64Encoding());
            extra->setAttribute ("userWtName" + juce::String (o + 1), userWavetableName[o]);
        }
    for (int w = 0; w < 2; ++w)
        if (morphValid[w].load())
        {
            juce::String s;
            auto& snap = w == 0 ? morphA : morphB;
            for (size_t i = 0; i < shadowMap.size(); ++i)
                s << shadowMap[i].param->getParameterID() << "=" << juce::String (snap[i].load(), 5) << ";";
            extra->setAttribute (w == 0 ? "morphA" : "morphB", s);
        }
    extra->setAttribute ("program", currentProgram);
    extra->setAttribute ("presetName", currentPresetName);
    extra->setAttribute ("presetFolder", currentPresetFolder);
    extra->setAttribute ("skin", skin.load());
    extra->setAttribute ("skinVersion", 2);   // 2: BULLY skin exists (and is the default)
    extra->setAttribute ("uiScale", (double) uiScale.load());
    juce::String shapes;
    for (auto& l : lfoCustom)
        for (auto& s : l) shapes << juce::String (s.load(), 3) << ",";
    extra->setAttribute ("lfoShapes", shapes);
    return xml;
}

void AcidanceProcessor::applyStateXml (const juce::XmlElement& source)
{
    juce::XmlElement xml (source);
    if (auto* extra = xml.getChildByName ("ACIDANCE_DATA"))
    {
        seq.deserialise (extra->getStringAttribute ("seq"));
        seq.deserialiseLocks (extra->getStringAttribute ("seqLocks"));
        for (int o = 0; o < 2; ++o)
        {
            const auto enc = extra->getStringAttribute ("userWt" + juce::String (o + 1));
            juce::MemoryBlock mb;
            if (enc.isNotEmpty() && mb.fromBase64Encoding (enc)
                && mb.getSize() == (size_t) Wavetables::kFrames * Wavetables::N * 2)
            {
                std::vector<float> frames ((size_t) Wavetables::kFrames * Wavetables::N);
                const auto* d = static_cast<const juce::int16*> (mb.getData());
                for (size_t i = 0; i < frames.size(); ++i) frames[i] = (float) d[i] / 32767.f;
                // (the saved wave choice comes back with the parameters right after this)
                setUserWavetable (o, std::move (frames), extra->getStringAttribute ("userWtName" + juce::String (o + 1)));
            }
        }
        for (int w = 0; w < 2; ++w)
        {
            const auto s = extra->getStringAttribute (w == 0 ? "morphA" : "morphB");
            auto& snap = w == 0 ? morphA : morphB;
            for (size_t i = 0; i < shadowMap.size(); ++i) snap[i] = shadowMap[i].raw->load();   // missing entries: as now
            for (auto& kv : juce::StringArray::fromTokens (s, ";", ""))
            {
                const auto id = kv.upToFirstOccurrenceOf ("=", false, false);
                for (size_t i = 0; i < shadowMap.size(); ++i)
                    if (shadowMap[i].param->getParameterID() == id) { snap[i] = kv.fromFirstOccurrenceOf ("=", false, false).getFloatValue(); break; }
            }
            morphValid[w] = s.isNotEmpty();
        }
        currentProgram = juce::jlimit (0, Presets::count() - 1, extra->getIntAttribute ("program"));
        currentPresetName = extra->getStringAttribute ("presetName", Presets::name (currentProgram));
        currentPresetFolder = extra->getStringAttribute ("presetFolder", Presets::category (currentProgram));
        auto vals = juce::StringArray::fromTokens (extra->getStringAttribute ("lfoShapes"), ",", "");
        for (int i = 0; i < std::min (vals.size(), kNumLfos * kLfoSteps); ++i)
            lfoCustom[i / kLfoSteps][i % kLfoSteps] = juce::jlimit (-1.f, 1.f, vals[i].getFloatValue());
        xml.removeChildElement (extra, true);
    }
    if (xml.hasTagName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (xml));
    ++programChanges;
}

void AcidanceProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    copyXmlToBinary (*createStateXml(), destData);
}

void AcidanceProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    fader.cancel(); // the saved sound wins over any preset change still waiting
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));
    if (xml == nullptr) return;
    if (auto* extra = xml->getChildByName ("ACIDANCE_DATA"))
    {
        // the skin only comes from the session, not presets. Sessions from before the BULLY skin
        // that still had the old default (Hyper) move to the new default look.
        int s = juce::jlimit (0, 5, extra->getIntAttribute ("skin", 5));
        if (extra->getIntAttribute ("skinVersion", 1) < 2 && s == 4) s = 5;
        skin = s;
        uiScale = (float) juce::jlimit (0.5, 1.5, extra->getDoubleAttribute ("uiScale", 0.9));
    }
    applyStateXml (*xml);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AcidanceProcessor();
}

juce::AudioProcessorEditor* AcidanceProcessor::createEditor()
{
    return skazi::withUpdateNotice (new AcidanceEditor (*this));
}
