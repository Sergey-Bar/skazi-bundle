#include "ZhugEngine.h"
#include <cmath>
#include <algorithm>

namespace zhug
{
const std::vector<skazi::ParamDef>& paramDefs()
{
    using P = skazi::ParamDef;
    static const std::vector<P> defs = {
        { "mode", "Drive Type", P::Choice, 0, 7, 0, "", 1, 1, { "TUBE", "TAPE", "TRANSISTOR", "DIGITAL", "FOLD", "FUZZ", "BIT CRUSH", "RECTIFY" } },
        { "drive", "Drive", P::Float, 0, 36, 12, "dB", 0.1f },
        { "tone", "Tone", P::Float, -100, 100, 0, "%", 0.1f },
        { "bias", "Bias", P::Float, 0, 100, 20, "%", 0.1f },
        { "auto", "Auto Gain", P::Bool, 0, 1, 1 },
        { "mb", "Multiband", P::Bool, 0, 1, 0 },
        { "lowd", "Low Drive", P::Float, 0, 100, 40, "%", 0.1f },
        { "midd", "Mid Drive", P::Float, 0, 100, 70, "%", 0.1f },
        { "highd", "High Drive", P::Float, 0, 100, 50, "%", 0.1f },
        { "xlow", "Low / Mid Crossover", P::Float, 40, 600, 180, "Hz", 1, 0.5f },
        { "xhigh", "Mid / High Crossover", P::Float, 1000, 10000, 3200, "Hz", 1, 0.5f },
        { "width", "Width", P::Float, 0, 200, 100, "%", 0.1f },
        { "bassmono", "Bass Mono", P::Float, 20, 500, 120, "Hz", 1, 0.5f },
        { "haas", "Haas", P::Float, 0, 20, 0, "ms", 0.1f },
        { "mix", "Mix", P::Float, 0, 100, 100, "%", 0.1f },
        { "gain", "Output", P::Float, -24, 12, 0, "dB", 0.1f },
    };
    return defs;
}

Settings Settings::fromValues (const std::function<float (const char*)>& v)
{
    Settings s;
    s.mode = (int) v ("mode"); s.driveDb = v ("drive"); s.tone = v ("tone") * 0.01f; s.bias = v ("bias") * 0.01f;
    s.autoGain = v ("auto") > 0.5f; s.multiband = v ("mb") > 0.5f;
    s.lowDrive = v ("lowd") * 0.01f; s.midDrive = v ("midd") * 0.01f; s.highDrive = v ("highd") * 0.01f;
    s.xLow = v ("xlow"); s.xHigh = v ("xhigh");
    s.width = v ("width") * 0.01f; s.bassMono = v ("bassmono"); s.haasMs = v ("haas");
    s.mix = v ("mix") * 0.01f; s.gain = juce::Decibels::decibelsToGain (v ("gain"));
    return s;
}

float shape (int mode, float x, float bias)
{
    const float b = bias * 0.5f;
    switch (mode)
    {
        case Tube:       return std::tanh (x + b) - std::tanh (b);
        case Tape:       { constexpr float k = 2.0f / juce::MathConstants<float>::pi; return k * (std::atan (x + b) - std::atan (b)); }
        case Transistor: { auto f = [] (float z) { return z / std::pow (1.0f + std::pow (std::abs (z), 2.5f), 0.4f); }; return f (x + b) - f (b); }
        case Digital:    return juce::jlimit (-1.0f, 1.0f, x + b) - juce::jlimit (-1.0f, 1.0f, b);
        case Fold:       return std::sin (x + b) - std::sin (b);
        case Fuzz:       { auto f = [] (float z) { return std::copysign (1.0f - std::exp (-std::abs (z) * 2.0f), z); }; return f (x + b * 3.0f) - f (b * 3.0f); }
        case Rectify:    { const float z = x * (1.0f - bias) + std::abs (x) * bias; return std::tanh (z); }
        case BitCrush:
        default:         return x; // bit crush is handled in the engine (it reduces bits, not gain)
    }
}

float referenceMakeup (const Settings& s)
{
    // one cycle of a -18 dB RMS sine through the drive, DC removed, compared with what went in
    constexpr int N = 256;
    const float amp = 0.125f * juce::MathConstants<float>::sqrt2;
    const float drive = s.mode == BitCrush ? 1.0f : juce::Decibels::decibelsToGain (s.driveDb) * (s.multiband ? s.midDrive * 2.0f : 1.0f);
    const float driveNorm = s.driveDb / 36.0f, q = std::pow (2.0f, 16.0f - driveNorm * 14.0f - 1.0f);
    float y[N], mean = 0.0f;
    for (int i = 0; i < N; ++i)
    {
        const float x = amp * std::sin (juce::MathConstants<float>::twoPi * (float) i / (float) N);
        y[i] = s.mode == BitCrush ? std::round (x * q) / q : shape (s.mode, x * drive, s.bias);
        mean += y[i];
    }
    mean /= (float) N;
    float ms = 0.0f;
    for (float v : y) ms += (v - mean) * (v - mean);
    const float rms = std::sqrt (ms / (float) N);
    return rms > 1.0e-6f ? juce::jlimit (0.05f, 4.0f, 0.125f / rms) : 1.0f;
}

void Engine::prepare (double sampleRate, int maxBlock)
{
    sr = sampleRate;
    os = std::make_unique<juce::dsp::Oversampling<float>> (2, 2, juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple, true, true);
    os->initProcessing ((size_t) maxBlock);
    latencySamples = (int) std::round (os->getLatencyInSamples());

    juce::dsp::ProcessSpec spec { sr * 4.0, (juce::uint32) (maxBlock * 4), 2 };
    xoLow.prepare (spec);
    xoHigh.prepare (spec);
    dry.setSize (2, maxBlock);
    work.setSize (2, maxBlock);
    for (auto& d : dryDelay) d.assign ((size_t) juce::jmax (1, latencySamples), 0.0f); // reading before writing gives exactly this many samples of delay
    dryPos = 0;
    haas.assign ((size_t) (sr * 0.025) + 1, 0.0f);
    haasPos = 0;
    makeup = makeupTarget = 1.0f; driveSm = 1.0f;
    makeupKey[0] = -1.0f;
}

void Engine::process (juce::AudioBuffer<float>& buffer, const Settings& s)
{
    const int n = buffer.getNumSamples();
    const int numCh = juce::jmin (2, buffer.getNumChannels());
    if (numCh == 0 || os == nullptr) return;

    // keep the dry signal, delayed by the oversampling latency, for MIX
    dry.setSize (2, n, false, false, true);
    work.setSize (2, n, false, false, true);
    for (int c = 0; c < 2; ++c)
        dry.copyFrom (c, 0, buffer, juce::jmin (c, numCh - 1), 0, n);
    if (latencySamples > 0)
    {
        const int L = (int) dryDelay[0].size();
        int p = dryPos;
        for (int i = 0; i < n; ++i)
        {
            for (int c = 0; c < 2; ++c)
            {
                const float x = dry.getSample (c, i);
                dry.setSample (c, i, dryDelay[c][(size_t) p]);
                dryDelay[c][(size_t) p] = x;
            }
            p = (p + 1) % L;
        }
        dryPos = p;
    }

    // input level for auto gain and the meter
    for (int i = 0; i < n; ++i)
    {
        const float a = buffer.getSample (0, i), b = buffer.getSample (numCh - 1, i);
        inPk = juce::jmax (inPk, std::abs (a), std::abs (b));
    }

    // --- 4x oversampled drive ---
    for (int c = 0; c < 2; ++c) work.copyFrom (c, 0, buffer, juce::jmin (c, numCh - 1), 0, n);
    juce::dsp::AudioBlock<float> block (work);
    auto up = os->processSamplesUp (block);
    const int un = (int) up.getNumSamples();

    const float driveTarget = s.mode == BitCrush ? 1.0f : juce::Decibels::decibelsToGain (s.driveDb);
    const float driveStep = (driveTarget - driveSm) / (float) un;
    xoLow.setCutoffFrequency (s.xLow);
    xoHigh.setCutoffFrequency (juce::jmax (s.xHigh, s.xLow * 1.5f));
    const float bands[3] = { s.lowDrive * 2.0f, s.midDrive * 2.0f, s.highDrive * 2.0f };
    const float driveNorm = s.driveDb / 36.0f;
    const float bits = 16.0f - driveNorm * 14.0f, q = std::pow (2.0f, bits - 1.0f);
    const int holdN = 1 + (int) (driveNorm * 15.0f);
    float bandSum[3] {};

    for (int i = 0; i < un; ++i)
    {
        driveSm += driveStep;
        const bool hold = s.mode == BitCrush && (crushCount++ % holdN) != 0;
        for (int c = 0; c < 2; ++c)
        {
            float* d = up.getChannelPointer ((size_t) c);
            const float x = d[i];
            float y;
            if (s.mode == BitCrush)
            {
                if (! hold) crushHold[c] = std::round (x * q) / q;
                y = crushHold[c];
            }
            else if (s.multiband)
            {
                float low, rest, mid, high;
                xoLow.processSample (c, x, low, rest);
                xoHigh.processSample (c, rest, mid, high);
                const float yl = shape (s.mode, low * driveSm * bands[0], s.bias);
                const float ym = shape (s.mode, mid * driveSm * bands[1], s.bias);
                const float yh = shape (s.mode, high * driveSm * bands[2], s.bias);
                bandSum[0] += yl * yl; bandSum[1] += ym * ym; bandSum[2] += yh * yh;
                y = yl + ym + yh;
            }
            else
            {
                y = shape (s.mode, x * driveSm, s.bias);
                bandSum[1] += y * y;
            }
            drivenPk = juce::jmax (drivenPk, std::abs (x * driveSm));
            d[i] = y;
        }
    }
    os->processSamplesDown (block);
    for (int b = 0; b < 3; ++b)
    {
        bandMs[b] += (bandSum[b] / (2.0f * un) - bandMs[b]) * 0.3f;
        uiBand[(size_t) b].store (std::sqrt (bandMs[b]));
    }

    // --- AUTO GAIN: a fixed makeup for the current drive type, drive and bias, measured on a
    // reference tone (-18 dB RMS sine) through the same drive. It never follows the music, so a quiet
    // part after a loud one, a gap or a new hit can't make the level jump; knob moves ramp with the drive.
    {
        const float key[5] = { (float) s.mode, s.driveDb, s.bias, s.multiband ? s.midDrive : -1.0f, (float) s.autoGain };
        if (! std::equal (std::begin (key), std::end (key), std::begin (makeupKey)))
        {
            const bool first = makeupKey[0] < 0.0f;
            std::copy (std::begin (key), std::end (key), std::begin (makeupKey));
            makeupTarget = s.autoGain ? referenceMakeup (s) : 1.0f;
            if (first) makeup = makeupTarget; // just prepared: start at the right level, no glide
        }
    }
    const float makeupStep = (makeupTarget - makeup) / (float) n;   // moves with DRIVE, which ramps over this same block

    // --- tone, auto gain, width, bass mono, haas, mix ---
    const float tiltA = (float) (1.0 - std::exp (-juce::MathConstants<double>::twoPi * 800.0 / sr));
    const float gl = juce::Decibels::decibelsToGain (-s.tone * 6.0f), gh = juce::Decibels::decibelsToGain (s.tone * 6.0f);
    const float dcA = (float) std::exp (-juce::MathConstants<double>::twoPi * 10.0 / sr);
    const float sideG = (float) std::tan (juce::MathConstants<double>::pi * juce::jlimit (20.0f, 500.0f, s.bassMono) / sr);
    const bool monoBass = s.bassMono > 21.0f;
    const int haasN = (int) haas.size();
    const int haasD = juce::jlimit (0, haasN - 1, (int) (s.haasMs * 0.001 * sr));
    const float decay = (float) std::exp (-(double) n / (0.2 * sr));

    for (int i = 0; i < n; ++i)
    {
        float w[2];
        for (int c = 0; c < 2; ++c)
        {
            float y = work.getSample (c, i);
            dc[c] = y - dcX[c] + dcA * dc[c];             // remove DC from bias / rectify
            dcX[c] = y;
            y = dc[c];
            tiltLp[c] += (y - tiltLp[c]) * tiltA;         // tilt EQ around 800 Hz
            w[c] = tiltLp[c] * gl + (y - tiltLp[c]) * gh;
        }
        makeup += makeupStep;
        w[0] *= makeup; w[1] *= makeup;

        if (numCh > 1)
        {
            float mid = 0.5f * (w[0] + w[1]), side = 0.5f * (w[0] - w[1]) * s.width;
            if (monoBass)
            {
                // high-pass the side so everything below BASS MONO is centred
                const float k = 1.4142f, a1 = 1.0f / (1.0f + sideG * (sideG + k)), a2 = sideG * a1, a3 = sideG * a2;
                const float v3 = side - sideIc2, v1 = a1 * sideIc1 + a2 * v3, v2 = sideIc2 + a2 * sideIc1 + a3 * v3;
                sideIc1 = 2.0f * v1 - sideIc1; sideIc2 = 2.0f * v2 - sideIc2;
                side = side - k * v1 - v2;
            }
            w[0] = mid + side; w[1] = mid - side;
            if (haasD > 0)
            {
                haas[(size_t) haasPos] = w[1];
                w[1] = haas[(size_t) ((haasPos - haasD + haasN) % haasN)];
                haasPos = (haasPos + 1) % haasN;
            }
        }

        for (int c = 0; c < numCh; ++c)
        {
            const float o = skazi::safeClip ((dry.getSample (c, i) * (1.0f - s.mix) + w[c] * s.mix) * s.gain);
            buffer.setSample (c, i, o);
            outPk = juce::jmax (outPk, std::abs (o));
        }
        const int sw = scopeWrite.load();
        scope[(size_t) sw * 2].store (buffer.getSample (0, i));
        scope[(size_t) sw * 2 + 1].store (buffer.getSample (numCh - 1, i));
        scopeWrite.store ((sw + 1) % kScope);
    }

    uiIn.store (inPk); uiOut.store (outPk); uiDriven.store (drivenPk);
    inPk *= decay; outPk *= decay; drivenPk *= decay;
}
} // namespace zhug
