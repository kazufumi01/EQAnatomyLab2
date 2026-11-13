#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "Params.h"
#include "EQCore.h"
#include "BandSaturator.h"
#include "XfadeConvolver.h"
#include "LinkRegistry.h"
#include "ResSuppressor.h"
#include "AnalogEQ.h"
#include "SamplePlayer.h"

// =============================================================================
//  Zero-latency engine: RBJ biquads with smoothed frequency / gain / Q,
//  coefficients recomputed every 32 samples. Works in the mid/side domain:
//  each band runs on mid, side or both (stereo). A structural change (band
//  on/off, type, slope, curve style, channel) builds a new chain that is
//  crossfaded in over 30 ms. Each band can be weighted towards transients or
//  body: out = in + w · (band(in) − in).
// =============================================================================
class IIRChain
{
public:
    struct Target { bool on; eqcore::Type type; int slope; double f, g, q; int mask; };

    void configure (const std::array<Target, eqcore::kBands>& t, eqcore::QMode qm, double fs)
    {
        qmode = qm; sampleRate = fs;
        for (int i = 0; i < eqcore::kBands; ++i)
        {
            auto& b = bands[(size_t) i]; const auto& x = t[(size_t) i];
            b.on = x.on && x.mask != 0; b.type = x.type; b.slope = x.slope; b.mask = x.mask;
            b.lf = b.tlf = std::log2 (x.f); b.g = b.tg = x.g; b.lq = b.tlq = std::log2 (x.q);
            b.z.clear();
        }
        anyLR = false; for (auto& b : bands) anyLR = anyLR || (b.on && (b.mask & 12) != 0);
        updateCoefficients (false);
    }
    bool sameStructure (const std::array<Target, eqcore::kBands>& t, eqcore::QMode qm) const
    {
        if (qm != qmode) return false;
        for (int i = 0; i < eqcore::kBands; ++i)
        {
            const auto& b = bands[(size_t) i]; const auto& x = t[(size_t) i];
            const bool on = x.on && x.mask != 0;
            if (b.on != on) return false;
            if (! on) continue;
            if (b.type != x.type || b.mask != x.mask || (b.slope != x.slope && x.type != eqcore::Type::Bell && x.type != eqcore::Type::Notch && x.type != eqcore::Type::AllPass && x.type != eqcore::Type::Tilt && x.type != eqcore::Type::FlatTilt)) return false;
        }
        return true;
    }
    void setTargets (const std::array<Target, eqcore::kBands>& t)
    {
        for (int i = 0; i < eqcore::kBands; ++i)
        {
            auto& b = bands[(size_t) i];
            b.tlf = std::log2 (t[(size_t) i].f); b.tg = t[(size_t) i].g; b.tlq = std::log2 (t[(size_t) i].q);
        }
    }
    void setDyn (const std::array<double, eqcore::kBands>& d) { dyn = d; }
    void updateCoefficients (bool smooth)
    {
        const double k = smooth ? 1.0 - std::exp (-32.0 / (sampleRate * 0.012)) : 1.0;
        for (size_t i = 0; i < bands.size(); ++i)
        {
            auto& b = bands[i];
            if (! b.on) { b.coefs.clear(); continue; }
            b.lf += (b.tlf - b.lf) * k; b.g += (b.tg - b.g) * k; b.lq += (b.tlq - b.lq) * k;
            // dynamic gain is added on top, unsmoothed: the detector envelope is already smooth
            eqcore::Band eb; eb.on = true; eb.type = b.type; eb.slope = b.slope; eb.f = std::exp2 (b.lf);
            eb.g = juce::jlimit (-30.0, 30.0, b.g + dyn[i]); eb.q = std::exp2 (b.lq);
            scratch.clear(); eqcore::sectionsFor (eb, qmode, scratch);
            b.coefs.resize (scratch.size());
            if (b.z.size() != scratch.size()) b.z.assign (scratch.size(), { 0, 0, 0, 0 });
            for (size_t s = 0; s < scratch.size(); ++s) b.coefs[s] = eqcore::digitalSection (scratch[s], sampleRate);
        }
    }
    // v[0] = mid, v[1] = side; w = per-band weight (1 = full effect).
    // mask bits: 1 mid, 2 side, 4 left, 8 right. The M/S stage runs first, then the L/R stage
    // (decoded to L / R, filtered, encoded back) — the same order as the FIR path.
    inline void process (double* v, const std::array<double, eqcore::kBands>& w)
    {
        run (v, w, 0);
        if (anyLR)
        {
            double lr[2] = { v[0] + v[1], v[0] - v[1] };
            run (lr, w, 2);
            v[0] = 0.5 * (lr[0] + lr[1]); v[1] = 0.5 * (lr[0] - lr[1]);
        }
    }
    inline void run (double* v, const std::array<double, eqcore::kBands>& w, int shift)
    {
        for (size_t bi = 0; bi < bands.size(); ++bi)
        {
            auto& b = bands[bi];
            if (b.coefs.empty()) continue;
            for (int ch = 0; ch < 2; ++ch)
            {
                if ((b.mask & (1 << (ch + shift))) == 0) continue;
                const double in = v[ch]; double x = in;
                for (size_t s = 0; s < b.coefs.size(); ++s)
                {
                    const auto& c = b.coefs[s]; auto& z = b.z[s];
                    const int o = ch * 2;
                    const double y = c.b0 * x + z[(size_t) o]; z[(size_t) o] = c.b1 * x - c.a1 * y + z[(size_t) o + 1]; z[(size_t) o + 1] = c.b2 * x - c.a2 * y; x = y;
                }
                v[ch] = w[bi] >= 0.999 ? x : in + w[bi] * (x - in);
            }
        }
    }
    void reserve() { scratch.reserve (24); for (auto& b : bands) { b.coefs.reserve (24); b.z.reserve (24); } }

private:
    struct B { bool on = false; eqcore::Type type = eqcore::Type::Bell; int slope = 3; int mask = 3;
               double lf = 10, g = 0, lq = 0, tlf = 10, tg = 0, tlq = 0;
               std::vector<eqcore::Biquad> coefs; std::vector<std::array<double, 4>> z; };
    std::array<B, eqcore::kBands> bands;
    std::array<double, eqcore::kBands> dyn {};
    bool anyLR = false;
    std::vector<eqcore::Section> scratch;
    eqcore::QMode qmode = eqcore::QMode::Constant;
    double sampleRate = 48000;
};

