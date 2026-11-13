#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <complex>
#include <cmath>
#include <vector>

// =============================================================================
//  Analog-type EQ models (EQ Anatomy Lab 2 1.0).
//  Each model = a set of filters laid out like a classic hardware panel + the
//  saturation character of its circuit. They are generic, educational models:
//  the controls and the way they interact follow the published behaviour of
//  the hardware families named in the help, not any circuit measurement.
//
//  Parameters: an_on, an_model, an_drive, an_out and 16 generic slots
//  an_k1 … an_k16 (0..1). What a slot means depends on the model (Slot table).
// =============================================================================
namespace analog
{
constexpr int kModels = 5, kSlots = 16;

enum Kind { Gain, Freq, Width, Choice, Steps };
struct Slot
{
    const char* label = nullptr;       // panel label (null = unused)
    Kind kind = Gain;
    float lo = 0, hi = 0;              // Gain: ±hi dB / Freq: Hz range (log) / Width: Q range (log)
    std::vector<float> values;         // Choice / Steps: the positions (Hz, dB, or 0/1 for switches)
    std::vector<const char*> names;    // Choice: text for each position (optional)
    float def = 0.5f;                  // default, normalised
    int group = 0;                     // column on the panel
    bool uni = false;                  // Gain: 0 … +hi instead of ±hi
};
struct Model
{
    const char* name; const char* nameJa; const char* family; int satType; float defDrive;
    const char* helpJa; const char* helpEn;
    std::vector<Slot> slots;
    std::vector<const char*> groups;   // column titles
};

inline float norm (const Slot& s, float v01) { return juce::jlimit (0.0f, 1.0f, v01); }
inline int stepIndex (const Slot& s, float v01) { return s.values.empty() ? 0 : juce::jlimit (0, (int) s.values.size() - 1, (int) std::lround (v01 * (float) (s.values.size() - 1))); }
inline float value (const Slot& s, float v01)
{
    v01 = juce::jlimit (0.0f, 1.0f, v01);
    switch (s.kind)
    {
        case Gain:  return s.uni ? v01 * s.hi : (v01 * 2.0f - 1.0f) * s.hi;
        case Freq:
        case Width: return s.lo * std::pow (s.hi / s.lo, v01);
        default:    return s.values.empty() ? 0.0f : s.values[(size_t) stepIndex (s, v01)];
    }
}
inline float toNorm (const Slot& s, float v)
{
    switch (s.kind)
    {
        case Gain:  return juce::jlimit (0.0f, 1.0f, s.uni ? v / s.hi : (v / s.hi + 1.0f) * 0.5f);
        case Freq:
        case Width: return juce::jlimit (0.0f, 1.0f, std::log (v / s.lo) / std::log (s.hi / s.lo));
        default:
        {
            int best = 0; for (int i = 1; i < (int) s.values.size(); ++i) if (std::abs (s.values[(size_t) i] - v) < std::abs (s.values[(size_t) best] - v)) best = i;
            return s.values.size() > 1 ? (float) best / (float) (s.values.size() - 1) : 0.0f;
        }
    }
}
inline juce::String text (const Slot& s, float v01)
{
    const float v = value (s, v01);
    auto hz = [] (float f) { return f >= 1000.0f ? juce::String (f / 1000.0f, f >= 10000.0f ? 0 : 1) + "k" : juce::String ((int) std::lround (f)); };
    switch (s.kind)
    {
        case Gain:  return (v > 0.05f ? "+" : "") + juce::String (v, 1);
        case Freq:  return hz (v);
        case Width: return juce::String (v, 2);
        case Steps: return (v > 0 ? "+" : "") + juce::String ((int) std::lround (v));
        case Choice:
        {
            const int i = stepIndex (s, v01);
            if (i < (int) s.names.size() && s.names[(size_t) i] != nullptr) return juce::String::fromUTF8 (s.names[(size_t) i]);
            return v <= 0 ? juce::String ("Off") : hz (v);
        }
    }
    return {};
}

// ---- the five models ----
inline const std::array<Model, kModels>& models()
{
    auto G = [] (const char* l, float r, int grp) { Slot s; s.label = l; s.kind = Gain; s.hi = r; s.def = 0.5f; s.group = grp; return s; };
    auto F = [] (const char* l, float lo, float hi, float defHz, int grp) { Slot s; s.label = l; s.kind = Freq; s.lo = lo; s.hi = hi; s.def = std::log (defHz / lo) / std::log (hi / lo); s.group = grp; return s; };
    auto W = [] (const char* l, float lo, float hi, float defQ, int grp) { Slot s; s.label = l; s.kind = Width; s.lo = lo; s.hi = hi; s.def = std::log (defQ / lo) / std::log (hi / lo); s.group = grp; return s; };
    auto C = [] (const char* l, std::vector<float> v, std::vector<const char*> n, int defIdx, int grp)
    { Slot s; s.label = l; s.kind = Choice; s.values = std::move (v); s.names = std::move (n); s.def = s.values.size() > 1 ? (float) defIdx / (float) (s.values.size() - 1) : 0.0f; s.group = grp; return s; };
    auto S = [] (const char* l, int grp)
    { Slot s; s.label = l; s.kind = Steps; s.values = { -8, -6, -4, -2, 0, 2, 4, 6, 8 }; s.def = 0.5f; s.group = grp; return s; };

    static const std::array<Model, kModels> m = [&]
    {
        std::array<Model, kModels> a;
        // 0 Passive mastering EQ: four bands, each bell or shelf, wide gain, bandwidth knob; passive gain/Q interaction; tube make-up stage
        a[0] = { "Passive Mastering", "パッシブ・マスタリング", "Manley Massive Passive", 1, 4.0f,
                 "パッシブ回路のEQ（Massive Passive 系の考え方を参考にした汎用モデル）。4バンドすべてがベル⇄シェルフ切替、ゲインは ±20 dB、BANDWIDTH で幅。\n"
                 "| 特徴 | 中身 |\n| パッシブ | 部品だけでカーブを作り、真空管で増幅し直す。ゲインを大きくするほど幅が広がる |\n| シェルフ | BANDWIDTH を広げると、肩に盛り上がりと手前の凹み（Gerzon 型）が出る |\n| 倍音 | 真空管段の2次倍音。DRIVE で量 |\n"
                 "使いどころ：マスターやバスで、広く大きく動かしても自然に聴こえる音作り。",
                 "A passive-circuit EQ (a generic model after the Massive Passive approach). Four bands, each bell or shelf, ±20 dB, BANDWIDTH sets the width.\n"
                 "| Trait | What it means |\n| Passive | the curve is made by parts alone and re-amplified by tubes; more gain = wider |\n| Shelf | wider BANDWIDTH adds the bump and the dip around the corner (Gerzon style) |\n| Harmonics | 2nd-order from the tube stage; DRIVE sets the amount |\n"
                 "Use: broad, large moves on a master or bus that still sound natural.",
                 { G ("GAIN", 20, 0), F ("FREQ", 22, 1000, 68, 0), W ("BW", 0.25f, 4.0f, 0.7f, 0), C ("SHAPE", { 0, 1 }, { "Bell", "Shelf" }, 1, 0),
                   G ("GAIN", 20, 1), F ("FREQ", 47, 2200, 330, 1), W ("BW", 0.25f, 4.0f, 0.7f, 1), C ("SHAPE", { 0, 1 }, { "Bell", "Shelf" }, 0, 1),
                   G ("GAIN", 20, 2), F ("FREQ", 220, 10000, 1500, 2), W ("BW", 0.25f, 4.0f, 0.7f, 2), C ("SHAPE", { 0, 1 }, { "Bell", "Shelf" }, 0, 2),
                   G ("GAIN", 20, 3), F ("FREQ", 560, 20000, 8200, 3), W ("BW", 0.25f, 4.0f, 0.7f, 3), C ("SHAPE", { 0, 1 }, { "Bell", "Shelf" }, 1, 3) },
                 { "BAND 1", "BAND 2", "BAND 3", "BAND 4" } };
        // 1 Precision mastering parametric: three bands, very low colour
        a[1] = { "Precision Parametric", "プレシジョン・パラメトリック", "Sontec MES-432", 4, 0.0f,
                 "精密な真空管なしのマスタリング用パラメトリック（Sontec 系の考え方を参考にした汎用モデル）。LOW・MID・HIGH の3バンド、幅は Q で細かく決める。\n"
                 "| 特徴 | 中身 |\n| 低い色付け | 増幅段は Class A のディスクリート。DRIVE 0 でほぼ無色 |\n| 対称なカーブ | ブーストとカットが同じ形。小さな補正向き |\n| 端の帯域 | LOW と HIGH はベル⇄シェルフ切替 |\n"
                 "使いどころ：マスタリングでの 0.5〜2 dB の精密な補正。",
                 "A precise mastering parametric without tubes (a generic model after the Sontec approach). LOW, MID, HIGH, with Q for the width.\n"
                 "| Trait | What it means |\n| Low colour | discrete class-A gain stage; nearly clean at DRIVE 0 |\n| Symmetric curves | boost and cut have the same shape: good for small fixes |\n| Outer bands | LOW and HIGH switch bell / shelf |\n"
                 "Use: precise 0.5-2 dB fixes in mastering.",
                 { G ("GAIN", 16, 0), F ("FREQ", 16, 500, 80, 0), W ("Q", 0.3f, 4.0f, 0.8f, 0), C ("SHAPE", { 0, 1 }, { "Bell", "Shelf" }, 1, 0),
                   G ("GAIN", 16, 1), F ("FREQ", 100, 6000, 1000, 1), W ("Q", 0.3f, 4.0f, 1.0f, 1),
                   G ("GAIN", 16, 2), F ("FREQ", 1000, 20000, 10000, 2), W ("Q", 0.3f, 4.0f, 0.8f, 2), C ("SHAPE", { 0, 1 }, { "Bell", "Shelf" }, 1, 2) },
                 { "LOW", "MID", "HIGH" } };
        // 2 Air band EQ: fixed wide bands and a very broad "air" lift
        a[2] = { "Air Band", "エア・バンド", "Maag EQ4", 4, 0.0f,
                 "周波数が固定の、幅の広い6バンド（Maag EQ4 系の考え方を参考にした汎用モデル）。右端の AIR は、選んだ周波数から上をなだらかに持ち上げる。\n"
                 "| 特徴 | 中身 |\n| 固定周波数 | SUB 10 Hz・40・160・650・2.5k。どれも幅が広く、重なって滑らかにつながる |\n| AIR | 2.5k〜40k から選ぶ。高い設定ほど可聴域の外から持ち上がり、耳障りにならずに明るくなる |\n| 低い色付け | DRIVE 0 でほぼ無色 |\n"
                 "使いどころ：ボーカルやマスターの「空気感」。AIR を 20k や 40k にして +3〜6 dB。",
                 "Six wide bands at fixed frequencies (a generic model after the Maag EQ4 approach). AIR at the right lifts gently from the chosen frequency up.\n"
                 "| Trait | What it means |\n| Fixed bands | SUB 10 Hz, 40, 160, 650, 2.5k: all wide, they blend smoothly |\n| AIR | 2.5k to 40k: the higher, the more it lifts from beyond hearing, brighter without harshness |\n| Low colour | nearly clean at DRIVE 0 |\n"
                 "Use: 'air' on vocals and masters. AIR at 20k or 40k, +3-6 dB.",
                 { G ("GAIN", 10, 0), G ("GAIN", 10, 1), G ("GAIN", 10, 2), G ("GAIN", 10, 3), G ("GAIN", 10, 4),
                   [&] { Slot s = G ("GAIN", 15, 5); s.uni = true; s.def = 0.0f; return s; }(),
                   C ("AIR FREQ", { 2500, 5000, 10000, 20000, 40000 }, { "2.5k", "5k", "10k", "20k", "40k" }, 3, 5) },
                 { "SUB", "40", "160", "650", "2.5K", "AIR" } };
        // 3 British curve EQ: stepped frequencies, broad / sharp mids, filters, transformer colour
        a[3] = { "British Curve", "ブリティッシュ・カーブ", "Chandler Curve Bender", 3, 6.0f,
                 "ステップ式の周波数と、トランスの太さを持つコンソール系EQ（Curve Bender 系の考え方を参考にした汎用モデル）。BASS・LOW MID・HIGH MID・TREBLE と、ハイパス／ローパス。\n"
                 "| 特徴 | 中身 |\n| ステップ周波数 | 決まった周波数から選ぶ。迷わず速く決まる |\n| BROAD／SHARP | 中域2バンドの幅の切替 |\n| トランス | 低域に厚みと奇数次の倍音。DRIVE で量 |\n"
                 "使いどころ：ミックスバスやドラムを太く、前に。",
                 "A console-style EQ with stepped frequencies and transformer weight (a generic model after the Curve Bender approach). BASS, LOW MID, HIGH MID, TREBLE, plus high-pass / low-pass.\n"
                 "| Trait | What it means |\n| Stepped frequencies | pick from fixed points: quick decisions |\n| BROAD / SHARP | width switch for the two mid bands |\n| Transformer | weight in the lows and odd harmonics; DRIVE sets the amount |\n"
                 "Use: make a mix bus or drums thicker and more forward.",
                 { C ("FREQ", { 20, 35, 60, 100, 150, 220 }, {}, 2, 0), G ("GAIN", 15, 0),
                   C ("FREQ", { 80, 120, 180, 270, 400, 600, 900 }, {}, 3, 1), G ("GAIN", 15, 1), C ("Q", { 0.7f, 2.0f }, { "Broad", "Sharp" }, 0, 1),
                   C ("FREQ", { 800, 1200, 1800, 2700, 4000, 6000, 8000 }, {}, 3, 2), G ("GAIN", 15, 2), C ("Q", { 0.7f, 2.0f }, { "Broad", "Sharp" }, 0, 2),
                   C ("FREQ", { 3000, 5000, 7500, 10000, 15000, 20000 }, {}, 3, 3), G ("GAIN", 15, 3),
                   C ("HPF", { 0, 20, 40, 80, 160, 320 }, {}, 0, 4), C ("LPF", { 0, 20000, 14000, 10000, 7000, 5000 }, {}, 0, 4) },
                 { "BASS", "LOW MID", "HIGH MID", "TREBLE", "FILTER" } };
        // 4 Program 7-band EQ: seven fixed bands, 2 dB steps, tube console colour
        a[4] = { "Program 7-Band", "プログラム7バンド", "Motown / Hitsville EQ", 1, 3.0f,
                 "7つの固定周波数を 2 dB ステップで上げ下げする、60年代スタジオの自作コンソール系EQ（UAD Hitsville EQ のモデリング元になった系統の考え方を参考にした汎用モデル）。\n"
                 "| 特徴 | 中身 |\n| 固定7バンド | 50・130・320・800・2k・5k・12.5k |\n| 2 dB ステップ | ±8 dB を 2 dB 刻み。再現しやすく、迷わない |\n| 幅の広いベル | 隣同士が重なり、全体を太い筆で描くようにトーンを整える |\n| 真空管 | 柔らかな2次倍音 |\n"
                 "使いどころ：ボーカルやベース、ミックス全体のトーンを大きく整える。",
                 "Seven fixed bands in 2 dB steps: the home-built console EQ family of 1960s studios (a generic model after the family that UAD's Hitsville EQ models).\n"
                 "| Trait | What it means |\n| Seven fixed bands | 50, 130, 320, 800, 2k, 5k, 12.5k |\n| 2 dB steps | ±8 dB in 2 dB steps: repeatable, quick |\n| Wide bells | neighbours overlap: tone shaping with a broad brush |\n| Tubes | soft 2nd-order harmonics |\n"
                 "Use: broad tone shaping of vocals, bass or the whole mix.",
                 { S ("dB", 0), S ("dB", 1), S ("dB", 2), S ("dB", 3), S ("dB", 4), S ("dB", 5), S ("dB", 6) },
                 { "50", "130", "320", "800", "2K", "5K", "12.5K" } };
        return a;
    }();
    return m;
}

// ---- biquads (RBJ cookbook) ----
struct Biquad
{
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    double z1[2] {}, z2[2] {};
    inline float process (int c, float x) { const double y = b0 * x + z1[c]; z1[c] = b1 * x - a1 * y + z2[c]; z2[c] = b2 * x - a2 * y; return (float) y; }
    void reset() { z1[0] = z1[1] = z2[0] = z2[1] = 0; }
    double magDb (double f, double fs) const
    {
        const double w = 2.0 * juce::MathConstants<double>::pi * f / fs;
        const std::complex<double> z1c = std::polar (1.0, -w), z2c = std::polar (1.0, -2.0 * w);
        const auto h = (b0 + b1 * z1c + b2 * z2c) / (1.0 + a1 * z1c + a2 * z2c);
        return 20.0 * std::log10 (std::max (1e-9, std::abs (h)));
    }
    void set (double nb0, double nb1, double nb2, double a0, double na1, double na2) { b0 = nb0 / a0; b1 = nb1 / a0; b2 = nb2 / a0; a1 = na1 / a0; a2 = na2 / a0; }
    static double w0 (double f, double fs) { return 2.0 * juce::MathConstants<double>::pi * juce::jlimit (5.0, fs * 0.49, f) / fs; }
    void peak (double f, double q, double db, double fs)
    {
        const double A = std::pow (10.0, db / 40.0), w = w0 (f, fs), al = std::sin (w) / (2.0 * q), c = std::cos (w);
        set (1 + al * A, -2 * c, 1 - al * A, 1 + al / A, -2 * c, 1 - al / A);
    }
    void lowShelf (double f, double q, double db, double fs)
    {
        const double A = std::pow (10.0, db / 40.0), w = w0 (f, fs), al = std::sin (w) / (2.0 * q), c = std::cos (w), s = 2.0 * std::sqrt (A) * al;
        set (A * ((A + 1) - (A - 1) * c + s), 2 * A * ((A - 1) - (A + 1) * c), A * ((A + 1) - (A - 1) * c - s),
             (A + 1) + (A - 1) * c + s, -2 * ((A - 1) + (A + 1) * c), (A + 1) + (A - 1) * c - s);
    }
    void highShelf (double f, double q, double db, double fs)
    {
        const double A = std::pow (10.0, db / 40.0), w = w0 (f, fs), al = std::sin (w) / (2.0 * q), c = std::cos (w), s = 2.0 * std::sqrt (A) * al;
        set (A * ((A + 1) + (A - 1) * c + s), -2 * A * ((A - 1) + (A + 1) * c), A * ((A + 1) + (A - 1) * c - s),
             (A + 1) - (A - 1) * c + s, 2 * ((A - 1) - (A + 1) * c), (A + 1) - (A - 1) * c - s);
    }
    void highPass (double f, double q, double fs) { const double w = w0 (f, fs), al = std::sin (w) / (2 * q), c = std::cos (w); set ((1 + c) / 2, -(1 + c), (1 + c) / 2, 1 + al, -2 * c, 1 - al); }
    void lowPass (double f, double q, double fs)  { const double w = w0 (f, fs), al = std::sin (w) / (2 * q), c = std::cos (w); set ((1 - c) / 2, 1 - c, (1 - c) / 2, 1 + al, -2 * c, 1 - al); }
    void flat() { b0 = 1; b1 = b2 = a1 = a2 = 0; }
};

// The filter design of a model for a set of slot values (shared by the audio and the display)
inline int design (int model, const float* k, double fs, std::array<Biquad, 16>& out)
{
    const auto& M = models()[(size_t) juce::jlimit (0, kModels - 1, model)];
    auto v = [&] (int i) { return i < (int) M.slots.size() ? value (M.slots[(size_t) i], k[i]) : 0.0f; };
    int n = 0;
    auto add = [&] () -> Biquad& { auto& b = out[(size_t) n++]; b.flat(); return b; };
    switch (model)
    {
        case 0:   // passive: bandwidth narrows as the gain falls (wider for big moves), shelves can bump (Q > 0.7)
            for (int b = 0; b < 4; ++b)
            {
                const double g = v (b * 4), f = v (b * 4 + 1), bw = v (b * 4 + 2); const bool shelf = v (b * 4 + 3) > 0.5f;
                if (std::abs (g) < 0.01) continue;
                const double q = bw * (1.0 - 0.35 * std::min (1.0, std::abs (g) / 20.0));
                auto& bq = add();
                if (shelf && b <= 1) bq.lowShelf (f, std::max (0.3, q), g, fs);
                else if (shelf) bq.highShelf (f, std::max (0.3, q), g, fs);
                else bq.peak (f, std::max (0.15, q), g, fs);
            }
            break;
        case 1:   // precision: symmetric parametric
        {
            auto band = [&] (int gi, int fi, int qi, int si, bool low)
            {
                const double g = v (gi); if (std::abs (g) < 0.01) return;
                auto& bq = add();
                const bool shelf = si >= 0 && v (si) > 0.5f;
                if (shelf) { if (low) bq.lowShelf (v (fi), std::min (1.2, (double) v (qi)), g, fs); else bq.highShelf (v (fi), std::min (1.2, (double) v (qi)), g, fs); }
                else bq.peak (v (fi), v (qi), g, fs);
            };
            band (0, 1, 2, 3, true); band (4, 5, 6, -1, false); band (7, 8, 9, 10, false);
            break;
        }
        case 2:   // air band: wide fixed bells, air = very gentle high shelf from (freq / 2), capped below Nyquist
        {
            const double fq[] = { 10, 40, 160, 650, 2500 };
            for (int b = 0; b < 5; ++b) { const double g = v (b); if (std::abs (g) > 0.01) add().peak (fq[b], b == 0 ? 0.5 : 0.55, g, fs); }
            const double air = std::max (0.0f, v (5));
            if (air > 0.01)
            {
                const double fa = v (6);
                const double corner = std::min (fs * 0.42, fa * 0.5);
                const double amount = fa > fs * 0.5 ? air * (0.45 + 0.55 * std::min (1.0, fs * 0.5 / fa)) : air;   // beyond Nyquist only its lower slope is heard
                add().highShelf (corner, 0.35, amount, fs);
            }
            break;
        }
        case 3:   // british curve
        {
            if (std::abs (v (1)) > 0.01) add().lowShelf (v (0), 0.6, v (1), fs);
            if (std::abs (v (3)) > 0.01) add().peak (v (2), v (4), v (3), fs);
            if (std::abs (v (6)) > 0.01) add().peak (v (5), v (7), v (6), fs);
            if (std::abs (v (9)) > 0.01) add().highShelf (v (8), 0.6, v (9), fs);
            if (v (10) > 1) { add().highPass (v (10), 0.54, fs); add().highPass (v (10), 1.31, fs); }   // 24 dB/oct
            if (v (11) > 1) add().lowPass (v (11), 0.707, fs);                                          // 12 dB/oct
            break;
        }
        case 4:   // program 7-band: wide overlapping bells
        {
            const double fq[] = { 50, 130, 320, 800, 2000, 5000, 12500 };
            for (int b = 0; b < 7; ++b) { const double g = v (b); if (std::abs (g) > 0.01) add().peak (fq[b], 0.9, g, fs); }
            break;
        }
        default: break;
    }
    return n;
}

// ---- the processor: filters, then the model's saturation (input-scaled by DRIVE, gain compensated) ----
class Processor
{
public:
    void prepare (double sampleRate) { fs = sampleRate; reset(); lastSig = -1; }
    void reset() { for (auto& b : bq) b.reset(); dc[0] = dc[1] = dy[0] = dy[1] = 0; }
    void set (int model, const float* k, float driveDb, float outDb)
    {
        double sig = model * 1000.0 + driveDb * 7.0 + outDb * 13.0; for (int i = 0; i < kSlots; ++i) sig += k[i] * (i + 3) * 17.0;
        if (sig != lastSig || model != curModel)
        {
            if (model != curModel) reset();
            curModel = model; n = design (model, k, fs, bq); lastSig = sig;
        }
        sat = models()[(size_t) juce::jlimit (0, kModels - 1, model)].satType;
        drive = juce::Decibels::decibelsToGain (driveDb - 12.0f);    // 0 dB knob = well below the knee
        out = juce::Decibels::decibelsToGain (outDb);
        const double b = sat == 1 ? 0.25 : 0.0;
        slope0 = sat == 1 ? 1.0 - std::tanh (b) * std::tanh (b) : 1.0;
        bias = b;
    }
    void process (float* L, float* R, int num)
    {
        float* io[2] = { L, R };
        const double dcA = 1.0 - 2.0 * juce::MathConstants<double>::pi * 8.0 / fs;
        for (int c = 0; c < 2; ++c)
        {
            float* d = io[c];
            for (int i = 0; i < num; ++i)
            {
                float x = d[i];
                for (int s = 0; s < n; ++s) x = bq[(size_t) s].process (c, x);
                const double u = x * drive;
                double y;
                switch (sat)
                {
                    case 1:  y = (std::tanh (u + bias) - std::tanh (bias)) / slope0; break;          // tube: asymmetric, 2nd order
                    case 3:  y = std::tanh (u) + 0.08 * u * u * u / (1.0 + u * u * u * u); break;   // transformer: soft, odd order
                    default: { const double t = std::tanh (u); y = t + 0.06 * t * t; break; }        // class A: gentle
                }
                y /= drive;
                if (sat == 1) { const double hp = y - dc[c] + dcA * dy[c]; dc[c] = y; dy[c] = hp; y = hp; }   // DC blocker for the tube's asymmetry
                d[i] = (float) (y * out);
            }
        }
    }
    // magnitude of the filters only (for the graph)
    static void response (int model, const float* k, double fs, const std::vector<double>& freqs, std::vector<double>& db)
    {
        std::array<Biquad, 16> b; const int n = design (model, k, fs, b);
        db.assign (freqs.size(), 0.0);
        for (size_t i = 0; i < freqs.size(); ++i) for (int s = 0; s < n; ++s) db[i] += b[(size_t) s].magDb (freqs[i], fs);
    }
private:
    double fs = 48000, lastSig = -1, drive = 1, out = 1, slope0 = 1, bias = 0;
    double dc[2] {}, dy[2] {};
    int curModel = -1, n = 0, sat = 4;
    std::array<Biquad, 16> bq;
};
}
