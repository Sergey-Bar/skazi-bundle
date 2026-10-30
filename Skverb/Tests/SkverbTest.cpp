// Offline checks for SKVERB, run against the processor directly.
//   SkverbTest <renders dir>             impulse response + drum loop per algorithm, every preset, freeze/shimmer stress
//   SkverbTest shot <out.png> [preset]   editor screenshot after playing drums through it
#include <juce_audio_utils/juce_audio_utils.h>
#include "../Source/PluginProcessor.h"

namespace
{
    constexpr double sr = 48000.0;
    constexpr int block = 512;

    void setParam (SkverbAudioProcessor& p, const char* id, float value)
    {
        auto* param = p.apvts.getParameter (id);
        param->setValueNotifyingHost (param->convertTo0to1 (value));
    }

    // Kick + snare + hat pattern at 140 BPM followed by silence, so the tail is audible.
    float drums (int n)
    {
        const int beat = (int) (sr * 60.0 / 140.0);
        const int bars = beat * 8;
        if (n >= bars)
            return 0.0f;
        const int inBeat = n % beat, beatIndex = n / beat;
        const float t = (float) inBeat / (float) sr;
        float v = std::sin (2.0f * 3.14159f * (50.0f * t + 120.0f * (1.0f - std::exp (-t * 30.0f)) / 30.0f)) * std::exp (-t * 9.0f) * 0.8f;
        if (beatIndex % 2 == 1)
        {
            juce::Random r (n);
            v += (r.nextFloat() * 2.0f - 1.0f) * std::exp (-t * 18.0f) * 0.5f;
        }
        const int half = inBeat - beat / 2;
        if (half >= 0)
        {
            juce::Random r (n * 7);
            v += (r.nextFloat() * 2.0f - 1.0f) * std::exp (-(float) half / (float) sr * 60.0f) * 0.2f;
        }
        return v;
    }

    struct Result { float peak = 0.0f; bool finite = true; float rt60 = 0.0f; float wetRms = 0.0f; };

    // Renders seconds of audio; source(n) gives the mono input. Returns stats and fills out.
    Result render (SkverbAudioProcessor& p, float seconds, const std::function<float (int)>& source, juce::AudioBuffer<float>& out)
    {
        const int total = (int) (seconds * sr);
        out.setSize (2, total);
        juce::AudioBuffer<float> buf (2, block);
        juce::MidiBuffer midi;
        Result r;
        for (int pos = 0; pos < total; pos += block)
        {
            const int n = juce::jmin (block, total - pos);
            buf.setSize (2, n, false, false, true);
            for (int i = 0; i < n; ++i)
                buf.setSample (0, i, source (pos + i)), buf.setSample (1, i, source (pos + i));
            p.processBlock (buf, midi);
            for (int c = 0; c < 2; ++c)
            {
                out.copyFrom (c, pos, buf, c, 0, n);
                for (int i = 0; i < n; ++i)
                {
                    const float v = buf.getSample (c, i);
                    if (! std::isfinite (v)) r.finite = false;
                    r.peak = juce::jmax (r.peak, std::abs (v));
                }
            }
        }
        return r;
    }

    // RT60 from Schroeder backward integration (T20 extrapolated) of a 100%-wet impulse response.
    float measureRt60 (const juce::AudioBuffer<float>& ir, int startSample)
    {
        const int n = ir.getNumSamples();
        std::vector<double> energy ((size_t) n + 1, 0.0);
        for (int i = n - 1; i >= 0; --i)
        {
            const double l = ir.getSample (0, i), r = ir.getSample (1, i);
            energy[(size_t) i] = energy[(size_t) i + 1] + l * l + r * r;
        }
        const double e0 = energy[(size_t) startSample];
        if (e0 <= 0.0)
            return 0.0f;
        int t5 = -1, t25 = -1;
        for (int i = startSample; i < n; ++i)
        {
            const double db = 10.0 * std::log10 (energy[(size_t) i] / e0 + 1e-30);
            if (t5 < 0 && db <= -5.0) t5 = i;
            if (t25 < 0 && db <= -25.0) { t25 = i; break; }
        }
        if (t5 < 0 || t25 < 0)
            return -1.0f;   // didn't decay 25 dB within the render
        return (float) ((t25 - t5) / sr * 3.0);
    }

    void writeWav (const juce::File& file, const juce::AudioBuffer<float>& buffer, int bits = 24)
    {
        file.deleteFile();
        juce::WavAudioFormat wav;
        std::unique_ptr<juce::AudioFormatWriter> writer (wav.createWriterFor (new juce::FileOutputStream (file), sr, 2, bits, {}, 0));
        if (writer != nullptr)
            writer->writeFromAudioSampleBuffer (buffer, 0, buffer.getNumSamples());
    }

