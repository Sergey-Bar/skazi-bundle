#pragma once

#include <juce_core/juce_core.h>
#include <cmath>

struct MorphWeights
{
    float lp, bp, hp;
};

// Morph 0..3 walks LP -> BP -> HP -> Notch (LP + HP) with no hard switches anywhere.
inline MorphWeights morphWeights (float morph) noexcept
{
    const float m = juce::jlimit (0.0f, 3.0f, morph);
    if (m < 1.0f)
        return { 1.0f - m, m, 0.0f };
    if (m < 2.0f)
        return { 0.0f, 2.0f - m, m - 1.0f };
    return { m - 2.0f, 0.0f, 1.0f };
}

// Damping (1/Q) of the resonant stage. Up to 90% the Q climbs from 0.707 to 25; past that the damping
// goes negative and the saturating feedback holds the filter in a stable self-oscillating sine.
inline float resonanceToDamping (float resonance) noexcept
{
    const float r = juce::jlimit (0.0f, 1.0f, resonance);
    if (r <= 0.9f)
        return 1.0f / (0.70710678f * std::pow (35.3553f, r / 0.9f));
    const float t = (r - 0.9f) / 0.1f;
    return 0.04f - 0.065f * t;
}

inline bool isSelfOscillating (float damping) noexcept { return damping < 0.02f; }

inline float fastTanh (float x) noexcept
{
    x = juce::jlimit (-3.0f, 3.0f, x);
    const float x2 = x * x;
    return x * (27.0f + x2) / (27.0f + 9.0f * x2);
}

// One 12 dB/oct TPT state-variable stage. Stable under per-sample cutoff modulation.
struct SvfStage
{
    float s1 = 0.0f, s2 = 0.0f;

    void reset() noexcept { s1 = s2 = 0.0f; }

    // With nonlinear set, the band integrator saturates like an analog OTA: the peak warms up
    // and rounds off instead of clipping, and self-oscillation settles at a fixed level.
    float process (float x, const MorphWeights& w, float g, float R2, float h, bool nonlinear) noexcept
    {
        const float hp = (x - s1 * (g + R2) - s2) * h;
        const float v1 = g * hp;
        const float bp = v1 + s1;
        s1 = bp + v1;
        const float v2 = g * bp;
        const float lp = v2 + s2;
        s2 = lp + v2;

        if (nonlinear)
        {
            // Saturation loss scales with g like the feedback gain does, so the self-oscillation
            // level (about -3 dBFS) holds at any cutoff and any oversampling rate.
            s1 += (3.0f * fastTanh (s1 * (1.0f / 3.0f)) - s1) * juce::jmin (1.0f, g * 4.0f);
            s2 = juce::jlimit (-8.0f, 8.0f, s2);
        }

        return w.lp * lp + w.bp * bp * juce::jmax (R2, 0.04f) + w.hp * hp;
    }
};

// Magnitude of the cascaded, morphed SVF (the TPT maps the analog prototype through a tan prewarp).
inline double svfMagnitude (const MorphWeights& weights, int numStages, double freq, double cutoff, double damping, double sampleRate)
{
    const auto pi = juce::MathConstants<double>::pi;
    const double w = std::tan (pi * juce::jmin (freq, sampleRate * 0.4995) / sampleRate)
                   / std::tan (pi * juce::jmin (cutoff, sampleRate * 0.4995) / sampleRate);
    double magnitude = 1.0;

    for (int s = 0; s < numStages; ++s)
    {
        const double R2 = s == numStages - 1 ? juce::jmax (damping, 0.02) : 1.41421356;
        const double re = 1.0 - w * w;
        const double im = w * R2;
        const double numRe = weights.lp - weights.hp * w * w;
        const double numIm = weights.bp * juce::jmax (R2, 0.04) * w;
        magnitude *= std::sqrt (numRe * numRe + numIm * numIm) / juce::jmax (1.0e-12, std::sqrt (re * re + im * im));
    }
    return magnitude;
}

// LFO shapes in -1..1. Sample & hold returns the value latched at each cycle start.
inline float lfoValue (int shape, float phase, float heldRandom) noexcept
{
    switch (shape)
    {
        case 0:  return std::sin (juce::MathConstants<float>::twoPi * phase);
        case 1:  return 1.0f - 4.0f * std::abs (phase - 0.5f);
        case 2:  return 2.0f * phase - 1.0f;
        case 3:  return phase < 0.5f ? 1.0f : -1.0f;
        default: return heldRandom;
    }
}

// Drive characters. Small signals pass at roughly unity loudness whatever the gain.
inline float saturate (int type, float x, float gain) noexcept
{
    const float driven = gain * x;
    const float makeup = 1.0f / std::sqrt (gain);
    switch (type)
    {
        case 0:  return (std::tanh (driven + 0.3f) - 0.29131261f) * makeup;   // tube: asymmetric, even harmonics
        case 1:  return std::atan (driven) * makeup;                          // tape: soft and round
        default: return juce::jlimit (-1.0f, 1.0f, driven) * makeup;         // hard: gritty clip
    }
}

// Lets resonant peaks through untouched up to 0 dBFS, then rounds them off below +3.5 dB.
inline float softLimit (float x) noexcept
{
    const float a = std::abs (x);
    if (a <= 1.0f)
        return x;
    return std::copysign (1.0f + 0.5f * std::tanh (2.0f * (a - 1.0f)), x);
}
