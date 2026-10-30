#include <map>
#include "AudioToMidi.h"
#include <juce_dsp/juce_dsp.h>
#include <algorithm>
#include <array>
#include <numeric>

namespace midigod
{
namespace
{
bool cancelled (const std::atomic<bool>* c) { return c != nullptr && c->load(); }

std::vector<float> resample (const std::vector<float>& in, double srIn, double srOut)
{
    if (std::abs (srIn - srOut) < 1.0) return in;
    std::vector<float> x (in);
    if (srOut < srIn)
        for (int pass = 0; pass < 2; ++pass) // anti-alias before dropping the rate
        {
            juce::IIRFilter f;
            f.setCoefficients (juce::IIRCoefficients::makeLowPass (srIn, 0.42 * srOut));
            f.processSamples (x.data(), (int) x.size());
        }
    const double ratio = srIn / srOut;
    const int outLen = juce::jmax (0, (int) ((double) x.size() / ratio) - 8);
    std::vector<float> out ((size_t) outLen);
    juce::LagrangeInterpolator li;
    if (outLen > 0) li.process (ratio, x.data(), out.data(), outLen);
    return out;
}

void lowPass (std::vector<float>& x, double sr, double hz)
{
    juce::IIRFilter f;
    f.setCoefficients (juce::IIRCoefficients::makeLowPass (sr, hz));
    f.processSamples (x.data(), (int) x.size());
}

// Magnitude spectrum of a Hann window centred on sample `offset + n * hop`, for every frame n
template <typename Fn>
void forEachSpectrum (const std::vector<float>& x, int order, int hop, int offset, int numFrames, Fn&& fn)
{
    const int N = 1 << order;
    juce::dsp::FFT fft (order);
    std::vector<float> win ((size_t) N), buf ((size_t) N * 2);
    for (int i = 0; i < N; ++i) win[(size_t) i] = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * i / N);
    for (int n = 0; n < numFrames; ++n)
    {
        const int start = offset + n * hop - N / 2;
        std::fill (buf.begin(), buf.end(), 0.0f);
        for (int i = 0; i < N; ++i)
        {
            const int k = start + i;
            if (k >= 0 && k < (int) x.size()) buf[(size_t) i] = x[(size_t) k] * win[(size_t) i];
        }
        fft.performFrequencyOnlyForwardTransform (buf.data(), true);
        fn (n, buf.data(), N / 2 + 1);
    }
}

float mean (const std::vector<float>& v) { return v.empty() ? 0.0f : std::accumulate (v.begin(), v.end(), 0.0f) / (float) v.size(); }

float percentile (std::vector<float> v, float p)
{
    if (v.empty()) return 0.0f;
    const size_t k = std::min (v.size() - 1, (size_t) (p * (float) (v.size() - 1)));
    std::nth_element (v.begin(), v.begin() + (long) k, v.end());
    return v[k];
}

// Local maxima above a moving median + delta, at least minGap frames apart
std::vector<int> pickPeaks (const std::vector<float>& f, float delta, int minGap, int radius)
{
    std::vector<int> peaks;
    std::vector<float> win;
    int last = -minGap;
    const int n = (int) f.size();
    for (int i = 0; i < n; ++i)
    {
        bool isMax = true;
        for (int k = juce::jmax (0, i - 2); k <= juce::jmin (n - 1, i + 2) && isMax; ++k)
            if (f[(size_t) k] > f[(size_t) i]) isMax = false;
        if (! isMax || i - last < minGap) continue;
        win.assign (f.begin() + juce::jmax (0, i - radius), f.begin() + juce::jmin (n, i + radius + 1));
        std::nth_element (win.begin(), win.begin() + (long) (win.size() / 2), win.end());
        if (f[(size_t) i] > win[win.size() / 2] + delta)
        {
            peaks.push_back (i);
            last = i;
        }
    }
    return peaks;
}

double roundUpToBars (double beats) { return juce::jlimit (4.0, 1024.0, std::ceil (beats / 4.0 - 1.0e-6) * 4.0); }

//==============================================================================
// Melody and bass: YIN pitch per frame, split into notes on silence, pitch jumps and onsets.
// Glides and pitch dives are not notes: a note's pitch is where it settles, and the moving frames
// go to the note they leave from.
std::vector<Note> trackMonophonic (const std::vector<float>& mono, double srIn, bool bass, float sens, double bpm, const std::atomic<bool>* cancel)
{
    const double sr = bass ? 5512.5 : 11025.0;
    auto x = resample (mono, srIn, sr);
    if (bass) { lowPass (x, sr, 700.0); lowPass (x, sr, 700.0); }

    const int W = bass ? 384 : 512, hop = bass ? 64 : 128;
    const int tauMin = (int) (sr / (bass ? 400.0 : 1500.0)), tauMax = (int) (sr / (bass ? 30.0 : 55.0));
    const int numFrames = ((int) x.size() - W - tauMax) / hop + 1;
    if (numFrames < 8) return {};

    std::vector<float> pitch ((size_t) numFrames, -1.0f), rms ((size_t) numFrames, 0.0f), d ((size_t) tauMax + 1);
    const float threshold = 0.10f + 0.15f * sens;
    for (int n = 0; n < numFrames; ++n)
    {
        if ((n & 255) == 0 && cancelled (cancel)) return {};
        const float* s = x.data() + (size_t) n * (size_t) hop;
        double e = 0.0;
        for (int j = 0; j < W; ++j) e += s[j] * s[j];
        rms[(size_t) n] = (float) std::sqrt (e / W);
        if (e < 1.0e-9) continue;

        // cumulative-mean-normalised difference
        double running = 0.0;
        d[0] = 1.0f;
        int found = -1;
        for (int tau = 1; tau <= tauMax; ++tau)
        {
            double sum = 0.0;
            for (int j = 0; j < W; ++j) { const float diff = s[j] - s[j + tau]; sum += diff * diff; }
            running += sum;
            d[(size_t) tau] = running > 0.0 ? (float) (sum * tau / running) : 1.0f;
        }
        for (int tau = tauMin; tau < tauMax; ++tau)
            if (d[(size_t) tau] < threshold)
            {
                while (tau + 1 < tauMax && d[(size_t) tau + 1] < d[(size_t) tau]) ++tau;
                found = tau;
                break;
            }
        if (found < 0) continue;
        const float a = d[(size_t) found - 1], b = d[(size_t) found], c = d[(size_t) found + 1];
        const float denom = a - 2.0f * b + c;
        const float tau = (float) found + (std::abs (denom) > 1.0e-9f ? 0.5f * (a - c) / denom : 0.0f);
        pitch[(size_t) n] = 69.0f + 12.0f * std::log2 ((float) sr / tau / 440.0f);
    }

    // quiet frames are not notes, however periodic they look
    const float maxRms = *std::max_element (rms.begin(), rms.end());
    if (maxRms <= 0.0f) return {};
    const float gate = maxRms * juce::Decibels::decibelsToGain (-(28.0f + 24.0f * sens));
    std::vector<bool> voiced ((size_t) numFrames);
    for (int n = 0; n < numFrames; ++n) voiced[(size_t) n] = pitch[(size_t) n] > 0.0f && rms[(size_t) n] > gate;

    // 5-frame median over voiced neighbours removes single-frame octave slips
    std::vector<float> smooth (pitch);
    for (int n = 0; n < numFrames; ++n)
    {
        if (! voiced[(size_t) n]) continue;
        float v[5]; int k = 0;
        for (int m = juce::jmax (0, n - 2); m <= juce::jmin (numFrames - 1, n + 2); ++m)
            if (voiced[(size_t) m]) v[k++] = pitch[(size_t) m];
        std::nth_element (v, v + k / 2, v + k);
        smooth[(size_t) n] = v[k / 2];
    }

    // onsets split repeated notes of the same pitch (rolling basses)
    std::vector<float> flux ((size_t) numFrames, 0.0f), prev;
    forEachSpectrum (x, bass ? 8 : 9, hop, W / 2, numFrames, [&] (int n, const float* mag, int bins)
    {
        std::vector<float> cur ((size_t) bins);
        float f = 0.0f;
        for (int k = 1; k < bins; ++k)
        {
            cur[(size_t) k] = std::log1p (10.0f * mag[k]);
            if (! prev.empty()) f += juce::jmax (0.0f, cur[(size_t) k] - prev[(size_t) k]);
        }
        flux[(size_t) n] = f;
        prev = std::move (cur);
    });
    if (cancelled (cancel)) return {};
    // an onset has to bring more energy: the splatter when a note stops is not a new note
    std::vector<bool> onset ((size_t) numFrames, false);
    for (int p : pickPeaks (flux, (1.2f - 0.8f * sens) * mean (flux), 4, 8))
    {
        float before = 1.0e9f, after = 0.0f;
        for (int k = juce::jmax (0, p - 3); k < p; ++k) before = juce::jmin (before, rms[(size_t) k]);
        for (int k = p; k < juce::jmin (numFrames, p + 3); ++k) after = juce::jmax (after, rms[(size_t) k]);
        if (p < 3 || after > before * 1.25f) onset[(size_t) p] = true;
    }

    const double secPerFrame = hop / sr, beatsPerSec = bpm / 60.0;
    const double frameTime0 = (W / 2.0) / sr;
    std::vector<Note> notes;
    int lastEnd = -10; // frame where the last note ended
    auto close = [&] (int a, int b)
    {
        // the pitch is the semitone the segment holds longest in one run, so glide-ins and dives don't pull it
        std::map<int, int> run;
        int best = -1, bestRun = 0, cur = -1, len = 0;
        float peak = 0.0f;
        for (int m = a; m < b; ++m)
        {
            if (! voiced[(size_t) m]) { cur = -1; continue; }
            peak = juce::jmax (peak, rms[(size_t) m]);
            const int q = juce::roundToInt (smooth[(size_t) m]);
            len = q == cur ? len + 1 : 1;
            cur = q;
            if (len > bestRun) { bestRun = len; best = q; }
        }
        if (bestRun < 3)
        {
            // never settled: a glide or a dive on the end of the note before, which keeps sounding through it
            if (! notes.empty() && a - lastEnd <= 2)
            {
                notes.back().length = (frameTime0 + (b - 0.5) * secPerFrame) * beatsPerSec - notes.back().start;
                lastEnd = b;
            }
            return;
        }
        lastEnd = b;
        int p = best;
        while (bass && p < 24) p += 12;
        for (int m = juce::jmax (0, a - 2); m <= juce::jmin (numFrames - 1, a + 1); ++m)
            if (onset[(size_t) m]) { a = m; break; }
        const double t0 = frameTime0 + (a - 0.5) * secPerFrame, t1 = frameTime0 + (b - 0.5) * secPerFrame;
        const float db = juce::Decibels::gainToDecibels (peak / maxRms);
        notes.push_back ({ juce::jlimit (0, 127, p), juce::jmax (0.0, t0) * beatsPerSec, (t1 - juce::jmax (0.0, t0)) * beatsPerSec,
                           juce::jlimit (20, 127, juce::roundToInt (127.0f + db * 2.2f)) });
    };

    // after a new note starts, its pitch may still be sliding (legato glide, pluck attack): while `settle` runs,
    // the reference pitch follows the latest steady frames instead of splitting again
    const int settleFrames = juce::jmax (3, (int) std::ceil (0.07 / secPerFrame));
    int start = -1, silent = 0, change = 0, settle = 0;
    std::vector<float> vals;
    for (int n = 0; n < numFrames; ++n)
    {
        if (! voiced[(size_t) n])
        {
            if (start >= 0 && ++silent >= 2) { close (start, n - silent + 1); start = -1; }
            continue;
        }
        silent = 0;
        const float p = smooth[(size_t) n];
        if (start < 0) { start = n; vals.assign (1, p); change = 0; settle = settleFrames / 2; continue; }
        if (onset[(size_t) n] && n - start >= 3) { close (start, n); start = n; vals.assign (1, p); change = 0; settle = settleFrames / 2; continue; }
        if (settle > 0)
        {
            --settle;
            if (std::abs (p - vals.back()) < 0.35f) vals.push_back (p);
            else vals.assign (1, p);
            continue;
        }
        std::vector<float> recent (vals.end() - (long) std::min (vals.size(), (size_t) 7), vals.end());
        std::nth_element (recent.begin(), recent.begin() + (long) (recent.size() / 2), recent.end());
        const float ref = recent[recent.size() / 2];
        if (std::abs (p - ref) > 0.8f)
        {
            // the new note starts where the pitch left the old one: that is where a glide's note-on was
            if (++change >= 3) { close (start, n - 2); start = n - 2; vals.assign (1, p); change = 0; settle = settleFrames; }
            continue;
        }
        change = 0;
        vals.push_back (p);
    }
    if (start >= 0) close (start, numFrames);

    // a quiet same-pitch scrap straight after a note is its release tail: give it back to the note
    std::vector<Note> merged;
    for (auto& n : notes)
    {
        if (! merged.empty())
        {
            auto& p = merged.back();
            const double gapSec = (n.start - (p.start + p.length)) / beatsPerSec;
            if (n.pitch == p.pitch && gapSec < 0.02 && n.length / beatsPerSec < 0.12 && n.velocity < p.velocity - 6)
            {
                p.length = n.start + n.length - p.start;
                continue;
            }
        }
        merged.push_back (n);
    }
    return merged;
}

//==============================================================================
// Drums: onsets in three bands, sorted into kick, snare/clap and closed/open hat
std::vector<Note> trackDrums (const std::vector<float>& mono, double srIn, float sens, double bpm, const std::atomic<bool>* cancel)
{
    const double sr = 22050.0;
    const auto x = resample (mono, srIn, sr);
    const int order = 9, hop = 128;
    const int numFrames = (int) x.size() / hop;
    if (numFrames < 16) return {};

    enum { Low, Mid, High, NumBands };
    const int lo[NumBands] = { 1, 28, 163 }, hi[NumBands] = { 4, 116, 257 }; // ~43-170 Hz, 1.2-5 kHz, 7-11 kHz
    std::vector<float> energy[NumBands];
    for (auto& e : energy) e.resize ((size_t) numFrames);
    forEachSpectrum (x, order, hop, 0, numFrames, [&] (int n, const float* mag, int bins)
    {
        for (int b = 0; b < NumBands; ++b)
        {
            double e = 0.0;
            for (int k = lo[b]; k < juce::jmin (hi[b], bins); ++k) e += mag[k] * mag[k];
            energy[b][(size_t) n] = (float) e;
        }
    });
    if (cancelled (cancel)) return {};

    struct Hit { int frame; float strength[NumBands]; };
    std::vector<Hit> hits;
    std::vector<float> flux[NumBands];
    float norm[NumBands], maxE[NumBands];
    for (int b = 0; b < NumBands; ++b)
    {
        const float m = juce::jmax (1.0e-12f, mean (energy[b]));
        maxE[b] = *std::max_element (energy[b].begin(), energy[b].end());
        flux[b].assign ((size_t) numFrames, 0.0f);
        for (int n = 1; n < numFrames; ++n)
            flux[b][(size_t) n] = juce::jmax (0.0f, std::log1p (energy[b][(size_t) n] * 20.0f / m) - std::log1p (energy[b][(size_t) n - 1] * 20.0f / m));
        norm[b] = juce::jmax (1.0e-6f, percentile (flux[b], 0.995f));
        const float floorE = maxE[b] * juce::Decibels::decibelsToGain (-(35.0f + 20.0f * sens));
        for (int p : pickPeaks (flux[b], (1.0f - 0.6f * sens) * norm[b] * 0.35f, 8, 10))
        {
            float e = 0.0f;
            for (int k = p; k < juce::jmin (numFrames, p + 3); ++k) e = juce::jmax (e, energy[b][(size_t) k]);
            if (e < floorE) continue;
            // join hits in other bands that land within 6 frames (~35 ms): a kick's low end peaks after its click
            Hit* h = nullptr;
            for (auto& other : hits) if (std::abs (other.frame - p) <= 6) { h = &other; break; }
            if (h == nullptr) { hits.push_back ({ p, { 0, 0, 0 } }); h = &hits.back(); }
            h->frame = juce::jmin (h->frame, p);
            h->strength[b] = juce::jmax (h->strength[b], flux[b][(size_t) p] / norm[b]);
        }
    }

    // energy a hit adds to a band over its first ~35 ms: a kick's click is over in a few ms, a clap's noise is not
    auto rise = [&] (int b, int f)
    {
        float before = 1.0e30f, after = 0.0f;
        for (int k = juce::jmax (0, f - 4); k < f; ++k) before = juce::jmin (before, energy[b][(size_t) k]);
        if (f == 0) before = 0.0f;
        int count = 0;
        for (int k = f; k < juce::jmin (numFrames, f + 6); ++k, ++count) after += energy[b][(size_t) k];
        return juce::jmax (0.0f, after / (float) juce::jmax (1, count) - before);
    };

    // The loop's own hits sort themselves: split a feature into a low and a high group (2-means),
    // or say there is only one group when the two would sit closer than minGap
    auto twoGroups = [] (std::vector<float> v, float minGap, float none)
    {
        if (v.size() < 4) return none;
        std::sort (v.begin(), v.end());
        float split = none, bestCost = 1.0e30f;
        const size_t smallest = std::max ((size_t) 2, v.size() / 5); // a group is more than a stray hit or two
        for (size_t k = smallest; k + smallest <= v.size(); ++k)
        {
            auto stats = [&] (size_t a, size_t b)
            {
                float m = 0.0f, var = 0.0f;
                for (size_t i = a; i < b; ++i) m += v[i];
                m /= (float) (b - a);
                for (size_t i = a; i < b; ++i) var += (v[i] - m) * (v[i] - m);
                return std::pair<float, float> { m, var };
            };
            const auto lo = stats (0, k), hi = stats (k, v.size());
            if (hi.first - lo.first > minGap && lo.second + hi.second < bestCost) { bestCost = lo.second + hi.second; split = 0.5f * (lo.first + hi.first); }
        }
        return split;
    };

    const float eps = 1.0e-12f;
    std::vector<float> midOnKick, midVsHigh, hatHighs;
    for (auto& h : hits)
    {
        const float L = rise (Low, h.frame) + eps, M = rise (Mid, h.frame) + eps, H = rise (High, h.frame) + eps;
        if (M < 1.0e-9f || H < 1.0e-9f) continue; // nothing to compare (the very first frame)
        if (h.strength[Low] > 0.25f) midOnKick.push_back (std::log (M / L));
        else midVsHigh.push_back (std::log (M / H));
    }
    // snares and claps are loudest at 1-5 kHz, hats above 7 kHz; open and closed hats may form two
    // groups of their own, so a snare also needs clearly more 1-5 kHz than top end
    const float split = juce::jmax (0.4f, twoGroups (midVsHigh, 1.0f, 0.0f));
    for (auto& h : hits)
        if (h.strength[Low] <= 0.25f)
        {
            const float M = rise (Mid, h.frame) + eps, H = rise (High, h.frame) + eps;
            if (std::log (M / H) <= split) hatHighs.push_back (std::log (H));
        }
    // A kick puts some energy in the upper bands too. With a clap on some kicks, those form a second group;
    // a hat on a kick shows as top-band energy close to what the loop's own hats have.
    const float clapOnKick = twoGroups (midOnKick, std::log (2.5f), 1.0e30f);
    const float hatLevel = hatHighs.empty() ? 1.0e30f : percentile (hatHighs, 0.5f) + std::log (0.15f + 0.25f * (1.0f - sens));

    const double beatsPerSec = bpm / 60.0;
    std::vector<Note> notes;
    for (auto& h : hits)
    {
        const double t = h.frame * hop / sr * beatsPerSec;
        const bool kick = h.strength[Low] > 0.25f;
        const float L = rise (Low, h.frame) + eps, M = rise (Mid, h.frame) + eps, H = rise (High, h.frame) + eps;
        bool snare, hat;
        if (kick)
        {
            notes.push_back ({ KickNote, t, 0.12, juce::jlimit (40, 127, juce::roundToInt (60.0f + 67.0f * juce::jmin (1.0f, h.strength[Low]))) });
            snare = std::log (M / L) > clapOnKick;
            hat = std::log (H) > hatLevel && ! (snare && M > 2.0f * H);
        }
        else
        {
            snare = h.strength[Mid] > 0.0f && std::log (M / H) > split;
            hat = ! snare && h.strength[High] > 0.0f;
        }
        const float s = h.strength[Mid], hh = h.strength[High];
        if (snare)
            notes.push_back ({ SnareNote, t, 0.12, juce::jlimit (40, 127, juce::roundToInt (60.0f + 67.0f * juce::jmin (1.0f, s))) });
        if (! hat) continue;
        // open hats ring: count frames until the top band falls to a quarter of its peak
        int peakF = h.frame;
        for (int k = h.frame; k < juce::jmin (numFrames, h.frame + 4); ++k) if (energy[High][(size_t) k] > energy[High][(size_t) peakF]) peakF = k;
        int ring = 0;
        while (peakF + ring < numFrames && energy[High][(size_t) (peakF + ring)] > energy[High][(size_t) peakF] * 0.25f && ring < 200) ++ring;
        const bool open = ring * hop / sr > 0.12;
        notes.push_back ({ open ? OpenHatNote : HatNote, t, open ? 0.2 : 0.1, juce::jlimit (30, 127, juce::roundToInt (50.0f + 77.0f * juce::jmin (1.0f, hh))) });
    }
    return notes;
}

//==============================================================================
// Chords: a chroma vector per beat, matched against every major and minor triad
std::vector<Note> trackChords (const std::vector<float>& mono, double srIn, float sens, double bpm, const std::atomic<bool>* cancel)
{
    const double sr = 11025.0;
    const auto x = resample (mono, srIn, sr);
    const int order = 12, hop = 512, N = 1 << order;
    const int numFrames = (int) x.size() / hop;
    if (numFrames < 4) return {};
    const double beatSec = 60.0 / bpm;
    const int numBeats = (int) std::ceil (x.size() / sr / beatSec);

    std::vector<std::array<float, 12>> chroma ((size_t) numBeats);
    std::vector<float> beatEnergy ((size_t) numBeats, 0.0f);
    for (auto& c : chroma) c.fill (0.0f);
    std::vector<int> pcOfBin ((size_t) N / 2 + 1, -1);
    for (int k = 1; k <= N / 2; ++k)
    {
        const double f = k * sr / N;
        if (f >= 55.0 && f <= 2000.0) pcOfBin[(size_t) k] = ((int) std::lround (69.0 + 12.0 * std::log2 (f / 440.0)) % 12 + 12) % 12;
    }
    forEachSpectrum (x, order, hop, 0, numFrames, [&] (int n, const float* mag, int bins)
    {
        const int beat = juce::jlimit (0, numBeats - 1, (int) (n * hop / sr / beatSec));
        for (int k = 1; k < bins; ++k)
            if (pcOfBin[(size_t) k] >= 0)
            {
                chroma[(size_t) beat][(size_t) pcOfBin[(size_t) k]] += std::sqrt (mag[k]);
                beatEnergy[(size_t) beat] += mag[k] * mag[k];
            }
    });
    if (cancelled (cancel)) return {};

    const float maxE = *std::max_element (beatEnergy.begin(), beatEnergy.end());
    const float gate = maxE * juce::Decibels::decibelsToGain (-(30.0f + 20.0f * sens));
    std::vector<int> label ((size_t) numBeats, -1); // root + 12 * minor, -1 = nothing
    for (int b = 0; b < numBeats; ++b)
    {
        if (beatEnergy[(size_t) b] < gate || maxE <= 0.0f) continue;
        const auto& c = chroma[(size_t) b];
        float len = 0.0f;
        for (float v : c) len += v * v;
        len = std::sqrt (juce::jmax (1.0e-12f, len));
        float best = 0.0f;
        for (int q = 0; q < 2; ++q)
            for (int r = 0; r < 12; ++r)
            {
                const float score = (c[(size_t) r] + c[(size_t) (r + (q == 0 ? 4 : 3)) % 12] + 0.8f * c[(size_t) (r + 7) % 12]) / (len * 1.6f);
                if (score > best) { best = score; label[(size_t) b] = r + 12 * q; }
            }
    }
    // a one-beat chord between two of the same is a passing note
    for (int b = 1; b + 1 < numBeats; ++b)
        if (label[(size_t) b - 1] == label[(size_t) b + 1] && label[(size_t) b] != label[(size_t) b - 1]) label[(size_t) b] = label[(size_t) b - 1];

    std::vector<Note> notes;
    for (int b = 0; b < numBeats;)
    {
        int run = 1;
        while (b + run < numBeats && label[(size_t) (b + run)] == label[(size_t) b]) ++run;
        if (label[(size_t) b] >= 0)
        {
            const int r = label[(size_t) b] % 12;
            const bool minor = label[(size_t) b] >= 12;
            const int root = 48 + r + (r < 5 ? 12 : 0);
            for (int p : { root, root + (minor ? 3 : 4), root + 7 })
                notes.push_back ({ p, (double) b, run * 0.98, 96 });
        }
        b += run;
    }
    return notes;
}
} // namespace

