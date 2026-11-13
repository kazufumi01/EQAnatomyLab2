#pragma once
#include <juce_dsp/juce_dsp.h>

// =============================================================================
//  Band saturator: Linkwitz-Riley 4th-order split (low | band | high),
//  only the band is driven. The low band goes through the matching all-pass
//  so the three bands sum flat.
//
//  Shapers use first-order ADAA (antiderivative anti-aliasing). Optional 2× / 4×
//  oversampling runs the whole saturator (split filters included) at the higher
//  rate, so every path is delayed equally and the bands still sum flat.
//  Output is divided by the small-signal gain, so Drive changes how much it
//  distorts, not how loud quiet material is. Auto level then restores the RMS
//  the shaper took off the peaks, so the band does not get quieter.
//
//  Types (tendencies, not models of specific units):
//    Tape        symmetric tanh, odd harmonics
//    Tube        asymmetric, the bias follows the level (drives fatter when pushed)
//    Clip        hard clip
//    Transformer tanh after a low-shelf pre-emphasis, so lows saturate first
//    Class A     tanh plus a tanh² term: some 2nd harmonic even at low drive
//    Exciter     asymmetric shaper; only what it adds above the LOW edge is mixed in
// =============================================================================
class SatCore
{
public:
    enum Type { Tape = 0, Tube, Clip, Transformer, ClassA, Exciter };

    void prepare (double fs, int maxBlock)
    {
        sampleRate = fs;
        juce::dsp::ProcessSpec spec { fs, (juce::uint32) std::max (1, maxBlock), 2 };
        for (auto* f : { &splitLo, &splitHi, &apHi }) { f->prepare (spec); f->reset(); }
        apHi.setType (juce::dsp::LinkwitzRileyFilterType::allpass);
        curLo = tgtLo; curHi = tgtHi; applyCutoffs();
        drive.reset (fs, 0.03); mix.reset (fs, 0.03);
        reset();
        eCoef = 1.0 - std::exp (-1.0 / (fs * 0.3));          // 300 ms energy average
        mCoef = 1.0 - std::exp (-1.0 / (fs * 0.05));         // 50 ms gain glide
        envCoef = 1.0 - std::exp (-1.0 / (fs * 0.08));       // tube bias follower
        design();
    }
    void setTargets (float lo, float hi, float driveDb, float mixPct, int t, bool autoLevel)
    {
        autoOn = autoLevel;
        lo = juce::jlimit (20.0f, 19000.0f, lo); hi = juce::jlimit (25.0f, 20000.0f, hi);
        if (hi < lo * 1.05f) hi = std::min (20000.0f, lo * 1.05f);
        tgtLo = lo; tgtHi = hi;
        if (t != type) { type = t; for (auto& s : st) { s.init = false; } }
        drive.setTargetValue (juce::Decibels::decibelsToGain (driveDb));
        mix.setTargetValue (mixPct * 0.01f);
    }
    void reset()
    {
        for (auto* f : { &splitLo, &splitHi, &apHi }) f->reset();
        for (auto& s : st) s = {};
        eIn = eOut = 0; makeup = 1;
    }
    double makeupGain() const { return makeup; }

    void process (float* L, float* R, int n, bool deltaOnly)
    {
        const bool useLo = tgtLo > 21.0f, useHi = tgtHi < 19500.0f;
        for (int i0 = 0; i0 < n; i0 += 32)
        {
            const float k = 1.0f - std::exp (-32.0f / (float) (sampleRate * 0.02));
            curLo = std::exp (std::log (curLo) + (std::log (tgtLo) - std::log (curLo)) * k);
            curHi = std::exp (std::log (curHi) + (std::log (tgtHi) - std::log (curHi)) * k);
            applyCutoffs();
            if (std::abs (curLo - designedLo) > designedLo * 0.01f) design();
            const int end = std::min (n, i0 + 32);
            for (int i = i0; i < end; ++i)
            {
                const float g = drive.getNextValue(), m = mix.getNextValue();
                float* io[2] = { L + i, R + i };
                double ei = 0, eo = 0;
                for (int c = 0; c < 2; ++c)
                {
                    auto& s = st[(size_t) c];
                    const float x = *io[c];
                    float low = 0, rest = x, band, high = 0;
                    if (useLo) splitLo.processSample (c, x, low, rest);
                    if (useHi) { splitHi.processSample (c, rest, band, high); low = apHi.processSample (c, low); }
                    else band = rest;

                    float in = band;
                    if (type == Transformer) in = (float) s.pre.process (in);           // lows hit the core harder
                    if (type == Tube) s.env += (std::abs (in * g) - s.env) * envCoef;
                    float wet = shape (s, in * g) / (g * slope0 (s));
                    if (type == Transformer) wet = (float) s.post.process (wet);
                    float added = wet * (float) makeup - band;
                    if (type == Exciter) added = (float) s.hp2.process (s.hp1.process (added));   // only above LOW
                    added = dcBlock (s, added);
                    ei += (double) band * band; eo += (double) wet * wet;
                    *io[c] = deltaOnly ? m * added : low + band + m * added + high;
                }
                const double want = autoOn && type != Exciter && eOut > 1e-10
                                  ? juce::jlimit (1.0, 4.0, std::sqrt ((eIn + 1e-12) / (eOut + 1e-12))) : 1.0;
                eIn += (ei - eIn) * eCoef; eOut += (eo - eOut) * eCoef;
                makeup += (want - makeup) * mCoef;
            }
        }
    }

