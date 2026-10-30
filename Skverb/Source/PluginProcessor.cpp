#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "../../Common/UpdateNotice.h"

namespace
{
    using namespace verb;

    struct Preset
    {
        const char* name;
        int mode;
        float decay, preDelay, feedback, size, lowCut, highCut, damping, bassMult, diffusion, early, width,
              modRate, modDepth, shimmerPitch, shimmerAmt, duck, drive, mix;
        float duckRelease = 250.0f;
        float revAmount = 0.0f, revTime = 0.8f;
        int fltType = 0;
        float fltCutoff = 2000.0f, fltReso = 20.0f, fltEnv = 0.0f;
    };

    //       name                      mode       decay  pre   fb  size  locut  hicut   damp bass diff early width rate depth pitch shim duck drive mix
    const Preset presets[] {
        { "Init Hall",                 hall,       2.5f,  20,   0,  60,   80, 12000,  40, 1.0f, 80, 50, 100, 0.6f, 30,  12,   0,   0,  0, 35 },
        { "HALL: Concert Hall",        hall,       2.8f,  25,   0,  70,   60, 11000,  45, 1.2f, 85, 55, 110, 0.5f, 25,  12,   0,   0,  0, 30 },
        { "HALL: Big Bright Hall",     hall,       3.5f,  30,   0,  80,  120, 16000,  20, 0.9f, 90, 45, 130, 0.7f, 35,  12,   0,   0,  0, 32 },
        { "HALL: Dark Velvet",         hall,       4.0f,  35,   0,  75,   60,  6000,  70, 1.4f, 85, 40, 110, 0.4f, 30,  12,   0,   0,  0, 30 },
        { "HALL: Vocal Hall",          hall,       2.2f,  60,   0,  60,  180, 10000,  45, 0.8f, 80, 40, 100, 0.6f, 25,  12,   0,  35,  0, 25 },
        { "HALL: Wide Strings",        hall,       5.0f,  40,   0,  85,  100, 12000,  40, 1.1f, 90, 35, 160, 0.4f, 40,  12,   0,   0,  0, 40 },
        { "PLATE: Classic Plate",      plate,      1.8f,  10,   0,  55,  120, 14000,  30, 0.8f, 90,  0, 100, 0.8f, 30,  12,   0,   0,  0, 25 },
        { "PLATE: Vocal Plate",        plate,      2.0f,  70,   0,  50,  200, 12000,  35, 0.7f, 90,  0, 110, 0.9f, 35,  12,   0,  40,  0, 22 },
        { "PLATE: Snare Plate",        plate,      1.3f,   5,   0,  45,  250, 15000,  25, 0.6f, 95,  0, 120, 1.0f, 20,  12,   0,   0, 10, 28 },
        { "PLATE: Gold Foil",          plate,      3.0f,  20,   0,  65,  150, 18000,  10, 0.7f, 95,  0, 130, 0.6f, 45,  12,   0,   0,  0, 30 },
        { "PLATE: Short Shiny",        plate,      0.8f,   0,   0,  35,  300, 16000,  20, 0.6f, 85,  0, 100, 1.2f, 25,  12,   0,   0,  0, 20 },
        { "ROOM: Small Room",          room,       0.6f,   5,   0,  40,   80, 10000,  50, 1.0f, 70, 80, 100, 0.5f, 15,  12,   0,   0,  0, 25 },
        { "ROOM: Drum Room",           room,       0.9f,   8,   0,  55,  100,  9000,  45, 1.1f, 75, 90, 120, 0.4f, 10,  12,   0,   0, 15, 30 },
        { "ROOM: Studio Live",         room,       1.1f,  10,   0,  60,   80, 12000,  40, 1.0f, 80, 70, 110, 0.5f, 15,  12,   0,   0,  0, 22 },
        { "ROOM: Wood Booth",          room,       0.4f,   2,   0,  25,  150,  7000,  65, 0.9f, 60, 90,  90, 0.3f, 10,  12,   0,   0,  0, 20 },
        { "ROOM: Garage",              room,       1.4f,  12,   0,  70,   90,  8000,  35, 1.2f, 70, 85, 130, 0.4f, 15,  12,   0,   0, 20, 30 },
        { "CHAMBER: Echo Chamber",     chamber,    1.8f,  15,   0,  60,  100, 11000,  40, 1.0f, 85, 60, 110, 0.6f, 25,  12,   0,   0,  0, 28 },
        { "CHAMBER: Stone Chamber",    chamber,    2.4f,  20,   0,  70,   80,  9000,  55, 1.3f, 85, 70, 120, 0.5f, 20,  12,   0,   0,  0, 30 },
        { "CHAMBER: Vintage Chamber",  chamber,    1.6f,  30,   0,  55,  200,  7000,  60, 0.9f, 80, 60, 100, 0.5f, 20,  12,   0,  20,  0, 25 },
        { "CATHEDRAL: Stone Cathedral",cathedral,  6.0f,  50,   0,  85,   60,  9000,  55, 1.4f, 90, 50, 130, 0.3f, 25,  12,   0,   0,  0, 35 },
        { "CATHEDRAL: Holy Space",     cathedral,  9.0f,  80,   0,  95,  100, 11000,  45, 1.2f, 95, 40, 150, 0.2f, 35,  12,   0,   0,  0, 40 },
        { "CATHEDRAL: Choir Loft",     cathedral,  4.5f, 100,   0,  75,  200,  8000,  50, 1.0f, 90, 45, 140, 0.3f, 30,  12,   0,  30,  0, 30 },
        { "SPRING: Surf Spring",       spring,     2.0f,   0,   0,  50,  150,  6000,  40, 0.8f, 50,  0, 100, 0.5f, 20,  12,   0,   0, 10, 35 },
        { "SPRING: Dub Tank",          spring,     3.5f,  10,  30,  60,  120,  5000,  50, 1.0f, 50,  0, 120, 0.4f, 25,  12,   0,   0, 30, 45 },
        { "SPRING: Guitar Amp",        spring,     1.4f,   0,   0,  40,  200,  6500,  35, 0.7f, 40,  0,  90, 0.6f, 15,  12,   0,   0,  0, 25 },
        { "SHIMMER: Angel Octave",     shimmer,    6.0f,  40,   0,  80,  200, 14000,  30, 0.8f, 90, 20, 150, 0.4f, 35,  12,  40,   0,  0, 40 },
        { "SHIMMER: Fifth Heaven",     shimmer,    7.0f,  50,   0,  85,  250, 13000,  35, 0.8f, 90, 20, 160, 0.3f, 40,   7,  45,   0,  0, 40 },
        { "SHIMMER: Two Octaves Up",   shimmer,    8.0f,  60,   0,  90,  300, 16000,  25, 0.7f, 95, 10, 170, 0.3f, 40,  24,  35,   0,  0, 35 },
        { "SHIMMER: Dark Sub Shimmer", shimmer,    6.0f,  30,   0,  80,   80,  7000,  55, 1.3f, 90, 20, 140, 0.3f, 35, -12,  40,   0,  0, 35 },
        { "SHIMMER: Ambient Bloom",    shimmer,   12.0f,  80,   0, 100,  250, 12000,  35, 0.9f, 95, 10, 180, 0.2f, 50,  12,  60,   0,  0, 50 },
        { "REVERSE: Reverse Swell",    reverse,    2.0f,   0,   0,  70,  150, 12000,  35, 0.9f, 85, 30, 130, 0.5f, 25,  12,   0,   0,  0, 45 },
        { "REVERSE: Ghost Vocal",      reverse,    3.0f,  20,   0,  75,  250, 10000,  40, 0.8f, 90, 20, 140, 0.4f, 30,  12,   0,   0,  0, 50 },
        { "REVERSE: Short Suck",       reverse,    0.8f,   0,   0,  55,  200, 14000,  30, 0.8f, 85, 30, 120, 0.6f, 20,  12,   0,   0,  0, 40 },
        { "GATED: 80s Snare",          gated,      0.35f,  0,   0,  60,  150, 12000,  30, 0.8f, 95, 80, 140, 0.5f, 10,  12,   0,   0, 15, 40 },
        { "GATED: Big Gate",           gated,      0.6f,  10,   0,  80,  100, 10000,  35, 1.0f, 95, 70, 160, 0.5f, 10,  12,   0,   0, 20, 45 },
        { "GATED: Tight Kick Gate",    gated,      0.2f,   0,   0,  45,   60,  8000,  40, 1.2f, 90, 90, 110, 0.5f,  5,  12,   0,   0, 10, 30 },
        { "BLACKHOLE: Event Horizon",  blackhole, 12.0f, 100,   0,  90,  100,  9000,  50, 1.2f, 95,  0, 160, 0.15f, 60, 12,   0,   0,  0, 50 },
        { "BLACKHOLE: Infinite Dark",  blackhole, 20.0f, 150,   0, 100,   80,  6000,  65, 1.4f, 95,  0, 180, 0.1f, 70,  12,   0,   0,  0, 55 },
        { "BLACKHOLE: Space Station",  blackhole,  8.0f,  60,  20,  85,  150, 11000,  40, 1.0f, 90,  0, 170, 0.25f, 55, 12,  15,   0,  0, 45 },
        { "BLACKHOLE: Galaxy Pad",     blackhole, 15.0f, 200,   0, 100,  200, 13000,  35, 0.9f, 95,  0, 200, 0.1f, 80,  12,  30,   0,  0, 60 },
        { "PSY: Lead Throw",           hall,       3.0f, 120,  35,  70,  300, 11000,  40, 0.6f, 85, 30, 150, 0.6f, 30,  12,   0,  60,  0, 35, 180 },
        { "PSY: Ducked Psy Hall",      hall,       2.5f,  40,   0,  70,  250, 12000,  40, 0.7f, 85, 40, 140, 0.5f, 30,  12,   0,  75,  0, 40, 150 },
        { "PSY: Goa Space Echo",       hall,       4.0f, 250,  55,  75,  300,  9000,  45, 0.6f, 85, 20, 160, 0.4f, 40,  12,   0,  50,  0, 40, 200 },
        { "PSY: Darkpsy Abyss",        blackhole, 10.0f,  80,  30,  95,  150,  7000,  55, 1.0f, 95,  0, 170, 0.2f, 70,  -12, 20,  40, 15, 45 },
        { "PSY: Morning Shimmer",      shimmer,    5.0f,  60,   0,  80,  350, 14000,  30, 0.6f, 90, 15, 160, 0.4f, 35,  12,  35,  55,  0, 35, 180 },
        { "PSY: Kick Safe Plate",      plate,      1.6f,  20,   0,  55,  400, 13000,  30, 0.5f, 90,  0, 130, 0.8f, 25,  12,   0,  80,  0, 30, 120 },
        { "VOX: Pop Vocal",            plate,      1.6f,  80,   0,  50,  220, 11000,  35, 0.7f, 90,  0, 110, 0.8f, 30,  12,   0,  45,  0, 20 },
        { "VOX: Slap & Space",         room,       1.2f, 110,  25,  55,  250,  9000,  45, 0.8f, 75, 60, 110, 0.5f, 15,  12,   0,  30,  0, 30 },
        { "VOX: Airy Ballad",          hall,       3.5f, 100,   0,  75,  250, 15000,  25, 0.7f, 90, 30, 140, 0.5f, 30,  12,   0,  40,  0, 28 },
        { "VOX: Whisper Cloud",        shimmer,    5.0f,  70,   0,  80,  300, 13000,  35, 0.7f, 95, 10, 170, 0.3f, 40,  12,  25,  30,  0, 40 },
        { "DRUMS: Snare Fatten",       plate,      1.0f,   0,   0,  45,  200, 14000,  25, 0.8f, 95,  0, 110, 0.9f, 15,  12,   0,   0, 20, 25 },
        { "DRUMS: Big Toms",           hall,       2.0f,  10,   0,  65,   80,  9000,  45, 1.3f, 85, 60, 130, 0.4f, 15,  12,   0,   0, 10, 30 },
        { "DRUMS: Clap Space",         chamber,    1.5f,  25,   0,  60,  300, 13000,  30, 0.6f, 90, 50, 150, 0.6f, 20,  12,   0,  20,  0, 30 },
        { "DRUMS: Hat Sparkle",        plate,      1.2f,  15,   0,  45,  800, 18000,  15, 0.5f, 90,  0, 160, 1.2f, 30,  12,   0,   0,  0, 25 },
        { "FX: Endless Pad",           hall,      30.0f,  50,   0, 100,  150, 11000,  35, 1.0f, 95, 10, 170, 0.2f, 50,  12,   0,   0,  0, 60 },
        { "FX: Dub Echo Verb",         chamber,    3.0f, 375,  65,  65,  250,  5000,  55, 0.8f, 70, 30, 140, 0.4f, 25,  12,   0,  30, 25, 45 },
        { "FX: Robot Comb",            room,       1.5f,   3,  85,  30,  200,  9000,  30, 0.8f, 40, 40, 100, 3.0f, 60,  12,   0,   0, 20, 40 },
        { "FX: Warp Drive",            blackhole,  6.0f,  30,  40,  80,  200, 12000,  40, 0.8f, 90,  0, 200, 4.0f, 100, 12,   0,   0, 40, 50 },
        { "FX: Riser Wash",            shimmer,   15.0f,   0,   0, 100,  400, 16000,  20, 0.6f, 95,  0, 200, 0.3f, 60,  24,  70,   0,  0, 70 },
        { "FX: Lo-Fi Radio Room",      room,       1.0f,  15,  20,  40,  600,  3500,  60, 0.6f, 70, 70,  60, 0.5f, 20,  12,   0,   0, 60, 35 },
        { "FX: Haunted Reverse",       reverse,    4.0f,  50,  30,  90,  200,  8000,  50, 1.0f, 95, 20, 180, 0.3f, 50, -12,  25,   0,  0, 60 },
        { "MIX: Glue Room",            room,       0.7f,   5,   0,  45,  150, 11000,  45, 0.8f, 75, 60, 110, 0.5f, 10,  12,   0,   0,  0, 12 },
        { "MIX: Bus Hall Send",        hall,       2.2f,  25,   0,  65,  200, 10000,  45, 0.7f, 85, 40, 120, 0.5f, 25,  12,   0,  20,  0, 100 },
        { "MIX: Plate Send",           plate,      1.8f,  20,   0,  55,  250, 12000,  35, 0.6f, 90,  0, 120, 0.8f, 30,  12,   0,  20,  0, 100 },

        // Reverse layer (SWELL) and tail filter (TAIL). Last columns: duck release, reverse amount, reverse time, filter type, cutoff, reso, env.
        { "SWELL: Reverse Hall Swell", hall,       3.0f,  20,   0,  75,  120, 12000,  40, 1.0f, 85, 40, 140, 0.5f, 30,  12,   0,   0,  0, 40, 250, 70, 1.2f },
        { "SWELL: Vocal Pre-Verb",     plate,      2.2f,  40,   0,  60,  250, 11000,  35, 0.7f, 90,  0, 120, 0.7f, 30,  12,   0,  30,  0, 35, 250, 55, 0.8f },
        { "SWELL: Psy Lead Suck",      hall,       2.5f,  60,  20,  70,  300, 12000,  40, 0.6f, 85, 30, 150, 0.5f, 30,  12,   0,  60,  0, 35, 180, 60, 0.5f },
        { "SWELL: Shimmer Rise",       shimmer,    6.0f,  40,   0,  85,  250, 14000,  30, 0.8f, 90, 15, 160, 0.3f, 40,  12,  40,   0,  0, 45, 250, 50, 2.0f },
        { "SWELL: Cathedral Inhale",   cathedral,  7.0f,  60,   0,  90,  100, 10000,  45, 1.2f, 95, 40, 150, 0.25f, 30, 12,   0,   0,  0, 45, 250, 65, 2.5f },
        { "TAIL: Dark Sweep Down",     hall,       4.0f,  30,   0,  75,  100, 16000,  25, 1.0f, 90, 40, 140, 0.4f, 30,  12,   0,   0,  0, 40, 250,  0, 0.8f, 1, 6000, 35,  80 },
        { "TAIL: Rising Air",          hall,       4.0f,  30,   0,  80,  150, 16000,  20, 0.8f, 90, 30, 150, 0.4f, 30,  12,   0,   0,  0, 40, 250,  0, 0.8f, 3,  400, 30, -70 },
        { "TAIL: Resonant Psy Tail",   hall,       3.5f,  50,  20,  75,  250, 14000,  30, 0.7f, 85, 30, 150, 0.5f, 30,  12,   0,  60,  0, 35, 180,  0, 0.8f, 1, 1800, 75,  60 },
        { "TAIL: Telephone Space",     room,       1.2f,  15,   0,  55,  100, 12000,  40, 0.8f, 75, 60, 100, 0.5f, 15,  12,   0,   0,  0, 35, 250,  0, 0.8f, 2, 1200, 40,   0 },
        { "TAIL: Acid Verb",           chamber,    2.5f,  20,  25,  60,  150, 14000,  30, 0.8f, 85, 50, 130, 0.6f, 25,  12,   0,  30, 30, 35, 200,  0, 0.8f, 1,  900, 85,  90 },
        { "TAIL: Underwater",          hall,       5.0f,  40,   0,  85,   60, 12000,  40, 1.3f, 90, 30, 150, 0.3f, 45,  12,   0,   0,  0, 45, 250,  0, 0.8f, 1,  500, 30,   0 },
        { "PSY: Reverse Filter Throw", hall,       3.0f, 100,  40,  70,  300, 12000,  40, 0.6f, 85, 30, 150, 0.5f, 30,  12,   0,  60,  0, 35, 180, 60, 0.6f, 1, 3000, 50,  70 },
    };

