#include "KubeEngine.h"
#include <cmath>
#include <complex>

namespace kube
{
namespace
{
    constexpr int kCtrl = 8; // control-rate step: pitch, filter coefficients, LFO
    constexpr double kLfoBeats[] = { 4.0, 2.0, 1.0, 0.5, 0.25, 0.125, 2.0 / 3.0, 1.0 / 3.0, 1.0 / 6.0, 1.5, 0.75, 8.0, 16.0 };
    constexpr double kDelayBeats[] = { 1.0, 0.5, 0.75, 0.25, 1.5, 2.0 / 3.0, 1.0 / 3.0, 2.0 };

    template <typename T, size_t N>
    T pickAt (const T (&list)[N], int index) { return list[(size_t) juce::jlimit (0, (int) N - 1, index)]; }

    // one-pole coefficient that gets ~95% of the way in `seconds`
    float coef (float seconds, double sr, int steps = 1)
    {
        if (seconds <= 0.0005f) return 1.0f;
        return 1.0f - (float) std::exp (-3.0 * steps / (seconds * sr));
    }

    double midiHz (double note) { return 440.0 * std::pow (2.0, (note - 69.0) / 12.0); }

    void resonanceK (int type, float reso, float& k1, float& k2)
    {
        const float r = juce::jlimit (0.0f, 1.0f, reso);
        if (type == LP24) { k1 = 1.848f; k2 = 0.765f - r * 0.745f; }
        else { k1 = 1.414f - r * 1.394f; k2 = k1; }
    }

    struct EnvCoef { float att, dec, rel, sus; };
    EnvCoef envCoef (const Adsr& a, double sr)
    {
        return { a.a <= 0.0005f ? 1.0f : (float) (1.0 / (a.a * sr)), coef (a.d, sr), coef (a.r, sr), a.s };
    }

    float envNext (float& level, int& stage, const EnvCoef& c)
    {
        switch (stage)
        {
            case 1: // attack
                level += c.att;
                if (level >= 1.0f) { level = 1.0f; stage = 2; }
                break;
            case 2: // decay, aimed a little past sustain so it really gets there
                level += (c.sus - 0.002f - level) * c.dec;
                if (level <= c.sus) { level = c.sus; stage = 3; }
                break;
            case 3: level = c.sus; break;
            case 4: // release
                level += (-0.002f - level) * c.rel;
                if (level <= 0.0f) { level = 0.0f; stage = 0; }
                break;
            default: level = 0.0f; break;
        }
        return level;
    }

