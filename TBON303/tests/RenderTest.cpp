// Offline checks for the JUCE-free DSP core:
//   1. diode ladder resonant peak lands on the requested cutoff
//   2. max resonance + hot drive stays bounded (no blow-ups / NaNs)
//   3. renders the default-style acid pattern through every drive and filter-2 mode to WAV
//
// Usage: Skazi303RenderTest [output-dir]

#include "dsp/AcidVoice.h"
#include "dsp/Effects.h"
#include "dsp/PatternGenerator.h"
#include "dsp/PresetBank.h"
#include <set>
#include <tuple>
#include "dsp/Sequencer.h"

#include <chrono>
#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

using namespace s303;

namespace
{
int failures = 0;

void check (bool ok, const std::string& what)
{
    std::printf ("[%s] %s\n", ok ? "PASS" : "FAIL", what.c_str());
    if (! ok)
        ++failures;
}

void writeWav (const std::string& path, const std::vector<float>& l, const std::vector<float>& r, int sr)
{
    std::ofstream f (path, std::ios::binary);
    auto u32 = [&] (uint32_t v) { f.write ((const char*) &v, 4); };
    auto u16 = [&] (uint16_t v) { f.write ((const char*) &v, 2); };
    const uint32_t frames = (uint32_t) l.size();
    f.write ("RIFF", 4); u32 (36 + frames * 4); f.write ("WAVE", 4);
    f.write ("fmt ", 4); u32 (16); u16 (1); u16 (2); u32 ((uint32_t) sr); u32 ((uint32_t) sr * 4); u16 (4); u16 (16);
    f.write ("data", 4); u32 (frames * 4);
    for (size_t i = 0; i < l.size(); ++i)
    {
        for (float v : { l[i], r[i] })
        {
            const int s = (int) std::lround (std::clamp (v, -1.0f, 1.0f) * 32767.0f);
            u16 ((uint16_t) (int16_t) s);
        }
    }
}

double measureGain (double cutoff, double reso, double freq, double sr)
{
    DiodeLadder f;
    f.prepare (sr);
    const int n = (int) (sr * 0.5);
    double peak = 0.0;
    for (int i = 0; i < n; ++i)
    {
        const float x = (float) (0.05 * std::sin (kTwoPi * freq * i / sr)); // small: stay linear
        const float y = f.process (x, cutoff, reso);
        if (i > n / 2)
            peak = std::max (peak, (double) std::fabs (y));
    }
    return peak / 0.05;
}

struct Render
{
    std::vector<float> l, r;
    float peak = 0.0f;
    double rms = 0.0;
    bool finite = true;
};

Render renderPattern (const Pattern& pat, VoiceParams vp, double bpm, int bars, double sr)
{
    AcidVoice voice;
    voice.prepare (sr * 2.0);
    StepSequencer seq;
    seq.reset();
    ModFX mod;
    mod.prepare (sr);
    BBDDelay delay;
    delay.prepare (sr);

    Render out;
    const int n = (int) (bars * 4 * 60.0 / bpm * sr);
    out.l.resize ((size_t) n);
    out.r.resize ((size_t) n);
    const double ppqPerSample = bpm / (60.0 * sr);
    double ppq = 0.0, sum = 0.0;

    for (int i = 0; i < n; ++i)
    {
        const auto ev = seq.tick (ppq * 4.0, pat, 0.1f, 0.5f, 36);
        if (ev.type == SeqEvent::NoteOn) voice.noteOn (ev.note, ev.accent, ev.slide);
        else if (ev.type == SeqEvent::NoteOff) voice.noteOff();
        ppq += ppqPerSample;

        // Crude 2x decimation is fine for a test render.
        const float a = voice.render (vp);
        const float b = voice.render (vp);
        const float m = 0.5f * (a + b);

        float l, r;
        mod.process (m, 1, 0.4f, 0.5f, 0.3f, l, r);
        delay.process (l, r, 0.75 * 60.0 / bpm, 0.4f, 2800.0f, 0.2f, l, r);
        out.l[(size_t) i] = l;
        out.r[(size_t) i] = r;

        if (! std::isfinite (l) || ! std::isfinite (r))
            out.finite = false;
        out.peak = std::max (out.peak, std::max (std::fabs (l), std::fabs (r)));
        sum += (double) l * l;
    }
    out.rms = std::sqrt (sum / n);
    return out;
}
} // namespace