// Dynamic EQ detector: the band's own shape (band-pass for a bell, low/high-pass
// for shelves), then an RMS envelope (5 ms attack, 120 ms release)
struct DynDetector
{
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0, env = 0;
    void design (eqcore::Type t, double f, double q, double fs)
    {
        const double w = 2.0 * juce::MathConstants<double>::pi * juce::jlimit (20.0, fs * 0.45, f) / fs;
        const double cw = std::cos (w), al = std::sin (w) / (2.0 * std::max (0.3, q)), a0 = 1.0 + al;
        if (t == eqcore::Type::LowShelf)       { b0 = (1 - cw) / 2; b1 = 1 - cw; b2 = (1 - cw) / 2; }
        else if (t == eqcore::Type::HighShelf) { b0 = (1 + cw) / 2; b1 = -(1 + cw); b2 = (1 + cw) / 2; }
        else                                   { b0 = al; b1 = 0; b2 = -al; }
        b0 /= a0; b1 /= a0; b2 /= a0; a1 = -2.0 * cw / a0; a2 = (1.0 - al) / a0;
    }
    inline void process (double x, double att, double rel)
    {
        const double y = b0 * x + z1; z1 = b1 * x - a1 * y + z2; z2 = b2 * x - a2 * y;
        const double e = y * y; env += (e > env ? att : rel) * (e - env);
    }
    double levelDb() const { return 10.0 * std::log10 (env * 2.0 + 1e-12); }   // sine peak = 0 dBFS
    void reset() { z1 = z2 = env = 0; }
};

