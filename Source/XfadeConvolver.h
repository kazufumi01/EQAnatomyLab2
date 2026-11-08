#pragma once
#include <juce_dsp/juce_dsp.h>
#include <atomic>
#include <mutex>
#include <array>
#include <vector>

// =============================================================================
//  Stereo, zero-latency, uniformly partitioned convolver whose kernel can be
//  swapped without losing the input history.
//
//  juce::dsp::Convolution builds a fresh engine for every new IR. That engine
//  starts from silence, so a linear-phase kernel (main tap 4096+ samples in)
//  outputs almost nothing for its whole latency, and the 50 ms crossfade lands
//  on that hole: dragging a Lin band crackles and drops out.
//
//  Here every kernel reads the same frequency-domain input history. A new
//  kernel's overlap is rebuilt from that history when it is installed, so its
//  output is exact from the first sample, and a short crossfade only smooths
//  the change of filter. Kernels are built on a background thread and handed
//  over through a lock-free four-slot mailbox (no allocation or locks on the
//  audio thread).
// =============================================================================
class XfadeConvolver
{
public:
    static constexpr int maxIRLength = 65536;

    // not concurrent with process()
    void prepare (double sampleRate, int maxBlock)
    {
        const std::lock_guard<std::mutex> lk (buildMutex);
        B = std::max (64, juce::nextPowerOfTwo (std::max (1, maxBlock)));
        F = 2 * B; S = F + 1;
        const int order = juce::roundToInt (std::log2 ((double) F));
        fftAudio = std::make_unique<juce::dsp::FFT> (order);
        fftBuild = std::make_unique<juce::dsp::FFT> (order);
        maxSegs = maxIRLength / B + 1;
        numIn = maxSegs + 1;
        for (auto& ir : pool) { ir.numSegs = 0; ir.cross = false; for (auto& s : ir.seg) s.assign ((size_t) maxSegs * (size_t) S, 0.0f); }
        for (auto& c : ch)
        {
            c.in.assign ((size_t) F, 0.0f);
            c.fdl.assign ((size_t) numIn * (size_t) S, 0.0f);
            c.work.assign ((size_t) (2 * F), 0.0f);
            for (int r = 0; r < 2; ++r) { c.acc[r].assign ((size_t) S, 0.0f); c.ov[r].assign ((size_t) B, 0.0f); c.y[r].assign ((size_t) B, 0.0f); }
        }
        buildWork.assign ((size_t) (2 * F), 0.0f);
        cur = 0; prev = 1; mailbox = 2; writer = 3;
        fading = false; fadePos = 0; fadeLen = std::max (1, (int) (sampleRate * 0.02));
        pos = 0; seg = 0;
    }

    void reset()
    {
        for (auto& c : ch)
        {
            std::fill (c.in.begin(), c.in.end(), 0.0f); std::fill (c.fdl.begin(), c.fdl.end(), 0.0f);
            for (int r = 0; r < 2; ++r) std::fill (c.ov[r].begin(), c.ov[r].end(), 0.0f);
        }
        pos = 0; seg = 0; fading = false;
    }

    // background thread: transform a stereo kernel and publish it
    // toM / toS (optional): cross kernels S → M and M → S (L/R bands in the M/S domain)
    void loadImpulseResponse (const float* left, const float* right, int length, const float* toM = nullptr, const float* toS = nullptr)
    {
        const std::lock_guard<std::mutex> lk (buildMutex);
        if (F == 0) return;
        length = juce::jlimit (1, maxIRLength, length);
        auto& ir = pool[(size_t) writer];
        ir.numSegs = std::min (maxSegs, (length + B - 1) / B);
        ir.cross = toM != nullptr && toS != nullptr;
        const float* src[4] { left, right != nullptr ? right : left, toM, toS };
        for (int c = 0; c < (ir.cross ? 4 : 2); ++c)
            for (int i = 0; i < ir.numSegs; ++i)
            {
                std::fill (buildWork.begin(), buildWork.end(), 0.0f);
                const int start = i * B, cnt = std::min (B, length - start);
                std::copy (src[c] + start, src[c] + start + cnt, buildWork.data());
                fftBuild->performRealOnlyForwardTransform (buildWork.data());
                pack (buildWork.data());
                std::copy (buildWork.data(), buildWork.data() + S, ir.seg[c].data() + (size_t) i * (size_t) S);
            }
        writer = mailbox.exchange (writer | kFresh) & kMask;
    }