    // Static transfer curve for the UI
    static float curve (int t, float x, float g)
    {
        const double b = 0.35, xb = (double) x * g;
        double y;
        switch (t)
        {
            case Tube:    y = std::tanh (xb + b) - std::tanh (b); break;
            case Clip:    y = juce::jlimit (-1.0, 1.0, xb); break;
            case ClassA:  y = std::tanh (xb) + 0.15 * std::tanh (xb) * std::tanh (xb); break;
            case Exciter: y = std::tanh (xb + 0.5) - std::tanh (0.5); break;
            default:      y = std::tanh (xb);
        }
        return (float) y;
    }

private:
    struct Bq
    {
        double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
        inline double process (double x) { const double y = b0 * x + z1; z1 = b1 * x - a1 * y + z2; z2 = b2 * x - a2 * y; return y; }
    };
    struct State { double x1 = 0, F1 = 0, env = 0; float dcx = 0, dcy = 0; bool init = false; Bq pre, post, hp1, hp2; };

    static double logcosh (double x) { const double a = std::abs (x); return a + std::log1p (std::exp (-2.0 * a)) - 0.6931471805599453; }
    double bias (const State& s) const { return type == Tube ? 0.2 + 0.5 * std::min (1.0, s.env) : type == Exciter ? 0.5 : 0.0; }
    double f (const State& s, double x) const
    {
        const double b = bias (s);
        switch (type)
        {
            case Tube: case Exciter: return std::tanh (x + b) - std::tanh (b);
            case Clip:   return juce::jlimit (-1.0, 1.0, x);
            case ClassA: { const double t = std::tanh (x); return t + 0.15 * t * t; }
            default:     return std::tanh (x);
        }
    }
    double F (const State& s, double x) const
    {
        const double b = bias (s);
        switch (type)
        {
            case Tube: case Exciter: return logcosh (x + b) - x * std::tanh (b);
            case Clip:   return std::abs (x) <= 1.0 ? 0.5 * x * x : std::abs (x) - 0.5;
            case ClassA: return logcosh (x) + 0.15 * (x - std::tanh (x));          // ∫tanh² = x − tanh
            default:     return logcosh (x);
        }
    }
    float slope0 (const State& s) const
    {
        const double b = bias (s);
        return (type == Tube || type == Exciter) ? (float) (1.0 - std::tanh (b) * std::tanh (b)) : 1.0f;
    }
    float shape (State& s, float xin)
    {
        const double x = xin, Fx = F (s, x);
        if (! s.init) { s.init = true; s.x1 = x; s.F1 = Fx; return (float) f (s, x); }
        const double dx = x - s.x1;
        // Tube: the bias follows the level and changes every sample. The previous antiderivative must use the SAME
        // (current) bias, or the bias change divided by a tiny dx (slow, loud lows near their peaks) becomes a spike
        const double F1 = type == Tube ? F (s, s.x1) : s.F1;
        const double y = std::abs (dx) < 1e-5 ? f (s, 0.5 * (x + s.x1)) : (Fx - F1) / dx;
        s.x1 = x; s.F1 = Fx;
        return (float) y;
    }
    float dcBlock (State& s, float x)          // 8 Hz one-pole high-pass: asymmetric curves make DC
    {
        const float r = 1.0f - (float) (2.0 * juce::MathConstants<double>::pi * 8.0 / sampleRate);
        const float y = x - s.dcx + r * s.dcy; s.dcx = x; s.dcy = y; return y;
    }
    // RBJ shelves (transformer emphasis) and the exciter's 4th-order high-pass at LOW
    void design()
    {
        designedLo = curLo;
        auto shelf = [this] (Bq& q, double f0, double db)
        {
            const double A = std::pow (10.0, db / 40.0), w = 2 * juce::MathConstants<double>::pi * f0 / sampleRate;
            const double cw = std::cos (w), al = std::sin (w) / (2 * 0.7071), sa = 2 * std::sqrt (A) * al;
            const double a0 = (A + 1) + (A - 1) * cw + sa;
            q.b0 = A * ((A + 1) - (A - 1) * cw + sa) / a0; q.b1 = 2 * A * ((A - 1) - (A + 1) * cw) / a0; q.b2 = A * ((A + 1) - (A - 1) * cw - sa) / a0;
            q.a1 = -2 * ((A - 1) + (A + 1) * cw) / a0; q.a2 = ((A + 1) + (A - 1) * cw - sa) / a0;
        };
        auto hp = [this] (Bq& q, double f0, double Q)
        {
            const double w = 2 * juce::MathConstants<double>::pi * juce::jlimit (20.0, sampleRate * 0.45, f0) / sampleRate;
            const double cw = std::cos (w), al = std::sin (w) / (2 * Q), a0 = 1 + al;
            q.b0 = (1 + cw) / 2 / a0; q.b1 = -(1 + cw) / a0; q.b2 = (1 + cw) / 2 / a0; q.a1 = -2 * cw / a0; q.a2 = (1 - al) / a0;
        };
        for (auto& s : st)
        {
            shelf (s.pre, 150.0, 9.0); shelf (s.post, 150.0, -9.0);
            hp (s.hp1, curLo, 0.5412); hp (s.hp2, curLo, 1.3066);
        }
    }
    void applyCutoffs() { splitLo.setCutoffFrequency (curLo); splitHi.setCutoffFrequency (curHi); apHi.setCutoffFrequency (curHi); }

