#pragma once

#include <cmath>
#include <array>
#include <vector>
#include <algorithm>

// SKWIDE engine building blocks. Every widening stage only writes into the side channel, so
// L + R always equals the (depth-EQ'd) mid: the plug-in can never cancel itself in mono.
namespace wide
{
    constexpr double twoPi = 6.283185307179586;

    // RBJ biquad, transposed direct form II, double precision state.
    struct Biquad
    {
        double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
        double z1 = 0, z2 = 0;

        float process (float x) noexcept
        {
            const double y = b0 * x + z1;
            z1 = b1 * x - a1 * y + z2;
            z2 = b2 * x - a2 * y;
            return (float) y;
        }

        void reset() noexcept { z1 = z2 = 0; }

        void set (double nb0, double nb1, double nb2, double a0, double na1, double na2) noexcept
        {
            b0 = nb0 / a0; b1 = nb1 / a0; b2 = nb2 / a0; a1 = na1 / a0; a2 = na2 / a0;
        }

        void lowpass (double sr, double f, double q) noexcept
        {
            const double w = twoPi * f / sr, c = std::cos (w), al = std::sin (w) / (2.0 * q);
            set ((1 - c) / 2, 1 - c, (1 - c) / 2, 1 + al, -2 * c, 1 - al);
        }

        void highpass (double sr, double f, double q) noexcept
        {
            const double w = twoPi * f / sr, c = std::cos (w), al = std::sin (w) / (2.0 * q);
            set ((1 + c) / 2, -(1 + c), (1 + c) / 2, 1 + al, -2 * c, 1 - al);
        }

        void allpass (double sr, double f, double q) noexcept
        {
            const double w = twoPi * f / sr, c = std::cos (w), al = std::sin (w) / (2.0 * q);
            set (1 - al, -2 * c, 1 + al, 1 + al, -2 * c, 1 - al);
        }

        void highShelf (double sr, double f, double gainDb) noexcept
        {
            const double A = std::pow (10.0, gainDb / 40.0), w = twoPi * f / sr, c = std::cos (w);
            const double al = std::sin (w) / 2.0 * std::sqrt (2.0), sq = 2.0 * std::sqrt (A) * al;
            set (A * ((A + 1) + (A - 1) * c + sq), -2 * A * ((A - 1) + (A + 1) * c), A * ((A + 1) + (A - 1) * c - sq),
                 (A + 1) - (A - 1) * c + sq, 2 * ((A - 1) - (A + 1) * c), (A + 1) - (A - 1) * c - sq);
        }

        void lowShelf (double sr, double f, double gainDb) noexcept
        {
            const double A = std::pow (10.0, gainDb / 40.0), w = twoPi * f / sr, c = std::cos (w);
            const double al = std::sin (w) / 2.0 * std::sqrt (2.0), sq = 2.0 * std::sqrt (A) * al;
            set (A * ((A + 1) - (A - 1) * c + sq), 2 * A * ((A - 1) - (A + 1) * c), A * ((A + 1) - (A - 1) * c - sq),
                 (A + 1) + (A - 1) * c + sq, -2 * ((A - 1) + (A + 1) * c), (A + 1) + (A - 1) * c - sq);
        }

        void peak (double sr, double f, double q, double gainDb) noexcept
        {
            const double A = std::pow (10.0, gainDb / 40.0), w = twoPi * f / sr, c = std::cos (w), al = std::sin (w) / (2.0 * q);
            set (1 + al * A, -2 * c, 1 - al * A, 1 + al / A, -2 * c, 1 - al / A);
        }
    };

    // Linkwitz-Riley 4th order split. low + high is an allpass of the input, so the crossover is phase coherent.
    struct Crossover
    {
        Biquad lp[2], hp[2];

        void set (double sr, double f) noexcept
        {
            for (auto& b : lp) b.lowpass (sr, f, 0.7071067811865476);
            for (auto& b : hp) b.highpass (sr, f, 0.7071067811865476);
        }

        void process (float x, float& low, float& high) noexcept
        {
            low = lp[1].process (lp[0].process (x));
            high = hp[1].process (hp[0].process (x));
        }

        void reset() noexcept
        {
            for (auto& b : lp) b.reset();
            for (auto& b : hp) b.reset();
        }
    };

    // Dense allpass chain: flat magnitude, phase that spins many turns across the spectrum.
    // Added to one side and subtracted from the other, it spreads a mono source into
    // interleaved left/right frequency zones that cancel exactly when summed to mono.
    struct Decorrelator
    {
        static constexpr int numStages = 10;
        Biquad stages[numStages];

        void prepare (double sr) noexcept
        {
            // Log spaced from 180 Hz to 15 kHz, with alternating Q so the zones are not too regular.
            for (int i = 0; i < numStages; ++i)
            {
                const double t = (double) i / (numStages - 1);
                const double f = std::min (180.0 * std::pow (15000.0 / 180.0, t), sr * 0.42);
                stages[i].allpass (sr, f, (i % 2) ? 0.9 : 0.55);
            }
        }

