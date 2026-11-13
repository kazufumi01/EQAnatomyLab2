#pragma once
// Included from Components.h (after ParamHelp, setHelp, theme). The analog EQ model's panel in the lower view.
#include "AnalogEQ.h"

// One rotary control: drag up/down (Shift = fine), wheel, double-click / Option+click = the model's default
class AnaKnob : public juce::Component
{
public:
    AnaKnob (ParamHelp& h) : ph (h) { markReset (*this); }
    juce::String id, label, valueText; int steps = 0; float def = 0.5f; bool bipolar = false;
    std::function<juce::String (float)> fmt;
    juce::Colour face = juce::Colours::black, ink = juce::Colours::white, accent = juce::Colours::orange;
    float get01() const { if (auto* p = ph.get (id)) return p->convertTo0to1 (ph.value (id)); return 0.0f; }
    void set01 (float v) const { if (auto* p = ph.get (id)) ph.set (id, p->convertFrom0to1 (juce::jlimit (0.0f, 1.0f, v))); }
    void paint (juce::Graphics& g) override
    {
        const float v = get01();
        auto r = getLocalBounds().toFloat();
        const float lh = juce::jlimit (9.0f, 12.0f, r.getHeight() * 0.18f);
        auto top = r.removeFromTop (lh), bot = r.removeFromBottom (lh);
        g.setColour (ink.withAlpha (0.85f)); g.setFont (theme::ui (lh - 1.5f, true)); g.drawText (label, top, juce::Justification::centred);
        const float d = std::min (r.getWidth(), r.getHeight()) - 4.0f, cx = r.getCentreX(), cy = r.getCentreY(), rad = d * 0.5f;
        const float a0 = juce::MathConstants<float>::pi * 1.25f, a1 = juce::MathConstants<float>::pi * 2.75f, ang = a0 + (a1 - a0) * v;
        // scale ticks (steps or 11 marks)
        const int n = steps > 1 ? steps : 11;
        g.setColour (ink.withAlpha (0.45f));
        for (int i = 0; i < n; ++i)
        {
            const float a = a0 + (a1 - a0) * (float) i / (float) (n - 1);
            const float r0 = rad + 1.5f, r1 = rad + (i == 0 || i == n - 1 || (bipolar && i == n / 2) ? 5.0f : 3.5f);
            g.drawLine (cx + r0 * std::sin (a), cy - r0 * std::cos (a), cx + r1 * std::sin (a), cy - r1 * std::cos (a), 1.0f);
        }
        // knob body
        juce::ColourGradient grad (face.brighter (0.35f), cx - rad * 0.4f, cy - rad * 0.5f, face.darker (0.5f), cx + rad * 0.5f, cy + rad * 0.6f, true);
        g.setGradientFill (grad); g.fillEllipse (cx - rad, cy - rad, d, d);
        g.setColour (juce::Colours::black.withAlpha (0.5f)); g.drawEllipse (cx - rad, cy - rad, d, d, 1.0f);
        g.setColour (over || drag ? accent : ink);
        g.drawLine (cx + rad * 0.25f * std::sin (ang), cy - rad * 0.25f * std::cos (ang), cx + rad * 0.92f * std::sin (ang), cy - rad * 0.92f * std::cos (ang), 2.2f);
        g.setColour (over || drag ? accent : ink.withAlpha (0.9f)); g.setFont (theme::mono (lh - 1.0f, true));
        g.drawText (fmt ? fmt (v) : juce::String (v, 2), bot, juce::Justification::centred);
    }
    void mouseEnter (const juce::MouseEvent&) override { over = true; repaint(); }
    void mouseExit (const juce::MouseEvent&) override { over = false; repaint(); }
    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.mods.isAltDown()) { reset(); return; }
        ph.lastId = id; ph.proc.undo.beginNewTransaction(); ph.begin (id); drag = true; start = get01(); y0 = e.getScreenPosition().toFloat().y;
    }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (! drag) return;
        const float y = e.getScreenPosition().toFloat().y, sens = e.mods.isShiftDown() ? 0.0012f : 0.006f;
        start = juce::jlimit (0.0f, 1.0f, start + (y0 - y) * sens); y0 = y;
        set01 (snap (start)); repaint();
    }
    void mouseUp (const juce::MouseEvent&) override { if (drag) { ph.end (id); drag = false; repaint(); } }
    void mouseDoubleClick (const juce::MouseEvent&) override { reset(); }
    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override
    {
        if (w.isInertial || std::abs (w.deltaY) < 0.01f) return;
        const float st = steps > 1 ? 1.0f / (float) (steps - 1) : (e.mods.isShiftDown() ? 0.003f : 0.015f);
        ph.proc.undo.beginNewTransaction(); ph.begin (id);
        set01 (snap (get01() + st * (w.deltaY > 0 ? 1.0f : -1.0f) * (w.isReversed ? -1.0f : 1.0f))); ph.end (id);
        repaint();
    }
