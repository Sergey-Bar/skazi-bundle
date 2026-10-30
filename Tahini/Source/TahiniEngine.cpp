#include "TahiniEngine.h"
#include <cmath>
#include <complex>

namespace tahini
{
namespace
{
    constexpr double twoPi = juce::MathConstants<double>::twoPi;
    // FDN line lengths in ms (mutually prime-ish so the echoes don't pile up into a metallic ring)
    constexpr double kFdnMs[8] = { 29.7, 37.1, 41.1, 43.7, 53.0, 59.3, 67.1, 73.9 };
    constexpr int kDiffSamples[4] = { 142, 107, 379, 277 };

    double pairGain (double hz, double loHz, double qLo, double hiHz, double qHi)
    {
        const double kLo = juce::jlimit (0.0, 1.0, 1.0 / qLo - 1.0);
        const std::complex<double> a (0.0, hz / loHz), b (0.0, hz / hiHz);
        return std::abs ((a * a + kLo * a) / (a * a + a / qLo + 1.0) / (b * b + b / qHi + 1.0));
    }

    // Loudest point of the low cut + high cut pair, so the feedback can be held under 1 at the resonance
    double filterPeak (double loHz, double qLo, double hiHz, double qHi)
    {
        double peak = 1.0;
        for (int i = 0; i <= 96; ++i)
            peak = std::max (peak, pairGain (20.0 * std::pow (1000.0, i / 96.0), loHz, qLo, hiHz, qHi));
        return peak;
    }
}

// RESO 0..1 -> filter Q (0.5 = no peak, 4 = +12 dB)
double resoQ (float reso) { return 0.5 + 3.5 * std::pow ((double) juce::jlimit (0.0f, 1.0f, reso), 1.5); }

float echoLowCut (const Settings& s) { return s.loCut; }

float echoHighCut (const Settings& s)
{
    float hi = s.hiCut;
    if (s.mode == Tape) hi = juce::jmin (hi, 5000.0f);
    if (s.mode == Bbd)  hi = juce::jmin (hi, 3800.0f);
    return hi * (1.0f - 0.6f * s.age); // old tape is darker
}

double echoFilterGain (const Settings& s, double hz)
{
    return pairGain (hz, echoLowCut (s), resoQ (s.loRes), echoHighCut (s), resoQ (s.hiRes));
}

float Engine::EchoFx::process (float x, const EchoCoeffs& c)
{
    float lp, bp, hp;
    lo.tick (x, c.gLo, c.r2Lo, lp, bp, hp);
    x = hp + c.kLo * bp;  // with no reso this is the gentle 6 dB low cut of 1.0
    hi.tick (x, c.gHi, c.r2Hi, lp, bp, hp);
    x = lp;
    if (c.bbd) { bbdLp += (x - bbdLp) * c.aBbd; x = bbdLp; }

    // every type is scaled so quiet signals pass at (or under) unity: echoes grind, the loop never runs away
    switch (c.type)
    {
        case DriveTube:
        {
            constexpr float bias = 0.35f; // lopsided like a tube: even harmonics
            const float y = (std::tanh (x * c.drive + bias) - std::tanh (bias)) / c.drive;
            const float o = y - dcIn + c.dcA * dcOut;
            dcIn = y; dcOut = o;
            return o;
        }
        case DriveFuzz:
        {
            const float u = x * c.drive;
            return u / std::sqrt (std::sqrt (1.0f + u * u * u * u)) / c.drive;
        }
        case DriveCrush:
            if (++holdCount >= c.hold) { holdCount = 0; held = std::trunc (x / c.step) * c.step; } // toward zero: tails die out
            return std::tanh (held);
        default:
            return std::tanh (x * c.drive) / c.drive;
    }
}

const std::vector<skazi::ParamDef>& paramDefs()
{
    using P = skazi::ParamDef;
    static const std::vector<P> defs = {
        { "dmode", "Delay Type", P::Choice, 0, 6, 2, "", 1, 1, { "PING-PONG", "STEREO", "TAPE", "DIGITAL", "BBD", "MULTI-TAP", "DUAL" } },
        { "pitch", "Echo Pitch", P::Float, -24, 24, 0, "st", 0.01f },
        { "reverse", "Reverse Echoes", P::Float, 0, 100, 0, "%", 0.1f },
        { "tstop", "Tape Stops", P::Float, 0, 100, 0, "%", 0.1f },
        { "age", "Tape Age", P::Float, 0, 100, 0, "%", 0.1f },
        { "dtime", "Delay Time", P::Choice, 0, 15, 12, "", 1, 1, skazi::noteLengthNames() },
        { "fb", "Feedback", P::Float, 0, 95, 55, "%", 0.1f },
        { "locut", "Low Cut", P::Float, 20, 4000, 250, "Hz", 1, 0.4f },
        { "hicut", "High Cut", P::Float, 200, 20000, 6500, "Hz", 1, 0.4f },
        { "wow", "Wow", P::Float, 0, 100, 25, "%", 0.1f },
        { "ddrive", "Delay Drive", P::Float, 0, 100, 10, "%", 0.1f },
        { "freeze", "Freeze", P::Bool, 0, 1, 0 },
        { "size", "Size", P::Float, 0, 100, 65, "%", 0.1f },
        { "decay", "Decay", P::Float, 0.2f, 20, 4.2f, "s", 0.01f, 0.4f },
        { "predelay", "Pre-Delay", P::Float, 0, 250, 30, "ms", 0.1f },
        { "damp", "Damping", P::Float, 0, 100, 50, "%", 0.1f },
        { "width", "Width", P::Float, 0, 100, 100, "%", 0.1f },
        { "duck", "Duck Amount", P::Float, 0, 100, 60, "%", 0.1f },
        { "duckrel", "Duck Release", P::Float, 20, 1000, 200, "ms", 1, 0.5f },
        { "dmix", "Delay Mix", P::Float, 0, 100, 55, "%", 0.1f },
        { "rmix", "Reverb Mix", P::Float, 0, 100, 30, "%", 0.1f },
        { "dry", "Dry Signal", P::Bool, 0, 1, 1 },
        { "gain", "Output", P::Float, -24, 12, 0, "dB", 0.1f },
        // 1.1: appended so old projects and presets load unchanged
        { "hpres", "Low Cut Reso", P::Float, 0, 100, 0, "%", 0.1f },
        { "lpres", "High Cut Reso", P::Float, 0, 100, 0, "%", 0.1f },
        { "dist", "Drive Type", P::Choice, 0, 3, 0, "", 1, 1, { "TAPE", "TUBE", "FUZZ", "CRUSH" } },
        { "pmix", "Pitch Blend", P::Float, 0, 100, 100, "%", 0.1f },
        { "psnap", "Pitch Snap", P::Bool, 0, 1, 1 },
    };
    return defs;
}

Settings Settings::fromValues (const std::function<float (const char*)>& v)
{
    Settings s;
    s.mode = (int) v ("dmode"); s.time = (int) v ("dtime"); s.feedback = v ("fb") * 0.01f;
    s.loCut = v ("locut"); s.hiCut = v ("hicut"); s.wow = v ("wow") * 0.01f; s.drive = v ("ddrive") * 0.01f;
    s.freeze = v ("freeze") > 0.5f; s.size = v ("size") * 0.01f; s.decay = v ("decay"); s.predelayMs = v ("predelay");
    s.damp = v ("damp") * 0.01f; s.width = v ("width") * 0.01f;
    s.duck = v ("duck") * 0.01f; s.duckRelMs = v ("duckrel");
    s.delayMix = v ("dmix") * 0.01f; s.reverbMix = v ("rmix") * 0.01f; s.dry = v ("dry") > 0.5f;
    s.gain = juce::Decibels::decibelsToGain (v ("gain"));
    s.pitch = v ("pitch"); s.reverse = v ("reverse") * 0.01f; s.tapeStop = v ("tstop") * 0.01f; s.age = v ("age") * 0.01f;
    s.loRes = v ("hpres") * 0.01f; s.hiRes = v ("lpres") * 0.01f; s.driveType = (int) v ("dist");
    s.pitchMix = v ("pmix") * 0.01f; s.pitchSnap = v ("psnap") > 0.5f;
    return s;
}

void Engine::prepare (double sampleRate, int)
{
    sr = sampleRate;
    for (auto& d : dl) d.init ((int) (sr * 9.0)); // room for reverse heads (2x the echo) and tape stops
    for (auto& sh : shift) { sh.window = juce::jmax (256, (int) (sr * 0.045)); sh.line.init (sh.window + 4); sh.phase = 0; }
    revPhase = 0; stopEnv = 0; stopping = false; stopCount = 0; dropEnv = 1.0f;
    pre.init ((int) (sr * 0.3));
    for (int i = 0; i < 4; ++i)
    {
        diff[i].length = juce::jmax (1, (int) (kDiffSamples[i] * sr / 44100.0));
        diff[i].line.init (diff[i].length + 2);
    }
    for (int i = 0; i < N; ++i) fdn[i].init ((int) ((kFdnMs[i] * 1.6 + 12.0) * 0.001 * sr));
    std::fill (std::begin (fdnLp), std::end (fdnLp), 0.0f);
    delaySmooth = delaySmoothR = 0; env = 0; pitchSmooth = 0; bbdPh = 0; loSmooth = hiSmooth = 0;
    for (auto* fx : { echoFx, tapFx, loopFx })
        for (int c = 0; c < 2; ++c) fx[c].reset();
}

void Engine::process (juce::AudioBuffer<float>& buffer, const Settings& s, juce::AudioPlayHead* playHead)
{
    const int n = buffer.getNumSamples();
    const int numCh = juce::jmin (2, buffer.getNumChannels());
    if (numCh == 0) return;
    float* L = buffer.getWritePointer (0);
    float* R = buffer.getWritePointer (numCh > 1 ? 1 : 0);

    const auto ht = skazi::hostTime (playHead);
    uiBpm.store (ht.bpm);
    const double target = juce::jlimit (1.0, sr * 4.0, skazi::noteBeats (s.time) * 60.0 / ht.bpm * sr);
    const bool dual = s.mode == Dual, multi = s.mode == MultiTap;
    // STEREO SPREAD: the right side runs up to 12 ms behind the left (WIDTH), so even a mono sound
    // echoes wide. PING-PONG is already left/right and DUAL has its own 3/4 timing.
    const double spread = (dual || s.mode == PingPong) ? 0.0 : 0.012 * s.width * sr;
    const double targetR = dual ? juce::jmax (1.0, target * 0.75) : target + spread; // DUAL: right side 3/4 of the left (1/4 -> 3/16)
    if (delaySmooth <= 0.0) delaySmooth = target;
    if (delaySmoothR <= 0.0) delaySmoothR = targetR;
    const bool tape = s.mode == Tape, bbd = s.mode == Bbd;
    const double smooth = tape ? 0.00005 : bbd ? 0.0003 : 0.0015;
    const double wowDepth = ((tape ? juce::jmax (0.2f, s.wow) : s.wow) + s.age * 0.8f) * 0.003 * sr;
    const double bbdDepth = bbd ? 0.0008 * sr : 0.0; // bucket-brigade chorus wobble, opposite on each side
    const bool invertR = s.mode == Stereo || bbd || dual;

    // echo filters + drive
    const float nyq = (float) (0.45 * sr);
    const float loTarget = juce::jmin (echoLowCut (s), nyq), hiTarget = juce::jmin (echoHighCut (s), nyq);
    if (loSmooth <= 0.0f) { loSmooth = loTarget; hiSmooth = hiTarget; }
    const float aCut = (float) (1.0 - std::exp (-1.0 / (0.02 * sr)));
    const double qLo = resoQ (s.loRes), qHi = resoQ (s.hiRes);
    EchoCoeffs ec {};
    ec.r2Lo = (float) (1.0 / qLo); ec.kLo = (float) juce::jlimit (0.0, 1.0, 1.0 / qLo - 1.0); ec.r2Hi = (float) (1.0 / qHi);
    ec.type = juce::jlimit (0, 3, s.driveType);
    switch (ec.type)
    {
        case DriveTube:  ec.drive = 1.0f + s.drive * 8.0f; break;
        case DriveFuzz:  ec.drive = 1.0f + s.drive * 30.0f; break;
        case DriveCrush: ec.drive = 1.0f; break;
        default:         ec.drive = 1.0f + s.drive * 3.0f + (tape ? 1.0f : 0.0f) + (bbd ? 0.5f : 0.0f); break;
    }
    ec.step = (float) std::exp2 (-(11.0 - 9.0 * s.drive));            // 12 bits down to 3
    ec.hold = 1 + (int) (s.drive * s.drive * 15.0 * sr / 48000.0);    // sample-rate reduction
    ec.dcA = (float) std::exp (-twoPi * 15.0 / sr);
    ec.bbd = bbd; ec.aBbd = (float) (1.0 - std::exp (-twoPi * 7000.0 / sr));
    // You hear every echo through the full resonance, but the copy that goes back round the loop gets a softer Q:
    // FEEDBACK keeps its length and feedback x peak stays under 1, so the echoes always fade.
    float fb = juce::jmin (0.95f, s.feedback);
    double qLoLoop = qLo, qHiLoop = qHi, peak = 1.0;
    const double loHz = juce::jmax (20.0f, loTarget), hiHz = juce::jmax (20.0f, hiTarget);
    for (int k = 0; k < 12; ++k)
    {
        peak = filterPeak (loHz, qLoLoop, hiHz, qHiLoop);
        if (fb * peak <= 0.97) break;
        qLoLoop = 0.5 + (qLoLoop - 0.5) * 0.7;
        qHiLoop = 0.5 + (qHiLoop - 0.5) * 0.7;
    }
    fb = juce::jmin (fb, (float) (0.97 / peak));
    EchoCoeffs loopEc = ec;
    loopEc.r2Lo = (float) (1.0 / qLoLoop); loopEc.kLo = (float) juce::jlimit (0.0, 1.0, 1.0 / qLoLoop - 1.0); loopEc.r2Hi = (float) (1.0 / qHiLoop);
    // while the two match, the loop copy just follows the heard one, so it starts warm (no click) when RESO goes up
    const bool softLoop = qLoLoop != qLo || qHiLoop != qHi;
    if (! softLoop) { loopFx[0] = echoFx[0]; loopFx[1] = echoFx[1]; }

    const double pitchTarget = s.pitchSnap ? std::round (s.pitch) : s.pitch;
    const double aPitch = 1.0 - std::exp (-1.0 / (0.05 * sr));
    const double beatSamples = 60.0 / ht.bpm * sr;
    const float stopIn = (float) (1.0 - std::exp (-1.0 / (0.25 * sr))), stopOut = (float) (1.0 - std::exp (-1.0 / (0.15 * sr)));
    const double maxRead = dl[0].buf.size() - 8.0;

    const float att = (float) (1.0 - std::exp (-1.0 / (0.001 * sr)));
    const float rel = (float) (1.0 - std::exp (-1.0 / (s.duckRelMs * 0.001 * sr)));

    double lens[N];
    float gains[N];
    const float dampFc = 20000.0f * (1.0f - s.damp) + 1500.0f * s.damp;
    const float aDamp = (float) (1.0 - std::exp (-twoPi * juce::jmin (dampFc, (float) (0.45 * sr)) / sr));
    for (int i = 0; i < N; ++i)
    {
        lens[i] = kFdnMs[i] * (0.35 + 1.25 * s.size) * 0.001 * sr;
        gains[i] = s.freeze ? 1.0f : (float) std::pow (10.0, -3.0 * lens[i] / (juce::jmax (0.2f, s.decay) * sr));
    }
    const double preSamples = juce::jlimit (1.0, sr * 0.29, s.predelayMs * 0.001 * sr);


    const float sideGain = s.width < 0.5f ? 2.0f * s.width : 1.0f + (s.width - 0.5f); // 0 mono, 50% as recorded, 100% 1.5x
    const float decayMeter = (float) std::exp (-(double) n / (0.2 * sr));
    for (int i = 0; i < n; ++i)
    {
        const float dryL = L[i], dryR = R[i];

        // duck the wet signal while the dry signal plays
        const float x = juce::jmax (std::abs (dryL), std::abs (dryR));
        env += (x - env) * (x > env ? att : rel);
        const float duckGain = 1.0f - s.duck * juce::jlimit (0.0f, 1.0f, env * 3.0f);

        // delay
        delaySmooth += (target - delaySmooth) * smooth;
        delaySmoothR += (targetR - delaySmoothR) * smooth;
        loSmooth += (loTarget - loSmooth) * aCut;
        hiSmooth += (hiTarget - hiSmooth) * aCut;
        ec.gLo = (float) std::tan (juce::MathConstants<double>::pi * loSmooth / sr);
        ec.gHi = (float) std::tan (juce::MathConstants<double>::pi * hiSmooth / sr);
        pitchSmooth += (pitchTarget - pitchSmooth) * aPitch;
        if (std::abs (pitchTarget - pitchSmooth) < 1.0e-3) pitchSmooth = pitchTarget;
        const double pitchRatio = std::exp2 (pitchSmooth / 12.0);
        bbdPh += 0.7 / sr;
        if (bbdPh > 1.0) bbdPh -= 1.0;
        wowPh += 0.6 / sr; flutterPh += 7.0 / sr;
        if (wowPh > 1.0) wowPh -= 1.0;
        if (flutterPh > 1.0) flutterPh -= 1.0;
        const double mod = wowDepth * (std::sin (twoPi * wowPh) + 0.2 * std::sin (twoPi * flutterPh))
                         + bbdDepth * (1.0 + std::sin (twoPi * bbdPh));
        const double dBase = juce::jmax (2.0, delaySmooth), dBaseR = juce::jmax (2.0, delaySmoothR);

        // TAPE STOP: now and then (once per beat it rolls the dice) the tape drags to a halt and spins back up
        if (++stopCount >= beatSamples)
        {
            stopCount = 0;
            if (! stopping && s.tapeStop > 0.0f && rng.nextFloat() < s.tapeStop * 0.3f)
            {
                stopping = true;
                stopHold = sr * (0.3 + 0.3 * rng.nextFloat());
            }
        }
        if (stopping) { stopEnv += (1.0f - stopEnv) * stopIn; if (--stopHold <= 0) stopping = false; }
        else          stopEnv += (0.0f - stopEnv) * stopOut;
        const double lag = stopEnv * 0.25 * sr;

        const double modR = invertR ? -mod : mod;
        // every echo goes through the filters + drive before you hear it, and again on each trip round the loop
        const float rawL = dl[0].read (juce::jlimit (1.0, maxRead, dBase + mod + lag));
        const float rawR = dl[1].read (juce::jlimit (1.0, maxRead, dBaseR + modR + lag));
        const float pL = echoFx[0].process (rawL, ec), pR = echoFx[1].process (rawR, ec);
        loopEc.gLo = ec.gLo; loopEc.gHi = ec.gHi;
        const float lL = softLoop ? loopFx[0].process (rawL, loopEc) : pL; // what goes back round the loop
        const float lR = softLoop ? loopFx[1].process (rawR, loopEc) : pR;
        float eL = pL, eR = pR;

        // MULTI-TAP: extra heads at 1/4, 1/2 and 3/4 of the echo, spread left / right / left
        if (multi)
        {
            auto tap = [&] (int c, double frac) { return dl[c].read (juce::jlimit (1.0, maxRead, dBase * frac + (c ? modR : mod) + lag)); };
            eL += tapFx[0].process (0.3f * tap (0, 0.25) + 0.55f * tap (0, 0.75), ec);
            eR += tapFx[1].process (0.45f * tap (1, 0.5), ec);
        }

        // REVERSE: a second head runs backwards through the last echo-length of tape
        if (s.reverse > 0.0f)
        {
            revPhase += 1.0;
            if (revPhase >= dBase) revPhase = 0.0;
            const double rd = juce::jlimit (1.0, maxRead, 1.0 + 2.0 * revPhase + lag);
            const float w = (float) std::sin (juce::MathConstants<double>::pi * revPhase / dBase);
            eL = eL * (1.0f - s.reverse) + dl[0].read (rd) * w * s.reverse;
            eR = eR * (1.0f - s.reverse) + dl[1].read (rd) * w * s.reverse;
        }

        // AGE: hiss and the odd dropout
        if (s.age > 0.0f)
        {
            if (rng.nextFloat() < s.age * 0.00004f) dropEnv = 0.15f + 0.3f * rng.nextFloat();
            dropEnv += (1.0f - dropEnv) * 0.0004f;
            const float dip = 1.0f - s.age * (1.0f - dropEnv);
            eL = eL * dip + (rng.nextFloat() - 0.5f) * 0.006f * s.age;
            eR = eR * dip + (rng.nextFloat() - 0.5f) * 0.006f * s.age;
        }

        // PITCH: every trip round the tape is shifted again, so the echoes spiral up or down.
        // BLEND under 100% keeps the unshifted echo too: at +12 that stacks octaves (shimmer).
        auto feedback = [&] (float x, int c)
        {
            const float shifted = shift[c].process (x, pitchRatio);
            return (shifted * s.pitchMix + x * (1.0f - s.pitchMix)) * fb;
        };
        if (s.mode == PingPong)
        {
            dl[0].write (0.5f * (dryL + dryR) + feedback (lR, 1));
            dl[1].write (feedback (lL, 0));
        }
        else
        {
            dl[0].write (dryL + feedback (lL, 0));
            dl[1].write (dryR + feedback (lR, 1));
        }

        // reverb: pre-delay -> 4 diffusers -> 8-line FDN with Householder mixing
        pre.write (s.freeze ? 0.0f : 0.5f * (dryL + dryR) + 0.25f * (eL + eR) * s.delayMix);
        float in = pre.read (preSamples);
        for (auto& d : diff) in = d.process (in);

        modPh += 0.5 / sr;
        if (modPh > 1.0) modPh -= 1.0;
        const double m = std::sin (twoPi * modPh) * 8.0;
        float o[N], sum = 0.0f;
        for (int k = 0; k < N; ++k)
        {
            o[k] = fdn[k].read (lens[k] + (k == 0 ? m : k == 3 ? -m : 0.0));
            fdnLp[k] += (o[k] - fdnLp[k]) * aDamp;
            const float damped = s.freeze ? o[k] : fdnLp[k];
            o[k] = damped * gains[k];
            sum += o[k];
        }
        float revL = 0.0f, revR = 0.0f;
        for (int k = 0; k < N; ++k)
        {
            const float y = o[k] - 0.25f * sum; // Householder: I - 2/N * ones
            fdn[k].write (y + in * ((k & 1) ? -0.5f : 0.5f));
            if (k & 1) revR += o[k] * ((k & 2) ? -0.35f : 0.35f);
            else       revL += o[k] * ((k & 2) ? -0.35f : 0.35f);
        }
        const float mid = 0.5f * (revL + revR), side = 0.5f * (revL - revR) * sideGain;
        revL = mid + side; revR = mid - side;

        // WIDTH works on the echoes too: past 50% it pushes them further out, to 1.5x the side at 100%.
        // PING-PONG echoes are already hard left / right, so they stay clean there instead of leaking across.
        const float echoMid = 0.5f * (eL + eR), echoSide = 0.5f * (eL - eR) * (s.mode == PingPong ? juce::jmin (1.0f, sideGain) : sideGain);
        eL = echoMid + echoSide; eR = echoMid - echoSide;

        const float wetL = (eL * s.delayMix + revL * s.reverbMix) * duckGain;
        const float wetR = (eR * s.delayMix + revR * s.reverbMix) * duckGain;
        const float outL = skazi::safeClip (((s.dry ? dryL : 0.0f) + wetL) * s.gain);
        const float outR = skazi::safeClip (((s.dry ? dryR : 0.0f) + wetR) * s.gain);
        if (numCh > 1) { L[i] = outL; R[i] = outR; }
        else           { L[i] = 0.5f * (outL + outR); }

        wetPeak = juce::jmax (wetPeak, std::abs (wetL), std::abs (wetR));
        outPeak = juce::jmax (outPeak, std::abs (outL), std::abs (outR));
    }
    uiWet.store (wetPeak);
    uiOut.store (outPeak);
    uiDuck.store (1.0f - s.duck * juce::jlimit (0.0f, 1.0f, env * 3.0f));
    wetPeak *= decayMeter;
    outPeak *= decayMeter;
}
} // namespace tahini
