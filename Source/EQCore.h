#pragma once
#include <cmath>
#include <vector>
#include <complex>
#include <array>
#include <algorithm>

// =============================================================================
//  EQ Anatomy — DSP core (C++ port of the web lab's eq-core.js)
//  Three ways to build the same curve:
//    IIR    : RBJ-cookbook biquads (bilinear transform, prewarped at f0)
//    Analog : minimum-phase FIR following the analog prototype (cepstral method)
//    Linear : linear-phase FIR (frequency sampling + Blackman window)
// =============================================================================
namespace eqcore
{
constexpr int kBands = 8;      // bands 7 and 8 were added in 0.1.1 (parameters appended)
constexpr double kPi  = 3.14159265358979323846;
constexpr double kTau = 2.0 * kPi;

enum class Type { Bell = 0, LowShelf, HighShelf, LowCut, HighCut, Notch, AllPass, Tilt, FlatTilt };   // Tilt / FlatTilt: 0.1.1
enum class Mode { ZeroLatency = 0, Analog, Linear };
// Curve styles. 0/1 are the original Const / Prop Q; the rest copy the *tendencies*
// of well-known analog EQ families (generic names, not models of specific units).
enum class QMode { Constant = 0, Proportional, BritishA, US500, ConsoleE, TubePassive, Precision };
inline const char* curveName (int i)
{
    static const char* n[] = { "Clean Const", "Clean Prop", "British A", "US 500", "Console E", "Tube Passive", "Precision" };
    return n[std::clamp (i, 0, 6)];
}

inline int slopeOrder (int slopeIndex)               // 6,12,18,24,36,48 dB/oct
{
    static const int o[] = { 1, 2, 3, 4, 6, 8 };
    return o[std::clamp (slopeIndex, 0, 5)];
}
inline int linearTaps (int resIndex)                 // low / mid / high
{
    static const int n[] = { 2048, 8192, 32768 };
    return n[std::clamp (resIndex, 0, 2)];
}
constexpr int kMinPhaseLen = 16384;

struct Band { bool on = false; Type type = Type::Bell; double f = 1000, g = 0, q = 1; int slope = 3;
              bool lin = false; int ch = 0; };            // lin: linear-phase band; ch: 0 stereo, 1 mid, 2 side

inline double effectiveQ (const Band& b, QMode qm)
{
    const double g = std::abs (b.g);
    if (b.type == Type::Bell)
        switch (qm)
        {
            case QMode::Proportional: return b.q * std::clamp (0.35 + g / 9.25, 0.3, 2.2);
            case QMode::BritishA:     return b.q * 0.55;                                    // broad, gain-independent bells
            case QMode::US500:        return b.q * std::clamp (0.25 + g / 6.0, 0.25, 3.0);  // strongly proportional
            case QMode::ConsoleE:     return b.q * std::clamp (0.5 + g / 14.0, 0.5, 1.8);   // mildly proportional
            case QMode::TubePassive:  return b.q * (b.g >= 0 ? 0.6 : 0.9);                  // broad boosts
            default:                  return b.q;
        }
    if (b.type == Type::LowShelf || b.type == Type::HighShelf)
        switch (qm)
        {
            case QMode::BritishA:    return std::max (b.q, 1.1);    // shoulder bump / dip
            case QMode::ConsoleE:    return std::max (b.q, 0.9);    // crisp knee
            case QMode::TubePassive: return b.g >= 0 ? 0.55 : 0.4;  // gentle passive shelves
            case QMode::Precision:   return std::min (b.q, 0.7071); // no overshoot
            default:                 return b.q;
        }
    return b.q;
}
// The passive low-cut shelf sits a little higher than the boost (so boost + cut interact)
inline double effectiveF (const Band& b, QMode qm)
{
    if (qm == QMode::TubePassive && b.type == Type::LowShelf && b.g < 0) return b.f * 1.6;
    return b.f;
}

using cplx = std::complex<double>;
// ---- analog sections ----
enum class Sec { Bell, LS, HS, Notch, AP, HP2, LP2, HP1, LP1, LS1, HS1, TiltHS, Gain };   // TiltHS: high shelf / A (−G/2 … +G/2 about f0); Gain: constant A   // LS1 / HS1: first-order shelves (6 dB/oct)
struct Section { Sec k; double A = 1, Q = 0.7071, f0 = 1000; };

// Flat tilt: first-order high shelves half an octave apart; the last ones sit close to 20 kHz.
// Their dB amounts are fitted to a straight 1 dB/oct line (small steps add in dB almost exactly).
inline double flatTiltCentre (int k) { return 10.0 * std::pow (2.0, k * 0.5); }        // 10 Hz … 20.5 kHz (k = 0 … 22)
inline const std::vector<double>& flatTiltWeights()
{
    static const std::vector<double> w = []
    {
        const int N = 23, U = N + 1;                             // + 1 for a free constant
        std::vector<double> A ((size_t) (U * U), 0.0), r ((size_t) U, 0.0);
        for (double fr = 20.0; fr <= 20000.0; fr *= std::pow (2.0, 1.0 / 12.0))
        {
            std::vector<double> row ((size_t) U, 1.0);
            for (int k = 0; k < N; ++k)
            {
                // dB of a 1 dB high shelf (A = 10^(1/40)) at fr
                const double a = std::pow (10.0, 1.0 / 40.0), x = fr / flatTiltCentre (k);
                row[(size_t) k] = 20.0 * std::log10 (std::abs ((std::complex<double> (0, x) * a + 1.0) / (std::complex<double> (0, x) / a + 1.0)));
            }
            const double y = std::log2 (fr / 20.0);
            for (int i = 0; i < U; ++i) { r[(size_t) i] += row[(size_t) i] * y; for (int j = 0; j < U; ++j) A[(size_t) (i * U + j)] += row[(size_t) i] * row[(size_t) j]; }
        }
        for (int i = 0; i < U; ++i) A[(size_t) (i * U + i)] += 1e-4;           // a little ridge keeps neighbours from fighting
        for (int c = 0; c < U; ++c)                                            // Gauss–Jordan
        {
            const double d = A[(size_t) (c * U + c)];
            for (int j = 0; j < U; ++j) A[(size_t) (c * U + j)] /= d; r[(size_t) c] /= d;
            for (int i = 0; i < U; ++i)
                if (i != c) { const double m = A[(size_t) (i * U + c)]; if (m == 0) continue;
                              for (int j = 0; j < U; ++j) A[(size_t) (i * U + j)] -= m * A[(size_t) (c * U + j)]; r[(size_t) i] -= m * r[(size_t) c]; }
        }
        return std::vector<double> (r.begin(), r.begin() + N);
    }();
    return w;
}

inline cplx analogSection (const Section& s, double w);
inline cplx analogSectionFwd (const Section& s, double w) { return analogSection (s, w); }
inline void sectionsFor (const Band& b, QMode qm, std::vector<Section>& out)
{
    const double A = std::pow (10.0, b.g / 40.0), Q = std::max (0.025, effectiveQ (b, qm)), F = effectiveF (b, qm);
    switch (b.type)
    {
        case Type::Bell:      out.push_back ({ Sec::Bell, A, Q, F }); break;
        // shelves: slope index 0 = first order (6 dB/oct, Q has no effect); any other value = the second-order shelf (12 dB/oct, as before)
        case Type::LowShelf:  out.push_back ({ b.slope == 0 ? Sec::LS1 : Sec::LS, A, Q, F }); break;
        case Type::HighShelf: out.push_back ({ b.slope == 0 ? Sec::HS1 : Sec::HS, A, Q, F }); break;
        case Type::Notch:     out.push_back ({ Sec::Notch, 1, Q, b.f }); break;
        case Type::AllPass:   out.push_back ({ Sec::AP, 1, Q, b.f }); break;
        // Tilt shelf: pivot at f (0 dB there), −G/2 below, +G/2 above; Q sets how sharp the turn is
        case Type::Tilt:      out.push_back ({ Sec::TiltHS, A, Q, b.f }); break;
        // Flat tilt: a constant dB/oct line, ±G at 20 Hz / 20 kHz around a 1 kHz centre (slope = 2G over 10 octaves),
        // moved so the response is 0 dB at f. Built from first-order shelves one octave apart (ripple about ±0.15 dB).
        case Type::FlatTilt:
        {
            // per-shelf dB weights for a 1 dB/oct line, fitted once (least squares, 20 Hz – 20 kHz, 1/12 oct grid)
            const auto& W = flatTiltWeights();
            const double slope = b.g / 5.0;                     // dB per octave: ±G at 20 Hz / 20 kHz around 632 Hz
            const size_t first = out.size();
            for (size_t k = 0; k < W.size(); ++k)
                out.push_back ({ Sec::HS1, std::pow (10.0, slope * W[k] / 40.0), 0.7071, flatTiltCentre ((int) k) });
            // normalise: 0 dB at the pivot
            cplx h = 1.0; for (size_t s = first; s < out.size(); ++s) h *= analogSectionFwd (out[s], F / out[s].f0);
            out.push_back ({ Sec::Gain, 1.0 / std::max (1e-9, std::abs (h)), 0.7071, 1000.0 });
        } break;
        case Type::LowCut: case Type::HighCut:
        {
            const int n = slopeOrder (b.slope);
            const double res = b.q / std::sqrt (0.5);
            const bool hp = b.type == Type::LowCut;
            for (int k = 1; k <= n / 2; ++k)
            {
                double qk = 1.0 / (2.0 * std::sin ((2 * k - 1) * kPi / (2.0 * n)));
                if (k == 1) qk *= res;
                out.push_back ({ hp ? Sec::HP2 : Sec::LP2, 1, qk, b.f });
            }
            if (n % 2) out.push_back ({ hp ? Sec::HP1 : Sec::LP1, 1, 0.7071, b.f });
        } break;
    }
}

inline cplx analogSection (const Section& s, double w)   // w = f / f0
{
    const cplx jw (0.0, w), s2 = jw * jw;
    const double A = s.A, Q = s.Q;
    switch (s.k)
    {
        case Sec::Bell:  return (s2 + jw * (A / Q) + 1.0) / (s2 + jw / (A * Q) + 1.0);
        case Sec::LS:  { const double sa = std::sqrt (A); return A * (s2 + jw * (sa / Q) + A) / (A * s2 + jw * (sa / Q) + 1.0); }
        case Sec::HS:  { const double sa = std::sqrt (A); return A * (A * s2 + jw * (sa / Q) + 1.0) / (s2 + jw * (sa / Q) + A); }
        case Sec::Notch: return (s2 + 1.0) / (s2 + jw / Q + 1.0);
        case Sec::AP:    return (s2 - jw / Q + 1.0) / (s2 + jw / Q + 1.0);
        case Sec::HP2:   return s2 / (s2 + jw / Q + 1.0);
        case Sec::LP2:   return 1.0 / (s2 + jw / Q + 1.0);
        case Sec::HP1:   return jw / (1.0 + jw);
        case Sec::LP1:   return 1.0 / (1.0 + jw);
        // first-order shelves, gain A^2 at the shelf, A (half the dB) at f0
        case Sec::LS1:   return (jw + A) / (jw + 1.0 / A);
        case Sec::HS1:   return (A * jw + 1.0) / (jw / A + 1.0);
        case Sec::TiltHS: { const double sa = std::sqrt (A); return (A * s2 + jw * (sa / Q) + 1.0) / (s2 + jw * (sa / Q) + A); }
        case Sec::Gain:  return A;
    }
    return 1.0;
}

struct Biquad { double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0; };

inline Biquad digitalSection (const Section& s, double fs)
{
    const double w0 = kTau * std::min (s.f0, fs * 0.4999) / fs, cw = std::cos (w0), sw = std::sin (w0);
    const double A = s.A, al = sw / (2.0 * s.Q);
    double b0 = 1, b1 = 0, b2 = 0, a0 = 1, a1 = 0, a2 = 0;
    switch (s.k)
    {
        case Sec::Bell:  b0 = 1 + al * A; b1 = -2 * cw; b2 = 1 - al * A; a0 = 1 + al / A; a1 = -2 * cw; a2 = 1 - al / A; break;
        case Sec::LS: { const double sa = 2 * std::sqrt (A) * al;
            b0 = A * ((A + 1) - (A - 1) * cw + sa); b1 = 2 * A * ((A - 1) - (A + 1) * cw); b2 = A * ((A + 1) - (A - 1) * cw - sa);
            a0 = (A + 1) + (A - 1) * cw + sa; a1 = -2 * ((A - 1) + (A + 1) * cw); a2 = (A + 1) + (A - 1) * cw - sa; } break;
        case Sec::Gain: { Biquad c; c.b0 = A; return c; }
        case Sec::TiltHS:
        case Sec::HS: { const double sa = 2 * std::sqrt (A) * al; const double sc = s.k == Sec::TiltHS ? 1.0 / A : 1.0;
            b0 = sc * A * ((A + 1) + (A - 1) * cw + sa); b1 = sc * -2 * A * ((A - 1) + (A + 1) * cw); b2 = sc * A * ((A + 1) + (A - 1) * cw - sa);
            a0 = (A + 1) - (A - 1) * cw + sa; a1 = 2 * ((A - 1) - (A + 1) * cw); a2 = (A + 1) - (A - 1) * cw - sa; } break;
        case Sec::Notch: b0 = 1; b1 = -2 * cw; b2 = 1; a0 = 1 + al; a1 = -2 * cw; a2 = 1 - al; break;
        case Sec::AP:    b0 = 1 - al; b1 = -2 * cw; b2 = 1 + al; a0 = 1 + al; a1 = -2 * cw; a2 = 1 - al; break;
        case Sec::HP2:   b0 = (1 + cw) / 2; b1 = -(1 + cw); b2 = (1 + cw) / 2; a0 = 1 + al; a1 = -2 * cw; a2 = 1 - al; break;
        case Sec::LP2:   b0 = (1 - cw) / 2; b1 = 1 - cw; b2 = (1 - cw) / 2; a0 = 1 + al; a1 = -2 * cw; a2 = 1 - al; break;
        case Sec::HP1: case Sec::LP1:
        {
            const double K = std::tan (w0 / 2), d = 1 + K;
            Biquad c; if (s.k == Sec::HP1) { c.b0 = 1 / d; c.b1 = -1 / d; } else { c.b0 = K / d; c.b1 = K / d; }
            c.a1 = (K - 1) / d; return c;
        }
        case Sec::LS1: case Sec::HS1:
        {
            // bilinear transform of (n1 s + n0) / (d1 s + d0), pre-warped at f0
            const double K = std::tan (w0 / 2);
            const double n1 = s.k == Sec::LS1 ? 1.0 : A, n0 = s.k == Sec::LS1 ? A : 1.0;
            const double d1 = s.k == Sec::LS1 ? 1.0 : 1.0 / A, d0 = s.k == Sec::LS1 ? 1.0 / A : 1.0;
            const double e0 = d1 / K + d0;
            Biquad c; c.b0 = (n1 / K + n0) / e0; c.b1 = (n0 - n1 / K) / e0; c.a1 = (d0 - d1 / K) / e0; return c;
        }
    }
    return { b0 / a0, b1 / a0, b2 / a0, a1 / a0, a2 / a0 };
}

inline cplx biquadResponse (const Biquad& c, double f, double fs)
{
    const double w = kTau * f / fs;
    const cplx z1 = std::polar (1.0, -w), z2 = std::polar (1.0, -2 * w);
    return (c.b0 + c.b1 * z1 + c.b2 * z2) / (1.0 + c.a1 * z1 + c.a2 * z2);
}

// ---- the whole EQ ----
struct Model
{
    std::vector<Section> analog;
    std::vector<Biquad>  digital, allpass;
    double fs = 48000;

