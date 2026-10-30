// Checks TAHINI's echo timing, feedback stability, reverb decay, freeze, duck and presets.
#include "../Source/TahiniEngine.h"
#include "../Source/Presets.h"
#include <cmath>
#include <cstdio>
#include <set>

using namespace tahini;

namespace
{
    int failures = 0;
    void expect (bool ok, const juce::String& what, const juce::String& detail = {})
    {
        if (! ok) ++failures;
        std::printf ("%-46s %s  %s\n", what.toRawUTF8(), ok ? "OK  " : "FAIL", detail.toRawUTF8());
    }

    const double sr = 48000.0;

    // input(i) gives the dry sample; returns the processed L and R
    std::pair<std::vector<float>, std::vector<float>> run (const Settings& s, int total, const std::function<float (int)>& input)
    {
        Engine e;
        e.prepare (sr, 2);
        juce::AudioBuffer<float> buf (2, 256);
        std::vector<float> L, R;
        for (int pos = 0; pos < total; pos += 256)
        {
            for (int i = 0; i < 256; ++i) { const float x = input (pos + i); buf.setSample (0, i, x); buf.setSample (1, i, x); }
            e.process (buf, s, nullptr);
            for (int i = 0; i < 256; ++i) { L.push_back (buf.getSample (0, i)); R.push_back (buf.getSample (1, i)); }
        }
        return { L, R };
    }

    double rms (const std::vector<float>& v, int from, int to)
    {
        double s = 0; to = std::min (to, (int) v.size());
        for (int i = from; i < to; ++i) s += (double) v[(size_t) i] * v[(size_t) i];
        return std::sqrt (s / std::max (1, to - from));
    }

    bool clean (const std::vector<float>& v) { for (float x : v) if (! std::isfinite (x) || std::abs (x) > 1.0f) return false; return true; }

    // amplitude of one frequency in v[from, to)
    double tone (const std::vector<float>& v, int from, int to, double hz)
    {
        double re = 0, im = 0;
        for (int i = from; i < to; ++i)
        {
            const double ph = juce::MathConstants<double>::twoPi * hz * i / sr;
            re += v[(size_t) i] * std::cos (ph); im += v[(size_t) i] * std::sin (ph);
        }
        return 2.0 * std::sqrt (re * re + im * im) / (to - from);
    }
    double dB (double x) { return 20.0 * std::log10 (std::max (1.0e-9, x)); }
    std::function<float (int)> sine (double hz, float amp = 0.5f)
    {
        return [hz, amp] (int i) { return amp * (float) std::sin (juce::MathConstants<double>::twoPi * hz * i / sr); };
    }
}

