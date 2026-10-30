#pragma once

#include <juce_dsp/juce_dsp.h>
#include "PluginProcessor.h"
#include "Theme.h"
#include "Widgets.h"

// Big analyzer: live input/output spectrum, the filter's response curve and a draggable cutoff/resonance node.
class FilterDisplay : public juce::Component
{
public:
    explicit FilterDisplay (LowpassAudioProcessor&);

    void tick();
    void paint (juce::Graphics&) override;
    void resized() override;

    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

private:
    static constexpr int fftOrder = 12;
    static constexpr int fftSize = 1 << fftOrder;

    float xForFreq (float freq) const;
    float freqForX (float x) const;
    float yForDb (float db) const;
    float yForSpectrumDb (float db) const;
    float yForResonance (float res) const;
    float resonanceForY (float y) const;

    void analyse (const SampleRing&, std::vector<float>& smoothed, double sampleRate);
    juce::Path responsePath (const MorphWeights&, int stages, float cutoff, float damping, double sampleRate) const;
    juce::Path spectrumPath (const std::vector<float>&) const;
    void setFromMouse (juce::Point<float>);
    void drawGrid (juce::Graphics&) const;

    LowpassAudioProcessor& proc;
    juce::RangedAudioParameter& cutoffParam;
    juce::RangedAudioParameter& resonanceParam;

    juce::dsp::FFT fft { fftOrder };
    juce::dsp::WindowingFunction<float> window { (size_t) fftSize, juce::dsp::WindowingFunction<float>::hann, false };
    std::vector<float> fftData = std::vector<float> ((size_t) fftSize * 2, 0.0f);
    std::vector<float> spectrumIn, spectrumOut;

    struct Particle
    {
        juce::Point<float> pos, vel;
        float life;
        juce::Colour colour;
    };
    std::vector<Particle> particles;
    juce::Random random;

    juce::Rectangle<float> plot;
    float time = 0.0f, scan = 0.0f;
    bool dragging = false;
};

// Triggered oscilloscope with auto-zoom.
class ScopeView : public juce::Component
{
public:
    explicit ScopeView (LowpassAudioProcessor&);
    void tick();
    void paint (juce::Graphics&) override;

private:
    static constexpr int history = 4096, window = 1024;
    LowpassAudioProcessor& proc;
    std::vector<float> outBuffer = std::vector<float> (history), inBuffer = std::vector<float> (history);
    std::vector<float> outTrace = std::vector<float> (window), inTrace = std::vector<float> (window);
    float zoom = 1.0f;
};

// IN / OUT / ENV / LFO bars.
class MeterView : public juce::Component
{
public:
    explicit MeterView (LowpassAudioProcessor&);
    void tick();
    void paint (juce::Graphics&) override;

private:
    LowpassAudioProcessor& proc;
    float inLevel = 0.0f, outLevel = 0.0f, inHold = 0.0f, outHold = 0.0f;
    int inHoldFrames = 0, outHoldFrames = 0;
};

// SKAZI logo, the SKILTER title with a glitching chromatic split, and the Asher Swissa credit.
class HeaderView : public juce::Component
{
public:
    explicit HeaderView (LowpassAudioProcessor&);
    void tick();
    void paint (juce::Graphics&) override;

private:
    LowpassAudioProcessor& proc;
    juce::Image logo;
    juce::Random random;
    float time = 0.0f;
    int glitchFrames = 0;
    float glitchOffset = 0.0f;
};

// One LFO cycle with the live playhead dot.
class LfoPreview : public juce::Component
{
public:
    explicit LfoPreview (LowpassAudioProcessor&);
    void tick();
    void paint (juce::Graphics&) override;

private:
    LowpassAudioProcessor& proc;
};

// 16-step sequencer lane: drag to draw step values, double-click a step to zero it.
class SequencerGrid : public juce::Component
{
public:
    explicit SequencerGrid (LowpassAudioProcessor&);
    void tick();
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

private:
    juce::Rectangle<float> lane() const;
    int stepAt (float x) const;
    void drawTo (juce::Point<float>);

    LowpassAudioProcessor& proc;
    std::array<juce::RangedAudioParameter*, ParamIDs::numSteps> steps {};
    std::array<bool, ParamIDs::numSteps> touched {};
    int lastStep = -1;
    float pulse = 0.0f;
};

// Scrolling history of the envelope follower.
class EnvHistory : public juce::Component
{
public:
    explicit EnvHistory (LowpassAudioProcessor&);
    void tick();
    void paint (juce::Graphics&) override;

private:
    LowpassAudioProcessor& proc;
    std::vector<float> values = std::vector<float> (180, 0.0f);
    size_t head = 0;
};

// Preset browser: previous / name (click for the categorised list) / next, plus the RANDOM die.
class PresetBar : public juce::Component
{
public:
    explicit PresetBar (LowpassAudioProcessor&);
    void tick();
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    void step (int delta);
    void showMenu();

    LowpassAudioProcessor& proc;
    NeonButton previous { "<", Theme::cyan }, next { ">", Theme::cyan }, random { "RANDOM", Theme::magenta, true };
    juce::Rectangle<int> nameArea;
    juce::String shownName;
    float hover = 0.0f;
};
