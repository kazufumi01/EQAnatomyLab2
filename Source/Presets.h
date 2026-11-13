#pragma once
#include <vector>
#include <utility>
#include "AnalogEQ.h"

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
    int model = -1;                // 1.0: the analog EQ model it uses (−1 none): listed under that type too
};

// an analog model's values: every slot (the model's default unless given, as a real value), on, drive
inline PV analogPV (int model, std::vector<std::pair<int, float>> v, float drive)
{
    static const char* K[] = { "an_k1", "an_k2", "an_k3", "an_k4", "an_k5", "an_k6", "an_k7", "an_k8",
                               "an_k9", "an_k10", "an_k11", "an_k12", "an_k13", "an_k14", "an_k15", "an_k16" };
    const auto& M = analog::models()[(size_t) model];
    PV p { { "an_on", 1.0f }, { "an_model", (float) model }, { "an_drive", drive } };
    for (int i = 0; i < (int) M.slots.size() && i < 16; ++i)
    {
        float n = M.slots[(size_t) i].def;
        for (auto& [k, val] : v) if (k == i) n = analog::toNorm (M.slots[(size_t) i], val);
        p.push_back ({ K[i], n });
    }
    return p;
}

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

        // ---------------------------------------------------------------- Lab 2: resonance suppressor
        { 0, "ボーカルの耳障りな共鳴（RES）", "Vocal harshness tamer (RES)",
          "声の 2〜8 kHz で、音程や母音によって出たり消えたりする刺さる共鳴を、出た瞬間だけ下げる", "Turns down the piercing resonances in the 2–8 kHz range that come and go with pitch and vowel, only while they ring",
          "RES タブの THRESH を下げていき、Δ で削れている音が「キン」「ジー」だけになる所で止める", "Lower THRESH on the RES tab until Δ plays only the 'ting' and 'zz', then stop",
          "Δ に声の芯（母音）が聞こえたら削りすぎ。DEPTH は 6 dB まで", "If Δ carries the core of the voice (the vowels), it is too much. Keep DEPTH at 6 dB or less",
          -1, { { "rs_on", 1 }, { "rs_mode", 1 }, { "rs_depth", 6 }, { "rs_thr", 6 }, { "rs_lo", 1500 }, { "rs_hi", 9000 }, { "rs_sel", 60 }, { "rs_att", 5 }, { "rs_rel", 120 },
                { "b1_on", 1 }, { "b1_type", 3 }, { "b1_freq", 90 }, { "b1_slope", 2 }, { "b1_q", 0.707f } }, {} },

        { 1, "アコギ・ピアノの鳴りすぎ（RES・Mid）", "Acoustic guitar / piano boom and ring (RES, Mid)",
          "胴鳴りや弦の共鳴が特定の音でだけ膨らむのを、センターだけで抑える（広がりは触らない）", "Holds down body and string resonances that swell on certain notes, in the centre only (the width is left alone)",
          "SELECT を 40〜80 % で動かし、膨らむ音だけが白い山として突き出る位置を探す（Resonance ボタンで見る）", "Move SELECT between 40 and 80 % until only the swelling notes stick out as white peaks (watch with the Resonance button)",
          "REL が短いと音程の切り替わりで揺れる。200 ms 前後から", "A short REL flutters as notes change; start around 200 ms",
          -1, { { "rs_on", 1 }, { "rs_mode", 1 }, { "rs_depth", 4 }, { "rs_thr", 5 }, { "rs_lo", 120 }, { "rs_hi", 3000 }, { "rs_sel", 65 }, { "rs_att", 10 }, { "rs_rel", 250 } }, {} },

        { 3, "マスターの共鳴を薄く（RES・M/S）", "Light resonance control on the master (RES, M/S)",
          "ミックス全体の、特定の音で出る耳障りな帯域を、センターとサイドで別々に、ごく薄く整える", "Gently evens out harsh spots that pop out on certain notes across the whole mix, centre and sides separately",
          "DEPTH 2〜3 dB のまま THRESH を下げ、Δ が小さく「チリチリ」聞こえる程度で止める", "Keep DEPTH at 2–3 dB and lower THRESH until Δ is a faint fizz, then stop",
          "マスターでは 3 dB を超えない。RES の遅延（約 21 ms）が加わる", "On the master, stay under 3 dB. RES adds about 21 ms of latency",
          -1, { { "rs_on", 1 }, { "rs_mode", 3 }, { "rs_depth", 3 }, { "rs_thr", 8 }, { "rs_lo", 200 }, { "rs_hi", 12000 }, { "rs_sel", 50 }, { "rs_att", 20 }, { "rs_rel", 300 } }, {} },

        { 4, "ボーカルに場所を空ける（RES・サイドチェイン）", "Make room for the vocal (RES, sidechain)",
          "ギターやシンセに挿し、ボーカルが鳴っている帯域だけを、その瞬間だけ空ける", "On a guitar or synth: clears only the frequencies the vocal is using, only while it sings",
          "ホストのサイドチェイン欄でボーカルを入力元にし、THRESH を下げて、ボーカルが入った時だけピンクの面が出る所にする", "Pick the vocal as the host's sidechain source, then lower THRESH until the pink area appears only while the vocal sings",
          "SELECT を下げると広い帯域を譲る（0 % に近いほどダイナミック EQ に近い）。DEPTH は 4〜6 dB", "Lower SELECT gives way over wider bands (near 0 % it acts like a dynamic EQ). DEPTH 4–6 dB",
          -1, { { "rs_on", 1 }, { "rs_mode", 0 }, { "rs_sc", 1 }, { "rs_depth", 5 }, { "rs_thr", 2 }, { "rs_lo", 250 }, { "rs_hi", 6000 }, { "rs_sel", 20 }, { "rs_att", 5 }, { "rs_rel", 150 } }, {} },

        // ---------------------------------------------------------------- 1.0: analog EQ models (by type and by use)
        { 3, "マスターの艶と重心（パッシブ）", "Master sheen and weight (passive)",
          "ミックス全体の低域に重心を、最上部に艶を、パッシブ回路のなだらかなカーブで足す", "Adds weight to the lows and sheen to the very top of a whole mix, with the gentle curves of a passive circuit",
          "4 列目（シェルフ）の GAIN を 0〜+3 dB で動かし、艶の量を決める", "Move column 4's GAIN (a shelf) between 0 and +3 dB to set the sheen",
          "マスターでは各 2 dB 前後まで。DRIVE を上げたら OUT で音量をそろえて比べる", "On a master keep each move around 2 dB. If you raise DRIVE, match the level with OUT before comparing",
          -1, analogPV (0, { { 0, 1.5f }, { 1, 39.0f }, { 3, 1.0f }, { 4, -1.0f }, { 5, 330.0f }, { 6, 0.7f }, { 7, 0.0f }, { 12, 2.0f }, { 13, 16000.0f }, { 15, 1.0f } }, 4.0f), {}, 0 },

        { 1, "ボーカルの太さと抜け（パッシブ）", "Vocal body and clarity (passive)",
          "胸の太さと、言葉の抜けを、広いベルでなめらかに足す", "Adds chest and intelligibility with broad, smooth bells",
          "3 列目の FREQ を 2〜5 kHz で動かし、言葉がはっきりする所で止める", "Sweep column 3's FREQ between 2 and 5 kHz and stop where the words come clear",
          "BW を狭めると色が強くなる。歯擦音が増えたら 4 列目を下げる", "Narrow BW and the colour gets stronger; if sibilance grows, lower column 4",
          -1, analogPV (0, { { 4, 2.0f }, { 5, 220.0f }, { 6, 1.0f }, { 8, 2.5f }, { 9, 3300.0f }, { 10, 0.7f }, { 12, 2.0f }, { 13, 12000.0f }, { 15, 1.0f } }, 4.0f), {}, 0 },

        { 3, "マスターの精密な補正（プレシジョン）", "Precise master correction (precision)",
          "色付けをほとんど足さずに、マスターの低域・中低域・高域を少しずつ整える", "Evens out the master's lows, low-mids and highs a little, with almost no added colour",
          "MID の FREQ を 200〜400 Hz で動かし、こもりが減る所で止める", "Sweep MID's FREQ between 200 and 400 Hz and stop where the mud clears",
          "1 か所 1.5 dB まで。迷ったら Bypass と聴き比べる", "Up to 1.5 dB per move; when in doubt, compare with Bypass",
          -1, analogPV (1, { { 0, 1.0f }, { 1, 40.0f }, { 3, 1.0f }, { 4, -1.5f }, { 5, 250.0f }, { 6, 1.2f }, { 7, 1.5f }, { 8, 12000.0f }, { 10, 1.0f } }, 0.0f), {}, 1 },

        { 1, "アコギの明瞭さ（プレシジョン）", "Acoustic guitar clarity (precision)",
          "箱鳴りのこもりを抑え、弦のきらめきを足す", "Holds back the boxy boom and adds string sparkle",
          "HIGH の FREQ を 4〜8 kHz で動かし、ピッキングが前に出る所で止める", "Sweep HIGH's FREQ between 4 and 8 kHz and stop where the picking comes forward",
          "低域を削りすぎると細くなる。ソロとミックスの両方で確かめる", "Cut too much low and it gets thin; check both solo and in the mix",
          -1, analogPV (1, { { 0, -3.0f }, { 1, 120.0f }, { 3, 1.0f }, { 4, -2.0f }, { 5, 400.0f }, { 6, 1.5f }, { 7, 3.0f }, { 8, 6000.0f }, { 9, 0.8f }, { 10, 0.0f } }, 0.0f), {}, 1 },

        { 3, "ミックスの空気感（エア・バンド）", "Air on the mix (air band)",
          "ミックス全体を、耳障りにならずに明るく開く", "Opens up the whole mix without harshness",
          "AIR の GAIN を 2〜6 dB で動かす。AIR FREQ は 20k から試す", "Move AIR's GAIN between 2 and 6 dB; start AIR FREQ at 20k",
          "AIR FREQ を下げるほど効きが強い。シンバルが痛くなったら 40k にする", "The lower AIR FREQ, the stronger; if cymbals turn harsh, go to 40k",
          -1, analogPV (2, { { 4, 1.0f }, { 5, 4.0f }, { 6, 20000.0f } }, 0.0f), {}, 2 },

        { 1, "ボーカルの息と空気（エア・バンド）", "Breath and air on a vocal (air band)",
          "息の成分と空気感を足し、少しだけ中低域を軽くする", "Adds breath and air, and lightens the low-mids a little",
          "AIR の GAIN を 3〜8 dB で動かし、息が自然に聞こえる量にする", "Move AIR's GAIN between 3 and 8 dB until the breath sounds natural",
          "サ行が目立ったら AIR FREQ を上げるか、ディエッサーを併用する", "If 's' sounds stick out, raise AIR FREQ or add a de-esser",
          -1, analogPV (2, { { 1, -1.0f }, { 2, -1.5f }, { 3, -1.0f }, { 5, 6.0f }, { 6, 40000.0f } }, 0.0f), {}, 2 },

        { 3, "ドラムバスの太さ（ブリティッシュ・カーブ）", "Drum bus thickness (British curve)",
          "キックの重さとスネアの抜けを、トランスの太さと一緒に足す", "Adds kick weight and snare crack together with transformer thickness",
          "BASS の GAIN を 0〜+4 dB で動かし、キックの重さを決める", "Move BASS GAIN between 0 and +4 dB to set the kick's weight",
          "DRIVE 10 dB を超えると歪みが聞こえる。HPF はベースと相談", "Above 10 dB DRIVE the distortion is audible; set the HPF with the bass in mind",
          -1, analogPV (3, { { 0, 60.0f }, { 1, 3.0f }, { 2, 400.0f }, { 3, -2.0f }, { 4, 0.7f }, { 5, 4000.0f }, { 6, 2.0f }, { 7, 0.7f }, { 8, 10000.0f }, { 9, 2.0f }, { 10, 20.0f } }, 8.0f), {}, 3 },

        { 1, "エレキギターの押し出し（ブリティッシュ・カーブ）", "Electric guitar push (British curve)",
          "低域を整理し、中高域で前に押し出す", "Cleans up the lows and pushes the upper mids forward",
          "HIGH MID の FREQ を 1.8〜4 kHz で切り替え、前に出る所を選ぶ", "Step HIGH MID's FREQ between 1.8 and 4 kHz and pick the one that comes forward",
          "押し出しすぎるとボーカルとぶつかる。ボーカルと一緒に聴いて決める", "Too much push clashes with the vocal; decide while hearing both",
          -1, analogPV (3, { { 2, 270.0f }, { 3, -2.0f }, { 4, 2.0f }, { 5, 2700.0f }, { 6, 3.0f }, { 7, 0.7f }, { 8, 10000.0f }, { 9, 1.5f }, { 10, 80.0f }, { 11, 10000.0f } }, 6.0f), {}, 3 },

        { 1, "ボーカルのトーン（プログラム7バンド）", "Vocal tone (program 7-band)",
          "60年代スタジオのように、広いバンドを段で動かしてボーカルの音色を大きく決める", "Sets the vocal's tone broadly with stepped, wide bands, the 1960s studio way",
          "2K と 5K を 0〜+4 dB で切り替え、前に出る量を決める", "Step 2K and 5K between 0 and +4 dB to set how far forward it sits",
          "段で動くので細かい調整はメインEQで行う", "It moves in steps: do the fine work in the main EQ",
          -1, analogPV (4, { { 1, -2.0f }, { 2, -2.0f }, { 4, 2.0f }, { 5, 2.0f }, { 6, 4.0f } }, 3.0f), {}, 4 },

        { 1, "ベースの芯（プログラム7バンド）", "Bass core (program 7-band)",
          "低域の芯と、小さなスピーカーでも聞こえる倍音を足す", "Adds a solid low end and the overtones that read on small speakers",
          "800 と 2K を 0〜+4 dB で切り替え、小さい音量でもベースが聞こえる所にする", "Step 800 and 2K between 0 and +4 dB until the bass reads at low volume",
          "50 を上げすぎるとキックとぶつかる。キックと一緒に聴く", "Too much at 50 clashes with the kick; listen with the kick",
          -1, analogPV (4, { { 0, 4.0f }, { 1, 2.0f }, { 2, -4.0f }, { 3, 2.0f }, { 4, 2.0f } }, 4.0f), {}, 4 },

        { 1, "キックの低域トリック（チューブ・プログラム）", "Kick low-end trick (tube program)",
          "同じ周波数で BOOST と ATTEN を両方上げ、太いのに濁らない低域を作る", "Raise BOOST and ATTEN at the same frequency: big lows that stay clear",
          "ATTEN を 0〜6 で動かし、低域の上の濁りが消える所で止める", "Move ATTEN between 0 and 6 and stop where the mud above the lows clears",
          "BOOST を上げすぎるとベースの場所を取る。サブウーファーかヘッドホンでも確認", "Too much BOOST takes the bass's place; check on a sub or headphones",
          -1, analogPV (5, { { 0, 60.0f }, { 1, 5.0f }, { 2, 4.0f }, { 3, 5000.0f }, { 4, 2.0f }, { 5, 1.5f } }, 5.0f), {}, 5 },

        { 3, "ミックスの柔らかい高域（チューブ・プログラム）", "Soft top on the mix (tube program)",
          "真空管の倍音と一緒に、柔らかく広い高域と軽い低域の厚みを足す", "Adds a soft, wide top and a little low-end weight with tube harmonics",
          "HIGH BOOST を 0〜4 で動かす。BANDWIDTH は Broad 寄りから", "Move HIGH BOOST between 0 and 4; start BANDWIDTH towards broad",
          "DRIVE を上げると倍音が増える。マスターでは 6 dB まで", "More DRIVE = more harmonics; on a master stay under 6 dB",
          -1, analogPV (5, { { 0, 30.0f }, { 1, 2.0f }, { 2, 1.0f }, { 3, 12000.0f }, { 4, 3.0f }, { 5, 2.5f }, { 6, 20000.0f }, { 7, 1.0f } }, 4.0f), {}, 5 },

        { 1, "ボーカルの存在感（ブリティッシュ・コンソールA）", "Vocal presence (British console A)",
          "トランスの厚みと一緒に、ボーカルを前に出す", "Brings the vocal forward together with transformer weight",
          "MID の FREQ を 1.6k〜4.8k で切り替え、言葉が前に出る所を選ぶ", "Step MID's FREQ between 1.6k and 4.8k and pick where the words come forward",
          "MID は幅が広いので +4 dB で十分。上げすぎると硬くなる", "MID is wide, so +4 dB is plenty; more turns hard",
          -1, analogPV (6, { { 0, 80.0f }, { 1, 110.0f }, { 2, -2.0f }, { 3, 3200.0f }, { 4, 3.0f }, { 5, 3.0f } }, 8.0f), {}, 6 },

        { 1, "スネアの胴と抜け（ブリティッシュ・コンソールA）", "Snare body and crack (British console A)",
          "スネアの胴鳴りと、アタックの抜けを同時に足す", "Adds the snare's body and the crack of its attack together",
          "LOW の GAIN を 0〜+4 dB で動かし、胴の太さを決める", "Move LOW GAIN between 0 and +4 dB to set the body",
          "DRIVE を上げるとアタックが丸くなる。好みで 6〜12 dB", "More DRIVE rounds the attack; 6-12 dB to taste",
          -1, analogPV (6, { { 0, 80.0f }, { 1, 220.0f }, { 2, 3.0f }, { 3, 4800.0f }, { 4, 3.0f }, { 5, 2.0f } }, 10.0f), {}, 6 },

        { 1, "キックの整形（ブリティッシュ・コンソールE）", "Kick shaping (British console E)",
          "低域のふくらみ、中低域の箱鳴り、ビーターのアタックを素早く整える", "Quickly shapes the low thump, the boxy low-mids and the beater's click",
          "LMF の FREQ を 250〜500 Hz で動かし、箱鳴りが消える所で止める", "Sweep LMF's FREQ between 250 and 500 Hz and stop where the box sound goes",
          "G タイプはゲインで Q が変わる。大きく削ると鋭くなる", "The G type changes Q with gain: big cuts get sharp",
          -1, analogPV (7, { { 9, 4.0f }, { 10, 60.0f }, { 11, 1.0f }, { 6, -4.0f }, { 7, 350.0f }, { 8, 1.5f }, { 3, 3.0f }, { 4, 3500.0f }, { 5, 1.5f }, { 12, 30.0f }, { 14, 1.0f } }, 2.0f), {}, 7 },

        { 1, "ボーカルの外科的カット（ブリティッシュ・コンソールE）", "Surgical vocal cuts (British console E)",
          "こもりと耳障りな帯域を狭く削り、高域を明るくする", "Cuts the mud and harsh spots narrowly and brightens the top",
          "HMF の FREQ を 2〜4 kHz で動かし、耳に痛い所で止める", "Sweep HMF's FREQ between 2 and 4 kHz and stop at the harsh spot",
          "E タイプは Q が一定。Q を上げすぎると不自然な穴になる", "The E type keeps Q constant; too high a Q leaves an unnatural hole",
          -1, analogPV (7, { { 12, 90.0f }, { 6, -3.0f }, { 7, 300.0f }, { 8, 2.0f }, { 3, -2.0f }, { 4, 2800.0f }, { 5, 3.0f }, { 0, 3.0f }, { 1, 10000.0f }, { 2, 0.0f }, { 14, 0.0f } }, 2.0f), {}, 7 },

        { 1, "スネアのアタック（アメリカン・コンソール）", "Snare attack (American console)",
          "比例 Q の押し出しで、スネアの胴とアタックを前に出す", "Pushes the snare's body and attack forward with proportional Q",
          "MID のゲインを 0〜+6 dB で切り替え、アタックの量を決める", "Step MID's gain between 0 and +6 dB to set the attack",
          "比例 Q なので大きく上げても濁りにくいが、+9 dB 以上は硬い", "Proportional Q keeps big boosts clean, but +9 dB and up turns hard",
          -1, analogPV (8, { { 0, 200.0f }, { 1, 4.0f }, { 2, 0.0f }, { 3, 5000.0f }, { 4, 4.0f }, { 5, 10000.0f }, { 6, 2.0f }, { 7, 1.0f } }, 8.0f), {}, 8 },

        { 3, "ミックスバスのパンチ（アメリカン・コンソール）", "Mix bus punch (American console)",
          "ディスクリート回路の押し出しで、ミックス全体に前に出る力を足す", "Adds forward punch to the whole mix with discrete-circuit push",
          "LOW と HIGH を +2 dB で、シェルフとピークを切り替えて比べる", "Set LOW and HIGH to +2 dB and compare shelf and peak",
          "バスでは各 2〜4 dB まで。DRIVE を上げたら OUT で音量をそろえる", "On a bus keep each at 2-4 dB; match the level with OUT after raising DRIVE",
          -1, analogPV (8, { { 0, 50.0f }, { 1, 2.0f }, { 2, 1.0f }, { 3, 3000.0f }, { 4, 2.0f }, { 5, 12500.0f }, { 6, 2.0f }, { 7, 1.0f } }, 6.0f), {}, 8 },
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
    int model = -1;
};
inline const std::vector<Preset>& presets()
{
    static const std::vector<Preset> out = []
    {
        std::vector<Preset> v;
        for (auto& d : presetDefs())
        {
            Preset p { d.cat, d.nameJa, d.nameEn, d.goalJa, d.goalEn, d.moveJa, d.moveEn, d.cautionJa, d.cautionEn, d.highlight, {}, d.model };
            if (d.slots.empty()) p.slots.push_back ({ d.base });
            else for (auto& sl : d.slots) { PV all = d.base; all.insert (all.end(), sl.begin(), sl.end()); p.slots.push_back ({ all }); }
            v.push_back (std::move (p));
        }
        return v;
    }();
    return out;
}

// factory presets in the order of the menu (by use: category by category), for the < > buttons
inline const std::vector<int>& presetOrder()
{
    static const std::vector<int> o = []
    {
        std::vector<int> v;
        for (int c = 0; c < 6; ++c) for (int i = 0; i < (int) presets().size(); ++i) if (presets()[(size_t) i].category == c) v.push_back (i);
        return v;
    }();
    return o;
}