// Detector-only EQ in front of the transient detector (never heard): 12 dB/oct low cut + two bells. Bypassed at its defaults.
struct DetBiquad
{
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
    inline double process (double x) { const double y = b0 * x + z1; z1 = b1 * x - a1 * y + z2; z2 = b2 * x - a2 * y; return y; }
};
struct DetectorEQ
{
    DetBiquad hp, p1, p2; bool hpOn = false, p1On = false, p2On = false; double fs = 48000; float key[7] { -1, -1, -1, -1, -1, -1, -1 };
    void prepare (double s) { fs = s; hp = {}; p1 = {}; p2 = {}; for (auto& k : key) k = -1.0e9f; hpOn = p1On = p2On = false; }
    static void highpass (DetBiquad& b, double fs, double f)
    {
        const double w = juce::MathConstants<double>::twoPi * f / fs, c = std::cos (w), al = std::sin (w) / (2.0 * 0.7071067811865476), a0 = 1.0 + al;
        const auto z1 = b.z1, z2 = b.z2;
        b = { (1.0 + c) / 2.0 / a0, -(1.0 + c) / a0, (1.0 + c) / 2.0 / a0, -2.0 * c / a0, (1.0 - al) / a0, z1, z2 };
    }
    static void peak (DetBiquad& b, double fs, double f, double g, double q)
    {
        const double A = std::pow (10.0, g / 40.0), w = juce::MathConstants<double>::twoPi * juce::jmin (f, fs * 0.45) / fs, c = std::cos (w), al = std::sin (w) / (2.0 * q), a0 = 1.0 + al / A;
        const auto z1 = b.z1, z2 = b.z2;
        b = { (1.0 + al * A) / a0, -2.0 * c / a0, (1.0 - al * A) / a0, -2.0 * c / a0, (1.0 - al / A) / a0, z1, z2 };
    }
    void set (float lc, float f1, float g1, float q1, float f2, float g2, float q2)
    {
        const float k[7] { lc, f1, g1, q1, f2, g2, q2 };
        if (std::equal (k, k + 7, key)) return;
        std::copy (k, k + 7, key);
        hpOn = lc > 20.5f; if (hpOn) highpass (hp, fs, lc);
        p1On = std::abs (g1) > 0.05f; if (p1On) peak (p1, fs, f1, g1, q1);
        p2On = std::abs (g2) > 0.05f; if (p2On) peak (p2, fs, f2, g2, q2);
    }
    bool active() const { return hpOn || p1On || p2On; }
    inline double process (double x) { if (hpOn) x = hp.process (x); if (p1On) x = p1.process (x); if (p2On) x = p2.process (x); return x; }
};

