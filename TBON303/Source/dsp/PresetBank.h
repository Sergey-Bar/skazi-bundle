#pragma once

// Factory bank: 16 categories x 32 presets = 512. Each category is a template
// (centre values plus how far each knob may wander); every preset is a seeded,
// deterministic variation of it with its own generated pattern, so the bank is
// identical on every machine and every build. The same templates drive RANDOM.

#include "PatternGenerator.h"
#include <cmath>
#include <cstdint>
#include <random>
#include <string>
#include <utility>
#include <vector>

namespace s303
{
struct Preset
{
    std::string category;
    std::string name;
    std::vector<std::pair<std::string, float>> values; // parameter id -> plain value
    Pattern pattern;
};

class PresetTemplate
{
public:
    PresetTemplate (std::string cat, std::vector<std::string> first, std::vector<std::string> second)
        : category (std::move (cat)), wordsA (std::move (first)), wordsB (std::move (second)) {}

    /** Linear range: base +/- spread. */
    PresetTemplate& lin (const char* id, float base, float spread) { vars.push_back ({ id, Kind::Lin, base, spread, {} }); return *this; }
    /** Exponential (frequencies, times): base * 2^(+/- octaves). */
    PresetTemplate& expo (const char* id, float base, float octaves) { vars.push_back ({ id, Kind::Exp, base, octaves, {} }); return *this; }
    /** One of a set of values (switches, choices, loop lengths). */
    PresetTemplate& pick (const char* id, std::vector<float> options) { vars.push_back ({ id, Kind::Pick, 0, 0, std::move (options) }); return *this; }

    Preset make (int index, uint32_t seed, float wildness = 1.0f) const
    {
        std::mt19937 rng (seed);
        std::uniform_real_distribution<float> uni (-1.0f, 1.0f);

        Preset p;
        p.category = category;
        p.name = nameFor (index);

        // Start from neutral defaults so nothing leaks over from the previous sound.
        for (auto& d : defaults())
            p.values.push_back (d);

        // Character and an FX scene first, so every preset sounds distinct; the
        // category's own settings below still win wherever it sets something.
        addCharacter (p.values, seed ^ 0x51ce5u);
        addFxScene (p.values, seed ^ 0xfe7u);

        for (auto& v : vars)
        {
            float value = v.base;
            if (v.kind == Kind::Lin)
                value = v.base + uni (rng) * v.spread * wildness;
            else if (v.kind == Kind::Exp)
                value = v.base * std::exp2 (uni (rng) * v.spread * wildness);
            else
                value = v.options[(size_t) (rng() % v.options.size())];
            set (p.values, v.id, value);
        }

        GenSettings g;
        g.scale = (int) get (p.values, "gen_scale");
        g.density = get (p.values, "gen_density");
        g.accent = get (p.values, "gen_accent");
        g.slide = get (p.values, "gen_slide");
        g.octave = get (p.values, "gen_octave");
        PatternGenerator gen (seed ^ 0x5eed303u);
        gen.generate (p.pattern, g);
        p.pattern.length = (int) get (p.values, "seq_len");
        return p;
    }

    const std::string& getCategory() const { return category; }

private:
    enum class Kind { Lin, Exp, Pick };

    using Values = std::vector<std::pair<std::string, float>>;