    float fold (float x, float amount)
    {
        if (amount <= 0.0f) return x;
        const float y = std::sin (x * juce::MathConstants<float>::halfPi * (1.0f + amount * 6.0f));
        const float m = juce::jmin (1.0f, amount * 4.0f);
        return x + m * (y - x);
    }
}

// Oscillator parameter ids: the oscillator letter, then the control
const char* const oscIds[2][16] = {
    { "aOn", "aTable", "aPos", "aOct", "aSemi", "aFine", "aLevel", "aUni", "aDetune", "aWidth", "aBlend", "aRand", "aWarp1", "aWarp1Amt", "aWarp2", "aWarp2Amt" },
    { "bOn", "bTable", "bPos", "bOct", "bSemi", "bFine", "bLevel", "bUni", "bDetune", "bWidth", "bBlend", "bRand", "bWarp1", "bWarp1Amt", "bWarp2", "bWarp2Amt" } };
const char* const oscNames[2][16] = {
    { "Osc A On", "Osc A Table", "Osc A Position", "Osc A Octave", "Osc A Semi", "Osc A Fine", "Osc A Level", "Osc A Unison", "Osc A Detune",
      "Osc A Width", "Osc A Blend", "Osc A Phase Random", "Osc A Warp 1", "Osc A Warp 1 Amount", "Osc A Warp 2", "Osc A Warp 2 Amount" },
    { "Osc B On", "Osc B Table", "Osc B Position", "Osc B Octave", "Osc B Semi", "Osc B Fine", "Osc B Level", "Osc B Unison", "Osc B Detune",
      "Osc B Width", "Osc B Blend", "Osc B Phase Random", "Osc B Warp 1", "Osc B Warp 1 Amount", "Osc B Warp 2", "Osc B Warp 2 Amount" } };

const juce::StringArray& warpNames()
{
    static const juce::StringArray n { "OFF", "SYNC", "BEND+", "BEND-", "PWM", "ASYM+", "ASYM-", "MIRROR", "FLIP", "QUANTIZE",
                                       "FM", "AM", "RM" }; // FM, AM and RM come from the other oscillator
    return n;
}

double lfoBeats (int index) { return pickAt (kLfoBeats, index); }
double delayBeats (int index) { return pickAt (kDelayBeats, index); }

const std::vector<skazi::ParamDef>& paramDefs()
{
    using P = skazi::ParamDef;
    static const std::vector<P> defs = [] {
        std::vector<P> d;
        for (int o = 0; o < 2; ++o)
        {
            const auto& id = oscIds[o];
            const auto& nm = oscNames[o];
            d.push_back ({ id[0], nm[0], P::Bool, 0, 1, o == 0 ? 1.0f : 0.0f });
            d.push_back ({ id[1], nm[1], P::Choice, 0, (float) kUserTable, o == 0 ? 20.0f : 14.0f, "", 1, 1, tableNames() }); // SAW STACK, PHASE DIST
            d.push_back ({ id[2], nm[2], P::Float, 0, 100, o == 0 ? 0.0f : 70.0f, "%", 0.1f });
            d.push_back ({ id[3], nm[3], P::Int, -3, 3, 0 });
            d.push_back ({ id[4], nm[4], P::Int, -12, 12, 0 });
            d.push_back ({ id[5], nm[5], P::Float, -100, 100, o == 0 ? 0.0f : 7.0f, "ct", 0.1f });
            d.push_back ({ id[6], nm[6], P::Float, 0, 100, 80, "%", 0.1f });
            d.push_back ({ id[7], nm[7], P::Int, 1, (float) Engine::kMaxUnison, 1 });
            d.push_back ({ id[8], nm[8], P::Float, 0, 100, 25, "%", 0.1f });
            d.push_back ({ id[9], nm[9], P::Float, 0, 100, 60, "%", 0.1f });
            d.push_back ({ id[10], nm[10], P::Float, 0, 100, 75, "%", 0.1f });
            d.push_back ({ id[11], nm[11], P::Float, 0, 100, 100, "%", 0.1f });
            d.push_back ({ id[12], nm[12], P::Choice, 0, (float) warpNames().size() - 1, WarpOff, "", 1, 1, warpNames() });
            d.push_back ({ id[13], nm[13], P::Float, 0, 100, 0, "%", 0.1f });
            d.push_back ({ id[14], nm[14], P::Choice, 0, (float) warpNames().size() - 1, WarpOff, "", 1, 1, warpNames() });
            d.push_back ({ id[15], nm[15], P::Float, 0, 100, 0, "%", 0.1f });
        }
        const juce::StringArray lfoRates { "1/1", "1/2", "1/4", "1/8", "1/16", "1/32", "1/4T", "1/8T", "1/16T", "1/4D", "1/8D", "2/1", "4/1" };
        const juce::StringArray dlyTimes { "1/4", "1/8", "1/8D", "1/16", "1/4D", "1/4T", "1/8T", "1/2" };
        const std::vector<P> rest = {
            { "sub", "Sub Level", P::Float, 0, 100, 0, "%", 0.1f },
            { "subOct", "Sub Octave", P::Choice, 0, 1, 0, "", 1, 1, { "-1 OCT", "-2 OCT" } },
            { "noise", "Noise", P::Float, 0, 100, 0, "%", 0.1f },
            { "fold", "Wavefold", P::Float, 0, 100, 0, "%", 0.1f },

            { "fltType", "Filter Type", P::Choice, 0, 4, LP24, "", 1, 1, { "LP 24", "LP 12", "BAND", "HIGH", "NOTCH" } },
            { "cutoff", "Cutoff", P::Float, 20, 20000, 6000, "Hz", 1, 0.25f },
            { "reso", "Resonance", P::Float, 0, 100, 20, "%", 0.1f },
            { "fltEnv", "Filter Env Amount", P::Float, -100, 100, 0, "%", 0.1f },
            { "keyTrk", "Key Track", P::Float, 0, 100, 50, "%", 0.1f },
            { "drive", "Drive", P::Float, 0, 100, 0, "%", 0.1f },

            { "ampA", "Amp Attack", P::Float, 0, 5000, 2, "ms", 0.1f, 0.3f },
            { "ampD", "Amp Decay", P::Float, 1, 5000, 300, "ms", 0.1f, 0.3f },
            { "ampS", "Amp Sustain", P::Float, 0, 100, 80, "%", 0.1f },
            { "ampR", "Amp Release", P::Float, 1, 8000, 150, "ms", 0.1f, 0.3f },
            { "fltA", "Filter Attack", P::Float, 0, 5000, 1, "ms", 0.1f, 0.3f },
            { "fltD", "Filter Decay", P::Float, 1, 5000, 250, "ms", 0.1f, 0.3f },
            { "fltS", "Filter Sustain", P::Float, 0, 100, 30, "%", 0.1f },
            { "fltR", "Filter Release", P::Float, 1, 8000, 200, "ms", 0.1f, 0.3f },

            { "envPos", "Filter Env > Position", P::Float, -100, 100, 0, "%", 0.1f },
            { "pitchAmt", "Pitch Env Amount", P::Float, -48, 48, 0, "st", 0.1f },
            { "pitchDec", "Pitch Env Decay", P::Float, 1, 2000, 60, "ms", 0.1f, 0.4f },

            { "lfoOn", "LFO On", P::Bool, 0, 1, 1 },
            { "lfoShape", "LFO Shape", P::Choice, 0, 4, Sine, "", 1, 1, { "SINE", "TRI", "SAW", "SQUARE", "S&H" } },
            { "lfoSync", "LFO Sync", P::Bool, 0, 1, 1 },
            { "lfoRate", "LFO Rate", P::Choice, 0, (float) lfoRates.size() - 1, 3, "", 1, 1, lfoRates },
            { "lfoHz", "LFO Hz", P::Float, 0.05f, 30, 2, "Hz", 0.01f, 0.4f },
            { "lfoCut", "LFO > Cutoff", P::Float, -100, 100, 0, "%", 0.1f },
            { "lfoPos", "LFO > Position", P::Float, -100, 100, 0, "%", 0.1f },
            { "lfoFold", "LFO > Fold", P::Float, 0, 100, 0, "%", 0.1f },

            { "mode", "Voice Mode", P::Choice, 0, 2, Poly, "", 1, 1, { "POLY", "MONO", "LEGATO" } },
            { "glide", "Glide", P::Float, 0, 2000, 0, "ms", 0.1f, 0.4f },
            { "vel", "Velocity", P::Float, 0, 100, 30, "%", 0.1f },

            { "chOn", "Chorus On", P::Bool, 0, 1, 0 },
            { "chRate", "Chorus Rate", P::Float, 0.05f, 5, 0.4f, "Hz", 0.01f, 0.5f },
            { "chDepth", "Chorus Depth", P::Float, 0, 100, 40, "%", 0.1f },
            { "chMix", "Chorus Mix", P::Float, 0, 100, 40, "%", 0.1f },
            { "dlyOn", "Delay On", P::Bool, 0, 1, 0 },
            { "dlyTime", "Delay Time", P::Choice, 0, (float) dlyTimes.size() - 1, 2, "", 1, 1, dlyTimes },
            { "dlyFb", "Delay Feedback", P::Float, 0, 95, 35, "%", 0.1f },
            { "dlyTone", "Delay Tone", P::Float, 0, 100, 50, "%", 0.1f },
            { "dlyMix", "Delay Mix", P::Float, 0, 100, 25, "%", 0.1f },
            { "revOn", "Reverb On", P::Bool, 0, 1, 0 },
            { "revSize", "Reverb Size", P::Float, 0, 100, 60, "%", 0.1f },
            { "revDamp", "Reverb Damping", P::Float, 0, 100, 50, "%", 0.1f },
            { "revMix", "Reverb Mix", P::Float, 0, 100, 20, "%", 0.1f },
            { "out", "Output", P::Float, -36, 6, -6, "dB", 0.1f },
        };
        d.insert (d.end(), rest.begin(), rest.end());
        return d;
    }();
    return defs;
}

Settings Settings::fromValues (const std::function<float (const char*)>& v)
{
    Settings s;
    for (int o = 0; o < 2; ++o)
    {
        const auto& id = oscIds[o];
        auto& c = s.osc[(size_t) o];
        c.on = v (id[0]) > 0.5f;
        c.table = juce::jlimit (0, kUserTable, (int) v (id[1]));
        c.pos = v (id[2]) * 0.01f;
        c.oct = (int) std::round (v (id[3]));
        c.semi = (int) std::round (v (id[4]));
        c.fine = v (id[5]);
        c.level = v (id[6]) * 0.01f;
        c.unison = juce::jlimit (1, Engine::kMaxUnison, (int) std::round (v (id[7])));
        c.detune = v (id[8]) * 0.01f;
        c.width = v (id[9]) * 0.01f;
        c.blend = v (id[10]) * 0.01f;
        c.rand = v (id[11]) * 0.01f;
        c.warp[0] = { (int) v (id[12]), v (id[13]) * 0.01f };
        c.warp[1] = { (int) v (id[14]), v (id[15]) * 0.01f };
    }
    s.sub = v ("sub") * 0.01f; s.subOct = (int) v ("subOct"); s.noise = v ("noise") * 0.01f; s.fold = v ("fold") * 0.01f;
    s.filterType = (int) v ("fltType"); s.cutoff = v ("cutoff"); s.reso = v ("reso") * 0.01f;
    s.filterEnv = v ("fltEnv") * 0.01f; s.keyTrack = v ("keyTrk") * 0.01f; s.drive = v ("drive") * 0.01f;
    s.amp = { v ("ampA") * 0.001f, v ("ampD") * 0.001f, v ("ampS") * 0.01f, v ("ampR") * 0.001f };
    s.flt = { v ("fltA") * 0.001f, v ("fltD") * 0.001f, v ("fltS") * 0.01f, v ("fltR") * 0.001f };
    s.envPos = v ("envPos") * 0.01f; s.pitchAmt = v ("pitchAmt"); s.pitchDec = v ("pitchDec") * 0.001f;
    s.lfoOn = v ("lfoOn") > 0.5f; s.lfoShape = (int) v ("lfoShape"); s.lfoSync = v ("lfoSync") > 0.5f; s.lfoRate = (int) v ("lfoRate"); s.lfoHz = v ("lfoHz");
    s.lfoCut = v ("lfoCut") * 0.01f; s.lfoPos = v ("lfoPos") * 0.01f; s.lfoFold = v ("lfoFold") * 0.01f;
    s.mode = (int) v ("mode"); s.glide = v ("glide") * 0.001f; s.velSens = v ("vel") * 0.01f;
    s.chorusOn = v ("chOn") > 0.5f; s.chorusRate = v ("chRate"); s.chorusDepth = v ("chDepth") * 0.01f; s.chorusMix = v ("chMix") * 0.01f;
    s.delayOn = v ("dlyOn") > 0.5f; s.delayTime = (int) v ("dlyTime"); s.delayFb = v ("dlyFb") * 0.01f;
    s.delayTone = v ("dlyTone") * 0.01f; s.delayMix = v ("dlyMix") * 0.01f;
    s.reverbOn = v ("revOn") > 0.5f; s.reverbSize = v ("revSize") * 0.01f; s.reverbDamp = v ("revDamp") * 0.01f; s.reverbMix = v ("revMix") * 0.01f;
    s.outGain = juce::Decibels::decibelsToGain (v ("out"));
    return s;
}

float filterMagnitude (int type, float cutoff, float reso, float freq)
{
    float k1, k2;
    resonanceK (type, reso, k1, k2);
    const std::complex<float> s (0.0f, freq / juce::jmax (1.0f, cutoff));
    auto sec = [&] (float k) { return s * s + k * s + 1.0f; };
    std::complex<float> h;
    switch (type)
    {
        case LP24:     h = 1.0f / (sec (k1) * sec (k2)); break;
        case LP12:     h = 1.0f / sec (k1); break;
        case BandPass: h = k1 * s / sec (k1); break;
        case HighPass: h = s * s / sec (k1); break;
        default:       h = (s * s + 1.0f) / sec (k1); break; // notch
    }
    return std::abs (h);
}

void Engine::prepare (double sampleRate, int maxBlock)
{
    sr = sampleRate;
    bankTable (0); // build the bank before the first note
    voiceBuf.setSize (2, juce::jmax (1, maxBlock));
    chorusMax = juce::jmax (1, maxBlock);
    chorus.prepare ({ sr, (juce::uint32) chorusMax, 2 });
    chorus.setCentreDelay (8.0f);
    chorus.setFeedback (0.1f);
    reverb.setSampleRate (sr);
    const size_t dlyLen = (size_t) (sr * 6.0) + 4; // two bars at 40 bpm
    dlyL.assign (dlyLen, 0.0f); dlyR.assign (dlyLen, 0.0f);
    for (auto* sm : { &outSmooth, &chorusMixSmooth, &delayMixSmooth, &reverbMixSmooth })
        sm->reset (sr, 0.05);
    reset();
}

void Engine::reset()
{
    for (auto& v : voices) { v.active = false; v.amp = {}; v.flt = {}; }
    held.clear();
    chorus.reset();
    reverb.reset();
    std::fill (dlyL.begin(), dlyL.end(), 0.0f); std::fill (dlyR.begin(), dlyR.end(), 0.0f);
    dlyLpL = dlyLpR = 0; dlySamples = 0;
}

void Engine::allNotesOff()
{
    for (auto& v : voices) { v.amp.gate (false); v.flt.gate (false); }
    held.clear();
}

float Engine::lfoValue (int shape)
{
    const float ph = (float) lfoPhase;
    switch (shape)
    {
        case Sine:     return std::sin (ph * juce::MathConstants<float>::twoPi);
        case Triangle: return ph < 0.5f ? 4.0f * ph - 1.0f : 3.0f - 4.0f * ph;
        case Saw:      return 1.0f - 2.0f * ph; // falling: psy filter wobbles snap open on the beat
        case Square:   return ph < 0.5f ? 1.0f : -1.0f;
        default:       return sh;
    }
}

void Engine::startVoice (Voice& v, int note, float velocity, bool retrigger, const Settings& s)
{
    const bool wasIdle = ! v.active;
    v.active = true;
    v.note = note;
    v.velocity = velocity;
    v.age = ++ageCounter;
    v.targetPitch = note;
    if (wasIdle || s.mode == Poly || s.glide <= 0.0f) v.pitch = note;

    if (wasIdle)
    {
        for (int o = 0; o < 2; ++o) // RAND 0: the same start every note (punchy basses); 100: free-running like analog
        {
            const auto& c = s.osc[(size_t) o];
            for (int u = 0; u < kMaxUnison; ++u)
            {
                const double spread = c.unison > 1 ? u / (double) c.unison : 0.0;
                v.phase[(size_t) o][(size_t) u] = spread * (1.0 - c.rand) + c.rand * rng.nextDouble();
            }
        }
        v.lastOsc[0] = v.lastOsc[1] = 0.0f;
        v.subPhase = 0;
        for (auto& f : v.s1) f.reset();
        for (auto& f : v.s2) f.reset();
        v.amp.level = 0; v.flt.level = 0;
    }
    if (retrigger)
    {
        v.amp.stage = Env::Attack;
        v.flt.stage = Env::Attack;
        v.flt.level = 0; // the filter sweep always starts from the bottom: every note gets its pluck
        v.pitchEnv = 1.0f;
    }
    controlCount = 0; // fresh coefficients for the new note right away
}

void Engine::noteOn (int note, float velocity, const Settings& s)
{
    const bool anyActive = std::any_of (voices.begin(), voices.end(), [] (const Voice& v) { return v.active && v.amp.stage != Env::Release; });
    if (! anyActive) lfoPhase = 0.0; // free LFO restarts with the phrase

    if (s.mode == Poly)
    {
        Voice* pick = nullptr;
        for (auto& v : voices) if (v.active && v.note == note) pick = &v;
        if (pick == nullptr) for (auto& v : voices) if (! v.active) { pick = &v; break; }
        if (pick == nullptr) // steal: oldest released voice first, then the oldest one
        {
            for (auto& v : voices)
                if (v.amp.stage == Env::Release && (pick == nullptr || v.age < pick->age)) pick = &v;
            if (pick == nullptr)
                for (auto& v : voices)
                    if (pick == nullptr || v.age < pick->age) pick = &v;
        }
        startVoice (*pick, note, velocity, true, s);
        return;
    }

    auto& v = voices[0];
    const bool overlapping = ! held.empty() && v.active && v.amp.stage != Env::Release;
    held.erase (std::remove (held.begin(), held.end(), note), held.end());
    held.push_back (note);
    if (s.mode == Legato && overlapping)
    {
        v.note = note; v.targetPitch = note; v.velocity = velocity;
        if (s.glide <= 0.0f) v.pitch = note;
        return;
    }
    startVoice (v, note, velocity, true, s);
}

void Engine::noteOff (int note, const Settings& s)
{
    if (s.mode == Poly)
    {
        for (auto& v : voices)
            if (v.active && v.note == note && v.amp.stage != Env::Release) { v.amp.gate (false); v.flt.gate (false); }
        return;
    }
    held.erase (std::remove (held.begin(), held.end(), note), held.end());
    auto& v = voices[0];
    if (! v.active || v.note != note) return;
    if (! held.empty()) // fall back to the key still held, legato
    {
        v.note = held.back(); v.targetPitch = v.note;
        if (s.glide <= 0.0f) v.pitch = v.note;
    }
    else { v.amp.gate (false); v.flt.gate (false); }
}

void Engine::updateControl (Voice& v, const Settings& s, float lfo, const std::array<WarpState, 2>& warps)
{
    if (v.pitch != v.targetPitch)
    {
        v.pitch += (v.targetPitch - v.pitch) * coef (s.glide, sr, kCtrl);
        if (std::abs (v.pitch - v.targetPitch) < 0.001) v.pitch = v.targetPitch;
    }
    v.pitchEnv *= 1.0f - coef (s.pitchDec, sr, kCtrl);
    const double notePitch = v.pitch + bend * 2.0 + s.pitchAmt * v.pitchEnv;
    const float fenv = v.flt.level;

    for (size_t o = 0; o < 2; ++o)
    {
        const auto& c = s.osc[o];
        const double base = midiHz (notePitch + c.oct * 12 + c.semi + c.fine * 0.01) / sr;
        const int n = c.unison;
        // BLEND sets the outer voices against the centre one(s); the stack keeps the same power either way
        auto voiceGain = [&] (int u) { return n == 1 || u == (n - 1) / 2 || u == n / 2 ? 1.0f : c.blend; };
        float power = 0.0f;
        for (int u = 0; u < n; ++u) power += voiceGain (u) * voiceGain (u);
        const float norm = c.level / std::sqrt (juce::jmax (1.0e-6f, power));
        const double detuneSemis = std::pow ((double) c.detune, 1.5); // up to a semitone each way, fine control at the bottom
        double maxInc = base;
        for (int u = 0; u < n; ++u)
        {
            const float spread = n == 1 ? 0.0f : (float) u / (float) (n - 1) * 2.0f - 1.0f;
            const double inc = base * std::pow (2.0, spread * detuneSemis / 12.0);
            v.inc[o][(size_t) u] = (float) inc;
            maxInc = juce::jmax (maxInc, inc);
            const float pan = spread * c.width;
            const float a = (pan + 1.0f) * juce::MathConstants<float>::pi * 0.25f;
            const float gain = voiceGain (u) * norm;
            v.panL[o][(size_t) u] = std::cos (a) * juce::MathConstants<float>::sqrt2 * gain;
            v.panR[o][(size_t) u] = std::sin (a) * juce::MathConstants<float>::sqrt2 * gain;
        }
        v.level[o] = juce::jlimit (0, kLevels - 1, Wavetable::levelFor (maxInc) + warps[o].levelAdd);
        v.pos[o] = juce::jlimit (0.0f, 1.0f, c.pos + s.envPos * fenv + s.lfoPos * lfo * 0.5f + modWheel * 0.5f);
    }
    v.foldAmt = juce::jlimit (0.0f, 1.0f, s.fold + s.lfoFold * (lfo * 0.5f + 0.5f));
    v.subInc = (float) (midiHz (notePitch - 12.0 * (s.subOct + 1)) / sr);

    const double octs = s.filterEnv * 6.0 * fenv + s.lfoCut * 4.0 * lfo + s.keyTrack * (notePitch - 60.0) / 12.0;
    const double fc = juce::jlimit (20.0, sr * 0.46, s.cutoff * std::pow (2.0, octs));
    v.g = (float) std::tan (juce::MathConstants<double>::pi * fc / sr);
    resonanceK (s.filterType, s.reso, v.k1, v.k2);
}

void Engine::renderVoices (float* l, float* r, int n, const Settings& s, const skazi::HostTime& host)
{
    const Wavetable* tables[2] = { &tableFor (s.osc[0], 0), &tableFor (s.osc[1], 1) };
    std::array<WarpState, 2> warps;
    warps[0].set (s.osc[0]);
    warps[1].set (s.osc[1]);
    const auto ampC = envCoef (s.amp, sr), fltC = envCoef (s.flt, sr);
    const double lfoInc = (s.lfoSync ? host.bpm / 60.0 / lfoBeats (s.lfoRate) : (double) s.lfoHz) / sr;
    const float driveGain = 1.0f + s.drive * 8.0f, driveNorm = 1.0f / std::sqrt (driveGain);

    for (int i = 0; i < n; ++i)
    {
        if (controlCount-- <= 0)
        {
            controlCount = kCtrl - 1;
            const float lfo = s.lfoOn ? lfoValue (s.lfoShape) : 0.0f; // off: the phase keeps running so it comes back in time
            uiLfo.store (lfo, std::memory_order_relaxed);
            for (auto& v : voices)
                if (v.active) updateControl (v, s, lfo, warps);
        }
        lfoPhase += lfoInc;
        if (lfoPhase >= 1.0) { lfoPhase -= std::floor (lfoPhase); sh = rng.nextFloat() * 2.0f - 1.0f; }

        float outL = 0, outR = 0;
        for (auto& v : voices)
        {
            if (! v.active) continue;
            int st = (int) v.amp.stage;
            const float a = envNext (v.amp.level, st, ampC);
            v.amp.stage = (Env::Stage) st;
            int fst = (int) v.flt.stage;
            envNext (v.flt.level, fst, fltC);
            v.flt.stage = (Env::Stage) fst;
            if (v.amp.stage == Env::Idle) { v.active = false; continue; }

            float xl = 0, xr = 0;
            for (size_t o = 0; o < 2; ++o)
            {
                const auto& c = s.osc[o];
                if (! c.on || c.level <= 0.0f) { v.lastOsc[o] = 0.0f; continue; }
                const float other = v.lastOsc[1 - o];
                float sum = 0.0f;
                for (int u = 0; u < c.unison; ++u)
                {
                    auto& ph = v.phase[o][(size_t) u];
                    const float x = warps[o].read (*tables[o], v.pos[o], ph, v.level[o], other);
                    ph += v.inc[o][(size_t) u];
                    if (ph >= 1.0) ph -= 1.0;
                    sum += x;
                    xl += x * v.panL[o][(size_t) u];
                    xr += x * v.panR[o][(size_t) u];
                }
                v.lastOsc[o] = sum / (float) c.unison;
            }
            if (s.sub > 0.0f)
            {
                const float sn = std::sin ((float) v.subPhase * juce::MathConstants<float>::twoPi) * s.sub;
                xl += sn; xr += sn;
            }
            v.subPhase += v.subInc;
            if (v.subPhase >= 1.0) v.subPhase -= 1.0;
            if (s.noise > 0.0f)
            {
                xl += (rng.nextFloat() * 2.0f - 1.0f) * s.noise * 0.5f;
                xr += (rng.nextFloat() * 2.0f - 1.0f) * s.noise * 0.5f;
            }

            float ch[2] = { fold (xl, v.foldAmt), fold (xr, v.foldAmt) };
            for (int c = 0; c < 2; ++c)
            {
                float x = ch[c];
                if (s.drive > 0.0f) x = std::tanh (x * driveGain) * driveNorm;
                float lp, bp, hp;
                v.s1[(size_t) c].tick (x, v.g, v.k1, lp, bp, hp);
                switch (s.filterType)
                {
                    case LP24:
                    {
                        float lp2, bp2, hp2;
                        v.s2[(size_t) c].tick (lp, v.g, v.k2, lp2, bp2, hp2);
                        x = lp2;
                        break;
                    }
                    case LP12:     x = lp; break;
                    case BandPass: x = bp * v.k1; break;
                    case HighPass: x = hp; break;
                    default:       x = lp + hp; break; // notch: everything but the band
                }
                ch[c] = x;
            }
            const float vel = 1.0f - s.velSens + s.velSens * v.velocity;
            outL += ch[0] * a * vel;
            outR += ch[1] * a * vel;
        }
        l[i] = outL * 0.5f;
        r[i] = outR * 0.5f;
    }
}

void Engine::runFx (juce::AudioBuffer<float>& buffer, int start, int n, const Settings& s, const skazi::HostTime& host)
{
    float* l = buffer.getWritePointer (0, start);
    float* r = buffer.getWritePointer (1, start);

    // chorus
    chorusMixSmooth.setTargetValue (s.chorusOn ? s.chorusMix : 0.0f);
    if (s.chorusOn || chorusMixSmooth.isSmoothing())
    {
        chorus.setRate (s.chorusRate);
        chorus.setDepth (s.chorusDepth);
        chorus.setMix (chorusMixSmooth.skip (n));
        // in slices no bigger than prepared: some hosts send larger blocks than they announce
        for (int done = 0; done < n; done += chorusMax)
        {
            float* chans[2] = { l + done, r + done };
            juce::dsp::AudioBlock<float> block (chans, 2, (size_t) juce::jmin (chorusMax, n - done));
            chorus.process (juce::dsp::ProcessContextReplacing<float> (block));
        }
    }

    // ping-pong delay, tempo synced
    delayMixSmooth.setTargetValue (s.delayOn ? s.delayMix : 0.0f);
    const int len = (int) dlyL.size();
    const float target = (float) juce::jlimit (16.0, (double) len - 4.0, delayBeats (s.delayTime) * 60.0 / host.bpm * sr);
    if (dlySamples <= 0.0f) dlySamples = target;
    const float toneC = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * (float) (800.0 * std::pow (2.0, s.delayTone * 4.3)) / (float) sr);
    for (int i = 0; i < n; ++i)
    {
        dlySamples += (target - dlySamples) * 0.0005f;
        float rp = (float) dlyWrite - dlySamples;
        if (rp < 0.0f) rp += (float) len;
        const int i0 = (int) rp, i1 = (i0 + 1) % len;
        const float fr = rp - (float) i0;
        const float dl = dlyL[(size_t) i0] + fr * (dlyL[(size_t) i1] - dlyL[(size_t) i0]);
        const float dr = dlyR[(size_t) i0] + fr * (dlyR[(size_t) i1] - dlyR[(size_t) i0]);
        dlyLpL += (dl - dlyLpL) * toneC;
        dlyLpR += (dr - dlyLpR) * toneC;
        const float in = s.delayOn ? (l[i] + r[i]) * 0.5f : 0.0f;
        dlyL[(size_t) dlyWrite] = in + std::tanh (dlyLpR * s.delayFb);
        dlyR[(size_t) dlyWrite] = std::tanh (dlyLpL * s.delayFb);
        dlyWrite = (dlyWrite + 1) % len;
        const float m = delayMixSmooth.getNextValue();
        l[i] += dl * m;
        r[i] += dr * m;
    }