        float process (float x) noexcept
        {
            for (auto& s : stages)
                x = s.process (x);
            return x;
        }

        void reset() noexcept
        {
            for (auto& s : stages) s.reset();
        }
    };

    // Polyphase IIR Hilbert pair (Olli Niemitalo): two outputs 90 degrees apart from ~20 Hz to ~20 kHz.
    // Mixing them with cos/sin rotates the phase of the whole signal by any angle.
    struct HilbertPair
    {
        struct Chain
        {
            std::array<double, 4> a2 {};
            std::array<double, 4> x1 {}, x2 {}, y1 {}, y2 {};

            void init (const double (&coeffs)[4]) noexcept
            {
                for (int i = 0; i < 4; ++i)
                    a2[(size_t) i] = coeffs[i] * coeffs[i];
            }

            double process (double x) noexcept
            {
                for (size_t i = 0; i < 4; ++i)
                {
                    const double y = a2[i] * (x + y2[i]) - x2[i];
                    x2[i] = x1[i]; x1[i] = x;
                    y2[i] = y1[i]; y1[i] = y;
                    x = y;
                }
                return x;
            }

            void reset() noexcept { x1 = x2 = y1 = y2 = {}; }
        };

        Chain chainA, chainB;
        double delayed = 0.0;

        HilbertPair()
        {
            static constexpr double ca[4] { 0.6923878, 0.9360654322959, 0.9882295226860, 0.9987488452737 };
            static constexpr double cb[4] { 0.4021921162426, 0.8561710882420, 0.9722909545651, 0.9952884791278 };
            chainA.init (ca);
            chainB.init (cb);
        }

        // Returns the input rotated by angle (radians), with flat magnitude.
        float rotate (float x, float cosA, float sinA) noexcept
        {
            const double a = chainA.process (x);
            const double re = delayed;
            delayed = a;
            const double im = chainB.process (x);
            return (float) (re * cosA + im * sinA);
        }

        void reset() noexcept
        {
            chainA.reset();
            chainB.reset();
            delayed = 0.0;
        }
    };

    // Short Schroeder allpass diffuser: smears the mid into a dense early-reflection cloud for DEPTH.
    struct Diffuser
    {
        static constexpr int numStages = 4;
        static constexpr int maxDelay = 4096;
        std::array<std::array<float, maxDelay>, numStages> buffers {};
        std::array<int, numStages> lengths {}, positions {};

        void prepare (double sr) noexcept
        {
            static constexpr double ms[numStages] { 4.7, 7.3, 11.9, 17.3 };
            for (int i = 0; i < numStages; ++i)
                lengths[(size_t) i] = std::min (maxDelay - 1, std::max (1, (int) (ms[i] * 0.001 * sr)));
            reset();
        }

        float process (float x) noexcept
        {
            constexpr float g = 0.62f;
            for (size_t i = 0; i < numStages; ++i)
            {
                auto& buf = buffers[i];
                auto& pos = positions[i];
                const float delayed = buf[(size_t) pos];
                const float v = x + g * delayed;
                buf[(size_t) pos] = v;
                pos = (pos + 1) % lengths[i];
                x = delayed - g * v;
            }
            return x;
        }

        void reset() noexcept
        {
            for (auto& b : buffers) b.fill (0.0f);
            positions.fill (0);
        }
    };
    // Fractional delay line (linear interpolation) for the SUPER WIDE Haas section. Up to ~35 ms.
    struct DelayLine
    {
        std::vector<float> buffer;
        int mask = 0, writePos = 0;

        void prepare (double sr)
        {
            int size = 1;
            while (size < (int) (0.035 * sr) + 4)
                size <<= 1;
            buffer.assign ((size_t) size, 0.0f);
            mask = size - 1;
            writePos = 0;
        }

        // Writes x and returns the signal delaySamples ago (0 returns x itself).
        float process (float x, float delaySamples) noexcept
        {
            buffer[(size_t) writePos] = x;
            const float d = std::min (delaySamples, (float) (mask - 2));
            const int whole = (int) d;
            const float frac = d - (float) whole;
            const float a = buffer[(size_t) ((writePos - whole) & mask)];
            const float b = buffer[(size_t) ((writePos - whole - 1) & mask)];
            writePos = (writePos + 1) & mask;
            return a + (b - a) * frac;
        }

        void reset() noexcept
        {
            std::fill (buffer.begin(), buffer.end(), 0.0f);
            writePos = 0;
        }
    };

    // LR4 high-pass on its own, used to keep the Haas delays out of the low end.
    struct HighPass4
    {
        Biquad stages[2];

        void set (double sr, double f) noexcept
        {
            for (auto& b : stages) b.highpass (sr, f, 0.7071067811865476);
        }

        float process (float x) noexcept { return stages[1].process (stages[0].process (x)); }
        void reset() noexcept { for (auto& b : stages) b.reset(); }
    };
}
