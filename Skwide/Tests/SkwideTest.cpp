// Loads the built SKWIDE.vst3, proves the mono sum is untouched and renders demo WAVs.
//   SkwideTest <path to SKWIDE.vst3> <render folder>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>

namespace
{
    constexpr double sampleRate = 48000.0;
    constexpr int blockSize = 512;
    int failures = 0;

    void check (bool ok, const juce::String& what)
    {
        std::cout << (ok ? "  PASS  " : "  FAIL  ") << what << std::endl;
        if (! ok)
            ++failures;
    }

    struct Host
    {
        std::unique_ptr<juce::AudioPluginInstance> plugin;

        juce::AudioProcessorParameter* find (const juce::String& name)
        {
            for (auto* p : plugin->getParameters())
                if (p->getName (100) == name)
                    return p;
            std::cout << "  ?? no parameter named " << name << std::endl;
            ++failures;
            return nullptr;
        }

        void set (const juce::String& name, const juce::String& text)
        {
            if (auto* p = find (name))
                p->setValue (p->getValueForText (text));
        }

        void setNormalised (const juce::String& name, float value)
        {
            if (auto* p = find (name))
                p->setValue (value);
        }

        void reset()
        {
            for (auto* p : plugin->getParameters())
                p->setValue (p->getDefaultValue());
            plugin->releaseResources();
            plugin->prepareToPlay (sampleRate, blockSize);
        }

        juce::AudioBuffer<float> run (const juce::AudioBuffer<float>& input, std::function<void (int)> onBlock = nullptr)
        {
            juce::AudioBuffer<float> out (input);
            juce::MidiBuffer midi;
            for (int start = 0, blockIndex = 0; start < out.getNumSamples(); start += blockSize, ++blockIndex)
            {
                if (onBlock)
                    onBlock (blockIndex);
                const int n = juce::jmin (blockSize, out.getNumSamples() - start);
                juce::AudioBuffer<float> block (out.getArrayOfWritePointers(), 2, start, n);
                plugin->processBlock (block, midi);
            }
            return out;
        }
    };

    juce::AudioBuffer<float> noise (double seconds, float level = 0.4f, bool mono = false, int seed = 1234)
    {
        juce::Random r (seed);
        juce::AudioBuffer<float> b (2, (int) (seconds * sampleRate));
        for (int i = 0; i < b.getNumSamples(); ++i)
        {
            const float v = (r.nextFloat() * 2.0f - 1.0f) * level;
            b.setSample (0, i, v);
            b.setSample (1, i, mono ? v : 0.6f * v + 0.4f * (r.nextFloat() * 2.0f - 1.0f) * level);
        }
        return b;
    }

    // Mono "synth lead": detuned saws with a plucky envelope, the classic thing you want to widen.
    juce::AudioBuffer<float> monoLead (double seconds)
    {
        juce::AudioBuffer<float> b (2, (int) (seconds * sampleRate));
        const float notes[] { 220.0f, 261.6f, 329.6f, 293.7f, 220.0f, 196.0f, 246.9f, 329.6f };
        double p1 = 0, p2 = 0;
        const int noteLen = (int) (sampleRate * 0.25);
        for (int i = 0; i < b.getNumSamples(); ++i)
        {
            const float f = notes[(i / noteLen) % 8];
            const float env = std::exp (-3.0f * (float) (i % noteLen) / (float) noteLen);
            p1 = std::fmod (p1 + f / sampleRate, 1.0);
            p2 = std::fmod (p2 + f * 1.006 / sampleRate, 1.0);
            const float v = (float) ((2.0 * p1 - 1.0) + (2.0 * p2 - 1.0)) * 0.22f * env;
            b.setSample (0, i, v);
            b.setSample (1, i, v);
        }
        return b;
    }

    juce::AudioBuffer<float> sine (double seconds, float freq, float leftGain, float rightGain)
    {
        juce::AudioBuffer<float> b (2, (int) (seconds * sampleRate));
        for (int i = 0; i < b.getNumSamples(); ++i)
        {
            const float v = 0.5f * (float) std::sin (juce::MathConstants<double>::twoPi * freq * i / sampleRate);
            b.setSample (0, i, v * leftGain);
            b.setSample (1, i, v * rightGain);
        }
        return b;
    }

