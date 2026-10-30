#pragma once

#include <juce_dsp/juce_dsp.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <atomic>
#include "../../Common/SkaziCore.h"
#include "Wavetables.h"

namespace kube
{
enum FilterType { LP24, LP12, BandPass, HighPass, Notch };
enum LfoShape { Sine, Triangle, Saw, Square, SampleHold };
enum VoiceMode { Poly, Mono, Legato };
enum WarpMode { WarpOff, WarpSync, WarpBendPlus, WarpBendMinus, WarpPwm, WarpAsymPlus, WarpAsymMinus, WarpMirror, WarpFlip,
                WarpQuantize, WarpFm, WarpAm, WarpRm };

const std::vector<skazi::ParamDef>& paramDefs();
const juce::StringArray& warpNames();
double lfoBeats (int index);
double delayBeats (int index);

struct Adsr { float a = 0.002f, d = 0.3f, s = 0.8f, r = 0.15f; }; // seconds, sustain 0..1

struct OscSettings
{
    struct Warp { int mode = WarpOff; float amount = 0; };
    bool on = true; int table = 0; float pos = 0; int oct = 0, semi = 0; float fine = 0, level = 0.8f;
    int unison = 1; float detune = 0.2f, width = 0.5f, blend = 0.75f, rand = 1.0f;
    std::array<Warp, 2> warp;
};

// An oscillator's two warp slots, worked out once per block and applied to every sample it reads
struct WarpState
{
    struct Slot { int mode = WarpOff; float amount = 0, a = 0; };
    std::array<Slot, 2> slot;
    int levelAdd = 0; // warps that squeeze the cycle raise the pitch inside it: use a duller mip level

    void set (const OscSettings& o)
    {
        double slope = 1.0;
        for (size_t i = 0; i < 2; ++i)
        {
            auto& s = slot[i];
            s.mode = o.warp[i].mode;
            s.amount = juce::jlimit (0.0f, 1.0f, o.warp[i].amount);
            const float m = s.amount;
            switch (s.mode)
            {
                case WarpSync:      s.a = 1.0f + m * 15.0f; slope *= s.a; break;
                case WarpBendPlus:
                case WarpBendMinus: slope *= 1.0 + 2.0 * m; break;
                case WarpPwm:       s.a = 1.0f - m * 0.95f; slope /= s.a; break;
                case WarpAsymPlus:  s.a = 0.5f + m * 0.45f; slope *= 0.5 / (1.0 - s.a); break;
                case WarpAsymMinus: s.a = 0.5f - m * 0.45f; slope *= 0.5 / s.a; break;
                case WarpMirror:    slope *= 1.0 + m; break;
                case WarpQuantize:  s.a = 2.0f + (1.0f - m) * (1.0f - m) * 62.0f; break;
                case WarpFm:        slope *= 1.0 + m * 2.0; break;
                default: break;
            }
        }
        levelAdd = (int) std::ceil (std::log2 (slope) - 0.01);
    }

    static double bendHalf (int mode, double u, float m) // u 0..1 across half a cycle
    {
        const double v = mode == WarpBendPlus ? u * u * u : 1.0 - (1.0 - u) * (1.0 - u) * (1.0 - u);
        return u + m * (v - u);
    }

    // One sample of the warped table; other = the other oscillator's latest output (for FM, AM and RM)
    float read (const Wavetable& t, float pos, double phase, int level, float other) const
    {
        double p = phase;
        for (auto& s : slot)
        {
            if (s.amount <= 0.0f) continue;
            switch (s.mode)
            {
                case WarpSync:      p *= s.a; p -= std::floor (p); break;
                case WarpBendPlus:
                case WarpBendMinus: p = p < 0.5 ? 0.5 * bendHalf (s.mode, 2.0 * p, s.amount) : 1.0 - 0.5 * bendHalf (s.mode, 2.0 - 2.0 * p, s.amount); break;
                case WarpPwm:       if (p >= s.a) return 0.0f; p /= s.a; break;
                case WarpAsymPlus:
                case WarpAsymMinus: p = p < s.a ? 0.5 * p / s.a : 0.5 + 0.5 * (p - s.a) / (1.0 - s.a); break;
                case WarpMirror:    p += s.amount * ((p < 0.5 ? 2.0 * p : 2.0 - 2.0 * p) - p); break;
                case WarpFm:        p += s.amount * 0.5 * other; p -= std::floor (p); break;
                default: break;
            }
        }
        float x = t.read (pos, p, level);
        for (auto& s : slot)
        {
            if (s.amount <= 0.0f) continue;
            switch (s.mode)
            {
                case WarpFlip:     if (phase > 1.0 - s.amount) x = -x; break;
                case WarpQuantize: x = std::round (x * s.a) / s.a; break;
                case WarpAm:       x *= 1.0f - s.amount + s.amount * (0.5f + 0.5f * other); break;
                case WarpRm:       x += s.amount * (x * other * 1.6f - x); break;
                default: break;
            }
        }
        return x;
    }
};

struct Settings
{
    std::array<OscSettings, 2> osc;
    float sub = 0; int subOct = 0; float noise = 0, fold = 0;
    int filterType = LP24; float cutoff = 8000, reso = 0.2f, filterEnv = 0, keyTrack = 0.5f, drive = 0;
    Adsr amp, flt;
    float envPos = 0, pitchAmt = 0, pitchDec = 0.05f;
    bool lfoOn = true; int lfoShape = Sine; bool lfoSync = true; int lfoRate = 3; float lfoHz = 2, lfoCut = 0, lfoPos = 0, lfoFold = 0;
    int mode = Poly; float glide = 0, velSens = 0.3f;
    bool chorusOn = false; float chorusRate = 0.4f, chorusDepth = 0.4f, chorusMix = 0.4f;
    bool delayOn = false; int delayTime = 2; float delayFb = 0.35f, delayTone = 0.5f, delayMix = 0.25f;
    bool reverbOn = false; float reverbSize = 0.6f, reverbDamp = 0.5f, reverbMix = 0.2f;
    float outGain = 0.5f;
    static Settings fromValues (const std::function<float (const char*)>& v);
};

// Magnitude of the filter at freq (for the display)
float filterMagnitude (int type, float cutoff, float reso, float freq);

class Engine
{
public:
    static constexpr int kVoices = 8, kMaxUnison = 16, kScope = 1024;

