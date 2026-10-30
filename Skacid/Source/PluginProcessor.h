#pragma once

#include "../../Common/SkaziProcessor.h"
#include "AcidEngine.h"

class SkacidProcessor : public skazi::SkaziProcessor
{
public:
    SkacidProcessor();

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override;

    bool acceptsMidi() const override { return true; }
    double getTailLengthSeconds() const override { return 3.0; }

    const std::vector<skazi::Preset>& presetList() const override;
    skazi::Preset randomPreset (juce::Random& rng) const override;
    // the track's key is yours: presets and RANDOM keep it, so every sound plays in the song's key
    bool keepOnPresetChange (const juce::String& id) const override { return id == "seqKey"; }

    skacid::Settings settings() const { return skacid::Settings::fromValues ([this] (const char* id) { return v (id); }); }
    skacid::Pattern pattern() const;

    // Pattern editing from the message thread. Writes go through the parameters, so they are saved and undoable.
    void setPattern (const skacid::Pattern&);
    void setStepField (int step, const char* field, float value);
    void generatePattern (int style);
    void mutatePattern();
    void shiftPattern (int direction);
    void clearPattern();

    // sequencer tools (the row under the pattern buttons)
    enum Tool { NotesUp, NotesDown, RandomNotes, RandomAccents, RandomGlides, GlideAll, GlideNone, Reverse, Copy, Paste };
    void patternTool (int tool);

    // a held drag on the grid is one edit for the host: begin, any number of changes, end
    void beginStepEdit (int step, const char* field);
    void endStepEdit (int step, const char* field);

private:
    juce::Array<juce::RangedAudioParameter*> editing;
    skacid::Pattern clipboard {};
    bool hasClipboard = false;

public:

    skacid::Engine engine;

private:
    std::array<std::array<std::atomic<float>*, 5>, skacid::NumSteps> stepValues {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SkacidProcessor)
};
