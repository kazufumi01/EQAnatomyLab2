#pragma once
#include "EQCore.h"
#include <vector>

// =============================================================================
//  Lessons (the first 22 in the same order and settings as the web lab). Each has steps and a
//  one-line "why", in Japanese and English, plus the EQ state it loads.
// =============================================================================
struct LessonBand { bool on; eqcore::Type type; double f, g, q; int slope; bool dyn = false; float thr = -24, range = -6;
                    bool lin = false; int ch = 0; float below = 0, tb = 0; int src = 0; };

struct LessonSat { bool on = false; int type = 0; float lo = 200, hi = 5000, drive = 9; float mix = 100; int os = 0; };

struct Lesson
{
    const char* titleJa; const char* titleEn;
    std::vector<const char*> stepsJa, stepsEn;
    const char* whyJa; const char* whyEn;
    int mode, res, qmode;                      // -1 res = keep
    std::vector<LessonBand> bands;             // the rest are switched off
    int select = 0;
    LessonSat sat {};
    std::vector<std::pair<const char*, float>> extra {};     // Lab 2: any other parameter (RES, bell slope …), after the bands
};

inline const std::vector<Lesson>& lessons()
{
    using T = eqcore::Type;
    static const std::vector<Lesson> L =
    {
        { "ベルの基本：周波数・ゲイン・Q", "Bell basics: frequency, gain, Q",
          { "1 kHz を +9 dB 持ち上げた状態で再生する", "点を左右に動かして周波数、上下でゲインを変える", "左右の幅ハンドル（または Shift＋上下ドラッグ、ホイール）で Q を変える", "Delta を押して、足した成分だけを聴く" },
          { "Play with 1 kHz boosted by +9 dB", "Drag the dot sideways for frequency, up and down for gain", "Change Q with the width handles (or Shift + drag, or the wheel)", "Press Delta to hear only what the EQ adds" },
          "Q 1 ≒ 1.4 オクターブ、Q 4 ≒ 0.36 オクターブ。Q が高いほど音程感が出る", "Q 1 ≈ 1.4 octaves, Q 4 ≈ 0.36 octaves. Higher Q sounds more pitched",
          0, -1, 0, { { true, T::Bell, 1000, 9, 1, 3 } } },

        { "一定Qと比例Q", "Constant Q vs proportional Q",
          { "2.5 kHz のベルを +3 dB と +12 dB で行き来する", "ヘッダーの Q を Const と Prop で切り替える", "塗りの形（帯域幅）の変わり方を比べる" },
          { "Move a 2.5 kHz bell between +3 dB and +12 dB", "Switch Q between Const and Prop in the header", "Compare how the shaded width changes" },
          "比例Qは小さなブーストを広く、大きなカットを細くする（API・SSL E系の振る舞い）", "Proportional Q keeps small boosts broad and big cuts narrow (API / SSL E style)",
          0, -1, 0, { { true, T::Bell, 2500, 3, 1, 3 } } },

        { "シェルフの「肩」とレゾナンス", "Shelf shoulders and resonance",
          { "120 Hz のローシェルフ +6 dB を聴く", "Q を 0.7 から 1.5 まで上げる", "境目にできる山と、すぐ上のへこみを確かめる" },
          { "Listen to a +6 dB low shelf at 120 Hz", "Raise Q from 0.7 to 1.5", "Find the bump at the corner and the dip just above it" },
          "Q 0.5 前後はなだらか、1.5 前後は低域に芯が出てすぐ上の濁りが減る", "Around Q 0.5 is smooth; around 1.5 firms up the low end and clears the mud above",
          0, -1, 0, { { true, T::LowShelf, 120, 6, 0.707, 3 } } },

        { "Pultec トリック", "The Pultec trick",
          { "60 Hz のブーストとカットのシェルフを重ねた状態で聴く", "バンド 2 をオフにして、ただのブーストと比べる", "45 Hz の山と 100 Hz のへこみを見る" },
          { "Listen with a boosting and a cutting shelf, both at 60 Hz", "Switch band 2 off and compare with a plain boost", "Find the bump near 45 Hz and the dip near 100 Hz" },
          "傾き（Q）が違う2つのシェルフの差し引きで、量感を足しつつもたつきを減らす", "Two shelves with different slopes add weight while removing flab",
          1, -1, 0, { { true, T::LowShelf, 60, 8, 1.2, 3 }, { true, T::LowShelf, 60, -6, 0.5, 3 } } },

        { "カットのスロープと位相の回転", "Cut slope and phase rotation",
          { "100 Hz のローカットをかける", "ポップアップでスロープを 6 → 48 dB/oct と上げる", "黄色の点線（位相）が最低域で回る量を見る", "GD に切り替えて、カットオフ付近の遅れを見る" },
          { "Apply a 100 Hz low cut", "Raise the slope from 6 to 48 dB/oct in the pop-up", "Watch how far the yellow dotted phase line turns at the bottom", "Switch to GD to see the delay near the cutoff" },
          "6 dB/oct ごとに位相が 90° 回る。48 dB/oct なら 720°", "Every 6 dB/oct adds 90° of rotation, so 48 dB/oct gives 720°",
          0, -1, 0, { { true, T::LowCut, 100, 0, 0.707, 1 } } },

        { "最小位相ではゲインと位相はセット", "In minimum phase, gain and phase move together",
          { "200 Hz を細く −6 dB カットする", "へこみの前後で位相の線が上下に振れるのを見る", "Zero latency と Analog を切り替えて、低中域では位相が同じことを確かめる" },
          { "Cut 200 Hz by −6 dB with a narrow Q", "Watch the phase line swing either side of the dip", "Switch Zero latency and Analog: the phase matches in the lows and mids" },
          "最小位相では振幅のカーブが決まると位相のカーブも一つに決まる", "In minimum phase, the magnitude curve fixes the phase curve",
          0, -1, 0, { { true, T::Bell, 200, -6, 4, 3 } } },

        { "リニア位相：位相は動かず、応答が前に広がる", "Linear phase: no phase shift, but the response spreads forward",
          { "前のレッスンと同じカーブをリニア位相で聴く", "位相の線が 0 のまま動かないのを確かめる", "インパルスの赤い部分（0 ms より左）を見る" },
          { "Listen to the previous curve in linear phase", "The phase line stays at 0", "Look at the red part of the impulse (left of 0 ms)" },
          "左右対称の FIR は、応答が音より前にも広がる", "A symmetric FIR spreads its response before the sound too",
          2, 1, 0, { { true, T::Bell, 200, -6, 4, 3 } } },

        { "プリリンギングを聴く", "Hearing pre-ringing",
          { "70 Hz を +15 dB・Q 8 でブーストする", "モードを Linear · High にする", "Pre-ring solo を押して、キックの前の「ブォーン」を聴く", "Zero latency に戻し、音が消えるのを確かめる" },
          { "Boost 70 Hz by +15 dB at Q 8", "Set the mode to Linear · High", "Press Pre-ring solo and hear the swell before each kick", "Go back to Zero latency and hear it disappear" },
          "低い周波数・高い Q・急なカットほどプリリンギングは長くなる", "Lower frequencies, higher Q and steeper cuts make pre-ringing longer",
          2, 2, 0, { { true, T::Bell, 70, 15, 8, 3 } } },

        { "解像度（タップ数）とレイテンシー", "Resolution (taps) and latency",
          { "40 Hz・48 dB/oct のローカットをリニア位相・低で聴く", "解像度を 低 → 中 → 高 と上げる", "カーブの精度と、ヘッダーのレイテンシー表示を比べる" },
          { "Listen to a 40 Hz, 48 dB/oct low cut in linear phase, low resolution", "Raise the resolution from Low to Mid to High", "Compare the accuracy with the latency shown in the header" },
          "FIR が長いほど低域は正確になるが、遅れとプリリンギングも伸びる", "A longer FIR rebuilds the lows more accurately but adds delay and pre-ringing",
          2, 0, 0, { { true, T::LowCut, 40, 0, 0.707, 5 } } },

        { "ナイキスト付近のクランプ", "Cramping near Nyquist",
          { "16 kHz を +6 dB・Q 2 で持ち上げる", "Zero latency と Analog を切り替える", "カーブの右側（16〜20 kHz）の形の違いを見る" },
          { "Boost 16 kHz by +6 dB at Q 2", "Switch between Zero latency and Analog", "Compare the right side of the curve (16–20 kHz)" },
          "双一次変換は高域をナイキストに押し込むため、形がゆがむ", "The bilinear transform squeezes the top into Nyquist and bends the shape",
          0, -1, 0, { { true, T::Bell, 16000, 6, 2, 3 } } },

        { "オールパス：振幅を変えずに位相だけ回す", "All-pass: turning phase without changing level",
          { "100 Hz のオールパスをかける（カーブは平ら）", "位相の線が 100 Hz で 180°、全体で 360° 回るのを見る", "Delta で、時間方向にずれた成分を聴く", "Linear にすると効果がなくなるのを確かめる" },
          { "Apply an all-pass at 100 Hz (the curve stays flat)", "Watch the phase reach 180° at 100 Hz and 360° overall", "Press Delta to hear what shifted in time", "Switch to Linear: the effect disappears" },
          "リニア位相は振幅しか再現しないため、オールパスは何もしなくなる", "Linear phase only rebuilds the magnitude, so an all-pass does nothing there",
          0, -1, 0, { { true, T::AllPass, 100, 0, 0.7, 3 } } },

        { "実践：どの方式を選ぶか", "In practice: which mode to choose",
          { "±1.5 dB 程度のマスタリング的な補正を聴く", "3 方式を切り替え、プリリンギングとレイテンシーを比べる", "ミックス中の処理なら Zero latency、緩やかな補正なら Linear も選択肢" },
          { "Listen to a gentle mastering move of about ±1.5 dB", "Switch modes and compare pre-ringing and latency", "Zero latency for most mixing; Linear is an option for gentle moves" },
          "急なローカットや低域の強いブーストにリニア位相は不向き", "Avoid linear phase for steep low cuts and strong low boosts",
          2, 1, 0, { { true, T::LowShelf, 90, 1, 0.6, 3 }, { true, T::Bell, 3200, -1, 0.7, 3 }, { true, T::HighShelf, 12000, 1.5, 0.6, 3 } }, 1 },

        { "帯域サチュレーション：歪ませる場所を選ぶ", "Band saturation: choosing where to distort",
          { "1.5〜6 kHz だけを Tape で Drive 15 dB 歪ませた状態で聴く", "Sat Δ を押して、足された倍音だけを聴く", "Tape・Tube・Clip を切り替え、倍音の質感を比べる", "グラフ上部の SAT ハンドルで帯域を 100 Hz まで広げ、濁り方を聴き比べる" },
          { "Listen with only 1.5–6 kHz driven 15 dB through Tape", "Press Sat Δ to hear only the added harmonics", "Switch Tape, Tube and Clip and compare the texture", "Drag the SAT handles at the top of the graph down to 100 Hz and hear how it muddies" },
          "帯域は「どこを歪ませるか」。生まれる倍音はその上にも広がり、帯域が広いほど混変調で濁る", "The band picks what gets driven; the harmonics land above it, and a wider band adds intermodulation mud",
          0, -1, 0, {}, 0, { true, 0, 1500, 6000, 15 } },

        { "ダイナミック EQ：うるさい時だけ削る", "Dynamic EQ: cutting only when it gets loud",
          { "3.5 kHz のベルをダイナミックにした状態で再生する（静的なゲインは 0 dB）", "ポップアップの検出メーターがしきい値（THR）を越えた時だけ、カーブが下がるのを見る", "THR を下げて、効き始めるタイミングの違いを聴く", "RANGE を +にすると、大きい時だけ持ち上がる（アップワード）" },
          { "Play with a dynamic bell at 3.5 kHz (static gain 0 dB)", "Watch the curve dip only when the detector meter in the pop-up passes THR", "Lower THR and hear it start to work earlier", "Set RANGE positive to boost only when it gets loud (upward)" },
          "静的な EQ は常に同じだけ変える。ダイナミック EQ は「その帯域が出過ぎた瞬間」だけ変えるので、普段の音色を保てる", "A static EQ always changes the same amount; a dynamic one only acts when the band jumps out, so the normal tone stays",
          0, -1, 0, { { true, T::Bell, 3500, 0, 2.5, 3, true, -30.0f, -6.0f } }, 0 },

        { "Min と Lin を混ぜる：プリリンギングの予算配分", "Mixing Min and Lin: budgeting pre-ringing",
          { "40 Hz の急なローカットと 70 Hz のブーストは Min、上の 2 つは Lin の状態で聴く", "インパルスの赤い部分（プリリンギング）が小さいことを確かめる", "バンド 1 と 2 も Lin にして、赤い部分が一気に伸びるのを見る", "Pre-ring solo で、伸びた分を耳で確かめる" },
          { "Listen with the steep 40 Hz cut and the 70 Hz boost on Min, the upper two on Lin", "Check the red (pre-ringing) part of the impulse is small", "Set bands 1 and 2 to Lin too and watch the red part grow", "Use Pre-ring solo to hear what grew" },
          "プリリンギングの長さは Q ÷（π×周波数）でほぼ決まる。低域・高い Q・急なカットだけ Min にすれば、残りは位相を回さずに済む", "Pre-ringing length is roughly Q / (π × frequency): keep lows, high Q and steep cuts on Min and the rest can stay phase-free",
          0, 1, 0, { { true, T::LowCut, 40, 0, 0.707, 5 }, { true, T::Bell, 70, 3, 2, 3 }, { true, T::Bell, 2500, -1.5, 0.8, 3, false, -24, -6, true }, { true, T::HighShelf, 10000, 2, 0.6, 3, false, -24, -6, true } }, 2 },

        { "M/S：真ん中と左右を別々に整える", "M/S: shaping centre and sides separately",
          { "Side の 8 kHz シェルフ +3 dB と、Mid の 300 Hz −2 dB を聴く", "グラフの実線（Mid）と水色の破線（Side）を見比べる", "右端の CORR を見ながら、Side を +6 dB まで上げる", "CORR が 0 に近づいたら、モノラルにしたときの抜けを確かめる" },
          { "Listen to a +3 dB side shelf at 8 kHz and −2 dB on the mid at 300 Hz", "Compare the solid (mid) and cyan dashed (side) curves", "Raise the side shelf to +6 dB while watching CORR on the right", "When CORR nears 0, check what disappears in mono" },
          "Side を持ち上げると広がるが、モノラルでは打ち消し合う成分も増える。広がりは CORR とモノラル試聴で確かめる", "Lifting the side widens, but more of it cancels in mono. Check width with CORR and a mono listen",
          0, -1, 0, { { true, T::HighShelf, 8000, 3, 0.7, 3, false, -24, -6, false, 2 }, { true, T::Bell, 300, -2, 1, 3, false, -24, -6, false, 1 } }, 0 },

        { "ダイナミック EQ の BELOW：小さい時だけ持ち上げる", "Dynamic EQ BELOW: lifting only the quiet parts",
          { "3 kHz のベル（静的なゲイン 0 dB）に BELOW +4 dB をかけた状態で聴く", "浮き窓の検出メーターが THR を下回った時だけ、カーブが上がるのを見る", "ABOVE を −4 dB にして、大きい時は削り、小さい時は持ち上げる形にする", "BELOW を +8 dB まで上げ、ブレスやノイズまで持ち上がる限界を聴く" },
          { "Listen to a 3 kHz bell (static 0 dB) with BELOW at +4 dB", "Watch the curve rise only when the detector meter drops under THR", "Set ABOVE to −4 dB: cut when loud, lift when quiet", "Push BELOW to +8 dB and hear where breaths and noise come up too" },
          "ABOVE は出過ぎを抑え、BELOW は埋もれた部分を引き上げる。BELOW は −60 dB 以下では反応しないが、上げすぎると部屋鳴りも目立つ", "ABOVE tames what jumps out; BELOW lifts what gets buried. BELOW ignores levels under −60 dB, but too much brings up the room",
          0, -1, 0, { { true, T::Bell, 3000, 0, 1.5, 3, true, -30.0f, 0.0f, false, 0, 4.0f } }, 0 },

        { "トランジェントとボディ：EQ をどこに効かせるか", "Transient and body: where the EQ acts",
          { "4 kHz +6 dB のベルを、アタックだけに効かせた状態（スライダーを左の TRANSIENT 側へ寄せた所）で聴く（浮き窓に T100 / B20 と出る）", "スライダーを中央に戻し、普通の EQ と比べる", "反対に右の BODY 側へ寄せて、余韻だけを明るくする（T20 / B100）", "ドラムやアコギなど、アタックのはっきりした音で試す" },
          { "Listen to a +6 dB bell at 4 kHz acting mostly on the attack (slider towards TRANSIENT on the left: the panel shows T100 / B20)", "Centre the slider and compare with a normal EQ", "Move it towards BODY on the right so it brightens only the sustain (T20 / B100)", "Try drums or acoustic guitar, anything with clear attacks" },
          "同じブーストでも、アタックだけなら抜けが良くなり、余韻だけなら響きが明るくなる。差を極端にすると音量が揺れて歪みやポンピングが出る", "The same boost on the attack adds bite; on the body it brightens the ring. Extreme settings wobble the level and can pump",
          0, -1, 0, { { true, T::Bell, 4000, 6, 1.2, 3, false, -24, -6, false, 0, 0, -80.0f } }, 0 },

        { "カーブの性格：同じ数値でも形が違う", "Curve character: same numbers, different shapes",
          { "1 kHz +6 dB・Q 1 のベルと、100 Hz +6 dB のローシェルフを Clean Const で見る", "ヘッダーの CURVE を British A → US 500 → Console E と切り替える", "Tube Passive で、ローシェルフのブーストとカットを重ねた形を見る（バンド 3 をオン）", "質感まで比べるなら、SAT の Transformer・Tube・Class A を足す" },
          { "Look at a +6 dB bell at 1 kHz (Q 1) and a +6 dB low shelf at 100 Hz in Clean Const", "Switch CURVE in the header: British A, US 500, Console E", "In Tube Passive, switch on band 3 to stack a low boost and cut", "For texture as well, add SAT Transformer, Tube or Class A" },
          "アナログ EQ の個性は、Q の決まり方とシェルフの肩の形に表れる。カーブの傾向だけで、機材の音そのものではない", "Analog EQ character shows in how Q behaves and in the shelf shoulders. These are shape tendencies, not the sound of the hardware",
          0, -1, 0, { { true, T::Bell, 1000, 6, 1, 3 }, { true, T::LowShelf, 100, 6, 0.7, 3 }, { false, T::LowShelf, 100, -4, 0.7, 3 } }, 0 },

        { "エキサイターとオーバーサンプリング", "Exciter and oversampling",
          { "Exciter で 5 kHz より上に倍音を足した状態（MIX 40 %）を聴く", "Sat Δ で、足された倍音だけを聴く", "OVERSAMPLING を 1x にして DRIVE を上げ、高域のザラつき（折り返しノイズ）を聴く", "2x・4x に戻して、ザラつきが減るのを確かめる" },
          { "Listen to Exciter adding harmonics above 5 kHz (MIX 40 %)", "Use Sat Δ to hear only the added harmonics", "Set OVERSAMPLING to 1x and raise DRIVE: hear the high-frequency grit (aliasing)", "Go back to 2x / 4x and hear the grit drop" },
          "倍音がナイキストを超えると低い周波数に折り返し、音楽と関係のない成分になる。高いレートで歪ませると、その分が減る", "Harmonics above Nyquist fold back as unrelated tones; distorting at a higher rate removes most of them",
          0, -1, 0, {}, 0, { true, 5, 5000, 16000, 15, 40, 1 } },

        { "サイドチェーン：別のトラックに場所を譲る", "Sidechain: making room for another track",
          { "ギターなどに挿し、ホストのサイドチェーン欄でボーカルを入力元にする", "2.5 kHz のベル（0 dB）の DETECT が SC になっているのを確かめる", "ボーカルが入った時だけカーブが下がるのを見る", "ABOVE を −3 〜 −6 dB で調整し、ボーカルの明瞭さとギターの存在感を両立させる" },
          { "Insert on a guitar and pick the vocal as the host's sidechain source", "Check the 2.5 kHz bell (0 dB) has DETECT set to SC", "Watch the curve dip only while the vocal sings", "Adjust ABOVE between −3 and −6 dB to keep both the vocal clear and the guitar present" },
          "常に削ると楽器が細くなる。ボーカルが鳴っている時だけ、ぶつかる帯域を譲れば、普段の音色は保てる", "Cutting all the time thins the instrument. Giving way only while the vocal sings keeps its normal tone",
          0, -1, 0, { { true, T::Bell, 2500, 0, 1.4, 3, true, -36.0f, -4.0f, false, 0, 0, 0, 1 } }, 0 },

        { "耳トレ：帯域を聴き分ける", "Ear training: naming the band",
          { "ヘッダーの EAR を押す（今の EQ は一時的に外れる）", "B（EQ）と A（原音）を聴き比べ、持ち上がった帯域を選ぶ", "Normal で 10 問中 7 問を目標に、慣れたら Hard（1/3 オクターブ・±3 dB）へ", "Cut や Both でも同じように練習する" },
          { "Press EAR in the header (your EQ is bypassed for now)", "Compare B (EQ) and A (original) and pick the boosted band", "Aim for 7 out of 10 on Normal, then try Hard (1/3 octave, ±3 dB)", "Practise Cut and Both the same way" },
          "周波数を言葉と結びつけておくと、ミックス中に「どこを触るか」をすぐ決められる。ブーストより、カットの方が聴き分けは難しい", "Linking frequencies to words lets you decide where to reach while mixing. Cuts are harder to hear than boosts",
          0, -1, 0, {}, 0 },

        { "レゾナンス・サプレッサーの基本", "Resonance suppressor basics",
          { "レッスン欄右上の ▶ Sample で「Voice + res」を鳴らす（3.15 kHz に鋭い共鳴を入れてある）", "グラフ上の Resonance を押し、白い線が 3 kHz 付近でピンクの線を超えているのを見る", "RES タブの Δ で、削っている音だけを聴く", "THRESH を 2 dB まで下げ、声の芯まで削れて音がやせるのを確かめてから 6 dB に戻す" },
          { "Play 'Voice + res' with ▶ Sample at the top right (a sharp resonance sits at 3.15 kHz)", "Press Resonance above the graph and see the white line rise above the pink one near 3 kHz", "Use Δ on the RES tab to hear only what is taken", "Lower THRESH to 2 dB, hear it eat into the voice and thin it, then go back to 6 dB" },
          "固定の EQ は常に削るが、サプレッサーは「周りより突き出た時だけ」削る。動く共鳴には強く、動かない音色の特徴には弱い", "A static EQ cuts all the time; the suppressor cuts only while something sticks out of its surroundings. Strong on moving resonances, weak on fixed tone",
          0, -1, 0, {}, 0, {}, { { "rs_on", 1 }, { "rs_depth", 6 }, { "rs_thr", 6 }, { "rs_lo", 300 }, { "rs_hi", 10000 }, { "rs_sel", 50 } } },

        { "固定のベル と サプレッサー", "A fixed bell vs the suppressor",
          { "▶ Sample の「Voice + res」を鳴らす。3.15 kHz に −8 dB・Q 8 のベルが入っている", "バンド 3 をダブルクリックでオフ・オンし、共鳴が消えるのを確かめる", "バンド 3 をオフにして RES タブの丸を押し、同じ共鳴がどう抑えられるか比べる", "Delta と RES Δ で、それぞれが何を削っているかを聴き比べる" },
          { "Play 'Voice + res'. A −8 dB, Q 8 bell sits at 3.15 kHz", "Double-click band 3 off and on: the resonance goes", "Turn band 3 off and switch RES on with its circle: compare how the same resonance is held down", "Compare what each takes with Delta and RES Δ" },
          "共鳴の周波数が決まっているならベルが確実で、遅延もない。音程で動くならサプレッサー。両方を薄く併用するのもよくある", "If the resonance stays put, a bell is precise and adds no latency; if it moves with pitch, the suppressor. A little of both is common",
          0, -1, 0, { { false, T::Bell, 1000, 0, 1, 3 }, { false, T::Bell, 1000, 0, 1, 3 }, { true, T::Bell, 3150, -8, 8, 3 } }, 2, {}, { { "rs_on", 0 } } },

        { "スレッショルド曲線と M/S", "The threshold curve and M/S",
          { "RES タブの Graph を押し、EQ のグラフ上でスレッショルド曲線を編集する", "4 番の点（3 kHz）を下げて、その帯域だけを強く削る", "2 番の点（300 Hz）を上げて、低中域の太さを守る", "RES タブのチャンネルを Mid と Side に切り替え、Δ で違いを聴く" },
          { "Press Graph on the RES tab and edit the threshold curve over the EQ graph", "Pull point 4 (3 kHz) down to work harder there", "Push point 2 (300 Hz) up to protect the low-mid body", "Switch the RES channels to Mid and to Side and hear the difference with Δ" },
          "全帯域に同じ基準を使うと、低域の豊かさまで共鳴と見なされやすい。曲線で帯域ごとに感度を変えると、削りたい所だけに効く", "One threshold everywhere tends to treat low-end richness as resonance too. Shaping it per band makes it work only where you want",
          0, -1, 0, {}, 0, {}, { { "rs_on", 1 }, { "rs_mode", 3 }, { "rs_depth", 6 }, { "rs_thr", 5 }, { "rs_n4_g", -3 }, { "rs_n2_g", 4 } } },

        { "ベルの傾き：12・24・48", "Bell slope: 12, 24, 48",
          { "1 kHz のベル +9 dB（傾き 12）を聴く", "浮き窓の SLOPE を 24、48 に変え、頭が平らに、裾が急になるのを見る", "Q を動かして、平らな部分の幅が変わるのを確かめる", "Phase を見て、48 では山の両端で位相が大きく回るのを確かめる" },
          { "Listen to a +9 dB bell at 1 kHz (slope 12)", "Set SLOPE in the floating panel to 24 and 48: the top flattens and the sides steepen", "Move Q and watch the width of the flat part change", "Watch the phase: with 48 it turns hard at both edges" },
          "帯域をまとめて持ち上げたいがその外は触りたくない時に、平らな頭と急な裾が役に立つ。代わりに位相の回りは大きくなる", "A flat top and steep sides help when you want to lift a whole range and leave the rest untouched; the phase turns more in return",
          0, -1, 0, { { true, T::Bell, 1000, 9, 1, 3 } }, 0, {}, { { "b1_bslope", 0 } } },
    };
    return L;
}
