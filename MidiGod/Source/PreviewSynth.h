#pragma once

// A small built-in synth so the pattern can be heard before it goes to a real instrument.
// The sound follows the role: rolling bass, squelchy acid, plucks, wide pads, or a basic drum kit.
#include "Pattern.h"
#include <array>

namespace midigod
{
class PreviewSynth
{
public:
    void prepare (double sampleRate) { sr = sampleRate; allOff(); }
    void setSound (int role, bool drums) { sound = drums ? Drums : role; }

    void noteOn (int note, int velocity)
    {
        // legato: a bass or acid note that starts while another is held slides the held voice to it
        if (sound == Bass || sound == Acid)
            for (auto& c : voices)
                if (c.active && ! c.released && c.drum < 0)
                {
                    c.note = note;
                    c.target = 440.0 * std::exp2 ((note - 69) / 12.0);
                    c.vel = juce::jmax (c.vel, velocity / 127.0f);
                    return;
                }

        Voice* v = nullptr;
        for (auto& c : voices) if (! c.active) { v = &c; break; }
        if (v == nullptr) // steal the oldest
        {
            v = &voices[0];
            for (auto& c : voices) if (c.age < v->age) v = &c;
        }
        *v = Voice();
        v->active = true;
        v->note = note;
        v->vel = velocity / 127.0f;
        v->age = ++counter;
        v->kind = sound;
        v->freq = v->target = 440.0 * std::exp2 ((note - 69) / 12.0);
        v->drum = sound == Drums ? drumKind (note) : -1;
    }

    void noteOff (int note)
    {
        for (auto& v : voices)
            if (v.active && v.note == note && ! v.released) v.released = true;
    }

    void allOff() { for (auto& v : voices) v.active = false; bendFactor = 1.0; }

    // pitch wheel, in semitones (the pitch dips of ROLL BASS)
    void setBend (float semis) { bendFactor = std::exp2 (semis / 12.0); }

    void render (float* left, float* right, int numSamples, float gain)
    {
        for (auto& v : voices)
        {
            if (! v.active) continue;
            for (int i = 0; i < numSamples && v.active; ++i)
            {
                const float s = (v.drum >= 0 ? drum (v) : tone (v)) * gain;
                left[i] += s;
                right[i] += s;
            }
        }
    }

private:
    struct Voice
    {
        bool active = false, released = false;
        int note = 60, kind = Bass, drum = -1;
        juce::uint32 age = 0;
        float vel = 1.0f, env = 0.0f, fenv = 1.0f, ic1 = 0.0f, ic2 = 0.0f, hp = 0.0f;
        double freq = 440.0, target = 440.0, ph1 = 0.0, ph2 = 0.3, t = 0.0;
        bool decaying = false;
    };

    struct Shape { float a, d, s, r, cutoff, envAmt, fdecay, res, detune, level; };
    static const Shape& shape (int role)
    {
        static const Shape table[NumRoles] = {
            { 0.002f, 0.20f, 0.60f, 0.04f,  260.0f,  7.0f, 0.10f, 0.30f, 0.00f, 0.50f }, // bass
            { 0.002f, 0.30f, 0.50f, 0.03f,  220.0f, 12.0f, 0.14f, 0.85f, 0.00f, 0.40f }, // acid
            { 0.005f, 0.40f, 0.70f, 0.15f, 2400.0f,  2.0f, 0.30f, 0.20f, 0.12f, 0.22f }, // lead
            { 0.001f, 0.25f, 0.00f, 0.10f, 1100.0f,  6.0f, 0.12f, 0.30f, 0.06f, 0.28f }, // pluck
            { 0.001f, 0.18f, 0.00f, 0.08f, 1300.0f,  5.0f, 0.10f, 0.30f, 0.05f, 0.26f }, // arp
            { 0.004f, 0.50f, 0.60f, 0.15f, 2000.0f,  2.0f, 0.25f, 0.15f, 0.10f, 0.14f }, // chords
            { 0.35f,  1.00f, 0.80f, 0.70f, 1400.0f,  0.5f, 1.00f, 0.10f, 0.15f, 0.12f }, // pad
            { 0.010f, 0.40f, 0.70f, 0.20f, 1800.0f,  1.5f, 0.30f, 0.20f, 0.10f, 0.18f }, // counter
            { 0.001f, 0.20f, 0.00f, 0.05f, 8000.0f,  0.0f, 0.20f, 0.10f, 0.00f, 0.50f }, // drums (unused)
        };
        return table[juce::jlimit (0, NumRoles - 1, role)];
    }

    static int drumKind (int note)
    {
        if (note <= 36) return 0;                                       // kick
        if (note == 38 || note == 39 || note == 40) return 1;           // snare, clap
        if (note == 42 || note == 44) return 2;                         // closed hat
        if (note == 46 || note == 51 || note == 49) return 3;           // open hat, ride
        return 4;                                                       // perc
    }