    /** Random voice colour: drive types, sub, second filter, pulse width, tracking. */
    static void addCharacter (Values& vals, uint32_t seed)
    {
        std::mt19937 r (seed);
        auto between = [&r] (float lo, float hi) { return lo + (hi - lo) * std::uniform_real_distribution<float> (0.0f, 1.0f) (r); };
        auto chance = [&r] (float pr) { return std::uniform_real_distribution<float> (0.0f, 1.0f) (r) < pr; };

        if (chance (0.55f)) set (vals, "post_type", (float) (r() % 14));
        if (chance (0.3f))
        {
            set (vals, "pre_type", (float) (r() % 14));
            set (vals, "pre_drive", between (0.1f, 0.4f));
        }
        if (chance (0.45f))
        {
            set (vals, "sub_wave", (float) (r() % 4));
            set (vals, "sub_level", between (0.15f, 0.5f));
            if (chance (0.25f)) set (vals, "sub_oct", 1.0f);
        }
        const float f2 = between (0.0f, 1.0f);
        if (f2 < 0.15f)
        {
            set (vals, "f2_type", 1.0f);
            set (vals, "f2_cutoff", between (400.0f, 3000.0f));
            set (vals, "f2_reso", between (0.2f, 0.6f));
        }
        else if (f2 < 0.25f)
        {
            set (vals, "f2_type", 2.0f);
            set (vals, "f2_vowel", between (0.0f, 4.0f));
            set (vals, "f2_mix", between (0.4f, 0.8f));
        }
        if (chance (0.3f)) set (vals, "pulse_width", between (0.1f, 0.6f));
        if (chance (0.3f)) set (vals, "flt_track", between (0.1f, 0.5f));
        set (vals, "sweep", std::exp2 (between (-1.0f, 1.0f)));

        // Oscillator and amp colour
        if (chance (0.35f)) set (vals, "wave", between (0.0f, 1.0f));
        if (chance (0.15f)) set (vals, "tune", chance (0.5f) ? 12.0f : -12.0f);
        set (vals, "drift", between (0.1f, 0.7f));
        if (chance (0.5f)) set (vals, "fat", between (0.0f, 0.5f));
        if (chance (0.25f)) set (vals, "vca_decay", std::exp2 (between (-1.7f, 1.0f)) * 2.5f);
        if (chance (0.5f)) set (vals, "dist_tone", between (2500.0f, 12000.0f));

        // Groove
        if (chance (0.25f)) set (vals, "seq_len", (float) (std::initializer_list<int> { 12, 14, 15, 16 }.begin()[r() % 4]));
        if (chance (0.3f)) set (vals, "swing", between (0.1f, 0.4f));
        set (vals, "gate_len", between (0.3f, 0.75f));
        if (chance (0.15f)) set (vals, "seq_rate", 2.0f);
        if (chance (0.3f)) set (vals, "gen_scale", (float) (r() % 8));
    }

    /** One of seven FX scenes, each with its own random variation. */
    static void addFxScene (Values& vals, uint32_t seed)
    {
        std::mt19937 r (seed);
        auto between = [&r] (float lo, float hi) { return lo + (hi - lo) * std::uniform_real_distribution<float> (0.0f, 1.0f) (r); };
        auto pickOf = [&r] (std::initializer_list<float> o) { return *(o.begin() + (r() % o.size())); };

        switch (r() % 7)
        {
            case 0: // dry and punchy
                set (vals, "width", 1.0f);
                break;
            case 1: // slapback
                set (vals, "dly_sync", pickOf ({ 4, 5 }));
                set (vals, "dly_mix", between (0.15f, 0.3f));
                set (vals, "dly_fb", between (0.15f, 0.3f));
                set (vals, "rev_mix", between (0.05f, 0.12f));
                break;
            case 2: // dub echo
                set (vals, "dly_sync", pickOf ({ 8, 11 }));
                set (vals, "dly_mix", between (0.3f, 0.45f));
                set (vals, "dly_fb", between (0.55f, 0.75f));
                set (vals, "dly_tone", between (1200.0f, 2200.0f));
                set (vals, "rev_mix", between (0.15f, 0.3f));
                set (vals, "rev_size", between (0.5f, 0.75f));
                break;
            case 3: // psy ping-pong
                set (vals, "dly_sync", pickOf ({ 6, 8 }));
                set (vals, "dly_mix", between (0.25f, 0.4f));
                set (vals, "dly_fb", between (0.4f, 0.6f));
                set (vals, "dly_tone", between (3000.0f, 6000.0f));
                break;
            case 4: // wide chorus
                set (vals, "mod_mode", 1.0f);
                set (vals, "mod_rate", between (0.3f, 0.9f));
                set (vals, "mod_depth", between (0.4f, 0.8f));
                set (vals, "mod_mix", between (0.3f, 0.5f));
                set (vals, "width", between (1.4f, 1.8f));
                set (vals, "rev_mix", between (0.05f, 0.15f));
                break;
            case 5: // flanger jet
                set (vals, "mod_mode", 2.0f);
                set (vals, "mod_rate", between (0.1f, 0.4f));
                set (vals, "mod_depth", between (0.6f, 0.9f));
                set (vals, "mod_mix", between (0.35f, 0.55f));
                break;
            default: // big hall
                set (vals, "rev_mix", between (0.3f, 0.45f));
                set (vals, "rev_size", between (0.75f, 0.95f));
                set (vals, "rev_damp", between (0.3f, 0.6f));
                set (vals, "dly_mix", between (0.05f, 0.15f));
                break;
        }
    }
    struct Var { std::string id; Kind kind; float base, spread; std::vector<float> options; };