// Transient detector: how much the mid signal's attack stands above its body → 0 (body) … 1 (attack)
//   Ratio: fast vs slow envelope (the original)   Slope: how many dB the fast envelope rose over the last 5 ms
struct TransientDetector
{
    int method = 0; std::vector<double> slopeBuf; int slopePos = 0;
    double fast = 0, slow = 0, mask = 0, fa = 0, fr = 0, sa = 0, sr = 0, mk = 0, mka = 0, fsT = 48000, lastD = 0;
    double sens = 0.0, width = 6.0;      // dB added before the ramp, and the ramp width (0 and 6 = the original fixed behaviour)
    void prepare (double fs)
    {
        auto c = [fs] (double t) { return 1.0 - std::exp (-1.0 / (fs * t)); };
        fa = c (0.001); fr = c (0.020); sa = c (0.030); sr = c (0.200); mk = c (0.002); mka = mk; fsT = fs;
        fast = slow = mask = 0;
        slopeBuf.assign ((size_t) std::max (1, (int) std::round (fs * 0.005)), -120.0); slopePos = 0;
    }
    // how fast the mask rises (ms). 2 ms = the same coefficient as the fall, i.e. the original behaviour bit for bit
    void setAttack (double ms) { mka = 1.0 - std::exp (-1.0 / (fsT * (ms / 1000.0))); }
    inline double process (double x)
    {
        const double a = x * x;
        fast += (a > fast ? fa : fr) * (a - fast);
        slow += (a > slow ? sa : sr) * (a - slow);
        double d;
        if (method == 1 && ! slopeBuf.empty())
        {
            const double e = 10.0 * std::log10 (fast + 1e-12);                       // dB per 5 ms
            d = e - slopeBuf[(size_t) slopePos]; slopeBuf[(size_t) slopePos] = e; slopePos = (slopePos + 1) % (int) slopeBuf.size();
        }
        else d = 10.0 * std::log10 ((fast + 1e-12) / (slow + 1e-12));     // dB the attack stands above the body
        lastD = d;
        const double tgt = juce::jlimit (0.0, 1.0, (d + sens) / width);
        mask += (tgt - mask) * (tgt > mask ? mka : mk);
        return mask;
    }
};

// Plain delay line for latency alignment
struct Delay
{
    std::vector<float> buf; int pos = 0, mask = 0;
    void prepare (int maxDelay) { int n = 1; while (n < maxDelay + 2) n <<= 1; buf.assign ((size_t) n, 0.0f); mask = n - 1; pos = 0; }
    void clear() { std::fill (buf.begin(), buf.end(), 0.0f); }
    inline float process (float x, int d) { buf[(size_t) pos] = x; const float y = buf[(size_t) ((pos - d) & mask)]; pos = (pos + 1) & mask; return y; }
};

// What the editor shows: the impulse response of the current setup and its numbers
struct DisplayIR { std::vector<float> h; int latency = 0; double preDb = -1e9, preMs = 0; double fs = 48000; int version = 0; };

