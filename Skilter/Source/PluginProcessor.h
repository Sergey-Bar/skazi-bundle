#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "FilterDSP.h"

namespace ParamIDs
{
    inline constexpr auto morph        = "MORPH";
    inline constexpr auto slope        = "SLOPE";
    inline constexpr auto cutoff       = "CUTOFF";
    inline constexpr auto resonance    = "RESONANCE";
    inline constexpr auto glide        = "GLIDE";
    inline constexpr auto drive        = "DRIVE";
    inline constexpr auto driveType    = "DRIVE_TYPE";
    inline constexpr auto drivePos     = "DRIVE_POS";
    inline constexpr auto lfoRate      = "LFO_RATE";
    inline constexpr auto lfoDepth     = "LFO_DEPTH";
    inline constexpr auto lfoShape     = "LFO_SHAPE";
    inline constexpr auto lfoSync      = "LFO_SYNC";
    inline constexpr auto lfoDiv       = "LFO_DIV";
    inline constexpr auto lfoStereo    = "LFO_STEREO";
    inline constexpr auto envAmount    = "ENV_AMOUNT";
    inline constexpr auto envAttack    = "ENV_ATTACK";
    inline constexpr auto envRelease   = "ENV_RELEASE";
    inline constexpr auto seqOn        = "SEQ_ON";
    inline constexpr auto seqRate      = "SEQ_RATE";
    inline constexpr auto seqSteps     = "SEQ_STEPS";
    inline constexpr auto seqDepth     = "SEQ_DEPTH";
    inline constexpr auto seqGlide     = "SEQ_GLIDE";
    inline constexpr auto macro        = "MACRO";
    inline constexpr auto macroCutoff  = "MACRO_CUTOFF";
    inline constexpr auto macroRes     = "MACRO_RES";
    inline constexpr auto macroDrive   = "MACRO_DRIVE";
    inline constexpr auto macroMix     = "MACRO_MIX";
    inline constexpr auto macroMorph   = "MACRO_MORPH";
    inline constexpr auto inGain       = "IN_GAIN";
    inline constexpr auto outGain      = "OUT_GAIN";
    inline constexpr auto mix          = "MIX";
    inline constexpr auto spread       = "SPREAD";
    inline constexpr auto stereoMode   = "STEREO_MODE";
    inline constexpr auto stereoOffset = "STEREO_OFFSET";
    inline constexpr auto oversampling = "OVERSAMPLING";

    inline constexpr int numSteps = 16;
    inline juce::String seqStep (int index) { return "SEQ_STEP_" + juce::String (index + 1); }
}

// Single-writer ring: the audio thread pushes, the editor copies the latest samples for the analyzer and scope.
struct SampleRing
{
    static constexpr int size = 1 << 15;
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

class LowpassAudioProcessor : public juce::AudioProcessor
{
public:
    LowpassAudioProcessor();

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.1; }

    // The 150 factory presets double as the host's program list.
    int getNumPrograms() override;
    int getCurrentProgram() override { return currentPreset; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void loadPreset (int index);
    void randomise();
    juce::String getPresetName() const { return presetName; }

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static const juce::StringArray slopeNames, shapeNames, divisionNames, driveTypeNames, drivePosNames, stereoModeNames, oversamplingNames;
    static juce::String morphName (float morph);

    juce::AudioProcessorValueTreeState apvts;

    // Live values the editor animates from (everything after modulation and the macro).
    SampleRing inputRing, outputRing;
    std::atomic<float> liveCutoff[2];
    std::atomic<float> liveEnvelope { 0.0f }, liveLfo { 0.0f }, liveLfoPhase { 0.0f };
    std::atomic<float> liveMorph { 0.0f }, liveResonance { 0.0f }, liveDamping { 1.414f }, liveDrive { 0.0f }, liveMix { 1.0f };
    std::atomic<float> liveSeqValue { 0.0f };
    std::atomic<int> liveSeqStep { 0 };
    std::atomic<float> inputPeak { 0.0f }, outputPeak { 0.0f };
    std::atomic<double> liveSampleRate { 44100.0 };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    static double divisionBeats (int index);

    void configureOversampling (int index);
    void runEngine (juce::dsp::AudioBlock<float> block, int stereoMode);
    std::atomic<float>* raw (const char* id) { return apvts.getRawParameterValue (id); }

    std::atomic<float>* morphParam = nullptr;
    std::atomic<float>* slopeParam = nullptr;
    std::atomic<float>* cutoffParam = nullptr;
    std::atomic<float>* resonanceParam = nullptr;
    std::atomic<float>* glideParam = nullptr;
    std::atomic<float>* driveParam = nullptr;
    std::atomic<float>* driveTypeParam = nullptr;
    std::atomic<float>* drivePosParam = nullptr;
    std::atomic<float>* lfoRateParam = nullptr;
    std::atomic<float>* lfoDepthParam = nullptr;
    std::atomic<float>* lfoShapeParam = nullptr;
    std::atomic<float>* lfoSyncParam = nullptr;
    std::atomic<float>* lfoDivParam = nullptr;
    std::atomic<float>* lfoStereoParam = nullptr;
    std::atomic<float>* envAmountParam = nullptr;
    std::atomic<float>* envAttackParam = nullptr;
    std::atomic<float>* envReleaseParam = nullptr;
    std::atomic<float>* seqOnParam = nullptr;
    std::atomic<float>* seqRateParam = nullptr;
    std::atomic<float>* seqStepsParam = nullptr;
    std::atomic<float>* seqDepthParam = nullptr;
    std::atomic<float>* seqGlideParam = nullptr;
    std::array<std::atomic<float>*, ParamIDs::numSteps> seqStepParams {};
    std::atomic<float>* macroParam = nullptr;
    std::atomic<float>* macroCutoffParam = nullptr;
    std::atomic<float>* macroResParam = nullptr;
    std::atomic<float>* macroDriveParam = nullptr;
    std::atomic<float>* macroMixParam = nullptr;
    std::atomic<float>* macroMorphParam = nullptr;
    std::atomic<float>* inGainParam = nullptr;
    std::atomic<float>* outGainParam = nullptr;
    std::atomic<float>* mixParam = nullptr;
    std::atomic<float>* spreadParam = nullptr;
    std::atomic<float>* stereoModeParam = nullptr;
    std::atomic<float>* stereoOffsetParam = nullptr;
    std::atomic<float>* oversamplingParam = nullptr;

    // inGain runs at the host rate; the rest run inside the (possibly oversampled) engine.
    juce::SmoothedValue<float> inGain;
    juce::SmoothedValue<float> outGain, mix, driveDb, resonance, morph, lfoDepth, envAmount, spread, offset, macro, seqDepth;

    std::unique_ptr<juce::dsp::Oversampling<float>> oversamplers[2];
    int oversamplingIndex = -1;
    double baseRate = 44100.0, engineRate = 44100.0;

    static constexpr int maxStages = 4;
    SvfStage stages[2][maxStages];
    int activeStages = 2;

    double lfoPhase = 0.0, seqPosition = 0.0;
    float seqValue = 0.0f;
    float logCutoff = 10.0f;
    float envelope = 0.0f;

    struct SampleAndHold
    {
        float lastPhase = 1.0f;
        float value = 0.0f;
    };
    SampleAndHold sampleHold[2];
    juce::Random random;

    void setParameter (const juce::String& id, float value);
    void resetParametersForPreset();

    int currentPreset = 0;
    juce::String presetName { "Init" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LowpassAudioProcessor)
};