private:
    float snap (float v) const { return steps > 1 ? std::round (v * (float) (steps - 1)) / (float) (steps - 1) : v; }
    void reset() { ph.proc.undo.beginNewTransaction(); ph.begin (id); set01 (def); ph.end (id); repaint(); }
    ParamHelp& ph; bool over = false, drag = false; float start = 0, y0 = 0;
};

// A two-position switch (Bell / Shelf, Broad / Sharp)
class AnaSwitch : public juce::Component
{
public:
    AnaSwitch (ParamHelp& h) : ph (h) { markReset (*this); }
    juce::String id, label; juce::StringArray names; float def = 0.0f;
    juce::Colour face = juce::Colours::black, ink = juce::Colours::white, accent = juce::Colours::orange;
    void paint (juce::Graphics& g) override
    {
        const bool on = ph.value (id) > 0.5f;
        auto r = getLocalBounds().toFloat().reduced (1.0f);
        const float lh = juce::jlimit (9.0f, 12.0f, getHeight() * 0.36f);
        g.setColour (ink.withAlpha (0.85f)); g.setFont (theme::ui (lh - 1.5f, true)); g.drawText (label, r.removeFromTop (lh), juce::Justification::centred);
        auto sw = r.reduced (2.0f, 1.0f);
        g.setColour (face.darker (0.6f)); g.fillRoundedRectangle (sw, 3.0f);
        auto half = sw.withWidth (sw.getWidth() * 0.5f); if (on) half.setX (sw.getCentreX());
        g.setColour (accent.withAlpha (0.9f)); g.fillRoundedRectangle (half.reduced (1.5f), 2.5f);
        g.setFont (theme::ui (std::max (8.5f, lh - 2.0f), true));
        for (int i = 0; i < 2; ++i)
        {
            auto t = sw.withWidth (sw.getWidth() * 0.5f).withX (sw.getX() + i * sw.getWidth() * 0.5f);
            g.setColour ((i == 1) == on ? juce::Colours::black : ink.withAlpha (0.7f));
            g.drawText (names[i], t, juce::Justification::centred);
        }
    }
    void mouseDown (const juce::MouseEvent& e) override
    {
        ph.lastId = id; ph.proc.undo.beginNewTransaction();
        ph.setGesture (id, e.mods.isAltDown() ? def : (ph.value (id) > 0.5f ? 0.0f : 1.0f)); repaint();
    }
private:
    ParamHelp& ph;
};

