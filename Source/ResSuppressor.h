#pragma once
#include <juce_dsp/juce_dsp.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <vector>

// =============================================================================
//  Resonance suppressor (Lab 2). One STFT stage after the EQ.
//
//  Each frame (1024-point FFT, hop 256, sqrt-Hann in and out), per detected channel:
//    level[k]  = 10·log10(power), averaged over ~30 ms and 3 bins (noise must not look like a resonance)
//    ref[k]    = mean of level over ±WIDTH/2 octaves (twice; the second pass clips peaks so they do not lift their own reference)
//    over[k]   = level − ref − (THRESHOLD + node curve)          the node curve: five points, as an EQ
//    target[k] = clamp(over, 0, DEPTH) · range weight            only inside LOW..HIGH (1/6-octave soft edges)
//    gain[k]   follows target with ATTACK / RELEASE, then a 3-bin smoothing across frequency
//  Channels (MODE): 0 Stereo — L/R linked, one gain for both (the image is kept)
//                   1 Mid    — only the Mid is detected and processed (the Side passes, delayed)
//                   2 Side   — only the Side
//                   3 M/S    — Mid and Side each detect and process on their own
//  SIDECHAIN: the levels come from the sidechain input (in the same channel layout) instead of the signal itself;
//             the gains are still applied to the signal. "Turn this down where the other track is loud".
//  Delta outputs X · (1 − g): only what is being removed.  Latency: 1024 samples.
// =============================================================================
class ResSuppressor
{
public:
    static constexpr int order = 10, N = 1 << order, H = N / 4, K = N / 2 + 1;
    static constexpr int kDisp = 96;                              // display points, log-spaced 20 Hz .. 20 kHz
    std::array<std::atomic<float>, kDisp> dispDb {};              // current reduction (dB, ≥ 0) for the graph (the larger of the channels)
    std::array<std::atomic<float>, kDisp> dispEx {};              // how far each point sticks out of its surroundings (dB)
    struct Nodes { std::array<float, 5> f { 100, 300, 1000, 3000, 10000 }, g {}; bool operator!= (const Nodes& o) const { return f != o.f || g != o.g; } };
    // the threshold offset at f: straight lines in log frequency between the nodes (sorted by frequency), flat outside
    static float nodeOffset (const Nodes& n, double f)
    {
        std::array<int, 5> o { 0, 1, 2, 3, 4 };
        std::sort (o.begin(), o.end(), [&] (int a, int b) { return n.f[(size_t) a] < n.f[(size_t) b]; });
        if (f <= n.f[(size_t) o[0]]) return n.g[(size_t) o[0]];
        for (int i = 0; i < 4; ++i)
        {
            const double f0 = n.f[(size_t) o[(size_t) i]], f1 = n.f[(size_t) o[(size_t) i + 1]];
            if (f <= f1)
            {
                const double t = f1 > f0 ? std::log (f / f0) / std::log (f1 / f0) : 1.0;
                return (float) (n.g[(size_t) o[(size_t) i]] + t * (n.g[(size_t) o[(size_t) i + 1]] - n.g[(size_t) o[(size_t) i]]));
            }
        }
        return n.g[(size_t) o[4]];
    }
    void setNodes (const Nodes& n) { if (n != nodes) { nodes = n; dirty = true; } }

    void prepare (double sampleRate)
    {
        fs = sampleRate;
        for (int i = 0; i < N; ++i) win[(size_t) i] = (float) std::sqrt (0.5 - 0.5 * std::cos (2.0 * juce::MathConstants<double>::pi * i / N));
        reset();
        dirty = true;
    }
    void reset()
    {
        for (int c = 0; c < 2; ++c)
        {
            std::fill (ring[(size_t) c].begin(), ring[(size_t) c].end(), 0.0f); std::fill (ringD[(size_t) c].begin(), ringD[(size_t) c].end(), 0.0f);
            std::fill (acc[(size_t) c].begin(), acc[(size_t) c].end(), 0.0f); std::fill (outq[(size_t) c].begin(), outq[(size_t) c].end(), 0.0f);
            std::fill (red[(size_t) c].begin(), red[(size_t) c].end(), 0.0f); std::fill (pw[(size_t) c].begin(), pw[(size_t) c].end(), 0.0f);
            std::fill (gsm[(size_t) c].begin(), gsm[(size_t) c].end(), 0.0f);
        }
        w = 0; hop = 0; oq = 0;
        for (auto& d : dispDb) d = 0.0f;
        for (auto& d : dispEx) d = -60.0f;
    }
    void setParams (float depthDb, float thrDb, float loHz, float hiHz, float selPct, float attMs, float relMs, bool deltaOut, int chMode, bool scDetect)
    {
        if (loHz != lo || hiHz != hi || selPct != sel) dirty = true;
        if (chMode != mode || scDetect != useSc) { mode = chMode; useSc = scDetect; reset(); }
        depth = depthDb; thr = thrDb; lo = loHz; hi = hiHz; sel = selPct; delta = deltaOut;
        const double fr = fs / H;                                  // frames per second
        aAtt = (float) (1.0 - std::exp (-1.0 / (fr * std::max (0.5, (double) attMs) * 0.001)));
        aRel = (float) (1.0 - std::exp (-1.0 / (fr * std::max (1.0, (double) relMs) * 0.001)));
        aDet = (float) (1.0 - std::exp (-1.0 / (fr * 0.03)));
    }
    // reference width in octaves for SELECT 0..100 %: 2 octaves (broad bumps count) .. 1/12 octave (only needle-sharp peaks)
    static double widthOct (float selPct) { return std::pow (2.0, juce::jmap ((double) selPct, 0.0, 100.0, 1.0, std::log2 (1.0 / 12.0))); }
    static int latency() { return N; }