    Model() = default;
    // include: which bands to build (default: all that are on)
    template <class Pred>
    Model (const std::array<Band, kBands>& bands, QMode qm, double sampleRate, Pred include) : fs (sampleRate)
    {
        for (int i = 0; i < kBands; ++i)
        {
            const auto& b = bands[(size_t) i];
            if (! b.on || ! include (i, b)) continue;
            std::vector<Section> ss; sectionsFor (b, qm, ss);
            for (auto& s : ss)
            {
                analog.push_back (s);
                const auto d = digitalSection (s, fs);
                digital.push_back (d);
                if (s.k == Sec::AP) allpass.push_back (d);
            }
        }
    }
    Model (const std::array<Band, kBands>& bands, QMode qm, double sampleRate)
        : Model (bands, qm, sampleRate, [] (int, const Band&) { return true; }) {}
    bool empty() const { return analog.empty(); }
    cplx analogResponse (double f) const { cplx h = 1.0; for (auto& s : analog) h *= analogSection (s, f / s.f0); return h; }
    cplx iirResponse (double f) const    { cplx h = 1.0; for (auto& c : digital) h *= biquadResponse (c, f, fs); return h; }
    cplx response (Mode m, double f) const { return m == Mode::ZeroLatency ? iirResponse (f) : analogResponse (f); }
};

// Magnitude (dB), unwrapped phase (deg, centred on the passband) and group delay (ms) at frequencies fr
struct Curves { std::vector<double> mag, phase, gd; };

inline Curves curves (const Model& m, Mode mode, const std::vector<double>& fr)
{
    const size_t n = fr.size(); Curves c; c.mag.resize (n); c.phase.resize (n); c.gd.resize (n);
    double prev = 0, off = 0, best = -1e9;
    std::vector<double> ph (n);
    for (size_t i = 0; i < n; ++i)
    {
        const auto h = m.response (mode, fr[i]);
        c.mag[i] = 20.0 * std::log10 (std::abs (h) + 1e-15);
        double p = std::arg (h);
        if (i) { while (p + off - prev > kPi) off -= kTau; while (p + off - prev < -kPi) off += kTau; }
        ph[i] = prev = p + off;
        best = std::max (best, c.mag[i]);
    }
    std::vector<size_t> pass; for (size_t i = 0; i < n; ++i) if (c.mag[i] > best - 1) pass.push_back (i);
    const double k = pass.empty() ? 0 : std::round (ph[pass[pass.size() / 2]] / kTau);
    for (size_t i = 0; i < n; ++i) ph[i] -= k * kTau;
    for (size_t i = 0; i < n; ++i)
    {
        const size_t a = i ? i - 1 : 0, b = std::min (n - 1, i + 1);
        c.gd[i] = -(ph[b] - ph[a]) / (kTau * (fr[b] - fr[a])) * 1000.0;
        c.phase[i] = ph[i] * 180.0 / kPi;
    }
    if (mode == Mode::Linear) { std::fill (c.phase.begin(), c.phase.end(), 0.0); std::fill (c.gd.begin(), c.gd.end(), 0.0); }
    return c;
}

// ---- FFT (radix-2, in place) ----
inline void fft (std::vector<cplx>& a, bool inverse)
{
    const size_t n = a.size();
    for (size_t i = 1, j = 0; i < n; ++i)
    {
        size_t bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) std::swap (a[i], a[j]);
    }
    for (size_t len = 2; len <= n; len <<= 1)
    {
        const double ang = (inverse ? 1 : -1) * kTau / (double) len;
        const cplx wl = std::polar (1.0, ang);
        for (size_t i = 0; i < n; i += len)
        {
            cplx w = 1.0;
            for (size_t k = 0; k < len / 2; ++k)
            {
                const cplx u = a[i + k], v = a[i + k + len / 2] * w;
                a[i + k] = u + v; a[i + k + len / 2] = u - v; w *= wl;
            }
        }
    }
    if (inverse) for (auto& x : a) x /= (double) n;
}

