#pragma once

#include <juce_dsp/juce_dsp.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <atomic>
#include "../../Common/SkaziCore.h"

namespace zhug
{
enum Mode { Tube, Tape, Transistor, Digital, Fold, Fuzz, BitCrush, Rectify, NumModes };

const std::vector<skazi::ParamDef>& paramDefs();

struct Settings
{
    int mode = Tube; float driveDb = 12, tone = 0, bias = 0.2f; bool autoGain = true;
    bool multiband = false; float lowDrive = 0.4f, midDrive = 0.7f, highDrive = 0.5f, xLow = 180, xHigh = 3200;
    float width = 1.0f, bassMono = 120, haasMs = 0, mix = 1.0f, gain = 1.0f;
    static Settings fromValues (const std::function<float (const char*)>& v);
};

// The waveshaper on its own (x already includes the drive gain); used by the engine and the curve display
float shape (int mode, float x, float bias);

class Engine
{
public:
    void prepare (double sampleRate, int maxBlock);
    int latency() const { return latencySamples; }
    void process (juce::AudioBuffer<float>& buffer, const Settings& s);

    // Read by the editor
    std::atomic<float> uiIn { 0 }, uiOut { 0 }, uiDriven { 0 };
    std::array<std::atomic<float>, 3> uiBand {};
    static constexpr int kScope = 512;
    std::array<std::atomic<float>, kScope * 2> scope {};
    std::atomic<int> scopeWrite { 0 };

private:
    double sr = 44100.0;
    std::unique_ptr<juce::dsp::Oversampling<float>> os;
    int latencySamples = 0;
    juce::dsp::LinkwitzRileyFilter<float> xoLow, xoHigh;
    juce::AudioBuffer<float> dry, work;
    std::vector<float> dryDelay[2]; int dryPos = 0;
    std::vector<float> haas; int haasPos = 0;
    float makeup = 1.0f, makeupTarget = 1.0f;
    float makeupKey[5] { -1, -1, -1, -1, -1 };   // settings the current AUTO GAIN target was measured for
    float tiltLp[2] {}, dc[2] {}, dcX[2] {}, sideIc1 = 0, sideIc2 = 0;
    float crushHold[2] {}; int crushCount = 0;
    float driveSm = 1.0f;
    float bandMs[3] {}, inPk = 0, outPk = 0, drivenPk = 0;
};
} // namespace zhug
