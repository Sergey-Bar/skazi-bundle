#include "Presets.h"
#include "TahiniEngine.h"

namespace tahini
{
namespace
{
    // Delay times (index into the note list): 0 1/1, 1 1/2, 2 1/4, 3 1/8, 4 1/16, 5 1/32, 6 1/2T, 7 1/4T,
    // 8 1/8T, 9 1/16T, 10 1/2D, 11 1/4D, 12 1/8D, 13 1/16D, 14 2/1, 15 4/1
    struct Category { const char* name; std::function<void (skazi::PresetGen&)> style; };

    const std::vector<Category>& categories()
    {
        static const std::vector<Category> c = {
            { "PSY SPACE", [] (skazi::PresetGen& g) {
                  g.set ("dmode", PingPong); g.set ("dtime", (float) g.pick ({ 12, 3, 8, 13 })); g.range ("fb", 40, 65);
                  g.range ("locut", 200, 500); g.range ("hicut", 4000, 9000); g.range ("wow", 0, 20);
                  g.range ("size", 60, 90); g.range ("decay", 3, 8); g.range ("damp", 30, 60);
                  g.range ("dmix", 50, 60); g.range ("rmix", 25, 45); g.range ("duck", 40, 80); } },
            { "GOA CAVE", [] (skazi::PresetGen& g) {
                  g.set ("dmode", PingPong); g.set ("dtime", (float) g.pick ({ 13, 8, 12 })); g.range ("fb", 50, 75);
                  g.range ("locut", 150, 400); g.range ("hicut", 3000, 7000);
                  g.range ("size", 80, 100); g.range ("decay", 8, 16); g.range ("damp", 20, 40); g.range ("predelay", 10, 60);
                  g.range ("dmix", 50, 60); g.range ("rmix", 30, 50); g.range ("duck", 30, 70); } },
            { "DUB ECHO", [] (skazi::PresetGen& g) {
                  g.set ("dmode", Tape); g.set ("dtime", (float) g.pick ({ 11, 12, 2 })); g.range ("fb", 60, 85);
                  g.range ("locut", 100, 300); g.range ("hicut", 2500, 5000); g.range ("wow", 30, 60); g.range ("ddrive", 20, 50);
                  g.range ("size", 40, 70); g.range ("decay", 1.5f, 4); g.range ("dmix", 60, 75); g.range ("rmix", 10, 25); g.range ("duck", 20, 50); } },
            { "TAPE ECHO", [] (skazi::PresetGen& g) {
                  g.set ("dmode", Tape); g.set ("dtime", (float) g.pick ({ 3, 12, 8 })); g.range ("fb", 35, 60);
                  g.range ("locut", 150, 400); g.range ("hicut", 3000, 6000); g.range ("wow", 20, 50); g.range ("ddrive", 10, 40);
                  g.range ("size", 30, 60); g.range ("decay", 1, 3); g.range ("dmix", 50, 65); g.range ("rmix", 5, 20); g.range ("duck", 20, 60); } },
            { "VOCAL THROW", [] (skazi::PresetGen& g) {
                  g.set ("dmode", (float) g.pick ({ PingPong, Stereo })); g.set ("dtime", (float) g.pick ({ 2, 12, 11 })); g.range ("fb", 30, 50);
                  g.range ("locut", 300, 600); g.range ("hicut", 5000, 8000); g.range ("wow", 0, 15);
                  g.range ("size", 50, 80); g.range ("decay", 2, 5); g.range ("dmix", 50, 65); g.range ("rmix", 15, 30); g.range ("duck", 60, 90); } },
            { "BIG HALL", [] (skazi::PresetGen& g) {
                  g.range ("dmix", 0, 15); g.range ("fb", 20, 40); g.range ("size", 70, 100); g.range ("decay", 4, 12);
                  g.range ("predelay", 20, 80); g.range ("damp", 30, 60); g.range ("rmix", 30, 60); g.range ("duck", 0, 40); g.range ("width", 80, 100); } },
            { "ROOMS", [] (skazi::PresetGen& g) {
                  g.range ("dmix", 0, 10); g.range ("size", 10, 40); g.range ("decay", 0.4f, 1.5f); g.range ("predelay", 0, 20);
                  g.range ("damp", 30, 70); g.range ("rmix", 15, 35); g.range ("duck", 0, 30); g.range ("width", 60, 100); } },
            { "CRAZY TAPE", [] (skazi::PresetGen& g) {
                  g.set ("dmode", Tape); g.set ("dtime", (float) g.pick ({ 3, 12, 8, 2 })); g.range ("fb", 45, 75);
                  g.range ("wow", 30, 80); g.range ("age", 30, 90); g.range ("ddrive", 20, 60); g.range ("hicut", 3000, 7000);
                  g.set ("reverse", g.chance (0.6f) ? g.rr (30, 100) : 0.0f); g.range ("tstop", 20, 80);
                  g.set ("pitch", g.chance (0.4f) ? (float) g.pick ({ -12, -7, -5, 5, 7, 12 }) : 0.0f);
                  g.range ("size", 40, 80); g.range ("decay", 2, 6); g.range ("dmix", 55, 70); g.range ("rmix", 10, 30); g.range ("duck", 30, 70); } },
            { "PITCH SPIRAL", [] (skazi::PresetGen& g) {
                  g.set ("dmode", (float) g.pick ({ Tape, PingPong })); g.set ("dtime", (float) g.pick ({ 3, 12, 2, 13 })); g.range ("fb", 50, 80);
                  g.set ("pitch", (float) g.pick ({ -12, -7, -5, -3, 3, 5, 7, 12 })); g.range ("wow", 10, 40); g.range ("age", 0, 40);
                  g.set ("reverse", g.chance (0.3f) ? g.rr (20, 60) : 0.0f);
                  g.range ("size", 60, 95); g.range ("decay", 3, 10); g.range ("dmix", 50, 65); g.range ("rmix", 20, 45); g.range ("duck", 30, 70); } },
            { "AMBIENT WASH", [] (skazi::PresetGen& g) {
                  g.set ("dmode", Tape); g.set ("dtime", (float) g.pick ({ 1, 10, 2 })); g.range ("fb", 50, 70); g.range ("wow", 20, 50);
                  g.range ("hicut", 3000, 6000); g.range ("size", 90, 100); g.range ("decay", 10, 20); g.range ("damp", 40, 70);
                  g.range ("dmix", 50, 60); g.range ("rmix", 50, 80); g.range ("duck", 20, 50); } },
            // 1.1: echo filters, drive types, delay types and pitch blend
            { "PSY THROWS", [] (skazi::PresetGen& g) {
                  g.set ("dmode", (float) g.pick ({ PingPong, PingPong, Dual, Digital })); g.set ("dtime", (float) g.pick ({ 12, 3, 8, 13 })); g.range ("fb", 45, 70);
                  g.range ("locut", 300, 900); g.range ("hpres", 0, 40); g.range ("hicut", 3500, 8000); g.range ("lpres", 0, 40);
                  g.set ("dist", (float) g.pick ({ DriveTape, DriveTube })); g.range ("ddrive", 10, 40); g.range ("wow", 0, 15);
                  g.range ("size", 50, 80); g.range ("decay", 2, 5); g.range ("dmix", 50, 65); g.range ("rmix", 15, 35); g.range ("duck", 60, 90); } },
            { "TECHNO DUB", [] (skazi::PresetGen& g) {
                  g.set ("dmode", (float) g.pick ({ Tape, Bbd, Stereo })); g.set ("dtime", (float) g.pick ({ 11, 2, 12, 7 })); g.range ("fb", 65, 88);
                  g.range ("locut", 100, 400); g.range ("hicut", 800, 3000); g.range ("lpres", 20, 70);
                  g.set ("dist", (float) g.pick ({ DriveTube, DriveTape })); g.range ("ddrive", 25, 60); g.range ("wow", 20, 50);
                  g.range ("size", 40, 70); g.range ("decay", 1.5f, 4); g.range ("dmix", 60, 75); g.range ("rmix", 10, 30); g.range ("duck", 20, 50); } },
            { "SHIMMER", [] (skazi::PresetGen& g) {
                  g.set ("dmode", (float) g.pick ({ Digital, Stereo, PingPong })); g.set ("dtime", (float) g.pick ({ 2, 1, 11, 10 })); g.range ("fb", 60, 80);
                  g.set ("pitch", (float) g.pick ({ 12, 12, 24, 7, -12 })); g.range ("pmix", 30, 60); g.range ("wow", 0, 15);
                  g.range ("locut", 300, 700); g.range ("hicut", 6000, 12000);
                  g.range ("size", 80, 100); g.range ("decay", 6, 16); g.range ("damp", 30, 60);
                  g.range ("dmix", 50, 60); g.range ("rmix", 35, 60); g.range ("duck", 20, 50); } },
            { "DIRTY ECHO", [] (skazi::PresetGen& g) {
                  g.set ("dmode", (float) g.pick ({ Bbd, Tape, PingPong, MultiTap })); g.set ("dtime", (float) g.pick ({ 3, 12, 8, 4, 2 })); g.range ("fb", 50, 80);
                  g.set ("dist", (float) g.pick ({ DriveFuzz, DriveCrush, DriveTube })); g.range ("ddrive", 40, 90);
                  g.range ("locut", 300, 1200); g.range ("hpres", 0, 60); g.range ("hicut", 1500, 6000); g.range ("lpres", 0, 60);
                  g.range ("size", 30, 70); g.range ("decay", 1, 4); g.range ("dmix", 55, 70); g.range ("rmix", 5, 25); g.range ("duck", 30, 70); } },
            { "PITCH ECHOES", [] (skazi::PresetGen& g) {
                  g.set ("dmode", (float) g.pick ({ PingPong, Stereo, Digital, Tape, Dual })); g.set ("dtime", (float) g.pick ({ 3, 12, 2, 8 })); g.range ("fb", 50, 78);
                  if (g.chance (0.25f)) { g.set ("psnap", 0); g.range ("pitch", -0.6f, 0.6f); g.range ("pmix", 40, 70); } // free detune smear
                  else { g.set ("pitch", (float) g.pick ({ -24, -12, -7, -5, 5, 7, 12, 24 })); g.range ("pmix", 60, 100); }
                  g.range ("locut", 150, 600); g.range ("hicut", 3000, 9000); g.range ("ddrive", 0, 30);
                  g.range ("size", 50, 90); g.range ("decay", 2, 8); g.range ("dmix", 50, 65); g.range ("rmix", 15, 40); g.range ("duck", 30, 70); } },
            { "MULTI + DUAL", [] (skazi::PresetGen& g) {
                  g.set ("dmode", (float) g.pick ({ MultiTap, Dual })); g.set ("dtime", (float) g.pick ({ 1, 2, 11, 10 })); g.range ("fb", 30, 60);
                  g.range ("locut", 200, 800); g.range ("hpres", 0, 30); g.range ("hicut", 2500, 8000); g.range ("lpres", 0, 30);
                  g.set ("dist", (float) g.pick ({ DriveTape, DriveTube, DriveCrush })); g.range ("ddrive", 0, 40);
                  g.range ("size", 40, 80); g.range ("decay", 1.5f, 5); g.range ("dmix", 50, 65); g.range ("rmix", 10, 30); g.range ("duck", 40, 80); } },
        };
        return c;
    }