    // l / r in and out; scL / scR: the sidechain (nullptr = none; then the signal is its own detector)
    void process (float* l, float* r, int n, const float* scL = nullptr, const float* scR = nullptr)
    {
        if (dirty) rebuild();
        const bool ms = mode != 0, sc = useSc && scL != nullptr;
        haveSc = sc;
        for (int i = 0; i < n; ++i)
        {
            float a = l[i], b = r[i];
            if (ms) { const float m = 0.5f * (a + b), s = 0.5f * (a - b); a = m; b = s; }
            ring[0][(size_t) w] = a; ring[1][(size_t) w] = b;
            if (sc)
            {
                float x = scL[i], y = scR != nullptr ? scR[i] : scL[i];
                if (ms) { const float m = 0.5f * (x + y), s = 0.5f * (x - y); x = m; y = s; }
                ringD[0][(size_t) w] = x; ringD[1][(size_t) w] = y;
            }
            const float oa = outq[0][(size_t) oq], ob = outq[1][(size_t) oq]; ++oq;
            if (ms) { l[i] = oa + ob; r[i] = oa - ob; } else { l[i] = oa; r[i] = ob; }
            w = (w + 1) & (N - 1);
            if (++hop == H) { hop = 0; oq = 0; frame(); }
        }
    }

private:
    double fs = 48000;
    float depth = 6, thr = 6, lo = 300, hi = 10000, sel = 50, aAtt = 0.5f, aRel = 0.1f, aDet = 0.3f;
    bool delta = false, dirty = true, useSc = false, haveSc = false; int mode = 0;
    juce::dsp::FFT fft { order };
    std::array<float, N> win {};
    std::array<std::array<float, N>, 2> ring {}, ringD {}, acc {};
    std::array<std::array<float, H>, 2> outq {};
    std::array<std::array<float, 2 * N>, 2> X {}, XD {};
    std::array<std::array<float, K>, 2> red {}, pw {}, gsm {};
    std::array<float, K> lev {}, ref {}, tmp {}, wgt {}, thrK {}, exK {};
    std::array<int, K> a0 {}, a1 {};
    std::array<double, K + 1> cs {};
    std::array<int, kDisp + 1> dispBin {};
    Nodes nodes;
    int w = 0, hop = 0, oq = 0;