    void reset (SkverbAudioProcessor& p)
    {
        p.prepareToPlay (sr, block);
    }

    int shot (const juce::File& out, int preset)
    {
        juce::ScopedJuceInitialiser_GUI gui;
        SkverbAudioProcessor proc;
        proc.setRateAndBufferSizeDetails (sr, block);
        proc.prepareToPlay (sr, block);
        if (preset >= 0)
            proc.setCurrentProgram (preset);

        std::unique_ptr<juce::AudioProcessorEditor> editor (proc.createEditor());
        editor->setSize (1200, 1066);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (50);

        juce::AudioBuffer<float> buf (2, block);
        juce::MidiBuffer midi;
        int pos = 0;
        for (int frame = 0; frame < 90; ++frame)
        {
            for (int b = 0; b < 2; ++b, pos += block)
            {
                for (int i = 0; i < block; ++i)
                    buf.setSample (0, i, drums ((pos + i) % (int) (sr * 1.7))), buf.setSample (1, i, buf.getSample (0, i));
                proc.processBlock (buf, midi);
            }
            juce::MessageManager::getInstance()->runDispatchLoopUntil (20);
        }

        const auto image = editor->createComponentSnapshot (editor->getLocalBounds(), true, 1.0f);
        out.deleteFile();
        juce::FileOutputStream stream (out);
        if (! stream.openedOk() || ! juce::PNGImageFormat().writeImageToStream (image, stream))
            return 1;
        std::printf ("wrote %s\n", out.getFullPathName().toRawUTF8());
        editor.reset();
        return 0;
    }
}

