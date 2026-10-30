// Checks ZHUG's drive types, auto gain, latency alignment, width, bass mono and presets.
#include "../Source/ZhugEngine.h"
#include "../Source/Presets.h"
#include <cmath>
#include <cstdio>
#include <set>

using namespace zhug;

namespace
{
    int failures = 0;
    void expect (bool ok, const juce::String& what, const juce::String& detail = {})
    {
        if (! ok) ++failures;
        std::printf ("%-48s %s  %s\n", what.toRawUTF8(), ok ? "OK  " : "FAIL", detail.toRawUTF8());
    }

    const double sr = 48000.0;
    constexpr double twoPi = juce::MathConstants<double>::twoPi;

    struct Out { std::vector<float> L, R, inL; int latency = 0; };

    Out run (const Settings& s, int total, const std::function<std::pair<float, float> (int)>& input)
    {
        Engine e;
        e.prepare (sr, 512);
        juce::AudioBuffer<float> buf (2, 512);
        Out o;
        o.latency = e.latency();
        for (int pos = 0; pos < total; pos += 512)
        {
            for (int i = 0; i < 512; ++i)
            {
                const auto [l, r] = input (pos + i);
                buf.setSample (0, i, l); buf.setSample (1, i, r);
                o.inL.push_back (l);
            }
            e.process (buf, s);
            for (int i = 0; i < 512; ++i) { o.L.push_back (buf.getSample (0, i)); o.R.push_back (buf.getSample (1, i)); }
        }
        return o;
    }

    double rms (const std::vector<float>& v, int from) { double s = 0; for (size_t i = (size_t) from; i < v.size(); ++i) s += (double) v[i] * v[i]; return std::sqrt (s / (double) (v.size() - (size_t) from)); }
    bool clean (const std::vector<float>& v) { for (float x : v) if (! std::isfinite (x) || std::abs (x) > 1.0f) return false; return true; }
    double db (double a, double b) { return 20.0 * std::log10 (a / b); }
}