inline std::vector<double> magnitudeGrid (const Model& m, size_t M)
{
    std::vector<double> g (M / 2 + 1);
    for (size_t k = 0; k <= M / 2; ++k) g[k] = std::abs (m.analogResponse (std::max (k * m.fs / (double) M, 1e-3)));
    return g;
}

inline std::vector<double> linearPhaseFIR (const Model& m, int N)
{
    const size_t M = std::max<size_t> (65536, (size_t) N * 2);
    const auto g = magnitudeGrid (m, M);
    std::vector<cplx> a (M, 0.0);
    for (size_t k = 0; k <= M / 2; ++k) { a[k] = g[k]; if (k && k < M / 2) a[M - k] = g[k]; }
    fft (a, true);
    std::vector<double> h ((size_t) N);
    const int c = N / 2;
    for (int n = 0; n < N; ++n)
    {
        const double w = 0.42 - 0.5 * std::cos (kTau * n / N) + 0.08 * std::cos (2 * kTau * n / N);
        const long idx = ((long) n - c) % (long) M;
        h[(size_t) n] = a[(size_t) (idx < 0 ? idx + (long) M : idx)].real() * w;
    }
    return h;
}

inline std::vector<double> minimumPhaseFIR (const Model& m, int N)
{
    const size_t M = 65536;
    const auto g = magnitudeGrid (m, M);
    std::vector<cplx> a (M, 0.0);
    for (size_t k = 0; k <= M / 2; ++k) { const double v = std::log (std::max (g[k], 1e-12)); a[k] = v; if (k && k < M / 2) a[M - k] = v; }
    fft (a, true);                                  // real cepstrum
    for (size_t n = 1; n < M / 2; ++n) { a[n] = 2.0 * a[n].real(); a[M - n] = 0.0; }
    a[0] = a[0].real(); a[M / 2] = a[M / 2].real();
    fft (a, false);
    for (auto& x : a) x = std::exp (x);
    fft (a, true);
    std::vector<double> h ((size_t) N);
    const int fade = N / 4;
    for (int n = 0; n < N; ++n)
        h[(size_t) n] = a[(size_t) n].real() * (n < N - fade ? 1.0 : 0.5 + 0.5 * std::cos (kPi * (n - (N - fade)) / fade));
    return h;
}