    struct Stereo
    {
        double midRms = 0, sideRms = 0, correlation = 0, peak = 0;
        bool finite = true;
    };

    Stereo analyse (const juce::AudioBuffer<float>& b, int skip)
    {
        Stereo s;
        double mm = 0, ss = 0, lr = 0, ll = 0, rr = 0;
        const int n = b.getNumSamples() - skip;
        for (int i = skip; i < b.getNumSamples(); ++i)
        {
            const double l = b.getSample (0, i), r = b.getSample (1, i);
            s.finite = s.finite && std::isfinite (l) && std::isfinite (r);
            s.peak = juce::jmax (s.peak, std::abs (l), std::abs (r));
            mm += 0.25 * (l + r) * (l + r);
            ss += 0.25 * (l - r) * (l - r);
            lr += l * r; ll += l * l; rr += r * r;
        }
        s.midRms = std::sqrt (mm / n);
        s.sideRms = std::sqrt (ss / n);
        s.correlation = lr / std::sqrt (juce::jmax (1.0e-30, ll * rr));
        return s;
    }

    // Largest difference between the mono sums of two buffers.
    double monoError (const juce::AudioBuffer<float>& a, const juce::AudioBuffer<float>& b, int skip)
    {
        double worst = 0;
        for (int i = skip; i < a.getNumSamples(); ++i)
            worst = juce::jmax (worst, std::abs ((double) a.getSample (0, i) + a.getSample (1, i) - b.getSample (0, i) - b.getSample (1, i)));
        return worst;
    }

    juce::String db (double ratio) { return juce::String (20.0 * std::log10 (juce::jmax (1.0e-12, ratio)), 1) + " dB"; }

    void writeWav (const juce::AudioBuffer<float>& b, const juce::File& file)
    {
        file.deleteFile();
        juce::WavAudioFormat wav;
        if (auto stream = file.createOutputStream())
            if (auto writer = std::unique_ptr<juce::AudioFormatWriter> (wav.createWriterFor (stream.get(), sampleRate, 2, 24, {}, 0)))
            {
                stream.release();
                writer->writeFromAudioSampleBuffer (b, 0, b.getNumSamples());
            }
    }
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    if (argc < 3)
    {
        std::cout << "usage: SkwideTest <SKWIDE.vst3> <render folder>" << std::endl;
        return 2;
    }

    juce::VST3PluginFormat format;
    juce::OwnedArray<juce::PluginDescription> types;
    format.findAllTypesForFile (types, argv[1]);
    if (types.isEmpty())
    {
        std::cout << "could not load " << argv[1] << std::endl;
        return 2;
    }

    juce::String error;
    Host host;
    host.plugin = format.createInstanceFromDescription (*types[0], sampleRate, blockSize, error);
    if (host.plugin == nullptr)
    {
        std::cout << "instantiate failed: " << error << std::endl;
        return 2;
    }
    host.plugin->enableAllBuses();
    std::cout << "Loaded " << host.plugin->getName() << " with " << host.plugin->getParameters().size() << " parameters" << std::endl;

    const juce::File renders (argv[2]);
    renders.createDirectory();
    const int skip = (int) (0.2 * sampleRate);

