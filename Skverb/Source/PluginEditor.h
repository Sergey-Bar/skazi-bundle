#pragma once

#include "PluginProcessor.h"
#include "Displays.h"
#include "Widgets.h"

class SkverbAudioProcessorEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit SkverbAudioProcessorEditor (SkverbAudioProcessor&);
    ~SkverbAudioProcessorEditor() override;

    void resized() override;

private:
    // Everything is laid out once at this size; the canvas is scaled to fit the window.
    static constexpr int designWidth = 1200, designHeight = 1066;

    struct Canvas : public juce::Component
    {
        juce::Image background;
        void paint (juce::Graphics& g) override { g.drawImage (background, getLocalBounds().toFloat()); }
    };

    void timerCallback() override;
    void attach (Knob&, const char* paramID, const juce::String& scaleMin = {}, const juce::String& scaleMax = {});
    void attach (LedToggle&, const char* paramID);
    void layout();
    void renderBackground();
    void stepPreset (int delta);

    SkverbAudioProcessor& proc;
    PanelLookAndFeel lookAndFeel;
    Canvas canvas;
    float renderedScale = 0.0f;

    HeaderView header;
    SpaceView space;
    MeterView meters;
    LedSelector modeSelector, filterSelector;

    Knob decay        { "Decay" };
    Knob preDelay     { "Pre-Delay" };
    Knob feedback     { "Feedback" };
    Knob size         { "Size" };
    Knob lowCut       { "Low Cut" };
    Knob highCut      { "High Cut" };
    Knob damping      { "Damping" };
    Knob bassMult     { "Bass Decay" };
    Knob diffusion    { "Diffusion" };
    Knob early        { "Early" };
    Knob width        { "Width" };
    Knob modRate      { "Rate" };
    Knob modDepth     { "Depth" };
    Knob shimmerPitch { "Pitch", true };
    Knob shimmerAmt   { "Amount" };
    Knob duck         { "Duck" };
    Knob duckRelease  { "Release" };
    Knob drive        { "Drive" };
    Knob mix          { "Mix" };
    Knob output       { "Output" };
    Knob revAmount    { "Amount" };
    Knob revTime      { "Time" };
    Knob fltCutoff    { "Cutoff" };
    Knob fltReso      { "Reso" };
    Knob fltEnv       { "Env", true };

    LedToggle freeze  { "FREEZE", Theme::ledIce };
    LedToggle sync    { "SYNC", Theme::amber };
    LedToggle killDry { "KILL DRY", Theme::ledRed };

    juce::ComboBox presetBox;
    PanelButton prevPreset { "<" }, nextPreset { ">" };
    PanelButton randomButton { "RANDOM" };

    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> sliderAttachments;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>> buttonAttachments;

    // Engraved section headings and the grooves between sections, drawn into the faceplate.
    struct Section
    {
        juce::Rectangle<int> bounds;
        juce::String title;
    };
    std::vector<Section> sections;
    juce::Rectangle<int> displayBounds;
    const juce::Rectangle<int> meterWindow { 476, 912, 666, 112 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SkverbAudioProcessorEditor)
};