inline void runBiquads (const std::vector<Biquad>& cs, std::vector<double>& h)
{
    for (auto& c : cs)
    {
        double x1 = 0, x2 = 0, y1 = 0, y2 = 0;
        for (auto& v : h) { const double y = c.b0 * v + c.b1 * x1 + c.b2 * x2 - c.a1 * y1 - c.a2 * y2; x2 = x1; x1 = v; y2 = y1; y1 = y; v = y; }
    }
}

struct Impulse { std::vector<double> h; int latency = 0; };

// The impulse response of a mode. FIR modes are what the plugin plays; the IIR one is for display.
inline Impulse impulse (const Model& m, Mode mode, int resIndex)
{
    Impulse r;
    if (mode == Mode::Linear) { const int N = linearTaps (resIndex); r.h = linearPhaseFIR (m, N); r.latency = N / 2; return r; }
    if (mode == Mode::Analog)
    {
        r.h = minimumPhaseFIR (m, kMinPhaseLen);
        if (! m.allpass.empty()) runBiquads (m.allpass, r.h);   // an all-pass has no magnitude: add it back
        return r;
    }
    r.h.assign (kMinPhaseLen, 0.0); r.h[0] = 1.0; runBiquads (m.digital, r.h);
    const int fade = kMinPhaseLen / 4;
    for (int n = kMinPhaseLen - fade; n < kMinPhaseLen; ++n) r.h[(size_t) n] *= 0.5 + 0.5 * std::cos (kPi * (n - (kMinPhaseLen - fade)) / fade);
    return r;
}