    std::cout << "\n[1] Default settings are transparent" << std::endl;
    host.reset();
    {
        const auto in = noise (1.0);
        const auto out = host.run (in);
        double worst = 0;
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < in.getNumSamples(); ++i)
                worst = juce::jmax (worst, (double) std::abs (in.getSample (ch, i) - out.getSample (ch, i)));
        check (worst < 1.0e-5, "output equals input (max diff " + juce::String (worst, 8) + ")");
    }

    std::cout << "\n[2] Mono sum is untouched by every widening control" << std::endl;
    struct Setting { const char* name; std::vector<std::pair<const char*, const char*>> values; std::vector<std::pair<const char*, float>> toggles; };
    const Setting settings[] {
        { "width 200%",                  { { "Width", "200" } }, {} },
        { "width 0%",                    { { "Width", "0" } }, {} },
        { "spread 100%",                 { { "Mono Spread", "100" } }, {} },
        { "width 200 + spread 100",      { { "Width", "200" }, { "Mono Spread", "100" } }, {} },
        { "phase 137 + drift 180 @ 3Hz", { { "Mono Spread", "100" }, { "Phase", "137" }, { "Drift Depth", "180" }, { "Drift Rate", "3" } }, {} },
        { "mono bass + width 180",       { { "Width", "180" }, { "Low Freq", "250" } }, { { "Mono Bass", 1.0f } } },
        { "low bypass off + spread",     { { "Width", "160" }, { "Mono Spread", "70" } }, { { "Low Bypass", 0.0f } } },
        { "swap L/R + width",            { { "Width", "150" } }, { { "Swap L/R", 1.0f } } },
        { "mix 50% + everything",        { { "Width", "200" }, { "Mono Spread", "100" }, { "Mix", "50" }, { "Phase", "90" } }, { { "Mono Bass", 1.0f } } },
    };
    for (const auto& s : settings)
    {
        host.reset();
        for (const auto& [name, value] : s.values)
            host.set (name, value);
        for (const auto& [name, value] : s.toggles)
            host.setNormalised (name, value);
        for (bool monoSource : { false, true })
        {
            const auto in = noise (1.0, 0.4f, monoSource);
            const auto out = host.run (in);
            const double err = monoError (in, out, 0);
            check (err < 2.0e-5, juce::String (s.name) + (monoSource ? " (mono in)" : " (stereo in)") + ": L+R error " + juce::String (err, 8));
        }
    }

    std::cout << "\n[3] A mono source really gets wide" << std::endl;
    for (const auto& [label, width, spreadValue] : { std::tuple ("width 150", "150", "0"), std::tuple ("width 200", "200", "0"), std::tuple ("spread 100", "100", "100"),
                                                     std::tuple ("width 200 + spread 100", "200", "100") })
    {
        host.reset();
        host.set ("Width", width);
        host.set ("Mono Spread", spreadValue);
        const auto out = host.run (noise (1.5, 0.4f, true));
        const auto st = analyse (out, skip);
        check (st.sideRms / st.midRms > 0.3 && st.correlation < 0.8,
               juce::String (label) + ": side/mid " + db (st.sideRms / st.midRms) + ", correlation " + juce::String (st.correlation, 2));
    }

    std::cout << "\n[4] Width 0% collapses to mono (LOW BYPASS off, otherwise the lows keep their stereo)" << std::endl;
    host.reset();
    host.set ("Width", "0");
    host.setNormalised ("Low Bypass", 0.0f);
    {
        const auto st = analyse (host.run (noise (1.0)), skip);
        check (st.sideRms < 1.0e-5, "side rms " + juce::String (st.sideRms, 8));
    }

    std::cout << "\n[5] Low end handling" << std::endl;
    {
        // 50 Hz pure side (out of phase) content.
        host.reset();
        host.set ("Low Freq", "150");
        host.setNormalised ("Mono Bass", 1.0f);
        const auto in = sine (1.5, 50.0f, 1.0f, -1.0f);
        const auto stIn = analyse (in, skip);
        const auto st = analyse (host.run (in), skip);
        check (st.sideRms / stIn.sideRms < 0.1, "MONO BASS removes 50 Hz side: " + db (st.sideRms / stIn.sideRms));

        host.reset();
        host.set ("Width", "200");
        host.set ("Low Freq", "150");
        const auto stLow = analyse (host.run (in), skip);
        check (std::abs (stLow.sideRms / stIn.sideRms - 1.0) < 0.15, "LOW BYPASS keeps 50 Hz side as is at width 200: " + db (stLow.sideRms / stIn.sideRms));

        host.reset();
        host.set ("Width", "200");
        host.set ("Low Freq", "150");
        const auto high = sine (1.5, 3000.0f, 1.0f, 0.2f);
        const auto stHighIn = analyse (high, skip);
        const auto stHigh = analyse (host.run (high), skip);
        check (stHigh.sideRms / stHighIn.sideRms > 1.8, "3 kHz side doubles at width 200: " + db (stHigh.sideRms / stHighIn.sideRms));

        // No widening of a mono kick region: mono 60 Hz with spread 100 stays mono.
        host.reset();
        host.set ("Mono Spread", "100");
        host.set ("Width", "200");
        const auto kick = analyse (host.run (sine (1.5, 45.0f, 1.0f, 1.0f)), skip);
        check (kick.sideRms / kick.midRms < 0.05, "mono 45 Hz stays centred with spread 100: side/mid " + db (kick.sideRms / kick.midRms));
    }

    std::cout << "\n[6] Mono check and mix" << std::endl;
    {
        host.reset();
        host.set ("Width", "200");
        host.setNormalised ("Mono Check", 1.0f);
        const auto st = analyse (host.run (noise (1.0)), skip);
        check (st.sideRms < 1.0e-5, "MONO CHECK outputs L = R (side " + juce::String (st.sideRms, 8) + ")");

        host.reset();
        host.set ("Width", "200");
        host.set ("Mix", "0");
        const auto in = noise (1.0);
        const auto out = host.run (in);
        double worst = 0;
        for (int i = skip; i < in.getNumSamples(); ++i)
            worst = juce::jmax (worst, (double) std::abs (in.getSample (0, i) - out.getSample (0, i)));
        check (worst < 1.0e-5, "MIX 0% is fully dry (max diff " + juce::String (worst, 8) + ")");
    }

    std::cout << "\n[7] Depth" << std::endl;
    {
        host.reset();
        host.set ("Depth", "-100");
        const auto far = analyse (host.run (noise (1.5, 0.4f, true)), skip);
        check (far.finite && far.sideRms > 0.01, "FAR adds a side-only room cloud (side/mid " + db (far.sideRms / far.midRms) + ")");
        host.reset();
        host.set ("Depth", "100");
        const auto near = analyse (host.run (noise (1.5, 0.4f)), skip);
        check (near.finite && near.peak < 2.0, "NEAR stays sane (peak " + juce::String (near.peak, 2) + ")");
    }

    std::cout << "\n[7b] SUPER WIDE (Haas)" << std::endl;
    {
        // Classic mode, R delayed 10 ms, LOW BYPASS off: R must be exactly L delayed by 480 samples.
        host.reset();
        host.setNormalised ("Super Wide", 1.0f);
        host.setNormalised ("Haas Mono Safe", 0.0f);
        host.setNormalised ("Low Bypass", 0.0f);
        host.set ("Delay L", "0");
        host.set ("Delay R", "10");
        const auto in = noise (1.0, 0.4f, true);
        const auto out = host.run (in);
        double worst = 0;
        for (int i = skip; i < in.getNumSamples(); ++i)
            worst = juce::jmax (worst, (double) std::abs (out.getSample (1, i) - in.getSample (0, i - 480)));
        check (worst < 1.0e-4, "classic: R is L delayed by exactly 10 ms (max diff " + juce::String (worst, 6) + ")");
        const auto st = analyse (out, skip);
        check (st.correlation < 0.2, "classic: mono source becomes super wide (correlation " + juce::String (st.correlation, 2) + ")");

        // Mono-safe mode: same delays, but the mono sum must be untouched.
        for (const auto& [l, r] : { std::pair ("0", "10"), std::pair ("0.23", "17.5"), std::pair ("7.3", "13.1"), std::pair ("30", "0") })
        {
            host.reset();
            host.setNormalised ("Super Wide", 1.0f);
            host.set ("Delay L", l);
            host.set ("Delay R", r);
            host.set ("Width", "160");
            for (bool monoSource : { false, true })
            {
                const auto src = noise (1.0, 0.4f, monoSource);
                const auto res = host.run (src);
                check (monoError (src, res, 0) < 2.0e-5, juce::String ("mono safe L ") + l + " / R " + r + " ms" + (monoSource ? " (mono in)" : " (stereo in)")
                                                          + ": L+R error " + juce::String (monoError (src, res, 0), 8));
            }
            const auto wideSt = analyse (host.run (noise (1.5, 0.4f, true)), skip);
            check (wideSt.sideRms / wideSt.midRms > 0.3, juce::String ("mono safe L ") + l + " / R " + r + " still widens: side/mid " + db (wideSt.sideRms / wideSt.midRms));
        }

        // LOW BYPASS keeps the delays out of a mono kick.
        host.reset();
        host.setNormalised ("Super Wide", 1.0f);
        host.setNormalised ("Haas Mono Safe", 0.0f);
        host.set ("Delay R", "15");
        host.set ("Low Freq", "150");
        const auto kick = analyse (host.run (sine (1.5, 45.0f, 1.0f, 1.0f)), skip);
        check (kick.sideRms / kick.midRms < 0.05, "classic + LOW BYPASS: 45 Hz stays centred, side/mid " + db (kick.sideRms / kick.midRms));

        // Amount 0 leaves the signal alone.
        host.reset();
        host.setNormalised ("Super Wide", 1.0f);
        host.set ("Haas Amount", "0");
        const auto src = noise (1.0);
        const auto res = host.run (src);
        double diffMax = 0;
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < src.getNumSamples(); ++i)
                diffMax = juce::jmax (diffMax, (double) std::abs (src.getSample (ch, i) - res.getSample (ch, i)));
        check (diffMax < 1.0e-5, "amount 0 is transparent (max diff " + juce::String (diffMax, 8) + ")");
    }

    std::cout << "\n[8] Random automation torture" << std::endl;
    {
        host.reset();
        juce::Random r (99);
        auto& params = host.plugin->getParameters();
        const auto out = host.run (noise (6.0), [&] (int)
        {
            for (auto* p : params)
                if (r.nextFloat() < 0.3f && p->getName (100) != "Output")
                    p->setValue (r.nextFloat());
        });
        const auto st = analyse (out, 0);
        check (st.finite, "no NaN or inf");
        check (st.peak < 4.0, "peak bounded (" + juce::String (st.peak, 2) + ")");
    }

    std::cout << "\n[8b] Every factory preset" << std::endl;
    {
        const int count = host.plugin->getNumPrograms();
        int bad = 0;
        for (int i = 0; i < count; ++i)
        {
            host.reset();
            host.plugin->setCurrentProgram (i);
            const auto st = analyse (host.run (noise (0.6, 0.4f, true)), skip / 2);
            if (! st.finite || st.peak > 3.0 || st.midRms + st.sideRms < 0.01)
            {
                std::cout << "  bad preset " << i << " " << host.plugin->getProgramName (i) << " peak " << st.peak << std::endl;
                ++bad;
            }
        }
        check (count == 70, juce::String (count) + " presets");
        check (bad == 0, "all presets finite, bounded and audible");
    }

    std::cout << "\n[9] Renders" << std::endl;
    {
        const auto lead = monoLead (4.0);
        writeWav (lead, renders.getChildFile ("01-mono-lead-dry.wav"));

        host.reset();
        host.set ("Width", "200");
        host.set ("Mono Spread", "100");
        host.setNormalised ("Mono Bass", 1.0f);
        writeWav (host.run (lead), renders.getChildFile ("02-mono-lead-200pct.wav"));

        host.reset();
        host.set ("Width", "170");
        host.set ("Mono Spread", "80");
        host.set ("Drift Depth", "180");
        host.set ("Drift Rate", "0.5");
        writeWav (host.run (lead), renders.getChildFile ("03-mono-lead-phase-drift.wav"));

        host.reset();
        host.set ("Width", "150");
        host.set ("Mono Spread", "60");
        host.set ("Depth", "-80");
        writeWav (host.run (lead), renders.getChildFile ("04-mono-lead-far.wav"));

        host.reset();
        host.setNormalised ("Super Wide", 1.0f);
        host.setNormalised ("Haas Mono Safe", 0.0f);
        host.set ("Delay R", "18");
        host.set ("Width", "120");
        writeWav (host.run (lead), renders.getChildFile ("05-mono-lead-haas-classic.wav"));

        host.reset();
        host.setNormalised ("Super Wide", 1.0f);
        host.set ("Delay L", "0.23");
        host.set ("Delay R", "11");
        host.set ("Width", "140");
        writeWav (host.run (lead), renders.getChildFile ("06-mono-lead-haas-safe.wav"));
        std::cout << "  wrote 6 WAVs to " << renders.getFullPathName() << std::endl;
    }

    std::cout << "\n" << (failures == 0 ? "ALL TESTS PASSED" : juce::String (failures) + " FAILURE(S)") << std::endl;
    host.plugin->releaseResources();
    host.plugin.reset();
    return failures == 0 ? 0 : 1;
}