    constexpr int numPresets = (int) std::size (presets);
}

SkverbAudioProcessor::SkverbAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "SKVERB", createParameterLayout())
{
}

juce::AudioProcessorValueTreeState::ParameterLayout SkverbAudioProcessor::createParameterLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    auto percent = [] (float v, int) { return String (roundToInt (v)) + "%"; };
    auto hz = [] (float v, int) { return v >= 1000.0f ? String (v / 1000.0f, v < 9950.0f ? 1 : 0) + " kHz" : String (roundToInt (v)) + " Hz"; };

    auto addFloat = [&] (const char* id, const char* name, NormalisableRange<float> range, float def,
                         std::function<String (float, int)> toText)
    {
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, range, def,
                                                           AudioParameterFloatAttributes().withStringFromValueFunction (std::move (toText))));
    };

    StringArray modes;
    for (int m = 0; m < numModes; ++m)
        modes.add (spec (m).name);
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ParamIDs::mode, 1 }, "Algorithm", modes, 0));

    addFloat (ParamIDs::decay, "Decay", NormalisableRange<float> (0.1f, 30.0f, 0.0f, 0.3f), 2.5f,
              [] (float v, int) { return String (v, v < 10.0f ? 2 : 1) + " s"; });
    addFloat (ParamIDs::preDelay, "Pre-Delay", NormalisableRange<float> (0.0f, 1000.0f, 0.0f, 0.4f), 20.0f,
              [] (float v, int) { return String (roundToInt (v)) + " ms"; });
    addFloat (ParamIDs::feedback, "Feedback", { 0.0f, 95.0f, 0.1f }, 0.0f, percent);
    addFloat (ParamIDs::size, "Size", { 0.0f, 100.0f, 0.1f }, 60.0f, percent);
    addFloat (ParamIDs::lowCut, "Low Cut", NormalisableRange<float> (20.0f, 1000.0f, 0.0f, 0.35f), 80.0f, hz);
    addFloat (ParamIDs::highCut, "High Cut", NormalisableRange<float> (1000.0f, 20000.0f, 0.0f, 0.4f), 12000.0f, hz);
    addFloat (ParamIDs::damping, "Damping", { 0.0f, 100.0f, 0.1f }, 40.0f, percent);
    addFloat (ParamIDs::bassMult, "Bass Decay", NormalisableRange<float> (0.25f, 4.0f, 0.0f, 0.5f), 1.0f,
              [] (float v, int) { return String (v, 2) + "x"; });
    addFloat (ParamIDs::diffusion, "Diffusion", { 0.0f, 100.0f, 0.1f }, 80.0f, percent);
    addFloat (ParamIDs::early, "Early", { 0.0f, 100.0f, 0.1f }, 50.0f, percent);
    addFloat (ParamIDs::width, "Width", { 0.0f, 200.0f, 0.1f }, 100.0f, percent);
    addFloat (ParamIDs::modRate, "Mod Rate", NormalisableRange<float> (0.05f, 5.0f, 0.0f, 0.4f), 0.6f,
              [] (float v, int) { return String (v, 2) + " Hz"; });
    addFloat (ParamIDs::modDepth, "Mod Depth", { 0.0f, 100.0f, 0.1f }, 30.0f, percent);
    addFloat (ParamIDs::shimmerPitch, "Shimmer Pitch", { -24.0f, 24.0f, 1.0f }, 12.0f,
              [] (float v, int) { const int st = roundToInt (v); return (st > 0 ? "+" : "") + String (st) + " st"; });
    addFloat (ParamIDs::shimmerAmt, "Shimmer", { 0.0f, 100.0f, 0.1f }, 0.0f, percent);
    addFloat (ParamIDs::duck, "Duck", { 0.0f, 100.0f, 0.1f }, 0.0f, percent);
    addFloat (ParamIDs::duckRelease, "Duck Release", NormalisableRange<float> (20.0f, 1000.0f, 0.0f, 0.5f), 250.0f,
              [] (float v, int) { return String (roundToInt (v)) + " ms"; });
    addFloat (ParamIDs::drive, "Drive", { 0.0f, 100.0f, 0.1f }, 0.0f, percent);
    addFloat (ParamIDs::mix, "Mix", { 0.0f, 100.0f, 0.1f }, 35.0f, percent);
    addFloat (ParamIDs::output, "Output", { -24.0f, 12.0f, 0.1f }, 0.0f,
              [] (float v, int) { return (v > 0.05f ? "+" : "") + String (v, 1) + " dB"; });
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { ParamIDs::freeze, 1 }, "Freeze", false));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { ParamIDs::sync, 1 }, "Pre-Delay Sync", false));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { ParamIDs::killDry, 1 }, "Kill Dry", false));

    addFloat (ParamIDs::revAmount, "Reverse Amount", { 0.0f, 100.0f, 0.1f }, 0.0f, percent);
    addFloat (ParamIDs::revTime, "Reverse Time", NormalisableRange<float> (0.1f, 2.5f, 0.0f, 0.6f), 0.8f,
              [] (float v, int) { return v < 1.0f ? String (roundToInt (v * 1000.0f)) + " ms" : String (v, 2) + " s"; });
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ParamIDs::fltType, 1 }, "Tail Filter", TailFilter::names, 0));
    addFloat (ParamIDs::fltCutoff, "Tail Cutoff", NormalisableRange<float> (40.0f, 18000.0f, 0.0f, 0.3f), 2000.0f, hz);
    addFloat (ParamIDs::fltReso, "Tail Resonance", { 0.0f, 100.0f, 0.1f }, 20.0f, percent);
    addFloat (ParamIDs::fltEnv, "Tail Envelope", { -100.0f, 100.0f, 0.1f }, 0.0f,
              [] (float v, int) { const int e = roundToInt (v); return (e > 0 ? "+" : "") + String (e) + "%"; });

    return layout;
}