struct PreRing { double db = -1e9; double ms = 0; };
inline PreRing preRinging (const std::vector<double>& h, int lat, double fs)
{
    const int guard = (int) std::round (fs * 0.00025);
    double tot = 0, pre = 0, rk = 0;
    for (int i = 0; i < (int) h.size(); ++i)
    {
        const double v = h[(size_t) i] * h[(size_t) i]; tot += v; if (i < lat - guard) pre += v;
        if (std::abs (i - lat) > guard) rk = std::max (rk, std::abs (h[(size_t) i]));
    }
    PreRing p;
    if (pre <= 0 || tot <= 0) return p;
    p.db = 10 * std::log10 (pre / tot);
    if (p.db < -120) { p.db = -1e9; return p; }
    int first = lat;
    for (int i = 0; i < lat - guard; ++i) if (std::abs (h[(size_t) i]) > rk * 0.01) { first = i; break; }
    p.ms = (lat - first) / fs * 1000.0;
    return p;
}

// Loudness-matching gain: mean power over log frequency, 30 Hz – 16 kHz
inline double matchGain (const Model& m, Mode mode)
{
    double s = 0; int n = 0;
    for (int i = 0; i <= 120; ++i) { const double f = 30 * std::pow (16000.0 / 30.0, i / 120.0); s += std::norm (m.response (mode == Mode::ZeroLatency ? Mode::ZeroLatency : Mode::Analog, f)); ++n; }
    return 1.0 / std::sqrt (std::max (1e-12, s / n));
}
// =============================================================================
//  v2: per-band phase (min / linear) and per-band channel (stereo / mid / side)
//  One channel (mid or side) = min-phase bands × linear-phase bands.
//  The linear part only contributes magnitude (its delay is the plugin latency).
// =============================================================================
struct ChannelModel
{
    Model minM, linM; Mode mode = Mode::ZeroLatency;
    cplx response (double f) const
    {
        cplx h = 1.0;
        if (! minM.empty()) h = mode == Mode::ZeroLatency ? minM.iirResponse (f) : minM.analogResponse (f);
        if (! linM.empty()) h *= std::abs (linM.analogResponse (f));
        return h;
    }
    bool hasLin() const { return mode == Mode::Linear || ! linM.empty(); }
};

