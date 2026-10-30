#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "WidenerDSP.h"

namespace ParamIDs
{
    inline constexpr auto width      = "WIDTH";
    inline constexpr auto spread     = "SPREAD";
    inline constexpr auto depth      = "DEPTH";
    inline constexpr auto phase      = "PHASE";
    inline constexpr auto driftRate  = "DRIFT_RATE";
    inline constexpr auto driftDepth = "DRIFT_DEPTH";
    inline constexpr auto lowOn      = "LOW_ON";
    inline constexpr auto lowFreq    = "LOW_FREQ";
    inline constexpr auto monoBass   = "MONO_BASS";
    inline constexpr auto mix        = "MIX";
    inline constexpr auto output     = "OUTPUT";
    inline constexpr auto swap       = "SWAP";
    inline constexpr auto monoCheck  = "MONO_CHECK";
    inline constexpr auto haasOn     = "HAAS_ON";
    inline constexpr auto haasLeft   = "HAAS_L";
    inline constexpr auto haasRight  = "HAAS_R";
    inline constexpr auto haasAmount = "HAAS_AMOUNT";
    inline constexpr auto haasSafe   = "HAAS_SAFE";
}

// Single-writer ring: the audio thread pushes, the editor copies the latest samples for the imager and scope.
struct SampleRing
{
    static constexpr int size = 1 << 14;
    static constexpr int mask = size - 1;

    void push (float v) noexcept
    {
        data[(size_t) writePos] = v;
        writePos = (writePos + 1) & mask;
    }

    void publish() noexcept { published.store (writePos, std::memory_order_release); }

    void copyLatest (float* dest, int num) const noexcept
    {
        const int start = (published.load (std::memory_order_acquire) - num) & mask;
        for (int i = 0; i < num; ++i)
            dest[i] = data[(size_t) ((start + i) & mask)];
    }

    std::array<float, size> data {};
    int writePos = 0;
    std::atomic<int> published { 0 };
};

class SkwideAudioProcessor : public juce::AudioProcessor
{
public:
    SkwideAudioProcessor();

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.05; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentPreset; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    // Rolls every control except MIX, OUTPUT and MONO CHECK.
    void randomise();
    std::atomic<bool> randomised { false };

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

    // What the editor draws from. "in" is the dry input, "out" the final output.
    SampleRing inL, inR, outL, outR;
    std::atomic<float> livePhase { 0.0f };   // decorrelator rotation in degrees, after drift
    std::atomic<float> liveSpread { 0.0f };  // effective mono-spread amount 0..1 (SPREAD + smart WIDTH)
    std::atomic<double> liveSampleRate { 44100.0 };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    std::atomic<float>* raw (const char* id) { return apvts.getRawParameterValue (id); }
    void updateDepthFilters (float depth);
    float splitTarget() const;
    float haasTarget() const;

    std::atomic<float>* widthParam = nullptr;
    std::atomic<float>* spreadParam = nullptr;
    std::atomic<float>* depthParam = nullptr;
    std::atomic<float>* phaseParam = nullptr;
    std::atomic<float>* driftRateParam = nullptr;
    std::atomic<float>* driftDepthParam = nullptr;
    std::atomic<float>* lowOnParam = nullptr;
    std::atomic<float>* lowFreqParam = nullptr;
    std::atomic<float>* monoBassParam = nullptr;
    std::atomic<float>* mixParam = nullptr;
    std::atomic<float>* outputParam = nullptr;
    std::atomic<float>* swapParam = nullptr;
    std::atomic<float>* monoCheckParam = nullptr;
    std::atomic<float>* haasOnParam = nullptr;
    std::atomic<float>* haasLeftParam = nullptr;
    std::atomic<float>* haasRightParam = nullptr;
    std::atomic<float>* haasAmountParam = nullptr;
    std::atomic<float>* haasSafeParam = nullptr;

    juce::SmoothedValue<float> width, spread, depth, phase, driftDepth, lowAmount, splitAmount, bassKeep,
                               mix, outGain, sideSign, monoAmount, lowFreq,
                               haasAmount, haasSafe, haasLeftSamples, haasRightSamples;

    double sampleRate = 44100.0;
    double driftPhase = 0.0;
    float appliedDepth = 1000.0f, appliedLowFreq = -1.0f;

    wide::Crossover sideSplit, midSplit;
    wide::Decorrelator decorrelator;
    wide::HilbertPair rotator;
    wide::Diffuser diffuser;
    wide::Biquad depthHigh, depthPresence, depthLow;
    wide::DelayLine haasDelay[2];
    wide::HighPass4 haasHighPass[2];
    float depthLevel = 1.0f, depthSideFocus = 1.0f, depthDiffuse = 0.0f;

    int currentPreset = 0;
    juce::Random random;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SkwideAudioProcessor)
};