bool SkverbAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;
    const auto in = layouts.getMainInputChannelSet();
    return in == juce::AudioChannelSet::stereo() || in == juce::AudioChannelSet::mono();
}

void SkverbAudioProcessor::prepareToPlay (double newSampleRate, int samplesPerBlock)
{
    sampleRate = newSampleRate;
    engine.prepare (sampleRate);
    wet.setSize (2, juce::jmax (1, samplesPerBlock), false, false, true);
    for (auto* s : { &dryGain, &wetGain, &outGain })
        s->reset (sampleRate, 0.03);
    const float mixAmount = get (ParamIDs::mix) * 0.01f;
    dryGain.setCurrentAndTargetValue (get (ParamIDs::killDry) > 0.5f ? 0.0f : std::cos (mixAmount * verb::pi * 0.5f));
    wetGain.setCurrentAndTargetValue (std::sin (mixAmount * verb::pi * 0.5f));
    outGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (get (ParamIDs::output)));
    duckEnv = 0.0f;
    duckSmooth = 1.0f;
}

void SkverbAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    if (n == 0)
        return;
    if (getTotalNumInputChannels() == 1)
        buffer.copyFrom (1, 0, buffer, 0, 0, n);
    if (wet.getNumSamples() < n)
        wet.setSize (2, n, false, false, true);

    // Pre-delay: milliseconds, or a note value when SYNC is on.
    float preDelayMs = get (ParamIDs::preDelay);
    int syncIndex = -1;
    if (get (ParamIDs::sync) > 0.5f)
    {
        double bpm = 120.0;
        if (auto* head = getPlayHead())
            if (auto pos = head->getPosition())
                if (auto b = pos->getBpm())
                    bpm = juce::jlimit (30.0, 300.0, *b);
        const float norm = apvts.getParameter (ParamIDs::preDelay)->getValue();
        syncIndex = juce::jlimit (0, Sync::count - 1, juce::roundToInt (norm * (float) (Sync::count - 1)));
        preDelayMs = juce::jmin (2000.0f, (float) (Sync::beats[syncIndex] * 60000.0 / bpm));
    }
    livePreDelayMs = preDelayMs;
    liveSyncIndex = syncIndex;

    verb::Settings s;
    s.mode          = (int) get (ParamIDs::mode);
    s.decay         = get (ParamIDs::decay);
    s.preDelayMs    = preDelayMs;
    s.feedback      = get (ParamIDs::feedback) * 0.01f;
    s.size          = get (ParamIDs::size) * 0.01f;
    s.lowCut        = get (ParamIDs::lowCut);
    s.highCut       = get (ParamIDs::highCut);
    s.damping       = get (ParamIDs::damping) * 0.01f;
    s.bassMult      = get (ParamIDs::bassMult);
    s.diffusion     = get (ParamIDs::diffusion) * 0.01f;
    s.early         = get (ParamIDs::early) * 0.01f;
    s.width         = get (ParamIDs::width) * 0.01f;
    s.modRate       = get (ParamIDs::modRate);
    s.modDepth      = get (ParamIDs::modDepth) * 0.01f;
    s.shimmerPitch  = get (ParamIDs::shimmerPitch);
    s.shimmerAmount = get (ParamIDs::shimmerAmt) * 0.01f;
    s.drive         = get (ParamIDs::drive) * 0.01f;
    s.freeze        = get (ParamIDs::freeze) > 0.5f;
    s.reverseAmount = get (ParamIDs::revAmount) * 0.01f;
    s.reverseTime   = get (ParamIDs::revTime);
    s.filterType    = (int) get (ParamIDs::fltType);
    s.filterCutoff  = get (ParamIDs::fltCutoff);
    s.filterReso    = get (ParamIDs::fltReso) * 0.01f;
    s.filterEnv     = get (ParamIDs::fltEnv) * 0.01f;

    auto* left = buffer.getWritePointer (0);
    auto* right = buffer.getWritePointer (1);
    auto* wetL = wet.getWritePointer (0);
    auto* wetR = wet.getWritePointer (1);
    engine.process (left, right, wetL, wetR, n, s);

    // Equal-power dry/wet; KILL DRY turns it into a 100% wet send effect.
    const float mixAmount = get (ParamIDs::mix) * 0.01f;
    const bool killDry = get (ParamIDs::killDry) > 0.5f;
    dryGain.setTargetValue (killDry ? 0.0f : std::cos (mixAmount * verb::pi * 0.5f));
    wetGain.setTargetValue (std::sin (mixAmount * verb::pi * 0.5f));
    outGain.setTargetValue (juce::Decibels::decibelsToGain (get (ParamIDs::output)));

    // Ducking: the wet dips while the dry input plays, then swells back in the gaps.
    const float duck = get (ParamIDs::duck) * 0.01f;
    const float attack = verb::onePoleCoeff (1.0f / 0.003f, sampleRate);
    const float release = verb::onePoleCoeff (1000.0f / get (ParamIDs::duckRelease), sampleRate);
    const float gainSmooth = verb::onePoleCoeff (1.0f / 0.01f, sampleRate);

    float inMax = 0.0f, wetMax = 0.0f, outMax = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        const float level = juce::jmax (std::abs (left[i]), std::abs (right[i]));
        duckEnv += (level - duckEnv) * (level > duckEnv ? attack : release);
        const float db = juce::Decibels::gainToDecibels (duckEnv, -80.0f);
        const float target = 1.0f - duck * juce::jlimit (0.0f, 1.0f, (db + 40.0f) / 34.0f);
        duckSmooth += (target - duckSmooth) * gainSmooth;

        const float w = wetGain.getNextValue() * duckSmooth;
        const float d = dryGain.getNextValue();
        const float g = outGain.getNextValue();
        const float wl = wetL[i] * w, wr = wetR[i] * w;
        inMax = juce::jmax (inMax, level);
        wetMax = juce::jmax (wetMax, std::abs (wl), std::abs (wr));
        left[i] = (left[i] * d + wl) * g;
        right[i] = (right[i] * d + wr) * g;
        outMax = juce::jmax (outMax, std::abs (left[i]), std::abs (right[i]));
    }

    auto raise = [] (std::atomic<float>& meter, float v)
    {
        float old = meter.load();
        while (v > old && ! meter.compare_exchange_weak (old, v)) {}
    };
    raise (inPeak, inMax);
    raise (wetPeak, wetMax);
    raise (outPeak, outMax);
    duckGain = duckSmooth;
    gateLevel = engine.getGate();
}