    std::string nameFor (int i) const
    {
        // 8 x 8 words: (i % 8, i / 8) is unique for i < 64, the offset just mixes it up.
        const size_t a = (size_t) i % wordsA.size();
        const size_t b = ((size_t) i / wordsA.size() + 3 * a) % wordsB.size();
        return wordsA[a] + " " + wordsB[b];
    }

    static const std::vector<std::pair<std::string, float>>& defaults()
    {
        static const std::vector<std::pair<std::string, float>> d = {
            { "wave", 0.0f }, { "tune", 0.0f }, { "fine", 0.0f }, { "drift", 0.3f },
            { "sub_level", 0.0f }, { "sub_wave", 0.0f }, { "sub_oct", 0.0f },
            { "flt_track", 0.0f }, { "acc_decay", 0.2f }, { "soft_attack", 3.0f }, { "sweep", 1.0f },
            { "pulse_width", 0.0f }, { "noise", 0.0f }, { "vca_decay", 2.5f }, { "fat", 0.15f }, { "amp_sus", 0.0f }, { "amp_rel", 4.0f },
            { "flt_att", 0.0f }, { "flt_sus", 0.0f }, { "flt_rel", 300.0f },
            { "cutoff", 420.0f }, { "reso", 0.75f }, { "envmod", 0.62f }, { "decay", 0.45f },
            { "accent", 0.75f }, { "slide_time", 60.0f },
            { "f2_type", 0.0f }, { "f2_cutoff", 1200.0f }, { "f2_reso", 0.4f }, { "f2_vowel", 0.0f }, { "f2_mix", 1.0f },
            { "pre_type", 0.0f }, { "pre_drive", 0.0f }, { "post_type", 0.0f }, { "post_drive", 0.3f },
            { "dist_tone", 6000.0f }, { "dist_mix", 1.0f },
            { "mod_mode", 0.0f }, { "mod_rate", 0.4f }, { "mod_depth", 0.5f }, { "mod_mix", 0.4f },
            { "dly_sync", 8.0f }, { "dly_fb", 0.4f }, { "dly_tone", 2800.0f }, { "dly_mix", 0.0f },
            { "rev_size", 0.35f }, { "rev_damp", 0.55f }, { "rev_mix", 0.0f }, { "width", 1.0f },
            { "seq_len", 16.0f }, { "seq_rate", 1.0f }, { "swing", 0.0f }, { "gate_len", 0.5f },
            { "root", 0.0f }, { "gen_scale", 0.0f }, { "gen_density", 0.75f }, { "gen_accent", 0.3f },
            { "gen_slide", 0.2f }, { "gen_octave", 0.25f },
        };
        return d;
    }

    static void set (std::vector<std::pair<std::string, float>>& vals, const std::string& id, float v)
    {
        for (auto& kv : vals)
            if (kv.first == id) { kv.second = v; return; }
        vals.emplace_back (id, v);
    }

    static float get (const std::vector<std::pair<std::string, float>>& vals, const std::string& id)
    {
        for (auto& kv : vals)
            if (kv.first == id) return kv.second;
        return 0.0f;
    }

