#pragma once

#include "PluginProcessor.h"
#include "Displays.h"
#include "Widgets.h"

class LowpassAudioProcessorEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit LowpassAudioProcessorEditor (LowpassAudioProcessor&);
    ~LowpassAudioProcessorEditor() override;

    void resized() override;

private:
    // Everything is laid out once at this size; the canvas is scaled to fit the window.
    static constexpr int designWidth = 1240, designHeight = 942;

    struct Canvas : public juce::Component
    {
        juce::Image background;
        void paint (juce::Graphics& g) override { g.drawImage (background, getLocalBounds().toFloat()); }
    };

    void timerCallback() override;
    void attach (NeonKnob&, const char* paramID);
    void attach (NeonKnob&, const char* paramID, std::atomic<float>& liveValue);
    void layout();
    void renderBackground();
    juce::Image renderLattice() const;

    LowpassAudioProcessor& proc;
    NeonLookAndFeel lookAndFeel;
    Canvas canvas;
    float renderedScale = 0.0f;
    juce::Image lattice;   // drawn once per window, only scaled on resize

    HeaderView header;
    FilterDisplay display;
    ScopeView scope;
    MeterView meters;
    LfoPreview lfoPreview;
    EnvHistory envHistory;
    SequencerGrid sequencer;
    PresetBar presetBar;

    NeonKnob cutoff       { "Cutoff", Theme::cyan };
    NeonKnob resonance    { "Resonance", Theme::cyan };
    NeonKnob morph        { "Morph", Theme::cyan };
    NeonKnob glide        { "Glide", Theme::cyan };
    NeonKnob drive        { "Drive", Theme::red };
    NeonKnob lfoRate      { "Rate", Theme::magenta };
    NeonKnob lfoDepth     { "Depth", Theme::magenta, true };
    NeonKnob lfoStereo    { "Stereo", Theme::magenta };
    NeonKnob envAmount    { "Amount", Theme::lime, true };
    NeonKnob envAttack    { "Attack", Theme::lime };
    NeonKnob envRelease   { "Release", Theme::lime };
    NeonKnob seqSteps     { "Steps", Theme::amber };
    NeonKnob seqDepth     { "Depth", Theme::amber, true };
    NeonKnob seqGlide     { "Glide", Theme::amber };
    NeonKnob macro        { "Macro", Theme::magenta };
    NeonKnob macroCutoff  { "Cut", Theme::magenta, true };
    NeonKnob macroRes     { "Res", Theme::magenta, true };
    NeonKnob macroDrive   { "Drv", Theme::magenta, true };
    NeonKnob macroMix     { "Mix", Theme::magenta, true };
    NeonKnob macroMorph   { "Mrf", Theme::magenta, true };
    NeonKnob inGain       { "Input", Theme::cyan, true };
    NeonKnob outGain      { "Output", Theme::cyan, true };
    NeonKnob mix          { "Mix", Theme::cyan };
    NeonKnob spread       { "Spread", Theme::cyan };
    NeonKnob stereoOffset { "R/S Offs", Theme::cyan, true };

    SegmentedSelector morphSelector, slopeSelector, oversamplingSelector, shapeSelector,
                      driveTypeSelector, drivePosSelector, stereoModeSelector;
    NeonToggle syncToggle { "SYNC", Theme::magenta };
    NeonToggle seqToggle { "RUN", Theme::amber };
    juce::ComboBox divisionBox, seqRateBox;

    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> sliderAttachments;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> syncAttachment, seqAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> divisionAttachment, seqRateAttachment;

    struct Panel
    {
        juce::Rectangle<int> bounds;
        juce::String index, title;
        juce::Colour accent;
    };
    std::vector<Panel> panels;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LowpassAudioProcessorEditor)
};
