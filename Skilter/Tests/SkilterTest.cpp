// Loads the built SKILTER.vst3, runs it through stress tests and renders demo WAVs.
//   SkilterTest <path to SKILTER.vst3> <render folder>
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

        void set (const juce::String& name, const juce::String& text)
        {
            for (auto* p : plugin->getParameters())
                if (p->getName (100) == name)
                {
                    p->setValue (p->getValueForText (text));
                    return;
                }
            std::cout << "  ?? no parameter named " << name << std::endl;
            ++failures;
        }

        void setNormalised (const juce::String& name, float value)
        {
            for (auto* p : plugin->getParameters())
                if (p->getName (100) == name)
                    p->setValue (value);
        }

        void reset (int oversampling = 1)
        {
            for (auto* p : plugin->getParameters())
                p->setValue (p->getDefaultValue());
            setNormalised ("Oversampling", (float) oversampling / 2.0f);
            plugin->releaseResources();
            plugin->prepareToPlay (sampleRate, blockSize);
        }

        // Runs the input through the plugin block by block; onBlock can automate parameters.
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

    juce::AudioBuffer<float> noise (double seconds, float level = 0.5f, bool mono = false)
    {
        juce::Random r (1234);
        juce::AudioBuffer<float> b (2, (int) (seconds * sampleRate));
        for (int i = 0; i < b.getNumSamples(); ++i)
        {
            const float v = (r.nextFloat() * 2.0f - 1.0f) * level;
            b.setSample (0, i, v);
            b.setSample (1, i, mono ? v : (r.nextFloat() * 2.0f - 1.0f) * level);
        }
        return b;
    }

    juce::AudioBuffer<float> saw (double seconds, float freq, float level = 0.5f)
    {
        juce::AudioBuffer<float> b (2, (int) (seconds * sampleRate));
        double phase = 0.0;
        for (int i = 0; i < b.getNumSamples(); ++i)
        {
            const float v = (float) (2.0 * phase - 1.0) * level;
            b.setSample (0, i, v);
            b.setSample (1, i, v);
            phase = std::fmod (phase + freq / sampleRate, 1.0);
        }
        return b;
    }

    struct Stats
    {
        float peak = 0.0f, rms = 0.0f, freq = 0.0f;
        bool finite = true;
    };

    Stats measure (const juce::AudioBuffer<float>& b, int channel = 0, int skip = 0)
    {
        Stats s;
        double sum = 0.0;
        int crossings = 0;
        const int n = b.getNumSamples() - skip;
        for (int i = skip; i < b.getNumSamples(); ++i)
        {
            const float v = b.getSample (channel, i);
            s.finite = s.finite && std::isfinite (v);
            s.peak = juce::jmax (s.peak, std::abs (v));
            sum += v * v;
            if (i > skip && b.getSample (channel, i - 1) < 0.0f && v >= 0.0f)
                ++crossings;
        }
        s.rms = (float) std::sqrt (sum / juce::jmax (1, n));
        s.freq = (float) crossings / (float) (n / sampleRate);
        return s;
    }

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
        std::cout << "usage: SkilterTest <SKILTER.vst3> <render folder>" << std::endl;
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

    std::cout << "\n[1] Defaults pass audio through" << std::endl;
    host.reset (1);
    {
        const auto in = saw (1.0, 220.0f);
        const auto out = host.run (in);
        const auto a = measure (in, 0, 4800), b = measure (out, 0, 4800);
        check (std::abs (b.rms / a.rms - 1.0f) < 0.05f, "output level within 5% of input (ratio " + juce::String (b.rms / a.rms, 3) + ")");
    }

    std::cout << "\n[2] Lowpass at 500 Hz, 24 dB" << std::endl;
    host.reset();
    host.set ("Cutoff", "500");
    {
        const auto out = host.run (noise (1.0));
        const auto s = measure (out, 0, 4800);
        check (s.rms < 0.08f, "noise strongly filtered (rms " + juce::String (s.rms, 3) + ")");
        check (s.freq < 900.0f, "dominant content below ~900 Hz (" + juce::String (s.freq, 0) + " Hz crossings)");
    }

    for (int ovs = 0; ovs <= 2; ++ovs)
    {
        std::cout << "\n[3." << ovs << "] Self-oscillation in silence, OVS " << (1 << ovs) << "x" << std::endl;
        host.reset (ovs);
        host.set ("Cutoff", "1000");
        host.set ("Resonance", "100");
        juce::AudioBuffer<float> silence (2, (int) (3.0 * sampleRate));
        silence.clear();
        const auto out = host.run (silence);
        const auto s = measure (out, 0, (int) sampleRate);
        check (s.finite, "no NaN or inf");
        check (s.peak > 0.5f && s.peak < 1.0f, "sings at a stable level (peak " + juce::String (s.peak, 3) + ")");
        check (std::abs (s.freq - 1000.0f) < 120.0f, "pitch near 1 kHz (" + juce::String (s.freq, 0) + " Hz)");
        if (ovs == 1)
            writeWav (out, renders.getChildFile ("self-oscillation-1kHz.wav"));
    }

    std::cout << "\n[4] Torture: max drive, max resonance, hot noise, every drive type and position" << std::endl;
    for (auto type : { "Tube", "Tape", "Hard" })
        for (auto pos : { "Pre", "Post" })
            for (int ovs : { 0, 2 })
            {
                host.reset (ovs);
                host.set ("Drive", "24");
                host.set ("Drive Type", type);
                host.set ("Drive Position", pos);
                host.set ("Resonance", "100");
                host.set ("Cutoff", "200");
                host.set ("Input Gain", "12");
                const auto out = host.run (noise (1.0, 1.0f), [&] (int block)
                {
                    // Sweep the cutoff hard through the whole range.
                    host.setNormalised ("Cutoff", 0.5f + 0.5f * std::sin ((float) block * 0.2f));
                });
                const auto s = measure (out);
                check (s.finite && s.peak < 2.0f, juce::String (type) + "/" + pos + " OVS " + juce::String (1 << ovs) + "x stays bounded (peak " + juce::String (s.peak, 2) + ")");
            }

    std::cout << "\n[5] Morph sweeps smoothly LP -> BP -> HP -> NOTCH" << std::endl;
    host.reset (1);
    host.set ("Cutoff", "1000");
    host.set ("Resonance", "60");
    {
        const int blocks = (int) (6.0 * sampleRate) / blockSize;
        const auto in = saw (6.0, 110.0f);
        const auto out = host.run (in, [&] (int block) { host.setNormalised ("Morph", juce::jlimit (0.0f, 1.0f, (float) block / (float) blocks)); });
        const auto s = measure (out);
        float maxJump = 0.0f;
        for (int i = 1; i < out.getNumSamples(); ++i)
            maxJump = juce::jmax (maxJump, std::abs (out.getSample (0, i) - out.getSample (0, i - 1)) - std::abs (in.getSample (0, i) - in.getSample (0, i - 1)));
        check (s.finite, "no NaN or inf");
        check (maxJump < 1.0f, "no clicks beyond the saw's own edges (" + juce::String (maxJump, 3) + ")");
        writeWav (out, renders.getChildFile ("morph-sweep-saw.wav"));
    }

    std::cout << "\n[6] Latency is reported to the host" << std::endl;
    for (int ovs = 0; ovs <= 2; ++ovs)
    {
        host.reset (ovs);
        host.run (noise (0.05));
        const int latency = host.plugin->getLatencySamples();
        check (ovs == 0 ? latency == 0 : latency > 0, juce::String (1 << ovs) + "x reports " + juce::String (latency) + " samples");
    }

    std::cout << "\n[7] Sequencer moves the cutoff in steps" << std::endl;
    host.reset (1);
    host.set ("Cutoff", "800");
    host.set ("Resonance", "50");
    host.setNormalised ("Seq On", 1.0f);
    host.set ("Seq Depth", "100");
    {
        auto out = host.run (noise (4.0));
        // At 120 BPM, 1/16 steps are 125 ms long; compare the loudness of each step.
        const int stepLength = (int) (0.125 * sampleRate);
        float quietest = 1.0f, loudest = 0.0f;
        for (int stepIndex = 4; stepIndex < 20; ++stepIndex)
        {
            juce::AudioBuffer<float> slice (out.getArrayOfWritePointers(), 2, stepIndex * stepLength + stepLength / 2, stepLength / 3);
            const float rms = measure (slice).rms;
            quietest = juce::jmin (quietest, rms);
            loudest = juce::jmax (loudest, rms);
        }
        check (loudest / juce::jmax (quietest, 1.0e-6f) > 2.0f, "steps differ in level (" + juce::String (juce::Decibels::gainToDecibels (loudest / quietest), 1) + " dB range)");
        writeWav (out, renders.getChildFile ("sequencer-noise.wav"));
    }

    std::cout << "\n[8] Macro sweep: one knob closes, resonates and drives" << std::endl;
    host.reset (1);
    {
        const int blocks = (int) (6.0 * sampleRate) / blockSize;
        const auto out = host.run (saw (6.0, 55.0f), [&] (int block)
        {
            const float t = (float) block / (float) blocks;
            host.setNormalised ("Macro", t < 0.5f ? t * 2.0f : 2.0f - t * 2.0f);
        });
        const auto s = measure (out);
        check (s.finite && s.peak < 2.0f, "macro sweep stays clean (peak " + juce::String (s.peak, 2) + ")");
        writeWav (out, renders.getChildFile ("macro-sweep-saw.wav"));
    }

    std::cout << "\n[9] Mid/Side keeps a mono source mono when only the side is offset" << std::endl;
    host.reset (1);
    host.set ("Stereo Mode", "Mid/Side");
    host.set ("Cutoff", "2000");
    host.set ("R/Side Offset", "-4");
    {
        const auto out = host.run (noise (1.0, 0.5f, true));
        float maxDiff = 0.0f;
        for (int i = 0; i < out.getNumSamples(); ++i)
            maxDiff = juce::jmax (maxDiff, std::abs (out.getSample (0, i) - out.getSample (1, i)));
        check (maxDiff < 1.0e-4f, "left and right identical (max diff " + juce::String (maxDiff, 6) + ")");
    }

    std::cout << "\n[10] Dual L/R offset makes the channels different" << std::endl;
    host.reset (1);
    host.set ("Stereo Mode", "Dual L/R");
    host.set ("Cutoff", "2000");
    host.set ("R/Side Offset", "-3");
    {
        const auto out = host.run (noise (1.0, 0.5f, true));
        const auto l = measure (out, 0, 4800), r = measure (out, 1, 4800);
        check (r.rms < l.rms * 0.75f, "right channel darker than left (L " + juce::String (l.rms, 3) + ", R " + juce::String (r.rms, 3) + ")");
    }

    std::cout << "\n[11] Oversampling removes aliasing from hard drive" << std::endl;
    {
        // A 7 kHz sine clipped hard makes a 35 kHz harmonic, which folds back to 13 kHz at 48 kHz.
        auto aliasLevel = [&] (int ovs)
        {
            host.reset (ovs);
            host.set ("Drive", "24");
            host.set ("Drive Type", "Hard");
            juce::AudioBuffer<float> sine (2, (int) sampleRate);
            for (int i = 0; i < sine.getNumSamples(); ++i)
                for (int ch = 0; ch < 2; ++ch)
                    sine.setSample (ch, i, 0.5f * std::sin (juce::MathConstants<float>::twoPi * 7000.0f * (float) i / (float) sampleRate));
            const auto out = host.run (sine);
            double re = 0.0, im = 0.0;
            for (int i = 4800; i < out.getNumSamples(); ++i)
            {
                const double w = juce::MathConstants<double>::twoPi * 13000.0 * i / sampleRate;
                re += out.getSample (0, i) * std::cos (w);
                im += out.getSample (0, i) * std::sin (w);
            }
            return juce::Decibels::gainToDecibels ((float) (2.0 * std::sqrt (re * re + im * im) / (out.getNumSamples() - 4800)), -200.0f);
        };
        const float at1x = aliasLevel (0), at4x = aliasLevel (2);
        check (at1x - at4x > 20.0f, "alias at 13 kHz: " + juce::String (at1x, 1) + " dB at 1x, " + juce::String (at4x, 1) + " dB at 4x");
    }

    std::cout << "\n[12] Every factory preset loads and stays clean" << std::endl;
    {
        const int count = host.plugin->getNumPrograms();
        check (count == 150, juce::String (count) + " presets exposed to the host");

        int bad = 0;
        float loudest = 0.0f;
        juce::String loudestName;
        juce::StringArray names;
        for (int i = 0; i < count; ++i)
        {
            host.reset (1);
            host.plugin->setCurrentProgram ((i + 1) % count); // step off first so every preset really loads
            host.plugin->setCurrentProgram (i);
            names.addIfNotAlreadyThere (host.plugin->getProgramName (i));
            const auto s = measure (host.run (noise (0.75, 0.7f)), 0, 2400);
            if (! s.finite || s.peak > 2.0f)
            {
                std::cout << "         bad preset " << i + 1 << " " << host.plugin->getProgramName (i) << " peak " << s.peak << std::endl;
                ++bad;
            }
            if (s.peak > loudest)
            {
                loudest = s.peak;
                loudestName = host.plugin->getProgramName (i);
            }
        }
        check (bad == 0, "all presets bounded on hot noise (loudest: " + loudestName + ", peak " + juce::String (loudest, 2) + ")");
        check (names.size() == count, juce::String (names.size()) + " unique preset names");
    }

    std::cout << "\n" << (failures == 0 ? "ALL TESTS PASSED" : juce::String (failures) + " FAILURE(S)") << std::endl;
    host.plugin.reset();
    return failures == 0 ? 0 : 1;
}