    // reverb
    reverbMixSmooth.setTargetValue (s.reverbOn ? s.reverbMix : 0.0f);
    juce::Reverb::Parameters p;
    p.roomSize = 0.3f + s.reverbSize * 0.68f;
    p.damping = s.reverbDamp;
    p.width = 1.0f;
    p.dryLevel = 0.5f; // juce::Reverb doubles dry: 0.5 is unity
    p.wetLevel = reverbMixSmooth.skip (n) / 3.0f;
    reverb.setParameters (p);
    if (s.reverbOn || reverbMixSmooth.isSmoothing() || p.wetLevel > 0.0f)
        reverb.processStereo (l, r, n);

    // output
    outSmooth.setTargetValue (s.outGain);
    float peak = 0.0f;
    int w = scopeWrite.load (std::memory_order_relaxed);
    for (int i = 0; i < n; ++i)
    {
        const float gn = outSmooth.getNextValue();
        l[i] = skazi::safeClip (l[i] * gn);
        r[i] = skazi::safeClip (r[i] * gn);
        const float m = (l[i] + r[i]) * 0.5f;
        peak = juce::jmax (peak, std::abs (m));
        scope[(size_t) w].store (m, std::memory_order_relaxed);
        w = (w + 1) % kScope;
    }
    scopeWrite.store (w, std::memory_order_relaxed);
    uiLevel.store (juce::jmax (peak, uiLevel.load (std::memory_order_relaxed) * 0.9f), std::memory_order_relaxed);
}