    std::string category;
    std::vector<std::string> wordsA, wordsB;
    std::vector<Var> vars;
};

// Choice indices used below (must match the processor's choice lists).
// dist: 0 Tube 1 Fuzz 2 Crush 3 Fold 4 Overdrive 5 Distortion 6 SuperDist 7 RingMod 8 Rectifier 9 Diode | f2: 0 Off 1 Ladder 2 Formant | mod: 0 Off 1 Chorus 2 Flanger
// dly_sync: 0 1/64 1 1/32T 2 1/32 3 1/16T 4 1/16 5 1/16D 6 1/8T 7 1/8 8 1/8D 9 1/4T 10 1/4 11 1/4D 12 1/2 13 1/2D 14 1bar | seq_rate: 0 1/32 1 1/16 2 1/16T 3 1/8
// gen_scale: 0 Minor 1 Phrygian 2 Dorian 3 HarmMin 4 MinPenta 5 Blues 6 Major 7 PhrygDom

inline const std::vector<PresetTemplate>& presetTemplates()
{
    static const std::vector<PresetTemplate> t = [] {
        std::vector<PresetTemplate> v;
        const std::vector<float> roots { 0, 2, 4, 5, 7, 9, 10, 11 };

        v.push_back (PresetTemplate ("TB Classic",
            { "Acid", "Silver", "Chicago", "Warehouse", "Phuture", "Box", "Original", "Analog" },
            { "Line", "Tracks", "Jack", "Rain", "Pulse", "Groove", "Memory", "Machine" })
            .pick ("wave", { 0, 0, 1 }).expo ("cutoff", 380, 0.9f).lin ("reso", 0.76f, 0.12f)
            .lin ("envmod", 0.62f, 0.18f).expo ("decay", 0.45f, 0.9f).lin ("accent", 0.75f, 0.15f)
            .lin ("post_drive", 0.3f, 0.15f).pick ("post_type", { 0, 0, 4, 9, 10, 11 }).lin ("flt_track", 0.15f, 0.15f).lin ("fat", 0.2f, 0.12f).pick ("gen_scale", { 0, 1, 2, 4 }).lin ("gen_density", 0.75f, 0.12f)
            .lin ("gen_accent", 0.3f, 0.1f).lin ("gen_slide", 0.22f, 0.1f).lin ("swing", 0.08f, 0.08f).pick ("root", roots));

        v.push_back (PresetTemplate ("Squelch",
            { "Liquid", "Wet", "Rubber", "Gloopy", "Juicy", "Slippery", "Bubbling", "Melting" },
            { "Squelch", "Goo", "Drop", "Slime", "Ooze", "Squeeze", "Wobble", "Bloop" })
            .pick ("wave", { 0, 1 }).expo ("cutoff", 260, 0.7f).lin ("reso", 0.86f, 0.08f)
            .lin ("envmod", 0.75f, 0.15f).expo ("decay", 0.32f, 0.6f).lin ("accent", 0.85f, 0.12f)
            .lin ("pre_drive", 0.3f, 0.2f).lin ("post_drive", 0.35f, 0.15f).pick ("post_type", { 0, 4, 9, 11 }).expo ("sweep", 1.4f, 0.7f).expo ("slide_time", 70, 0.6f).lin ("fat", 0.2f, 0.12f)
            .lin ("gen_slide", 0.35f, 0.1f).lin ("gen_accent", 0.4f, 0.1f).pick ("gen_scale", { 0, 1, 3 }).pick ("root", roots));

        v.push_back (PresetTemplate ("Acid Techno",
            { "Brutal", "Warehouse", "Raw", "Distorted", "Rave", "Industrial", "Hard", "Pounding" },
            { "Screamer", "Saw", "Torture", "Riot", "Rage", "Hammer", "Fever", "Assault" })
            .pick ("wave", { 0, 0, 1 }).expo ("cutoff", 420, 0.8f).lin ("reso", 0.88f, 0.08f)
            .lin ("envmod", 0.65f, 0.2f).expo ("decay", 0.35f, 0.7f).lin ("accent", 0.8f, 0.15f)
            .pick ("pre_type", { 0, 1, 4 }).lin ("pre_drive", 0.5f, 0.2f).pick ("post_type", { 1, 5, 6, 9, 11, 13 })
            .lin ("post_drive", 0.6f, 0.2f).expo ("dist_tone", 4500, 0.7f).lin ("gen_density", 0.85f, 0.1f)
            .pick ("gen_scale", { 0, 1, 7 }).pick ("root", roots).lin ("rev_mix", 0.08f, 0.06f).lin ("fat", 0.3f, 0.2f));

        // Hardcore / hard acid: drive into the filter, resonance at or past self-oscillation,
        // huge fast sweeps, hard accents, then heavy distortion after the filter.
        v.push_back (PresetTemplate ("Hard Acid",
            { "Hardcore", "Gabber", "Rave", "Terror", "Frenchcore", "Mental", "Nuclear", "Psycho" },
            { "Acid", "Squelch", "Scream", "Blast", "Fury", "Machine", "Siren", "Meltdown" })
            .pick ("wave", { 0, 0, 1 }).expo ("cutoff", 500, 0.9f).lin ("reso", 0.93f, 0.07f)
            .lin ("envmod", 0.82f, 0.15f).expo ("decay", 0.22f, 0.6f).lin ("accent", 0.9f, 0.1f)
            .expo ("acc_decay", 0.15f, 0.5f).expo ("sweep", 1.6f, 0.6f).lin ("flt_track", 0.25f, 0.2f)
            .pick ("pre_type", { 1, 5, 6, 9, 11 }).lin ("pre_drive", 0.55f, 0.2f)
            .pick ("post_type", { 6, 5, 13, 1, 11 }).lin ("post_drive", 0.7f, 0.2f).expo ("dist_tone", 5500, 0.6f)
            .lin ("fat", 0.4f, 0.15f).lin ("gen_density", 0.9f, 0.08f).lin ("gen_accent", 0.45f, 0.1f)
            .lin ("gen_slide", 0.25f, 0.1f).pick ("gen_scale", { 1, 3, 7, 0 }).pick ("root", roots)
            .pick ("seq_rate", { 1, 1, 0 }).lin ("swing", 0.02f, 0.02f));

        v.push_back (PresetTemplate ("Deep Acid",
            { "Midnight", "Deep", "Submerged", "Velvet", "Hazy", "Slow", "Dark", "Hypnotic" },
            { "Waters", "Current", "Fog", "Drift", "Dream", "Tide", "Cave", "Mood" })
            .pick ("wave", { 0, 1 }).expo ("cutoff", 190, 0.6f).lin ("reso", 0.55f, 0.15f)
            .lin ("envmod", 0.35f, 0.15f).expo ("decay", 0.8f, 0.6f).lin ("accent", 0.5f, 0.2f)
            .lin ("sub_level", 0.4f, 0.2f).lin ("rev_mix", 0.15f, 0.08f).lin ("dly_mix", 0.15f, 0.1f)
            .pick ("dly_sync", { 7, 8, 10 }).lin ("gen_density", 0.6f, 0.12f).pick ("gen_scale", { 0, 2, 4 })
            .lin ("swing", 0.15f, 0.1f).pick ("root", roots));

        v.push_back (PresetTemplate ("Psy Acid",
            { "Cosmic", "Fractal", "Forest", "Astral", "Shaman", "Lunar", "Electric", "Twisted" },
            { "Spiral", "Portal", "Mantra", "Vortex", "Serpent", "Ritual", "Nebula", "Trance" })
            .pick ("wave", { 0, 1, 1 }).expo ("cutoff", 520, 0.8f).lin ("reso", 0.78f, 0.12f)
            .lin ("envmod", 0.7f, 0.15f).expo ("decay", 0.25f, 0.6f).lin ("accent", 0.7f, 0.15f)
            .lin ("post_drive", 0.4f, 0.15f).pick ("dly_sync", { 8, 6 }).lin ("dly_mix", 0.2f, 0.1f)
            .lin ("dly_fb", 0.45f, 0.15f).lin ("gen_density", 0.95f, 0.05f).lin ("gen_slide", 0.1f, 0.08f)
            .pick ("gen_scale", { 1, 7, 3 }).pick ("seq_len", { 16, 16, 15, 14, 12 }).pick ("root", { 4, 5, 6, 7 }));

        v.push_back (PresetTemplate ("Tech House Bass",
            { "Groovy", "Club", "Shuffled", "Bumpy", "Bouncy", "Late", "Ibiza", "Sunset" },
            { "Bassline", "Shuffle", "Bump", "Strut", "Swagger", "Hustle", "Roller", "Funk" })
            .pick ("wave", { 0, 1 }).expo ("cutoff", 300, 0.6f).lin ("reso", 0.3f, 0.15f)
            .lin ("envmod", 0.35f, 0.15f).expo ("decay", 0.3f, 0.5f).lin ("accent", 0.5f, 0.2f)
            .lin ("sub_level", 0.6f, 0.15f).lin ("post_drive", 0.2f, 0.1f).lin ("gen_density", 0.5f, 0.12f)
            .lin ("swing", 0.32f, 0.12f).lin ("gate_len", 0.4f, 0.1f).lin ("gen_octave", 0.15f, 0.1f)
            .lin ("gen_slide", 0.12f, 0.08f).pick ("gen_scale", { 4, 0, 5, 2 }).pick ("root", roots).lin ("fat", 0.35f, 0.2f).expo ("vca_decay", 0.8f, 0.8f));

        v.push_back (PresetTemplate ("Rolling Bass",
            { "Rolling", "Driving", "Endless", "Steady", "Pumping", "Running", "Locked", "Motor" },
            { "Engine", "Wheel", "Train", "Belt", "Chain", "Gear", "Loop", "Piston" })
            .pick ("wave", { 0, 1 }).expo ("cutoff", 260, 0.6f).lin ("reso", 0.35f, 0.15f)
            .lin ("envmod", 0.45f, 0.15f).expo ("decay", 0.2f, 0.5f).lin ("accent", 0.55f, 0.15f)
            .lin ("sub_level", 0.5f, 0.15f).lin ("gen_density", 0.92f, 0.06f).lin ("gate_len", 0.35f, 0.08f)
            .lin ("gen_octave", 0.3f, 0.15f).lin ("gen_slide", 0.08f, 0.06f).pick ("gen_scale", { 0, 1, 4 }).pick ("root", roots).lin ("fat", 0.25f, 0.2f));

        v.push_back (PresetTemplate ("Sub Bass",
            { "Heavy", "Low", "Massive", "Round", "Pure", "Earth", "Thick", "Solid" },
            { "Sub", "Weight", "Floor", "Pressure", "Rumble", "Bottom", "Foundation", "Quake" })
            .pick ("wave", { 1, 1, 0 }).expo ("cutoff", 150, 0.5f).lin ("reso", 0.12f, 0.1f)
            .lin ("envmod", 0.15f, 0.1f).expo ("decay", 1.2f, 0.5f).lin ("accent", 0.35f, 0.15f)
            .lin ("sub_level", 0.85f, 0.1f).pick ("sub_wave", { 0, 0, 1 }).pick ("sub_oct", { 0, 0, 1 }).lin ("post_drive", 0.08f, 0.08f)
            .lin ("gen_density", 0.5f, 0.12f).lin ("gen_octave", 0.05f, 0.05f).lin ("gen_slide", 0.3f, 0.1f)
            .lin ("gate_len", 0.7f, 0.15f).pick ("gen_scale", { 0, 4 }).pick ("root", roots).lin ("fat", 0.45f, 0.2f));

        v.push_back (PresetTemplate ("Moog Ladder Bass",
            { "Vintage", "Fat", "Warm", "Woody", "Classic", "Creamy", "Golden", "Studio" },
            { "Ladder", "Bass", "Seventies", "Funk", "Oak", "Model", "Tube", "Cabinet" })
            .pick ("wave", { 0, 1 }).expo ("cutoff", 700, 0.6f).lin ("reso", 0.3f, 0.15f)
            .lin ("envmod", 0.45f, 0.15f).expo ("decay", 0.5f, 0.6f).pick ("f2_type", { 1 })
            .expo ("f2_cutoff", 700, 0.8f).lin ("f2_reso", 0.4f, 0.2f).lin ("sub_level", 0.4f, 0.2f)
            .lin ("post_drive", 0.3f, 0.15f).expo ("dist_tone", 3500, 0.5f).lin ("gen_density", 0.65f, 0.12f)
            .pick ("gen_scale", { 2, 4, 5, 0 }).lin ("swing", 0.12f, 0.1f).pick ("root", roots).lin ("fat", 0.35f, 0.2f));

        v.push_back (PresetTemplate ("Electroclash",
            { "Neon", "Berlin", "Synthetic", "Glam", "Chrome", "Plastic", "Disco", "Robot" },
            { "Clash", "Punk", "Kiss", "Fashion", "Vinyl", "Strobe", "Pose", "Romance" })
            .pick ("wave", { 1, 1, 0 }).expo ("cutoff", 800, 0.7f).lin ("reso", 0.5f, 0.2f)
            .lin ("envmod", 0.5f, 0.2f).expo ("decay", 0.3f, 0.6f).pick ("post_type", { 1, 2, 7, 5, 13 }).lin ("pulse_width", 0.3f, 0.3f)
            .lin ("post_drive", 0.4f, 0.15f).pick ("f2_type", { 0, 1 }).expo ("f2_cutoff", 2500, 0.8f)
            .pick ("mod_mode", { 1, 2 }).lin ("mod_mix", 0.3f, 0.15f).lin ("width", 1.3f, 0.3f)
            .pick ("gen_scale", { 0, 5, 4 }).lin ("gen_octave", 0.35f, 0.15f).pick ("root", roots));

        v.push_back (PresetTemplate ("Talking Acid",
            { "Talking", "Whispering", "Chanting", "Speaking", "Singing", "Yelling", "Murmuring", "Preaching" },
            { "Vowel", "Mouth", "Voice", "Oracle", "Choir", "Throat", "Robot", "Ghost" })
            .pick ("wave", { 0, 1 }).expo ("cutoff", 600, 0.7f).lin ("reso", 0.6f, 0.2f)
            .lin ("envmod", 0.6f, 0.2f).expo ("decay", 0.4f, 0.6f).pick ("f2_type", { 2 })
            .lin ("f2_vowel", 2.0f, 2.0f).expo ("f2_cutoff", 1000, 0.7f).lin ("f2_reso", 0.6f, 0.2f)
            .lin ("f2_mix", 0.8f, 0.15f).lin ("post_drive", 0.25f, 0.15f).lin ("gen_slide", 0.3f, 0.1f)
            .pick ("gen_scale", { 1, 0, 3 }).pick ("root", roots));

        v.push_back (PresetTemplate ("Metal Fold",
            { "Steel", "Chrome", "Iron", "Razor", "Titanium", "Molten", "Jagged", "Forged" },
            { "Fold", "Blade", "Edge", "Shard", "Spike", "Grinder", "Anvil", "Wire" })
            .pick ("wave", { 0, 1 }).expo ("cutoff", 600, 0.8f).lin ("reso", 0.7f, 0.15f)
            .lin ("envmod", 0.6f, 0.2f).expo ("decay", 0.35f, 0.6f).pick ("post_type", { 3, 7, 8 })
            .lin ("post_drive", 0.5f, 0.25f).expo ("dist_tone", 9000, 0.5f).lin ("dist_mix", 0.85f, 0.15f)
            .pick ("pre_type", { 0, 3 }).lin ("pre_drive", 0.15f, 0.15f).pick ("gen_scale", { 1, 7, 3 }).pick ("root", roots));

        v.push_back (PresetTemplate ("Lo-Fi Crush",
            { "Dusty", "Broken", "8-Bit", "Crunchy", "Grainy", "Cheap", "Bent", "Old" },
            { "Crusher", "Cassette", "Console", "Chip", "Sampler", "Toy", "Radio", "Tape" })
            .pick ("wave", { 0, 1 }).expo ("cutoff", 500, 0.7f).lin ("reso", 0.6f, 0.2f)
            .lin ("envmod", 0.5f, 0.2f).expo ("decay", 0.35f, 0.6f).pick ("post_type", { 2, 2, 10, 13 }).lin ("noise", 0.25f, 0.15f)
            .lin ("post_drive", 0.4f, 0.2f).expo ("dist_tone", 3000, 0.6f).lin ("rev_mix", 0.18f, 0.1f)
            .pick ("gen_scale", { 0, 4, 5 }).lin ("swing", 0.15f, 0.12f).pick ("root", roots));

        v.push_back (PresetTemplate ("Dub Acid",
            { "Dub", "Echo", "Spacey", "Roots", "Smoky", "Haunted", "Distant", "Rainy" },
            { "Chamber", "Siren", "Station", "Shadow", "Reflection", "Tunnel", "Horizon", "Canyon" })
            .pick ("wave", { 0, 1 }).expo ("cutoff", 300, 0.7f).lin ("reso", 0.65f, 0.15f)
            .lin ("envmod", 0.5f, 0.2f).expo ("decay", 0.6f, 0.6f).lin ("dly_mix", 0.35f, 0.1f)
            .lin ("dly_fb", 0.6f, 0.12f).expo ("dly_tone", 1800, 0.5f).pick ("dly_sync", { 5, 8, 11 })
            .lin ("rev_mix", 0.25f, 0.1f).lin ("rev_size", 0.6f, 0.2f).lin ("gen_density", 0.55f, 0.12f)
            .lin ("swing", 0.2f, 0.1f).pick ("gen_scale", { 0, 2, 4 }).pick ("root", roots));

        v.push_back (PresetTemplate ("Lead & Pluck",
            { "Bright", "Sharp", "Shiny", "Glassy", "Laser", "Sparkling", "Hyper", "Crystal" },
            { "Lead", "Pluck", "Zap", "Stab", "Arp", "Spark", "Flash", "Hook" })
            .pick ("tune", { 12, 12, 24 }).pick ("wave", { 0, 1 }).expo ("cutoff", 1200, 0.7f)
            .lin ("reso", 0.5f, 0.2f).lin ("envmod", 0.55f, 0.2f).expo ("decay", 0.2f, 0.5f)
            .pick ("mod_mode", { 1, 2 }).lin ("mod_mix", 0.4f, 0.15f).lin ("dly_mix", 0.25f, 0.1f).expo ("vca_decay", 0.5f, 0.8f)
            .pick ("dly_sync", { 6, 8 }).lin ("width", 1.4f, 0.3f).lin ("rev_mix", 0.15f, 0.08f)
            .lin ("gen_octave", 0.4f, 0.15f).pick ("gen_scale", { 0, 6, 3, 2 }).pick ("root", roots));

        v.push_back (PresetTemplate ("FX & Weird",
            { "Alien", "Glitched", "Mutant", "Warped", "Unstable", "Haywire", "Feral", "Quantum" },
            { "Signal", "Insect", "Transmission", "Anomaly", "Probe", "Static", "Creature", "Error" })
            .pick ("wave", { 0, 1 }).expo ("cutoff", 700, 1.2f).lin ("reso", 0.9f, 0.08f)
            .lin ("envmod", 0.7f, 0.25f).expo ("decay", 0.3f, 1.0f).lin ("drift", 0.7f, 0.25f)
            .pick ("post_type", { 1, 2, 3, 7, 8 }).lin ("post_drive", 0.5f, 0.3f).pick ("mod_mode", { 2 })
            .lin ("mod_mix", 0.5f, 0.2f).lin ("dly_mix", 0.2f, 0.15f).pick ("seq_rate", { 0, 1, 2 })
            .pick ("seq_len", { 16, 13, 11, 7, 15 }).lin ("gen_octave", 0.5f, 0.2f)
            .pick ("gen_scale", { 7, 3, 1, 5 }).pick ("root", roots));

        return v;
    }();
    return t;
}

constexpr int kPresetsPerCategory = 32;

inline int numFactoryPresets() { return (int) presetTemplates().size() * kPresetsPerCategory; }

inline Preset factoryPreset (int index)
{
    const auto& t = presetTemplates();
    index = std::clamp (index, 0, numFactoryPresets() - 1);
    const int cat = index / kPresetsPerCategory;
    const int i = index % kPresetsPerCategory;
    // 1.4: each category's knobs wander 40% further than their base spread, for variety.
    return t[(size_t) cat].make (i, 0x303u * 7919u + (uint32_t) index * 2654435761u, 1.4f);
}

/** A fresh sound in the style of a random category, pushed a bit further than the factory spread. */
inline Preset randomPreset (uint32_t seed)
{
    const auto& t = presetTemplates();
    const auto& tpl = t[(size_t) (seed % t.size())];
    auto p = tpl.make ((int) (seed % 64), seed * 2246822519u + 1u, 1.35f);
    p.name = "Random " + tpl.getCategory();
    return p;
}
} // namespace s303
