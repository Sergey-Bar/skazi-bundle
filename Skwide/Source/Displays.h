#pragma once

#include "PluginProcessor.h"
#include "Theme.h"

// Logo, title and credit strip.
class HeaderView : public juce::Component
{
public:
    HeaderView();
    void tick();
    void paint (juce::Graphics&) override;

private:
    juce::Image logo;
    juce::Random random;
    float glitchOffset = 0.0f;
    int glitchFrames = 0;
};

// Frequency imager: mid and side spectra of the output, the input side as a ghost, a per-band
// width strip and the draggable LOW BYPASS crossover.
class ImagerView : public juce::Component
{
public:
    explicit ImagerView (SkwideAudioProcessor&);
    void tick();
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;

private:
    static constexpr int fftOrder = 12, fftSize = 1 << fftOrder, numBins = fftSize / 2;
    static constexpr int numBands = 28;

    float xForFreq (float freq) const;
    float freqForX (float x) const;
    float yForDb (float db) const;
    juce::Rectangle<float> plotArea() const;
    juce::Rectangle<float> stripArea() const;
    void analyse (const SampleRing& l, const SampleRing& r, std::vector<float>& mid, std::vector<float>& side);

    SkwideAudioProcessor& proc;
    juce::dsp::FFT fft { fftOrder };
    juce::dsp::WindowingFunction<float> window { (size_t) fftSize, juce::dsp::WindowingFunction<float>::hann };
    std::vector<float> bufL, bufR, fftMid, fftSide;
    std::vector<float> outMid, outSide, inMid, inSide;   // smoothed power per bin
    std::array<float, numBands> bandWidth {}, bandWidthIn {};
    juce::ParameterAttachment lowFreqAttachment;
    float lowFreq = 120.0f;
    bool lowOn = true, monoBass = false, dragging = false, hoverLine = false;
    int frame = 0;
};

// Goniometer (Lissajous) or polar-sample view of the output with persistence, plus the
// correlation meter and the mono-compatibility readout. Click the scope to switch modes.
class ScopeView : public juce::Component
{
public:
    explicit ScopeView (SkwideAudioProcessor&);
    void tick();
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    juce::Rectangle<float> scopeArea() const;

    SkwideAudioProcessor& proc;
    juce::Image trail;
    std::vector<float> l, r, il, ir;
    bool polarMode = false;
    float correlation = 1.0f, balance = 0.0f, monoDelta = 0.0f, level = 0.0f;
    bool hasSignal = false;
};

// Big dial that shows the decorrelator's phase rotation live; drag it to set PHASE.
class PhaseWheel : public juce::Component
{
public:
    explicit PhaseWheel (SkwideAudioProcessor&);
    void tick();
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    void setFromMouse (juce::Point<float>);

    SkwideAudioProcessor& proc;
    juce::ParameterAttachment attachment;
    float value = 0.0f;
    float live = 0.0f;
    std::array<float, 48> history {};
    int historyPos = 0;
};

// Perspective room showing where DEPTH places the source.
class DepthView : public juce::Component
{
public:
    explicit DepthView (SkwideAudioProcessor&);
    void tick();
    void paint (juce::Graphics&) override;

private:
    SkwideAudioProcessor& proc;
    float shown = 0.0f, width = 1.0f, time = 0.0f;
};

// SUPER WIDE timeline: where each side's Haas delay lands, which side leads, and the mono mode.
class HaasView : public juce::Component
{
public:
    explicit HaasView (SkwideAudioProcessor&);
    void tick();
    void paint (juce::Graphics&) override;

private:
    float xForMs (juce::Rectangle<float> lane, float ms) const;

    SkwideAudioProcessor& proc;
    float left = 0.0f, right = 0.0f, amount = 0.0f, time = 0.0f;
    bool on = false, safe = true;
};

// Effective mono-spread (SPREAD plus the smart part of WIDTH) as a glowing bar.
class SpreadMeter : public juce::Component
{
public:
    explicit SpreadMeter (SkwideAudioProcessor&);
    void tick();
    void paint (juce::Graphics&) override;

private:
    SkwideAudioProcessor& proc;
    float shown = 0.0f;
};