// ch: 1 = mid, 2 = side. Stereo bands go into both.
inline ChannelModel channelModel (const std::array<Band, kBands>& bands, QMode qm, double fs, Mode mode, int ch)
{
    ChannelModel cm; cm.mode = mode;
    auto inCh = [ch] (const Band& b) { return b.ch == 0 || b.ch == ch; };
    auto isLin = [mode] (const Band& b) { return mode == Mode::Linear || b.lin; };
    cm.minM = Model (bands, qm, fs, [&] (int, const Band& b) { return inCh (b) && ! isLin (b); });
    cm.linM = Model (bands, qm, fs, [&] (int, const Band& b) { return inCh (b) && isLin (b); });
    return cm;
}

// Curves of a channel, plus |H - 1| (what Delta plays) in dB
struct CurvesV2 : Curves { std::vector<double> delta; };
inline CurvesV2 curves (const ChannelModel& m, const std::vector<double>& fr)
{
    const size_t n = fr.size(); CurvesV2 c; c.mag.resize (n); c.phase.resize (n); c.gd.resize (n); c.delta.resize (n);
    double prev = 0, off = 0, best = -1e9;
    std::vector<double> ph (n);
    for (size_t i = 0; i < n; ++i)
    {
        const auto h = m.response (fr[i]);
        c.mag[i] = 20.0 * std::log10 (std::abs (h) + 1e-15);
        c.delta[i] = 20.0 * std::log10 (std::abs (h - 1.0) + 1e-9);
        double p = std::arg (h);
        if (i) { while (p + off - prev > kPi) off -= kTau; while (p + off - prev < -kPi) off += kTau; }
        ph[i] = prev = p + off;
        best = std::max (best, c.mag[i]);
    }
    std::vector<size_t> pass; for (size_t i = 0; i < n; ++i) if (c.mag[i] > best - 1) pass.push_back (i);
    const double k = pass.empty() ? 0 : std::round (ph[pass[pass.size() / 2]] / kTau);
    for (size_t i = 0; i < n; ++i) ph[i] -= k * kTau;
    for (size_t i = 0; i < n; ++i)
    {
        const size_t a = i ? i - 1 : 0, b = std::min (n - 1, i + 1);
        c.gd[i] = -(ph[b] - ph[a]) / (kTau * (fr[b] - fr[a])) * 1000.0;
        c.phase[i] = ph[i] * 180.0 / kPi;
    }
    return c;
}