    double sampleRate = 48000;
    juce::dsp::LinkwitzRileyFilter<float> splitLo, splitHi, apHi;
    float tgtLo = 200, tgtHi = 5000, curLo = 200, curHi = 5000, designedLo = 0; int type = Tape;
    juce::LinearSmoothedValue<float> drive, mix;
    std::array<State, 2> st;
    bool autoOn = true; double eIn = 0, eOut = 0, makeup = 1, eCoef = 0.0001, mCoef = 0.001, envCoef = 0.001;
};

class BandSaturator
{
public:
    using Type = SatCore::Type;
    void prepare (double fs, int maxBlock)
    {
        for (int i = 0; i < 3; ++i) cores[(size_t) i].prepare (fs * (1 << i), maxBlock * (1 << i));
        for (int i = 0; i < 2; ++i)
        {
            os[(size_t) i] = std::make_unique<juce::dsp::Oversampling<float>> (2, (size_t) (i + 1),
                                juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple, true, true);
            os[(size_t) i]->initProcessing ((size_t) std::max (1, maxBlock));
        }
    }
    void setOversampling (int index) { factorIndex = juce::jlimit (0, 2, index); }
    int latencySamples() const { return factorIndex == 0 || ! os[(size_t) factorIndex - 1] ? 0 : (int) std::round (os[(size_t) factorIndex - 1]->getLatencyInSamples()); }
    void setTargets (float lo, float hi, float driveDb, float mixPct, int t, bool autoLevel)
    { for (auto& c : cores) c.setTargets (lo, hi, driveDb, mixPct, t, autoLevel); }
    void reset() { for (auto& c : cores) c.reset(); for (auto& o : os) if (o) o->reset(); }
    float currentMakeupDb() const { return (float) (20.0 * std::log10 (cores[(size_t) factorIndex].makeupGain())); }

    // L and R must be two separate buffers
    void process (float* L, float* R, int n, bool deltaOnly)
    {
        if (factorIndex == 0) { cores[0].process (L, R, n, deltaOnly); return; }
        float* ch[2] = { L, R };
        juce::dsp::AudioBlock<float> block (ch, 2, (size_t) n);
        auto& o = *os[(size_t) factorIndex - 1];
        auto up = o.processSamplesUp (block);
        cores[(size_t) factorIndex].process (up.getChannelPointer (0), up.getChannelPointer (1), (int) up.getNumSamples(), deltaOnly);
        o.processSamplesDown (block);
    }
    static float curve (int t, float x, float g) { return SatCore::curve (t, x, g); }

private:
    std::array<SatCore, 3> cores;
    std::array<std::unique_ptr<juce::dsp::Oversampling<float>>, 2> os;
    int factorIndex = 0;
};
