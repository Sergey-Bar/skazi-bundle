#pragma once
#include <juce_core/juce_core.h>
#include <array>
#include <cmath>
#include <vector>
#include "Parameters.h"

// Multi-mode filter: classic SVF / ladder types plus "crazy" ones (combs, formant, phaser, crush, ring, fold...).
// Everything runs per sample so the cutoff can be modulated at audio rate without zipper noise.
class FilterEngine
{
public:
    static constexpr int maxChannels = 2;

    void prepare (double sampleRate)
    {
        sr = (float) sampleRate;
        envFall = std::exp (-1.0f / (0.05f * sr));
        combSize = (int) (sampleRate / 15.0) + 4;
        for (auto& c : chans)
            for (auto& line : c.comb)
                line.assign ((size_t) combSize, 0.0f);
        reset();
    }

    void reset()
    {
        for (auto& c : chans)
        {
            for (auto& s : c.svf) s = {};
            for (auto& line : c.comb) std::fill (line.begin(), line.end(), 0.0f);
            c.combWrite = 0;
            c.ap.fill (0.0f);
            c.ladder.fill (0.0f);
            c.ladderOut = c.feedback = c.hold = c.holdCount = c.ringPhase = c.inEnv = 0.0f;
        }
    }

    void setType (int newType)
    {
        if (newType != type)
        {
            type = newType;
            reset();
        }
    }

    float process (int ch, float x, float cutoff, float res, float drive) noexcept
    {
        auto& c = chans[(size_t) ch];
        const float fc = juce::jlimit (20.0f, sr * 0.45f, cutoff);
        const float g  = std::tan (pi * fc / sr);
        const float k  = 2.0f - 1.96f * res;
        const float in = drive > 0.001f ? std::tanh (x * (1.0f + drive * 9.0f)) / (1.0f + drive * 1.5f) : x;
        c.inEnv = juce::jmax (std::abs (x), c.inEnv * envFall);

        switch (type)
        {
            case ft::LP12: return c.svf[0].process (in, g, k).lp;
            case ft::LP24: return c.svf[1].process (c.svf[0].process (in, g, 1.414f).lp, g, k).lp;
            case ft::HP12: return c.svf[0].process (in, g, k).hp;
            case ft::HP24: return c.svf[1].process (c.svf[0].process (in, g, 1.414f).hp, g, k).hp;
            case ft::BP12: return c.svf[0].process (in, g, k).bp * k;
            case ft::BP24: return c.svf[1].process (c.svf[0].process (in, g, k).bp * k, g, k).bp * k;

            case ft::Notch:
            {
                const auto o = c.svf[0].process (in, g, k);
                return o.lp + o.hp;
            }
            case ft::Notch24:
            {
                const auto o1 = c.svf[0].process (in, g, k);
                const auto o2 = c.svf[1].process (o1.lp + o1.hp, g, k);
                return o2.lp + o2.hp;
            }
            case ft::Peak:
            {
                constexpr float kp = 0.7f;
                return in + c.svf[0].process (in, g, kp).bp * kp * res * 4.0f;
            }

            case ft::LadderLP: return ladder (c, in, g, tameFeedback (c, res * 4.0f, g)) * (1.0f + res * 1.5f);
            case ft::Squelch:  return ladder (c, in * 2.5f, g, tameFeedback (c, 1.0f + res * 3.6f, g)) * (0.5f + res * 0.6f);

            case ft::CombPlus:  return comb (c, in, fc, 0.3f + res * 0.68f);
            case ft::CombMinus: return comb (c, in, fc, -(0.3f + res * 0.68f));

            case ft::Metallic:
            {
                static constexpr float ratios[] = { 1.0f, 1.4142f, 1.7321f };
                const float fb = 0.5f + res * 0.45f;
                float sum = 0.0f;
                for (int l = 0; l < 3; ++l)
                {
                    const float d = juce::jlimit (1.0f, (float) combSize - 2.0f, sr / (fc * ratios[l]));
                    const float y = in + fb * readComb (c, l, d);
                    c.comb[(size_t) l][(size_t) c.combWrite] = std::tanh (y);
                    sum += y;
                }
                advanceComb (c);
                return sum * 0.33f * (1.0f - fb * 0.6f);
            }

            case ft::Formant:
            {
                // Cutoff knob morphs through the vowels A-E-I-O-U.
                static constexpr float vowels[5][3] = { { 800, 1150, 2900 }, { 350, 2000, 2800 }, { 270, 2140, 2950 },
                                                        { 450, 800, 2830 },  { 325, 700, 2700 } };
                static constexpr float gains[3] = { 1.0f, 0.6f, 0.35f };
                const float morph = juce::jlimit (0.0f, 3.999f, std::log2 (fc / 20.0f) / std::log2 (1000.0f) * 4.0f);
                const int v = (int) morph;
                const float t = morph - (float) v;
                const float kf = 2.0f - res * 1.85f;
                float sum = 0.0f;
                for (int f = 0; f < 3; ++f)
                {
                    const float freq = vowels[v][f] + t * (vowels[v + 1][f] - vowels[v][f]);
                    const float gf = std::tan (pi * juce::jmin (freq, sr * 0.45f) / sr);
                    sum += c.svf[(size_t) f].process (in, gf, kf).bp * kf * gains[f];
                }
                return sum * 1.6f;
            }

            case ft::Phaser:
            {
                const float a = (g - 1.0f) / (g + 1.0f);
                float v = in + c.feedback * res * 0.9f;
                for (auto& z : c.ap)
                {
                    const float y = a * v + z;
                    z = v - a * y;
                    v = y;
                }
                c.feedback = std::tanh (v);
                return 0.5f * (in + v);
            }

            case ft::BitCrush:
            {
                // Cutoff sets the sample-rate reduction, resonance sets the bit depth.
                const float holdLen = juce::jmax (1.0f, sr / (fc * 2.0f));
                c.holdCount += 1.0f;
                if (c.holdCount >= holdLen)
                {
                    c.holdCount -= holdLen;
                    const float q = std::exp2 (15.0f - res * 13.0f);
                    c.hold = std::round (in * q) / q;
                }
                return c.hold;
            }

            case ft::RingMod:
            {
                c.ringPhase += fc / sr;
                if (c.ringPhase >= 1.0f) c.ringPhase -= 1.0f;
                const float y = in * std::sin (twoPi * c.ringPhase);
                const float b = c.svf[0].process (y, g, k).bp * k;
                return y + (b - y) * res;
            }

            case ft::WaveFold:
            {
                const float folded = std::sin (x * (1.0f + res * 8.0f + drive * 8.0f)) * 0.8f;
                return c.svf[0].process (folded, g, 1.0f).lp;
            }

            case ft::Scream:
            {
                const float b = c.svf[0].process (in + c.feedback * res * 0.4f, g, k).bp;
                c.feedback = std::tanh (b * 0.5f * (1.0f + drive * 6.0f));
                return c.feedback * 0.7f;
            }

            case ft::Telephone:
            {
                const float gh = std::tan (pi * juce::jmax (20.0f, fc * 0.4f) / sr);
                const float gl = std::tan (pi * juce::jmin (sr * 0.45f, fc * 2.5f) / sr);
                const float h = c.svf[0].process (in, gh, 1.2f).hp;
                const float l = c.svf[1].process (h, gl, k).lp;
                return std::tanh (l * 3.0f) * 0.45f;
            }

            default: return in;
        }
    }

private:
    static constexpr float pi    = juce::MathConstants<float>::pi;
    static constexpr float twoPi = juce::MathConstants<float>::twoPi;