//==============================================================================
ConvertResult convertAudio (const juce::AudioBuffer<float>& audio, double sampleRate, int mode, float sensitivity, double bpm,
                            const std::atomic<bool>* cancel)
{
    ConvertResult res;
    const int len = audio.getNumSamples();
    if (len <= 0 || sampleRate <= 0.0) { res.error = "EMPTY AUDIO"; return res; }
    bpm = juce::jlimit (20.0, 999.0, bpm);
    sensitivity = juce::jlimit (0.0f, 1.0f, sensitivity);

    std::vector<float> mono ((size_t) len, 0.0f);
    const int chans = juce::jmax (1, audio.getNumChannels());
    for (int ch = 0; ch < audio.getNumChannels(); ++ch)
    {
        const float* d = audio.getReadPointer (ch);
        for (int i = 0; i < len; ++i) mono[(size_t) i] += d[i] / (float) chans;
    }

    res.seconds = len / sampleRate;
    res.drums = mode == DrumHits;
    switch (mode)
    {
        case BassLine:   res.notes = trackMonophonic (mono, sampleRate, true, sensitivity, bpm, cancel); break;
        case DrumHits:   res.notes = trackDrums (mono, sampleRate, sensitivity, bpm, cancel); break;
        case ChordTrack: res.notes = trackChords (mono, sampleRate, sensitivity, bpm, cancel); break;
        default:         res.notes = trackMonophonic (mono, sampleRate, false, sensitivity, bpm, cancel); break;
    }
    if (cancelled (cancel)) { res.error = "CANCELLED"; return res; }

    double end = res.seconds * bpm / 60.0;
    for (auto& n : res.notes) end = juce::jmax (end, n.start + n.length);
    res.lengthBeats = roundUpToBars (end);
    if (res.notes.empty()) res.error = "NO NOTES FOUND";
    return res;
}

ConvertResult convertFile (const juce::File& file, int mode, float sensitivity, double bpm, const std::atomic<bool>* cancel)
{
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));
    if (reader == nullptr)
    {
        ConvertResult res;
        res.error = "CAN'T READ " + file.getFileName().toUpperCase();
        return res;
    }
    const auto total = reader->lengthInSamples;
    const auto wanted = (juce::int64) juce::jmin ((double) total, kMaxSeconds * reader->sampleRate);
    juce::AudioBuffer<float> buf ((int) juce::jmin<juce::uint32> (2, juce::jmax<juce::uint32> (1, reader->numChannels)), (int) wanted);
    reader->read (&buf, 0, (int) wanted, 0, true, buf.getNumChannels() > 1);
    auto res = convertAudio (buf, reader->sampleRate, mode, sensitivity, bpm, cancel);
    res.truncated = wanted < total;
    return res;
}

bool isAudioFile (const juce::String& path)
{
    for (auto* ext : { ".wav", ".aif", ".aiff", ".flac", ".mp3", ".ogg", ".m4a" })
        if (path.endsWithIgnoreCase (ext)) return true;
    return false;
}
} // namespace midigod
