#include "Presets.h"

namespace midigod
{
namespace
{
//                                    bass acid lead pluck arp chords pad counter drums
const float kGate[NumRoles]       = { 60,  70,  80,  40,   60,  60,    100, 80,     70 };
const float kDensity[NumRoles]    = { 70,  65,  60,  60,   80,  60,    50,  50,     70 };
const int   kBars[NumRoles]       = { 1,   1,   2,   2,    2,   2,     2,   2,      1 }; // 2 BARS / 4 BARS

skazi::Preset make (int genre, int role, const juce::String& name)
{
    skazi::Preset p { name, genreInfo (genre).title, skazi::defaultValues (paramDefs()) };
    auto& v = p.values;
    const bool psy = genre == FullOn || genre == ProgPsy || genre == PsyTech;
    v["genre"] = (float) genre;
    v["role"] = (float) role;
    v["bars"] = (float) kBars[role];
    v["swing"] = genreInfo (genre).swing * 100.0f;
    v["gate"] = kGate[role];
    v["density"] = kDensity[role] + (psy && role == Lead ? 15.0f : 0.0f) - (genre == DeepHouse ? 10.0f : 0.0f);
    v["move"] = genre == Trance || genre == Edm || genre == MelodicTechno ? 60.0f : 45.0f;
    v["dyn"] = role == Drums || role == Acid ? 75.0f : 60.0f;
    v["seed"] = (float) (1 + (genre * 37 + role * 11) % 97);
    if (genre == DeepHouse && role != Drums) v["human"] = 20.0f;
    return p;
}

std::vector<skazi::Preset> build()
{
    std::vector<skazi::Preset> list;
    list.push_back ({ "Init", "FACTORY", skazi::defaultValues (paramDefs()) });
    for (int g = 0; g < NumGenres; ++g)
    {
        for (int r = 0; r < NumRoles; ++r)
            list.push_back (make (g, r, roleTitle (r)));

        // a few signature extras per genre
        switch (g)
        {
            case FullOn:
            {
                auto p = make (g, Bass, "Triplet Gallop Bass"); p.values["grid"] = 1; list.push_back (p);
                p = make (g, Lead, "Freygish Lead"); p.values["scale"] = Freygish; p.values["move"] = 65; list.push_back (p);
                p = make (g, Arp, "Phrygian Arp"); p.values["scale"] = Phrygian; list.push_back (p);
                break;
            }
            case ProgPsy:
            {
                auto p = make (g, Bass, "Rolling Bass Full"); p.values["density"] = 90; list.push_back (p);
                p = make (g, Pluck, "Hypnotic Pluck"); p.values["move"] = 25; p.values["density"] = 45; list.push_back (p);
                break;
            }
            case PsyTech:
            {
                auto p = make (g, Acid, "Phrygian Acid"); p.values["scale"] = Phrygian; p.values["move"] = 70; list.push_back (p);
                break;
            }
            case TechHouse:
            {
                auto p = make (g, Bass, "Swing Bass"); p.values["swing"] = 62; list.push_back (p);
                p = make (g, Drums, "Shuffle Groove"); p.values["swing"] = 60; p.values["density"] = 85; list.push_back (p);
                break;
            }
            case DeepHouse:
            {
                auto p = make (g, Chords, "Dorian Stabs"); p.values["scale"] = Dorian; list.push_back (p);
                break;
            }
            case MelodicTechno:
            {
                auto p = make (g, Arp, "Wide Arp"); p.values["move"] = 85; p.values["bars"] = 3; list.push_back (p);
                p = make (g, Lead, "Harmonic Minor Lead"); p.values["scale"] = HarmMinor; list.push_back (p);
                break;
            }
            case HardTechno:
            {
                auto p = make (g, Bass, "Rumble Bass"); p.values["gate"] = 95; p.values["density"] = 90; list.push_back (p);
                break;
            }
            case Trance:
            {
                auto p = make (g, Pluck, "Uplifting Pluck"); p.values["scale"] = Major; p.values["bars"] = 3; list.push_back (p);
                p = make (g, Chords, "Pumping Chords"); p.values["density"] = 80; list.push_back (p);
                break;
            }
            case Edm:
            {
                auto p = make (g, Lead, "Festival Lead"); p.values["scale"] = Major; p.values["move"] = 70; list.push_back (p);
                break;
            }
            default: break;
        }
    }

    // ROLL BASS: rolling basslines with octave jumps, pitch dips and fills
    struct Roll { int genre, style, scale, bars; float sync, kick, oct, dip, var, evolve; int mut; const char* name; float glide, groove; };
    static const Roll rolls[] = {
        { FullOn,        Roller,  Phrygian,  2, 10, 100, 35,  0, 30,  0,   0, "TLV Roller", 20, 20 },
        { FullOn,        Gallop,  Freygish,  2, 10, 100, 25,  0, 30,  0,   0, "Goa Gallop", 0, 10 },
        { ProgPsy,       OffRoll, Minor,     3, 15, 100, 20,  0, 20, 50,   0, "Night Prog Roll", 30, 25 },
        { PsyTech,       OffRoll, Phrygian,  2, 30, 100, 40, 30, 30,  0,   0, "Psy-Tech Chug", 10, 40 },
        { FullOn,        Roller,  Minor,     2, 10, 100, 30, 60, 30,  0,   0, "Dipping Roller", 0, 15 },
        { PsyTech,       OffRoll, Minor,     2, 20, 100, 80,  0, 35,  0,   0, "Octave Bouncer", 0, 20 },
        { ProgPsy,       Broken,  Phrygian,  2, 60, 100, 30, 20, 50,  0,   0, "Broken Psy", 25, 45 },
        { HardTechno,    Roller,  Minor,     2, 15, 100, 20, 20, 30,  0,   0, "Hard Roll", 10, 20 },
        { HardTechno,    OffRoll, Phrygian,  2, 10, 100, 15, 80, 25,  0,   0, "Rumble Dive", 0, 15 },
        { MelodicTechno, Roller,  Minor,     3, 20, 100, 25, 10, 40, 40,   0, "Melodic Roll", 35, 30 },
        { Trance,        Offbeat, Minor,     2,  5, 100, 30, 45, 30,  0,   0, "Trance Offbeat Dip", 15, 10 },
        { TechHouse,     Broken,  Minor,     2, 55, 100, 25, 15, 40,  0,   0, "Tech House Bouncer", 20, 50 },
        { BassHouse,     OffRoll, Minor,     2, 30, 100, 50, 70, 35,  0,   0, "Bass House Diver", 30, 35 },
        { DeepHouse,     Broken,  Dorian,    2, 40, 100, 15, 10, 30,  0,   0, "Deep Broken Line", 25, 45 },
        { FullOn,        Roller,  Phrygian,  3, 15, 100, 40, 30, 40, 85,   0, "Evolving Roller", 25, 30 },
        { PsyTech,       Roller,  Minor,     2, 70,  60, 30, 20, 40,  0,   0, "Syncopated Roll", 15, 40 },
        { FullOn,        Roller,  Minor,     2, 40,   0, 20,  0, 30,  0,   0, "Kick Hugger", 0, 20 },
        { ProgPsy,       Roller,  Phrygian,  2, 25, 100, 35, 25, 60,  0, 137, "Mutant Roll", 30, 35 },
        { FullOn,        Gallop,  Hungarian, 2, 15, 100, 30, 30, 35,  0,   0, "Hungarian Gallop", 10, 20 },
        { MelodicTechno, OffRoll, Dorian,    3, 20, 100, 20, 25, 35, 30,   0, "Dark Dorian Roll", 40, 30 },
    };
    for (const auto& r : rolls)
    {
        auto p = make (r.genre, Bass, r.name);
        p.category = "ROLL BASS";
        auto& v = p.values;
        v["rbStyle"] = (float) r.style;
        v["scale"] = (float) r.scale;
        v["bars"] = (float) r.bars;
        v["rbSync"] = r.sync;
        v["rbKick"] = r.kick;
        v["rbOct"] = r.oct;
        v["rbDip"] = r.dip;
        v["rbVar"] = r.var;
        v["rbEvolve"] = r.evolve;
        v["rbMut"] = (float) r.mut;
        v["rbGlide"] = r.glide;
        v["rbGroove"] = r.groove;
        list.push_back (p);
    }
    return list;
}
} // namespace

const std::vector<skazi::Preset>& presets()
{
    static const auto list = build();
    return list;
}

skazi::Preset randomPreset (juce::Random& rng)
{
    skazi::Preset p { "Random", "RANDOM", skazi::defaultValues (paramDefs()) };
    skazi::PresetGen pg { rng, p.values };
    const int genre = rng.nextInt (NumGenres), role = rng.nextInt (NumRoles);
    const bool psy = genre == FullOn || genre == ProgPsy || genre == PsyTech;
    pg.set ("genre", (float) genre);
    pg.set ("role", (float) role);
    pg.set ("scale", (float) (pg.chance (0.6f) ? Minor : pg.pick ({ Phrygian, HarmMinor, Dorian, Freygish, Hungarian, Pentatonic, Major })));
    pg.set ("bars", (float) pg.pick ({ 1, 2, 2, 3 }));
    pg.set ("grid", psy && pg.chance (0.2f) ? 1.0f : 0.0f);
    pg.set ("density", std::round (pg.rr (35.0f, 95.0f)));
    pg.set ("swing", std::round (genreInfo (genre).swing * 100.0f + (genre >= TechHouse && genre <= BassHouse ? pg.rr (-2.0f, 6.0f) : 0.0f)));
    pg.set ("gate", std::round (pg.rr (30.0f, 95.0f)));
    pg.set ("dyn", std::round (pg.rr (30.0f, 90.0f)));
    pg.set ("move", std::round (pg.rr (20.0f, 90.0f)));
    pg.set ("human", pg.chance (0.3f) ? std::round (pg.rr (5.0f, 30.0f)) : 0.0f);
    pg.set ("seed", (float) (1 + rng.nextInt (999)));
    if (role == Bass)
    {
        pg.set ("rbStyle", (float) rng.nextInt (NumRollStyles));
        pg.set ("rbSync", std::round (pg.rr (0.0f, 70.0f)));
        pg.set ("rbKick", pg.chance (0.85f) ? 100.0f : std::round (pg.rr (0.0f, 80.0f)));
        pg.set ("rbOct", std::round (pg.rr (0.0f, 70.0f)));
        pg.set ("rbDip", pg.chance (0.5f) ? std::round (pg.rr (10.0f, 80.0f)) : 0.0f);
        pg.set ("rbVar", std::round (pg.rr (10.0f, 70.0f)));
        pg.set ("rbEvolve", pg.chance (0.3f) ? std::round (pg.rr (30.0f, 90.0f)) : 0.0f);
        pg.set ("rbGlide", pg.chance (0.6f) ? std::round (pg.rr (10.0f, 60.0f)) : 0.0f);
        pg.set ("rbGroove", std::round (pg.rr (0.0f, 60.0f)));
    }
    return p;
}
} // namespace midigod