    void prepare (double sampleRate, int maxBlock);
    void reset();
    void process (juce::AudioBuffer<float>& buffer, const juce::MidiBuffer& midi, const Settings& s, const skazi::HostTime& host);
    void allNotesOff();

    // Read by the editor
    std::atomic<float> uiPos[2] { 0.0f, 0.0f }, uiCutoff { 8000 }, uiLfo { 0 }, uiLevel { 0 };
    std::atomic<int> uiVoices { 0 };
    std::array<std::atomic<float>, kScope> scope {};
    std::atomic<int> scopeWrite { 0 };

    // Each oscillator's loaded .wav table (owned by the processor, which keeps replaced ones alive)
    std::array<std::atomic<const Wavetable*>, 2> userTables { nullptr, nullptr };

    // The table an oscillator plays: its USER table when that's chosen and loaded, else a bank table
    const Wavetable& tableFor (const OscSettings& o, int osc) const
    {
        if (o.table >= kUserTable)
            if (auto* u = userTables[(size_t) osc].load (std::memory_order_acquire)) return *u;
        return bankTable (juce::jmin (o.table, kBankSize - 1));
    }

private:
    struct Env
    {
        enum Stage { Idle, Attack, Decay, Sustain, Release } stage = Idle;
        float level = 0;
        void gate (bool on) { stage = on ? Attack : (stage == Idle ? Idle : Release); }
    };

    struct Svf
    {
        float ic1 = 0, ic2 = 0;
        void reset() { ic1 = ic2 = 0; }
        // returns { lp, bp, hp }
        void tick (float x, float g, float k, float& lp, float& bp, float& hp)
        {
            const float a1 = 1.0f / (1.0f + g * (g + k)), a2 = g * a1, a3 = g * a2;
            const float v3 = x - ic2, v1 = a1 * ic1 + a2 * v3, v2 = ic2 + a2 * ic1 + a3 * v3;
            ic1 = 2.0f * v1 - ic1; ic2 = 2.0f * v2 - ic2;
            lp = v2; bp = v1; hp = x - k * v1 - v2;
        }
    };

    struct Voice
    {
        bool active = false;
        int note = -1; float velocity = 1; juce::uint32 age = 0;
        double pitch = 60, targetPitch = 60; // semitones, glides to target
        std::array<std::array<double, kMaxUnison>, 2> phase {};
        double subPhase = 0;
        Env amp, flt;
        float pitchEnv = 0;
        std::array<Svf, 2> s1, s2; // two stages per channel
        // control-rate state
        float g = 0.1f, k1 = 1.4f, k2 = 1.4f;
        std::array<std::array<float, kMaxUnison>, 2> inc {}, panL {}, panR {};
        std::array<int, 2> level { 0, 0 };
        float pos[2] { 0, 0 }, foldAmt = 0;
        float subInc = 0;
        float lastOsc[2] { 0, 0 }; // each oscillator's previous sample, for FM/AM/RM from the other
    };

    void noteOn (int note, float velocity, const Settings& s);
    void noteOff (int note, const Settings& s);
    void startVoice (Voice& v, int note, float velocity, bool retrigger, const Settings& s);
    void updateControl (Voice& v, const Settings& s, float lfo, const std::array<WarpState, 2>& warps);
    void renderVoices (float* l, float* r, int n, const Settings& s, const skazi::HostTime& host);
    void runFx (juce::AudioBuffer<float>& buffer, int start, int n, const Settings& s, const skazi::HostTime& host);
    float lfoValue (int shape);

    double sr = 44100.0;
    std::array<Voice, kVoices> voices;
    juce::uint32 ageCounter = 0;
    std::vector<int> held; // mono note stack, last = newest
    float bend = 0, modWheel = 0;
    double lfoPhase = 0; float sh = 0; bool lfoWrapped = false;
    juce::Random rng { 0x4b554245 }; // fixed seed: renders repeat exactly, phases still differ note to note
    int controlCount = 0, lastMode = -1, chorusMax = 512;

    juce::AudioBuffer<float> voiceBuf;
    juce::dsp::Chorus<float> chorus;
    juce::Reverb reverb;
    std::vector<float> dlyL, dlyR; int dlyWrite = 0; float dlySamples = 0, dlyLpL = 0, dlyLpR = 0;
    juce::SmoothedValue<float> outSmooth, chorusMixSmooth, delayMixSmooth, reverbMixSmooth;
};
} // namespace kube
