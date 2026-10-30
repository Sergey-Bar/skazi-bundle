#pragma once

#include "PluginProcessor.h"
#include "Displays.h"
#include "Widgets.h"

class SkwideAudioProcessorEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit SkwideAudioProcessorEditor (SkwideAudioProcessor&);
    ~SkwideAudioProcessorEditor() override;

    void resized() override;

private:
    // Everything is laid out once at this size; the canvas is scaled to fit the window.
    static constexpr int designWidth = 1200, designHeight = 952;

    struct Canvas : public juce::Component
    {
        juce::Image background;
        void paint (juce::Graphics& g) override { g.drawImage (background, getLocalBounds().toFloat()); }
    };

    void timerCallback() override;
    void attach (NeonKnob&, const char* paramID);
    void attach (NeonToggle&, const char* paramID);
    void layout();
    void renderBackground();
    void stepPreset (int delta);

    SkwideAudioProcessor& proc;
    NeonLookAndFeel lookAndFeel;
    Canvas canvas;
    float renderedScale = 0.0f;

    HeaderView header;
    ImagerView imager;
    ScopeView scope;
    PhaseWheel phaseWheel;
    DepthView depthView;
    SpreadMeter spreadMeter;
    HaasView haasView;

    NeonKnob width      { "Width", Theme::cyan };
    NeonKnob spread     { "Mono Spread", Theme::magenta };
    NeonKnob depth      { "Depth", Theme::lime, true };
    NeonKnob phase      { "Phase", Theme::cyan };
    NeonKnob driftRate  { "Drift Rate", Theme::magenta };
    NeonKnob driftDepth { "Drift Depth", Theme::magenta };
    NeonKnob lowFreq    { "Low Freq", Theme::amber };
    NeonKnob mix        { "Mix", Theme::cyan };
    NeonKnob output     { "Output", Theme::cyan, true };

    NeonToggle lowOn     { "LOW BYPASS", Theme::amber };
    NeonToggle monoBass  { "MONO BASS", Theme::amber };
    NeonToggle swap      { "SWAP L/R", Theme::cyan };
    NeonToggle monoCheck { "MONO CHECK", Theme::red };
    NeonToggle haasOn    { "SUPER WIDE", Theme::magenta };
    NeonToggle haasSafe  { "MONO SAFE", Theme::lime };

    NeonKnob haasLeft   { "Delay L", Theme::cyan };
    NeonKnob haasRight  { "Delay R", Theme::magenta };
    NeonKnob haasAmount { "Amount", Theme::magenta };

    juce::ComboBox presetBox;
    NeonButton prevPreset { "<", Theme::cyan }, nextPreset { ">", Theme::cyan };
    NeonButton randomButton { "RANDOM", Theme::magenta, true };

    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> sliderAttachments;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>> buttonAttachments;

    struct Panel
    {
        juce::Rectangle<int> bounds;
        juce::String index, title;
        juce::Colour accent;
    };
    std::vector<Panel> panels;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SkwideAudioProcessorEditor)
};
