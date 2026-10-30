#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "ReverbDSP.h"

namespace ParamIDs
{
    inline constexpr auto mode         = "MODE";
    inline constexpr auto decay        = "DECAY";
    inline constexpr auto preDelay     = "PREDELAY";
    inline constexpr auto feedback     = "FEEDBACK";
    inline constexpr auto size         = "SIZE";
    inline constexpr auto lowCut       = "LOWCUT";
    inline constexpr auto highCut      = "HIGHCUT";
    inline constexpr auto damping      = "DAMPING";
    inline constexpr auto bassMult     = "BASSMULT";
    inline constexpr auto diffusion    = "DIFFUSION";
    inline constexpr auto early        = "EARLY";
    inline constexpr auto width        = "WIDTH";
    inline constexpr auto modRate      = "MODRATE";
    inline constexpr auto modDepth     = "MODDEPTH";
    inline constexpr auto shimmerPitch = "SHPITCH";
    inline constexpr auto shimmerAmt   = "SHAMOUNT";
    inline constexpr auto duck         = "DUCK";
    inline constexpr auto duckRelease  = "DUCKREL";
    inline constexpr auto drive        = "DRIVE";
    inline constexpr auto mix          = "MIX";
    inline constexpr auto output       = "OUTPUT";
    inline constexpr auto freeze       = "FREEZE";
    inline constexpr auto sync         = "SYNC";
    inline constexpr auto killDry      = "KILLDRY";
    inline constexpr auto revAmount    = "REVAMT";
    inline constexpr auto revTime      = "REVTIME";
    inline constexpr auto fltType      = "FLTTYPE";
    inline constexpr auto fltCutoff    = "FLTCUT";
    inline constexpr auto fltReso      = "FLTRES";
    inline constexpr auto fltEnv       = "FLTENV";
}

namespace TailFilter
{
    inline const juce::StringArray names { "OFF", "LP", "BP", "HP" };
}

namespace Sync
{
    inline const juce::StringArray names { "1/64", "1/32", "1/16T", "1/16", "1/8T", "1/16D", "1/8", "1/4T", "1/8D", "1/4", "1/2T", "1/4D", "1/2", "1/1" };
    inline constexpr float beats[] { 1.0f / 16, 1.0f / 8, 1.0f / 6, 1.0f / 4, 1.0f / 3, 3.0f / 8, 1.0f / 2, 2.0f / 3, 3.0f / 4, 1.0f, 4.0f / 3, 3.0f / 2, 2.0f, 4.0f };
    inline constexpr int count = (int) std::size (beats);
}

class SkverbAudioProcessor : public juce::AudioProcessor
{
public:
    SkverbAudioProcessor();

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 30.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentPreset; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    // Rolls every control except MIX, OUTPUT, FREEZE and KILL DRY.
    void randomise();
    std::atomic<bool> randomised { false };

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

    // Meters for the editor: peaks since the editor last took them, and the live duck gain.
    std::atomic<float> inPeak { 0.0f }, wetPeak { 0.0f }, outPeak { 0.0f };
    std::atomic<float> duckGain { 1.0f }, gateLevel { 1.0f };
    std::atomic<float> livePreDelayMs { 0.0f };
    std::atomic<int> liveSyncIndex { -1 };   // -1 when SYNC is off

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    float get (const char* id) const { return apvts.getRawParameterValue (id)->load(); }
    void set (const char* id, float value);

    verb::Engine engine;
    juce::AudioBuffer<float> wet;
    juce::SmoothedValue<float> dryGain, wetGain, outGain;
    float duckEnv = 0.0f, duckSmooth = 1.0f;
    double sampleRate = 44100.0;

    int currentPreset = 0;
    juce::Random random;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SkverbAudioProcessor)
};
