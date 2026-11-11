#pragma once
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>
#include <cmath>
#include <vector>

// =============================================================================
//  Resonance suppressor (Lab 2). One STFT stage after the EQ, stereo-linked.
//
//  Each frame (1024-point FFT, hop 256, sqrt-Hann in and out):
//    level[k]  = 10·log10(|L|² + |R|²)                      linked, so the stereo image is kept; averaged over ~30 ms and
//                                                            3 bins so that the random ups and downs of noise are not taken for peaks
//    ref[k]    = mean of level over ±WIDTH/2 octaves         the "smooth" spectrum around the bin (computed twice;
//                                                            the second pass clips peaks so they do not lift their own reference)
//    over[k]   = level − ref − THRESHOLD                     how far the bin sticks out
//    target[k] = clamp(over, 0, DEPTH) · range weight        only inside LOW..HIGH (1/6-octave soft edges)
//    gain[k]   follows target with ATTACK / RELEASE, then a 3-bin smoothing across frequency
//  Delta outputs X · (1 − g): only what is being removed.
//  Latency: 1024 samples (the window length).
// =============================================================================
class ResSuppressor
{
public:
    static constexpr int order = 10, N = 1 << order, H = N / 4, K = N / 2 + 1;
    static constexpr int kDisp = 96;                              // display points, log-spaced 20 Hz .. 20 kHz
    std::array<std::atomic<float>, kDisp> dispDb {};              // current reduction (dB, ≥ 0) for the graph

    void prepare (double sampleRate)
    {
        fs = sampleRate;
        for (int i = 0; i < N; ++i) win[(size_t) i] = (float) std::sqrt (0.5 - 0.5 * std::cos (2.0 * juce::MathConstants<double>::pi * i / N));
        reset();
        dirty = true;
    }
    void reset()
    {
        for (int c = 0; c < 2; ++c) { std::fill (ring[c].begin(), ring[c].end(), 0.0f); std::fill (acc[c].begin(), acc[c].end(), 0.0f); std::fill (outq[c].begin(), outq[c].end(), 0.0f); }
        std::fill (red.begin(), red.end(), 0.0f); std::fill (pw.begin(), pw.end(), 0.0f);
        w = 0; hop = 0; oq = 0;
        for (auto& d : dispDb) d = 0.0f;
    }
    void setParams (float depthDb, float thrDb, float loHz, float hiHz, float selPct, float attMs, float relMs, bool deltaOut)
    {
        if (depthDb != depth || thrDb != thr || loHz != lo || hiHz != hi || selPct != sel) dirty = true;
        depth = depthDb; thr = thrDb; lo = loHz; hi = hiHz; sel = selPct; delta = deltaOut;
        const double fr = fs / H;                                  // frames per second
        aAtt = (float) (1.0 - std::exp (-1.0 / (fr * std::max (0.5, (double) attMs) * 0.001)));
        aRel = (float) (1.0 - std::exp (-1.0 / (fr * std::max (1.0, (double) relMs) * 0.001)));
        aDet = (float) (1.0 - std::exp (-1.0 / (fr * 0.03)));
    }
    // reference width in octaves for SELECT 0..100 %: 2 octaves (broad bumps count) .. 1/12 octave (only needle-sharp peaks)
    static double widthOct (float selPct) { return std::pow (2.0, juce::jmap ((double) selPct, 0.0, 100.0, 1.0, std::log2 (1.0 / 12.0))); }
    static int latency() { return N; }

