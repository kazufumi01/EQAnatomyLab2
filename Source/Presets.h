#pragma once
#include <vector>
#include <utility>

// =============================================================================
//  Task templates. Each preset is a starting point for one job: what it is for,
//  the one control to move first (its band pulses in the graph), and the sign
//  that you have gone too far. Comparison presets also fill slots A–D.
//  Values are deliberately gentle: they are where you start, not where you end.
// =============================================================================
using PV = std::vector<std::pair<const char*, float>>;

struct PresetDef
{
    int cat;
    const char* nameJa; const char* nameEn;
    const char* goalJa; const char* goalEn;
    const char* moveJa; const char* moveEn;
    const char* cautionJa; const char* cautionEn;
    int highlight;                 // band to pulse (0–5), −1 none
    PV base;
    std::vector<PV> slots;         // A–D for comparison presets
};

inline const char* presetCategoryName (int c, bool ja)
{
    static const char* j[] = { "掃除", "音色づくり", "空間とステレオ", "バスとマスタリング", "譲り合い", "学習と比較" };
    static const char* e[] = { "Cleanup", "Tone", "Space & stereo", "Bus & mastering", "Making room", "Learn & compare" };
    return (ja ? j : e)[c];
}

// types: 0 Bell 1 LowShelf 2 HighShelf 3 LowCut 4 HighCut 5 Notch 6 AllPass 7 Tilt 8 FlatTilt
// slopes: 0 6 dB 1 12 dB 2 18 dB 3 24 dB 4 36 dB 5 48 dB
inline const std::vector<PresetDef>& presetDefs()
{
    static const std::vector<PresetDef> P =
    {
        // ---------------------------------------------------------------- Cleanup
        { 0, "低音楽器以外のローエンド整理", "Low-end cleanup (non-bass sources)",
          "ボーカル・ギター・シンセなど、低音を担当しない楽器の不要な低域と 300 Hz 付近の濁りを取る", "Removes unneeded lows and the 300 Hz mud from vocals, guitars, synths and other non-bass parts",
          "バンド 1（LC）の周波数を上げていき、音が痩せる手前で止めて少し戻す", "Raise band 1 (low cut) until the sound starts to thin, then back off a little",
          "ボーカルや低めのギターは 80 Hz 前後まで。上げすぎると胸の厚みが消える", "Around 80 Hz for vocals and low guitars; too high and the chest disappears",
          0, { { "b1_on", 1 }, { "b1_type", 3 }, { "b1_freq", 100 }, { "b1_slope", 2 }, { "b1_q", 0.707f },
               { "b2_on", 1 }, { "b2_type", 0 }, { "b2_freq", 300 }, { "b2_gain", -2 }, { "b2_q", 1.2f } }, {} },

        { 0, "共鳴探し（シーク＆デストロイ）", "Resonance hunt (seek and destroy)",
          "耳障りに鳴る共鳴を見つけて、細く削る", "Finds a ringing resonance and notches it out",
          "バンド 3 の FREQ をゆっくり動かし（Alt＋ドラッグでソロ試聴）、鳴る所で止めたら GAIN を −3〜−5 dB に反転する", "Sweep band 3's FREQ slowly (Alt-drag to solo); where it rings, flip GAIN to −3 to −5 dB",
          "+10 dB は探すときだけ。音量に注意（Auto gain オン）。削る数は 2〜3 か所まで", "+10 dB is only for searching — mind the level (Auto gain is on). Cut two or three spots at most",
          2, { { "b3_on", 1 }, { "b3_type", 0 }, { "b3_freq", 1000 }, { "b3_gain", 10 }, { "b3_q", 8 } }, {} },

        { 0, "ボーカルのこもり取り（ダイナミック）", "Vocal mud control (dynamic)",
          "低い音程で出るこもりだけを、出た瞬間に抑える", "Tames the boominess only when low notes bring it out",
          "バンド 2 の THR を、こもる箇所でだけ now が動く位置に合わせる", "Set band 2's THR so 'now' moves only on the boomy passages",
          "now が常に動くなら THR を上げる。削りすぎると胸の響きが消える", "If 'now' moves all the time, raise THR; too much removes the chest resonance",
          1, { { "b2_on", 1 }, { "b2_type", 0 }, { "b2_freq", 280 }, { "b2_gain", 0 }, { "b2_q", 1.5f },
               { "b2_dyn", 1 }, { "b2_thr", -30 }, { "b2_range", -3 } }, {} },

        { 0, "歯擦音を抑える", "Taming sibilance",
          "サ行の刺さる帯域だけを、出た瞬間に下げる", "Dips the hissy band only when an 's' comes through",
          "バンド 5 の FREQ を 5〜9 kHz で動かし（Alt＋ドラッグでソロ）、刺さる所に合わせる", "Move band 5's FREQ between 5 and 9 kHz (Alt-drag to solo) onto the harshest spot",
          "検出のアタックは 5 ms 固定。強い歯擦音は専用のディエッサーと併用する", "Detection attack is fixed at 5 ms; pair with a dedicated de-esser for strong sibilance",
          4, { { "b5_on", 1 }, { "b5_type", 0 }, { "b5_freq", 6500 }, { "b5_gain", 0 }, { "b5_q", 3 },
               { "b5_dyn", 1 }, { "b5_thr", -34 }, { "b5_range", -6 } }, {} },

        { 0, "耳に痛い帯域の抑制", "Harshness control",
          "大きく歌ったり弾いたりした時だけ出る 2〜4 kHz の痛さを抑える", "Softens the 2–4 kHz bite that appears only when the part gets loud",
          "バンド 4 の THR を、痛い瞬間だけ now が動く位置に合わせる", "Set band 4's THR so 'now' moves only at the painful moments",
          "削りすぎると前に出なくなる。−4 dB 以内が目安", "Too much and it loses presence; stay within about −4 dB",
          3, { { "b4_on", 1 }, { "b4_type", 0 }, { "b4_freq", 3000 }, { "b4_gain", 0 }, { "b4_q", 2 },
               { "b4_dyn", 1 }, { "b4_thr", -30 }, { "b4_range", -4 } }, {} },

        // ---------------------------------------------------------------- Tone
        { 1, "ベッドルーム・ボーカル", "Bedroom-pop vocal",
          "近くで歌っているような柔らかさと、息の質感を作る（ドリームポップ／インディーポップ向け）", "Close, soft and breathy (dream pop / indie pop)",
          "SAT の MIX を 20〜40 % で動かし、息の質感の量を決める", "Move SAT MIX between 20 and 40 % to set how much breath texture you want",
          "バンド 5 の HS は Lin なので全体が遅れる（LAT 表示）。サ行が目立ったら HS を下げる", "Band 5's shelf is Lin, so everything is delayed (see LAT). If 's' sounds get loud, lower the shelf",
          4, { { "qmode", 2 }, { "b1_on", 1 }, { "b1_type", 3 }, { "b1_freq", 90 }, { "b1_slope", 2 },
               { "b2_on", 1 }, { "b2_type", 0 }, { "b2_freq", 220 }, { "b2_gain", -2 }, { "b2_q", 1 },
               { "b4_on", 1 }, { "b4_type", 0 }, { "b4_freq", 3000 }, { "b4_gain", 1.5f }, { "b4_q", 1 }, { "b4_tb", -60 },
               { "b5_on", 1 }, { "b5_type", 2 }, { "b5_freq", 10000 }, { "b5_gain", 2 }, { "b5_q", 0.7f }, { "b5_phase", 1 },
               { "sat_on", 1 }, { "sat_type", 0 }, { "sat_lo", 1500 }, { "sat_hi", 6000 }, { "sat_drive", 6 }, { "sat_mix", 30 } }, {} },

        { 1, "キックの芯とアタック", "Kick: weight and click",
          "60 Hz の芯、350 Hz の箱鳴り取り、4 kHz のアタックで輪郭を作る", "Weight at 60 Hz, less box at 350 Hz, click at 4 kHz",
          "バンド 2（350 Hz）の深さを −2〜−6 dB で決める", "Set the depth of band 2 (350 Hz) between −2 and −6 dB",
          "60 Hz を上げすぎるとベースとぶつかる。4 kHz はアタックだけに効かせている（+80）", "Too much 60 Hz fights the bass. The 4 kHz boost acts on the attack only (+80)",
          1, { { "b1_on", 1 }, { "b1_type", 0 }, { "b1_freq", 60 }, { "b1_gain", 3 }, { "b1_q", 1.5f },
               { "b2_on", 1 }, { "b2_type", 0 }, { "b2_freq", 350 }, { "b2_gain", -4 }, { "b2_q", 2 },
               { "b4_on", 1 }, { "b4_type", 0 }, { "b4_freq", 4000 }, { "b4_gain", 3 }, { "b4_q", 1.2f }, { "b4_tb", -80 } }, {} },

        { 1, "ベースのラインを見えるように", "Bass: making the line audible",
          "量感を足しながらもたつきを減らし、小さい音量でもラインが聞こえるようにする", "Adds weight without flab and keeps the line audible at low volume",
          "バンド 4 を 700 Hz〜1.2 kHz で動かし、小さい音量でもラインが聞こえる所を探す", "Move band 4 between 700 Hz and 1.2 kHz until the line reads at low volume",
          "Tube Passive カーブのため、バンド 1 と 2 のブーストとカットが干渉して 45 Hz 付近の山と 100 Hz 付近のへこみができる", "In Tube Passive, bands 1 and 2 interact: a bump near 45 Hz and a dip near 100 Hz",
          3, { { "qmode", 5 }, { "b1_on", 1 }, { "b1_type", 1 }, { "b1_freq", 60 }, { "b1_gain", 4 }, { "b1_q", 0.7f },
               { "b2_on", 1 }, { "b2_type", 1 }, { "b2_freq", 60 }, { "b2_gain", -3 }, { "b2_q", 0.7f },
               { "b4_on", 1 }, { "b4_type", 0 }, { "b4_freq", 800 }, { "b4_gain", 2.5f }, { "b4_q", 1.2f } }, {} },

        // ---------------------------------------------------------------- Space & stereo
        { 2, "ボーカルの場所を空ける（楽器バス用）", "Making space for the vocal (instrument bus)",
          "楽器バスの中央（Mid）だけ 2.5 kHz を少し下げ、ボーカルの居場所を作る", "Dips 2.5 kHz in the centre (mid) of the instrument bus to make room for the vocal",
          "バンド 4 の GAIN を、ボーカルが前に出るまで −1〜−3 dB で動かす", "Move band 4's GAIN between −1 and −3 dB until the vocal sits in front",
          "Side は触らないので広がりはそのまま。ボーカルが中央にない曲では効かない", "Sides are untouched, so the width stays. Does nothing if the vocal is not centred",
          3, { { "b4_on", 1 }, { "b4_type", 0 }, { "b4_freq", 2500 }, { "b4_gain", -2 }, { "b4_q", 1 }, { "b4_ch", 1 } }, {} },

        { 2, "広がりと空気感", "Width and air",
          "左右の差（Side）の高域だけを持ち上げ、中央はそのままで広がりを出す", "Lifts only the side's top end: wider, with the centre untouched",
          "右端の CORR を見ながら、バンド 5 の GAIN を決める", "Set band 5's GAIN while watching CORR on the right",
          "CORR が 0 を下回る手前まで。モノラルで消える成分が増える。Lin なので全体が遅れる", "Stop before CORR drops below 0 — more of it vanishes in mono. Lin, so everything is delayed",
          4, { { "b5_on", 1 }, { "b5_type", 2 }, { "b5_freq", 10000 }, { "b5_gain", 3 }, { "b5_q", 0.7f }, { "b5_ch", 2 }, { "b5_phase", 1 } }, {} },

        // ---------------------------------------------------------------- Bus & mastering
        { 3, "マスターの微調整", "Master touch-up",
          "急なローカットは Min、緩やかなトーン補正は Lin にして、プリリンギングを抑えながら整える", "Steep low cut on Min, gentle tone moves on Lin: shaping with little pre-ringing",
          "バンド 5 の HS の量（0〜+1.5 dB）で明るさを決める", "Set the brightness with band 5's shelf (0 to +1.5 dB)",
          "±1 dB 前後が目安。Lin のため LAT 約 85 ms（Res Mid）", "Around ±1 dB is plenty. Lin, so about 85 ms latency (Res Mid)",
          4, { { "res", 1 }, { "b1_on", 1 }, { "b1_type", 3 }, { "b1_freq", 25 }, { "b1_slope", 3 },
               { "b2_on", 1 }, { "b2_type", 1 }, { "b2_freq", 90 }, { "b2_gain", 0.8f }, { "b2_q", 0.6f }, { "b2_phase", 1 },
               { "b4_on", 1 }, { "b4_type", 0 }, { "b4_freq", 3200 }, { "b4_gain", -0.8f }, { "b4_q", 0.7f }, { "b4_phase", 1 },
               { "b5_on", 1 }, { "b5_type", 2 }, { "b5_freq", 12000 }, { "b5_gain", 1 }, { "b5_q", 0.6f }, { "b5_phase", 1 } }, {} },

        // ---------------------------------------------------------------- Making room
        { 4, "ボーカルに譲るギター・シンセ", "Guitar / synth making room for the vocal",
          "ボーカルが歌っている間だけ、ぶつかる 2.5 kHz を下げる（サイドチェーン）", "Dips 2.5 kHz only while the vocal sings (sidechain)",
          "ホストのサイドチェーン入力にボーカルを指定し、バンド 4 の ABOVE を −3〜−6 dB で決める", "Pick the vocal as the host's sidechain source, then set band 4's ABOVE between −3 and −6 dB",
          "サイドチェーンが未接続だと反応しない（Ableton はデバイスの Sidechain 欄で指定）", "Does nothing without a sidechain (in Ableton, set it in the device's Sidechain section)",
          3, { { "b4_on", 1 }, { "b4_type", 0 }, { "b4_freq", 2500 }, { "b4_gain", 0 }, { "b4_q", 1.4f },
               { "b4_dyn", 1 }, { "b4_src", 1 }, { "b4_thr", -36 }, { "b4_range", -4 } }, {} },

        // ---------------------------------------------------------------- Learn & compare
        { 5, "位相の比較（A / B / C）", "Phase comparison (A / B / C)",
          "同じカーブを A＝Zero latency、B＝Analog、C＝Linear で聴き比べる", "The same curve as A = Zero latency, B = Analog, C = Linear",
          "A・B・C を切り替え、インパルス表示と音の立ち上がりを比べる", "Switch A, B and C; compare the impulse view and the attacks",
          "C（Linear）だけレイテンシーが付く。切り替えの瞬間はフェードで音が途切れる", "Only C (Linear) adds latency; switching fades briefly",
          1, { { "res", 1 }, { "b2_on", 1 }, { "b2_type", 0 }, { "b2_freq", 120 }, { "b2_gain", 6 }, { "b2_q", 3 },
               { "b5_on", 1 }, { "b5_type", 2 }, { "b5_freq", 8000 }, { "b5_gain", 4 }, { "b5_q", 0.7f } },
          { { { "mode", 0 } }, { { "mode", 1 } }, { { "mode", 2 } } } },

        { 5, "カーブの性格比較（A〜D とモーフ）", "Curve character (A–D and morph)",
          "同じ数値を A＝Clean、B＝British A、C＝US 500、D＝Tube Passive で比べる", "The same numbers as A = Clean, B = British A, C = US 500, D = Tube Passive",
          "スロットを切り替えるか、MORPH で A から B へ連続して変える", "Switch slots, or use MORPH to slide from A to B",
          "カーブの形の傾向だけで、機材の音そのものではない", "Shape tendencies only, not the sound of the hardware",
          2, { { "b2_on", 1 }, { "b2_type", 1 }, { "b2_freq", 100 }, { "b2_gain", 6 }, { "b2_q", 0.7f },
               { "b3_on", 1 }, { "b3_type", 0 }, { "b3_freq", 1000 }, { "b3_gain", 6 }, { "b3_q", 1 } },
          { { { "qmode", 0 } }, { { "qmode", 2 } }, { { "qmode", 3 } }, { { "qmode", 5 } } } },
    };
    return P;
}

// What the editor and processor use: each slot holds the full set of values
struct PresetSlot { PV values; };
struct Preset
{
    int category;
    const char* nameJa; const char* nameEn;
    const char* purposeJa; const char* purposeEn;
    const char* firstJa; const char* firstEn;
    const char* cautionJa; const char* cautionEn;
    int focus;
    std::vector<PresetSlot> slots;      // one for a normal preset, A–D for comparisons
};
inline const std::vector<Preset>& presets()
{
    static const std::vector<Preset> out = []
    {
        std::vector<Preset> v;
        for (auto& d : presetDefs())
        {
            Preset p { d.cat, d.nameJa, d.nameEn, d.goalJa, d.goalEn, d.moveJa, d.moveEn, d.cautionJa, d.cautionEn, d.highlight, {} };
            if (d.slots.empty()) p.slots.push_back ({ d.base });
            else for (auto& sl : d.slots) { PV all = d.base; all.insert (all.end(), sl.begin(), sl.end()); p.slots.push_back ({ all }); }
            v.push_back (std::move (p));
        }
        return v;
    }();
    return out;
}
