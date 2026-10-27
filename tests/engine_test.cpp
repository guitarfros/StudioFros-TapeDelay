// Standalone sanity tests for the tape engine (no JUCE needed).
// Build:  g++ -std=c++17 -O2 -I../Source engine_test.cpp -o engine_test && ./engine_test
#include "TapeEngine.h"
#include <cstdio>
#include <cstdlib>

static int failures = 0;
#define CHECK(cond, msg) do { if (!(cond)) { std::printf("FAIL: %s\n", msg); ++failures; } else std::printf("ok:   %s\n", msg); } while (0)

static int peakIndex (const std::vector<float>& v, int from, int to)
{
    int best = from; float m = 0.f;
    for (int i = from; i < to && i < (int) v.size(); ++i)
        if (std::abs (v[(size_t) i]) > m) { m = std::abs (v[(size_t) i]); best = i; }
    return best;
}

int main()
{
    const double sr = 48000.0;
    const int N = 48000 * 2;

    // 1) timing of heads 1,2,3
    {
        sf::TapeEngine e; e.prepare (sr);
        sf::EchoParams p; p.baseDelaySamples = 4800.f; p.feedback = 0.f; p.headMask = 7; p.stereo = false;
        std::vector<float> in ((size_t) N, 0.f), out ((size_t) N), outR ((size_t) N);
        in[0] = 1.f;
        e.process (in.data(), in.data(), out.data(), outR.data(), N, p);
        const int a = peakIndex (out, 100, 7000), b = peakIndex (out, 7000, 12000), c = peakIndex (out, 12000, 20000);
        std::printf("   head peaks at %d %d %d\n", a, b, c);
        CHECK (std::abs (a - 4800) <= 12, "head 1 at 1x base delay");
        CHECK (std::abs (b - 9600) <= 12, "head 2 at 2x base delay");
        CHECK (std::abs (c - 14400) <= 12, "head 3 at 3x base delay");
    }

    // 2) feedback repeats decay
    {
        sf::TapeEngine e; e.prepare (sr);
        sf::EchoParams p; p.baseDelaySamples = 4800.f; p.feedback = 0.5f; p.headMask = 1; p.stereo = false;
        std::vector<float> in ((size_t) N, 0.f), out ((size_t) N), outR ((size_t) N);
        in[0] = 1.f;
        e.process (in.data(), in.data(), out.data(), outR.data(), N, p);
        float e1 = std::abs (out[(size_t) peakIndex (out, 4000, 5600)]);
        float e2 = std::abs (out[(size_t) peakIndex (out, 9000, 10600)]);
        float e3 = std::abs (out[(size_t) peakIndex (out, 13800, 15400)]);
        std::printf("   repeats %.4f %.4f %.4f\n", e1, e2, e3);
        CHECK (e2 < e1 && e3 < e2 && e2 > 0.02f, "repeats decay with intensity 0.5");
    }

    // 3) runaway feedback stays bounded and finite for every saturation setting
    for (float sat : { 0.f, 0.5f, 1.f })
    {
        sf::TapeEngine e; e.prepare (sr);
        sf::EchoParams p; p.baseDelaySamples = 2400.f; p.feedback = 1.12f; p.headMask = 7; p.stereo = true;
        p.saturation = sat; p.age = 0.5f; p.wow = 1.f;
        const int blk = 512, blocks = (int) (sr * 20 / blk);
        std::vector<float> in ((size_t) blk, 0.f), oL ((size_t) blk), oR ((size_t) blk);
        float mx = 0.f; bool finite = true;
        for (int b = 0; b < blocks; ++b)
        {
            for (int i = 0; i < blk; ++i) in[(size_t) i] = (b < 5) ? 0.5f * std::sin (0.05f * (float) (b * blk + i)) : 0.f;
            e.process (in.data(), in.data(), oL.data(), oR.data(), blk, p);
            for (int i = 0; i < blk; ++i)
            {
                if (! std::isfinite (oL[(size_t) i]) || ! std::isfinite (oR[(size_t) i])) finite = false;
                mx = std::max (mx, std::max (std::abs (oL[(size_t) i]), std::abs (oR[(size_t) i])));
            }
        }
        std::printf("   sat=%.1f  max |out| = %.3f\n", sat, mx);
        CHECK (finite && mx < 4.f, "self-oscillation is finite and bounded");
    }

    // 4) head mask 0 -> silence (no hiss, no echoes)
    {
        sf::TapeEngine e; e.prepare (sr);
        sf::EchoParams p; p.headMask = 0; p.age = 1.f;
        std::vector<float> in (4096, 0.3f), oL (4096), oR (4096);
        e.process (in.data(), in.data(), oL.data(), oR.data(), 4096, p);
        float mx = 0.f; for (float v : oL) mx = std::max (mx, std::abs (v));
        CHECK (mx == 0.f, "no heads selected -> echo output is silent");
    }

    // 5) tone stack
    {
        sf::ToneStack t; t.prepare (sr); t.set (6.f, -6.f);
        std::vector<float> l (2048, 0.1f), r (2048, 0.1f);
        t.process (l.data(), r.data(), 2048);
        CHECK (std::isfinite (l.back()) && std::abs (l.back()) < 1.f, "tone stack finite");
    }

    std::printf (failures == 0 ? "\nALL TESTS PASSED\n" : "\n%d TEST(S) FAILED\n", failures);
    return failures;
}