    // audio thread: in place on two channels
    void process (float* left, float* right, int n)
    {
        float* io[2] { left, right };
        int done = 0;
        while (done < n)
        {
            if (pos == 0) takePending();
            const int k = std::min (n - done, B - pos);
            const bool blockStart = pos == 0, blockEnd = pos + k == B;
            const IR* irs[2] { &pool[(size_t) cur], fading ? &pool[(size_t) prev] : nullptr };

            for (int c = 0; c < 2; ++c)
            {
                auto& C = ch[(size_t) c];
                std::copy (io[c] + done, io[c] + done + k, C.in.data() + pos);
                // spectrum of the (partly filled) current block
                float* x = C.fdl.data() + (size_t) seg * (size_t) S;
                std::copy (C.in.begin(), C.in.end(), C.work.begin());
                fftAudio->performRealOnlyForwardTransform (C.work.data());
                pack (C.work.data());
                std::copy (C.work.data(), C.work.data() + S, x);
            }
            for (int c = 0; c < 2; ++c)
            {
                auto& C = ch[(size_t) c];
                const auto& X = ch[(size_t) (1 - c)];           // the other input (cross terms)
                const float* x = C.fdl.data() + (size_t) seg * (size_t) S;
                const float* xo = X.fdl.data() + (size_t) seg * (size_t) S;

                for (int r = 0; r < 2; ++r)
                {
                    const IR* ir = irs[r];
                    if (ir == nullptr) continue;
                    const float* h = ir->seg[c].data();
                    const float* hx = ir->cross ? ir->seg[(size_t) (2 + c)].data() : nullptr;
                    if (blockStart)                            // older blocks: once per block
                    {
                        std::fill (C.acc[r].begin(), C.acc[r].end(), 0.0f);
                        for (int i = 1; i < ir->numSegs; ++i)
                        {
                            const size_t at = (size_t) ((seg + i) % numIn) * (size_t) S;
                            mac (C.fdl.data() + at, h + (size_t) i * (size_t) S, C.acc[r].data());
                            if (hx != nullptr) mac (X.fdl.data() + at, hx + (size_t) i * (size_t) S, C.acc[r].data());
                        }
                    }
                    float* o = C.work.data();
                    std::copy (C.acc[r].begin(), C.acc[r].end(), o);
                    if (ir->numSegs > 0) { mac (x, h, o); if (hx != nullptr) mac (xo, hx, o); }
                    unpack (o);
                    fftAudio->performRealOnlyInverseTransform (o);
                    for (int j = 0; j < k; ++j) C.y[r][(size_t) j] = o[pos + j] + C.ov[r][(size_t) (pos + j)];
                    if (blockEnd) std::copy (o + B, o + 2 * B, C.ov[r].data());
                }

                float* out = io[c] + done;
                if (fading)
                    for (int j = 0; j < k; ++j)
                    {
                        const float g = std::min (1.0f, (float) (fadePos + j) / (float) fadeLen);
                        out[j] = C.y[0][(size_t) j] * g + C.y[1][(size_t) j] * (1.0f - g);
                    }
                else
                    std::copy (C.y[0].data(), C.y[0].data() + k, out);
            }

            if (fading && (fadePos += k) >= fadeLen) fading = false;
            pos += k; done += k;
            if (pos == B)
            {
                pos = 0;
                for (auto& c : ch) std::fill (c.in.begin(), c.in.end(), 0.0f);
                seg = seg > 0 ? seg - 1 : numIn - 1;
            }
        }
    }

private:
    static constexpr int kFresh = 0x100, kMask = 0xff;
    struct IR { int numSegs = 0; bool cross = false; std::array<std::vector<float>, 4> seg; };   // seg 0 M→M, 1 S→S, 2 S→M, 3 M→S
    struct Channel { std::vector<float> in, fdl, work; std::array<std::vector<float>, 2> acc, ov, y; };

    // block boundary only: install the newest kernel, rebuilding its overlap
    // from the input history so it is exact from the first sample
    void takePending()
    {
        if (fading || (mailbox.load() & kFresh) == 0) return;
        const int got = mailbox.exchange (prev) & kMask;
        prev = cur; cur = got;
        const auto& ir = pool[(size_t) cur];
        const int prevSeg = (seg + 1) % numIn;
        for (auto& C : ch)
        {
            std::swap (C.ov[0], C.ov[1]);
            const int c = (int) (&C - ch.data());
            float* o = C.work.data();
            std::fill (o, o + S, 0.0f);
            const auto& X = ch[(size_t) (1 - c)];
            for (int i = 0; i < ir.numSegs; ++i)
            {
                const size_t at = (size_t) ((prevSeg + i) % numIn) * (size_t) S;
                mac (C.fdl.data() + at, ir.seg[(size_t) c].data() + (size_t) i * (size_t) S, o);
                if (ir.cross) mac (X.fdl.data() + at, ir.seg[(size_t) (2 + c)].data() + (size_t) i * (size_t) S, o);
            }
            unpack (o);
            fftAudio->performRealOnlyInverseTransform (o);
            std::copy (o + B, o + 2 * B, C.ov[0].data());
        }
        fading = true; fadePos = 0;
    }

    // Packed spectrum layout and complex MAC (same scheme as JUCE's engine)
    void pack (float* s) const noexcept
    {
        const int h = F / 2;
        for (int i = 0; i < h; ++i) s[i] = s[i << 1];
        s[h] = 0;
        for (int i = 1; i < h; ++i) s[i + h] = -s[((F - i) << 1) + 1];
    }
    void mac (const float* in, const float* ir, float* out) const noexcept
    {
        using FVO = juce::FloatVectorOperations;
        const int h = F / 2;
        FVO::addWithMultiply (out, in, ir, h);
        FVO::subtractWithMultiply (out, in + h, ir + h, h);
        FVO::addWithMultiply (out + h, in, ir + h, h);
        FVO::addWithMultiply (out + h, in + h, ir, h);
        out[F] += in[F] * ir[F];
    }
    void unpack (float* s) const noexcept
    {
        const int h = F / 2;
        for (int i = 1; i < h; ++i) { s[(F - i) << 1] = s[i]; s[((F - i) << 1) + 1] = -s[h + i]; }
        s[1] = 0.0f; s[F + 1] = 0.0f;
        for (int i = 1; i < h; ++i) { s[i << 1] = s[(F - i) << 1]; s[(i << 1) + 1] = -s[((F - i) << 1) + 1]; }
    }

    int B = 0, F = 0, S = 0, maxSegs = 0, numIn = 0;
    std::unique_ptr<juce::dsp::FFT> fftAudio, fftBuild;
    std::array<IR, 4> pool;
    std::array<Channel, 2> ch;
    std::vector<float> buildWork;
    std::mutex buildMutex;
    std::atomic<int> mailbox { 2 };
    int cur = 0, prev = 1, writer = 3;               // reader owns cur + prev, builder owns writer
    bool fading = false; int fadePos = 0, fadeLen = 960;
    int pos = 0, seg = 0;
};