    void process (float* l, float* r, int n)
    {
        if (dirty) rebuild();
        for (int i = 0; i < n; ++i)
        {
            ring[0][(size_t) w] = l[i]; ring[1][(size_t) w] = r[i];
            l[i] = outq[0][(size_t) oq]; r[i] = outq[1][(size_t) oq]; ++oq;
            w = (w + 1) & (N - 1);
            if (++hop == H) { hop = 0; oq = 0; frame(); }
        }
    }

private:
    double fs = 48000;
    float depth = 6, thr = 6, lo = 300, hi = 10000, sel = 50, aAtt = 0.5f, aRel = 0.1f, aDet = 0.3f; bool delta = false, dirty = true;
    juce::dsp::FFT fft { order };
    std::array<float, N> win {};
    std::array<std::array<float, N>, 2> ring {}, acc {};
    std::array<std::array<float, H>, 2> outq {};
    std::array<std::array<float, 2 * N>, 2> X {};
    std::array<float, K> red {}, pw {}, lev {}, ref {}, tmp {}, wgt {};
    std::array<int, K> a0 {}, a1 {};
    std::array<double, K + 1> cs {};
    std::array<int, kDisp + 1> dispBin {};
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
            // LOW..HIGH with 1/6-octave cosine edges
            const double e = 1.0 / 6.0;
            const double dl = std::log2 (f / std::max (20.0, (double) lo)), dh = std::log2 (std::min (20000.0, (double) hi) / f);
            auto edge = [e] (double d) { return d >= 0 ? 1.0 : d <= -e ? 0.0 : 0.5 + 0.5 * std::cos (juce::MathConstants<double>::pi * d / e); };
            wgt[(size_t) k] = (float) (edge (dl) * edge (dh));
        }
        for (int d = 0; d <= kDisp; ++d)
            dispBin[(size_t) d] = juce::jlimit (1, K - 1, (int) std::round (20.0 * std::pow (1000.0, (double) d / kDisp) * N / fs));
    }
    void meanOf (const std::array<float, K>& v)
    {
        cs[0] = 0; for (int k = 0; k < K; ++k) cs[(size_t) k + 1] = cs[(size_t) k] + v[(size_t) k];
        for (int k = 0; k < K; ++k) ref[(size_t) k] = (float) ((cs[(size_t) a1[(size_t) k] + 1] - cs[(size_t) a0[(size_t) k]]) / (a1[(size_t) k] - a0[(size_t) k] + 1));
    }
    void frame()
    {
        for (int c = 0; c < 2; ++c)
        {
            auto& x = X[(size_t) c];
            for (int i = 0; i < N; ++i) x[(size_t) i] = ring[(size_t) c][(size_t) ((w + i) & (N - 1))] * win[(size_t) i];
            std::fill (x.begin() + N, x.end(), 0.0f);
            fft.performRealOnlyForwardTransform (x.data(), true);
        }
        for (int k = 0; k < K; ++k)
        {
            const float p = X[0][(size_t) (2 * k)] * X[0][(size_t) (2 * k)] + X[0][(size_t) (2 * k + 1)] * X[0][(size_t) (2 * k + 1)]
                          + X[1][(size_t) (2 * k)] * X[1][(size_t) (2 * k)] + X[1][(size_t) (2 * k + 1)] * X[1][(size_t) (2 * k + 1)];
            pw[(size_t) k] += (p - pw[(size_t) k]) * aDet;        // ~30 ms power average: noise must not look like a resonance
        }
        for (int k = 0; k < K; ++k)
            lev[(size_t) k] = 10.0f * std::log10 (0.25f * pw[(size_t) std::max (0, k - 1)] + 0.5f * pw[(size_t) k] + 0.25f * pw[(size_t) std::min (K - 1, k + 1)] + 1e-12f);
        meanOf (lev);
        for (int k = 0; k < K; ++k) tmp[(size_t) k] = std::min (lev[(size_t) k], ref[(size_t) k] + 3.0f);   // peaks do not lift their own reference
        meanOf (tmp);
        const float quiet = 20.0f * std::log10 ((float) N * 0.25f) - 100.0f;     // about −100 dBFS per bin: leave silence alone
        for (int k = 0; k < K; ++k)
        {
            float t = lev[(size_t) k] > quiet ? juce::jlimit (0.0f, depth, lev[(size_t) k] - ref[(size_t) k] - thr) * wgt[(size_t) k] : 0.0f;
            auto& rd = red[(size_t) k];
            rd += (t - rd) * (t > rd ? aAtt : aRel);
        }
        for (int k = 0; k < K; ++k)
        {
            const float s = 0.25f * red[(size_t) std::max (0, k - 1)] + 0.5f * red[(size_t) k] + 0.25f * red[(size_t) std::min (K - 1, k + 1)];
            tmp[(size_t) k] = s;
            float g = std::pow (10.0f, -s / 20.0f);
            if (delta) g = 1.0f - g;
            for (int c = 0; c < 2; ++c) { X[(size_t) c][(size_t) (2 * k)] *= g; X[(size_t) c][(size_t) (2 * k + 1)] *= g; }
        }
        for (int d = 0; d < kDisp; ++d)
        {
            float m = 0; for (int k = dispBin[(size_t) d]; k <= std::max (dispBin[(size_t) d], dispBin[(size_t) d + 1] - 1); ++k) m = std::max (m, tmp[(size_t) k]);
            dispDb[(size_t) d] = m;
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