// The panel: model menu, IN, DRIVE, OUT, and the model's controls in columns, on a faceplate in the model's colours
class AnalogPanel : public juce::Component
{
public:
    explicit AnalogPanel (ParamHelp& h) : ph (h), drive (h), out (h)
    {
        for (int i = 0; i < analog::kSlots; ++i) { knobs.add (new AnaKnob (h)); switches.add (new AnaSwitch (h)); addChildComponent (knobs.getLast()); addChildComponent (switches.getLast()); }
        addAndMakeVisible (modelBtn); modelBtn.getProperties().set ("chevron", 1);
        modelBtn.onClick = [this] { modelMenu(); };
        addAndMakeVisible (expBtn); expBtn.setClickingTogglesState (false);
        expBtn.onClick = [this] { expandMenu(); };
        setHelp (expBtn, "EXPAND TO EQ", "このモデルを、メインEQの空いているバンド（B9 から順に）に展開する。展開後もこのパネルで操作でき、つまみの動きがそのままバンドに反映される。\n| 何が起きるか | 中身 |\n| フィルター | モデルの各フィルターが、普通のベル・シェルフ・カットのバンドになる（カーブは Clean Const。形はモデルと同じ） |\n| 回路 | SAT が Analog 系のタイプ・全帯域・同じ DRIVE になる（アナログ段と同じかかり方） |\n| OUT | このパネルの OUT がそのまま効く |\nアナログEQが「普通のEQのどんなバンドの組み合わせか」がグラフの点で見える。展開中にもう一度押すと、切り離す（バンドを残す）か元に戻すかを選べる",
                 "Lays this model out on free main-EQ bands (from B9 up). The panel keeps working after that: every knob move goes straight to those bands.\n| What happens | Detail |\n| Filters | each of the model's filters becomes an ordinary bell, shelf or cut band (curve Clean Const, the same shape as the model) |\n| Circuit | SAT takes the matching Analog type, full band, the same DRIVE (drives exactly like the analog stage) |\n| OUT | this panel's OUT still applies |\nYou see, as dots on the graph, which ordinary bands the analog EQ is made of. Press again while expanded to detach (keep the bands) or undo the expansion");
        addAndMakeVisible (onBtn); onBtn.setClickingTogglesState (false);
        onBtn.onClick = [this] { ph.toggleGesture (pid::anOn); };
        drive.id = pid::anDrive; drive.label = "DRIVE"; drive.def = 4.0f / 24.0f;
        drive.fmt = [] (float v) { return juce::String (v * 24.0f, 1) + " dB"; };
        out.id = pid::anOut; out.label = "OUT"; out.def = 0.5f; out.bipolar = true;
        out.fmt = [] (float v) { const float d = v * 24.0f - 12.0f; return (d > 0.05f ? "+" : "") + juce::String (d, 1) + " dB"; };
        addAndMakeVisible (drive); addAndMakeVisible (out);
        // see-through faceplate: the graph shows behind it (0 = solid, 90 % = nearly clear)
        seeThru.setSliderStyle (juce::Slider::LinearHorizontal); seeThru.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        seeThru.setRange (0.0, 90.0, 5.0); seeThru.setDoubleClickReturnValue (true, 30.0);
        seeThru.setValue ((double) ph.proc.uiState.getProperty ("anAlpha", 30.0), juce::dontSendNotification);
        seeThru.onValueChange = [this] { ph.proc.uiState.setProperty ("anAlpha", seeThru.getValue(), nullptr); repaint(); };
        addAndMakeVisible (seeThru);
        setHelp (seeThru, "SEE-THROUGH", "パネルの透過。上げるほど後ろのグラフ（カーブ・スペアナ・C）が透けて見える。ダブルクリックで 30 %", "How see-through the panel is: more = the graph behind (curves, analyzer, C) shows through. Double-click: 30 %");
        setHelp (onBtn, "ANALOG IN", "アナログEQモデルのオン／オフ。メインEQとレゾナンス・サプレッサーの後ろ、帯域サチュレーターの前に入る。オフの時は処理を通らない（レイテンシーなし）",
                 "Analog EQ model on / off. It sits after the main EQ and the resonance suppressor, before the band saturator. Off = not in the signal path (no latency)");
        setHelp (modelBtn, "ANALOG MODEL", "モデルを選ぶ。選ぶとそのモデルのつまみが初期値に戻り、メインEQの全バンドのカーブ（浮き窓の CURVE）と SAT のタイプも、そのモデルに合わせて変わる（Cmd＋Z で戻せる）。プリセット欄の「アナログEQタイプ別」からも選べる。\n| モデル | タイプ・用途 |\n| Passive Tube Master | 真空管で持ち上げる、4バンドのパッシブ・マスタリングEQ |\n| Precision Master 3-Band | 色付けの少ない、精密なマスタリング用パラメトリック |\n| Fixed-Band Air Lift | 幅の広い固定6バンドと、超高域から持ち上げる AIR |\n| British Stepped Bus | ミックスバスやドラム向けの、ステップ式の英国コンソール系カーブ |\n| 60s Studio 7-Band | 60年代スタジオの自作コンソールEQ（2 dB ステップ） |\n| Vintage Tube Boost/Atten | 50〜60年代の真空管プログラムEQ（同じ低域をブーストとアッテン） |\n| 70s British Inductor | 70年代英国卓の、インダクターと Class A のチャンネルEQ |\n| 80s British Large Console | 80年代の大型コンソールの、4バンド・チャンネルEQ |\n| American Discrete Punch | ステップ式と比例 Q の、米国ディスクリート卓EQ |\n名前は時代・回路・用途のヒント。どれも学習用の汎用モデルで、特定の製品の回路を測ったものではない",
                 "Pick the model. Choosing one resets its controls and sets every main-EQ band's curve (CURVE in the floating panel) and the SAT type to match the model (Cmd+Z undoes). Also in the preset menu under 'By analog EQ type'.\n| Model | Kind / use |\n| Passive Tube Master | passive 4-band mastering EQ with a tube make-up stage |\n| Precision Master 3-Band | clean, exact solid-state mastering parametric |\n| Fixed-Band Air Lift | six wide fixed bands plus an ultra-high AIR shelf |\n| British Stepped Bus | stepped British console curves for the mix bus and drums |\n| 60s Studio 7-Band | a 1960s studio's home-built console EQ, 2 dB steps |\n| Vintage Tube Boost/Atten | a 1950s-60s tube program EQ: boost and attenuate the same lows |\n| 70s British Inductor | the class-A inductor channel EQ of 1970s British desks |\n| 80s British Large Console | the four-band channel EQ of large 1980s consoles |\n| American Discrete Punch | a discrete American console EQ: stepped, proportional Q |\nThe names hint at the era, the circuit and the job. All are generic learning models, not measured circuits of any product");
        setHelp (drive, "ANALOG DRIVE", "モデルの回路（真空管・トランス・Class A）にどれだけ強く入れるか。上げるほど倍音が増え、ピークが丸くなる。音量は自動で合わせる。\n| 値 | 聴こえ方 |\n| 0〜6 dB | ほぼ透明。太さがわずかに増える |\n| 6〜12 dB | 倍音がはっきり。中低域が濃くなる |\n| 12 dB〜 | 歪みとして聴こえる。効果音的 |",
                 "How hard the model's circuit (tube, transformer, class A) is driven. More = more harmonics and rounder peaks; the level is matched.\n| Value | Sounds like |\n| 0-6 dB | nearly transparent, a touch thicker |\n| 6-12 dB | clear harmonics, denser low-mids |\n| 12 dB up | audible distortion, an effect |");
        setHelp (out, "ANALOG OUT", "アナログ段の出力レベル（±12 dB）。オン／オフで音量をそろえて比べるために使う", "The analog stage's output level (±12 dB). Use it to match the level when comparing on / off");
    }
    void refresh()
    {
        const int m = juce::jlimit (0, analog::kModels - 1, (int) ph.value (pid::anModel));
        const bool on = ph.value (pid::anOn) > 0.5f;
        onBtn.setButtonText ("IN"); onBtn.setToggleState (on, juce::dontSendNotification);
        const bool linked = ph.value (pid::anLink) > 0.5f;
        expBtn.setButtonText (linked ? ph.tr ("EQに展開中", "EXPANDED") : ph.tr ("EQに展開", "EXPAND")); expBtn.setToggleState (linked, juce::dontSendNotification);
        seeThru.setValue ((double) ph.proc.uiState.getProperty ("anAlpha", 30.0), juce::dontSendNotification);
        if (m != shown) { shown = m; layoutModel(); }
        modelBtn.setButtonText (juce::String (models()[(size_t) m].name));
        for (auto* c : getChildren()) c->repaint();
        repaint();
    }
    void resized() override { layoutModel(); }
    void paint (juce::Graphics& g) override
    {
        const auto st = style (shown < 0 ? 0 : shown);
        auto r = getLocalBounds().toFloat();
        const float a = 1.0f - (float) juce::jlimit (0.0, 90.0, (double) ph.proc.uiState.getProperty ("anAlpha", 30.0)) / 100.0f;
        g.setColour (st.face.withAlpha (a)); g.fillRoundedRectangle (r, 6.0f);
        g.setColour (st.face.brighter (0.25f)); g.drawHorizontalLine (1, r.getX() + 6, r.getRight() - 6);
        g.setColour (juce::Colours::black.withAlpha (0.4f)); g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.0f);
        // screws
        for (auto p : { juce::Point<float> (8, 8), juce::Point<float> (r.getRight() - 8, 8), juce::Point<float> (8, r.getBottom() - 8), juce::Point<float> (r.getRight() - 8, r.getBottom() - 8) })
        { g.setColour (st.face.darker (0.5f)); g.fillEllipse (p.x - 3, p.y - 3, 6, 6); g.setColour (st.ink.withAlpha (0.3f)); g.drawLine (p.x - 2, p.y - 2, p.x + 2, p.y + 2, 1.0f); }
        if (shown < 0) return;
        const auto& M = models()[(size_t) shown];
        g.setColour (st.ink); g.setFont (theme::ui (13.0f, true));
        g.drawText (juce::String::fromUTF8 (ph.ja() ? M.nameJa : M.name), 344, 5, 420, 18, juce::Justification::centredLeft);
        g.setColour (st.ink.withAlpha (0.6f)); g.setFont (theme::ui (10.0f));
        if (ph.proc.isLinkedBand (ph.proc.linkedBands().isEmpty() ? -1 : ph.proc.linkedBands()[0]))
        {
            juce::StringArray b; for (int x : ph.proc.linkedBands()) b.add ("B" + juce::String (x + 1));
            g.setColour (juce::Colour (0xff6fd3a4));
            g.drawText (ph.tr ("展開先：", "Expanded to: ") + b.joinIntoString (" ") + ph.tr ("（つまみの動きがそのまま反映）", " (the knobs drive them live)"), 344, 21, 440, 14, juce::Justification::centredLeft);
        }
        else g.drawText (analog::hint (shown, ph.ja()), 344, 21, 420, 14, juce::Justification::centredLeft);
        g.setColour (st.ink.withAlpha (0.7f)); g.setFont (theme::ui (9.5f, true));
        g.drawText (ph.tr ("透過 ", "SEE-THRU ") + juce::String ((int) seeThru.getValue()) + "%", seeThru.getX() - 78, seeThru.getY(), 74, seeThru.getHeight(), juce::Justification::centredRight);
        // column titles and dividers
        g.setFont (theme::ui (10.0f, true));
        for (size_t c = 0; c < colX.size(); ++c)
        {
            g.setColour (st.accent.withAlpha (0.9f));
            g.drawText (juce::String::fromUTF8 (M.groups[c]), (int) colX[c], 40, (int) colW, 14, juce::Justification::centred);
            if (c > 0) { g.setColour (st.ink.withAlpha (0.18f)); g.drawVerticalLine ((int) colX[c] - 2, 40.0f, (float) getHeight() - 8.0f); }
        }
    }
    static const std::array<analog::Model, analog::kModels>& models() { return analog::models(); }