    // Topology-preserving state variable filter (Zavalishin / Cytomic).
    struct Svf
    {
        float ic1 = 0.0f, ic2 = 0.0f;
        struct Out { float lp, bp, hp; };

        Out process (float v0, float g, float k) noexcept
        {
            const float a1 = 1.0f / (1.0f + g * (g + k));
            const float a2 = g * a1, a3 = g * a2;
            const float v3 = v0 - ic2;
            const float v1 = a1 * ic1 + a2 * v3;
            const float v2 = ic2 + a2 * ic1 + a3 * v3;
            ic1 = 2.0f * v1 - ic1;
            ic2 = 2.0f * v2 - ic2;
            return { v2, v1, v0 - k * v1 - v2 };
        }
    };

    struct Chan
    {
        std::array<Svf, 3> svf;
        std::array<std::vector<float>, 3> comb;
        int combWrite = 0;
        std::array<float, 6> ap {};
        std::array<float, 4> ladder {};
        float ladderOut = 0.0f, feedback = 0.0f, hold = 0.0f, holdCount = 0.0f, ringPhase = 0.0f;
        float inEnv = 0.0f; // input level follower: lets the ladder resonate past its limit only while sound comes in
    };

    // Past a feedback limit the ladder sings on its own and keeps whistling after the music stops.
    // The limit falls as the cutoff rises (the one-sample feedback delay): fitted from the filter's
    // linear stability, within 0.006. Extra feedback over it only stays while sound comes in, so a
    // screaming acid filter still screams on the track but rings out in under a second after it.
    static float tameFeedback (const Chan& c, float fb, float g) noexcept
    {
        const float G = g / (1.0f + g);
        const float limit = 0.96f * ((((2.7513f * G - 7.8764f) * G + 9.9545f) * G - 7.7828f) * G + 3.9944f);
        if (fb <= limit) return fb;
        return limit + (fb - limit) * juce::jmin (1.0f, c.inEnv * 30.0f);
    }

    // 4-pole ladder with saturated feedback (one-sample feedback delay, good enough for a creative filter).
    static float ladder (Chan& c, float x, float g, float fb) noexcept
    {
        const float G = g / (1.0f + g);
        float u = std::tanh (x - fb * c.ladderOut);
        for (auto& s : c.ladder)
        {
            const float v = (u - s) * G;
            const float y = v + s;
            s = y + v;
            u = y;
        }
        c.ladderOut = u;
        return u;
    }

    float readComb (const Chan& c, int line, float delay) const noexcept
    {
        float rp = (float) c.combWrite - delay;
        if (rp < 0.0f) rp += (float) combSize;
        const int i0 = (int) rp;
        const int i1 = (i0 + 1) % combSize;
        const float f = rp - (float) i0;
        const auto& b = c.comb[(size_t) line];
        return b[(size_t) i0] + f * (b[(size_t) i1] - b[(size_t) i0]);
    }

    void advanceComb (Chan& c) const noexcept
    {
        if (++c.combWrite >= combSize) c.combWrite = 0;
    }

    float comb (Chan& c, float x, float fc, float fb) noexcept
    {
        const float d = juce::jlimit (1.0f, (float) combSize - 2.0f, sr / fc);
        const float y = x + fb * readComb (c, 0, d);
        c.comb[0][(size_t) c.combWrite] = std::tanh (y); // tanh keeps runaway resonance in check
        advanceComb (c);
        return y * (1.0f - std::abs (fb) * 0.7f);
    }

    std::array<Chan, maxChannels> chans;
    float sr = 44100.0f, envFall = 0.9995f;
    int combSize = 4;
    int type = ft::LP24;
};