class EQAnatomyProcessor : public juce::AudioProcessor,
                           private juce::AudioProcessorValueTreeState::Listener,
                           private juce::Timer,
                           private juce::Thread
{
public:
    EQAnatomyProcessor();
    ~EQAnatomyProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "EQ Anatomy Lab 2 v1"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.4; }
    // instance link: the host's track name (when it tells us) names this instance in the other instances' analyzer menus
    void updateTrackProperties (const TrackProperties& p) override { if (p.name.has_value() && p.name->isNotEmpty()) linkTap->setName (*p.name); }
    std::shared_ptr<LinkTap> linkTap = LinkRegistry::add();
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::UndoManager undo;
    juce::AudioProcessorValueTreeState apvts;

    // ---- editor helpers ----
    std::array<eqcore::Band, eqcore::kBands> currentBands() const;
    eqcore::Mode currentMode() const;
    eqcore::QMode currentQMode() const;
    int currentRes() const;
    double getSampleRateOrDefault() const { return sr > 0 ? sr : 48000.0; }
    float currentAutoGainDb() const { return autoGainDb.load(); }
    int  currentLatency() const { return latencyReported; }
    void copyDisplayIR (DisplayIR& out) { const juce::SpinLock::ScopedLockType l (irLock); out = displayIR; }
    int  displayVersion() const { return displayVersionAtomic.load(); }
    bool bandIsLin (int i) const;
    bool dynAvailable (int band) const;          // DYN and Transient/Body need a zero-latency min-phase band
    bool sidechainConnected() const { return scConnected.load(); }

    // Dynamic EQ readouts for the editor (dB)
    std::array<std::atomic<float>, eqcore::kBands> dynGainDb {}, detLevelDb {};
    std::atomic<float> satMakeupDb { 0.0f };
    // Meters: peak (dBFS, max since last read), RMS (300 ms), output correlation
    std::array<std::atomic<float>, 2> inPeak {}, outPeak {}, inRms {}, outRms {};
    std::atomic<float> correlation { 1.0f };

    // Band solo (−1 = off) and ear training
    std::atomic<int> soloBand { -1 };
    // kind 0: a hidden bell (guess the band). kind 1 (Lab 2): the same filter in min and linear phase on A / B — which one pre-rings?
    struct EarQuestion { float f = 1000, g = 6, q = 2; int kind = 0; int type = 3; int slope = 5; bool linOnB = true; };
    void setEar (bool on, EarQuestion q);
    void setEarPlayEQ (bool b) { earB = b; if (earKind.load() == 1) loadEarKernel(); }
    bool earActive() const { return earOn.load(); }

    // Analyzer: post-EQ Mid (0.5·(L+R)), Side, and the Transient / Body parts of the Mid.
    // All four share one FIFO and are written together. Transient = mid·mask, Body = mid·(1−mask),
    // with the detector's mask delayed to line up with the (possibly latent) output.
    juce::AbstractFifo analyzerFifo { 16384 };
    std::vector<float> analyzerBuffer = std::vector<float> (16384, 0.0f);   // Mid
    std::vector<float> analyzerS = std::vector<float> (16384, 0.0f);
    std::vector<float> analyzerT = std::vector<float> (16384, 0.0f);
    std::vector<float> analyzerB = std::vector<float> (16384, 0.0f);
    // 0.1.1: the input before the EQ (Mid / Side of the dry, delayed like the output) and the sidechain (mono sum)
    std::vector<float> analyzerInM = std::vector<float> (16384, 0.0f);
    std::vector<float> analyzerInS = std::vector<float> (16384, 0.0f);
    std::vector<float> analyzerSC = std::vector<float> (16384, 0.0f);
    std::atomic<bool> analyzerNeedTB { false };      // the editor shows a Transient / Body layer
    // T/B scope: the raw input (Mid) while the scope is shown. The editor runs its own copy of the detector on it,
    // so it can change the time span and re-draw a paused picture with new Sens / Width / Attack values.
    static constexpr int scopeRawN = 1 << 20;            // about 21 s at 48 kHz, 10 s at 96 kHz
    std::vector<float> scopeRaw = std::vector<float> ((size_t) scopeRawN, 0.0f);
    std::atomic<juce::uint64> scopeRawW { 0 };           // samples written so far
    std::atomic<bool> scopeNeed { false };
    DetectorEQ detEq;                                    // what the transient detector hears (audio thread; the editor only reads active())

    // Solo view of the output: 0 off, 1 Mid, 2 Side, 3 Transient, 4 Body. Never saved (like ear training): always off after loading.
    // Yields to EAR, Bypass, Delta, Pre-ring solo and band solo, in that sense they take priority.
    std::atomic<int> soloView { 0 };
    bool soloViewBlocked() const
    {
        return earOn.load() || soloBand.load() >= 0
            || apvts.getRawParameterValue (pid::bypass)->load() > 0.5f || apvts.getRawParameterValue (pid::delta)->load() > 0.5f
            || apvts.getRawParameterValue (pid::preSolo)->load() > 0.5f;
    }
    int soloViewEffective() const { return soloViewBlocked() ? 0 : soloView.load(); }

    // Editor-only state, saved with the plugin
    juce::ValueTree uiState { "UI" };
    void switchSlot (int slot);                 // A–D
    int  currentSlot() const { return (int) uiState.getProperty ("slot", 0); }
    void storeCurrentSlot();
    juce::ValueTree slotState (int slot) const;
    void morph (int from, int to, float t);     // writes interpolated parameters
    void applyLesson (int index);
    void applyPreset (int index);               // −1 = flat
    // user presets: one XML file per preset (every parameter except Bypass / Delta / Pre-ring solo), with an optional note
    static juce::File userPresetDir();
    juce::Array<juce::File> userPresets() const;
    bool saveUserPreset (const juce::String& name, const juce::String& note);
    bool loadUserPreset (const juce::File& f);
    void resetToFlat();

