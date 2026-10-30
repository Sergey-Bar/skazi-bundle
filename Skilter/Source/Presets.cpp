#include "Presets.h"
#include "PluginProcessor.h"

namespace
{
    using Values = std::vector<std::pair<juce::String, float>>;

    // Choice indices, so the tables below read like the UI.
    enum Morph { LP = 0, BP = 1, HP = 2, NOTCH = 3 };
    enum Slope { DB12 = 0, DB24 = 1, DB36 = 2, DB48 = 3 };
    enum Shape { SINE = 0, TRI = 1, SAW = 2, SQUARE = 3, SH = 4 };
    enum Div { D32 = 0, D16 = 1, D8T = 2, D8 = 3, D4T = 4, D4 = 5, D2 = 6, BAR1 = 7, BAR2 = 8, BAR4 = 9 };
    enum Drive { TUBE = 0, TAPE = 1, HARD = 2 };
    enum Stereo { STEREO = 0, MIDSIDE = 1, DUAL = 2 };

    constexpr float patterns[10][16] {
        {  1.0f,  0.0f,  0.5f,  0.0f, -0.5f,  0.0f,  0.75f, 0.25f,  1.0f,  0.0f,  0.5f, -0.25f, -0.75f,  0.0f,  0.5f,  0.0f },  // pump
        {  0.0f,  0.25f, 0.5f,  0.75f, 1.0f,  0.75f, 0.5f,  0.25f,  0.0f, -0.25f,-0.5f, -0.75f, -1.0f, -0.75f,-0.5f, -0.25f },  // wave
        {  1.0f, -1.0f, -1.0f,  1.0f, -1.0f, -1.0f,  1.0f, -1.0f,   1.0f, -1.0f,  1.0f, -1.0f,  -1.0f,  1.0f, -1.0f,  1.0f },  // gate
        {  0.0f,  0.5f,  1.0f,  0.5f,  0.25f, 0.75f, 0.0f,  1.0f,   0.0f,  0.5f,  1.0f,  0.5f,   0.25f, 0.75f, 0.0f,  1.0f },  // arp
        {  1.0f,  0.0f, -0.5f,  0.75f,-1.0f,  0.25f, 0.5f, -0.25f,  0.0f,  1.0f, -0.75f, 0.5f,  -0.5f,  0.0f,  0.75f,-1.0f },  // broken
        {  1.0f, -1.0f,  1.0f, -1.0f,  1.0f, -1.0f,  1.0f, -1.0f,   1.0f, -1.0f,  1.0f, -1.0f,   1.0f, -1.0f,  1.0f, -1.0f },  // pulse
        {  1.0f,  0.25f,-0.5f,  1.0f,  0.25f,-0.5f,  1.0f,  0.25f, -0.5f,  1.0f,  0.25f,-0.5f,   1.0f,  0.25f,-0.5f,  0.0f },  // triplet feel
        { -1.0f, -0.75f,-0.5f, -0.25f, 0.0f,  0.25f, 0.5f,  0.75f,  1.0f,  0.75f, 0.5f,  0.25f,  0.0f, -0.25f,-0.5f, -0.75f },  // ladder up
        {  1.0f,  0.75f, 0.5f,  0.25f, 0.0f, -0.25f,-0.5f, -0.75f, -1.0f, -0.75f,-0.5f, -0.25f,  0.0f,  0.25f, 0.5f,  0.75f },  // ladder down
        { -1.0f,  1.0f, -1.0f, -1.0f,  1.0f, -1.0f,  1.0f,  1.0f,  -1.0f,  1.0f, -1.0f, -1.0f,   1.0f,  1.0f, -1.0f,  1.0f },  // syncopated gate
    };

    Values withPattern (Values v, int pattern)
    {
        for (int i = 0; i < ParamIDs::numSteps; ++i)
            v.push_back ({ ParamIDs::seqStep (i), patterns[pattern][i] });
        return v;
    }