int main()
{
    Settings base = Settings::fromValues ([] (const char* id) { return skazi::defaultValues (paramDefs()).at (id); });
    const auto click = [] (int i) { return i == 0 ? 0.8f : 0.0f; };
    const int T = (int) (0.75 * 60.0 / 120.0 * sr); // 1/8 dotted at 120 bpm

    // echo timing: STEREO mode, delay only
    {
        auto s = base; s.mode = Stereo; s.reverbMix = 0; s.duck = 0; s.dry = false; s.wow = 0; s.feedback = 0.5f; s.delayMix = 1.0f;
        const auto [L, R] = run (s, T * 3, click);
        int peakAt = 0; float best = 0;
        for (int i = T / 2; i < T * 3 / 2; ++i) if (std::abs (L[(size_t) i]) > best) { best = std::abs (L[(size_t) i]); peakAt = i; }
        expect (std::abs (peakAt - T) < 48, "first echo lands on 1/8 dotted", juce::String (peakAt) + " vs " + juce::String (T) + " samples");
    }
    // ping-pong: first echo left, second echo right
    {
        auto s = base; s.mode = PingPong; s.reverbMix = 0; s.duck = 0; s.dry = false; s.wow = 0; s.feedback = 0.6f; s.delayMix = 1.0f;
        const auto [L, R] = run (s, T * 3, click);
        const double l1 = rms (L, T - 200, T + 200), r1 = rms (R, T - 200, T + 200);
        const double l2 = rms (L, 2 * T - 200, 2 * T + 200), r2 = rms (R, 2 * T - 200, 2 * T + 200);
        expect (l1 > r1 * 10 && r2 > l2 * 10, "PING-PONG bounces left then right");
    }
    // stability: max feedback for a long time
    {
        auto s = base; s.feedback = 0.95f; s.drive = 1.0f; s.duck = 0;
        juce::Random rnd (3);
        const auto [L, R] = run (s, (int) (sr * 20), [&rnd] (int i) { return i < 48000 ? (rnd.nextFloat() - 0.5f) : 0.0f; });
        expect (clean (L) && clean (R), "95% feedback stays stable for 20 s");
    }
    // reverb decay time: about -60 dB after DECAY seconds
    {
        auto s = base; s.delayMix = 0; s.duck = 0; s.dry = false; s.decay = 2.0f; s.predelayMs = 0; s.damp = 0;
        const auto [L, R] = run (s, (int) (sr * 2.5), click);
        const double early = rms (L, (int) (0.1 * sr), (int) (0.2 * sr)), late = rms (L, (int) (2.0 * sr), (int) (2.1 * sr));
        const double drop = 20.0 * std::log10 (late / early);
        expect (drop < -45.0 && drop > -75.0, "reverb falls ~60 dB over DECAY (2 s)", juce::String (drop, 1) + " dB");
    }
    // freeze holds the tail
    {
        auto s = base; s.delayMix = 0; s.duck = 0; s.dry = false; s.decay = 1.0f;
        juce::Random rnd (4);
        Engine e; e.prepare (sr, 2);
        juce::AudioBuffer<float> buf (2, 256);
        double before = 0, after = 0;
        for (int pos = 0; pos < (int) (sr * 4); pos += 256)
        {
            for (int i = 0; i < 256; ++i) { const float x = pos < 24000 ? rnd.nextFloat() - 0.5f : 0.0f; buf.setSample (0, i, x); buf.setSample (1, i, x); }
            s.freeze = pos > 26000;
            e.process (buf, s, nullptr);
            const double m = buf.getRMSLevel (0, 0, 256);
            if (pos > 30000 && pos < 40000) before = std::max (before, m);
            if (pos > (int) (sr * 3.5)) after = std::max (after, m);
        }
        expect (after > before * 0.3, "FREEZE keeps the tail ringing", juce::String (before, 4) + " -> " + juce::String (after, 4));
    }
    // duck: wet is pushed down while the dry signal plays
    {
        auto s = base; s.dry = false; s.mode = Stereo;
        const auto tone = [] (int i) { return 0.5f * (float) std::sin (i * 0.05); };
        s.duck = 0.0f;
        const double open = rms (run (s, (int) sr * 2, tone).first, (int) sr, (int) sr * 2);
        s.duck = 1.0f;
        const double ducked = rms (run (s, (int) sr * 2, tone).first, (int) sr, (int) sr * 2);
        expect (ducked < open * 0.3, "DUCK pushes the wet down under the signal", juce::String (open, 3) + " -> " + juce::String (ducked, 3));
    }
    // dry off: no direct sound
    {
        auto s = base; s.dry = false;
        const auto [L, R] = run (s, 512, click);
        expect (std::abs (L[0]) < 1.0e-4f, "DRY off removes the direct sound");
    }

    // TAPE FX ---------------------------------------------------------------
    auto tapeBase = base;
    tapeBase.mode = Stereo; tapeBase.reverbMix = 0; tapeBase.duck = 0; tapeBase.dry = false; tapeBase.wow = 0;
    tapeBase.delayMix = 1.0f; tapeBase.feedback = 0.7f; tapeBase.loCut = 20; tapeBase.hiCut = 20000;
    tapeBase.width = 0.5f; // the echoes as recorded: these tests read the left side on its own
    const auto burst = [] (int i) { return i < 4800 ? 0.5f * (float) std::sin (juce::MathConstants<double>::twoPi * 440.0 * i / sr) : 0.0f; };
    auto freqAt = [] (const std::vector<float>& v, int from, int to)
    {
        int crossings = 0;
        for (int i = from + 1; i < to; ++i) if (v[(size_t) i - 1] < 0.0f && v[(size_t) i] >= 0.0f) ++crossings;
        return crossings * sr / (to - from);
    };
    {
        auto s = tapeBase; s.pitch = 12.0f;
        const auto [L, R] = run (s, T * 3, burst);
        const double f1 = freqAt (L, T + 800, T + 4000), f2 = freqAt (L, 2 * T + 800, 2 * T + 4000);
        expect (std::abs (f1 - 440.0) < 25.0 && std::abs (f2 - 880.0) < 45.0, "PITCH +12: each echo an octave higher",
                juce::String (f1, 0) + " Hz -> " + juce::String (f2, 0) + " Hz");
    }
    {
        // a hit that starts loud and fades: played backwards the echo swells instead
        const auto hit = [] (int i) { return i < 9000 ? 0.6f * (float) std::sin (i * 0.06) * (float) std::exp (-i / 1500.0) : 0.0f; };
        auto s = tapeBase; s.feedback = 0.0f;
        const auto [Ln, Rn] = run (s, T * 2, hit);
        s.reverse = 1.0f;
        const auto [Lr, Rr] = run (s, T * 2, hit);
        const double nEarly = rms (Ln, T, T + 3000), nLate = rms (Ln, T + 6000, T + 9000);
        const double rEarly = rms (Lr, T, T + 3000), rLate = rms (Lr, T + 6000, T + 9000);
        expect (nEarly > nLate * 3 && rLate > rEarly, "REVERSE plays the echo backwards",
                "normal " + juce::String (nEarly / std::max (1e-9, nLate), 1) + "x early, reversed " + juce::String (rLate / std::max (1e-9, rEarly), 1) + "x late");
    }
    {
        auto s = tapeBase; s.tapeStop = 1.0f; s.age = 1.0f; s.wow = 1.0f; s.reverse = 0.5f; s.pitch = -7.0f; s.feedback = 0.9f; s.mode = Tape;
        juce::Random rnd (8);
        const auto [L, R] = run (s, (int) (sr * 15), [&rnd] (int i) { return i % 48000 < 4000 ? (rnd.nextFloat() - 0.5f) * 0.8f : 0.0f; });
        expect (clean (L) && clean (R) && rms (L, 0, (int) L.size()) > 0.005, "all tape FX maxed at 90% feedback stays clean for 15 s");
    }

    // 1.1 ECHO FILTERS -------------------------------------------------------
    // one echo, nothing else: what comes out is the input through the echo filters + drive
    auto echoOnly = base;
    echoOnly.mode = Digital; echoOnly.feedback = 0; echoOnly.dry = false; echoOnly.reverbMix = 0; echoOnly.duck = 0;
    echoOnly.wow = 0; echoOnly.delayMix = 1.0f; echoOnly.drive = 0; echoOnly.loCut = 20; echoOnly.hiCut = 20000;
    const int W0 = T + 4800, W1 = W0 + 9600; // a whole number of cycles for every test frequency
    auto echoLevel = [&] (const Settings& s, double hz, float amp = 0.05f) { return tone (run (s, W1 + 256, sine (hz, amp)).first, W0, W1, hz); };
    auto response = [&] (const Settings& s, double hz) { return dB (echoLevel (s, hz) / echoLevel (echoOnly, hz)); };
    {
        auto s = echoOnly; s.loCut = 1000;
        const double at = response (s, 1000), below = response (s, 250), above = response (s, 4000);
        expect (std::abs (at + 3.0) < 1.0 && below < -10.0 && above > -1.0, "LOW CUT 1 kHz: -3 dB at 1k, gone below",
                juce::String (below, 1) + " / " + juce::String (at, 1) + " / " + juce::String (above, 1) + " dB at 250 / 1k / 4k");
        s.loRes = 1.0f;
        const double peak = response (s, 1000);
        expect (peak > 9.0 && peak < 14.0, "LOW CUT reso 100%: peak at the cutoff", juce::String (peak, 1) + " dB at 1k");
    }
    {
        auto s = echoOnly; s.hiCut = 2000;
        const double at = response (s, 2000), below = response (s, 500), above = response (s, 8000);
        expect (std::abs (at + 6.0) < 1.0 && above < -20.0 && below > -1.0, "HIGH CUT 2 kHz: -6 dB at 2k, steep above",
                juce::String (below, 1) + " / " + juce::String (at, 1) + " / " + juce::String (above, 1) + " dB at 500 / 2k / 8k");
        s.hiRes = 1.0f;
        const double peak = response (s, 2000);
        expect (peak > 9.0 && peak < 14.0, "HIGH CUT reso 100%: peak at the cutoff", juce::String (peak, 1) + " dB at 2k");
    }
    {
        // the filters sit in the loop: each repeat is darker (or thinner) than the one before
        auto s = echoOnly; s.feedback = 0.8f;
        auto perRepeat = [&] (const Settings& st, double hz)
        {
            const auto L = run (st, 2 * T + 3600, [hz] (int i) { return i < 4800 ? 0.05f * (float) std::sin (juce::MathConstants<double>::twoPi * hz * i / sr) : 0.0f; }).first;
            return tone (L, 2 * T + 1000, 2 * T + 3400, hz) / std::max (1.0e-9, tone (L, T + 1000, T + 3400, hz));
        };
        s.hiCut = 2000;
        const double hiKeep = perRepeat (s, 6000), loKeep = perRepeat (s, 300);
        expect (hiKeep < loKeep * 0.3, "HIGH CUT darkens every repeat", "6 kHz keeps " + juce::String (hiKeep, 2) + "x per repeat, 300 Hz " + juce::String (loKeep, 2) + "x");
        s.hiCut = 20000; s.loCut = 1000;
        const double bassKeep = perRepeat (s, 150), midKeep = perRepeat (s, 3000);
        expect (bassKeep < midKeep * 0.3, "LOW CUT thins every repeat", "150 Hz keeps " + juce::String (bassKeep, 2) + "x per repeat, 3 kHz " + juce::String (midKeep, 2) + "x");
    }

    {
        // resonance must not eat the tail: FEEDBACK 85% with a resonant high cut still rings on
        auto s = echoOnly; s.feedback = 0.85f; s.hiCut = 900; s.hiRes = 0.8f;
        const auto L = run (s, 4 * T + 3600, [] (int i) { return i < 4800 ? 0.05f * (float) std::sin (juce::MathConstants<double>::twoPi * 900.0 * i / sr) : 0.0f; }).first;
        const double e1 = tone (L, T + 1000, T + 3400, 900.0), e4 = tone (L, 4 * T + 1000, 4 * T + 3400, 900.0);
        expect (e4 > e1 * 0.3, "RESO 80% keeps the feedback tail", "repeat 4 at " + juce::String (e4 / std::max (1.0e-9, e1), 2) + "x of repeat 1");
    }

    // 1.1 DRIVE --------------------------------------------------------------
    {
        auto residual = [&] (const Settings& s)
        {
            const auto L = run (s, W1 + 256, sine (440.0, 0.5f)).first;
            const double total = rms (L, W0, W1), fund = tone (L, W0, W1, 440.0) / std::sqrt (2.0);
            return std::sqrt (std::max (0.0, total * total - fund * fund)) / std::max (1.0e-9, fund);
        };
        const double clean0 = residual (echoOnly);
        static const char* names[] = { "TAPE", "TUBE", "FUZZ", "CRUSH" };
        for (int t = 0; t < 4; ++t)
        {
            auto s = echoOnly; s.driveType = t; s.drive = 0.8f;
            const double r = residual (s);
            expect (r > 0.1 && r > clean0 * 3.0, juce::String ("DRIVE ") + names[t] + " at 80% distorts",
                    juce::String (r * 100.0, 1) + "% residual (drive 0: " + juce::String (clean0 * 100.0, 1) + "%)");
            s.drive = 1.0f;
            const double quiet = echoLevel (s, 440.0, 0.01f) / echoLevel (echoOnly, 440.0, 0.01f);
            expect (quiet < 1.05, juce::String ("DRIVE ") + names[t] + " adds no gain to quiet echoes", juce::String (quiet, 3) + "x");
        }
        auto second = [&] (int type)
        {
            auto s = echoOnly; s.driveType = type; s.drive = 0.6f;
            const auto L = run (s, W1 + 256, sine (440.0, 0.5f)).first;
            return tone (L, W0, W1, 880.0) / tone (L, W0, W1, 440.0);
        };
        const double tapeH2 = second (DriveTape), tubeH2 = second (DriveTube);
        expect (tubeH2 > 0.03 && tapeH2 < 0.01, "TUBE is lopsided (2nd harmonic), TAPE is not",
                "H2 " + juce::String (tubeH2 * 100.0, 1) + "% vs " + juce::String (tapeH2 * 100.0, 2) + "%");
    }

    // 1.1 PITCH --------------------------------------------------------------
    {
        // frequency from the first to the last rising zero crossing, interpolated
        auto pitchOf = [] (const std::vector<float>& v, int from, int to)
        {
            double first = -1, last = -1; int count = 0;
            for (int i = from + 1; i < to; ++i)
                if (v[(size_t) i - 1] < 0.0f && v[(size_t) i] >= 0.0f)
                {
                    const double t = i - 1 + v[(size_t) i - 1] / (v[(size_t) i - 1] - v[(size_t) i]);
                    if (first < 0) first = t;
                    last = t; ++count;
                }
            return count > 1 ? (count - 1) * sr / (last - first) : 0.0;
        };
        const auto longBurst = [] (int i) { return i < 9600 ? 0.3f * (float) std::sin (juce::MathConstants<double>::twoPi * 440.0 * i / sr) : 0.0f; };
        auto secondEcho = [&] (const Settings& s) { const auto L = run (s, 2 * T + 9600, longBurst).first; return pitchOf (L, 2 * T + 1500, 2 * T + 9000); };
        for (float st : { -24.0f, -12.0f, 7.0f, 12.0f, 24.0f })
        {
            auto s = tapeBase; s.pitch = st;
            const double want = 440.0 * std::exp2 (st / 12.0), got = secondEcho (s);
            expect (std::abs (got / want - 1.0) < 0.03, "PITCH " + juce::String (st > 0 ? "+" : "") + juce::String ((int) st) + ": repeat at x" + juce::String (std::exp2 (st / 12.0), 3),
                    juce::String (got, 1) + " Hz, want " + juce::String (want, 1));
        }
        {
            auto s = tapeBase; s.pitch = 0.5f; s.pitchSnap = false;
            const double free = secondEcho (s);
            s.pitch = 0.4f; s.pitchSnap = true;
            const double snapped = secondEcho (s);
            expect (std::abs (free - 452.8) < 3.0 && std::abs (snapped - 440.0) < 3.0, "PITCH free +0.5 st / SNAP rounds 0.4 to 0",
                    juce::String (free, 1) + " Hz / " + juce::String (snapped, 1) + " Hz");
        }
        {
            auto s = tapeBase; s.pitch = 12.0f; s.pitchMix = 0.5f;
            const auto L = run (s, 2 * T + 9600, longBurst).first;
            const double a = tone (L, 2 * T + 1500, 2 * T + 9500, 440.0), b = tone (L, 2 * T + 1500, 2 * T + 9500, 880.0);
            expect (a > 0.3 * b && b > 0.3 * a, "PITCH BLEND 50% at +12: octave stack (shimmer)",
                    "440 Hz " + juce::String (a, 3) + ", 880 Hz " + juce::String (b, 3));
        }
    }

    // 1.1 DELAY TYPES --------------------------------------------------------
    {
        auto s = echoOnly; s.mode = Dual;
        const auto [L, R] = run (s, T * 2, click);
        auto peakAt = [] (const std::vector<float>& v, int from, int to)
        {
            int at = from; float best = 0;
            for (int i = from; i < to; ++i) if (std::abs (v[(size_t) i]) > best) { best = std::abs (v[(size_t) i]); at = i; }
            return at;
        };
        const int l = peakAt (L, 100, T * 3 / 2), r = peakAt (R, 100, T * 3 / 2);
        expect (std::abs (l - T) < 48 && std::abs (r - T * 3 / 4) < 48, "DUAL: left on the beat, right at 3/4", juce::String (l) + " / " + juce::String (r) + " samples");
    }
    {
        auto s = echoOnly; s.mode = MultiTap;
        const auto [L, R] = run (s, T * 2, click);
        auto at = [&] (const std::vector<float>& v, double frac) { const int c = (int) (T * frac); return rms (v, c - 100, c + 100); };
        const double quiet = rms (L, T / 8 - 100, T / 8 + 100) + 1.0e-6;
        expect (at (L, 0.25) > quiet * 20 && at (R, 0.5) > quiet * 20 && at (L, 0.75) > quiet * 20 && at (L, 1.0) > quiet * 20 && at (R, 1.0) > quiet * 20,
                "MULTI-TAP: taps at 1/4, 1/2, 3/4 and the full echo");
    }
    {
        // RMS, not one frequency: the BBD wobble moves the pitch a little
        auto level = [&] (const Settings& st) { return rms (run (st, W1 + 256, sine (6000.0, 0.05f)).first, W0, W1); };
        auto s = echoOnly;
        const double digital = level (s);
        s.mode = Bbd;
        const double bbdLevel = level (s);
        expect (bbdLevel < digital * 0.4, "BBD is darker than DIGITAL", "6 kHz: " + juce::String (dB (bbdLevel / digital), 1) + " dB");
    }

    // everything up at once: echoes stay bounded and die away
    {
        static const char* modes[] = { "PING-PONG", "STEREO", "TAPE", "DIGITAL", "BBD", "MULTI-TAP", "DUAL" };
        int unbounded = 0; juce::String worst;
        double worstRatio = 0;
        for (int m = 0; m < 7; ++m)
            for (int t = 0; t < 4; ++t)
                for (int narrow = 0; narrow < 2; ++narrow)
                {
                    auto s = echoOnly; s.mode = m; s.driveType = t; s.drive = 1.0f; s.feedback = 0.95f;
                    s.pitch = (float) ((m + t) % 3 - 1) * 24.0f;
                    if (narrow) { s.loCut = 800; s.hiCut = 1200; s.loRes = s.hiRes = 1.0f; }
                    juce::Random rnd (m * 10 + t);
                    const auto [L, R] = run (s, (int) (sr * 14), [&rnd] (int i) { return i < 24000 ? (rnd.nextFloat() - 0.5f) * 1.8f : 0.0f; });
                    const double early = rms (L, (int) sr, (int) sr * 3) + rms (R, (int) sr, (int) sr * 3);
                    const double late = rms (L, (int) sr * 12, (int) sr * 14) + rms (R, (int) sr * 12, (int) sr * 14);
                    const double ratio = late / std::max (1.0e-9, early);
                    if (ratio > worstRatio) { worstRatio = ratio; worst = juce::String (modes[m]) + " drive " + juce::String (t) + (narrow ? " reso" : ""); }
                    if (! (clean (L) && clean (R)) || ratio > 0.7) ++unbounded;
                }
        expect (unbounded == 0, "95% feedback + full drive + reso, every type: bounded, fades",
                juce::String (unbounded) + " bad; slowest " + worst + " keeps " + juce::String (worstRatio, 2) + "x after 11 s");
    }

    // 1.0 projects: old IDs still there, the 1.0 presets keep their numbers, new controls default to off
    {
        const auto defs = skazi::defaultValues (paramDefs());
        bool ids = true;
        for (auto* id : { "dmode", "pitch", "reverse", "tstop", "age", "dtime", "fb", "locut", "hicut", "wow", "ddrive", "freeze", "size", "decay",
                          "predelay", "damp", "width", "duck", "duckrel", "dmix", "rmix", "dry", "gain" })
            ids = ids && defs.count (id) == 1;
        const auto& l = presets();
        static const char* oldCats[] = { "PSY SPACE", "GOA CAVE", "DUB ECHO", "TAPE ECHO", "VOCAL THROW", "BIG HALL", "ROOMS", "CRAZY TAPE", "PITCH SPIRAL", "AMBIENT WASH" };
        bool order = l.size() > 150 && l[0].name == "Init" && l[9].name == "Backwards Echo" && l[10].name == skazi::presetName (400);
        for (int i = 10; i < 150 && order; ++i) order = l[(size_t) i].category == oldCats[(i - 10) / 14];
        const auto d = Settings::fromValues ([&defs] (const char* id) { return defs.at (id); });
        const bool neutral = d.loRes == 0 && d.hiRes == 0 && d.driveType == DriveTape && d.pitchMix == 1.0f && d.mode == Tape;
        expect (ids && order && neutral, "1.0 IDs, preset order and sound defaults kept");
    }

    // presets with a drum-ish input
    const auto& list = presets();
    std::set<juce::String> names;
    std::set<std::map<juce::String, float>> values;
    int bad = 0;
    for (auto& p : list)
    {
        names.insert (p.name);
        values.insert (p.values);
        const auto s = Settings::fromValues ([&p] (const char* id) { return p.values.at (id); });
        const auto [L, R] = run (s, (int) (sr * 3), [] (int i) { const int ph = i % 24000; return ph < 2400 ? 0.6f * (float) std::sin (ph * 0.02) * (1.0f - ph / 2400.0f) : 0.0f; });
        if (! (clean (L) && clean (R) && rms (L, 0, (int) L.size()) > 0.005)) ++bad;
    }
    std::printf ("\n%d presets\n", (int) list.size());
    expect (names.size() == list.size(), "every preset name unique");
    expect (values.size() == list.size(), "every preset's settings different");
    expect (bad == 0, "every preset processes cleanly", juce::String (bad) + " bad");

    std::printf ("\n%s (%d failures)\n", failures == 0 ? "ALL PASSED" : "FAILED", failures);
    return failures == 0 ? 0 : 1;
}
