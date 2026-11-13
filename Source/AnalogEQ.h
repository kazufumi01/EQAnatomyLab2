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
constexpr int kModels = 9, kSlots = 16;

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
        case Gain:  return s.uni && s.hi == 10.0f ? juce::String (v, 1) : (v > 0.05f ? "+" : "") + juce::String (v, 1);
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

// ---- the models (names hint at the era, the circuit and the job, not at any product) ----
inline const std::array<Model, kModels>& models()
{
    auto G = [] (const char* l, float r, int grp) { Slot s; s.label = l; s.kind = Gain; s.hi = r; s.def = 0.5f; s.group = grp; return s; };
    auto F = [] (const char* l, float lo, float hi, float defHz, int grp) { Slot s; s.label = l; s.kind = Freq; s.lo = lo; s.hi = hi; s.def = std::log (defHz / lo) / std::log (hi / lo); s.group = grp; return s; };
    auto W = [] (const char* l, float lo, float hi, float defQ, int grp) { Slot s; s.label = l; s.kind = Width; s.lo = lo; s.hi = hi; s.def = std::log (defQ / lo) / std::log (hi / lo); s.group = grp; return s; };
    auto C = [] (const char* l, std::vector<float> v, std::vector<const char*> n, int defIdx, int grp)
    { Slot s; s.label = l; s.kind = Choice; s.values = std::move (v); s.names = std::move (n); s.def = s.values.size() > 1 ? (float) defIdx / (float) (s.values.size() - 1) : 0.0f; s.group = grp; return s; };
    auto S = [] (const char* l, int grp)
    { Slot s; s.label = l; s.kind = Steps; s.values = { -8, -6, -4, -2, 0, 2, 4, 6, 8 }; s.def = 0.5f; s.group = grp; return s; };
    auto S2 = [] (const char* l, std::vector<float> v, int grp)
    { Slot s; s.label = l; s.kind = Steps; s.values = std::move (v); s.def = 0.5f; s.group = grp; return s; };
    auto D = [] (const char* l, int grp)       // a 0-10 dial (the circuit decides how many dB that is)
    { Slot s; s.label = l; s.kind = Gain; s.uni = true; s.hi = 10.0f; s.def = 0.0f; s.group = grp; return s; };

    static const std::array<Model, kModels> m = [&]
    {
        std::array<Model, kModels> a;
        // 0 Passive mastering EQ: four bands, each bell or shelf, wide gain, bandwidth knob; passive gain/Q interaction; tube make-up stage
        a[0] = { "Passive Tube Master", "真空管パッシブ・マスター", "passive 4-band mastering EQ with a tube make-up stage", 1, 4.0f,
                 "パッシブ回路のEQ（学習用の汎用モデル）。4バンドすべてがベル⇄シェルフ切替、ゲインは ±20 dB、BANDWIDTH で幅。\n"
                 "| 特徴 | 中身 |\n| パッシブ | 部品だけでカーブを作り、真空管で増幅し直す。ゲインを大きくするほど幅が広がる |\n| シェルフ | BANDWIDTH を広げると、肩に盛り上がりと手前の凹み（Gerzon 型）が出る |\n| 倍音 | 真空管段の2次倍音。DRIVE で量 |\n"
                 "使いどころ：マスターやバスで、広く大きく動かしても自然に聴こえる音作り。",
                 "A passive-circuit EQ (a generic learning model). Four bands, each bell or shelf, ±20 dB, BANDWIDTH sets the width.\n"
                 "| Trait | What it means |\n| Passive | the curve is made by parts alone and re-amplified by tubes; more gain = wider |\n| Shelf | wider BANDWIDTH adds the bump and the dip around the corner (Gerzon style) |\n| Harmonics | 2nd-order from the tube stage; DRIVE sets the amount |\n"
                 "Use: broad, large moves on a master or bus that still sound natural.",
                 { G ("GAIN", 20, 0), F ("FREQ", 22, 1000, 68, 0), W ("BW", 0.25f, 4.0f, 0.7f, 0), C ("SHAPE", { 0, 1 }, { "Bell", "Shelf" }, 1, 0),
                   G ("GAIN", 20, 1), F ("FREQ", 47, 2200, 330, 1), W ("BW", 0.25f, 4.0f, 0.7f, 1), C ("SHAPE", { 0, 1 }, { "Bell", "Shelf" }, 0, 1),
                   G ("GAIN", 20, 2), F ("FREQ", 220, 10000, 1500, 2), W ("BW", 0.25f, 4.0f, 0.7f, 2), C ("SHAPE", { 0, 1 }, { "Bell", "Shelf" }, 0, 2),
                   G ("GAIN", 20, 3), F ("FREQ", 560, 20000, 8200, 3), W ("BW", 0.25f, 4.0f, 0.7f, 3), C ("SHAPE", { 0, 1 }, { "Bell", "Shelf" }, 1, 3) },
                 { "BAND 1", "BAND 2", "BAND 3", "BAND 4" } };
        // 1 Precision mastering parametric: three bands, very low colour
        a[1] = { "Precision Master 3-Band", "精密マスタリング3バンド", "clean, exact solid-state mastering parametric", 4, 0.0f,
                 "精密な真空管なしのマスタリング用パラメトリック（学習用の汎用モデル）。LOW・MID・HIGH の3バンド、幅は Q で細かく決める。\n"
                 "| 特徴 | 中身 |\n| 低い色付け | 増幅段は Class A のディスクリート。DRIVE 0 でほぼ無色 |\n| 対称なカーブ | ブーストとカットが同じ形。小さな補正向き |\n| 端の帯域 | LOW と HIGH はベル⇄シェルフ切替 |\n"
                 "使いどころ：マスタリングでの 0.5〜2 dB の精密な補正。",
                 "A precise mastering parametric without tubes (a generic learning model). LOW, MID, HIGH, with Q for the width.\n"
                 "| Trait | What it means |\n| Low colour | discrete class-A gain stage; nearly clean at DRIVE 0 |\n| Symmetric curves | boost and cut have the same shape: good for small fixes |\n| Outer bands | LOW and HIGH switch bell / shelf |\n"
                 "Use: precise 0.5-2 dB fixes in mastering.",
                 { G ("GAIN", 16, 0), F ("FREQ", 16, 500, 80, 0), W ("Q", 0.3f, 4.0f, 0.8f, 0), C ("SHAPE", { 0, 1 }, { "Bell", "Shelf" }, 1, 0),
                   G ("GAIN", 16, 1), F ("FREQ", 100, 6000, 1000, 1), W ("Q", 0.3f, 4.0f, 1.0f, 1),
                   G ("GAIN", 16, 2), F ("FREQ", 1000, 20000, 10000, 2), W ("Q", 0.3f, 4.0f, 0.8f, 2), C ("SHAPE", { 0, 1 }, { "Bell", "Shelf" }, 1, 2) },
                 { "LOW", "MID", "HIGH" } };
        // 2 Air band EQ: fixed wide bands and a very broad "air" lift
        a[2] = { "Fixed-Band Air Lift", "固定バンド・エアリフト", "six wide fixed bands plus an ultra-high AIR shelf", 4, 0.0f,
                 "周波数が固定の、幅の広い6バンド（学習用の汎用モデル）。右端の AIR は、選んだ周波数から上をなだらかに持ち上げる。\n"
                 "| 特徴 | 中身 |\n| 固定周波数 | SUB 10 Hz・40・160・650・2.5k。どれも幅が広く、重なって滑らかにつながる |\n| AIR | 2.5k〜40k から選ぶ。高い設定ほど可聴域の外から持ち上がり、耳障りにならずに明るくなる |\n| 低い色付け | DRIVE 0 でほぼ無色 |\n"
                 "使いどころ：ボーカルやマスターの「空気感」。AIR を 20k や 40k にして +3〜6 dB。",
                 "Six wide bands at fixed frequencies (a generic learning model). AIR at the right lifts gently from the chosen frequency up.\n"
                 "| Trait | What it means |\n| Fixed bands | SUB 10 Hz, 40, 160, 650, 2.5k: all wide, they blend smoothly |\n| AIR | 2.5k to 40k: the higher, the more it lifts from beyond hearing, brighter without harshness |\n| Low colour | nearly clean at DRIVE 0 |\n"
                 "Use: 'air' on vocals and masters. AIR at 20k or 40k, +3-6 dB.",
                 { G ("GAIN", 10, 0), G ("GAIN", 10, 1), G ("GAIN", 10, 2), G ("GAIN", 10, 3), G ("GAIN", 10, 4),
                   [&] { Slot s = G ("GAIN", 15, 5); s.uni = true; s.def = 0.0f; return s; }(),
                   C ("AIR FREQ", { 2500, 5000, 10000, 20000, 40000 }, { "2.5k", "5k", "10k", "20k", "40k" }, 3, 5) },
                 { "SUB", "40", "160", "650", "2.5K", "AIR" } };
        // 3 British curve EQ: stepped frequencies, broad / sharp mids, filters, transformer colour
        a[3] = { "British Stepped Bus", "英国ステップ・バスEQ", "stepped British console curves for the mix bus and drums", 3, 6.0f,
                 "ステップ式の周波数と、トランスの太さを持つコンソール系EQ（学習用の汎用モデル）。BASS・LOW MID・HIGH MID・TREBLE と、ハイパス／ローパス。\n"
                 "| 特徴 | 中身 |\n| ステップ周波数 | 決まった周波数から選ぶ。迷わず速く決まる |\n| BROAD／SHARP | 中域2バンドの幅の切替 |\n| トランス | 低域に厚みと奇数次の倍音。DRIVE で量 |\n"
                 "使いどころ：ミックスバスやドラムを太く、前に。",
                 "A console-style EQ with stepped frequencies and transformer weight (a generic learning model). BASS, LOW MID, HIGH MID, TREBLE, plus high-pass / low-pass.\n"
                 "| Trait | What it means |\n| Stepped frequencies | pick from fixed points: quick decisions |\n| BROAD / SHARP | width switch for the two mid bands |\n| Transformer | weight in the lows and odd harmonics; DRIVE sets the amount |\n"
                 "Use: make a mix bus or drums thicker and more forward.",
                 { C ("FREQ", { 20, 35, 60, 100, 150, 220 }, {}, 2, 0), G ("GAIN", 15, 0),
                   C ("FREQ", { 80, 120, 180, 270, 400, 600, 900 }, {}, 3, 1), G ("GAIN", 15, 1), C ("Q", { 0.7f, 2.0f }, { "Broad", "Sharp" }, 0, 1),
                   C ("FREQ", { 800, 1200, 1800, 2700, 4000, 6000, 8000 }, {}, 3, 2), G ("GAIN", 15, 2), C ("Q", { 0.7f, 2.0f }, { "Broad", "Sharp" }, 0, 2),
                   C ("FREQ", { 3000, 5000, 7500, 10000, 15000, 20000 }, {}, 3, 3), G ("GAIN", 15, 3),
                   C ("HPF", { 0, 20, 40, 80, 160, 320 }, {}, 0, 4), C ("LPF", { 0, 20000, 14000, 10000, 7000, 5000 }, {}, 0, 4) },
                 { "BASS", "LOW MID", "HIGH MID", "TREBLE", "FILTER" } };
        // 4 Program 7-band EQ: seven fixed bands, 2 dB steps, tube console colour
        a[4] = { "60s Studio 7-Band", "60年代スタジオ7バンド", "a 1960s studio's home-built console EQ, 2 dB steps", 1, 3.0f,
                 "7つの固定周波数を 2 dB ステップで上げ下げする、60年代スタジオの自作コンソール系EQ（学習用の汎用モデル）。\n"
                 "| 特徴 | 中身 |\n| 固定7バンド | 50・130・320・800・2k・5k・12.5k |\n| 2 dB ステップ | ±8 dB を 2 dB 刻み。再現しやすく、迷わない |\n| 幅の広いベル | 隣同士が重なり、全体を太い筆で描くようにトーンを整える |\n| 真空管 | 柔らかな2次倍音 |\n"
                 "使いどころ：ボーカルやベース、ミックス全体のトーンを大きく整える。",
                 "Seven fixed bands in 2 dB steps: the home-built console EQ family of 1960s studios (a generic learning model).\n"
                 "| Trait | What it means |\n| Seven fixed bands | 50, 130, 320, 800, 2k, 5k, 12.5k |\n| 2 dB steps | ±8 dB in 2 dB steps: repeatable, quick |\n| Wide bells | neighbours overlap: tone shaping with a broad brush |\n| Tubes | soft 2nd-order harmonics |\n"
                 "Use: broad tone shaping of vocals, bass or the whole mix.",
                 { S ("dB", 0), S ("dB", 1), S ("dB", 2), S ("dB", 3), S ("dB", 4), S ("dB", 5), S ("dB", 6) },
                 { "50", "130", "320", "800", "2K", "5K", "12.5K" } };
        // 5 Tube program EQ: low boost and low attenuation on the same frequency, high boost with bandwidth, high attenuation
        a[5] = { "Vintage Tube Boost/Atten", "ヴィンテージ真空管ブースト／アッテン", "a 1950s-60s tube program EQ: boost and attenuate the same lows", 1, 5.0f,
                 "真空管のプログラムEQ（学習用の汎用モデル）。低域は BOOST と ATTEN が同じ周波数のつまみで、同時に回せる。\n"
                 "| 特徴 | 中身 |\n| 低域の同時操作 | BOOST はシェルフ、ATTEN はそれより少し上から効く。両方上げると、低域が持ち上がり、その少し上が凹む（有名な使い方） |\n| 高域 BOOST | ベル。BANDWIDTH で幅（Sharp⇄Broad） |\n| 高域 ATTEN | 5k・10k・20k から下げるシェルフ |\n| 目盛り | 0〜10。dB ではない（回路で決まる） |\n"
                 "使いどころ：キックやベースの「太いのに濁らない低域」、ボーカルの柔らかい空気感。",
                 "A tube program EQ (a generic learning model). In the lows, BOOST and ATTEN share one frequency and can be used together.\n"
                 "| Trait | What it means |\n| Lows together | BOOST is a shelf, ATTEN starts a little higher: both up = more lows with a dip just above (the famous trick) |\n| High BOOST | a bell; BANDWIDTH from sharp to broad |\n| High ATTEN | a shelf down from 5k, 10k or 20k |\n| Scale | 0-10 dials, not dB (the circuit decides) |\n"
                 "Use: big lows that stay clear on kick and bass, soft air on vocals.",
                 { C ("CPS", { 20, 30, 60, 100 }, {}, 2, 0), D ("BOOST", 0), D ("ATTEN", 0),
                   C ("KCS", { 3000, 4000, 5000, 8000, 10000, 12000, 16000 }, {}, 4, 1), D ("BOOST", 1), W ("BANDWIDTH", 0.5f, 2.5f, 1.0f, 1),
                   C ("ATTEN SEL", { 5000, 10000, 20000 }, { "5k", "10k", "20k" }, 1, 2), D ("ATTEN", 2) },
                 { "LOW FREQUENCY", "HIGH BOOST", "HIGH ATTEN" } };
        // 6 British console A: stepped low / mid frequencies, fixed 12k shelf, high-pass, transformer weight
        a[6] = { "70s British Inductor", "70年代英国インダクター・チャンネル", "the class-A inductor channel EQ of 1970s British desks", 3, 8.0f,
                 "インダクターとトランスのコンソールEQ（学習用の汎用モデル）。周波数はステップ式、高域は 12 kHz 固定のシェルフ。\n"
                 "| 特徴 | 中身 |\n| ステップ周波数 | 低域 35・60・110・220、中域 360〜7.2k |\n| 中域の幅 | やや広めの固定。上げると前に出る |\n| 高域 12k | 固定のシェルフ。上げると柔らかい輝き |\n| トランス | 低域の厚みと倍音。DRIVE で量 |\n"
                 "使いどころ：ボーカル・ギター・ドラムの録りやミックスで、太さと存在感を足す。",
                 "An inductor and transformer console EQ (a generic learning model). Stepped frequencies, a fixed 12 kHz high shelf.\n"
                 "| Trait | What it means |\n| Stepped frequencies | low 35, 60, 110, 220; mid 360 to 7.2k |\n| Mid width | fixed, fairly wide: boosts push forward |\n| High 12k | a fixed shelf: a soft sheen |\n| Transformer | weight and harmonics in the lows; DRIVE sets the amount |\n"
                 "Use: thickness and presence on vocals, guitars and drums.",
                 { C ("HPF", { 0, 50, 80, 160, 300 }, {}, 0, 0),
                   C ("FREQ", { 35, 60, 110, 220 }, {}, 1, 1), G ("GAIN", 16, 1),
                   C ("FREQ", { 360, 700, 1600, 3200, 4800, 7200 }, {}, 2, 2), G ("GAIN", 18, 2),
                   G ("GAIN", 16, 3) },
                 { "HIGH PASS", "LOW", "MID", "HIGH 12K" } };
        // 7 British console E: four-band parametric with bell switches, filters, E (constant Q) / G (proportional Q)
        a[7] = { "80s British Large Console", "80年代英国大型卓チャンネル", "the four-band channel EQ of large 1980s consoles", 2, 2.0f,
                 "大型コンソールのチャンネルEQ（学習用の汎用モデル）。4バンドのパラメトリックとハイパス／ローパス。\n"
                 "| 特徴 | 中身 |\n| 4バンド | HF と LF はシェルフ⇄ベル切替、HMF と LMF は Q つき |\n| Q TYPE | Const＝Q が一定（鋭く切れる）。Prop＝ゲインで Q が変わる（小さく動かすと広く、大きく動かすと鋭い）。この卓の2つの世代の違い |\n| フィルター | HPF 16〜350 Hz、LPF 3〜22 kHz |\n| 色付け | VCA と Class A。DRIVE 0 でほぼ透明 |\n"
                 "使いどころ：ドラムやボーカルの素早い整形、不要な帯域の外科的なカット。",
                 "A large-format console channel EQ (a generic learning model). A four-band parametric plus high-pass / low-pass.\n"
                 "| Trait | What it means |\n| Four bands | HF and LF switch shelf / bell, HMF and LMF have Q |\n| Q TYPE | Const = constant Q (cuts sharply); Prop = Q follows the gain (wide for small moves, sharp for big ones): the two generations of this desk |\n| Filters | HPF 16-350 Hz, LPF 3-22 kHz |\n| Colour | VCA and class A; nearly clean at DRIVE 0 |\n"
                 "Use: quick shaping of drums and vocals, surgical cuts.",
                 { G ("GAIN", 15, 0), F ("FREQ", 1500, 16000, 8000, 0), C ("BELL", { 0, 1 }, { "Shelf", "Bell" }, 0, 0),
                   G ("GAIN", 18, 1), F ("FREQ", 600, 7000, 2500, 1), W ("Q", 0.5f, 3.0f, 1.0f, 1),
                   G ("GAIN", 18, 2), F ("FREQ", 200, 2500, 600, 2), W ("Q", 0.5f, 3.0f, 1.0f, 2),
                   G ("GAIN", 16, 3), F ("FREQ", 30, 450, 100, 3), C ("BELL", { 0, 1 }, { "Shelf", "Bell" }, 0, 3),
                   [&] { Slot s = F ("HPF", 16, 350, 16, 4); s.def = 0.0f; return s; }(), [&] { Slot s = F ("LPF", 3000, 22000, 22000, 4); s.def = 1.0f; return s; }(),
                   C ("Q TYPE", { 0, 1 }, { "Const", "Prop" }, 0, 5) },
                 { "HF", "HMF", "LMF", "LF", "FILTER", "CHAR" } };
        // 8 American console: stepped frequencies and gains, proportional Q, shelf / peak switches
        a[8] = { "American Discrete Punch", "米国ディスクリート・パンチ3バンド", "a discrete American console EQ: stepped, proportional Q", 3, 6.0f,
                 "ディスクリートのコンソールEQ（学習用の汎用モデル）。周波数もゲインもステップ式、Q はゲインに比例。\n"
                 "| 特徴 | 中身 |\n| ステップ | 3バンドとも5つの周波数、ゲインは ±2・4・6・9・12 dB |\n| 比例 Q | 小さく動かすと広く、大きく動かすと鋭い。強く押し出しても濁りにくい |\n| LOW／HIGH | シェルフ⇄ピーク切替 |\n| 色付け | トランスとディスクリート回路の押し出し。DRIVE で量 |\n"
                 "使いどころ：スネアやキックのアタック、ギターの押し出しなど、パンチを足したい時。",
                 "A discrete console EQ (a generic learning model). Stepped frequencies and gains, proportional Q.\n"
                 "| Trait | What it means |\n| Steps | five frequencies per band, gains ±2, 4, 6, 9, 12 dB |\n| Proportional Q | wide for small moves, sharp for big ones: strong pushes stay clean |\n| LOW / HIGH | shelf / peak switch |\n| Colour | transformer and discrete push; DRIVE sets the amount |\n"
                 "Use: punch: snare and kick attack, pushing guitars forward.",
                 { C ("FREQ", { 30, 40, 50, 100, 200 }, {}, 3, 0), S2 ("GAIN", { -12, -9, -6, -4, -2, 0, 2, 4, 6, 9, 12 }, 0), C ("SHAPE", { 0, 1 }, { "Peak", "Shelf" }, 1, 0),
                   C ("FREQ", { 400, 800, 1500, 3000, 5000 }, {}, 2, 1), S2 ("GAIN", { -12, -9, -6, -4, -2, 0, 2, 4, 6, 9, 12 }, 1),
                   C ("FREQ", { 5000, 7000, 10000, 12500, 15000 }, {}, 2, 2), S2 ("GAIN", { -12, -9, -6, -4, -2, 0, 2, 4, 6, 9, 12 }, 2), C ("SHAPE", { 0, 1 }, { "Peak", "Shelf" }, 1, 2) },
                 { "LOW", "MID", "HIGH" } };
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

// ---- 1.0: a model as a fixed list of sections. Each one can be laid onto a main-EQ band (bell, 12 dB/oct shelf,
//      12 / 24 dB/oct cut), which is how "expand to the EQ" reproduces the model exactly with ordinary bands.
//      The list always has the same length for a model; a section that does nothing is marked off.
enum SecKind { SPeak = 0, SLowShelf, SHighShelf, SHighPass, SLowPass };
struct Sec { bool on = false; SecKind kind = SPeak; double f = 1000, q = 0.7071, g = 0; int order = 2; };   // order: 2 = 12 dB/oct, 4 = 24 dB/oct (Butterworth)
inline int maxSections (int model)
{
    static const int n[] = { 4, 3, 6, 6, 7, 4, 4, 6, 3 };
    return n[juce::jlimit (0, kModels - 1, model)];
}
inline std::vector<Sec> sections (int model, const float* k, double fs)
{
    model = juce::jlimit (0, kModels - 1, model);
    const auto& M = models()[(size_t) model];
    auto v = [&] (int i) { return i < (int) M.slots.size() ? value (M.slots[(size_t) i], k[i]) : 0.0f; };
    std::vector<Sec> s;
    auto add = [&] (bool on, SecKind kind, double f, double q, double g, int order = 2)
    { Sec x; x.on = on; x.kind = kind; x.f = juce::jlimit (5.0, fs * 0.49, f); x.q = q; x.g = g; x.order = order; s.push_back (x); };
    auto live = [] (double g) { return std::abs (g) > 0.01; };
    switch (model)
    {
        case 0:   // passive: bandwidth narrows as the gain falls (wider for big moves), shelves can bump (Q > 0.7)
            for (int b = 0; b < 4; ++b)
            {
                const double g = v (b * 4), f = v (b * 4 + 1), bw = v (b * 4 + 2); const bool shelf = v (b * 4 + 3) > 0.5f;
                const double q = bw * (1.0 - 0.35 * std::min (1.0, std::abs (g) / 20.0));
                if (shelf) add (live (g), b <= 1 ? SLowShelf : SHighShelf, f, std::max (0.3, q), g);
                else       add (live (g), SPeak, f, std::max (0.15, q), g);
            }
            break;
        case 1:   // precision: symmetric parametric
        {
            auto band = [&] (int gi, int fi, int qi, int si, bool low)
            {
                const double g = v (gi); const bool shelf = si >= 0 && v (si) > 0.5f;
                if (shelf) add (live (g), low ? SLowShelf : SHighShelf, v (fi), std::min (1.2, (double) v (qi)), g);
                else       add (live (g), SPeak, v (fi), v (qi), g);
            };
            band (0, 1, 2, 3, true); band (4, 5, 6, -1, false); band (7, 8, 9, 10, false);
            break;
        }
        case 2:   // air band: wide fixed bells, air = very gentle high shelf from (freq / 2), capped below Nyquist
        {
            const double fq[] = { 10, 40, 160, 650, 2500 };
            for (int b = 0; b < 5; ++b) add (live (v (b)), SPeak, fq[b], b == 0 ? 0.5 : 0.55, v (b));
            const double air = std::max (0.0f, v (5)), fa = v (6);
            const double corner = std::min (fs * 0.42, fa * 0.5);
            const double amount = fa > fs * 0.5 ? air * (0.45 + 0.55 * std::min (1.0, fs * 0.5 / fa)) : air;   // beyond Nyquist only its lower slope is heard
            add (air > 0.01, SHighShelf, corner, 0.35, amount);
            break;
        }
        case 3:   // British stepped bus
            add (live (v (1)), SLowShelf, v (0), 0.6, v (1));
            add (live (v (3)), SPeak, v (2), v (4), v (3));
            add (live (v (6)), SPeak, v (5), v (7), v (6));
            add (live (v (9)), SHighShelf, v (8), 0.6, v (9));
            add (v (10) > 1, SHighPass, std::max (20.0f, v (10)), 0.7071, 0, 4);   // 24 dB/oct
            add (v (11) > 1, SLowPass, std::max (1000.0f, v (11)), 0.7071, 0, 2);  // 12 dB/oct
            break;
        case 4:   // 60s studio 7-band: wide overlapping bells
        {
            const double fq[] = { 50, 130, 320, 800, 2000, 5000, 12500 };
            for (int b = 0; b < 7; ++b) add (live (v (b)), SPeak, fq[b], 0.9, v (b));
            break;
        }
        case 5:   // vintage tube boost / atten: dial 0-10 -> dB set by the circuit
        {
            const double lf = v (0), lb = v (1) * 1.35, la = v (2) * 1.75;      // about +13.5 / -17.5 dB at 10
            add (lb > 0.01, SLowShelf, lf, 0.9, lb);                              // resonant shelf: a little bump at the corner
            add (la > 0.01, SLowShelf, lf * 2.2, 0.55, -la);                      // starts higher: boost + atten = lows up, dip above
            const double hb = v (4) * 1.6;
            add (hb > 0.01, SPeak, v (3), 1.0 / std::max (0.3, (double) v (5)) * 0.9, hb);   // BANDWIDTH: larger = broader
            const double ha = v (7) * 1.6;
            add (ha > 0.01, SHighShelf, v (6), 0.55, -ha);
            break;
        }
        case 6:   // 70s British inductor channel
            add (v (0) > 1, SHighPass, std::max (20.0f, v (0)), 0.7071, 0, 4);   // 24 dB/oct
            add (live (v (2)), SLowShelf, v (1), 0.7, v (2));
            add (live (v (4)), SPeak, v (3), 1.0, v (4));
            add (live (v (5)), SHighShelf, 12000, 0.6, v (5));
            break;
        case 7:   // 80s British large console: constant / proportional Q
        {
            const bool prop = v (14) > 0.5f;
            auto pq = [&] (double q, double g) { return prop ? q * juce::jlimit (0.5, 2.5, 0.45 + std::abs (g) / 9.0) : q; };   // Prop: Q follows the gain
            if (v (2) > 0.5f) add (live (v (0)), SPeak, v (1), pq (0.8, v (0)), v (0)); else add (live (v (0)), SHighShelf, v (1), 0.7, v (0));
            add (live (v (3)), SPeak, v (4), pq (v (5), v (3)), v (3));
            add (live (v (6)), SPeak, v (7), pq (v (8), v (6)), v (6));
            if (v (11) > 0.5f) add (live (v (9)), SPeak, v (10), pq (0.8, v (9)), v (9)); else add (live (v (9)), SLowShelf, v (10), 0.7, v (9));
            add (v (12) > 17.0f, SHighPass, v (12), 0.7071, 0, 4);       // 24 dB/oct
            add (v (13) < 21000.0f, SLowPass, v (13), 0.7071, 0, 2);     // 12 dB/oct
            break;
        }
        case 8:   // American discrete: proportional Q
        {
            auto q = [] (double g) { return 0.45 + std::abs (g) / 12.0 * 1.5; };
            if (v (2) > 0.5f) add (live (v (1)), SLowShelf, v (0), 0.6 + std::abs (v (1)) / 30.0, v (1)); else add (live (v (1)), SPeak, v (0), q (v (1)), v (1));
            add (live (v (4)), SPeak, v (3), q (v (4)), v (4));
            if (v (7) > 0.5f) add (live (v (6)), SHighShelf, v (5), 0.6 + std::abs (v (6)) / 30.0, v (6)); else add (live (v (6)), SPeak, v (5), q (v (6)), v (6));
            break;
        }
        default: break;
    }
    return s;
}

// The filter design of a model for a set of slot values (shared by the audio and the display): the sections as biquads
inline int design (int model, const float* k, double fs, std::array<Biquad, 16>& out)
{
    int n = 0;
    auto add = [&] () -> Biquad& { auto& b = out[(size_t) n++]; b.flat(); return b; };
    for (const auto& s : sections (model, k, fs))
    {
        if (! s.on || n >= 15) continue;
        switch (s.kind)
        {
            case SPeak:      add().peak (s.f, s.q, s.g, fs); break;
            case SLowShelf:  add().lowShelf (s.f, s.q, s.g, fs); break;
            case SHighShelf: add().highShelf (s.f, s.q, s.g, fs); break;
            case SHighPass:  if (s.order >= 4) { add().highPass (s.f, 0.5412, fs); add().highPass (s.f, 1.3066, fs); } else add().highPass (s.f, s.q, fs); break;
            case SLowPass:   if (s.order >= 4) { add().lowPass (s.f, 0.5412, fs); add().lowPass (s.f, 1.3066, fs); } else add().lowPass (s.f, s.q, fs); break;
        }
    }
    return n;
}

// ---- the processor: filters, then the model's saturation (input-scaled by DRIVE, gain compensated) ----
class Processor
{
public:
    void prepare (double sampleRate) { fs = sampleRate; reset(); lastSig = -1; }
    void reset() { for (auto& b : bq) b.reset(); dc[0] = dc[1] = dy[0] = dy[1] = 0; }
    void set (int model, const float* k, float driveDb, float outDb, bool linked = false)   // linked: the filters live in the main EQ and the circuit in the saturator; only OUT is left here
    {
        double sig = model * 1000.0 + driveDb * 7.0 + outDb * 13.0; for (int i = 0; i < kSlots; ++i) sig += k[i] * (i + 3) * 17.0;
        if (sig != lastSig || model != curModel)
        {
            if (model != curModel) reset();
            curModel = model; n = design (model, k, fs, bq); lastSig = sig;
        }
        sat = models()[(size_t) juce::jlimit (0, kModels - 1, model)].satType;
        if (linked) { n = 0; sat = -1; lastSig = -1; }
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
                    case -1: y = u; break;                                                          // linked: no circuit here
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

namespace analog
{
// what kind of EQ each model is (shown under its name and in the menus)
inline juce::String hint (int m, bool ja)
{
    static const char* j[] = { "真空管で持ち上げる、4バンドのパッシブ・マスタリングEQ", "色付けの少ない、精密なマスタリング用パラメトリック", "幅の広い固定6バンドと、超高域から持ち上げる AIR", "ミックスバスやドラム向けの、ステップ式の英国コンソール系カーブ", "60年代スタジオの自作コンソールEQ（2 dB ステップ）", "50〜60年代の真空管プログラムEQ（同じ低域をブーストとアッテン）", "70年代英国卓の、インダクターと Class A のチャンネルEQ", "80年代の大型コンソールの、4バンド・チャンネルEQ", "ステップ式と比例 Q の、米国ディスクリート卓EQ" };
    m = juce::jlimit (0, kModels - 1, m);
    return ja ? juce::String::fromUTF8 (j[m]) : juce::String (models()[(size_t) m].family);
}
}
