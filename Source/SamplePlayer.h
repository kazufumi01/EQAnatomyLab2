#pragma once
#include <cmath>
#include <cstdint>

// =============================================================================
//  Built-in practice material for Learn mode (Lab 2): plays when no track feeds the plugin.
//  Everything is synthesised (no files): a 2-bar loop at 92 BPM.
//    Drums  : kick (pitch-swept sine + click), snare (noise + 190 Hz body), closed hats on eighths (slightly right)
//    Voice  : a sawtooth with vibrato through three vowel formants ("a": 730 / 1090 / 2440 Hz)
//             plus a deliberate narrow resonance at 3.15 kHz (+14 dB, Q 12): something for the EQ and RES to find
//  Source: 0 drums, 1 voice, 2 both.
// =============================================================================
class SamplePlayer
{
public:
    void prepare (double sampleRate)
    {
        fs = sampleRate;
        stepLen = fs * 60.0 / 92.0 / 4.0;
        bp[0].bandpass (730, 7, fs);  bp[1].bandpass (1090, 8, fs); bp[2].bandpass (2440, 10, fs);
        res.peak (3150, 12, 14.0, fs);
        reset();
    }
    void reset() { pos = 0; kickT = snT = hatT = 1e9; noteT = 0; ph = 0; vibPh = 0; for (auto& b : bp) b.clear(); res.clear(); hpN = hpH = 0; lastNote = -1; }
    void render (float* l, float* r, int n, int source)
    {
        static const int kick[] = { 0, 10, 16, 26 }, snare[] = { 8, 24 };
        static const int notes[] = { 64, 67, 69, 67, 64, 62, 60, 62 };      // one per beat, 2 bars
        for (int i = 0; i < n; ++i)
        {
            const double t = pos / stepLen;
            const int step = (int) t % 32;
            const bool onStep = std::fmod (t, 1.0) < 1.0 / stepLen;
            if (onStep)
            {
                for (int k : kick) if (k == step) kickT = 0;
                for (int s : snare) if (s == step) snT = 0;
                if (step % 2 == 0) hatT = 0;
                if (step % 4 == 0) { noteT = 0; lastNote = notes[step / 4]; }
            }
            double dl = 0, dr = 0;
            if (source != 1)
            {
                // kick
                if (kickT < 0.5)
                {
                    const double f = 45.0 + 75.0 * std::exp (-kickT / 0.03);
                    kickPh += f / fs;
                    const double k = std::sin (2 * juce::MathConstants<double>::pi * kickPh) * std::exp (-kickT / 0.25) * 0.55 + (kickT < 0.002 ? 0.3 * (1 - kickT / 0.002) : 0.0);
                    dl += k; dr += k; kickT += 1.0 / fs;
                } else kickPh = 0;
                const double nz = noise();
                // snare
                if (snT < 0.4)
                {
                    hpN += 0.35 * (nz - hpN);
                    const double s = (nz - hpN) * std::exp (-snT / 0.15) * 0.35 + std::sin (2 * juce::MathConstants<double>::pi * 190.0 * snT) * std::exp (-snT / 0.08) * 0.25;
                    dl += s; dr += s; snT += 1.0 / fs;
                }
                // hats
                if (hatT < 0.1)
                {
                    hpH += 0.7 * (nz - hpH);
                    const double h = (nz - hpH) * std::exp (-hatT / 0.035) * 0.16;
                    dl += h * 0.75; dr += h; hatT += 1.0 / fs;
                }
            }
            if (source != 0 && lastNote >= 0)
            {
                const double vib = noteT > 0.15 ? 0.15 * std::sin (2 * juce::MathConstants<double>::pi * vibPh) : 0.0;     // ±15 cents after 150 ms
                vibPh += 5.5 / fs;
                const double f = 440.0 * std::pow (2.0, (lastNote - 69 + vib * 0.1) / 12.0);
                const double dt = f / fs;
                ph += dt; if (ph >= 1) ph -= 1;
                double saw = 2 * ph - 1;                                                         // polyBLEP sawtooth
                if (ph < dt) { const double x = ph / dt; saw -= x + x - x * x - 1; }
                else if (ph > 1 - dt) { const double x = (ph - 1) / dt; saw -= x * x + x + x + 1; }
                const double env = std::min (1.0, noteT / 0.02) * (noteT > stepLen * 4 / fs - 0.06 ? std::max (0.0, (stepLen * 4 / fs - noteT) / 0.06) : 1.0);
                const double v = bp[0].process (saw) * 1.0 + bp[1].process (saw) * 0.55 + bp[2].process (saw) * 0.3;
                const double o = res.process (v) * env * 0.9;
                dl += o; dr += o; noteT += 1.0 / fs;
            }
            l[i] = (float) dl; if (r != nullptr) r[i] = (float) dr; else l[i] = (float) (0.5 * (dl + dr));
            pos += 1.0; if (pos >= stepLen * 32) pos -= stepLen * 32;
        }
    }
private:
    struct Biquad
    {
        double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
        void bandpass (double f, double q, double fs) { const double w = 2 * juce::MathConstants<double>::pi * f / fs, al = std::sin (w) / (2 * q), a0 = 1 + al; b0 = al / a0; b1 = 0; b2 = -al / a0; a1 = -2 * std::cos (w) / a0; a2 = (1 - al) / a0; }
        void peak (double f, double q, double db, double fs)
        {
            const double A = std::pow (10.0, db / 40.0), w = 2 * juce::MathConstants<double>::pi * f / fs, al = std::sin (w) / (2 * q), a0 = 1 + al / A;
            b0 = (1 + al * A) / a0; b1 = -2 * std::cos (w) / a0; b2 = (1 - al * A) / a0; a1 = b1; a2 = (1 - al / A) / a0;
        }
        double process (double x) { const double y = b0 * x + z1; z1 = b1 * x - a1 * y + z2; z2 = b2 * x - a2 * y; return y; }
        void clear() { z1 = z2 = 0; }
    };
    double noise() { seed = seed * 1664525u + 1013904223u; return (double) (seed >> 8) / 8388608.0 - 1.0; }
    double fs = 48000, stepLen = 7826, pos = 0, kickT = 1e9, snT = 1e9, hatT = 1e9, noteT = 0, ph = 0, vibPh = 0, kickPh = 0, hpN = 0, hpH = 0;
    int lastNote = -1; std::uint32_t seed = 12345u;
    Biquad bp[3], res;
};