int main()
{
    Settings base = Settings::fromValues ([] (const char* id) { return skazi::defaultValues (paramDefs()).at (id); });
    const auto sine = [] (int i) { const float x = 0.3f * (float) std::sin (twoPi * 220.0 * i / sr); return std::make_pair (x, x); };

    // MIX 0: output is the input, delayed by exactly the reported latency
    {
        auto s = base; s.mix = 0.0f;
        const auto o = run (s, 24000, sine);
        double err = 0;
        for (size_t i = 12000; i < 23000; ++i) err = std::max (err, (double) std::abs (o.L[i] - o.inL[i - (size_t) o.latency]));
        expect (o.latency > 0 && err < 1.0e-4, "dry path matches the reported latency", juce::String (o.latency) + " samples, error " + juce::String (err, 6));
    }

    // every drive type: clean, audible, adds harmonics
    const char* names[] = { "TUBE", "TAPE", "TRANSISTOR", "DIGITAL", "FOLD", "FUZZ", "BIT CRUSH", "RECTIFY" };
    for (int m = 0; m < NumModes; ++m)
    {
        auto s = base; s.mode = m; s.driveDb = 24.0f; s.bias = 0.3f;
        const auto o = run (s, 48000, sine);
        // distortion = what is left after removing the best-fit clean sine
        double num = 0, den = 0;
        for (size_t i = 24000; i < o.L.size(); ++i) { const double ref = o.inL[i - (size_t) o.latency]; num += o.L[i] * ref; den += ref * ref; }
        const double k = num / den;
        double resid = 0, total = 0;
        for (size_t i = 24000; i < o.L.size(); ++i) { const double ref = o.inL[i - (size_t) o.latency]; resid += std::pow (o.L[i] - k * ref, 2.0); total += (double) o.L[i] * o.L[i]; }
        const double thd = std::sqrt (resid / total);
        expect (clean (o.L) && rms (o.L, 24000) > 0.05 && thd > 0.03, juce::String ("drive type ") + names[m], "distortion " + juce::String (thd * 100, 1) + "%");
    }

    // auto gain keeps a -18 dB RMS sine (its reference level) at its level
    {
        const auto ref = [] (int i) { const float x = 0.177f * (float) std::sin (twoPi * 220.0 * i / sr); return std::make_pair (x, x); };
        auto s = base; s.driveDb = 30.0f; s.autoGain = true;
        const auto o = run (s, 96000, ref);
        const double d = db (rms (o.L, 48000), rms (o.inL, 48000));
        expect (std::abs (d) < 2.5, "AUTO GAIN matches input level at +30 dB drive", juce::String (d, 2) + " dB");
    }

    // auto gain never jumps: a quiet part right after a loud one comes in at its own level
    for (int m = 0; m < NumModes; ++m)
    {
        if (m == BitCrush) continue; // crushing a quiet signal breaks it into steps by design
        auto s = base; s.mode = m; s.driveDb = 24.0f; s.autoGain = true; s.bias = 0.0f; // bias adds level-dependent DC by design; this checks the gain
        const auto loudThenQuiet = [] (int i) { const float x = (i < 48000 ? 0.5f : 0.01f) * (float) std::sin (twoPi * 110.0 * i / sr); return std::make_pair (x, x); };
        const auto o = run (s, 144384, loudThenQuiet);
        auto part = [&] (int a, int z) { double m2 = 0; for (int i = a; i < z; ++i) m2 += (double) o.L[(size_t) i] * o.L[(size_t) i]; return std::sqrt (m2 / (z - a)); };
        const double jump = db (part (48500, 50900), part (120000, 144000));
        expect (jump < 3.0, juce::String ("no level jump after a loud part, ") + names[m], juce::String (jump, 1) + " dB");
    }

    // width 0 is mono, width 200 widens
    const auto stereo = [] (int i) { return std::make_pair (0.3f * (float) std::sin (twoPi * 440.0 * i / sr), 0.3f * (float) std::sin (twoPi * 660.0 * i / sr)); };
    {
        auto s = base; s.width = 0.0f; s.bassMono = 20.0f;
        auto o = run (s, 24000, stereo);
        double diff = 0; for (size_t i = 12000; i < o.L.size(); ++i) diff = std::max (diff, (double) std::abs (o.L[i] - o.R[i]));
        expect (diff < 1.0e-4, "WIDTH 0% is mono");
        auto sideOf = [] (const Out& x) { double sd = 0; for (size_t i = 12000; i < x.L.size(); ++i) sd += std::pow (x.L[i] - x.R[i], 2.0); return sd; };
        s.width = 1.0f; const double normal = sideOf (run (s, 24000, stereo));
        s.width = 2.0f; const double wide = sideOf (run (s, 24000, stereo));
        expect (wide > normal * 2.5, "WIDTH 200% widens the stereo image", juce::String (wide / normal, 2) + "x side energy");
    }
    // bass mono: a low, out-of-phase signal gets centred
    {
        const auto lowWide = [] (int i) { const float x = 0.3f * (float) std::sin (twoPi * 50.0 * i / sr); return std::make_pair (x, -x); };
        auto s = base; s.driveDb = 0.0f; s.autoGain = false; s.bassMono = 20.0f;
        const auto off = run (s, 48000, lowWide);
        s.bassMono = 200.0f;
        const auto on = run (s, 48000, lowWide);
        auto side = [] (const Out& x) { double sd = 0; for (size_t i = 24000; i < x.L.size(); ++i) sd += std::pow (x.L[i] - x.R[i], 2.0); return std::sqrt (sd); };
        expect (side (on) < side (off) * 0.2, "BASS MONO centres the low end", juce::String (db (side (on), side (off)), 1) + " dB side");
    }
    // multiband with extreme settings stays clean
    {
        auto s = base; s.multiband = true; s.driveDb = 36.0f; s.lowDrive = 1; s.midDrive = 1; s.highDrive = 1; s.mode = Fuzz;
        juce::Random rnd (9);
        const auto o = run (s, 48000, [&rnd] (int) { const float x = (rnd.nextFloat() - 0.5f) * 1.6f; return std::make_pair (x, x * 0.7f); });
        expect (clean (o.L) && clean (o.R), "multiband at full drive stays clean");
    }

    // presets on a bass-ish loop
    const auto& list = presets();
    std::set<juce::String> pnames;
    std::set<std::map<juce::String, float>> values;
    int bad = 0;
    for (auto& p : list)
    {
        pnames.insert (p.name);
        values.insert (p.values);
        const auto s = Settings::fromValues ([&p] (const char* id) { return p.values.at (id); });
        const auto o = run (s, 24000, [] (int i) { const float x = 0.4f * (float) std::sin (twoPi * 55.0 * i / sr) * (float) std::exp (-(i % 12000) / 4000.0); return std::make_pair (x, x * 0.8f); });
        if (! (clean (o.L) && clean (o.R) && rms (o.L, 0) > 0.01)) ++bad;
    }
    std::printf ("\n%d presets\n", (int) list.size());
    expect (pnames.size() == list.size(), "every preset name unique");
    expect (values.size() == list.size(), "every preset's settings different");
    expect (bad == 0, "every preset processes cleanly", juce::String (bad) + " bad");

    std::printf ("\n%s (%d failures)\n", failures == 0 ? "ALL PASSED" : "FAILED", failures);
    return failures == 0 ? 0 : 1;
}