    static float polyBlep (double t, double dt)
    {
        if (t < dt) { t /= dt; return (float) (t + t - t * t - 1.0); }
        if (t > 1.0 - dt) { t = (t - 1.0) / dt; return (float) (t * t + t + t + 1.0); }
        return 0.0f;
    }

    float noise() { seed = seed * 1664525u + 1013904223u; return (float) (int) seed * (1.0f / 2147483648.0f); }

    float tone (Voice& v)
    {
        const auto& p = shape (v.kind);
        const float dt = 1.0f / (float) sr;
        // ADSR
        if (v.released) v.env -= v.env * dt / juce::jmax (0.005f, p.r) * 4.0f;
        else if (! v.decaying) { v.env += dt / juce::jmax (0.001f, p.a); if (v.env >= 1.0f) { v.env = 1.0f; v.decaying = true; } }
        else v.env += (p.s - v.env) * dt / juce::jmax (0.01f, p.d) * 3.0f;
        if (v.released && v.env < 1.0e-4f) { v.active = false; return 0.0f; }
        v.fenv -= v.fenv * dt / p.fdecay * 3.0f;

        v.freq += (v.target - v.freq) * (1.0 - std::exp (-1.0 / (0.015 * sr))); // fast 303-style slide
        const double f = v.freq * bendFactor;
        const double inc1 = f / sr, inc2 = f * std::exp2 (p.detune / 12.0) / sr;
        float osc = (float) (2.0 * v.ph1 - 1.0) - polyBlep (v.ph1, inc1);
        if (p.detune > 0.0f) osc = 0.5f * (osc + (float) (2.0 * v.ph2 - 1.0) - polyBlep (v.ph2, inc2));
        v.ph1 += inc1; if (v.ph1 >= 1.0) v.ph1 -= 1.0;
        v.ph2 += inc2; if (v.ph2 >= 1.0) v.ph2 -= 1.0;

        // TPT state-variable low-pass, the envelope opens it
        const float fc = juce::jlimit (30.0f, (float) sr * 0.45f, p.cutoff * (1.0f + p.envAmt * v.fenv * (0.5f + 0.5f * v.vel)));
        const float g = std::tan (juce::MathConstants<float>::pi * fc / (float) sr), k = 2.0f - 1.9f * p.res;
        const float a1 = 1.0f / (1.0f + g * (g + k)), a2 = g * a1, a3 = g * a2;
        const float v3 = osc - v.ic2, v1 = a1 * v.ic1 + a2 * v3, v2 = v.ic2 + a2 * v.ic1 + a3 * v3;
        v.ic1 = 2.0f * v1 - v.ic1;
        v.ic2 = 2.0f * v2 - v.ic2;
        return v2 * v.env * v.vel * p.level;
    }

    float drum (Voice& v)
    {
        const float t = (float) v.t;
        v.t += 1.0 / sr;
        float out = 0.0f, end = 0.3f;
        switch (v.drum)
        {
            case 0: // kick: fast pitch drop into a short sine tail
            {
                const double f = 48.0 + 900.0 * std::exp (-t / 0.004) + 90.0 * std::exp (-t / 0.03);
                v.ph1 += f / sr;
                out = (float) std::sin (juce::MathConstants<double>::twoPi * v.ph1) * std::exp (-t / 0.11f) * 0.9f;
                end = 0.35f;
                break;
            }
            case 1: // snare / clap: tone + noise
            {
                v.ph1 += 190.0 / sr;
                out = 0.35f * (float) std::sin (juce::MathConstants<double>::twoPi * v.ph1) * std::exp (-t / 0.04f)
                    + 0.5f * noise() * std::exp (-t / 0.07f);
                end = 0.3f;
                break;
            }
            case 2: case 3: // hats: high-passed noise
            {
                const float n = noise();
                const float hpOut = n - v.hp;
                v.hp += 0.6f * hpOut;
                out = hpOut * 0.4f * std::exp (-t / (v.drum == 2 ? 0.025f : 0.18f));
                end = v.drum == 2 ? 0.12f : 0.6f;
                break;
            }
            default:
            {
                v.ph1 += 900.0 / sr;
                out = 0.4f * (float) std::sin (juce::MathConstants<double>::twoPi * v.ph1) * std::exp (-t / 0.03f);
                end = 0.15f;
                break;
            }
        }
        if (t > end) v.active = false;
        return out * v.vel;
    }

    std::array<Voice, 24> voices;
    double sr = 44100.0, bendFactor = 1.0;
    int sound = Bass;
    juce::uint32 counter = 0, seed = 12345;
};
} // namespace midigod
