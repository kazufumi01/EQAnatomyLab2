#pragma once
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_dsp/juce_dsp.h>
#include <vector>
#include <thread>
#include <map>

// =============================================================================
//  Reference curve "C" (EQ Anatomy Lab 2 1.0)
//  A long-term average spectrum in 1/3-octave bands (bandwise energy), 1 kHz = 0 dB.
//  The analyzer draws narrowband levels tilted +3 dB/oct, which reads like bandwise energy,
//  so C is drawn on the analyzer's scale and lined up with A over 200 Hz – 5 kHz.
//  C is a guide to spot a lean, not a target to match.
// =============================================================================
namespace refc
{
struct Curve
{
    juce::String name, note;               // note: where the numbers come from (shown in the help)
    std::vector<std::pair<double, double>> pts;   // Hz, dB (bandwise, 1 kHz = 0)
    float tolerance = 3.0f;                // ± dB band drawn around the line (a design assumption)
    bool valid() const { return pts.size() >= 2; }
    double at (double f) const      // log-frequency interpolation
    {
        if (pts.empty()) return 0.0;
        if (f <= pts.front().first) return pts.front().second;
        if (f >= pts.back().first) return pts.back().second;
        for (size_t i = 1; i < pts.size(); ++i)
            if (f <= pts[i].first)
            {
                const double t = std::log (f / pts[i - 1].first) / std::log (pts[i].first / pts[i - 1].first);
                return pts[i - 1].second + t * (pts[i].second - pts[i - 1].second);
            }
        return pts.back().second;
    }
    void normalise1k() { const double r = at (1000.0); for (auto& p : pts) p.second -= r; }
    juce::String serialise() const
    {
        juce::String s; for (auto& p : pts) s << juce::String (p.first, 1) << ":" << juce::String (p.second, 2) << ";";
        return s;
    }
    static Curve parse (const juce::String& name, const juce::String& note, const juce::String& s)
    {
        Curve c; c.name = name; c.note = note;
        for (auto& t : juce::StringArray::fromTokens (s, ";", ""))
            if (t.containsChar (':')) c.pts.push_back ({ t.upToFirstOccurrenceOf (":", false, false).getDoubleValue(), t.fromFirstOccurrenceOf (":", false, false).getDoubleValue() });
        return c;
    }
    juce::var toJson() const
    {
        auto* o = new juce::DynamicObject();
        o->setProperty ("name", name); o->setProperty ("note", note); o->setProperty ("tolerance_db", tolerance);
        juce::Array<juce::var> a; for (auto& p : pts) { juce::Array<juce::var> pr; pr.add (p.first); pr.add (p.second); a.add (juce::var (pr)); }
        o->setProperty ("points_third_octave_band", a); o->setProperty ("basis", "bandwise, 1 kHz = 0 dB");
        return juce::var (o);
    }
    static Curve fromJson (const juce::var& v)
    {
        Curve c; c.name = v["name"].toString(); c.note = v["note"].toString();
        if (v.hasProperty ("tolerance_db")) c.tolerance = (float) (double) v["tolerance_db"];
        if (auto* a = v["points_third_octave_band"].getArray()) for (auto& p : *a) if (p.size() >= 2) c.pts.push_back ({ (double) p[0], (double) p[1] });
        return c;
    }
};

// ---- built-in curves ----
// base: Pestana et al. 2013 (AES 135, paper 8960), -5 dB/oct narrowband 100 Hz-4 kHz (measured), steeper below 60 Hz (trend; slope assumed),
// 60-100 Hz extended (assumed), above 4 kHz -8 dB/oct (assumed). Converted to 1/3-octave bands (derived). Values from the user's design file.
inline const std::vector<std::pair<double, double>>& basePoints()
{
    static const std::vector<std::pair<double, double>> p {
        { 20, -15.7 }, { 31.5, -5.8 }, { 40, -0.7 }, { 50, 4.2 }, { 63, 8.0 }, { 80, 7.3 }, { 100, 6.6 }, { 125, 6.0 }, { 160, 5.3 }, { 200, 4.6 },
        { 250, 4.0 }, { 315, 3.3 }, { 400, 2.6 }, { 500, 2.0 }, { 630, 1.3 }, { 800, 0.6 }, { 1000, 0.0 }, { 1250, -0.6 }, { 1600, -1.4 }, { 2000, -2.0 },
        { 2500, -2.6 }, { 3150, -3.3 }, { 4000, -4.0 }, { 5000, -5.6 }, { 6300, -7.3 }, { 8000, -9.0 }, { 10000, -10.6 }, { 12500, -12.2 }, { 16000, -14.0 }, { 20000, -15.6 } };
    return p;
}
// offsets: { low Hz, high Hz, dB } added to the base with 1/3-octave soft edges (design assumptions; directions from the studies)
struct Offset { double lo, hi, db; };
inline Curve withOffsets (const juce::String& name, const juce::String& note, std::initializer_list<Offset> off)
{
    Curve c; c.name = name; c.note = note;
    for (auto [f, d] : basePoints())
    {
        double add = 0;
        for (auto& o : off)
        {
            const double e = 1.0 / 3.0;   // octaves of fade at each edge
            const double inLo = juce::jlimit (0.0, 1.0, 0.5 + std::log2 (f / o.lo) / e), inHi = juce::jlimit (0.0, 1.0, 0.5 - std::log2 (f / o.hi) / e);
            add += o.db * std::min (inLo, inHi);
        }
        c.pts.push_back ({ f, d + add });
    }
    c.normalise1k();
    return c;
}
// "What's Going On" (1971) measured by the user: 1/3-octave levels of the master and four Demucs stems (loop-back recording)
inline Curve fromColumn (const juce::String& name, const juce::String& note, int col)
{
    static const double t[][6] = {
        { 22.4, -17.7, -17.27, -29.79, -18.79, -28.71 }, { 28.3, -18.1, -18.08, -25.93, -19.47, -28.31 }, { 35.6, -18.8, -21.52, -22.16, -23.51, -30.51 },
        { 44.9, -10.8, -20.60, -10.98, -27.06, -32.16 }, { 56.6, -5.9, -14.11, -5.81, -29.60, -26.30 }, { 71.3, -4.4, -4.90, -5.72, -32.63, -31.03 },
        { 89.8, -4.3, -3.01, -6.60, -34.00, -28.22 }, { 113.1, -7.6, -13.03, -7.75, -28.87, -20.16 }, { 142.5, -11.2, -18.61, -11.50, -14.96, -25.31 },
        { 179.6, -9.1, -12.34, -11.43, -9.65, -19.54 }, { 226.3, -8.7, -11.81, -13.93, -7.00, -13.61 }, { 285.1, -9.8, -10.49, -22.72, -8.15, -12.46 },
        { 359.2, -10.0, -13.51, -25.00, -6.28, -13.10 }, { 452.5, -10.1, -20.43, -34.15, -5.76, -10.41 }, { 570.2, -10.1, -23.20, -43.54, -4.89, -12.39 },
        { 718.4, -11.2, -22.35, -49.99, -5.90, -13.61 }, { 905.1, -12.8, -28.92, -55.49, -7.56, -15.28 }, { 1140.4, -14.6, -32.32, -59.55, -9.09, -18.16 },
        { 1436.8, -17.9, -34.08, -64.90, -12.62, -20.43 }, { 1810.2, -19.5, -35.36, -69.04, -15.63, -18.79 }, { 2280.7, -24.0, -34.10, -73.03, -20.68, -23.03 },
        { 2873.5, -27.4, -35.49, -75.87, -25.35, -25.96 }, { 3620.4, -27.2, -38.51, -78.69, -22.81, -28.38 }, { 4561.4, -33.0, -38.29, -80.10, -30.53, -33.46 },
        { 5747.0, -32.5, -40.73, -80.92, -26.76, -43.43 }, { 7240.8, -36.1, -41.95, -84.29, -30.58, -48.93 }, { 9122.8, -37.8, -44.74, -86.22, -31.68, -55.58 },
        { 11494.0, -45.1, -51.46, -89.22, -38.98, -62.40 }, { 14481.5, -54.4, -56.46, -91.60, -49.58, -66.04 }, { 18245.6, -60.0, -60.12, -92.25, -57.15, -68.25 } };
    Curve c; c.name = name; c.note = note; c.tolerance = 0.0f;
    for (auto& r : t) c.pts.push_back ({ r[0], r[col] });
    c.normalise1k();
    return c;
}
struct Builtin { const char* group; Curve c; };
inline const std::vector<Builtin>& builtins()
{
    static const std::vector<Builtin> b = []
    {
        std::vector<Builtin> v;
        const juce::String rs = juce::String::fromUTF8 ("研究値＋仮定：Pestana ほか 2013（米英ヒット曲 772 曲）。100 Hz〜4 kHz の傾きは実測、60 Hz 以下の傾き・4 kHz 以上は仮定。±3 dB の帯は目安（仮定）");
        const juce::String go = juce::String::fromUTF8 ("ジャンル／年代の補正：方向は研究の傾向（Pestana 2013・Hove 2019・ȚîNȚ 2025）、量は仮定。実際の参考曲で確かめること");
        v.push_back ({ "Research", [&] { Curve c; c.name = "Pop average"; c.note = rs; for (auto& p : basePoints()) c.pts.push_back (p); return c; }() });
        v.push_back ({ "Genre", withOffsets ("Electronic", go, { { 20, 60, 3 }, { 8000, 16000, 1.5 } }) });
        v.push_back ({ "Genre", withOffsets ("Hip-hop", go, { { 20, 60, 4 }, { 8000, 16000, 1 } }) });
        v.push_back ({ "Genre", withOffsets ("Rock", go, { { 20, 50, -1 } }) });
        v.push_back ({ "Genre", withOffsets ("Jazz", go, { { 6000, 20000, -2 } }) });
        v.push_back ({ "Genre", withOffsets ("Folk", go, { { 20, 60, -2 }, { 6000, 20000, -2 } }) });
        v.push_back ({ "Era", withOffsets ("1950s", go, { { 20, 80, -6 }, { 6000, 20000, -3 } }) });
        v.push_back ({ "Era", withOffsets ("1970s", go, { { 20, 60, -3 } }) });
        v.push_back ({ "Era", withOffsets ("1990", go, { { 20, 80, -1.5 }, { 2000, 5000, 1 } }) });
        v.push_back ({ "Era", withOffsets ("2000", go, { { 8000, 16000, 1 } }) });
        v.push_back ({ "Era", withOffsets ("2020", go, { { 20, 120, 1.5 }, { 150, 400, 0.5 }, { 8000, 16000, -1 } }) });
        const juce::String wn = juce::String::fromUTF8 ("実測：What's Going On（Marvin Gaye, 1971）。ループバック録音、Welch 法、1/3 オクターブ。ステムは Demucs による AI 分離（漏れあり、目安）");
        v.push_back ({ "What's Going On (1971)", fromColumn ("WGO 1971 master", wn, 1) });
        v.push_back ({ "What's Going On (1971)", fromColumn ("WGO 1971 drums", wn, 2) });
        v.push_back ({ "What's Going On (1971)", fromColumn ("WGO 1971 bass", wn, 3) });
        v.push_back ({ "What's Going On (1971)", fromColumn ("WGO 1971 vocals", wn, 4) });
        v.push_back ({ "What's Going On (1971)", fromColumn ("WGO 1971 other", wn, 5) });
        return v;
    }();
    return b;
}

// ---- user curves: one JSON file each ----
inline juce::File userDir() { return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile ("JPK").getChildFile ("EQ Anatomy Lab 2 v1").getChildFile ("Curves"); }
inline juce::Array<juce::File> userFiles() { auto a = userDir().findChildFiles (juce::File::findFiles, false, "*.json"); a.sort(); return a; }
inline bool save (const Curve& c)
{
    auto d = userDir(); d.createDirectory();
    auto f = d.getChildFile (juce::File::createLegalFileName (c.name) + ".json");
    return f.replaceWithText (juce::JSON::toString (c.toJson()));
}
inline Curve load (const juce::File& f) { return Curve::fromJson (juce::JSON::parse (f)); }

// ---- analysis of an audio file ----
// Long-term average spectrum of the parts that carry music: frames under -60 dBFS (silence) and frames more than 20 dB
// below the mean level (very quiet passages, fades, room tone) are left out. 8192-point Hann frames, 50 % overlap,
// summed into 1/3-octave bands (bandwise energy), 1 kHz = 0 dB.
struct Result { Curve curve; double seconds = 0, usedPct = 0; juce::String error; };
inline Result analyse (const juce::File& file, std::function<bool()> cancelled = {})
{
    Result r;
    juce::AudioFormatManager fm; fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> rd (fm.createReaderFor (file));
    if (rd == nullptr) { r.error = juce::String::fromUTF8 ("読み込めない形式です（WAV / AIFF / FLAC / MP3 / OGG）"); return r; }
    const double fs = rd->sampleRate;
    const juce::int64 total = std::min<juce::int64> (rd->lengthInSamples, (juce::int64) (fs * 60 * 20));   // up to 20 minutes
    r.seconds = (double) total / fs;
    constexpr int order = 13, N = 1 << order, hop = N / 2;
    juce::dsp::FFT fft (order);
    juce::dsp::WindowingFunction<float> win ((size_t) N, juce::dsp::WindowingFunction<float>::hann, false);
    juce::AudioBuffer<float> buf ((int) std::max (1u, rd->numChannels), N);
    std::vector<float> frame ((size_t) (2 * N));
    std::vector<double> acc ((size_t) (N / 2 + 1), 0.0);
    auto readMono = [&] (juce::int64 pos)
    {
        rd->read (&buf, 0, N, pos, true, true);
        double e = 0;
        for (int i = 0; i < N; ++i)
        {
            float m = 0; for (int ch = 0; ch < buf.getNumChannels(); ++ch) m += buf.getSample (ch, i);
            m /= (float) buf.getNumChannels(); frame[(size_t) i] = m; e += (double) m * m;
        }
        return 10.0 * std::log10 (e / N + 1e-20);
    };
    // pass 1: the level of every frame (no spectra kept, so long files need little memory)
    std::vector<double> levels;
    for (juce::int64 pos = 0; pos + N <= total; pos += hop)
    {
        if (cancelled && cancelled()) { r.error = "cancelled"; return r; }
        levels.push_back (readMono (pos));
    }
    if (levels.empty()) { r.error = juce::String::fromUTF8 ("短すぎて解析できません（0.2 秒以上）"); return r; }
    // gates: absolute -60 dBFS, then relative: 20 dB under the power mean of what passed
    double sum = 0; int cnt = 0;
    for (double d : levels) if (d > -60.0) { sum += std::pow (10.0, d / 10.0); ++cnt; }
    if (cnt == 0) { r.error = juce::String::fromUTF8 ("ほぼ無音のファイルです"); return r; }
    const double rel = 10.0 * std::log10 (sum / cnt) - 20.0;
    // pass 2: power spectra of the frames that passed
    int used = 0; size_t fi = 0;
    for (juce::int64 pos = 0; pos + N <= total; pos += hop, ++fi)
    {
        if (cancelled && cancelled()) { r.error = "cancelled"; return r; }
        if (! (levels[fi] > -60.0 && levels[fi] > rel)) continue;
        readMono (pos);
        std::fill (frame.begin() + N, frame.end(), 0.0f);
        win.multiplyWithWindowingTable (frame.data(), (size_t) N);
        fft.performFrequencyOnlyForwardTransform (frame.data(), true);
        for (int k = 0; k <= N / 2; ++k) acc[(size_t) k] += (double) frame[(size_t) k] * frame[(size_t) k];
        ++used;
    }
    r.usedPct = 100.0 * used / (double) levels.size();
    // 1/3-octave bands (IEC nominal centres 20 Hz … 20 kHz), bandwise energy
    static const double centres[] = { 20, 25, 31.5, 40, 50, 63, 80, 100, 125, 160, 200, 250, 315, 400, 500, 630, 800, 1000, 1250, 1600, 2000, 2500,
                                      3150, 4000, 5000, 6300, 8000, 10000, 12500, 16000, 20000 };
    Curve c; c.name = file.getFileNameWithoutExtension(); c.tolerance = 0.0f;
    for (double fc : centres)
    {
        const double lo = fc * std::pow (2.0, -1.0 / 6.0), hi = fc * std::pow (2.0, 1.0 / 6.0);
        if (hi > fs * 0.5) break;
        double e = 0; int nb = 0;
        for (int k = std::max (1, (int) std::ceil (lo * N / fs)); k <= std::min (N / 2, (int) std::floor (hi * N / fs)); ++k) { e += acc[(size_t) k]; ++nb; }
        if (nb == 0) { const int k = juce::jlimit (1, N / 2, (int) std::lround (fc * N / fs)); e = acc[(size_t) k] * (hi - lo) * N / fs; }   // narrower than a bin: scale one bin
        c.pts.push_back ({ fc, 10.0 * std::log10 (e / used + 1e-30) });
    }
    c.normalise1k();
    c.note = juce::String::fromUTF8 ("解析：") + file.getFileName() + " / " + juce::String (r.seconds, 1) + juce::String::fromUTF8 (" 秒中 ")
           + juce::String (r.usedPct, 0) + juce::String::fromUTF8 ("% を使用（無音と、平均より 20 dB 以上小さい部分は除外）");
    r.curve = std::move (c);
    return r;
}
inline bool isAudio (const juce::String& path) { return juce::File (path).hasFileExtension ("wav;wave;aif;aiff;flac;mp3;ogg"); }
}