int main (int argc, char* argv[])
{
    if (argc > 2 && juce::String (argv[1]) == "shot")
        return shot (juce::File::getCurrentWorkingDirectory().getChildFile (argv[2]), argc > 3 ? juce::String (argv[3]).getIntValue() : -1);

    juce::ScopedJuceInitialiser_GUI gui;
    const auto dir = juce::File::getCurrentWorkingDirectory().getChildFile (argc > 1 ? argv[1] : "renders");
    dir.createDirectory();
    int failures = 0;

    SkverbAudioProcessor proc;
    proc.setRateAndBufferSizeDetails (sr, block);
    juce::AudioBuffer<float> out;

    // 1. Every algorithm: 100% wet impulse response (RT60 check) and the drum loop at 40% mix.
    std::printf ("%-10s %8s %8s %8s %8s\n", "MODE", "IR peak", "RT60", "drumPk", "status");
    for (int m = 0; m < verb::numModes; ++m)
    {
        proc.setCurrentProgram (0);
        setParam (proc, ParamIDs::mode, (float) m);
        setParam (proc, ParamIDs::decay, m == verb::gated ? 0.4f : 2.0f);
        setParam (proc, ParamIDs::preDelay, 0.0f);
        setParam (proc, ParamIDs::mix, 100.0f);
        reset (proc);
        auto ir = render (proc, 8.0f, [] (int n) { return n == 0 ? 1.0f : 0.0f; }, out);
        const float rt = measureRt60 (out, 0);
        writeWav (dir.getChildFile ("ir-" + juce::String (m).paddedLeft ('0', 2) + "-" + verb::spec (m).name + ".wav"), out, 32);   // float, so the quiet tail is measurable

        setParam (proc, ParamIDs::mix, 40.0f);
        reset (proc);
        auto dr = render (proc, 9.0f, drums, out);
        writeWav (dir.getChildFile ("drums-" + juce::String (m).paddedLeft ('0', 2) + "-" + verb::spec (m).name + ".wav"), out);

        const bool ok = ir.finite && dr.finite && dr.peak < 3.0f && ir.peak > 1e-4f;
        failures += ok ? 0 : 1;
        std::printf ("%-10s %8.3f %8.2f %8.3f %8s\n", verb::spec (m).name, ir.peak, rt, dr.peak, ok ? "ok" : "FAIL");
    }

    // 2. Every preset through the drum loop.
    std::printf ("\n%-30s %8s %8s\n", "PRESET", "peak", "status");
    for (int i = 0; i < proc.getNumPrograms(); ++i)
    {
        proc.setCurrentProgram (i);
        reset (proc);
        auto r = render (proc, 10.0f, drums, out);
        const bool ok = r.finite && r.peak < 3.0f;
        failures += ok ? 0 : 1;
        std::printf ("%-30s %8.3f %8s\n", proc.getProgramName (i).toRawUTF8(), r.peak, ok ? "ok" : "FAIL");
        if (i % 6 == 0)
            writeWav (dir.getChildFile (juce::String::formatted ("preset-%02d.wav", i)), out);
    }

    // 3. Stress: freeze + max shimmer + max feedback + drive for 30 s must stay bounded.
    proc.setCurrentProgram (0);
    setParam (proc, ParamIDs::mode, (float) verb::shimmer);
    setParam (proc, ParamIDs::shimmerAmt, 100.0f);
    setParam (proc, ParamIDs::feedback, 95.0f);
    setParam (proc, ParamIDs::preDelay, 3.0f);
    setParam (proc, ParamIDs::drive, 100.0f);
    setParam (proc, ParamIDs::decay, 30.0f);
    setParam (proc, ParamIDs::bassMult, 4.0f);
    setParam (proc, ParamIDs::mix, 100.0f);
    reset (proc);
    auto stress = render (proc, 20.0f, drums, out);
    setParam (proc, ParamIDs::freeze, 1.0f);
    auto frozen = render (proc, 20.0f, [] (int) { return 0.0f; }, out);
    const bool stressOk = stress.finite && frozen.finite && stress.peak < 8.0f && frozen.peak < 8.0f;
    failures += stressOk ? 0 : 1;
    std::printf ("\nstress (shimmer 100, fb 95, drive 100, 30s): peak %.3f, frozen peak %.3f  %s\n", stress.peak, frozen.peak, stressOk ? "ok" : "FAIL");

    // 4. Mode switching mid-tail must not blow up.
    proc.setCurrentProgram (0);
    reset (proc);
    float switchPeak = 0.0f;
    bool switchFinite = true;
    for (int m = 0; m < verb::numModes; ++m)
    {
        setParam (proc, ParamIDs::mode, (float) m);
        auto r = render (proc, 0.5f, drums, out);
        switchPeak = juce::jmax (switchPeak, r.peak);
        switchFinite = switchFinite && r.finite;
    }
    failures += (switchFinite && switchPeak < 3.0f) ? 0 : 1;
    std::printf ("mode switching: peak %.3f %s\n", switchPeak, switchFinite && switchPeak < 3.0f ? "ok" : "FAIL");

    // 5. Tail filter at full resonance with the envelope sweeping, switching type every half second.
    proc.setCurrentProgram (0);
    setParam (proc, ParamIDs::fltReso, 100.0f);
    setParam (proc, ParamIDs::fltEnv, 100.0f);
    setParam (proc, ParamIDs::fltCutoff, 800.0f);
    setParam (proc, ParamIDs::mix, 100.0f);
    reset (proc);
    float filterPeak = 0.0f;
    bool filterFinite = true;
    for (int k = 0; k < 12; ++k)
    {
        setParam (proc, ParamIDs::fltType, (float) (k % 4));
        auto r = render (proc, 0.5f, drums, out);
        filterPeak = juce::jmax (filterPeak, r.peak);
        filterFinite = filterFinite && r.finite;
    }
    const bool filterOk = filterFinite && filterPeak < 3.0f;
    failures += filterOk ? 0 : 1;
    std::printf ("tail filter (reso 100, env 100, type switching): peak %.3f %s\n", filterPeak, filterOk ? "ok" : "FAIL");

    // 6. Reverse layer at 100%: an impulse should swell up, peaking well after it arrives.
    proc.setCurrentProgram (0);
    setParam (proc, ParamIDs::revAmount, 100.0f);
    setParam (proc, ParamIDs::revTime, 1.0f);
    setParam (proc, ParamIDs::preDelay, 0.0f);
    setParam (proc, ParamIDs::mix, 100.0f);
    reset (proc);
    auto rev = render (proc, 4.0f, [] (int n) { return n == 0 ? 1.0f : 0.0f; }, out);
    writeWav (dir.getChildFile ("reverse-layer.wav"), out);
    int peakAt = 0;
    float best = 0.0f;
    for (int i = 0; i < out.getNumSamples(); ++i)
        if (std::abs (out.getSample (0, i)) > best)
            best = std::abs (out.getSample (0, i)), peakAt = i;
    const bool revOk = rev.finite && peakAt > (int) (0.3 * sr);
    failures += revOk ? 0 : 1;
    std::printf ("reverse layer: peak at %.2f s %s\n", peakAt / sr, revOk ? "ok" : "FAIL");

    std::printf ("\n%s (%d failures)\n", failures == 0 ? "ALL PASSED" : "FAILED", failures);
    return failures == 0 ? 0 : 1;
}