void SkverbAudioProcessor::set (const char* id, float value)
{
    if (auto* param = apvts.getParameter (id))
        param->setValueNotifyingHost (param->convertTo0to1 (value));
}

void SkverbAudioProcessor::randomise()
{
    auto& r = random;
    auto chance = [&r] (float p) { return r.nextFloat() < p; };
    auto logRange = [&r] (float lo, float hi) { return lo * std::pow (hi / lo, r.nextFloat()); };

    const int mode = r.nextInt (numModes);
    set (ParamIDs::mode, (float) mode);
    set (ParamIDs::decay, mode == gated ? logRange (0.15f, 0.8f) : logRange (0.5f, mode == blackhole || mode == shimmer ? 20.0f : 8.0f));
    set (ParamIDs::preDelay, chance (0.3f) ? r.nextFloat() * 10.0f : logRange (10.0f, 300.0f));
    set (ParamIDs::feedback, chance (0.7f) ? 0.0f : r.nextFloat() * 70.0f);
    set (ParamIDs::size, 30.0f + r.nextFloat() * 70.0f);
    set (ParamIDs::lowCut, logRange (40.0f, 500.0f));
    set (ParamIDs::highCut, logRange (4000.0f, 18000.0f));
    set (ParamIDs::damping, 10.0f + r.nextFloat() * 60.0f);
    set (ParamIDs::bassMult, logRange (0.5f, 1.6f));
    set (ParamIDs::diffusion, 50.0f + r.nextFloat() * 50.0f);
    set (ParamIDs::early, r.nextFloat() * 90.0f);
    set (ParamIDs::width, 80.0f + r.nextFloat() * 120.0f);
    set (ParamIDs::modRate, logRange (0.1f, 2.0f));
    set (ParamIDs::modDepth, r.nextFloat() * 70.0f);
    const float pitches[] { 12.0f, 12.0f, 7.0f, 24.0f, -12.0f, 5.0f, 19.0f };
    set (ParamIDs::shimmerPitch, pitches[r.nextInt ((int) std::size (pitches))]);
    set (ParamIDs::shimmerAmt, mode == shimmer || chance (0.2f) ? r.nextFloat() * 60.0f : 0.0f);
    set (ParamIDs::duck, chance (0.5f) ? 0.0f : r.nextFloat() * 80.0f);
    set (ParamIDs::duckRelease, logRange (80.0f, 500.0f));
    set (ParamIDs::drive, chance (0.7f) ? 0.0f : r.nextFloat() * 50.0f);
    set (ParamIDs::sync, chance (0.25f) ? 1.0f : 0.0f);
    set (ParamIDs::revAmount, mode != reverse && chance (0.25f) ? 30.0f + r.nextFloat() * 60.0f : 0.0f);
    set (ParamIDs::revTime, logRange (0.2f, 2.0f));
    set (ParamIDs::fltType, chance (0.35f) ? (float) (1 + r.nextInt (3)) : 0.0f);
    set (ParamIDs::fltCutoff, logRange (300.0f, 8000.0f));
    set (ParamIDs::fltReso, r.nextFloat() * 70.0f);
    set (ParamIDs::fltEnv, chance (0.4f) ? 0.0f : r.nextFloat() * 160.0f - 80.0f);

    randomised = true;
}

