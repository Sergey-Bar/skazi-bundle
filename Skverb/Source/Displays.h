#pragma once

#include "PluginProcessor.h"
#include "Theme.h"

// Logo, model name and subtitle, silkscreened on the faceplate.
class HeaderView : public juce::Component
{
public:
    HeaderView();
    void paint (juce::Graphics&) override;

private:
    juce::Image logo;
};

// The amber display: the reverb's shape drawn from the controls (pre-delay, early-reflection
// spikes, the decay envelope with feedback echoes, reverse swell, gate or freeze), plus a live
// strip of the input and wet levels underneath.
class SpaceView : public juce::Component
{
public:
    explicit SpaceView (SkverbAudioProcessor&);
    void tick (float inLevel, float wetLevel);
    void paint (juce::Graphics&) override;

private:
    float value (const char* id) const { return proc.apvts.getRawParameterValue (id)->load(); }

    SkverbAudioProcessor& proc;
    static constexpr int historySize = 240;
    std::array<float, historySize> inHistory {}, wetHistory {};
    int historyPos = 0;
    float animPhase = 0.0f;
    juce::Random random;
    juce::Image pixelGrid;   // cached LCD matrix + vignette overlay
};

// LED ladders for IN / WET / OUT and the duck gain reduction.
class MeterView : public juce::Component
{
public:
    explicit MeterView (SkverbAudioProcessor&);
    void tick (float inLevel, float wetLevel, float outLevel);
    void paint (juce::Graphics&) override;

private:
    SkverbAudioProcessor& proc;
    float in = 0.0f, wet = 0.0f, out = 0.0f, duck = 1.0f;
    float inHold = 0.0f, wetHold = 0.0f, outHold = 0.0f;
};