    std::vector<Preset> build()
    {
        using namespace ParamIDs;
        std::vector<Preset> list;
        auto add = [&list] (const char* category, const char* name, Values values) { list.push_back ({ category, name, std::move (values) }); };

        // ---- Basics -------------------------------------------------------------------
        add ("Basics", "Init", {});
        add ("Basics", "Warm Lowpass", { { cutoff, 2500 }, { resonance, 0.2f }, { drive, 3 } });
        add ("Basics", "Deep Low 48", { { cutoff, 400 }, { slope, DB48 }, { resonance, 0.1f } });
        add ("Basics", "Clean Highpass", { { morph, HP }, { cutoff, 250 } });
        add ("Basics", "Steep Highpass", { { morph, HP }, { cutoff, 800 }, { slope, DB48 } });
        add ("Basics", "Vocal Band", { { morph, BP }, { cutoff, 1500 }, { resonance, 0.35f } });
        add ("Basics", "Telephone", { { morph, BP }, { cutoff, 1800 }, { resonance, 0.5f }, { slope, DB36 }, { drive, 8 }, { driveType, HARD } });
        add ("Basics", "Notch Out", { { morph, NOTCH }, { cutoff, 1000 }, { resonance, 0.3f } });
        add ("Basics", "Air Cut", { { cutoff, 9000 }, { slope, DB12 } });
        add ("Basics", "Resonant Low", { { cutoff, 1200 }, { resonance, 0.7f } });

        // ---- DJ Sweeps: everything lives on the Macro knob ---------------------------
        add ("DJ Sweeps", "Club Closer", { { macroCutoff, -1.0f }, { macroRes, 0.4f }, { macroDrive, 0.2f } });
        add ("DJ Sweeps", "Warm Fade", { { macroCutoff, -0.8f }, { macroRes, 0.15f }, { macroDrive, 0.0f }, { slope, DB12 } });
        add ("DJ Sweeps", "Resonant Dive", { { macroCutoff, -0.9f }, { macroRes, 0.75f }, { macroDrive, 0.1f } });
        add ("DJ Sweeps", "Drive Dive", { { macroCutoff, -0.85f }, { macroRes, 0.3f }, { macroDrive, 0.7f }, { driveType, TAPE } });
        add ("DJ Sweeps", "HP Riser", { { morph, HP }, { cutoff, 20 }, { macroCutoff, 1.0f }, { macroRes, 0.45f }, { macroDrive, 0.0f } });
        add ("DJ Sweeps", "Thin Out", { { morph, HP }, { cutoff, 20 }, { slope, DB48 }, { macroCutoff, 0.9f }, { macroRes, 0.1f }, { macroDrive, 0.0f } });
        add ("DJ Sweeps", "Notch Tunnel", { { morph, NOTCH }, { cutoff, 20000 }, { macroCutoff, -1.0f }, { macroRes, 0.2f }, { macroDrive, 0.0f } });
        add ("DJ Sweeps", "Morph Flip", { { cutoff, 1200 }, { resonance, 0.35f }, { macroCutoff, 0.0f }, { macroRes, 0.2f }, { macroDrive, 0.0f }, { macroMorph, 0.67f } });
        add ("DJ Sweeps", "Wash Out", { { macroCutoff, -0.7f }, { macroRes, 0.3f }, { macroDrive, 0.0f }, { glide, 300 }, { spread, 0.4f } });
        add ("DJ Sweeps", "Breakdown", { { macroCutoff, -0.95f }, { macroRes, 0.55f }, { macroDrive, 0.35f }, { slope, DB36 }, { glide, 120 } });

        // ---- Wobble: tempo-synced LFO on the lowpass ---------------------------------
        {
            const char* names[] { "Wub Quarter", "Eighth Wobble", "Triplet Wub", "Sixteenth Buzz", "Saw Wobble",
                                  "Half Note Growl", "Tri Bounce", "Square Chop", "Bar Breath", "Fast Wobble" };
            const int shapes[] { SINE, SINE, SINE, TRI, SAW, SINE, TRI, SQUARE, SINE, SINE };
            const int divs[] { D4, D8, D8T, D16, D4, D2, D8, D8, BAR1, D16 };
            const float cutoffs[] { 400, 500, 350, 700, 450, 300, 600, 500, 800, 550 };
            const float depths[] { 0.7f, 0.65f, 0.7f, 0.5f, -0.7f, 0.8f, 0.6f, 0.55f, 0.75f, 0.5f };
            for (int i = 0; i < 10; ++i)
                add ("Wobble", names[i], { { cutoff, cutoffs[i] }, { resonance, 0.35f + 0.04f * (float) i }, { lfoSync, 1 }, { lfoDiv, (float) divs[i] },
                                           { lfoShape, (float) shapes[i] }, { lfoDepth, depths[i] }, { drive, 4.0f + (float) (i % 3) * 3.0f }, { glide, 5 } });
        }

        // ---- Acid: high resonance, envelope-driven squelch ---------------------------
        add ("Acid", "303 Squelch", { { cutoff, 500 }, { resonance, 0.8f }, { envAmount, 0.7f }, { envAttack, 2 }, { envRelease, 120 }, { drive, 8 } });
        add ("Acid", "Rubber Acid", { { cutoff, 350 }, { resonance, 0.75f }, { envAmount, 0.8f }, { envAttack, 5 }, { envRelease, 250 }, { drive, 6 }, { driveType, TAPE } });
        add ("Acid", "Acid Bite", { { cutoff, 700 }, { resonance, 0.85f }, { envAmount, 0.55f }, { envAttack, 1 }, { envRelease, 60 }, { drive, 12 }, { driveType, HARD } });
        add ("Acid", "Hot Squelch", { { cutoff, 450 }, { resonance, 0.82f }, { envAmount, 0.75f }, { envAttack, 1.5f }, { envRelease, 90 }, { drive, 16 }, { drivePos, 1 } });
        add ("Acid", "Liquid Acid", { { cutoff, 600 }, { resonance, 0.78f }, { envAmount, 0.6f }, { envAttack, 8 }, { envRelease, 400 }, { glide, 80 }, { slope, DB12 } });
        add ("Acid", "Acid Sequencer", withPattern ({ { cutoff, 500 }, { resonance, 0.8f }, { seqOn, 1 }, { seqDepth, 0.6f }, { seqGlide, 0.35f }, { drive, 9 }, { envAmount, 0.3f } }, 3));
        add ("Acid", "Tape Acid", { { cutoff, 400 }, { resonance, 0.77f }, { envAmount, 0.65f }, { envRelease, 180 }, { drive, 10 }, { driveType, TAPE } });
        add ("Acid", "Screamer", { { cutoff, 900 }, { resonance, 0.88f }, { envAmount, 0.5f }, { envAttack, 1 }, { envRelease, 80 }, { drive, 18 }, { outGain, -4 } });
        add ("Acid", "Acid Pluck", { { cutoff, 250 }, { resonance, 0.7f }, { envAmount, 0.9f }, { envAttack, 0.5f }, { envRelease, 50 }, { drive, 5 } });
        add ("Acid", "Dark Acid", { { cutoff, 220 }, { resonance, 0.83f }, { slope, DB36 }, { envAmount, 0.6f }, { envRelease, 200 }, { drive, 12 }, { driveType, TAPE } });

        // ---- Sequencer: rhythmic cutoff patterns --------------------------------------
        {
            const char* names[] { "Step Pump", "Glide Lines", "Gated Steps", "Arp Filter", "Broken Steps",
                                  "Trance Pulse", "Triplet Steps", "Slow Steps", "Up Ladder", "Down Ladder" };
            const int rates[] { D16, D16, D16, D16, D16, D16, D8T, D8, D16, D16 };
            const float glides[] { 0.15f, 0.7f, 0.02f, 0.1f, 0.05f, 0.03f, 0.2f, 0.5f, 0.3f, 0.3f };
            for (int i = 0; i < 10; ++i)
                add ("Sequencer", names[i], withPattern ({ { cutoff, 700 }, { resonance, 0.4f + 0.03f * (float) i }, { seqOn, 1 }, { seqRate, (float) rates[i] },
                                                           { seqDepth, 0.55f + 0.04f * (float) i }, { seqGlide, glides[i] }, { drive, 4 } }, i));
        }

        // ---- Trance Gate: deep negative steps nearly close the filter ----------------
        {
            const char* names[] { "Gate 16ths", "Gate Offbeat", "Gate Dotted", "Gate Stutter", "Gate Half",
                                  "Gate Triplet", "Gate Broken", "Gate Build", "Gate Swing", "Gate Chop" };
            const int pats[] { 5, 2, 9, 5, 2, 6, 4, 7, 9, 2 };
            const int rates[] { D16, D16, D16, D32, D8, D8T, D16, D16, D16, D32 };
            for (int i = 0; i < 10; ++i)
                add ("Trance Gate", names[i], withPattern ({ { cutoff, 3000 }, { slope, DB48 }, { resonance, 0.15f }, { seqOn, 1 }, { seqRate, (float) rates[i] },
                                                             { seqDepth, 1.0f }, { seqGlide, 0.04f } }, pats[i]));
        }

        // ---- Auto-Wah: envelope follower on the bandpass -----------------------------
        add ("Auto-Wah", "Funk Wah", { { morph, BP }, { cutoff, 450 }, { resonance, 0.6f }, { envAmount, 0.8f }, { envAttack, 3 }, { envRelease, 120 } });
        add ("Auto-Wah", "Quack", { { morph, BP }, { cutoff, 350 }, { resonance, 0.75f }, { envAmount, 0.9f }, { envAttack, 1 }, { envRelease, 70 } });
        add ("Auto-Wah", "Slow Wah", { { morph, BP }, { cutoff, 400 }, { resonance, 0.55f }, { envAmount, 0.7f }, { envAttack, 40 }, { envRelease, 600 } });
        add ("Auto-Wah", "Bass Wah", { { morph, 0.5f }, { cutoff, 200 }, { resonance, 0.6f }, { envAmount, 0.75f }, { envAttack, 5 }, { envRelease, 200 } });
        add ("Auto-Wah", "Inverse Wah", { { morph, BP }, { cutoff, 2500 }, { resonance, 0.55f }, { envAmount, -0.7f }, { envAttack, 3 }, { envRelease, 150 } });
        add ("Auto-Wah", "Guitar Wah", { { morph, BP }, { cutoff, 600 }, { resonance, 0.65f }, { slope, DB12 }, { envAmount, 0.6f }, { envAttack, 10 }, { envRelease, 250 } });
        add ("Auto-Wah", "Wah Drive", { { morph, BP }, { cutoff, 500 }, { resonance, 0.6f }, { envAmount, 0.7f }, { drive, 14 }, { driveType, TUBE } });
        add ("Auto-Wah", "Talking Wah", { { morph, 0.6f }, { cutoff, 500 }, { resonance, 0.7f }, { envAmount, 0.65f }, { envAttack, 6 }, { envRelease, 180 } });
        add ("Auto-Wah", "Snappy Wah", { { morph, BP }, { cutoff, 300 }, { resonance, 0.8f }, { envAmount, 1.0f }, { envAttack, 0.3f }, { envRelease, 40 } });
        add ("Auto-Wah", "Wide Wah", { { morph, BP }, { cutoff, 450 }, { resonance, 0.6f }, { envAmount, 0.75f }, { spread, 0.6f } });

        // ---- Drive & Grit -------------------------------------------------------------
        add ("Drive & Grit", "Tube Warmth", { { cutoff, 6000 }, { drive, 10 }, { driveType, TUBE } });
        add ("Drive & Grit", "Tape Saturate", { { cutoff, 8000 }, { drive, 14 }, { driveType, TAPE }, { slope, DB12 } });
        add ("Drive & Grit", "Hard Clip", { { cutoff, 5000 }, { drive, 18 }, { driveType, HARD }, { outGain, -4 } });
        add ("Drive & Grit", "Post Crunch", { { cutoff, 1500 }, { resonance, 0.5f }, { drive, 16 }, { driveType, HARD }, { drivePos, 1 } });
        add ("Drive & Grit", "Fuzz Filter", { { cutoff, 1100 }, { resonance, 0.45f }, { drive, 24 }, { driveType, HARD }, { outGain, -6 } });
        add ("Drive & Grit", "Grit Band", { { morph, BP }, { cutoff, 1200 }, { resonance, 0.4f }, { drive, 15 }, { driveType, TUBE } });
        add ("Drive & Grit", "Broken Radio", { { morph, BP }, { cutoff, 1700 }, { resonance, 0.55f }, { slope, DB48 }, { drive, 22 }, { driveType, HARD }, { outGain, -3 } });
        add ("Drive & Grit", "Saturated Sub", { { cutoff, 180 }, { resonance, 0.3f }, { drive, 12 }, { driveType, TUBE }, { drivePos, 1 } });
        add ("Drive & Grit", "Crushed Highs", { { morph, HP }, { cutoff, 1500 }, { drive, 20 }, { driveType, HARD }, { drivePos, 1 }, { outGain, -5 } });
        add ("Drive & Grit", "Overdrive Lead", { { cutoff, 3000 }, { resonance, 0.5f }, { drive, 18 }, { driveType, TAPE }, { drivePos, 1 } });

        // ---- Stereo -------------------------------------------------------------------
        add ("Stereo", "Wide Spread", { { cutoff, 1200 }, { resonance, 0.4f }, { spread, 0.8f } });
        add ("Stereo", "Stereo Wobble", { { cutoff, 500 }, { resonance, 0.45f }, { lfoSync, 1 }, { lfoDiv, D4 }, { lfoDepth, 0.6f }, { lfoStereo, 180 } });
        add ("Stereo", "Side Dark", { { stereoMode, MIDSIDE }, { cutoff, 12000 }, { stereoOffset, -4 } });
        add ("Stereo", "Side Bright", { { stereoMode, MIDSIDE }, { morph, HP }, { cutoff, 80 }, { stereoOffset, 3.5f } });
        add ("Stereo", "Dual Shift", { { stereoMode, DUAL }, { cutoff, 1500 }, { resonance, 0.45f }, { stereoOffset, -1.5f } });
        add ("Stereo", "Ping Pong Filter", { { cutoff, 700 }, { resonance, 0.5f }, { lfoSync, 1 }, { lfoDiv, D8 }, { lfoShape, SQUARE }, { lfoDepth, 0.55f }, { lfoStereo, 180 } });
        add ("Stereo", "Mono Bass Focus", { { stereoMode, MIDSIDE }, { morph, HP }, { cutoff, 20 }, { stereoOffset, 2.6f } });
        add ("Stereo", "Haas Filter", { { stereoMode, DUAL }, { morph, BP }, { cutoff, 1000 }, { resonance, 0.3f }, { stereoOffset, 1.0f } });
        add ("Stereo", "Rotating Field", { { cutoff, 900 }, { resonance, 0.4f }, { lfoRate, 0.3f }, { lfoDepth, 0.5f }, { lfoStereo, 90 }, { spread, 0.3f } });
        add ("Stereo", "Spread Notch", { { morph, NOTCH }, { cutoff, 1100 }, { resonance, 0.25f }, { spread, 1.0f } });

        // ---- Self-Osc FX: the filter sings on its own --------------------------------
        add ("Self-Osc FX", "Sine Tone", { { cutoff, 440 }, { resonance, 1.0f }, { slope, DB12 }, { outGain, -6 } });
        add ("Self-Osc FX", "Siren", { { cutoff, 900 }, { resonance, 0.97f }, { lfoRate, 0.5f }, { lfoDepth, 0.4f }, { outGain, -6 } });
        add ("Self-Osc FX", "Laser Zap", { { cutoff, 1500 }, { resonance, 0.96f }, { lfoSync, 1 }, { lfoDiv, D8 }, { lfoShape, SAW }, { lfoDepth, -1.0f }, { outGain, -6 } });
        add ("Self-Osc FX", "Bubble", { { cutoff, 800 }, { resonance, 0.95f }, { lfoSync, 1 }, { lfoDiv, D16 }, { lfoShape, SH }, { lfoDepth, 0.5f }, { glide, 30 }, { outGain, -6 } });
        add ("Self-Osc FX", "Ringing Bell", { { morph, BP }, { cutoff, 1200 }, { resonance, 0.93f }, { mix, 0.6f }, { outGain, -4 } });
        add ("Self-Osc FX", "Space Whistle", { { cutoff, 2400 }, { resonance, 0.98f }, { lfoRate, 4 }, { lfoDepth, 0.05f }, { outGain, -8 } });
        add ("Self-Osc FX", "Alien Chirp", withPattern ({ { cutoff, 900 }, { resonance, 0.96f }, { seqOn, 1 }, { seqDepth, 0.6f }, { seqGlide, 0.5f }, { outGain, -6 } }, 4));
        add ("Self-Osc FX", "Drone Sing", { { cutoff, 110 }, { resonance, 1.0f }, { lfoRate, 0.05f }, { lfoDepth, 0.1f }, { drive, 6 }, { outGain, -6 } });
        add ("Self-Osc FX", "Zap Kick", { { cutoff, 60 }, { resonance, 0.94f }, { envAmount, 0.8f }, { envAttack, 0.3f }, { envRelease, 90 }, { outGain, -6 } });
        add ("Self-Osc FX", "Resonant Ghost", { { cutoff, 700 }, { resonance, 0.92f }, { lfoRate, 0.2f }, { lfoDepth, 0.3f }, { mix, 0.45f }, { outGain, -3 } });

        // ---- Vowels & Morph -----------------------------------------------------------
        add ("Vowels & Morph", "Vowel A", { { morph, 0.7f }, { cutoff, 800 }, { resonance, 0.7f } });
        add ("Vowels & Morph", "Vowel E", { { morph, 1.2f }, { cutoff, 1900 }, { resonance, 0.7f } });
        add ("Vowels & Morph", "Vowel O", { { morph, 0.85f }, { cutoff, 500 }, { resonance, 0.72f } });
        add ("Vowels & Morph", "Nasal", { { morph, 1.4f }, { cutoff, 1300 }, { resonance, 0.78f }, { slope, DB36 } });
        add ("Vowels & Morph", "Formant Sweep", { { cutoff, 900 }, { resonance, 0.7f }, { macroCutoff, 0.3f }, { macroRes, 0.0f }, { macroDrive, 0.0f }, { macroMorph, 0.5f } });
        add ("Vowels & Morph", "Morph Lane", { { morph, 1.5f }, { cutoff, 1000 }, { resonance, 0.6f }, { lfoSync, 1 }, { lfoDiv, BAR1 }, { lfoDepth, 0.4f } });
        add ("Vowels & Morph", "BP Glass", { { morph, BP }, { cutoff, 3500 }, { resonance, 0.82f }, { slope, DB12 }, { mix, 0.7f } });
        add ("Vowels & Morph", "Hollow", { { morph, 2.5f }, { cutoff, 900 }, { resonance, 0.4f } });
        add ("Vowels & Morph", "Between Worlds", { { morph, 1.5f }, { cutoff, 1400 }, { resonance, 0.55f }, { spread, 0.4f } });
        add ("Vowels & Morph", "Air Formant", { { morph, 1.8f }, { cutoff, 5000 }, { resonance, 0.65f }, { mix, 0.8f } });

        // ---- Phase & Notch: slow notches imitate a phaser ----------------------------
        {
            const char* names[] { "Slow Phaser", "Jet Notch", "Stereo Phaser", "Tri Phaser", "Deep Notch",
                                  "Notch Sweep Sync", "Fast Phaser", "Wide Jet", "Subtle Phase", "Notch Wobble" };
            const float rates[] { 0.15f, 0.08f, 0.2f, 0.3f, 0.1f, 1.0f, 2.5f, 0.06f, 0.2f, 1.0f };
            const int shapes[] { SINE, TRI, SINE, TRI, SINE, SINE, SINE, TRI, SINE, SINE };
            const float stereo[] { 0, 0, 120, 60, 0, 90, 0, 180, 45, 0 };
            for (int i = 0; i < 10; ++i)
            {
                Values v { { morph, NOTCH }, { cutoff, 1000 }, { resonance, 0.2f + 0.05f * (float) (i % 5) }, { lfoRate, rates[i] },
                           { lfoShape, (float) shapes[i] }, { lfoDepth, 0.45f + 0.04f * (float) i }, { lfoStereo, stereo[i] }, { mix, i == 8 ? 0.4f : 0.6f } };
                if (i == 5 || i == 9)
                    v.insert (v.end(), { { lfoSync, 1 }, { lfoDiv, i == 5 ? (float) BAR2 : (float) D8 } });
                if (i == 4)
                    v.push_back ({ slope, DB48 });
                add ("Phase & Notch", names[i], std::move (v));
            }
        }

        // ---- Random S&H ---------------------------------------------------------------
        {
            const char* names[] { "Random Steps", "Computer Bleeps", "S&H Bass", "Glitch Filter", "Random Band",
                                  "Chaos 32nds", "Lazy Random", "Stereo Random", "Random Notch", "Data Stream" };
            const int divs[] { D16, D16, D8, D32, D16, D32, BAR1, D8, D8, D32 };
            const float morphs[] { LP, BP, LP, LP, BP, LP, LP, LP, NOTCH, BP };
            const float res[] { 0.5f, 0.85f, 0.45f, 0.6f, 0.65f, 0.55f, 0.4f, 0.5f, 0.3f, 0.8f };
            for (int i = 0; i < 10; ++i)
                add ("Random S&H", names[i], { { morph, morphs[i] }, { cutoff, i == 2 ? 300.0f : 1000.0f }, { resonance, res[i] }, { lfoSync, 1 }, { lfoDiv, (float) divs[i] },
                                               { lfoShape, SH }, { lfoDepth, 0.6f }, { glide, i == 6 ? 250.0f : 8.0f }, { lfoStereo, i == 7 ? 180.0f : 0.0f } });
        }

        // ---- Builds & Risers: slow synced saws that climb over bars ------------------
        add ("Builds & Risers", "HP Build 4 Bars", { { morph, HP }, { cutoff, 600 }, { resonance, 0.4f }, { lfoSync, 1 }, { lfoDiv, BAR4 }, { lfoShape, SAW }, { lfoDepth, 0.85f } });
        add ("Builds & Risers", "LP Opener 4 Bars", { { cutoff, 1000 }, { resonance, 0.35f }, { lfoSync, 1 }, { lfoDiv, BAR4 }, { lfoShape, SAW }, { lfoDepth, 0.9f } });
        add ("Builds & Risers", "Res Riser", { { cutoff, 900 }, { resonance, 0.8f }, { lfoSync, 1 }, { lfoDiv, BAR2 }, { lfoShape, SAW }, { lfoDepth, 0.8f }, { drive, 6 } });
        add ("Builds & Risers", "Tension 2 Bars", { { morph, HP }, { cutoff, 500 }, { resonance, 0.55f }, { slope, DB36 }, { lfoSync, 1 }, { lfoDiv, BAR2 }, { lfoShape, SAW }, { lfoDepth, 0.9f } });
        add ("Builds & Risers", "Drop Prep", { { morph, HP }, { cutoff, 20 }, { macroCutoff, 1.0f }, { macroRes, 0.6f }, { macroDrive, 0.3f }, { macroMix, 0.0f } });
        add ("Builds & Risers", "Snare Roll Filter", { { morph, BP }, { cutoff, 1200 }, { resonance, 0.5f }, { lfoSync, 1 }, { lfoDiv, BAR4 }, { lfoShape, SAW }, { lfoDepth, 0.75f } });
        add ("Builds & Risers", "Climb", withPattern ({ { cutoff, 800 }, { resonance, 0.5f }, { seqOn, 1 }, { seqRate, D4 }, { seqDepth, 0.9f }, { seqGlide, 0.6f } }, 7));
        add ("Builds & Risers", "Lift Off", { { morph, 1.6f }, { cutoff, 900 }, { resonance, 0.6f }, { lfoSync, 1 }, { lfoDiv, BAR4 }, { lfoShape, SAW }, { lfoDepth, 0.9f }, { spread, 0.5f } });
        add ("Builds & Risers", "Fall Down", withPattern ({ { cutoff, 900 }, { resonance, 0.45f }, { seqOn, 1 }, { seqRate, D4 }, { seqDepth, 0.9f }, { seqGlide, 0.6f } }, 8));
        add ("Builds & Risers", "Pressure Cooker", { { cutoff, 700 }, { resonance, 0.7f }, { lfoSync, 1 }, { lfoDiv, BAR2 }, { lfoShape, SAW }, { lfoDepth, 0.8f }, { drive, 14 }, { driveType, TAPE } });

        // ---- Subtle & Mix: parallel colour ------------------------------------------
        add ("Subtle & Mix", "Gentle Warmth", { { cutoff, 5000 }, { drive, 4 }, { mix, 0.5f } });
        add ("Subtle & Mix", "Soft Top", { { cutoff, 10000 }, { slope, DB12 }, { mix, 0.6f } });
        add ("Subtle & Mix", "Parallel Grit", { { cutoff, 2500 }, { resonance, 0.3f }, { drive, 16 }, { driveType, TAPE }, { mix, 0.35f } });
        add ("Subtle & Mix", "Low Glue", { { cutoff, 300 }, { resonance, 0.25f }, { drive, 6 }, { mix, 0.3f } });
        add ("Subtle & Mix", "Air Lift", { { morph, HP }, { cutoff, 4000 }, { resonance, 0.3f }, { mix, 0.35f } });
        add ("Subtle & Mix", "Mellow", { { cutoff, 3500 }, { resonance, 0.1f }, { mix, 0.7f } });
        add ("Subtle & Mix", "Analog Touch", { { cutoff, 12000 }, { resonance, 0.2f }, { drive, 6 }, { driveType, TUBE }, { mix, 0.6f } });
        add ("Subtle & Mix", "Slow Breathe", { { cutoff, 2000 }, { resonance, 0.25f }, { lfoRate, 0.1f }, { lfoDepth, 0.25f }, { mix, 0.6f } });
        add ("Subtle & Mix", "Polish", { { morph, NOTCH }, { cutoff, 3000 }, { resonance, 0.15f }, { mix, 0.4f } });
        add ("Subtle & Mix", "Vintage Roll", { { cutoff, 7000 }, { slope, DB12 }, { drive, 5 }, { driveType, TAPE }, { mix, 0.8f } });

        jassert (list.size() == 150);
        return list;
    }
}

const std::vector<Preset>& factoryPresets()
{
    static const std::vector<Preset> presets = build();
    return presets;
}