int SkverbAudioProcessor::getNumPrograms() { return numPresets; }

const juce::String SkverbAudioProcessor::getProgramName (int index)
{
    return juce::isPositiveAndBelow (index, numPresets) ? presets[index].name : "";
}

void SkverbAudioProcessor::setCurrentProgram (int index)
{
    if (! juce::isPositiveAndBelow (index, numPresets))
        return;

    currentPreset = index;
    randomised = false;
    const auto& p = presets[index];
    set (ParamIDs::mode, (float) p.mode);
    set (ParamIDs::decay, p.decay);
    set (ParamIDs::preDelay, p.preDelay);
    set (ParamIDs::feedback, p.feedback);
    set (ParamIDs::size, p.size);
    set (ParamIDs::lowCut, p.lowCut);
    set (ParamIDs::highCut, p.highCut);
    set (ParamIDs::damping, p.damping);
    set (ParamIDs::bassMult, p.bassMult);
    set (ParamIDs::diffusion, p.diffusion);
    set (ParamIDs::early, p.early);
    set (ParamIDs::width, p.width);
    set (ParamIDs::modRate, p.modRate);
    set (ParamIDs::modDepth, p.modDepth);
    set (ParamIDs::shimmerPitch, p.shimmerPitch);
    set (ParamIDs::shimmerAmt, p.shimmerAmt);
    set (ParamIDs::duck, p.duck);
    set (ParamIDs::duckRelease, p.duckRelease);
    set (ParamIDs::drive, p.drive);
    set (ParamIDs::mix, p.mix);
    set (ParamIDs::revAmount, p.revAmount);
    set (ParamIDs::revTime, p.revTime);
    set (ParamIDs::fltType, (float) p.fltType);
    set (ParamIDs::fltCutoff, p.fltCutoff);
    set (ParamIDs::fltReso, p.fltReso);
    set (ParamIDs::fltEnv, p.fltEnv);
    set (ParamIDs::sync, 0.0f);
    set (ParamIDs::freeze, 0.0f);
    set (ParamIDs::killDry, 0.0f);
}

void SkverbAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("preset", currentPreset, nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void SkverbAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto state = juce::ValueTree::fromXml (*xml);
            currentPreset = juce::jlimit (0, numPresets - 1, (int) state.getProperty ("preset", 0));
            apvts.replaceState (state);
        }
}

juce::AudioProcessorEditor* SkverbAudioProcessor::createEditor()
{
    return skazi::withUpdateNotice (new SkverbAudioProcessorEditor (*this));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SkverbAudioProcessor();
}