private:
    void parameterChanged (const juce::String&, float) override { dirty = true; }
    void timerCallback() override;
    void run() override;                 // builds FIR kernels off the audio thread
    std::array<IIRChain::Target, eqcore::kBands> targets() const;
    void computeLatency();

    double sr = 0; int blockSize = 512;
    IIRChain chainA, chainB; IIRChain* cur = &chainA; IIRChain* nxt = &chainB; int xfade = 0, xfadeLen = 1440;
    int subCounter = 0;
    XfadeConvolver convolution;
    juce::AudioBuffer<float> ms, dryD, tmp;
    Delay dryDelay[2], postDelay[2];
    juce::LinearSmoothedValue<float> outGain, autoGain;
    BandSaturator sat; bool satWasOn = false;
public:
    ResSuppressor rs;           // the editor reads rs.dispDb for the graph
    std::atomic<int> sampleSrc { -1 };   // Learn mode's practice material replacing the input: −1 off, 0 drums, 1 voice, 2 both (not saved)
private:
    bool rsWasOn = false; Delay rsDelay[2];
    SamplePlayer player; int playerWas = -1;
    Delay scDelay[2]; std::vector<float> scBuf[2];      // the sidechain, delayed like the EQ's output, for the suppressor's detector
    std::array<std::atomic<float>*, 10> rsNodeP {};      // threshold nodes f / g, looked up once
    analog::Processor analogEq; bool anWasOn = false;     // 1.0: analog EQ model stage
    std::array<std::atomic<float>*, analog::kSlots> anKP {};
    std::array<DynDetector, eqcore::kBands> det; std::array<double, eqcore::kBands> dynNow {}; double detAtt = 0.1, detRel = 0.01;
    TransientDetector trans;
    std::vector<double> maskBuf, maskDel;     // per-block transient mask, and the same delayed to match the output
    Delay maskDelay;
    double svG[5] { 1, 0, 0, 0, 0 }, svC = 0.01;      // solo-view cross-fade gains (normal, M, S, T, B)
    std::atomic<bool> scConnected { false };

    std::atomic<bool> dirty { true };
    std::atomic<int>  wantedMode { 0 }, wantedKernelLat { 0 }, wantedSatLat { 0 }, wantedRsLat { 0 };
    int  activeMode = 0, activeKernelLat = 0, activeSatLat = 0, activeRsLat = 0, modeFade = 0, modeFadeLen = 720; bool fadingOut = false;
    std::atomic<float> autoGainDb { 0.0f };
    int latencyReported = 0;
    double rmsCoef = 0.001; double inMs[2] {}, outMs[2] {}, cLR = 0, cLL = 0, cRR = 0;

    // band solo filter
    struct Bq { double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
                inline double process (double x) { const double y = b0 * x + z1; z1 = b1 * x - a1 * y + z2; z2 = b2 * x - a2 * y; return y; } };
    Bq soloF[2], earF[2]; int lastSolo = -1;
    std::atomic<bool> earOn { false }, earB { true }; std::atomic<float> earGainComp { 1.0f };
    // pre-ring ear training: one convolver, kernel A or B (both 4096 samples late, so they line up)
    static constexpr int kEarPreLat = 4096;
    std::atomic<int> earKind { 0 }; XfadeConvolver earConv; std::vector<float> earKA, earKB; void loadEarKernel();
    juce::SpinLock earLock; EarQuestion earQ; bool earDirty = false;

    juce::WaitableEvent buildEvent;
    std::atomic<int> buildRequest { 0 };
    juce::SpinLock irLock; DisplayIR displayIR; std::atomic<int> displayVersionAtomic { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EQAnatomyProcessor)
};
