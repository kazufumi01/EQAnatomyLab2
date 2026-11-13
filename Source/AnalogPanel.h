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
        addAndMakeVisible (onBtn); onBtn.setClickingTogglesState (false);
        onBtn.onClick = [this] { ph.toggleGesture (pid::anOn); };
        drive.id = pid::anDrive; drive.label = "DRIVE"; drive.def = 4.0f / 24.0f;
        drive.fmt = [] (float v) { return juce::String (v * 24.0f, 1) + " dB"; };
        out.id = pid::anOut; out.label = "OUT"; out.def = 0.5f; out.bipolar = true;
        out.fmt = [] (float v) { const float d = v * 24.0f - 12.0f; return (d > 0.05f ? "+" : "") + juce::String (d, 1) + " dB"; };
        addAndMakeVisible (drive); addAndMakeVisible (out);
        setHelp (onBtn, "ANALOG IN", "アナログEQモデルのオン／オフ。メインEQとレゾナンス・サプレッサーの後ろ、帯域サチュレーターの前に入る。オフの時は処理を通らない（レイテンシーなし）",
                 "Analog EQ model on / off. It sits after the main EQ and the resonance suppressor, before the band saturator. Off = not in the signal path (no latency)");
        setHelp (modelBtn, "ANALOG MODEL", "モデルを選ぶ。選ぶとそのモデルのつまみが初期値に戻る（Cmd＋Z で戻せる）。\n| モデル | 系統 |\n| Passive Mastering | Massive Passive 系 |\n| Precision Parametric | Sontec 系 |\n| Air Band | Maag EQ4 系 |\n| British Curve | Curve Bender 系 |\n| Program 7-Band | Hitsville EQ のモデリング元の系統 |\nどれも公開されている動作を参考にした学習用の汎用モデルで、実機の回路を測ったものではない",
                 "Pick the model. Choosing one resets its controls (Cmd+Z undoes).\n| Model | Family |\n| Passive Mastering | Massive Passive style |\n| Precision Parametric | Sontec style |\n| Air Band | Maag EQ4 style |\n| British Curve | Curve Bender style |\n| Program 7-Band | the family modelled by Hitsville EQ |\nAll are generic learning models after the published behaviour, not measured circuits");
        setHelp (drive, "ANALOG DRIVE", "モデルの回路（真空管・トランス・Class A）にどれだけ強く入れるか。上げるほど倍音が増え、ピークが丸くなる。音量は自動で合わせる。\n| 値 | 聴こえ方 |\n| 0〜6 dB | ほぼ透明。太さがわずかに増える |\n| 6〜12 dB | 倍音がはっきり。中低域が濃くなる |\n| 12 dB〜 | 歪みとして聴こえる。効果音的 |",
                 "How hard the model's circuit (tube, transformer, class A) is driven. More = more harmonics and rounder peaks; the level is matched.\n| Value | Sounds like |\n| 0-6 dB | nearly transparent, a touch thicker |\n| 6-12 dB | clear harmonics, denser low-mids |\n| 12 dB up | audible distortion, an effect |");
        setHelp (out, "ANALOG OUT", "アナログ段の出力レベル（±12 dB）。オン／オフで音量をそろえて比べるために使う", "The analog stage's output level (±12 dB). Use it to match the level when comparing on / off");
    }
    void refresh()
    {
        const int m = juce::jlimit (0, analog::kModels - 1, (int) ph.value (pid::anModel));
        const bool on = ph.value (pid::anOn) > 0.5f;
        onBtn.setButtonText ("IN"); onBtn.setToggleState (on, juce::dontSendNotification);
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
        g.setColour (st.face); g.fillRoundedRectangle (r, 6.0f);
        g.setColour (st.face.brighter (0.25f)); g.drawHorizontalLine (1, r.getX() + 6, r.getRight() - 6);
        g.setColour (juce::Colours::black.withAlpha (0.4f)); g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.0f);
        // screws
        for (auto p : { juce::Point<float> (8, 8), juce::Point<float> (r.getRight() - 8, 8), juce::Point<float> (8, r.getBottom() - 8), juce::Point<float> (r.getRight() - 8, r.getBottom() - 8) })
        { g.setColour (st.face.darker (0.5f)); g.fillEllipse (p.x - 3, p.y - 3, 6, 6); g.setColour (st.ink.withAlpha (0.3f)); g.drawLine (p.x - 2, p.y - 2, p.x + 2, p.y + 2, 1.0f); }
        if (shown < 0) return;
        const auto& M = models()[(size_t) shown];
        g.setColour (st.ink); g.setFont (theme::ui (13.0f, true));
        g.drawText (juce::String::fromUTF8 (ph.ja() ? M.nameJa : M.name), 210, 5, 260, 18, juce::Justification::centredLeft);
        g.setColour (st.ink.withAlpha (0.6f)); g.setFont (theme::ui (10.0f));
        g.drawText (ph.tr ("参考：", "After: ") + juce::String::fromUTF8 (M.family) + ph.tr (" 系", " style"), 210, 21, 300, 14, juce::Justification::centredLeft);
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
            default: return { juce::Colour (0xffd9cfb6), juce::Colour (0xff2a241b), juce::Colour (0xff2f6b4f), juce::Colour (0xff3b3328) };   // cream, green accent
        }
    }
    void layoutModel()
    {
        const int m = shown < 0 ? 0 : shown;
        const auto& M = models()[(size_t) m];
        const auto st = style (m);
        modelBtn.setBounds (12, 8, 150, 22); onBtn.setBounds (166, 8, 34, 22);
        const int W = getWidth(), H = getHeight();
        const int ctrlW = 64;
        drive.setBounds (W - 2 * ctrlW - 14, 4, ctrlW, std::min (H - 8, 78)); out.setBounds (W - ctrlW - 10, 4, ctrlW, std::min (H - 8, 78));
        for (auto* k : { &drive, &out }) { k->face = st.knob; k->ink = st.ink; k->accent = st.accent; }
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
    void modelMenu()
    {
        juce::PopupMenu menu;
        const int cur = (int) ph.value (pid::anModel);
        for (int i = 0; i < analog::kModels; ++i)
            menu.addItem (i + 1, juce::String (models()[(size_t) i].name) + "  (" + ph.tr ("参考：", "after ") + juce::String::fromUTF8 (models()[(size_t) i].family) + ")", true, i == cur);
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&modelBtn), [this] (int r)
        {
            if (r <= 0) return;
            const int m = r - 1; const auto& M = models()[(size_t) m];
            ph.proc.undo.beginNewTransaction();
            ph.setGesture (pid::anModel, (float) m);
            for (int i = 0; i < analog::kSlots; ++i) ph.setGesture (pid::anK (i), i < (int) M.slots.size() ? M.slots[(size_t) i].def : 0.5f);
            ph.setGesture (pid::anDrive, M.defDrive);
            if (ph.value (pid::anOn) < 0.5f) ph.setGesture (pid::anOn, 1.0f);
            refresh();
        });
    }
    ParamHelp& ph;
    juce::TextButton modelBtn, onBtn;
    AnaKnob drive, out;
    juce::OwnedArray<AnaKnob> knobs; juce::OwnedArray<AnaSwitch> switches;
    std::vector<float> colX; float colW = 60; int shown = -1;
};