private:
    struct Style { juce::Colour face, ink, accent, knob; };
    static Style style (int m)
    {
        switch (m)
        {
            case 0:  return { juce::Colour (0xff2b2f36), juce::Colour (0xffe8e4da), juce::Colour (0xff6fb7e6), juce::Colour (0xff1a1c20) };   // dark grey, blue lamps
            case 1:  return { juce::Colour (0xffb9bcc0), juce::Colour (0xff1c1f24), juce::Colour (0xffc0392b), juce::Colour (0xff3a3d42) };   // brushed silver
            case 2:  return { juce::Colour (0xff1d1f22), juce::Colour (0xffdedede), juce::Colour (0xffd8c25a), juce::Colour (0xff9ea3a8) };   // black, silver knobs
            case 3:  return { juce::Colour (0xff59646e), juce::Colour (0xfff1ede4), juce::Colour (0xffe8a33a), juce::Colour (0xff23272c) };   // blue-grey console
            case 4:  return { juce::Colour (0xffd9cfb6), juce::Colour (0xff2a241b), juce::Colour (0xff2f6b4f), juce::Colour (0xff3b3328) };   // cream, green accent
            case 5:  return { juce::Colour (0xff3d4a57), juce::Colour (0xffeee6d2), juce::Colour (0xffe0b25a), juce::Colour (0xff15171a) };   // slate blue, cream lettering
            case 6:  return { juce::Colour (0xff5d6b7c), juce::Colour (0xfff2efe8), juce::Colour (0xfff3dca8), juce::Colour (0xff7a2a26) };   // console blue-grey, red knobs, cream titles
            case 7:  return { juce::Colour (0xff2e3135), juce::Colour (0xffe6e6e6), juce::Colour (0xff5fa8e0), juce::Colour (0xff5a5d62) };   // dark grey, coloured caps
            default: return { juce::Colour (0xff141518), juce::Colour (0xffd8dde4), juce::Colour (0xff3d8de0), juce::Colour (0xff6d737b) };   // black, blue
        }
    }
    void layoutModel()
    {
        const int m = shown < 0 ? 0 : shown;
        const auto& M = models()[(size_t) m];
        const auto st = style (m);
        modelBtn.setBounds (12, 8, 190, 22); onBtn.setBounds (206, 8, 34, 22); expBtn.setBounds (244, 8, 92, 22);
        const int W = getWidth(), H = getHeight();
        const int ctrlW = 64;
        drive.setBounds (W - 2 * ctrlW - 14, 4, ctrlW, std::min (H - 8, 78)); out.setBounds (W - ctrlW - 10, 4, ctrlW, std::min (H - 8, 78));
        for (auto* k : { &drive, &out }) { k->face = st.knob; k->ink = st.ink; k->accent = st.accent; }
        seeThru.setBounds (W - 2 * ctrlW - 14 - 96, 10, 90, 16);
        seeThru.setColour (juce::Slider::thumbColourId, st.accent); seeThru.setColour (juce::Slider::trackColourId, st.ink.withAlpha (0.35f));
        seeThru.setColour (juce::Slider::backgroundColourId, st.ink.withAlpha (0.15f));
        const int nCol = (int) M.groups.size();
        const int left = 12, right = W - 2 * ctrlW - 24;
        colW = (float) (right - left) / (float) std::max (1, nCol);
        colX.clear(); for (int c = 0; c < nCol; ++c) colX.push_back (left + c * colW);
        // items per column
        std::vector<int> cnt ((size_t) nCol, 0); for (auto& s : M.slots) cnt[(size_t) s.group]++;
        int maxN = 1; for (int c : cnt) maxN = std::max (maxN, c);
        const float top = 56.0f, avail = (float) H - top - 6.0f;
        // knobs go side by side inside a column when it is wide enough
        std::vector<int> used ((size_t) nCol, 0);
        for (int i = 0; i < analog::kSlots; ++i) { knobs[i]->setVisible (false); switches[i]->setVisible (false); }
        for (int i = 0; i < (int) M.slots.size(); ++i)
        {
            const auto& s = M.slots[(size_t) i]; const int c = s.group, k = used[(size_t) c]++, n = cnt[(size_t) c];
            const int perRow = std::max (1, std::min (n, (int) (colW / 62.0f)));
            const int rows = (n + perRow - 1) / perRow;
            const float cw = colW / (float) perRow, ch = std::min (84.0f, avail / (float) rows);
            const float x = colX[(size_t) c] + (k % perRow) * cw, y = top + (k / perRow) * ch;
            const auto b = juce::Rectangle<float> (x + 2, y, cw - 4, ch - 2).toNearestInt();
            const juce::String pidk = pid::anK (i);
            if (s.kind == analog::Choice && s.values.size() == 2)
            {
                auto* w = switches[i]; w->id = pidk; w->label = s.label; w->names = { juce::String::fromUTF8 (s.names[0]), juce::String::fromUTF8 (s.names[1]) };
                w->def = s.def; w->face = st.face; w->ink = st.ink; w->accent = st.accent;
                w->setBounds (b.withSizeKeepingCentre (b.getWidth(), std::min (b.getHeight(), 36))); w->setVisible (true);
                setHelp (*w, s.label, slotHelpJa (s), slotHelpEn (s));
            }
            else
            {
                auto* w = knobs[i]; w->id = pidk; w->label = s.label; w->def = s.def; w->bipolar = (s.kind == analog::Gain && ! s.uni) || s.kind == analog::Steps;
                w->steps = (s.kind == analog::Choice || s.kind == analog::Steps) ? (int) s.values.size() : 0;
                w->fmt = [sp = &s] (float v) { return analog::text (*sp, v); };
                w->face = st.knob; w->ink = st.ink; w->accent = st.accent;
                w->setBounds (b); w->setVisible (true);
                setHelp (*w, s.label, slotHelpJa (s), slotHelpEn (s));
            }
        }
        setHelp (*this, "ANALOG EQ", models()[(size_t) m].helpJa, models()[(size_t) m].helpEn);
        repaint();
    }
    static const char* slotHelpJa (const analog::Slot& s)
    {
        switch (s.kind)
        {
            case analog::Gain:   return "このバンドの増減（dB）。上下ドラッグ、Shift で細かく、ダブルクリックで 0";
            case analog::Freq:   return "このバンドの周波数。連続して動く。上下ドラッグ、Shift で細かく";
            case analog::Width:  return "幅（Q）。小さいほど広い。\n| 値 | 幅 |\n| 0.5 | 約 2.5 oct |\n| 1 | 約 1.4 oct |\n| 3 | 約 1/2 oct |";
            case analog::Steps:  return "2 dB ステップの増減（±8 dB）。決まった段だけを選ぶので、同じ設定を再現しやすい";
            case analog::Choice: return "決まった周波数（または設定）から選ぶステップ式。上下ドラッグで段を切り替え";
        }
        return "";
    }
    static const char* slotHelpEn (const analog::Slot& s)
    {
        switch (s.kind)
        {
            case analog::Gain:   return "This band's boost / cut (dB). Drag up/down, Shift for fine, double-click for 0";
            case analog::Freq:   return "This band's frequency, continuous. Drag up/down, Shift for fine";
            case analog::Width:  return "Width (Q). Smaller = wider.\n| Value | Width |\n| 0.5 | about 2.5 oct |\n| 1 | about 1.4 oct |\n| 3 | about 1/2 oct |";
            case analog::Steps:  return "Boost / cut in 2 dB steps (±8 dB): fixed positions, easy to repeat";
            case analog::Choice: return "Stepped: pick from fixed frequencies (or settings). Drag up/down to step";
        }
        return "";
    }
    void expandMenu()
    {
        auto& pr = ph.proc;
        if (ph.value (pid::anLink) < 0.5f)
        {
            pr.undo.beginNewTransaction();
            if (! pr.expandAnalog())
            {
                const int need = analog::maxSections ((int) ph.value (pid::anModel));
                juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon, ph.tr ("空いているバンドが足りません", "Not enough free bands"),
                    ph.tr ("このモデルには ", "This model needs ") + juce::String (need) + ph.tr (" バンド必要です（空き ", " bands (free: ") + juce::String (pr.freeBandCount())
                    + ph.tr ("）。使っていないバンドをオフにしてから、もう一度押してください。", "). Switch off bands you do not use, then press again."));
            }
            refresh(); return;
        }
        juce::PopupMenu m;
        m.addItem (1, ph.tr ("切り離す（バンドは普通のバンドとして残す。パネルはオフ）", "Detach (the bands stay as ordinary bands; the panel goes off)"));
        m.addItem (2, ph.tr ("展開を戻す（バンドを消して、アナログ段に戻す）", "Undo the expansion (remove the bands, back to the analog stage)"));
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&expBtn), [this] (int r)
        {
            if (r <= 0) return;
            ph.proc.undo.beginNewTransaction(); ph.proc.collapseAnalog (r == 1); refresh();
        });
    }
    void modelMenu()
    {
        juce::PopupMenu menu;
        const int cur = (int) ph.value (pid::anModel);
        for (int i = 0; i < analog::kModels; ++i)
            menu.addItem (i + 1, juce::String (models()[(size_t) i].name) + juce::String::fromUTF8 ("   \xe2\x80\x94 ") + analog::hint (i, ph.ja()), true, i == cur);
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&modelBtn), [this] (int r)
        {
            if (r <= 0) return;
            ph.proc.undo.beginNewTransaction();
            ph.proc.applyAnalogModel (r - 1);      // its controls at defaults, every band's curve and the saturator type follow
            refresh();
        });
    }
    ParamHelp& ph;
    juce::TextButton modelBtn, onBtn, expBtn;
    AnaKnob drive, out;
    juce::Slider seeThru;
    juce::OwnedArray<AnaKnob> knobs; juce::OwnedArray<AnaSwitch> switches;
    std::vector<float> colX; float colW = 60; int shown = -1;
};