int main (int argc, char** argv)
{
    const std::string dir = argc > 1 ? argv[1] : ".";
    const double sr = 44100.0;

    // 1. Resonant peak placement (run at the voice's 2x rate).
    for (double fc : { 200.0, 1000.0, 4000.0 })
    {
        double best = 0.0, bestF = 0.0;
        for (double f = fc * 0.5; f < fc * 2.0; f *= 1.01)
        {
            const double gn = measureGain (fc, 0.8, f, sr * 2.0);
            if (gn > best) { best = gn; bestF = f; }
        }
        const double ratio = bestF / fc;
        char msg[160];
        std::snprintf (msg, sizeof msg, "diode peak for cutoff %.0f Hz at %.0f Hz (ratio %.2f, gain %.1f dB)",
                       fc, bestF, ratio, 20.0 * std::log10 (best));
        check (ratio > 0.85 && ratio < 1.15, msg);
    }

    // 1b. Bass hold-up at high resonance (the 303's feedback high-pass keeps the low end).
    {
        const double sr2 = sr * 2.0;
        std::printf ("  bass vs resonance (cutoff 1 kHz):\n");
        for (double res : { 0.0, 0.5, 0.7, 0.85 })
            std::printf ("    reso %.1f:  55Hz %+.1f dB  110Hz %+.1f dB  220Hz %+.1f dB  peak(1k) %+.1f dB\n", res,
                         20.0 * std::log10 (measureGain (1000.0, res, 55.0, sr2)),
                         20.0 * std::log10 (measureGain (1000.0, res, 110.0, sr2)),
                         20.0 * std::log10 (measureGain (1000.0, res, 220.0, sr2)),
                         20.0 * std::log10 (measureGain (1000.0, res, 1000.0, sr2)));
        const double loss = 20.0 * std::log10 (measureGain (1000.0, 0.85, 55.0, sr2) / measureGain (1000.0, 0.0, 55.0, sr2));
        char msg[120];
        std::snprintf (msg, sizeof msg, "55 Hz bass at full resonance is %.1f dB vs no resonance (303-like: within -8 dB)", loss);
        check (loss > -8.0, msg);
    }

    // 1c. Full resonance screams: after one kick it keeps ringing by itself, on the cutoff, bounded.
    {
        const double sr2 = sr * 2.0;
        DiodeLadder f;
        f.prepare (sr2);
        std::vector<float> y;
        for (int i = 0; i < (int) sr2; ++i)
            y.push_back (f.process (i < 64 ? 0.5f : 0.0f, 1000.0, 1.0));
        double sum = 0.0; float peak = 0.0f; int crossings = 0;
        const int from = (int) (sr2 * 0.5);
        for (int i = from; i < (int) y.size(); ++i)
        {
            sum += (double) y[(size_t) i] * y[(size_t) i];
            peak = std::max (peak, std::fabs (y[(size_t) i]));
            if (y[(size_t) i - 1] < 0.0f && y[(size_t) i] >= 0.0f) ++crossings;
        }
        const double rms = std::sqrt (sum / (y.size() - from)), hz = crossings / 0.5;
        char msg[160];
        std::snprintf (msg, sizeof msg, "max resonance self-oscillates: %.0f Hz (cutoff 1000), rms %.2f, peak %.2f", hz, rms, peak);
        check (rms > 0.1 && peak < 3.0f && std::abs (hz / 1000.0 - 1.0) < 0.12, msg);
    }

    // 2. Stability at the extremes.
    {
        DiodeLadder f;
        f.prepare (sr * 2.0);
        Noise nz;
        bool ok = true;
        float peak = 0.0f;
        for (int i = 0; i < (int) sr * 4; ++i)
        {
            const double cutoff = 30.0 * std::pow (2.0, 9.0 * (0.5 + 0.5 * std::sin (i * 1.0e-4)));
            const float y = f.process (nz.next() * 4.0f, cutoff, 1.0);
            ok = ok && std::isfinite (y);
            peak = std::max (peak, std::fabs (y));
        }
        char msg[120];
        std::snprintf (msg, sizeof msg, "diode at max resonance, hot noise, swept cutoff: finite, peak %.2f", peak);
        check (ok && peak < 20.0f, msg);
    }

    // 3. Pattern renders.
    Pattern pat;
    const int notes[16] = { 0, 0, 12, 0, 3, 0, 7, 10, 0, 0, 12, 3, 5, 0, 7, 0 };
    const int octs[16]  = { 0, 0, 0, 1, 0, 0, 0, 0, 0, -1, 0, 0, 0, 0, 0, 1 };
    const bool gate[16] = { 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1 };
    const bool acc[16]  = { 1, 0, 0, 1, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0 };
    const bool sld[16]  = { 0, 0, 1, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 1 };
    for (int i = 0; i < 16; ++i)
        pat.steps[i] = { notes[i], octs[i], gate[i], acc[i], sld[i] };

    struct Case { const char* name; VoiceParams vp; int length; };
    std::vector<Case> cases;

    VoiceParams base;
    base.cutoffHz = 420.0f; base.resonance = 0.75f; base.envMod = 0.6f; base.decaySec = 0.45f;
    base.accent = 0.7f; base.postDrive = 0.25f;
    cases.push_back ({ "classic_saw", base, 16 });

    auto sq = base; sq.wave = 1.0f; sq.subLevel = 0.6f;
    cases.push_back ({ "square_sub", sq, 16 });

    auto pre = base; pre.preDrive = 0.7f; pre.resonance = 0.95f; pre.postDrive = 0.6f; pre.postType = DistType::Fuzz;
    cases.push_back ({ "screaming_pre_fuzz", pre, 16 });

    auto fold = base; fold.postDrive = 0.8f; fold.postType = DistType::Wavefold;
    cases.push_back ({ "wavefold", fold, 15 });

    auto crush = base; crush.postDrive = 0.6f; crush.postType = DistType::Bitcrush;
    cases.push_back ({ "bitcrush", crush, 16 });

    const char* newDrives[] = { "overdrive", "distortion", "super_dist", "ring_mod", "rectifier", "diode",
                                "tape", "transistor", "germanium", "crunch" };
    for (int t = 0; t < 10; ++t)
    {
        auto d = base;
        d.postType = (DistType) (4 + t);
        d.postDrive = 0.7f;
        d.preType = (DistType) (4 + t);
        d.preDrive = 0.35f;
        cases.push_back ({ newDrives[t], d, 16 });
    }

    auto lad = base; lad.f2Type = Filter2Type::Ladder24; lad.f2CutoffHz = 1500.0f; lad.f2Resonance = 0.7f;
    cases.push_back ({ "ladder24", lad, 16 });

    auto form = base; form.f2Type = Filter2Type::Formant; form.f2Vowel = 2.5f; form.f2Resonance = 0.6f;
    cases.push_back ({ "formant", form, 16 });

    for (auto& c : cases)
    {
        auto p = pat;
        p.length = c.length;
        const auto r = renderPattern (p, c.vp, 128.0, 4, sr);
        writeWav (dir + "/" + c.name + ".wav", r.l, r.r, (int) sr);
        char msg[200];
        std::snprintf (msg, sizeof msg, "%-20s finite=%d peak=%.2f rms=%.3f", c.name, (int) r.finite, r.peak, r.rms);
        check (r.finite && r.peak < 4.0f && r.rms > 0.01, msg);
    }

    // 4. Generator stays in scale and only slides into notes.
    {
        PatternGenerator gen (42);
        GenSettings gs;
        gs.scale = 1; // phrygian
        bool ok = true;
        for (int t = 0; t < 200; ++t)
        {
            Pattern gp;
            gen.generate (gp, gs);
            for (int i = 0; i < 16; ++i)
            {
                const auto& s = gp.steps[i];
                const auto& deg = scales()[1].degrees;
                const bool inScale = s.note == 12 || std::find (deg.begin(), deg.end(), s.note) != deg.end();
                ok = ok && inScale && (! s.slide || gp.steps[(i + 1) % 16].gate);
            }
        }
        check (ok, "generator: 200 patterns stay in scale, slides only into notes");
    }

    // 5. Preset variety: how many distinct sound "recipes" the factory bank uses.
    {
        auto val = [] (const Preset& p, const char* id)
        {
            for (auto& kv : p.values)
                if (kv.first == id) return kv.second;
            return 0.0f;
        };
        std::set<std::tuple<int, int, int, int, int, int, int>> recipes;
        std::set<int> fxScenes;
        const int n = numFactoryPresets();
        for (int i = 0; i < n; ++i)
        {
            const auto p = factoryPreset (i);
            const int fx = (int) val (p, "mod_mode") * 100 + (val (p, "dly_mix") > 0.05f ? 10 : 0) + (val (p, "rev_mix") > 0.1f ? 1 : 0);
            fxScenes.insert (fx);
            recipes.insert ({ (int) val (p, "post_type"), (int) (val (p, "pre_drive") > 0.05f ? val (p, "pre_type") : -1),
                              (int) val (p, "f2_type"), (int) (val (p, "wave") * 3.0f), (int) (val (p, "sub_level") > 0.1f ? val (p, "sub_wave") : -1),
                              fx, (int) val (p, "seq_len") });
        }
        char msg[160];
        std::snprintf (msg, sizeof msg, "preset variety: %d distinct recipes out of %d presets, %d FX combinations",
                       (int) recipes.size(), n, (int) fxScenes.size());
        check (recipes.size() > (size_t) (n * 8 / 10), msg);
    }

    // 5b. Wheels: pitch bend moves the note by the set range, the mod wheel brightens the filter
    {
        const double sr2 = 88200.0;
        double octavesUp = 0.0;
        auto play = [&] (double bendSemis, double modWheel, double& hz, double& bright)
        {
            AcidVoice v;
            v.prepare (sr2);
            VoiceParams p;
            p.cutoffHz = (float) (300.0 * std::exp2 (octavesUp)); p.resonance = 0.0f; p.envMod = 0.0f; p.drift = 0.0f; p.ampDecaySec = 8.0f;
            v.setBend (bendSemis);
            v.setModWheel (modWheel, 0, 1.0);
            v.noteOn (48, false, false);
            std::vector<float> y;
            for (int i = 0; i < (int) sr2; ++i) y.push_back (v.render (p));
            int crossings = 0; double sum = 0.0, diff = 0.0;
            const int from = (int) (sr2 * 0.5);
            for (int i = from; i < (int) y.size(); ++i)
            {
                if (y[(size_t) i - 1] < 0.0f && y[(size_t) i] >= 0.0f) ++crossings;
                sum += std::fabs (y[(size_t) i]);
                diff += std::fabs (y[(size_t) i] - y[(size_t) i - 1]);
            }
            hz = crossings / 0.5;
            bright = diff / std::max (1.0e-9, sum);
        };
        double hz0, b0, hzUp, bUp, hzDown, bDown, hzMod, bMod, hzOpen, bOpen;
        play (0.0, 0.0, hz0, b0);
        play (12.0, 0.0, hzUp, bUp);
        play (-24.0, 0.0, hzDown, bDown);
        play (0.0, 1.0, hzMod, bMod);
        octavesUp = 4.0;
        play (0.0, 0.0, hzOpen, bOpen); // the cutoff knob 4 octaves up, what a full mod wheel should sound like
        char msg[200];
        std::snprintf (msg, sizeof msg, "pitch wheel: %.0f Hz, +12 st %.0f Hz, -24 st %.0f Hz", hz0, hzUp, hzDown);
        check (std::abs (hzUp / hz0 - 2.0) < 0.1 && std::abs (hzDown / hz0 - 0.25) < 0.05, msg);
        std::snprintf (msg, sizeof msg, "mod wheel opens the filter 4 octaves: brightness %.4f -> %.4f (cutoff +4 oct: %.4f)", b0, bMod, bOpen);
        check (bOpen > b0 * 1.02 && std::abs (bMod - bOpen) < (bOpen - b0) * 0.2, msg);
    }

    // 6. Cost of building presets: a host may ask for all program names at once
    {
        const auto t0 = std::chrono::steady_clock::now();
        for (int i = 0; i < numFactoryPresets(); ++i)
            factoryPreset (i);
        const double ms = std::chrono::duration<double, std::milli> (std::chrono::steady_clock::now() - t0).count();
        char msg[120];
        std::snprintf (msg, sizeof msg, "building all %d presets takes %.1f ms", numFactoryPresets(), ms);
        check (true, msg);
    }

    std::printf ("%s (%d failure%s)\n", failures ? "FAILED" : "ALL PASSED", failures, failures == 1 ? "" : "s");
    return failures ? 1 : 0;
}