    void rebuild()
    {
        dirty = false;
        const double half = widthOct (sel) * 0.5;
        for (int k = 0; k < K; ++k)
        {
            const double f = std::max (1.0, k * fs / N);
            a0[(size_t) k] = juce::jlimit (1, K - 1, std::min (k - 2, (int) std::floor (f * std::pow (2.0, -half) * N / fs)));
            a1[(size_t) k] = juce::jlimit (1, K - 1, std::max (k + 2, (int) std::ceil (f * std::pow (2.0, half) * N / fs)));
            const double e = 1.0 / 6.0;
            const double dl = std::log2 (f / std::max (20.0, (double) lo)), dh = std::log2 (std::min (20000.0, (double) hi) / f);
            auto edge = [e] (double d) { return d >= 0 ? 1.0 : d <= -e ? 0.0 : 0.5 + 0.5 * std::cos (juce::MathConstants<double>::pi * d / e); };
            wgt[(size_t) k] = (float) (edge (dl) * edge (dh));
            thrK[(size_t) k] = nodeOffset (nodes, f);
        }
        for (int d = 0; d <= kDisp; ++d)
            dispBin[(size_t) d] = juce::jlimit (1, K - 1, (int) std::round (20.0 * std::pow (1000.0, (double) d / kDisp) * N / fs));
    }
    void meanOf (const std::array<float, K>& v)
    {
        cs[0] = 0; for (int k = 0; k < K; ++k) cs[(size_t) k + 1] = cs[(size_t) k] + v[(size_t) k];
        for (int k = 0; k < K; ++k) ref[(size_t) k] = (float) ((cs[(size_t) a1[(size_t) k] + 1] - cs[(size_t) a0[(size_t) k]]) / (a1[(size_t) k] - a0[(size_t) k] + 1));
    }
    static float binPow (const std::array<float, 2 * N>& x, int k) { return x[(size_t) (2 * k)] * x[(size_t) (2 * k)] + x[(size_t) (2 * k + 1)] * x[(size_t) (2 * k + 1)]; }
    // one detector: power per bin → smoothed reduction in gsm[c] (dB); "sticks out by" into exK (the larger of the channels)
    void detect (int c, bool linked)
    {
        const auto& D = haveSc ? XD : X;
        auto& p = pw[(size_t) c];
        for (int k = 0; k < K; ++k)
        {
            const float v = linked ? binPow (D[0], k) + binPow (D[1], k) : binPow (D[(size_t) c], k);
            p[(size_t) k] += (v - p[(size_t) k]) * aDet;
        }
        for (int k = 0; k < K; ++k)
            lev[(size_t) k] = 10.0f * std::log10 (0.25f * p[(size_t) std::max (0, k - 1)] + 0.5f * p[(size_t) k] + 0.25f * p[(size_t) std::min (K - 1, k + 1)] + 1e-12f);
        meanOf (lev);
        for (int k = 0; k < K; ++k) tmp[(size_t) k] = std::min (lev[(size_t) k], ref[(size_t) k] + 3.0f);
        meanOf (tmp);
        const float quiet = 20.0f * std::log10 ((float) N * 0.25f) - 100.0f;     // about −100 dBFS per bin: leave silence alone
        auto& rd = red[(size_t) c];
        for (int k = 0; k < K; ++k)
        {
            const bool loud = lev[(size_t) k] > quiet;
            const float t = loud ? juce::jlimit (0.0f, depth, lev[(size_t) k] - ref[(size_t) k] - thr - thrK[(size_t) k]) * wgt[(size_t) k] : 0.0f;
            rd[(size_t) k] += (t - rd[(size_t) k]) * (t > rd[(size_t) k] ? aAtt : aRel);
            if (loud) exK[(size_t) k] = std::max (exK[(size_t) k], lev[(size_t) k] - ref[(size_t) k]);
        }
        for (int k = 0; k < K; ++k)
            gsm[(size_t) c][(size_t) k] = 0.25f * rd[(size_t) std::max (0, k - 1)] + 0.5f * rd[(size_t) k] + 0.25f * rd[(size_t) std::min (K - 1, k + 1)];
    }
    void frame()
    {
        for (int c = 0; c < 2; ++c)
        {
            auto& x = X[(size_t) c];
            for (int i = 0; i < N; ++i) x[(size_t) i] = ring[(size_t) c][(size_t) ((w + i) & (N - 1))] * win[(size_t) i];
            std::fill (x.begin() + N, x.end(), 0.0f);
            fft.performRealOnlyForwardTransform (x.data(), true);
            if (haveSc)
            {
                auto& y = XD[(size_t) c];
                for (int i = 0; i < N; ++i) y[(size_t) i] = ringD[(size_t) c][(size_t) ((w + i) & (N - 1))] * win[(size_t) i];
                std::fill (y.begin() + N, y.end(), 0.0f);
                fft.performRealOnlyForwardTransform (y.data(), true);
            }
        }
        std::fill (exK.begin(), exK.end(), -60.0f);
        // which channels are detected, and which gain each channel gets
        bool proc[2] = { true, true }; int src[2] = { 0, 1 };
        switch (mode)
        {
            case 0:  detect (0, true); src[1] = 0; break;                                   // linked
            case 1:  detect (0, false); proc[1] = false; break;                             // Mid
            case 2:  detect (1, false); proc[0] = false; break;                             // Side
            default: detect (0, false); detect (1, false); break;                           // M/S
        }
        for (int c = 0; c < 2; ++c)
        {
            auto& x = X[(size_t) c];
            for (int k = 0; k < K; ++k)
            {
                float g = proc[c] ? std::pow (10.0f, -gsm[(size_t) src[c]][(size_t) k] / 20.0f) : 1.0f;
                if (delta) g = 1.0f - g;
                x[(size_t) (2 * k)] *= g; x[(size_t) (2 * k + 1)] *= g;
            }
        }
        for (int d = 0; d < kDisp; ++d)
        {
            float m = 0, ex = -60.0f;
            for (int k = dispBin[(size_t) d]; k <= std::max (dispBin[(size_t) d], dispBin[(size_t) d + 1] - 1); ++k)
            {
                for (int c = 0; c < 2; ++c) if (proc[c]) m = std::max (m, gsm[(size_t) src[c]][(size_t) k]);
                ex = std::max (ex, exK[(size_t) k]);
            }
            dispDb[(size_t) d] = m; dispEx[(size_t) d] = ex;
        }
        // back to time, window again, overlap-add (sqrt-Hann² at 75 % overlap sums to 2)
        for (int c = 0; c < 2; ++c)
        {
            auto& x = X[(size_t) c]; auto& a = acc[(size_t) c];
            fft.performRealOnlyInverseTransform (x.data());
            for (int i = 0; i < N; ++i) a[(size_t) i] += x[(size_t) i] * win[(size_t) i] * 0.5f;
            std::copy (a.begin(), a.begin() + H, outq[(size_t) c].begin());
            std::copy (a.begin() + H, a.end(), a.begin());
            std::fill (a.end() - H, a.end(), 0.0f);
        }
    }
};