    std::vector<skazi::Preset> build()
    {
        const auto defs = skazi::defaultValues (paramDefs());
        std::vector<skazi::Preset> list;
        auto factory = [&] (const char* name, std::initializer_list<std::pair<const char*, float>> changes)
        {
            skazi::Preset p { name, "FACTORY", defs };
            for (auto& [id, v] : changes) p.values[id] = v;
            list.push_back (std::move (p));
        };
        factory ("Init", {});
        factory ("Goa Cave", { { "dtime", 13 }, { "fb", 65 }, { "size", 95 }, { "decay", 12 }, { "rmix", 40 } });
        factory ("Dub Siren Echo", { { "dmode", Tape }, { "dtime", 11 }, { "fb", 80 }, { "wow", 45 }, { "hicut", 3500 } });
        factory ("Clean Ping-Pong", { { "rmix", 0 }, { "fb", 45 }, { "duck", 0 } });
        factory ("Big Hall", { { "dmix", 0 }, { "size", 90 }, { "decay", 7 }, { "rmix", 45 } });
        factory ("Send Only (no dry)", { { "dry", 0 }, { "dmix", 100 }, { "rmix", 100 }, { "duck", 0 } });
        factory ("Broken Tape Machine", { { "dmode", Tape }, { "age", 80 }, { "tstop", 60 }, { "wow", 70 }, { "reverse", 40 }, { "fb", 65 } });
        factory ("Rising Spiral", { { "pitch", 12 }, { "fb", 70 }, { "dtime", 12 }, { "rmix", 35 } });
        factory ("Falling Spiral", { { "pitch", -12 }, { "fb", 70 }, { "dtime", 12 }, { "rmix", 35 } });
        factory ("Backwards Echo", { { "reverse", 100 }, { "fb", 55 }, { "dtime", 2 } });

        auto generate = [&] (int from, int to, int& g)
        {
            for (int ci = from; ci < to; ++ci)
                for (int i = 0; i < 14; ++i, ++g)
                {
                    const auto& cat = categories()[(size_t) ci];
                    juce::Random r ((juce::int64) g * 3571 + 77);
                    skazi::Preset p { skazi::presetName (g + 400), cat.name, defs };
                    skazi::PresetGen gen { r, p.values };
                    cat.style (gen);
                    list.push_back (std::move (p));
                }
        };
        // the 1.0 list stays first and in order, so preset numbers don't move
        constexpr int oldCategories = 10;
        int g = 0;
        generate (0, oldCategories, g);

        // 1.1: echo filters, drive, delay types and pitch
        auto lab = [&] (const char* name, std::initializer_list<std::pair<const char*, float>> changes)
        {
            factory (name, {});
            auto& p = list.back();
            p.category = "ECHO LAB";
            p.values["wow"] = 0; // clean unless the preset asks for wobble
            for (auto& [id, v] : changes) p.values[id] = v;
        };
        lab ("Psy Lead Throw", { { "dmode", PingPong }, { "dtime", 12 }, { "fb", 55 }, { "locut", 600 }, { "hpres", 30 }, { "hicut", 5000 },
                                 { "lpres", 20 }, { "ddrive", 30 }, { "dmix", 55 }, { "rmix", 25 }, { "duck", 75 } });
        lab ("Techno Dub Chord", { { "dmode", Tape }, { "dtime", 11 }, { "fb", 78 }, { "locut", 300 }, { "hicut", 1800 }, { "lpres", 45 },
                                   { "dist", DriveTube }, { "ddrive", 45 }, { "wow", 35 }, { "dmix", 60 }, { "rmix", 15 }, { "duck", 40 } });
        lab ("Octave Shimmer", { { "dmode", Digital }, { "dtime", 2 }, { "fb", 70 }, { "pitch", 12 }, { "pmix", 45 }, { "locut", 400 }, { "hicut", 9000 },
                                 { "size", 90 }, { "decay", 10 }, { "rmix", 45 }, { "dmix", 55 }, { "duck", 30 } });
        lab ("Sub Drop Echo", { { "dmode", Stereo }, { "dtime", 3 }, { "fb", 65 }, { "pitch", -12 }, { "locut", 80 }, { "hicut", 4000 }, { "dmix", 55 } });
        lab ("Fuzz Grinder", { { "dmode", Bbd }, { "dtime", 8 }, { "fb", 70 }, { "dist", DriveFuzz }, { "ddrive", 70 }, { "locut", 500 }, { "hicut", 3000 },
                               { "dmix", 55 }, { "rmix", 15 } });
        lab ("Bitcrush Ping", { { "dmode", PingPong }, { "dtime", 4 }, { "fb", 60 }, { "dist", DriveCrush }, { "ddrive", 60 }, { "locut", 800 }, { "rmix", 10 } });
        lab ("Telephone Echo", { { "dmode", Digital }, { "dtime", 3 }, { "fb", 60 }, { "locut", 1200 }, { "hpres", 50 }, { "hicut", 2500 }, { "lpres", 50 },
                                 { "dist", DriveTube }, { "ddrive", 35 } });
        lab ("Resonant Dub", { { "dmode", Tape }, { "dtime", 11 }, { "fb", 85 }, { "locut", 120 }, { "hicut", 900 }, { "lpres", 80 }, { "ddrive", 40 },
                               { "dmix", 65 }, { "rmix", 10 } });
        lab ("Dual Quarter + 3/16", { { "dmode", Dual }, { "dtime", 2 }, { "fb", 50 }, { "locut", 350 }, { "hicut", 7000 }, { "rmix", 20 } });
        lab ("Multi-Tap Roll", { { "dmode", MultiTap }, { "dtime", 1 }, { "fb", 45 }, { "locut", 400 }, { "hicut", 6000 }, { "rmix", 20 } });
        lab ("BBD Chorus Echo", { { "dmode", Bbd }, { "dtime", 3 }, { "fb", 50 }, { "ddrive", 25 }, { "rmix", 20 } });
        lab ("Fifth Spiral Up", { { "dmode", PingPong }, { "dtime", 12 }, { "fb", 72 }, { "pitch", 7 }, { "rmix", 35 } });
        lab ("Detune Smear", { { "dmode", Stereo }, { "dtime", 3 }, { "fb", 65 }, { "psnap", 0 }, { "pitch", 0.3f }, { "pmix", 50 }, { "rmix", 30 } });
        lab ("Glitch Crush Spiral", { { "dmode", MultiTap }, { "dtime", 2 }, { "fb", 60 }, { "dist", DriveCrush }, { "ddrive", 50 }, { "pitch", 12 },
                                      { "locut", 500 }, { "rmix", 15 } });
        lab ("Octave Down Shimmer", { { "dmode", Digital }, { "dtime", 1 }, { "fb", 70 }, { "pitch", -12 }, { "pmix", 50 }, { "hicut", 5000 },
                                      { "size", 85 }, { "decay", 9 }, { "rmix", 45 } });
        lab ("Filter Throw Wash", { { "dmode", PingPong }, { "dtime", 11 }, { "fb", 80 }, { "locut", 900 }, { "hpres", 60 }, { "hicut", 3000 },
                                    { "lpres", 60 }, { "size", 80 }, { "decay", 6 }, { "rmix", 35 }, { "duck", 70 } });

        generate (oldCategories, (int) categories().size(), g);
        return list;
    }
}

const std::vector<skazi::Preset>& presets()
{
    static const std::vector<skazi::Preset> list = build();
    return list;
}

skazi::Preset randomPreset (juce::Random& rng)
{
    const auto& cats = categories();
    const auto& cat = cats[(size_t) rng.nextInt ((int) cats.size())];
    skazi::Preset p { "Random", cat.name, skazi::defaultValues (paramDefs()) };
    juce::Random r (rng.nextInt64());
    skazi::PresetGen gen { r, p.values };
    cat.style (gen);
    return p;
}
} // namespace tahini
