#pragma once

#include "../../Common/SkaziProcessor.h"
#include "AudioToMidi.h"
#include "Pattern.h"
#include "PreviewSynth.h"

class MidiGodProcessor : public skazi::SkaziProcessor, private juce::Timer
{
public:
    MidiGodProcessor();
    ~MidiGodProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override;

    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }
    double getTailLengthSeconds() const override { return 0.8; }

    const std::vector<skazi::Preset>& presetList() const override;
    skazi::Preset randomPreset (juce::Random& rng) const override;

    // The key belongs to the track and the audio / output settings to the session: presets leave them alone
    bool keepOnPresetChange (const juce::String& id) const override
    {
        return id == "key" || id.startsWith ("a2m") || id == "vol" || id == "midiOut" || id == "freeRun";
    }

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    midigod::Settings settings() const { return midigod::Settings::fromValues ([this] (const char* id) { return v (id); }); }

    // The loop that plays and exports right now
    std::shared_ptr<const midigod::Pattern> pattern() const { return std::atomic_load (&live); }

    // GOD MODE: a fresh variation of the current genre and role (back to the generator if audio was loaded)
    void godMode();

    // MUTATE: the ROLL BASS motif with a few steps changed; GOD MODE clears it
    void mutate();

    // Audio to MIDI, analysed on a background thread at the project tempo
    void convertAudio (const juce::File& file);
    bool isAnalysing() const { return analysing.load(); }
    bool isFromAudio() const { return fromAudio; }
    const juce::String& getAudioStatus() const { return audioStatus; }
    void backToGenerator();

    // The current loop as a .mid in the temp folder, ready to drag into the DAW
    juce::File writeMidiFile() const;
    bool saveMidiFile (const juce::File& target) const;

    double tempo() const;
    std::atomic<double> uiBeat { -1.0 }; // play position inside the loop, -1 when stopped

private:
    void timerCallback() override;
    void regenerate();
    std::vector<float> watched (const std::vector<const char*>& ids) const;
    void installAudio (midigod::ConvertResult res, const juce::File& file, int mode);
    void setParam (const char* id, float value);

    std::shared_ptr<const midigod::Pattern> live, retired[2];

    // audio-derived loop (raw notes; quantize, key snap and octave are applied on top)
    bool fromAudio = false, audioDrums = false;
    std::vector<midigod::Note> audioNotes;
    double audioLength = 16.0;
    juce::String audioName, audioStatus { "DROP A WAV / MP3 / FLAC HERE OR CLICK TO LOAD" };
    int audioMode = 0;
    juce::File audioFile;

    std::vector<float> lastIdentity, lastShape, lastFinish, lastAnalysis;
    void rememberWatched();

    struct Event { int sample; bool on; int note, velocity; bool fromHost; bool bend = false; float bendValue = 0.0f; };
    std::vector<Event> events;
    juce::MidiBuffer generated;
    juce::ThreadPool pool { juce::ThreadPoolOptions{}.withNumberOfThreads (1) };
    std::atomic<bool> analysing { false }, cancelAnalysis { false };
    std::shared_ptr<std::atomic<bool>> alive = std::make_shared<std::atomic<bool>> (true);

    // playback
    midigod::PreviewSynth synth;
    std::array<int, 128> sounding {};
    bool bent = false;
    const midigod::Pattern* playingPattern = nullptr;
    double freeBeat = 0.0, expectedBeat = -1.0;
    bool wasRunning = false;
    std::atomic<double> bpmNow { 0.0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MidiGodProcessor)
};
