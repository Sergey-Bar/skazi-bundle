#include "Pattern.h"
#include <algorithm>
#include <array>
#include <cstring>
#include <random>

namespace midigod
{
using skazi::ParamDef;

const GenreInfo& genreInfo (int g)
{
    static const GenreInfo info[NumGenres] = {
        { "FULL-ON",        "Full-On Psy",    145.0, 0.50f },
        { "PROG PSY",       "Progressive Psy", 138.0, 0.50f },
        { "PSY-TECH",       "Psy-Tech",       140.0, 0.52f },
        { "TECH HOUSE",     "Tech House",     126.0, 0.56f },
        { "DEEP HOUSE",     "Deep House",     122.0, 0.58f },
        { "BASS HOUSE",     "Bass House",     126.0, 0.54f },
        { "MELODIC TECHNO", "Melodic Techno", 124.0, 0.50f },
        { "HARD TECHNO",    "Hard Techno",    150.0, 0.50f },
        { "TRANCE",         "Trance",         138.0, 0.50f },
        { "EDM",            "EDM",            128.0, 0.50f },
    };
    return info[juce::jlimit (0, NumGenres - 1, g)];
}

const char* roleTitle (int r)
{
    static const char* const t[NumRoles] = { "Bass", "Acid", "Lead", "Pluck", "Arp", "Chords", "Pad", "Counter", "Drums" };
    return t[juce::jlimit (0, NumRoles - 1, r)];
}

const char* keyName (int k)
{
    static const char* const n[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    return n[((k % 12) + 12) % 12];
}

const char* scaleTitle (int s)
{
    static const char* const t[NumScales] = { "Minor", "Major", "Phrygian", "Harmonic Minor", "Dorian", "Freygish", "Hungarian", "Pentatonic" };
    return t[juce::jlimit (0, NumScales - 1, s)];
}

const char* modeTitle (int m)
{
    static const char* const t[NumModes] = { "Melody", "Bass", "Drums", "Chords" };
    return t[juce::jlimit (0, NumModes - 1, m)];
}

const char* rollStyleTitle (int s)
{
    static const char* const t[NumRollStyles] = { "AUTO", "ROLLER", "OFF-ROLL", "GALLOP", "OFFBEAT", "BROKEN" };
    return t[juce::jlimit (0, NumRollStyles - 1, s)];
}

const std::vector<int>& scaleSteps (int s)
{
    static const std::vector<int> steps[NumScales] = {
        { 0, 2, 3, 5, 7, 8, 10 }, // minor
        { 0, 2, 4, 5, 7, 9, 11 }, // major
        { 0, 1, 3, 5, 7, 8, 10 }, // phrygian
        { 0, 2, 3, 5, 7, 8, 11 }, // harmonic minor
        { 0, 2, 3, 5, 7, 9, 10 }, // dorian
        { 0, 1, 4, 5, 7, 8, 10 }, // freygish (phrygian dominant)
        { 0, 2, 3, 6, 7, 8, 11 }, // hungarian minor
        { 0, 3, 5, 7, 10 },       // minor pentatonic
    };
    return steps[juce::jlimit (0, NumScales - 1, s)];
}

std::vector<ParamDef> paramDefs()
{
    juce::StringArray genres, roles, keys, scales, styles;
    for (int st = 0; st < NumRollStyles; ++st) styles.add (rollStyleTitle (st));
    for (int g = 0; g < NumGenres; ++g) genres.add (genreInfo (g).label);
    for (int r = 0; r < NumRoles; ++r) roles.add (juce::String (roleTitle (r)).toUpperCase());
    for (int k = 0; k < 12; ++k) keys.add (keyName (k));
    for (int s = 0; s < NumScales; ++s) scales.add (juce::String (scaleTitle (s)).toUpperCase());

    return {
        { "genre",   "Genre",       ParamDef::Choice, 0, NumGenres - 1, FullOn, "", 1, 1, genres },
        { "role",    "Role",        ParamDef::Choice, 0, NumRoles - 1, Bass, "", 1, 1, roles },
        { "key",     "Key",         ParamDef::Choice, 0, 11, 6, "", 1, 1, keys },
        { "scale",   "Scale",       ParamDef::Choice, 0, NumScales - 1, Minor, "", 1, 1, scales },
        { "bars",    "Bars",        ParamDef::Choice, 0, 3, 2, "", 1, 1, { "1 BAR", "2 BARS", "4 BARS", "8 BARS" } },
        { "grid",    "Grid",        ParamDef::Choice, 0, 1, 0, "", 1, 1, { "1/16", "TRIPLET" } },
        { "octave",  "Octave",      ParamDef::Int,    -2, 2, 0, "oct", 1 },
        { "density", "Density",     ParamDef::Float,  0, 100, 65, "%", 1 },
        { "swing",   "Swing",       ParamDef::Float,  50, 75, 50, "%", 1 },
        { "gate",    "Gate",        ParamDef::Float,  10, 100, 70, "%", 1 },
        { "dyn",     "Dynamics",    ParamDef::Float,  0, 100, 60, "%", 1 },
        { "human",   "Humanize",    ParamDef::Float,  0, 100, 0, "%", 1 },
        { "move",    "Movement",    ParamDef::Float,  0, 100, 50, "%", 1 },
        { "seed",    "Variation",   ParamDef::Int,    1, 999, 1, "", 1 },

        { "a2mMode",  "Audio Mode",     ParamDef::Choice, 0, NumModes - 1, Melody, "", 1, 1, { "MELODY", "BASS", "DRUMS", "CHORDS" } },
        { "a2mQuant", "Audio Quantize", ParamDef::Choice, 0, 4, 2, "", 1, 1, { "OFF", "1/8", "1/16", "1/16T", "1/32" } },
        { "a2mSnap",  "Snap To Key",    ParamDef::Bool,   0, 1, 0 },
        { "a2mSens",  "Sensitivity",    ParamDef::Float,  0, 100, 50, "%", 1 },

        { "vol",     "Preview Volume", ParamDef::Float, -60, 0, -12, "dB", 0.1f },
        { "midiOut", "MIDI Out",       ParamDef::Bool, 0, 1, 1 },
        { "freeRun", "Free Run",       ParamDef::Bool, 0, 1, 0 },

        { "rbStyle",  "Roll Style",     ParamDef::Choice, 0, NumRollStyles - 1, RollAuto, "", 1, 1, styles },
        { "rbSync",   "Syncopation",    ParamDef::Float,  0, 100, 15, "%", 1 },
        { "rbKick",   "Kick Avoid",     ParamDef::Float,  0, 100, 100, "%", 1 },
        { "rbOct",    "Octave Jump",    ParamDef::Float,  0, 100, 20, "%", 1 },
        { "rbDip",    "Pitch Dip",      ParamDef::Float,  0, 100, 0, "%", 1 },
        { "rbVar",    "Bass Variation", ParamDef::Float,  0, 100, 30, "%", 1 },
        { "rbEvolve", "Evolve",         ParamDef::Float,  0, 100, 0, "%", 1 },
        { "rbMut",    "Mutation",       ParamDef::Int,    0, 999, 0, "", 1 },
        { "rbGlide",  "Glide",          ParamDef::Float,  0, 100, 0, "%", 1 },
        { "rbGroove", "Groove",         ParamDef::Float,  0, 100, 0, "%", 1 },
    };
}

namespace
{
struct Rng
{
    explicit Rng (juce::uint32 seed) : g (seed) {}
    float f() { return std::uniform_real_distribution<float> (0.0f, 1.0f) (g); }
    int i (int n) { return n <= 1 ? 0 : std::uniform_int_distribution<int> (0, n - 1) (g); }
    bool chance (float p) { return f() < p; }
    template <typename T> const T& pick (const std::vector<T>& v) { return v[(size_t) i ((int) v.size())]; }
    std::mt19937 g;
};

int floorDiv (int a, int b) { return a >= 0 ? a / b : -((-a + b - 1) / b); }

enum Family { Psy, House, Techno, Anthem };
Family familyOf (int genre)
{
    switch (genre)
    {
        case FullOn: case ProgPsy: case PsyTech: return Psy;
        case TechHouse: case DeepHouse: case BassHouse: return House;
        case MelodicTechno: case HardTechno: return Techno;
        default: return Anthem;
    }
}

using Prog = std::array<int, 4>; // chord root as a scale degree, one per bar

const std::vector<Prog>& progressions (int genre)
{
    static const std::vector<Prog> psy = { { 0, 0, 0, 0 }, { 0, 0, 0, 5 }, { 0, 0, 0, 6 }, { 0, 0, 5, 6 }, { 0, 0, 1, 0 } };
    static const std::vector<Prog> psyTech = { { 0, 0, 0, 0 }, { 0, 0, 5, 5 }, { 0, 0, 3, 3 }, { 0, 0, 0, 6 } };
    static const std::vector<Prog> house = { { 0, 0, 0, 0 }, { 0, 0, 5, 6 }, { 0, 0, 3, 3 }, { 0, 0, 0, 4 } };
    static const std::vector<Prog> deep = { { 0, 3, 0, 3 }, { 0, 5, 3, 4 }, { 1, 4, 0, 0 }, { 0, 6, 5, 4 }, { 0, 0, 3, 3 } };
    static const std::vector<Prog> melodic = { { 0, 5, 2, 6 }, { 0, 0, 5, 5 }, { 0, 3, 5, 4 }, { 0, 2, 5, 6 }, { 0, 5, 3, 6 } };
    static const std::vector<Prog> trance = { { 0, 5, 2, 6 }, { 0, 5, 3, 4 }, { 5, 6, 0, 0 }, { 0, 3, 5, 4 }, { 0, 6, 5, 6 } };
    static const std::vector<Prog> edm = { { 0, 5, 2, 6 }, { 5, 6, 0, 0 }, { 0, 4, 5, 3 }, { 3, 4, 5, 5 }, { 0, 5, 3, 6 } };
    switch (genre)
    {
        case FullOn: case ProgPsy: case HardTechno: return psy;
        case PsyTech: return psyTech;
        case TechHouse: case BassHouse: return house;
        case DeepHouse: return deep;
        case MelodicTechno: return melodic;
        case Trance: return trance;
        default: return edm;
    }
}

struct Gen
{
    Gen (const Settings& st, juce::uint32 salt)
        : s (st), r ((juce::uint32) st.seed * 7919u + (juce::uint32) st.genre * 131u + (juce::uint32) st.role * 17u + salt),
          steps (scaleSteps (st.scale)), family (familyOf (st.genre))
    {
        spb = s.triplet ? 3 : 4;
        stepLen = 1.0 / spb;
        perBar = 4 * spb;
        prog = r.pick (progressions (s.genre));
    }

    // scale degree (any integer, 7 = next octave) -> MIDI note above `base` (a C)
    int pitch (int deg, int base) const
    {
        const int n = (int) steps.size();
        const int oct = floorDiv (deg, n);
        return base + s.key + oct * 12 + steps[(size_t) (deg - oct * n)];
    }

    int chord (int bar) const { return prog[(size_t) (bar % 4)]; }
    int degreesPerOctave() const { return (int) steps.size(); }

    // accent 0..1 -> velocity; DYNAMICS spreads accents and ghosts apart
    int vel (float accent) const { return juce::jlimit (1, 127, juce::roundToInt (juce::jmap (s.dynamics, 104.0f, 58.0f + 69.0f * accent))); }

    void add (std::vector<Note>& out, int p, double start, double len, int v) const
    {
        out.push_back ({ juce::jlimit (0, 127, p), start, juce::jmax (1.0 / 64.0, len), v });
    }

    // nearest chord tone (root, 3rd, 5th, optionally 7th) of the chord on `root` to degree `deg`
    int snapToChord (int deg, int root, bool seventh = false) const
    {
        const int n = degreesPerOctave();
        int best = deg, bestDist = 1000;
        for (int oct = -2; oct <= 3; ++oct)
            for (int t : { 0, 2, 4, 6 })
            {
                if (t == 6 && ! seventh) continue;
                const int cand = root + t + oct * n;
                const int d = std::abs (cand - deg);
                if (d < bestDist || (d == bestDist && cand < best)) { bestDist = d; best = cand; }
            }
        return best;
    }

    const Settings& s;
    Rng r;
    const std::vector<int>& steps;
    Family family;
    int spb = 4, perBar = 16;
    double stepLen = 0.25;
    Prog prog;
};

// bass roots land at 33-62 Hz (C0-B0 with C3 = 60), where psy and house subs sit
constexpr int kBassBase = 24, kAcidBase = 36, kMidBase = 60, kLeadBase = 72;

// 16-step pattern strings: '.' rest, 'r' root, 'o' octave up, '5' fifth, '7' seventh, '3' third, 'g' ghost root, '-' tie
void playTemplate (Gen& g, std::vector<Note>& out, const char* t, int bar, int root, int base, float gateScale)
{
    const int n = (int) std::strlen (t);
    for (int i = 0; i < n; ++i)
    {
        const char c = t[i];
        if (c == '.' || c == '-') continue;
        int len = 1;
        while (i + len < n && t[i + len] == '-') ++len;
        int deg = root;
        float acc = (i % 4 == 2) ? 0.9f : 0.75f;
        switch (c)
        {
            case 'o': deg = root + g.degreesPerOctave(); acc = 1.0f; break;
            case '5': deg = root + 4; break;
            case '7': deg = root + 6 - g.degreesPerOctave(); break;
            case '3': deg = root + 2; break;
            case 'g': acc = 0.2f; break;
            default: break;
        }
        if (c == 'g' && ! g.r.chance (g.s.density)) continue;
        const double step = 0.25;
        g.add (out, g.pitch (deg, base), bar * 4.0 + i * step, step * len * gateScale, g.vel (acc));
    }
}

//==============================================================================
// ROLL BASS: a one-bar motif around a four-on-the-floor kick, repeated over the loop. Later bars drift from it
// (VARIATION), the loop can build up (EVOLVE), phrase ends get fills, and notes jump octaves or dip in pitch.
struct RollStep { bool on = false; int deg = 0; float acc = 0.7f, dip = 0.0f; int len = 1; bool ghost = false; };

int autoRollStyle (const Settings& s)
{
    switch (s.genre)
    {
        case FullOn: case HardTechno: return Roller;
        case ProgPsy: return s.density > 0.8f ? Roller : OffRoll;
        case PsyTech: case TechHouse: case BassHouse: return OffRoll;
        case MelodicTechno: return s.density > 0.5f ? Roller : OffRoll;
        case DeepHouse: return Broken;
        case Trance: return s.density > 0.6f ? OffRoll : Offbeat;
        default: return Offbeat; // EDM
    }
}

void rollBass (Gen& g, std::vector<Note>& out, juce::String& detail)
{
    const auto& s = g.s;
    const int style = s.rollStyle == RollAuto ? autoRollStyle (s) : s.rollStyle;
    const bool tri = s.triplet || style == Gallop;
    const int spb = tri ? 3 : 4, perBar = 4 * spb, oct = g.degreesPerOctave();
    const double stepLen = 1.0 / spb, gateScale = 0.35 + 0.6 * s.gate;
    auto kickStep = [spb] (int i) { return i % spb == 0; };

    // which steps of each beat carry the bass (the kick owns step 0)
    std::vector<int> slots;
    if (tri) slots = { 1, 2 };
    else switch (style)
    {
        case Roller: case Broken: slots = { 1, 2, 3 }; break;
        case OffRoll: slots = { 2, 3 }; break;
        default: slots = { 2 }; break; // offbeat
    }

    std::vector<RollStep> motif ((size_t) perBar);
    for (int beat = 0; beat < 4; ++beat)
    {
        auto beatSlots = slots;
        if (style == Broken) // a different handful of steps every beat, never none
        {
            beatSlots.clear();
            for (int k : slots) if (g.r.chance (0.35f + 0.5f * s.density)) beatSlots.push_back (k);
            if (beatSlots.empty()) beatSlots.push_back (g.r.pick (slots));
        }
        for (size_t k = 0; k < beatSlots.size(); ++k)
        {
            if (k == 0 && beatSlots.size() == 3 && s.density < 0.4f && g.r.chance (0.6f - s.density)) continue;
            auto& st = motif[(size_t) (beat * spb + beatSlots[k])];
            st.on = true;
            st.acc = beatSlots.size() == 1 ? 0.9f : 0.65f + 0.35f * (float) k / (float) (beatSlots.size() - 1);
        }
    }

    // SYNCOPATION: extra notes off the usual slots, notes nudged a step, and (with KICK AVOID down) hits on the kick
    for (int i = 0; i < perBar; ++i)
    {
        auto& st = motif[(size_t) i];
        if (st.on) continue;
        if (kickStep (i) ? g.r.chance ((1.0f - s.kickAvoid) * (0.2f + 0.6f * s.syncopation)) : g.r.chance (0.35f * s.syncopation * s.density))
        {
            st.on = true;
            st.acc = kickStep (i) ? 0.85f : 0.5f;
        }
    }
    for (int i = 0; i < perBar; ++i)
    {
        const int to = i + (g.r.chance (0.5f) ? 1 : -1);
        if (! motif[(size_t) i].on || to < 0 || to >= perBar || motif[(size_t) to].on || (kickStep (to) && s.kickAvoid > 0.5f)) continue;
        if (g.r.chance (0.3f * s.syncopation)) std::swap (motif[(size_t) i], motif[(size_t) to]);
    }

    // OCTAVE JUMP favours the first note of the bar and the last of a beat; MOVEMENT adds the odd fifth
    bool first = true;
    for (int i = 0; i < perBar; ++i)
    {
        auto& st = motif[(size_t) i];
        if (! st.on) continue;
        const bool lastInBeat = i % spb == spb - 1 || i + 1 == perBar || ! motif[(size_t) (i + 1)].on;
        if (g.r.chance (s.octaveJump * (first ? 1.0f : lastInBeat ? 0.55f : 0.25f))) { st.deg = oct; st.acc = 1.0f; }
        else if (g.r.chance (0.08f * s.movement)) st.deg = 4;
        first = false;
    }

    // PITCH DIP lands on notes with room after them (or the last note before a kick), and the dipped note rings
    // into the next step so the dive is heard; a high PITCH DIP clears that step in a full roll
    for (int i = 0; i < perBar; ++i)
    {
        auto& st = motif[(size_t) i];
        if (! st.on) continue;
        const bool nextFree = i + 1 < perBar && ! kickStep (i + 1);
        const bool room = nextFree && ! motif[(size_t) (i + 1)].on;
        if (g.r.chance (s.pitchDip * (room ? 0.8f : ! nextFree ? 0.45f : 0.2f)))
        {
            st.dip = juce::jlimit (0.05f, 1.0f, (0.15f + 0.85f * s.pitchDip) * (0.6f + 0.4f * g.r.f()));
            if (nextFree && (room || g.r.chance (0.5f * s.pitchDip)))
            {
                motif[(size_t) (i + 1)].on = false;
                st.len = 2;
            }
        }
    }

    // MUTATE: the same motif with about a quarter of its steps changed, from its own random stream
    auto mutateStep = [&] (Rng& m, RollStep& st, int i)
    {
        if (kickStep (i) && s.kickAvoid > 0.5f) return;
        const float x = m.f();
        if (! st.on) { st = {}; st.on = true; st.acc = 0.6f; st.deg = m.chance (0.3f) ? oct : 0; }
        else if (x < 0.35f) st.on = false;
        else if (x < 0.7f || s.pitchDip <= 0.0f) st.deg = st.deg == 0 ? oct : 0;
        else st.dip = st.dip > 0.0f ? 0.0f : juce::jmax (0.25f, s.pitchDip);
    };
    if (s.mutation > 0)
    {
        Rng m ((juce::uint32) s.mutation * 2654435761u + (juce::uint32) s.seed);
        for (int i = 0; i < perBar; ++i)
            if (m.chance (0.25f)) mutateStep (m, motif[(size_t) i], i);
    }

    // GROOVE: the steps of each beat get their own weight (the 16th after the "and" pushes hardest),
    // and quiet ghost notes fill some gaps
    if (s.groove > 0.0f)
    {
        static const float weight4[] = { 0.9f, 0.5f, 1.0f, 0.68f }, weight3[] = { 0.9f, 0.6f, 1.0f };
        for (int i = 0; i < perBar; ++i)
        {
            auto& st = motif[(size_t) i];
            const float w = tri ? weight3[i % 3] : weight4[i % 4];
            if (st.on) { if (st.deg != oct) st.acc += (w - st.acc) * s.groove; }
            else if (! kickStep (i) && g.r.chance (0.4f * s.groove))
            {
                st.on = st.ghost = true;
                st.acc = 0.12f;
            }
        }
    }

    // a high GLIDE wants something to slide between: some roots before another root start an octave up and fall
    if (s.glide > 0.0f)
        for (int i = 0; i + 1 < perBar; ++i)
        {
            auto& st = motif[(size_t) i];
            const auto& nx = motif[(size_t) (i + 1)];
            if (st.on && nx.on && ! st.ghost && ! nx.ghost && st.deg == 0 && nx.deg == 0 && st.dip <= 0.0f && g.r.chance (0.3f * s.glide))
                st.deg = oct;
        }

    const bool followChords = g.family != Psy && s.genre != HardTechno;
    juce::StringArray fills;
    for (int bar = 0; bar < s.bars; ++bar)
    {
        const float t = s.bars > 1 ? (float) bar / (float) (s.bars - 1) : 1.0f;
        const int root = followChords ? g.chord (bar) : (g.chord (bar) == 1 ? 1 : 0);
        auto line = motif;

        if (bar > 0)
            for (int i = 0; i < perBar; ++i)
                if (g.r.chance (0.18f * s.variation)) mutateStep (g.r, line[(size_t) i], i);

        // EVOLVE: early bars play a thinner, plainer line and the loop builds to its end
        const float hold = s.evolve * (1.0f - t);
        for (auto& st : line)
        {
            if (! st.on) continue;
            if (st.acc < 0.95f && g.r.chance (0.7f * hold)) st.on = false;
            if (g.r.chance (hold)) { st.deg = 0; st.dip = 0.0f; st.len = 1; }
        }

        // fills on the last beat of a phrase: every second bar in short loops, every fourth in long ones
        const bool phraseEnd = s.bars >= 2 && bar % 2 == 1 && (s.bars <= 4 || bar % 4 == 3);
        if (phraseEnd && g.r.chance (0.25f + 0.75f * s.variation))
        {
            const int last = 3 * spb;
            for (int i = last; i < perBar; ++i) line[(size_t) i] = {};
            switch (g.r.i (4))
            {
                case 0:
                {
                    fills.addIfNotAlreadyThere ("OCTAVE RUN");
                    static const std::vector<int> run = { 2, 4, 7 }; // climbs to the octave
                    for (int i = last + 1, k = 0; i < perBar; ++i, ++k)
                    {
                        const int d = run[(size_t) juce::jmin (k + (tri ? 1 : 0), 2)];
                        line[(size_t) i] = { true, d == 7 ? oct : d, 0.6f + 0.13f * (float) k, 0.0f, 1 };
                    }
                    break;
                }
                case 1:
                    fills.addIfNotAlreadyThere ("PICKUP");
                    for (int i = last + 1; i < perBar; ++i) line[(size_t) i] = { true, i - perBar, 0.7f, 0.0f, 1 }; // walks up into the next root
                    break;
                case 2:
                    fills.addIfNotAlreadyThere ("DROP"); // a beat of silence before the next phrase
                    break;
                default:
                    fills.addIfNotAlreadyThere ("STUTTER");
                    for (int i = last; i < perBar; ++i)
                        if (! kickStep (i) || s.kickAvoid < 0.5f) line[(size_t) i] = { true, 0, 0.45f + 0.5f * (float) (i - last) / (float) spb, 0.0f, 1 };
                    break;
            }
        }

        for (int i = 0; i < perBar; ++i)
        {
            const auto& st = line[(size_t) i];
            if (! st.on) continue;
            const double start = bar * 4.0 + i * stepLen;
            double len = st.len * stepLen * (st.dip > 0.0f ? 0.95 : st.ghost ? 0.5 : gateScale);

            // GLIDE: a note slides into a different note one or two steps later by overlapping it
            int next = i + 1;
            while (next < perBar && ! line[(size_t) next].on) ++next;
            const bool slide = next < perBar && next - i <= 2 && st.dip <= 0.0f && ! st.ghost && ! line[(size_t) next].ghost
                            && line[(size_t) next].deg != st.deg && g.r.chance (0.85f * s.glide);
            if (slide) len = (next - i) * stepLen + 0.04;
            // KICK AVOID also keeps notes from ringing into the next kick
            else if (s.kickAvoid > 0.5f)
                len = juce::jmin (len, std::floor (start + 1.0e-6) + 1.0 - start - 0.02);
            const size_t before = out.size();
            g.add (out, g.pitch (root + st.deg, kBassBase), start, len, g.vel (st.acc));
            if (out.size() > before)
            {
                out.back().dip = st.dip;
                out.back().slide = slide;
                if (st.ghost) out.back().velocity = juce::jmax (1, out.back().velocity * 2 / 5);
            }
        }
    }

    const juce::String dot = juce::String::fromUTF8 (" \xc2\xb7 ");
    detail = juce::String (rollStyleTitle (style)) + dot + "1-BAR MOTIF" + (s.bars > 1 ? " X" + juce::String (s.bars) : juce::String())
           + dot + (fills.isEmpty() ? juce::String ("NO FILLS") : "FILLS: " + fills.joinIntoString (", "))
           + (s.mutation > 0 ? dot + "MUT #" + juce::String (s.mutation) : juce::String());
}

// The classic house grooves get their dips here, on notes with a gap after them
void houseDips (Gen& g, std::vector<Note>& out)
{
    if (g.s.pitchDip <= 0.0f) return;
    std::sort (out.begin(), out.end(), [] (const Note& a, const Note& b) { return a.start < b.start; });
    for (size_t i = 0; i < out.size(); ++i)
    {
        const double gap = (i + 1 < out.size() ? out[i + 1].start : g.s.bars * 4.0) - (out[i].start + out[i].length);
        if (gap > 0.2 && g.r.chance (0.8f * g.s.pitchDip))
            out[i].dip = (0.15f + 0.85f * g.s.pitchDip) * (0.6f + 0.4f * g.r.f());
    }
}

void bass (Gen& g, std::vector<Note>& out, juce::String& detail)
{
    const auto& s = g.s;
    const double gateScale = 0.35 + 0.6 * s.gate;

    // AUTO in the house genres keeps the hand-written house grooves; any other style uses ROLL BASS
    if (g.family == House && ! s.triplet && s.rollStyle == RollAuto)
    {
        static const std::vector<const char*> tech = { "..r-..r...r-.ro.", "..r...r..r..r.o.", ".r..r.r..r..r..5", "..r-r-..r-..o-..", "..rg..r...rg..o." };
        static const std::vector<const char*> deep = { "r--..r..5--..r..", "..r--.r...7--.5.", "r-..r-.5..r-.o..", "r--.....r-.3.5.." };
        static const std::vector<const char*> bassH = { "..o.r.o...o.r.o.", "..r-.r..o-.r.o.r", "..ro..ro..ro.r5.", "r.o..r.o..r.o.ro" };
        const auto& list = s.genre == DeepHouse ? deep : s.genre == BassHouse ? bassH : tech;
        const char* a = g.r.pick (list);
        const char* b = g.r.pick (list);
        for (int bar = 0; bar < s.bars; ++bar)
            playTemplate (g, out, (bar % 2 == 1 && g.r.chance (0.3f + 0.6f * s.movement)) ? b : a, bar, g.chord (bar), kBassBase, (float) gateScale);
        houseDips (g, out);
        detail = "CLASSIC HOUSE GROOVE";
        return;
    }

    rollBass (g, out, detail);
}

//==============================================================================
void acid (Gen& g, std::vector<Note>& out)
{
    const auto& s = g.s;
    struct Step { bool on; int deg; bool accent, slide; };
    const int motifBars = juce::jmin (s.bars, 2);
    std::vector<Step> motif ((size_t) (motifBars * g.perBar));
    for (size_t i = 0; i < motif.size(); ++i)
    {
        auto& st = motif[i];
        st.on = i == 0 || g.r.chance (0.45f + 0.5f * s.density);
        const float x = g.r.f();
        st.deg = x < 0.45f ? 0 : x < 0.63f ? g.degreesPerOctave() : x < 0.73f ? 2 : x < 0.83f ? 4 : x < 0.91f ? 6 : x < 0.96f ? 1 : -1;
        if (g.r.chance (0.1f + 0.2f * s.movement)) st.deg += g.degreesPerOctave();
        st.accent = g.r.chance (0.3f);
        st.slide = g.r.chance (0.12f + 0.2f * s.movement);
    }

    const bool follow = g.family == Anthem || s.genre == MelodicTechno || s.genre == DeepHouse;
    for (int bar = 0; bar < s.bars; ++bar)
    {
        std::vector<Step> line (motif.begin() + (bar % motifBars) * g.perBar, motif.begin() + (bar % motifBars + 1) * g.perBar);
        if (bar == s.bars - 1 && s.bars >= 4)
            for (int m = 0; m < 3; ++m)
            {
                auto& st = line[(size_t) (g.perBar / 2 + g.r.i (g.perBar / 2))];
                st.on = true;
                st.deg = g.r.pick (std::vector<int> { 0, 2, 4, 7, 9 });
                st.accent = g.r.chance (0.6f);
            }
        const int shift = follow ? g.chord (bar) : 0;
        for (int i = 0; i < g.perBar; ++i)
        {
            const auto& st = line[(size_t) i];
            if (! st.on) continue;
            int next = i + 1;
            while (next < g.perBar && ! line[(size_t) next].on) ++next;
            const double room = (next - i) * g.stepLen;
            const double len = st.slide && next < g.perBar ? room + 0.02 : g.stepLen * (0.3 + 0.6 * s.gate);
            g.add (out, g.pitch (st.deg + shift, kAcidBase), bar * 4.0 + i * g.stepLen, len, st.accent ? g.vel (1.0f) : g.vel (0.55f));
        }
    }
}

//==============================================================================
// Melodic roles share one motif engine: a 1-bar rhythm + degrees, repeated as A A' A A''
struct Motif { std::vector<bool> on; std::vector<int> deg; };

std::vector<bool> melodyRhythm (Gen& g, bool pluck)
{
    const auto& s = g.s;
    std::vector<bool> on ((size_t) g.perBar, false);
    static const std::vector<const char*> anthem = { "x..x..x...x..x..", "x..x..x.x.x.x...", "x.xx.x.x..x.x.x.", "x..x..x..x..x.x.", "x...x.x.x..x.x.." };
    static const std::vector<const char*> hook = { "...x..x.....x...", "x..x......x..x..", "..x.x...x..x....", "x.....x..x.x....", "..x..x..x.....x." };
    static const std::vector<const char*> plucks = { "x.xx.x.xx.x.x.x.", ".x.x.xx..x.x.xx.", "x..x.x..x.x..x.x", "..x.x.x...x.x.xx" };

    if (g.family == Psy || s.triplet)
    {
        for (int i = 0; i < g.perBar; ++i)
            on[(size_t) i] = i == 0 || g.r.chance ((pluck ? 0.3f : 0.45f) + 0.5f * s.density);
        return on;
    }
    const char* t = pluck ? g.r.pick (plucks) : g.r.pick (g.family == Anthem ? anthem : hook);
    for (int i = 0; i < g.perBar; ++i)
        on[(size_t) i] = t[i] == 'x' || g.r.chance (juce::jmax (0.0f, s.density - 0.55f) * 0.5f);
    return on;
}

int melodyStep (Gen& g, int deg, int lo, int hi)
{
    const auto& s = g.s;
    if (g.r.chance (0.25f * (1.0f - s.movement))) return deg;
    const int maxLeap = 1 + juce::roundToInt (s.movement * 4.0f);
    int d = 1 + (int) (g.r.f() * g.r.f() * maxLeap); // mostly steps, sometimes leaps
    if (g.r.chance (0.5f)) d = -d;
    int n = deg + d;
    if (n > hi) n = deg - std::abs (d);
    if (n < lo) n = deg + std::abs (d);
    return juce::jlimit (lo, hi, n);
}

Motif makeMotif (Gen& g, int root, bool pluck)
{
    Motif m;
    m.on = melodyRhythm (g, pluck);
    m.deg.assign (m.on.size(), 0);
    int deg = g.snapToChord (g.r.i (5), root);
    for (size_t i = 0; i < m.on.size(); ++i)
    {
        if (! m.on[i]) continue;
        deg = melodyStep (g, deg, -2, 9);
        const bool strong = (int) i % g.spb == 0;
        if (strong || (pluck && g.r.chance (0.6f))) deg = g.snapToChord (deg, root);
        m.deg[i] = deg;
    }
    return m;
}

// New notes for steps [from, to) of a motif, joining smoothly onto what comes before
Motif vary (Gen& g, Motif m, int from, int to, int root, bool pluck, bool endOnRoot)
{
    const auto fresh = melodyRhythm (g, pluck);
    int deg = 0;
    for (int i = from - 1; i >= 0; --i) if (m.on[(size_t) i]) { deg = m.deg[(size_t) i]; break; }
    int last = -1;
    for (int i = from; i < to; ++i)
    {
        m.on[(size_t) i] = fresh[(size_t) i];
        if (! m.on[(size_t) i]) continue;
        deg = melodyStep (g, deg, -2, 9);
        if (i % g.spb == 0) deg = g.snapToChord (deg, root);
        m.deg[(size_t) i] = deg;
        last = i;
    }
    if (endOnRoot && last >= 0)
    {
        // land on the root in whichever octave is nearest
        const int n = g.degreesPerOctave(), d = m.deg[(size_t) last];
        m.deg[(size_t) last] = root + n * (int) std::round ((d - root) / (double) n);
    }
    return m;
}

void melody (Gen& g, std::vector<Note>& out, int base, bool pluck)
{
    const auto& s = g.s;
    const int root0 = g.chord (0);
    const Motif a = makeMotif (g, root0, pluck);
    const Motif answer = vary (g, a, g.perBar / 2, g.perBar, root0, pluck, false);
    const Motif cadence = vary (g, a, g.perBar / 2, g.perBar, root0, pluck, true);

    const bool transpose = g.family == Anthem || s.genre == MelodicTechno;
    for (int bar = 0; bar < s.bars; ++bar)
    {
        const Motif& m = s.bars == 1 ? a : bar % 4 == 1 ? answer : (bar % 4 == 3 || (s.bars == 2 && bar == 1)) ? cadence : a;
        const int root = g.chord (bar);
        const int shift = transpose ? root - root0 : 0;
        for (int i = 0; i < g.perBar; ++i)
        {
            if (! m.on[(size_t) i]) continue;
            int next = i + 1;
            while (next < g.perBar && ! m.on[(size_t) next]) ++next;
            int deg = m.deg[(size_t) i] + shift;
            if (i % g.spb == 0) deg = g.snapToChord (deg, root);
            const double room = (next - i) * g.stepLen;
            const double len = pluck ? g.stepLen * (0.3 + 0.7 * s.gate) : juce::jmax (g.stepLen * 0.5, room * s.gate);
            const float acc = i % g.spb == 0 ? 1.0f : i % 2 == 0 ? 0.75f : 0.6f;
            g.add (out, g.pitch (deg, base), bar * 4.0 + i * g.stepLen, len, g.vel (acc));
        }
    }
}

//==============================================================================
void arp (Gen& g, std::vector<Note>& out)
{
    const auto& s = g.s;
    const int n = g.degreesPerOctave();
    const int mode = g.r.i (6); // up, down, up-down, in-out, trance gate, fixed random
    std::vector<int> order;
    for (int k = 0; k < 8; ++k) order.push_back (g.r.i (6));
    const int base = g.family == Anthem ? kLeadBase - 12 : kMidBase;

    for (int bar = 0; bar < s.bars; ++bar)
    {
        const int root = g.chord (bar);
        std::vector<int> tones = { root, root + 2, root + 4 };
        if (s.movement > 0.35f) tones.push_back (root + 6);
        tones.push_back (root + n);
        if (s.movement > 0.65f) { tones.push_back (root + n + 2); tones.push_back (root + n + 4); }
        const int count = (int) tones.size();
        for (int i = 0; i < g.perBar; ++i)
        {
            if (i % g.spb != 0 && g.r.chance ((1.0f - s.density) * 0.5f)) continue;
            int idx = 0;
            switch (mode)
            {
                case 0: idx = i % count; break;
                case 1: idx = count - 1 - i % count; break;
                case 2: { const int c = juce::jmax (1, 2 * count - 2); const int k = i % c; idx = k < count ? k : c - k; break; }
                case 3: idx = (i % 2 == 0) ? (i / 2) % count : count - 1 - (i / 2) % count; break;
                case 4: { static const int t[] = { 0, 2, 1, 2 }; idx = juce::jmin (count - 1, t[i % 4] + (i / 8 % 2)); break; }
                default: idx = order[(size_t) (i % 8)] % count; break;
            }
            g.add (out, g.pitch (tones[(size_t) idx], base), bar * 4.0 + i * g.stepLen, g.stepLen * (0.25 + 0.75 * s.gate),
                   g.vel (i % g.spb == 0 ? 1.0f : 0.6f));
        }
    }
}

//==============================================================================
// Voice-led chord: the inversion nearest the previous one, kept around `centre`
std::vector<int> voice (Gen& g, int root, int voices, bool seventh, std::vector<int>& prev, int centre)
{
    std::vector<int> pcs = { g.pitch (root, 0), g.pitch (root + 2, 0), g.pitch (root + 4, 0) };
    if (seventh) pcs.push_back (g.pitch (root + 6, 0));
    std::vector<int> best;
    int bestCost = 1 << 30;
    for (size_t inv = 0; inv < pcs.size(); ++inv)
    {
        // lowest note of this inversion just under the centre, the rest stacked closely above it
        std::vector<int> c;
        int low = pcs[inv];
        while (low < centre - 8) low += 12;
        while (low >= centre + 4) low -= 12;
        c.push_back (low);
        for (size_t k = 1; k < pcs.size(); ++k)
        {
            int p = pcs[(inv + k) % pcs.size()];
            while (p <= c.back()) p += 12;
            while (p - 12 > c.back()) p -= 12;
            c.push_back (p);
        }
        while ((int) c.size() < voices) c.push_back (c[c.size() - pcs.size()] + 12);
        int cost = 0;
        if (prev.empty())
            for (int p : c) cost += std::abs (p - centre);
        else
            for (size_t k = 0; k < c.size(); ++k) cost += std::abs (c[k] - prev[juce::jmin (k, prev.size() - 1)]);
        if (cost < bestCost) { bestCost = cost; best = c; }
    }
    prev = best;
    return best;
}

void chords (Gen& g, std::vector<Note>& out, bool pad)
{
    const auto& s = g.s;
    const bool seventh = s.genre == DeepHouse || (s.genre == MelodicTechno && s.movement > 0.3f) || (pad && s.movement > 0.5f);
    std::vector<int> prev;

    if (pad)
    {
        for (int bar = 0; bar < s.bars;)
        {
            int run = 1;
            while (bar + run < s.bars && g.chord (bar + run) == g.chord (bar)) ++run;
            const auto v = voice (g, g.chord (bar), 4, seventh, prev, kMidBase + 4);
            for (size_t k = 0; k < v.size(); ++k)
                g.add (out, v[k], bar * 4.0, run * 4.0 - 0.02, g.vel (k == 0 ? 0.55f : 0.45f));
            bar += run;
        }
        return;
    }

    static const std::vector<const char*> deep = { "x..x..x.........", "..x...x...x..x..", "...x..x...x.....", "x.....x...x..x.." };
    static const std::vector<const char*> tech = { "...x..x....x..x.", "..x....x..x.....", "...x.....x....x." };
    static const std::vector<const char*> techno = { "x.....x.....x...", "x..x..x.........", "..x.....x..x...." };
    static const std::vector<const char*> psy = { "......x.......x.", "..x...x...x...x.", "......x...x....." };
    static const std::vector<const char*> pump = { "..x...x...x...x." };

    const char* t = nullptr;
    double hold = 0.5 * s.gate;
    bool sustain = false;
    switch (g.family)
    {
        case House: t = g.r.pick (s.genre == DeepHouse ? deep : tech); hold = s.genre == DeepHouse ? 0.75 * s.gate : 0.35 * s.gate; break;
        case Techno: if (s.genre == MelodicTechno && s.density < 0.6f) sustain = true; else t = g.r.pick (techno); break;
        case Anthem: if (s.density < 0.5f) sustain = true; else t = pump[0]; break;
        default: t = g.r.pick (psy); hold = 0.3 * s.gate; break;
    }

    for (int bar = 0; bar < s.bars; ++bar)
    {
        const auto v = voice (g, g.chord (bar), 3 + (seventh ? 1 : 0), seventh, prev, kMidBase + 4);
        if (sustain)
        {
            for (int p : v) g.add (out, p, bar * 4.0, 4.0 * (0.5 + 0.49 * s.gate), g.vel (0.7f));
            continue;
        }
        for (int i = 0; i < 16; ++i)
            if (t[i] == 'x')
                for (int p : v) g.add (out, p, bar * 4.0 + i * 0.25, juce::jmax (0.08, hold), g.vel (i % 4 == 0 ? 1.0f : 0.75f));
    }
}

//==============================================================================
// A slower second voice under the lead: longer notes on chord tones, placed where the lead leaves room
void counter (Gen& g, std::vector<Note>& out, const std::vector<Note>& lead)
{
    const auto& s = g.s;
    const int n = g.degreesPerOctave();
    static const std::vector<const char*> rhythms = { "x...x...x...x...", "..x...x...x...x.", "x.....x...x.....", "x.......x...x...", "....x.x.....x.x." };
    const char* t = g.r.pick (rhythms);
    std::vector<bool> on (16);
    for (int i = 0; i < 16; ++i) on[(size_t) i] = t[i] == 'x' || (i % 2 == 0 && g.r.chance (juce::jmax (0.0f, s.density - 0.6f)));

    int deg = 2 + g.r.i (3);
    for (int bar = 0; bar < s.bars; ++bar)
    {
        const int root = g.chord (bar);
        int placed = 0;
        for (int i = 0; i < 16; ++i)
        {
            if (! on[(size_t) i]) continue;
            const double t0 = bar * 4.0 + i * 0.25;
            // step aside when the lead starts a note at the same moment (but say something every bar)
            const bool clash = std::any_of (lead.begin(), lead.end(), [&] (const Note& l) { return std::abs (l.start - t0) < 1.0e-6; });
            if (clash && placed > 0 && g.r.chance (0.5f)) continue;
            ++placed;
            int next = i + 1;
            while (next < 16 && ! on[(size_t) next]) ++next;
            deg = g.snapToChord (juce::jlimit (0, n + 4, deg + g.r.pick (std::vector<int> { -2, -1, 0, 1, 2 })), root, true);
            g.add (out, g.pitch (deg, kMidBase), t0, (next - i) * 0.25 * (0.4 + 0.6 * s.gate), g.vel (i % 4 == 0 ? 0.7f : 0.55f));
        }
    }
}

//==============================================================================
struct DrumLine { int note; const char* steps; float chance; }; // chance applies to lower-case hits

void drums (Gen& g, std::vector<Note>& out)
{
    const auto& s = g.s;
    std::vector<DrumLine> kit;
    switch (s.genre)
    {
        case FullOn: case ProgPsy:
            kit = { { KickNote, "X...X...X...X...", 1 }, { OpenHatNote, "..x...x...x...x.", 1 },
                    { HatNote, "x.x.x.x.x.x.x.x.", 1 }, { HatNote, ".x.x.x.x.x.x.x.x", s.density },
                    { s.genre == ProgPsy ? ClapNote : SnareNote, "....x.......x...", s.genre == ProgPsy ? 1.0f : 0.7f },
                    { RideNote, "..x...x...x...x.", s.density - 0.3f } };
            break;
        case PsyTech: case HardTechno:
            kit = { { KickNote, "X...X...X...X...", 1 }, { OpenHatNote, "..x...x...x...x.", 1 }, { ClapNote, "....x.......x...", 1 },
                    { HatNote, "xxxxxxxxxxxxxxxx", s.density }, { RimNote, "...x..x....x..x.", s.density - 0.3f },
                    { RideNote, "x.x.x.x.x.x.x.x.", s.genre == HardTechno ? s.density : 0.0f } };
            break;
        case TechHouse: case DeepHouse: case BassHouse:
            kit = { { KickNote, "X...X...X...X...", 1 }, { ClapNote, "....X.......X...", 1 }, { OpenHatNote, "..x...x...x...x.", 1 },
                    { HatNote, "x.x.x.x.x.x.x.x.", s.density + 0.2f }, { ShakerNote, "xxxxxxxxxxxxxxxx", s.density - 0.2f },
                    { RimNote, s.genre == DeepHouse ? "......x..x......" : ".......x..x....x", s.density } };
            break;
        case MelodicTechno:
            kit = { { KickNote, "X...X...X...X...", 1 }, { SnareNote, "....x.......x...", 1 }, { HatNote, "x.xxx.xxx.xxx.x.", s.density + 0.1f },
                    { OpenHatNote, "..x...x...x...x.", s.density }, { RimNote, "..x.....x..x....", s.density - 0.2f } };
            break;
        default:
            kit = { { KickNote, "X...X...X...X...", 1 }, { ClapNote, "....X.......X...", 1 }, { OpenHatNote, "..x...x...x...x.", 1 },
                    { HatNote, "xxxxxxxxxxxxxxxx", s.density } };
            break;
    }

    for (int bar = 0; bar < s.bars; ++bar)
    {
        for (auto& line : kit)
            for (int i = 0; i < 16; ++i)
            {
                const char c = line.steps[i];
                if (c == '.') continue;
                if (c == 'x' && line.chance < 1.0f && ! g.r.chance (line.chance)) continue;
                const bool open = line.note == OpenHatNote;
                const float acc = c == 'X' ? 1.0f : line.note == HatNote || line.note == ShakerNote ? (i % 2 == 0 ? 0.7f : 0.35f) : 0.8f;
                g.add (out, line.note, bar * 4.0 + i * 0.25, open ? 0.2 : 0.12, g.vel (acc));
            }
        // Trance and EDM build into the next phrase with a snare roll
        if ((s.genre == Trance || s.genre == Edm) && s.bars >= 4 && bar % 4 == 3 && s.density > 0.3f)
            for (int i = 8; i < 16; ++i)
                g.add (out, SnareNote, bar * 4.0 + i * 0.25, 0.1, g.vel (0.4f + 0.6f * (float) (i - 8) / 7.0f));
    }
}

//==============================================================================
void applyGroove (std::vector<Note>& notes, const Settings& s, juce::uint32 seed, bool isDrums)
{
    if (! s.triplet && s.swing > 0.501f)
    {
        const double push = (s.swing - 0.5) * 0.5; // 66% = a triplet feel
        for (auto& n : notes)
        {
            const double inEighth = std::fmod (n.start, 0.5);
            if (std::abs (inEighth - 0.25) < 1.0e-6)
            {
                n.start += push;
                n.length = juce::jmax (1.0 / 64.0, n.length - push * 0.5);
            }
        }
    }
    if (s.humanize > 0.0f)
    {
        Rng r (seed ^ 0x9e3779b9u);
        for (auto& n : notes)
        {
            if (! (isDrums && n.pitch == KickNote))
                n.start = juce::jmax (0.0, n.start + (r.f() - 0.5) * 0.03 * s.humanize);
            n.velocity = juce::jlimit (1, 127, n.velocity + juce::roundToInt ((r.f() - 0.5f) * 24.0f * s.humanize));
        }
    }
}
} // namespace

//==============================================================================
void tidy (std::vector<Note>& notes, double lengthBeats)
{
    notes.erase (std::remove_if (notes.begin(), notes.end(), [&] (const Note& n) { return n.start < 0.0 || n.start >= lengthBeats - 1.0e-6; }),
                 notes.end());
    // same pitch on the same step twice: keep the louder one
    std::sort (notes.begin(), notes.end(), [] (const Note& a, const Note& b)
    {
        if (a.pitch != b.pitch) return a.pitch < b.pitch;
        if (std::abs (a.start - b.start) >= 1.0e-6) return a.start < b.start;
        return a.velocity > b.velocity;
    });
    notes.erase (std::unique (notes.begin(), notes.end(), [] (const Note& a, const Note& b) { return a.pitch == b.pitch && std::abs (a.start - b.start) < 1.0e-6; }),
                 notes.end());
    for (size_t i = 0; i < notes.size(); ++i)
    {
        auto& n = notes[i];
        double limit = lengthBeats - n.start;
        if (i + 1 < notes.size() && notes[i + 1].pitch == n.pitch)
            limit = notes[i + 1].start - n.start - 0.002;
        n.length = juce::jlimit (1.0 / 128.0, juce::jmax (1.0 / 128.0, limit), n.length);
    }
    std::sort (notes.begin(), notes.end(), [] (const Note& a, const Note& b) { return a.start != b.start ? a.start < b.start : a.pitch < b.pitch; });
}

Pattern generate (const Settings& s)
{
    Pattern p;
    p.lengthBeats = s.bars * 4.0;
    p.drums = s.role == Drums;
    Gen g (s, 0);

    switch (s.role)
    {
        case Bass:    bass (g, p.notes, p.detail); break;
        case Acid:    acid (g, p.notes); break;
        case Lead:    melody (g, p.notes, kLeadBase, false); break;
        case Pluck:   melody (g, p.notes, kMidBase + 12, true); break;
        case Arp:     arp (g, p.notes); break;
        case Chords:  chords (g, p.notes, false); break;
        case Pad:     chords (g, p.notes, true); break;
        case Counter:
        {
            Settings leadSettings = s;
            leadSettings.role = Lead;
            Gen lg (leadSettings, 0);
            std::vector<Note> lead;
            melody (lg, lead, kLeadBase, false);
            Gen cg (s, 1);
            counter (cg, p.notes, lead);
            break;
        }
        default:      drums (g, p.notes); break;
    }

    if (! p.drums)
        for (auto& n : p.notes) n.pitch = juce::jlimit (0, 127, n.pitch + 12 * s.octave);
    applyGroove (p.notes, s, (juce::uint32) s.seed, p.drums);
    tidy (p.notes, p.lengthBeats);
    p.bends = dipBends (p.notes);

    const auto& gi = genreInfo (s.genre);
    const juce::String dot = juce::String::fromUTF8 (" \xc2\xb7 ");
    const juce::String keyText = p.drums ? juce::String() : juce::String (keyName (s.key)) + " " + scaleTitle (s.scale);
    p.title = (juce::String (gi.title) + dot + roleTitle (s.role) + (p.drums ? juce::String() : dot + keyText) + dot
               + juce::String (s.bars) + (s.bars == 1 ? " BAR" : " BARS") + dot + "VAR " + juce::String (s.seed)).toUpperCase();
    p.fileStem = "MIDI GOD - " + juce::String (gi.title) + " " + roleTitle (s.role) + (p.drums ? juce::String() : " " + keyText) + " v" + juce::String (s.seed);
    return p;
}

//==============================================================================
double quantizeBeats (int choice)
{
    static const double q[] = { 0.0, 0.5, 0.25, 1.0 / 6.0, 0.125 };
    return q[juce::jlimit (0, 4, choice)];
}

void finishNotes (std::vector<Note>& notes, double lengthBeats, const Finish& f)
{
    const auto& steps = scaleSteps (f.scale);
    for (auto& n : notes)
    {
        if (f.quantize > 0.0)
        {
            const double end = n.start + n.length;
            n.start = std::round (n.start / f.quantize) * f.quantize;
            double qEnd = std::round (end / f.quantize) * f.quantize;
            if (qEnd <= n.start + 1.0e-6) qEnd = n.start + f.quantize;
            n.length = qEnd - n.start;
        }
        if (f.drums) continue;
        if (f.snap)
        {
            // nearest in-scale pitch, the lower one on a tie
            int best = n.pitch, bestDist = 99;
            for (int d = 0; d <= 6 && bestDist > 0; ++d)
                for (int cand : { n.pitch - d, n.pitch + d })
                {
                    const int pc = ((cand - f.key) % 12 + 12) % 12;
                    if (std::find (steps.begin(), steps.end(), pc) != steps.end() && d < bestDist) { best = cand; bestDist = d; }
                }
            n.pitch = best;
        }
        n.pitch = juce::jlimit (0, 127, n.pitch + 12 * f.octave);
    }
    tidy (notes, lengthBeats);
}

std::vector<Bend> dipBends (const std::vector<Note>& notes)
{
    std::vector<Bend> out;
    for (auto& n : notes)
    {
        if (n.dip <= 0.0f) continue;
        // the dive starts 40% into the note and speeds up towards its end
        const double a = n.start + n.length * 0.4, b = n.start + n.length;
        const int steps = juce::jmax (4, (int) std::ceil ((b - a) * 48.0));
        for (int k = 0; k <= steps; ++k)
        {
            const float x = (float) k / (float) steps;
            out.push_back ({ a + (b - a) * x, -n.dip * x * x });
        }
        out.push_back ({ b + 1.0e-4, 0.0f });
    }
    if (! out.empty()) out.insert (out.begin(), Bend { 0.0, 0.0f });
    std::stable_sort (out.begin(), out.end(), [] (const Bend& x, const Bend& y) { return x.beat < y.beat; });
    return out;
}

int bendTo14Bit (float value) { return juce::jlimit (0, 16383, 8192 + juce::roundToInt (juce::jlimit (-1.0f, 1.0f, value) * 8191.0f)); }

juce::MidiFile toMidiFile (const Pattern& p, double bpm)
{
    const int tpq = 960;
    auto ticks = [tpq] (double beats) { return std::round (beats * tpq); };
    juce::MidiMessageSequence seq;
    auto name = juce::MidiMessage::textMetaEvent (3, p.fileStem.fromFirstOccurrenceOf ("MIDI GOD - ", false, false));
    seq.addEvent (name, 0.0);
    seq.addEvent (juce::MidiMessage::tempoMetaEvent (juce::roundToInt (60.0e6 / juce::jmax (20.0, bpm))), 0.0);
    seq.addEvent (juce::MidiMessage::timeSignatureMetaEvent (4, 4), 0.0);
    for (auto& n : p.notes)
    {
        const double on = ticks (n.start), off = juce::jmax (on + 1.0, ticks (n.start + n.length));
        seq.addEvent (juce::MidiMessage::noteOn (1, n.pitch, (juce::uint8) juce::jlimit (1, 127, n.velocity)), on);
        seq.addEvent (juce::MidiMessage::noteOff (1, n.pitch), off);
    }
    for (auto& b : p.bends)
        seq.addEvent (juce::MidiMessage::pitchWheel (1, bendTo14Bit (b.value)), ticks (b.beat));
    seq.addEvent (juce::MidiMessage::endOfTrack(), ticks (p.lengthBeats));
    seq.sort();
    seq.updateMatchedPairs();
    juce::MidiFile f;
    f.setTicksPerQuarterNote (tpq);
    f.addTrack (seq);
    return f;
}
} // namespace midigod