void Engine::process (juce::AudioBuffer<float>& buffer, const juce::MidiBuffer& midi, const Settings& s, const skazi::HostTime& host)
{
    const int n = buffer.getNumSamples();
    if (voiceBuf.getNumSamples() < n) voiceBuf.setSize (2, n, false, false, true);
    float* l = voiceBuf.getWritePointer (0);
    float* r = voiceBuf.getWritePointer (1);

    // a mode change drops the held notes so nothing hangs
    if (s.mode != lastMode) { if (lastMode >= 0) allNotesOff(); lastMode = s.mode; }

    // a synced LFO follows the host's bar position while it plays
    if (s.lfoSync && host.playing)
    {
        const double beats = lfoBeats (s.lfoRate);
        const double ph = host.ppq / beats - std::floor (host.ppq / beats);
        if (std::abs (ph - lfoPhase) > 0.02 && std::abs (ph - lfoPhase) < 0.98) lfoPhase = ph;
    }

    int pos = 0;
    for (const auto meta : midi)
    {
        const int at = juce::jlimit (0, n, meta.samplePosition);
        if (at > pos) { renderVoices (l + pos, r + pos, at - pos, s, host); pos = at; }
        const auto m = meta.getMessage();
        if (m.isNoteOn()) noteOn (m.getNoteNumber(), m.getFloatVelocity(), s);
        else if (m.isNoteOff()) noteOff (m.getNoteNumber(), s);
        else if (m.isPitchWheel()) bend = (m.getPitchWheelValue() - 8192) / 8192.0f;
        else if (m.isControllerOfType (1)) modWheel = m.getControllerValue() / 127.0f;
        else if (m.isAllNotesOff() || m.isAllSoundOff()) allNotesOff();
    }
    if (pos < n) renderVoices (l + pos, r + pos, n - pos, s, host);

    buffer.copyFrom (0, 0, voiceBuf, 0, 0, n); // stereo out only (see isBusesLayoutSupported)
    buffer.copyFrom (1, 0, voiceBuf, 1, 0, n);
    for (int c = 2; c < buffer.getNumChannels(); ++c) buffer.clear (c, 0, n);

    runFx (buffer, 0, n, s, host);
    uiVoices.store ((int) std::count_if (voices.begin(), voices.end(), [] (const Voice& v) { return v.active; }), std::memory_order_relaxed);
    uiCutoff.store (s.cutoff, std::memory_order_relaxed);
    const Voice* newest = nullptr; // the display follows the last note played
    for (auto& v : voices)
        if (v.active && (newest == nullptr || v.age > newest->age)) newest = &v;
    for (int o = 0; o < 2; ++o)
        uiPos[o].store (newest != nullptr ? newest->pos[o] : s.osc[(size_t) o].pos, std::memory_order_relaxed);
}
} // namespace kube
