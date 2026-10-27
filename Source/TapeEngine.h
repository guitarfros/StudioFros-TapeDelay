// StudioFros - Tape Delay
// Copyright (C) 2026 GuitarFros
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Pure C++17 tape-echo engine (no JUCE dependency, so it can be unit-tested
// on its own).  Models a three-head tape echo:
//   * one endless "tape" (circular buffer), three playback heads at 1x/2x/3x
//     the base delay, selectable through a head mask
//   * tape speed inertia (the base delay glides instead of jumping)
//   * wow / flutter / slow drift (modulates the tape speed)
//   * tape saturation on the record side, progressive HF loss on every
//     repeat ("tape age"), hiss and random dropouts
//   * feedback ("intensity") that is allowed to run into self-oscillation but
//     is always bounded by the tape saturation
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace sf
{
constexpr float kPi = 3.14159265358979323846f;
constexpr float kTwoPi = 2.0f * kPi;

//==============================================================================
/** Transposed direct form II biquad, up to two channels. */
struct Biquad
{
    float b0 = 1.f, b1 = 0.f, b2 = 0.f, a1 = 0.f, a2 = 0.f;
    float z1[2] = { 0.f, 0.f }, z2[2] = { 0.f, 0.f };

    void reset() noexcept { z1[0] = z1[1] = z2[0] = z2[1] = 0.f; }

    float process (int ch, float x) noexcept
    {
        const float y = b0 * x + z1[ch];
        z1[ch] = b1 * x - a1 * y + z2[ch];
        z2[ch] = b2 * x - a2 * y;
        return y;
    }

    void setIdentity() noexcept { b0 = 1.f; b1 = b2 = a1 = a2 = 0.f; }

    // RBJ cookbook shelves (slope S = 1)
    void setShelf (bool high, double sampleRate, double freq, double gainDb) noexcept
    {
        if (std::abs (gainDb) < 1.0e-3) { setIdentity(); return; }

        const double A = std::pow (10.0, gainDb / 40.0);
        const double w0 = 2.0 * 3.14159265358979323846 * freq / sampleRate;
        const double cw = std::cos (w0), sw = std::sin (w0);
        const double alpha = sw * 0.5 * std::sqrt (2.0);
        const double tsa = 2.0 * std::sqrt (A) * alpha;

        double nb0, nb1, nb2, na0, na1, na2;
        if (! high)
        {
            nb0 = A * ((A + 1) - (A - 1) * cw + tsa);
            nb1 = 2 * A * ((A - 1) - (A + 1) * cw);
            nb2 = A * ((A + 1) - (A - 1) * cw - tsa);
            na0 = (A + 1) + (A - 1) * cw + tsa;
            na1 = -2 * ((A - 1) + (A + 1) * cw);
            na2 = (A + 1) + (A - 1) * cw - tsa;
        }
        else
        {
            nb0 = A * ((A + 1) + (A - 1) * cw + tsa);
            nb1 = -2 * A * ((A - 1) + (A + 1) * cw);
            nb2 = A * ((A + 1) + (A - 1) * cw - tsa);
            na0 = (A + 1) - (A - 1) * cw + tsa;
            na1 = 2 * ((A - 1) - (A + 1) * cw);
            na2 = (A + 1) - (A - 1) * cw - tsa;
        }

        b0 = (float) (nb0 / na0);
        b1 = (float) (nb1 / na0);
        b2 = (float) (nb2 / na0);
        a1 = (float) (na1 / na0);
        a2 = (float) (na2 / na0);
    }
};

//==============================================================================
/** Bass / treble shelving pair applied to the wet signal. */
class ToneStack
{
public:
    void prepare (double sr) { sampleRate = sr; lastBass = lastTreble = 1000.f; reset(); set (0.f, 0.f); }
    void reset() noexcept { low.reset(); high.reset(); }

    void set (float bassDb, float trebleDb)
    {
        if (bassDb == lastBass && trebleDb == lastTreble) return;
        lastBass = bassDb; lastTreble = trebleDb;
        low.setShelf (false, sampleRate, 200.0, bassDb);
        high.setShelf (true, sampleRate, 3200.0, trebleDb);
    }

    void process (float* l, float* r, int n) noexcept
    {
        for (int i = 0; i < n; ++i)
        {
            l[i] = high.process (0, low.process (0, l[i]));
            r[i] = high.process (1, low.process (1, r[i]));
        }
    }

private:
    Biquad low, high;
    double sampleRate = 44100.0;
    float lastBass = 1000.f, lastTreble = 1000.f;
};

//==============================================================================
struct EchoParams
{
    float baseDelaySamples = 4800.f;  // delay of head 1 (head n = n * base)
    float feedback = 0.3f;            // 0 .. ~1.12 (>1 self-oscillates)
    float wow = 0.f;                  // 0..1  wow & flutter amount
    float saturation = 0.f;           // 0..1
    float age = 0.f;                  // 0..1  HF loss, hiss, dropouts
    int   headMask = 1;               // bit0 = head 1, bit1 = head 2, bit2 = head 3
    bool  stereo = true;              // pan the heads left / centre / right
};

//==============================================================================
class TapeEngine
{
public:
    void prepare (double sr)
    {
        sampleRate = (float) sr;

        int size = 1;
        const double needed = 8.0 * sr + 16.0;
        while ((double) size < needed) size <<= 1;
        buffer.assign ((size_t) size, 0.f);
        mask = size - 1;

        wowInc  = kTwoPi * 0.55f / sampleRate;
        fl1Inc  = kTwoPi * 6.9f  / sampleRate;
        fl2Inc  = kTwoPi * 11.3f / sampleRate;
        driftCoef = 1.f - std::exp (-kTwoPi * 0.35f / sampleRate);
        driftNorm = 1.f / (0.577f * std::sqrt (driftCoef / (2.f - driftCoef)));
        baseCoef  = 1.f - std::exp (-1.f / (0.12f * sampleRate));   // tape speed inertia
        dropCoef  = 1.f - std::exp (-1.f / (0.005f * sampleRate));
        hpR = std::exp (-kTwoPi * 35.f / sampleRate);

        reset();
    }

    void reset()
    {
        std::fill (buffer.begin(), buffer.end(), 0.f);
        writePos = 0;
        initialised = false;
        lp1 = lp2 = 0.f;
        hpX = hpY = 0.f;
        drift = 0.f;
        dropRemain = 0; dropGain = 1.f; dropDepth = 0.f;
        noisePrev = 0.f;
        if (rng == 0) rng = 0x9E3779B9u;
    }

    /** Processes n samples.  inL/inR are the (already gained & gated) signals
        sent to the tape.  echoL/echoR receive the *echo only* (no dry). */
    void process (const float* inL, const float* inR, float* echoL, float* echoR,
                  int n, const EchoParams& p) noexcept
    {
        if (buffer.empty()) return;

        const float maxBase = (float) (mask - 16) / 3.2f;
        const float targetBase = std::min (std::max (p.baseDelaySamples, 8.f), maxBase);
        if (! initialised) { base = targetBase; initialised = true; }

        const int mask_ = p.headMask & 7;
        int count = 0;
        for (int h = 0; h < 3; ++h) count += (mask_ >> h) & 1;
        const float norm = count > 0 ? 1.f / std::sqrt ((float) count) : 0.f;

        // head levels and pan gains
        static const float headLevel[3] = { 1.0f, 0.9f, 0.8f };
        static const float headPan[3]   = { -0.6f, 0.f, 0.6f };
        float gL[3], gR[3];
        for (int h = 0; h < 3; ++h)
        {
            if (p.stereo)
            {
                const float th = (headPan[h] + 1.f) * kPi * 0.25f;
                gL[h] = std::cos (th) * 1.41421356f * headLevel[h] * norm;
                gR[h] = std::sin (th) * 1.41421356f * headLevel[h] * norm;
            }
            else
            {
                gL[h] = gR[h] = headLevel[h] * norm;
            }
        }
        // mono feedback sum weights
        float gFb[3];
        for (int h = 0; h < 3; ++h) gFb[h] = headLevel[h] * norm;

        const float age = std::min (std::max (p.age, 0.f), 1.f);
        const float sat = std::min (std::max (p.saturation, 0.f), 1.f);
        const float wowAmt = std::min (std::max (p.wow, 0.f), 1.f);

        const float drive = 1.f + 4.f * sat;
        const float invSqrtDrive = 1.f / std::sqrt (drive);
        const float bias = 0.1f * sat;
        const float biasOffset = std::tanh (drive * bias);

        float fc = 16000.f * std::pow (2800.f / 16000.f, age);
        fc = std::min (fc, 0.45f * sampleRate);
        const float lpA = 1.f - std::exp (-kTwoPi * fc / sampleRate);

        const float hissAmp = age * age * 0.0035f;
        const float dropProb = (age * age * 0.8f) / sampleRate;

        const float fbCoef = p.feedback * invSqrtDrive;   // keeps loop gain ~ feedback

        for (int i = 0; i < n; ++i)
        {
            base += (targetBase - base) * baseCoef;

            // --- tape speed modulation
            wowPh += wowInc; if (wowPh > kTwoPi) wowPh -= kTwoPi;
            fl1Ph += fl1Inc; if (fl1Ph > kTwoPi) fl1Ph -= kTwoPi;
            fl2Ph += fl2Inc; if (fl2Ph > kTwoPi) fl2Ph -= kTwoPi;
            drift += (nextRandomBipolar() - drift) * driftCoef;
            const float dr = std::min (std::max (drift * driftNorm, -3.f), 3.f);

            const float lfo = 0.010f * std::sin (wowPh)
                            + 0.0022f * std::sin (fl1Ph)
                            + 0.0012f * std::sin (fl2Ph)
                            + 0.0035f * dr;
            const float speed = 1.f + wowAmt * lfo;

            // --- dropouts (random level dips)
            float dropTarget = 1.f;
            if (dropRemain > 0)
            {
                --dropRemain;
                dropTarget = 1.f - dropDepth;
            }
            else if (age > 0.01f && nextRandom01() < dropProb)
            {
                dropRemain = (int) (sampleRate * (0.015f + 0.06f * nextRandom01()));
                dropDepth = 0.35f + 0.5f * nextRandom01();
            }
            dropGain += (dropTarget - dropGain) * dropCoef;

            // --- playback heads
            float oL = 0.f, oR = 0.f, fb = 0.f;
            for (int h = 0; h < 3; ++h)
            {
                if (((mask_ >> h) & 1) == 0) continue;
                const float d = std::min (std::max (base * (float) (h + 1) * speed, 4.f), (float) (mask - 8));
                const float s = readHermite (d);
                oL += s * gL[h];
                oR += s * gR[h];
                fb += s * gFb[h];
            }

            if (count > 0)
            {
                const float noise = nextRandomBipolar();
                const float hiss = (noise - noisePrev) * 0.5f * hissAmp;   // highpassed white
                noisePrev = noise;
                oL = oL * dropGain + hiss;
                oR = oR * dropGain + hiss;
                fb *= dropGain;
            }
            echoL[i] = oL;
            echoR[i] = oR;

            // --- record side
            const float inMono = 0.5f * (inL[i] + inR[i]);

            // DC / sub blocker in the feedback path
            const float hpOut = hpR * (hpY + fb - hpX);
            hpX = fb; hpY = hpOut;

            float w = inMono + fbCoef * hpOut;
            w = (std::tanh (drive * (w + bias)) - biasOffset) * invSqrtDrive;

            lp1 += lpA * (w - lp1);
            lp2 += lpA * (lp1 - lp2);

            buffer[(size_t) writePos] = flush (lp2);
            writePos = (writePos + 1) & mask;
        }
    }

private:
    static float flush (float v) noexcept { return std::abs (v) < 1.0e-20f ? 0.f : v; }

    float readHermite (float d) const noexcept
    {
        const int di = (int) d;
        const float mu = d - (float) di;
        const float pm1 = buffer[(size_t) ((writePos - di + 1) & mask)];
        const float p0  = buffer[(size_t) ((writePos - di)     & mask)];
        const float p1  = buffer[(size_t) ((writePos - di - 1) & mask)];
        const float p2  = buffer[(size_t) ((writePos - di - 2) & mask)];

        const float c0 = p0;
        const float c1 = 0.5f * (p1 - pm1);
        const float c2 = pm1 - 2.5f * p0 + 2.f * p1 - 0.5f * p2;
        const float c3 = 0.5f * (p2 - pm1) + 1.5f * (p0 - p1);
        return ((c3 * mu + c2) * mu + c1) * mu + c0;
    }

    uint32_t nextU32() noexcept
    {
        rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
        return rng;
    }
    float nextRandom01() noexcept { return (float) (nextU32() >> 8) * (1.f / 16777216.f); }
    float nextRandomBipolar() noexcept { return nextRandom01() * 2.f - 1.f; }

    std::vector<float> buffer;
    int mask = 0, writePos = 0;
    float sampleRate = 44100.f;

    float base = 4800.f, baseCoef = 0.001f;
    bool initialised = false;

    float wowPh = 0.f, fl1Ph = 0.f, fl2Ph = 0.f, wowInc = 0.f, fl1Inc = 0.f, fl2Inc = 0.f;
    float drift = 0.f, driftCoef = 0.f, driftNorm = 1.f;

    float lp1 = 0.f, lp2 = 0.f, hpX = 0.f, hpY = 0.f, hpR = 0.995f;

    int dropRemain = 0; float dropGain = 1.f, dropDepth = 0.f, dropCoef = 0.01f;
    float noisePrev = 0.f;
    uint32_t rng = 0x9E3779B9u;
};

} // namespace sf