// Minimum-phase spectrum (complex, size M) for a model's magnitude, via the real cepstrum
inline std::vector<cplx> minimumPhaseSpectrum (const Model& m, size_t M)
{
    std::vector<cplx> a (M, 0.0);
    if (m.empty()) { for (auto& x : a) x = 1.0; return a; }
    const auto g = magnitudeGrid (m, M);
    for (size_t k = 0; k <= M / 2; ++k) { const double v = std::log (std::max (g[k], 1e-12)); a[k] = v; if (k && k < M / 2) a[M - k] = v; }
    fft (a, true);
    for (size_t n = 1; n < M / 2; ++n) { a[n] = 2.0 * a[n].real(); a[M - n] = 0.0; }
    a[0] = a[0].real(); a[M / 2] = a[M / 2].real();
    fft (a, false);
    for (auto& x : a) x = std::exp (x);
    return a;
}

// The FIR the plugin convolves for one channel. Inactive = nothing to convolve.
struct Kernel { std::vector<double> h; int latency = 0; bool active = false; };
inline Kernel buildKernel (const ChannelModel& cm, int resIndex)
{
    Kernel k; const int N = linearTaps (resIndex);
    if (cm.mode == Mode::Linear)
    {
        k.active = true; k.latency = N / 2;
        if (cm.linM.empty()) { k.h.assign ((size_t) N, 0.0); k.h[(size_t) (N / 2)] = 1.0; }
        else k.h = linearPhaseFIR (cm.linM, N);
        return k;
    }
    if (cm.mode == Mode::ZeroLatency)
    {
        if (cm.linM.empty()) return k;
        k.active = true; k.latency = N / 2; k.h = linearPhaseFIR (cm.linM, N); return k;
    }
    // Analog: min-phase FIR of the min bands, times the linear bands' magnitude
    if (cm.minM.empty() && cm.linM.empty()) return k;
    k.active = true;
    if (cm.linM.empty())
    {
        k.h = minimumPhaseFIR (cm.minM, kMinPhaseLen);
        if (! cm.minM.allpass.empty()) runBiquads (cm.minM.allpass, k.h);
        return k;
    }
    const size_t M = std::max<size_t> (65536, (size_t) N * 4);
    auto a = minimumPhaseSpectrum (cm.minM, M);
    const auto g = magnitudeGrid (cm.linM, M);
    const int L = N / 2;
    for (size_t i = 0; i < M; ++i)
    {
        const size_t kk = i <= M / 2 ? i : M - i;
        const double ang = -kTau * (double) i * L / (double) M;
        a[i] *= g[kk] * std::polar (1.0, ang);
    }
    fft (a, true);
    const int T = N + kMinPhaseLen / 2, fade = kMinPhaseLen / 8;
    k.h.resize ((size_t) T);
    for (int n = 0; n < T; ++n)
    {
        double w = 1.0;
        if (n < L) w = 0.42 - 0.5 * std::cos (kPi * n / L) + 0.08 * std::cos (kTau * n / L);   // rising half-Blackman
        else if (n > T - fade) w = 0.5 + 0.5 * std::cos (kPi * (n - (T - fade)) / fade);
        k.h[(size_t) n] = a[(size_t) n].real() * w;
    }
    if (! cm.minM.allpass.empty()) runBiquads (cm.minM.allpass, k.h);
    k.latency = L;
    return k;
}

// What the impulse view shows for a channel (the IIR part rendered on top of the kernel)
inline Impulse impulse (const ChannelModel& cm, const Kernel& k)
{
    Impulse r;
    if (cm.mode != Mode::ZeroLatency) { r.h = k.active ? k.h : std::vector<double> { 1.0 }; r.latency = k.latency; r.h.resize (std::max<size_t> (r.h.size(), 4096), 0.0); return r; }
    const size_t len = (k.active ? k.h.size() : 0) + kMinPhaseLen;
    r.h.assign (len, 0.0);
    if (k.active) std::copy (k.h.begin(), k.h.end(), r.h.begin()); else r.h[0] = 1.0;
    runBiquads (cm.minM.digital, r.h);
    const int fade = kMinPhaseLen / 4, n0 = (int) len - fade;
    for (int n = n0; n < (int) len; ++n) r.h[(size_t) n] *= 0.5 + 0.5 * std::cos (kPi * (n - n0) / fade);
    r.latency = k.latency;
    return r;
}

inline double matchGain (const ChannelModel& m)
{
    double s = 0; int n = 0;
    for (int i = 0; i <= 120; ++i) { const double f = 30 * std::pow (16000.0 / 30.0, i / 120.0); s += std::norm (m.response (f)); ++n; }
    return 1.0 / std::sqrt (std::max (1e-12, s / n));
}
} // namespace eqcore
