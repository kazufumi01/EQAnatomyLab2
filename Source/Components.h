#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_dsp/juce_dsp.h>
#include "PluginProcessor.h"
#include "Theme.h"
#include "Lessons.h"
#include "Presets.h"

// Small helpers shared by the editor's parts
struct ParamHelp
{
    explicit ParamHelp (EQAnatomyProcessor& p) : proc (p) {}
    EQAnatomyProcessor& proc;
    juce::RangedAudioParameter* get (const juce::String& id) const { return proc.apvts.getParameter (id); }
    float value (const juce::String& id) const { return proc.apvts.getRawParameterValue (id)->load(); }
    void set (const juce::String& id, float v) const { if (auto* p = get (id)) p->setValueNotifyingHost (p->convertTo0to1 (v)); }
    void begin (const juce::String& id) const { if (auto* p = get (id)) p->beginChangeGesture(); }
    void end (const juce::String& id) const { if (auto* p = get (id)) p->endChangeGesture(); }
    void setGesture (const juce::String& id, float v) const { begin (id); set (id, v); end (id); }
    // Option(Alt)+click: back to the default value, one gesture = one undo step
    static bool altDown() { return juce::ModifierKeys::currentModifiers.isAltDown(); }
    bool resetDefault (const juce::String& id) const
    {
        auto* p = get (id); if (p == nullptr) return false;
        proc.undo.beginNewTransaction();
        p->beginChangeGesture(); p->setValueNotifyingHost (p->getDefaultValue()); p->endChangeGesture();
        return true;
    }
    void toggleGesture (const juce::String& id) const { if (altDown()) resetDefault (id); else setGesture (id, value (id) > 0.5f ? 0.0f : 1.0f); }
    bool ja() const { return proc.uiState.getProperty ("lang", "ja").toString() == "ja"; }
    juce::String tr (const char* j, const char* e) const { return juce::String::fromUTF8 (ja() ? j : e); }
    int selected() const { return juce::jlimit (0, eqcore::kBands - 1, (int) proc.uiState.getProperty ("selected", 2)); }
    // selecting a band brings the floating panel back next to its point; a different band also closes the extension
    void select (int i) const
    {
        auto& ui = proc.uiState;
        ui.setProperty ("popPinned", false, nullptr);      // the extension stays as it is
        ui.setProperty ("selected", i, nullptr);
    }
    // hover help, filled by the editor from the component under the mouse
    juce::String hoverTitle, hoverText;
};

// Short help shown in the info card when the mouse is over a control (Learn off)
inline void setHelp (juce::Component& c, const char* title, const char* ja, const char* en)
{
    auto& p = c.getProperties();
    p.set ("helpT", juce::String::fromUTF8 (title)); p.set ("helpJ", juce::String::fromUTF8 (ja)); p.set ("helpE", juce::String::fromUTF8 (en));
}
// ComboBox that jumps to the default on Option+click instead of opening its menu
struct ResetCombo : public juce::ComboBox
{
    std::function<void()> onAlt;
    void mouseDown (const juce::MouseEvent& e) override { skip = e.mods.isAltDown() && onAlt && ! e.mods.isPopupMenu(); if (skip) onAlt(); else juce::ComboBox::mouseDown (e); }
    void mouseUp (const juce::MouseEvent& e) override { if (skip) { skip = false; return; } juce::ComboBox::mouseUp (e); }
    void mouseDrag (const juce::MouseEvent& e) override { if (! skip) juce::ComboBox::mouseDrag (e); }
private:
    bool skip = false;
};
inline void markReset (juce::Component& c) { c.getProperties().set ("resettable", true); }

inline const char* mouseHelpJa() { return "点をドラッグ：周波数とゲイン／Cmd＋ドラッグ：縦か横に固定／Alt＋ドラッグ：そのバンドだけを試聴／幅ハンドル・Shift＋上下・ホイール：Q／ダブルクリック：追加（クリックした周波数で形を自動選択：〜45 Hz ローカット／〜180 Hz ローシェルフ／〜6 kHz ベル／〜14 kHz ハイシェルフ／それ以上ハイカット）・オフ／右クリック：浮き窓の表示切替／DYN の▲▼（塗り＝ABOVE、輪＝BELOW）：上下ドラッグで可動範囲、Option＋クリックで初期値／上部の SAT タブ：サチュレーターの帯域／Cmd＋Z：元に戻す"; }
inline const char* mouseHelpEn() { return "Drag a dot: frequency and gain / Cmd + drag: lock to one axis / Alt + drag: solo that band / width handles, Shift + drag or wheel: Q / double-click: add (the shape follows the frequency: below 45 Hz low cut, 180 Hz low shelf, 6 kHz bell, 14 kHz high shelf, above that high cut) or switch off / right-click: show or hide the floating panel / DYN triangles (filled = ABOVE, ring = BELOW): drag up/down for the range, Option+click resets / SAT tabs: saturator band / Cmd + Z: undo"; }

inline juce::String fmtHz (double f)
{
    if (f >= 1000) return juce::String (f / 1000.0, f >= 10000 ? 1 : 2) + "k";
    return juce::String ((int) std::round (f));
}
inline juce::String fmtDb (double g, int d = 1)
{
    const double r = std::round (g * std::pow (10, d)) / std::pow (10, d);
    return (r > 0 ? "+" : r < 0 ? juce::String::fromUTF8 ("\xe2\x88\x92") : "") + juce::String (std::abs (r), d);
}

// =============================================================================
//  DragValue: drag up / down (Shift = fine), wheel, double-click to type
// =============================================================================
class DragValue : public juce::Component
{
public:
    DragValue (ParamHelp& h, juce::String captionText) : ph (h), caption (std::move (captionText)) { setOpaque (false); markReset (*this); }
    std::function<juce::String (float)> format = [] (float v) { return juce::String (v, 2); };
    std::function<float (const juce::String&)> parse = [] (const juce::String& s) { return s.getFloatValue(); };
    juce::Colour valueColour = theme::text;
    bool compact = false;                       // caption beside the value instead of under it
    void setParam (const juce::String& id) { if (id != paramID) { paramID = id; repaint(); } }
    void setCaption (const juce::String& c) { if (c != caption) { caption = c; repaint(); } }
    bool isBusy() const { return dragging || editor != nullptr; }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (0.5f);
        g.setColour (theme::hilite (over || dragging ? 0.25f : 0.1f));
        g.fillRoundedRectangle (r, 4.0f);
        if (editor != nullptr) return;
        const float v = ph.value (paramID);
        if (compact)
        {
            g.setColour (theme::muted); g.setFont (theme::ui (9.5f));
            g.drawText (caption, 6, 0, getWidth() - 12, 14, juce::Justification::centredLeft);
            g.setColour (valueColour); g.setFont (theme::mono (12.5f, true));
            g.drawText (format (v), 6, 12, getWidth() - 12, getHeight() - 14, juce::Justification::centredLeft);
        }
        else
        {
            g.setColour (valueColour); g.setFont (theme::mono (15.0f, true));
            g.drawText (format (v), 0, 0, getWidth(), getHeight() - 14, juce::Justification::centred);
            g.setColour (theme::muted); g.setFont (theme::ui (9.5f));
            g.drawText (caption, 0, getHeight() - 15, getWidth(), 14, juce::Justification::centred);
        }
    }
    void mouseEnter (const juce::MouseEvent&) override { over = true; repaint(); }
    void mouseExit (const juce::MouseEvent&) override { over = false; repaint(); }
    std::function<void()> onRightClick;
    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu()) { if (onRightClick) onRightClick(); return; }
        if (e.mods.isAltDown() && ph.resetDefault (paramID)) { repaint(); return; }      // Option+click: default (no drag)
        if (auto* p = ph.get (paramID)) { start = p->getValue(); lastY = e.getScreenPosition().toFloat().y; p->beginChangeGesture(); dragging = true; }
        setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
    }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        auto* p = ph.get (paramID); if (p == nullptr || ! dragging) return;
        const float sens = e.mods.isShiftDown() ? 0.0008f : 0.005f;      // 200 px = full range, Shift ×1/6
        // screen coordinates: the popup follows the node, so local positions would feed back
        const float y = e.getScreenPosition().toFloat().y;
        start = juce::jlimit (0.0f, 1.0f, start + (lastY - y) * sens); lastY = y;
        p->setValueNotifyingHost (start); repaint();
    }
    void mouseUp (const juce::MouseEvent&) override
    {
        if (! dragging) return;
        if (auto* p = ph.get (paramID)) p->endChangeGesture();
        dragging = false; setMouseCursor (juce::MouseCursor::NormalCursor); repaint();
    }
    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override
    {
        auto* p = ph.get (paramID); if (p == nullptr || w.isInertial || std::abs (w.deltaY) < 0.01f) return;
        const float step = (e.mods.isShiftDown() ? 0.002f : 0.01f) * (w.deltaY > 0 ? 1.0f : -1.0f) * (w.isReversed ? -1.0f : 1.0f);
        p->beginChangeGesture(); p->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, p->getValue() + step)); p->endChangeGesture(); repaint();
    }
    void mouseDoubleClick (const juce::MouseEvent&) override
    {
        editor = std::make_unique<juce::TextEditor>();
        editor->setFont (theme::mono (13.0f, true)); editor->setJustification (juce::Justification::centred);
        editor->setText (format (ph.value (paramID)).trim(), false); editor->selectAll();
        editor->setBounds (getLocalBounds().withTrimmedBottom (compact ? 0 : 14));
        editor->onReturnKey = [this] { commit (true); };
        editor->onEscapeKey = [this] { commit (false); };
        editor->onFocusLost = [this] { commit (true); };
        addAndMakeVisible (*editor); editor->grabKeyboardFocus();
    }

private:
    void commit (bool apply)
    {
        if (editor == nullptr) return;
        const auto txt = editor->getText();
        juce::MessageManager::callAsync ([sp = juce::Component::SafePointer<DragValue> (this)] { if (sp != nullptr) { sp->editor.reset(); sp->repaint(); } });
        if (apply && txt.trim().isNotEmpty())
            if (auto* p = ph.get (paramID))
                ph.setGesture (paramID, p->getNormalisableRange().getRange().clipValue (parse (txt.replace (juce::String::fromUTF8 ("\xe2\x88\x92"), "-"))));
    }
    ParamHelp& ph; juce::String paramID, caption;
    float start = 0, lastY = 0; bool over = false, dragging = false;
    std::unique_ptr<juce::TextEditor> editor;
};

inline float parseHz (const juce::String& s) { auto t = s.trim(); float v = t.getFloatValue(); if (t.containsIgnoreCase ("k")) v *= 1000.0f; return v; }

// =============================================================================
//  Seg: a small segmented switch bound to a choice parameter
// =============================================================================
class Seg : public juce::Component
{
public:
    Seg (ParamHelp& h, juce::StringArray labels) : ph (h), items (std::move (labels)) { markReset (*this); }
    void setParam (const juce::String& id) { paramID = id; repaint(); }
    juce::Colour onColour = theme::accent;
    std::function<bool()> enabled = [] { return true; };
    std::function<bool (int)> itemOn = [] (int) { return true; };       // dims single items (still clickable)
    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (theme::shade (0.33f)); g.fillRoundedRectangle (r, 4.0f);
        const int sel = paramID.isEmpty() ? 0 : (int) ph.value (paramID);
        const float w = r.getWidth() / (float) items.size();
        for (int i = 0; i < items.size(); ++i)
        {
            const bool en = enabled() && itemOn (i);
            auto c = juce::Rectangle<float> (r.getX() + i * w, r.getY(), w, r.getHeight()).reduced (2.0f);
            if (i == sel) { g.setColour (onColour.withAlpha (en ? 1.0f : 0.4f)); g.fillRoundedRectangle (c, 3.0f); }
            g.setColour (i == sel ? theme::onAccent : (en ? theme::muted : theme::faint));
            g.setFont (theme::ui (10.0f, i == sel));
            g.drawText (items[i], c, juce::Justification::centred);
        }
    }
    void mouseDown (const juce::MouseEvent& e) override
    {
        if (paramID.isEmpty() || e.mods.isPopupMenu()) return;
        if (e.mods.isAltDown() && ph.resetDefault (paramID)) { repaint(); return; }
        const int i = juce::jlimit (0, items.size() - 1, (int) (e.x * items.size() / std::max (1, getWidth())));
        ph.proc.undo.beginNewTransaction();
        ph.setGesture (paramID, (float) i); repaint();
    }
private:
    ParamHelp& ph; juce::StringArray items; juce::String paramID;
};

// =============================================================================
//  Band editor, in two layouts (0.1.1)
//    compact = the floating panel. Follows the dot; drag its top strip to park it
//      somewhere else (double-click the strip to make it follow again)
//        ⋮⋮ ● B3  type            SOLO
//        FREQ    GAIN    Q
//        SLOPE ▸        CH ▸
//    full = the detail panel at the bottom left: band tabs, everything above,
//      then PHASE, DYN (detector, meter, six numbers), TRANSIENT/BODY, T/B scope
// =============================================================================
// A button that also answers a long press (0.5 s): onLongPress runs and the click is skipped
class LongPressButton : public juce::TextButton, private juce::Timer
{
public:
    std::function<void()> onLongPress;
    void mouseDown (const juce::MouseEvent& e) override { fired = false; startTimer (500); juce::TextButton::mouseDown (e); }
    void mouseUp (const juce::MouseEvent& e) override
    {
        stopTimer();
        if (fired) { fired = false; setState (juce::Button::buttonNormal); return; }   // the long press already acted
        juce::TextButton::mouseUp (e);
    }
private:
    bool fired = false;
    void timerCallback() override { stopTimer(); if (isMouseButtonDown()) { fired = true; if (onLongPress) onLongPress(); } }
};

class BandPopup : public juce::Component
{
public:
    explicit BandPopup (ParamHelp& h, bool fullLayout = false)
        : ph (h), full (fullLayout), fq (h, "FREQ Hz"), gn (h, "GAIN dB"), qv (h, "Q"), th (h, "THR"), rg (h, "ABOVE"), bl (h, "BELOW"),
          at (h, "ATT ms"), rl (h, "REL ms"), kn (h, "KNEE dB"),
          phaseSeg (h, { "Min", "Lin" }), srcSeg (h, { "In", "SC", "M", "S" })
    {
        setOpaque (false);
        if (! full) for (auto* d : { &fq, &gn, &qv }) d->onRightClick = [this] { hidePanel(); };
        dynBtn.setButtonText ("DYN");
        dynBtn.onClick = [this] { ph.toggleGesture (pid::band (band, "dyn")); }; markReset (dynBtn);
        soloBtn.setButtonText ("SOLO"); soloBtn.setClickingTogglesState (false);
        soloBtn.onClick = [this] { ph.proc.soloBand = ph.proc.soloBand.load() == band ? -1 : band; };
        soloBtn.setColour (juce::TextButton::buttonOnColourId, theme::pre);   // refreshed in refresh()
        // slope and channel open a small menu (they take little room); Option+click resets
        slopeBtn.onClick = [this] { if (ParamHelp::altDown()) ph.resetDefault (pid::band (band, "slope")); else slopeMenu(); }; markReset (slopeBtn);
        chBtn.onClick    = [this] { if (ParamHelp::altDown()) ph.resetDefault (pid::band (band, "ch")); else chMenu(); }; markReset (chBtn);
        scopeBtn.setButtonText ("T/B Scope"); scopeBtn.setClickingTogglesState (false);
        scopeBtn.onClick = [this] { ph.proc.uiState.setProperty ("scope", ! (bool) ph.proc.uiState.getProperty ("scope", false), nullptr); };
        for (auto* b : { &slopeBtn, &chBtn, &scopeBtn, &dynBtn, &soloBtn }) b->getProperties().set ("fontPx", full ? 10.0 : 10.5);
        for (auto* c : { (juce::Component*) &soloBtn, (juce::Component*) &slopeBtn, (juce::Component*) &chBtn }) addAndMakeVisible (c);
        // Lab 2: ▾ opens every setting in the floating panel (the old detail panel), ▴ goes back to the compact one
        expBtn.setButtonText ({}); expBtn.getProperties().set ("chevron", full ? 2 : 1);
        expBtn.onClick = [this]
        {
            auto& ui = ph.proc.uiState;
            // keep the panel where it is: park it at its current place before it grows or shrinks
            if (! (bool) ui.getProperty ("popPinned", false))
            { ui.setProperty ("popPinned", true, nullptr); ui.setProperty ("popX", getX(), nullptr); ui.setProperty ("popY", getY(), nullptr); }
            ui.setProperty ("popExpand", ! (bool) ui.getProperty ("popExpand", false), nullptr);
        };
        addAndMakeVisible (expBtn);
        setHelp (expBtn, full ? "COMPACT" : "EXPAND", full ? "小さい表示に戻す" : "このバンドのすべての設定（PHASE・DYN 一式・TRANS/BODY・T/B Scope）を浮き窓に広げて表示",
                 full ? "Back to the compact panel" : "Show every setting of this band (phase, the whole DYN set, transient/body, T/B Scope) in the floating panel");
        th.format = [] (float v) { return fmtDb (v, 0); };
        rg.format = [] (float v) { return fmtDb (v); };
        bl.format = [] (float v) { return fmtDb (v); };
        auto num = [] (float v) { return v == std::round (v) ? juce::String ((int) v) : juce::String (v, 1); };
        at.format = [] (float v) { return juce::String (v, v < 10.0f ? 1 : 0); };
        rl.format = [] (float v) { return juce::String ((int) std::round (v)); };
        kn.format = num;
        type.addItemList (typeNames(), 1);
        type.setColour (juce::ComboBox::backgroundColourId, theme::panel.withAlpha (0.33f));
        type.setColour (juce::ComboBox::outlineColourId, theme::text.withAlpha (0.33f));
        addAndMakeVisible (type);
        markReset (type);
        type.onAlt = [this] { ph.resetDefault (pid::band (band, "type")); };
        type.onChange  = [this] { ph.setGesture (pid::band (band, "type"), (float) (type.getSelectedId() - 1)); };
        fq.format = [] (float v) { return fmtHz (v); };  fq.parse = parseHz;
        gn.format = [] (float v) { return fmtDb (v); };
        qv.format = [] (float v) { return juce::String (v, v < 10 ? 2 : 1); };
        for (auto* d : { &fq, &gn, &qv }) { d->compact = full; addAndMakeVisible (*d); }
        if (full)
        {
            for (auto* d : { &th, &rg, &bl, &at, &rl, &kn }) { d->compact = true; addAndMakeVisible (*d); }
            for (auto* s : { &phaseSeg, &srcSeg }) addAndMakeVisible (*s);
            addAndMakeVisible (dynBtn); addAndMakeVisible (scopeBtn);
            srcSeg.itemOn = [this] (int i) { return i != 1 || ph.proc.sidechainConnected(); };   // SC needs the host's sidechain; Mid / Side always work
            tb.setSliderStyle (juce::Slider::LinearHorizontal); tb.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
            tb.setRange (-100.0, 100.0, 1.0); tb.setDoubleClickReturnValue (true, 0.0); markReset (tb);
            tb.onDragStart = [this] { tbDrag = true; ph.proc.undo.beginNewTransaction(); ph.begin (pid::band (band, "tb")); };
            tb.onDragEnd = [this] { tbDrag = false; ph.end (pid::band (band, "tb")); };
            tb.onValueChange = [this]
            {
                if (refreshing) return;
                if (tbDrag) ph.set (pid::band (band, "tb"), (float) tb.getValue());      // left (−) = Transient only, right (+) = Body only
                else { ph.proc.undo.beginNewTransaction(); ph.setGesture (pid::band (band, "tb"), (float) tb.getValue()); }   // Option+click / double-click / wheel
            };
            addAndMakeVisible (tb);
        }

        if (full)
            setHelp (*this, "BAND PANEL", "選択中のバンドのすべての設定。上の 1〜8：クリックで選択（選択中ならもう一度で浮き窓の表示切替）、ダブルクリックでオン・オフ（塗り＝オン、輪＝オフ）。キー 1〜8 でも選べる",
                     "Every setting of the selected band. Tabs 1–8: click to select (click the selected one again to show or hide the floating panel), double-click to switch on/off (filled = on, ring = off). Keys 1–8 select too");
        else
            setHelp (*this, "BAND", "左の色の丸でバンドのオン・オフ（塗り＝オン、輪＝オフ）。上の帯をドラッグで移動、ダブルクリックで点に付いて動く表示に戻す。右クリックで浮き窓を隠す",
                     "The coloured dot switches the band (filled = on, ring = off). Drag the top strip to move the panel, double-click it to follow the dot again. Right-click hides the panel");
        setHelp (type, "TYPE", "フィルターの種類。Bell・Shelf・Cut・Notch・All-pass", "Filter type: bell, shelf, cut, notch or all-pass");
        setHelp (slopeBtn, "SLOPE", "傾き。カット：6〜48 dB/oct（6 dB/oct 増えるごとに位相が 90° 多く回る）／シェルフ：6 dB/oct（1次。なだらかで位相の回りが小さい。Q は効かない）か 12 dB/oct（2次。従来のシェルフ、Q で肩が出る）。Bell・Notch・All-pass にはない",
                 "Slope. Cuts: 6–48 dB/oct (each extra 6 dB/oct turns the phase another 90°) / shelves: 6 dB/oct (first order: gentle, less phase shift, Q has no effect) or 12 dB/oct (second order: the original shelf, Q adds a shoulder). Not used by bell, notch or all-pass");
        setHelp (fq, "FREQ", "中心（またはカットオフ）周波数。上下ドラッグ、Shift で微調整、ダブルクリックで入力（2.5k など）", "Centre or cutoff frequency. Drag up/down, Shift for fine, double-click to type (e.g. 2.5k)");
        setHelp (gn, "GAIN", "ブースト・カット量（±30 dB）。上下ドラッグ、Shift で微調整、ダブルクリックで入力", "Boost or cut (±30 dB). Drag up/down, Shift for fine, double-click to type");
        setHelp (qv, "Q", "帯域の幅。大きいほど狭い。Q 1 ≒ 1.4 オクターブ", "Bandwidth: higher is narrower. Q 1 ≈ 1.4 octaves");
        setHelp (soloBtn, "BAND SOLO", "このバンドの帯域だけを聴く（グラフ上で Alt＋ドラッグ中も）。注意：音量が大きく変わるので、モニターを下げてから",
                 "Hear only this band's region (also while Alt-dragging in the graph). Careful: the level changes a lot, turn the monitors down first");
        setHelp (chBtn, "CH Stereo / Mid / Side / Left / Right", "どのチャンネルに掛けるか。Stereo：左右両方／Mid：中央だけ／Side：左右の差だけ／Left・Right：片側だけ（M/S のバンドの後に掛かる）。注意：Side を持ち上げすぎるとモノラル再生で消える成分が増える。右の CORR が 0 を下回ったら控えめに。L/R のバンドがあると、Linear・Analog と Lin バンドでは畳み込みが4系統になり CPU が約2倍になる",
                 "Which channel the band acts on. Stereo: both / Mid: the centre only / Side: the difference only / Left, Right: one side only (applied after the M/S bands). Careful: big side boosts vanish in mono — back off if CORR on the right drops below 0. With an L/R band, Linear, Analog and Lin bands run four convolutions instead of two: about twice the CPU");
        setHelp (phaseSeg, "PHASE Min / Lin", "Min：最小位相（遅れなし、位相が回る）／Lin：リニア位相（位相は回らないが、プリリンギングが出る）。注意：Lin のバンドが1つでもあると全体が FIR 分遅れる。低域・高い Q・急なカットは Min 推奨",
                 "Min: minimum phase (no delay, the phase turns) / Lin: linear phase (no phase shift, but pre-ringing). Careful: one Lin band delays everything by the FIR. Keep lows, high Q and steep cuts on Min");
        setHelp (dynBtn, "DYN", "ダイナミック EQ。この帯域のレベルに合わせてゲインが動く。注意：Min のバンドで、Zero latency の時だけ働く", "Dynamic EQ: the gain follows this band's level. Careful: Min bands in Zero latency only");
        setHelp (srcSeg, "DETECT In / SC / M / S", "何を聞いて動くか。In：入力（バンドの CH が Side なら Side、それ以外は Mid）／SC：ホストのサイドチェーン入力（例：ボーカルが入った時だけギターの 2〜4 kHz を下げる）／M：入力の Mid／S：入力の Side（例：Side が大きい時だけ効かせる）。M・S はバンドの CH 設定に関係なくその成分を聞く。注意：SC は Ableton ではデバイスのサイドチェーン欄で入力元を指定する。つないでいない間は灰色で、In と同じ動き",
                 "What the band listens to. In: the input (the Side if the band's CH is Side, otherwise the Mid) / SC: the host's sidechain (e.g. dip guitars at 2–4 kHz only while the vocal sings) / M: the input's Mid / S: the input's Side (e.g. act only when the sides are loud). M and S ignore the band's CH setting. Careful: pick the SC source in the host's sidechain section; it is dimmed and acts like In until connected");
        setHelp (th, "THR", "しきい値。ここを境に、上なら ABOVE、下なら BELOW の量だけ動く（KNEE の幅だけ離れると全量）。上の検出メーターの白い線をドラッグしても動かせる", "Threshold. Above it ABOVE applies, below it BELOW (full amount one KNEE width away). You can also drag the white line in the meter");
        setHelp (at, "ATT", "検出器の立ち上がりの速さ（ms、0.5〜200、既定 5）。短いほどアタックにすぐ反応、長いほど短い音を見逃す。注意：短すぎると低域で波形に追従してゲインがひずむ", "How fast the detector rises (ms, 0.5–200, default 5). Short reacts to attacks at once, long lets short hits through. Careful: very short on lows follows the waveform and distorts the gain");
        setHelp (rl, "REL", "検出器が下がる速さ（ms、5〜2000、既定 120）。長いほどゲインの戻りがゆっくり。注意：短いとポンピング、長いと次の音まで戻らない", "How fast the detector falls (ms, 5–2000, default 120). Long returns the gain slowly. Careful: short pumps, long may not recover before the next hit");
        setHelp (kn, "KNEE", "しきい値から全量に達するまでの幅（dB、0〜24、既定 12）。0 はしきい値で一気に切り替わる（ハード）、大きいほどなだらか", "Distance from the threshold to the full amount (dB, 0–24, default 12). 0 switches hard at the threshold; larger is gentler");
        setHelp (rg, "ABOVE", "しきい値より大きい時の変化量。マイナス＝出過ぎた時だけ削る／プラス＝大きい時だけ持ち上げる", "Change when above the threshold. Negative: cut when it jumps out / positive: lift when loud");
        setHelp (bl, "BELOW", "しきい値より小さい時の変化量。プラス＝小さい時だけ持ち上げる（アップワード）／マイナス＝小さい時は下げる（エキスパンダー的）。注意：プラスにすると無音部分のノイズも上がる。−60 dB 以下では反応しない",
                 "Change when below the threshold. Positive lifts quiet passages (upward) / negative pushes them down (expander-like). Careful: positive also lifts noise; it ignores levels under −60 dB");
        setHelp (tb, "TRANSIENT / BODY", "EQ の効きをアタックと余韻に配分する。左（TRANS）に寄せるとアタックだけに効き、右（BODY）に寄せると余韻だけに効く。T・B の数字はそれぞれへの効きの割合（％）。ダブルクリックで中央。注意：差を極端にすると音量が揺れ、歪みやポンピングが出る。Min のバンド（Zero latency）だけ",
                 "Splits the band's effect between attack and body. Left (TRANS): the attack only; right (BODY): the body only. T and B show how much of the effect each gets (%). Double-click centres. Careful: extreme settings wobble the level and can pump. Min bands in Zero latency only");
        setHelp (scopeBtn, "T/B SCOPE", "入力をアタック（Transient）と胴（Body）に色分けしてスクロール表示し、検出器の SENS・WIDTH・ATTACK・方式・検出用 EQ を調整する（全バンド共通）。開くとグラフは自動で Split になり、閉じると元の表示に戻る",
                 "Scrolls the input coloured by attack (Transient) and body, and holds the detector's SENS, WIDTH, ATTACK, method and detector EQ (shared by all bands). Opening it switches the graph to Split; closing it restores the previous view");
    }

    void refresh (int b)
    {
        refreshing = true;
        band = b;
        const int t = (int) ph.value (pid::band (b, "type"));
        if (type.getSelectedId() != t + 1) type.setSelectedId (t + 1, juce::dontSendNotification);
        const int s = (int) ph.value (pid::band (b, "slope"));
        const bool cut = t == (int) eqcore::Type::LowCut || t == (int) eqcore::Type::HighCut;
        const bool shelf = t == (int) eqcore::Type::LowShelf || t == (int) eqcore::Type::HighShelf;
        const bool noGain = cut || t == (int) eqcore::Type::Notch || t == (int) eqcore::Type::AllPass;
        static const char* shortSlope[] = { "6 dB", "12 dB", "18 dB", "24 dB", "36 dB", "48 dB" };
        const juce::String sv = cut ? (full ? juce::String (shortSlope[juce::jlimit (0, 5, s)]) : slopeNames()[juce::jlimit (0, 5, s)])
                              : shelf ? juce::String (s == 0 ? (full ? "6 dB" : "6 dB/oct") : (full ? "12 dB" : "12 dB/oct"))
                              : juce::String::fromUTF8 ("\xe2\x80\x94");
        slopeBtn.setButtonText ((full ? juce::String() : juce::String ("SLOPE  ")) + sv + (cut || shelf ? juce::String::fromUTF8 (" \xe2\x96\xb8") : juce::String()));
        slopeBtn.setEnabled (cut || shelf);
        static const char* chLong[] = { "Stereo", "Mid", "Side", "Left", "Right" };
        static const char* chShort[] = { "St", "M", "S", "L", "R" };
        const int ch = juce::jlimit (0, 4, (int) ph.value (pid::band (b, "ch")));
        chBtn.setButtonText (juce::String ("CH  ") + (full ? chShort[ch] : chLong[ch]) + juce::String::fromUTF8 (" \xe2\x96\xb8"));
        gn.setAlpha (noGain ? 0.35f : 1.0f); gn.setEnabled (! noGain);
        fq.setParam (pid::band (b, "freq")); gn.setParam (pid::band (b, "gain")); qv.setParam (pid::band (b, "q"));
        qv.setAlpha ((shelf && s == 0) || t == (int) eqcore::Type::FlatTilt ? 0.35f : 1.0f);   // a first-order shelf / flat tilt has no Q
        gn.valueColour = theme::bandColour (b);
        expBtn.getProperties().set ("chevron", (bool) ph.proc.uiState.getProperty ("popExpand", false) ? 2 : 1); expBtn.repaint();
        const bool bandOn = ph.value (pid::band (b, "on")) > 0.5f;
        for (auto* c : { (juce::Component*) &fq, (juce::Component*) &type, (juce::Component*) &slopeBtn, (juce::Component*) &chBtn })
            c->setAlpha (bandOn ? 1.0f : 0.45f);
        if (! bandOn) { gn.setAlpha (0.45f); qv.setAlpha (0.45f); }
        if (! (cut || shelf)) slopeBtn.setAlpha (0.35f);
        soloBtn.setToggleState (ph.proc.soloBand.load() == b, juce::dontSendNotification);
        soloBtn.setColour (juce::TextButton::buttonOnColourId, theme::pre);
        type.setColour (juce::ComboBox::backgroundColourId, theme::panel.withAlpha (0.33f)); type.setColour (juce::ComboBox::outlineColourId, theme::text.withAlpha (0.33f));
        if (full)
        {
            phaseSeg.setParam (pid::band (b, "phase")); srcSeg.setParam (pid::band (b, "src"));
            for (auto* sg : { &phaseSeg, &srcSeg }) sg->onColour = theme::bandColour (b);
            const bool linForced = ph.proc.currentMode() == eqcore::Mode::Linear;
            phaseSeg.enabled = [linForced] { return ! linForced; };
            // dynamics and transient/body: bells and shelves, zero-latency min bands
            dynType = t == (int) eqcore::Type::Bell || t == (int) eqcore::Type::LowShelf || t == (int) eqcore::Type::HighShelf;
            const bool avail = ph.proc.dynAvailable (b);
            const bool dOn = ph.value (pid::band (b, "dyn")) > 0.5f;
            dynBtn.setToggleState (dOn, juce::dontSendNotification);
            th.setParam (pid::band (b, "thr")); rg.setParam (pid::band (b, "range")); bl.setParam (pid::band (b, "below"));
            at.setParam (pid::band (b, "att")); rl.setParam (pid::band (b, "rel")); kn.setParam (pid::band (b, "knee"));
            for (auto* c : { (juce::Component*) &dynBtn, (juce::Component*) &th, (juce::Component*) &rg, (juce::Component*) &bl, (juce::Component*) &srcSeg,
                             (juce::Component*) &at, (juce::Component*) &rl, (juce::Component*) &kn, (juce::Component*) &tb })
                c->setVisible (dynType);
            for (auto* c : { (juce::Component*) &th, (juce::Component*) &rg, (juce::Component*) &bl, (juce::Component*) &srcSeg,
                             (juce::Component*) &at, (juce::Component*) &rl, (juce::Component*) &kn })
                c->setAlpha (dOn && avail ? 1.0f : 0.45f);
            dynBtn.setAlpha (avail ? 1.0f : 0.45f);
            tb.setAlpha (avail ? 1.0f : 0.4f);
            tb.setColour (juce::Slider::thumbColourId, theme::bandColour (b));
            tb.setColour (juce::Slider::trackColourId, theme::bandColour (b).withAlpha (0.4f));
            if (! tb.isMouseButtonDown()) tb.setValue (ph.value (pid::band (b, "tb")), juce::dontSendNotification);
            scopeBtn.setToggleState ((bool) ph.proc.uiState.getProperty ("scope", false), juce::dontSendNotification);
            for (auto* d : { &th, &rg, &bl, &at, &rl, &kn }) d->repaint();
            for (auto* sg : { &phaseSeg, &srcSeg }) sg->repaint();
        }
        resized();
        for (auto* d : { &fq, &gn, &qv }) d->repaint();
        repaint();
        refreshing = false;
    }
    juce::Rectangle<int> dotArea() const { return full ? juce::Rectangle<int>() : juce::Rectangle<int> { 22, 7, 12, 12 }; }
    juce::Rectangle<int> stripArea() const { return { 0, 0, getWidth(), 26 }; }
    juce::Rectangle<int> tabArea (int) const { return {}; }      // Lab 2: band tabs removed (the extension sits under the compact panel)
    void hidePanel() { if (! full) ph.proc.uiState.setProperty ("popup", false, nullptr); }
    // the white threshold line in the detector meter can be dragged (±5 px wide hit area, one gesture per drag)
    float thrX() const
    {
        const auto m = meterArea.toFloat();
        return m.getX() + m.getWidth() * juce::jlimit (0.0f, 1.0f, (ph.value (pid::band (band, "thr")) + 60.0f) / 60.0f);
    }
    bool overThr (juce::Point<int> p) const
    {
        return full && dynType && meterArea.expanded (0, 5).contains (p) && std::abs ((float) p.x - thrX()) <= 5.0f;
    }
    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu()) { hidePanel(); return; }
        if (full)
        {
            for (int i = 0; i < eqcore::kBands; ++i)
                if (tabArea (i).contains (e.getPosition()))
                {
                    if (e.getNumberOfClicks() >= 2) return;            // the double-click handles it
                    ph.proc.undo.beginNewTransaction();
                    clickTab (i); return;
                }
        }
        if (overThr (e.getPosition()))
        {
            if (auto* p = ph.get (pid::band (band, "thr")))
            { p->beginChangeGesture(); thrDrag = true; thrStartDb = ph.value (pid::band (band, "thr")); thrStartX = e.getScreenPosition().x; repaint(); }
            return;
        }
        if (! full && dotArea().expanded (5).contains (e.getPosition()))
        {
            const bool on = ph.value (pid::band (band, "on")) > 0.5f;
            ph.proc.undo.beginNewTransaction();
            ph.proc.uiState.setProperty ("keepOff", on ? band : -1, nullptr);
            if (e.mods.isAltDown()) ph.resetDefault (pid::band (band, "on")); else
            ph.setGesture (pid::band (band, "on"), on ? 0.0f : 1.0f);
            repaint();
            return;
        }
        if (! full && stripArea().contains (e.getPosition()))
        {
            // move the panel: screen coordinates, so the panel's own movement does not feed back
            moving = true; moveStart = getPosition(); moveFrom = e.getScreenPosition();
            setMouseCursor (juce::MouseCursor::DraggingHandCursor);
        }
    }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (moving)
        {
            if (auto* p = getParentComponent())
            {
                const auto d = e.getScreenPosition() - moveFrom;
                const int x = juce::jlimit (4, p->getWidth() - getWidth() - 4, moveStart.x + d.x), y = juce::jlimit (4, p->getHeight() - getHeight() - 4, moveStart.y + d.y);
                setTopLeftPosition (x, y);
                auto& ui = ph.proc.uiState;
                ui.setProperty ("popPinned", true, nullptr); ui.setProperty ("popX", x, nullptr); ui.setProperty ("popY", y, nullptr);
            }
            return;
        }
        if (! thrDrag) return;
        // screen coordinates, like DragValue: the panel must not feed back into the measurement
        const float db = juce::jlimit (-60.0f, 0.0f, thrStartDb + (float) (e.getScreenPosition().x - thrStartX) * 60.0f / (float) std::max (1, meterArea.getWidth()));
        ph.set (pid::band (band, "thr"), db); repaint();
    }
    void mouseUp (const juce::MouseEvent&) override
    {
        if (moving) { moving = false; setMouseCursor (juce::MouseCursor::NormalCursor); return; }
        if (! thrDrag) return;
        ph.end (pid::band (band, "thr")); thrDrag = false; repaint();
    }
    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        if (full)
        {
            for (int i = 0; i < eqcore::kBands; ++i)
                if (tabArea (i).contains (e.getPosition()))
                {
                    auto& ui = ph.proc.uiState;
                    if (toggledPopup) ui.setProperty ("popup", ! (bool) ui.getProperty ("popup", true), nullptr);   // undo the first click's toggle
                    toggledPopup = false;
                    const bool on = ph.value (pid::band (i, "on")) > 0.5f;
                    if (on) ui.setProperty ("keepOff", i, nullptr);
                    ph.setGesture (pid::band (i, "on"), on ? 0.0f : 1.0f);
                    repaint(); return;
                }
            return;
        }
        // double-click the strip: follow the dot again
        if (stripArea().contains (e.getPosition()) && ! dotArea().expanded (5).contains (e.getPosition()))
            ph.proc.uiState.setProperty ("popPinned", false, nullptr);
    }
    void mouseMove (const juce::MouseEvent& e) override
    {
        const bool o = dotArea().expanded (5).contains (e.getPosition()), t = overThr (e.getPosition());
        const bool strip = ! full && ! o && stripArea().contains (e.getPosition());
        setMouseCursor (o ? juce::MouseCursor::PointingHandCursor : t ? juce::MouseCursor::LeftRightResizeCursor
                          : strip ? juce::MouseCursor::DraggingHandCursor : juce::MouseCursor::NormalCursor);
        if (o != dotOver || t != thrOver) { dotOver = o; thrOver = t; repaint(); }
    }
    void mouseExit (const juce::MouseEvent&) override { if (dotOver || thrOver) { dotOver = thrOver = false; repaint(); } }
    bool isBusy() const { return moving || thrDrag || fq.isBusy() || gn.isBusy() || qv.isBusy() || th.isBusy() || rg.isBusy() || bl.isBusy()
                              || at.isBusy() || rl.isBusy() || kn.isBusy() || tb.isMouseButtonDown(); }
    static constexpr int compactW = 270, compactH = 100, fullW = 270, fullH = 140;
    int idealHeight() const { return full ? fullH : compactH; }
    int idealWidth() const { return full ? fullW : compactW; }
    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (0.5f);
        const bool bandOn = ph.value (pid::band (band, "on")) > 0.5f;
        if (! full)
        {
            g.setColour (theme::popup.withAlpha (0.86f)); g.fillRoundedRectangle (r, 8.0f);
            // the drag strip
            juce::Path strip; strip.addRoundedRectangle (r.getX(), r.getY(), r.getWidth(), 26.0f, 8.0f, 8.0f, true, true, false, false);
            g.setColour (theme::hilite (0.06f)); g.fillPath (strip);
            g.setColour (theme::text.withAlpha (0.12f)); g.drawHorizontalLine (26, 1.0f, r.getRight() - 1.0f);
            g.setColour (theme::text.withAlpha (0.18f)); g.drawRoundedRectangle (r, 8.0f, 1.0f);
            g.setColour (theme::faint);
            for (int k = 0; k < 3; ++k) for (int c = 0; c < 2; ++c) g.fillEllipse (8.0f + c * 4.0f, 8.0f + k * 4.0f, 2.0f, 2.0f);
            const auto d = dotArea().toFloat();
            if (dotOver) { g.setColour (theme::hilite (0.13f)); g.fillEllipse (d.expanded (4.0f)); }
            g.setColour (theme::bandColour (band));
            if (bandOn) g.fillEllipse (d); else g.drawEllipse (d.reduced (0.75f), 1.5f);
            g.setColour (bandOn ? theme::text : theme::muted); g.setFont (theme::ui (12.0f, true));
            g.drawText ("B" + juce::String (band + 1), 38, 3, 24, 20, juce::Justification::centredLeft);
            return;
        }
        // ---- extension panel (looks like the floating panel above it)
        g.setColour (theme::popup.withAlpha (0.86f)); g.fillRoundedRectangle (r, 8.0f);
        g.setColour (theme::text.withAlpha (0.18f)); g.drawRoundedRectangle (r, 8.0f, 1.0f);
        for (int i = 0; i < 0; ++i)
        {
            const auto a = tabArea (i).toFloat();
            const bool on = ph.value (pid::band (i, "on")) > 0.5f, sel = i == band;
            if (sel) { g.setColour (theme::bandColour (i).withAlpha (0.18f)); g.fillRoundedRectangle (a, 3.0f); }
            g.setColour (sel ? theme::bandColour (i) : theme::line); g.drawRoundedRectangle (a.reduced (0.5f), 3.0f, sel ? 1.5f : 1.0f);
            const juce::Rectangle<float> dd (a.getX() + 3, a.getY() + 3, 5, 5);
            g.setColour (theme::bandColour (i));
            if (on) g.fillEllipse (dd); else g.drawEllipse (dd, 1.0f);
            g.setColour (sel ? theme::text : on ? theme::text.withAlpha (0.8f) : theme::muted); g.setFont (theme::mono (10.5f, true));
            g.drawText (juce::String (i + 1), a.withTrimmedLeft (5), juce::Justification::centred);
        }
        {
            // tags of the selected band, right of the tabs
            juce::StringArray tg;
            if (bandOn && ph.proc.bandIsLin (band)) tg.add ("Lin");
            if (bandOn && ph.value (pid::band (band, "dyn")) > 0.5f) tg.add ("D");
            if (bandOn && std::abs (ph.value (pid::band (band, "tb"))) > 0.5f) tg.add ("T/B");
            g.setColour (bandOn ? theme::bandColour (band) : theme::muted); g.setFont (theme::mono (9.0f, true));
            juce::ignoreUnused (tg);
        }
        g.setColour (theme::text.withAlpha (0.12f)); g.drawHorizontalLine (rowSep, 7.0f, (float) getWidth() - 7.0f);
        g.setColour (theme::muted); g.setFont (theme::ui (9.5f));
        g.drawText ("PHASE", 7, rowPhase, 38, 20, juce::Justification::centredLeft);
        if (! dynType)
        {
            g.setColour (theme::faint); g.setFont (theme::ui (9.5f));
            g.drawFittedText (ph.tr ("DYN と TRANSIENT / BODY は Bell と Shelf のバンドだけ", "DYN and TRANSIENT / BODY: bell and shelf bands only"),
                              juce::Rectangle<int> (7, meterArea.getY(), getWidth() - 14, 30), juce::Justification::centredLeft, 2);
            return;
        }
        g.setColour (theme::muted); g.setFont (theme::ui (9.0f));
        g.drawText ("TRANS", 7, rowTB, 34, 22, juce::Justification::centredLeft);
        g.drawText ("BODY", tb.getRight() + 2, rowTB, 30, 22, juce::Justification::centredLeft);
        // detector meter: this band's level vs threshold, and the gain it is applying now
        auto m = meterArea.toFloat();
        const bool dOn = ph.value (pid::band (band, "dyn")) > 0.5f, live = dOn && ph.proc.dynAvailable (band);
        g.setColour (theme::shade (0.4f)); g.fillRoundedRectangle (m, 3.0f);
        auto xOfDb = [&] (float db) { return m.getX() + m.getWidth() * juce::jlimit (0.0f, 1.0f, (db + 60.0f) / 60.0f); };
        if (live)
        {
            const float lv = ph.proc.detLevelDb[(size_t) band].load();
            g.setColour (theme::bandColour (band).withAlpha (0.8f));
            g.fillRoundedRectangle (m.withRight (xOfDb (lv)).reduced (0, 2), 2.0f);
        }
        const float tx = xOfDb (ph.value (pid::band (band, "thr")));
        const bool grab = thrOver || thrDrag;
        g.setColour (theme::text); g.fillRect (tx - (grab ? 1.5f : 1.0f), m.getY(), grab ? 3.0f : 2.0f, m.getHeight());
        g.fillRoundedRectangle (tx - 2.5f, m.getY() - 2.0f, 5.0f, 3.0f, 1.0f);                       // a small grip: this line is draggable
        g.fillRoundedRectangle (tx - 2.5f, m.getBottom() - 1.0f, 5.0f, 3.0f, 1.0f);
        g.setFont (theme::mono (8.5f));
        const auto txtR = meterArea.withY (meterArea.getBottom() + 1).withHeight (10);
        if (! ph.proc.dynAvailable (band))
        { g.setColour (theme::pre); g.drawText (juce::String ("ZL + Min only"), txtR, juce::Justification::centredRight); }
        else
        {
            g.setColour (theme::muted);
            g.drawText (live ? juce::String ("now ") + fmtDb (ph.proc.dynGainDb[(size_t) band].load()) + " dB" : juce::String ("detector"), txtR, juce::Justification::centredRight);
        }
        const float v = ph.value (pid::band (band, "tb"));
        g.setColour (theme::text); g.setFont (theme::mono (8.5f, true));
        g.drawText ("T" + juce::String ((int) std::round (v > 0 ? 100 - v : 100)) + " / B" + juce::String ((int) std::round (v < 0 ? 100 + v : 100)),
                    txtR.withX (7).withWidth (100), juce::Justification::centredLeft);
    }
    void resized() override
    {
        if (! full)
        {
            type.setBounds (64, 3, 132, 20);
            soloBtn.setBounds (getWidth() - 60, 3, 52, 20);
            const int w = (getWidth() - 24) / 3, y = 31, h = 38;
            fq.setBounds (8, y, w, h); gn.setBounds (12 + w, y, w, h); qv.setBounds (16 + 2 * w, y, w, h);
            const int bw = (getWidth() - 20) / 2;
            slopeBtn.setBounds (8, 74, bw, 20); chBtn.setBounds (12 + bw, 74, bw - 26, 20); expBtn.setBounds (12 + 2 * bw - 22, 74, 22, 20);
            return;
        }
        // Lab 2: the extension under the compact panel (same width): phase, the DYN set, transient / body, T/B Scope
        for (auto* c : { (juce::Component*) &soloBtn, (juce::Component*) &type, (juce::Component*) &slopeBtn, (juce::Component*) &chBtn,
                         (juce::Component*) &expBtn, (juce::Component*) &fq, (juce::Component*) &gn, (juce::Component*) &qv })
            c->setVisible (false);
        const int w = (getWidth() - 20) / 3;
        rowSep = -10; rowPhase = 8;
        phaseSeg.setBounds (44, rowPhase, 62, 20);
        dynBtn.setBounds (112, rowPhase - 1, 38, 22);
        srcSeg.setBounds (154, rowPhase, getWidth() - 161, 20);
        meterArea = { 7, 32, getWidth() - 14, 8 };
        DragValue* dv[] = { &th, &rg, &bl, &at, &rl, &kn };
        for (int k = 0; k < 6; ++k) dv[k]->setBounds (7 + (k % 3) * (w + 3), 53 + (k / 3) * 30, w, 28);
        rowTB = 111;
        tb.setBounds (40, rowTB, getWidth() - 40 - 112, 22);
        scopeBtn.setBounds (getWidth() - 74, rowTB + 1, 67, 20);
    }
private:
    void clickTab (int i)
    {
        auto& ui = ph.proc.uiState;
        toggledPopup = false;
        if (ph.selected() != i) { ph.select (i); ui.setProperty ("popup", true, nullptr); }
        else { ui.setProperty ("popup", ! (bool) ui.getProperty ("popup", true), nullptr); toggledPopup = true; }
        if (ph.value (pid::band (i, "on")) < 0.5f) ui.setProperty ("keepOff", i, nullptr);   // show the floating panel for an off band too
        repaint();
    }
    void slopeMenu()
    {
        const int t = (int) ph.value (pid::band (band, "type")), s = (int) ph.value (pid::band (band, "slope"));
        const bool cut = t == (int) eqcore::Type::LowCut || t == (int) eqcore::Type::HighCut;
        const bool shelf = t == (int) eqcore::Type::LowShelf || t == (int) eqcore::Type::HighShelf;
        juce::PopupMenu m;
        if (cut) for (int i = 0; i < 6; ++i) m.addItem (i + 1, slopeNames()[i], true, i == s);
        else if (shelf) { m.addItem (1, "6 dB/oct", true, s == 0); m.addItem (2, "12 dB/oct", true, s != 0); }
        else return;
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&slopeBtn), [this, b = band] (int r)
        {
            if (r <= 0) return;
            ph.proc.undo.beginNewTransaction(); ph.setGesture (pid::band (b, "slope"), (float) (r - 1));
        });
    }
    void chMenu()
    {
        const int ch = (int) ph.value (pid::band (band, "ch"));
        juce::PopupMenu m;
        m.addItem (1, "Stereo", true, ch == 0); m.addItem (2, "Mid", true, ch == 1); m.addItem (3, "Side", true, ch == 2);
        m.addSeparator();
        m.addItem (4, "Left", true, ch == 3); m.addItem (5, "Right", true, ch == 4);
        m.addSectionHeader (ph.tr ("L/R：Linear / Analog / Lin バンドでは畳み込みの CPU が約2倍", "L/R: about twice the convolution CPU in Linear / Analog / Lin bands"));
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&chBtn), [this, b = band] (int r)
        {
            if (r <= 0) return;
            ph.proc.undo.beginNewTransaction(); ph.setGesture (pid::band (b, "ch"), (float) (r - 1));
        });
    }
    ParamHelp& ph; const bool full; int band = 0;
    ResetCombo type; DragValue fq, gn, qv, th, rg, bl, at, rl, kn;
    Seg phaseSeg, srcSeg;
    juce::TextButton dynBtn, soloBtn, slopeBtn, chBtn, scopeBtn; juce::TextButton expBtn; juce::Slider tb;
    bool tbDrag = false, dynType = true, dotOver = false, thrOver = false, thrDrag = false, refreshing = false, moving = false, toggledPopup = false;
    juce::Rectangle<int> meterArea; int rowPhase = 89, rowTB = 192, rowSep = 85;
    float thrStartDb = 0; int thrStartX = 0;
    juce::Point<int> moveStart, moveFrom;
};

// =============================================================================
//  Band saturator strip
// =============================================================================
class SatStrip : public juce::Component
{
public:
    explicit SatStrip (ParamHelp& h) : ph (h), lo (h, "LOW"), hi (h, "HIGH"), drive (h, "DRIVE"), mix (h, "MIX"), os (h, { "1x", "2x", "4x" })
    {
        onBtn.setClickingTogglesState (false);
        onBtn.onClick = [this] { ph.toggleGesture (pid::satOn); }; markReset (onBtn);
        deltaBtn.onClick = [this] { ph.toggleGesture (pid::satDelta); }; markReset (deltaBtn);
        autoBtn.onClick = [this] { ph.toggleGesture (pid::satAuto); }; markReset (autoBtn);
        type.addItemList ({ "Tape", "Tube", "Clip", "Transformer", "Class A", "Exciter" }, 1);
        markReset (type); type.onAlt = [this] { ph.resetDefault (pid::satType); };
        type.onChange = [this] { ph.setGesture (pid::satType, (float) (type.getSelectedId() - 1)); };
        lo.setParam (pid::satLo); hi.setParam (pid::satHi); drive.setParam (pid::satDrive); mix.setParam (pid::satMix); os.setParam (pid::satOS);
        os.onColour = theme::sat;
        for (auto* d : { &lo, &hi }) { d->format = [] (float v) { return fmtHz (v) + " Hz"; }; d->parse = parseHz; }
        drive.format = [] (float v) { return juce::String (v, 1) + " dB"; };
        mix.format = [] (float v) { return juce::String ((int) std::round (v)) + " %"; };
        for (auto* d : { &lo, &hi, &drive, &mix }) { d->compact = true; d->valueColour = theme::sat; addAndMakeVisible (*d); }
        setHelp (onBtn, "SAT", "帯域サチュレーターのオン・オフ。EQ の後ろで、選んだ帯域だけを歪ませる", "Band saturator on/off. After the EQ, drives only the chosen band");
        setHelp (type, "SAT TYPE", "Tape：対称でなめらか／Tube：非対称、強く入れるほど太く／Clip：硬い／Transformer：低域ほど飽和しやすい／Class A：軽く入れても少し色が付く／Exciter：LOW より上に倍音だけを足す。注意：特定の機材の再現ではなく、回路の傾向を示すモデル",
                 "Tape: smooth / Tube: asymmetric, fatter when pushed / Clip: hard / Transformer: lows saturate first / Class A: a little colour even when light / Exciter: adds harmonics above LOW only. Careful: tendencies, not models of specific units");
        setHelp (lo, "LOW", "歪ませる帯域の下端。グラフ上部の SAT タブでも動かせる。Exciter ではここより上だけに倍音を足す", "Low edge of the driven band (also the SAT tab). Exciter adds harmonics only above it");
        setHelp (hi, "HIGH", "歪ませる帯域の上端。グラフ上部の SAT タブでも動かせる", "High edge of the driven band. Also the SAT tab at the top of the graph");
        setHelp (drive, "DRIVE", "歪みの量。小さい音の音量は変えずに、どれだけ潰すかだけを変える", "Amount of distortion; quiet material keeps its level");
        setHelp (mix, "MIX", "歪ませた音と元の音の割合（パラレル）", "Blend of driven and clean band (parallel)");
        setHelp (os, "OVERSAMPLING", "サチュレーターだけを 2 倍・4 倍のレートで処理し、折り返しノイズを減らす。1x と聴き比べると高域のザラつきの正体が分かる。注意：2x で CPU 約 2〜3 倍、4x で約 4〜6 倍。わずかなレイテンシーが加わる",
                 "Runs the saturator at 2× / 4× to cut aliasing; compare with 1x to hear the grit. Careful: 2x costs about 2–3× CPU, 4x about 4–6×, plus a little latency");
        setHelp (deltaBtn, "SAT Δ", "サチュレーターが足した成分（倍音と潰れ）だけを聴く", "Hear only what the saturator adds (harmonics and squash)");
        setHelp (autoBtn, "AUTO LVL", "潰れたピークの分だけ帯域の RMS を戻し、音量差なしで音色を比べる（Exciter では働かない）", "Restores the RMS the peaks lost so you compare tone, not level (not used by Exciter)");
        for (auto* c : { (juce::Component*) &onBtn, (juce::Component*) &deltaBtn, (juce::Component*) &autoBtn, (juce::Component*) &type, (juce::Component*) &os }) addAndMakeVisible (*c);
    }
    void refresh()
    {
        const bool on = ph.value (pid::satOn) > 0.5f;
        os.onColour = theme::sat;
        onBtn.setButtonText ("SAT"); onBtn.setToggleState (on, juce::dontSendNotification);
        deltaBtn.setButtonText (juce::String::fromUTF8 ("Sat \xce\x94")); deltaBtn.setToggleState (ph.value (pid::satDelta) > 0.5f, juce::dontSendNotification);
        autoBtn.setButtonText (makeupArea.isEmpty() && on && ph.value (pid::satAuto) > 0.5f ? fmtDb (ph.proc.satMakeupDb.load()) : juce::String ("Auto lvl"));
        autoBtn.getProperties().set ("fontPx", 10.5); autoBtn.setToggleState (ph.value (pid::satAuto) > 0.5f, juce::dontSendNotification);
        const int t = (int) ph.value (pid::satType);
        if (type.getSelectedId() != t + 1) type.setSelectedId (t + 1, juce::dontSendNotification);
        for (auto* c : { (juce::Component*) &lo, (juce::Component*) &hi, (juce::Component*) &drive, (juce::Component*) &mix, (juce::Component*) &deltaBtn, (juce::Component*) &autoBtn, (juce::Component*) &type, (juce::Component*) &os })
            c->setAlpha (on ? 1.0f : 0.45f);
        lo.repaint(); hi.repaint(); drive.repaint(); mix.repaint(); os.repaint(); repaint();
    }
    void resized() override
    {
        if (getHeight() > 80 && getWidth() < 240)
        {
            const int W = getWidth() - 16, cw = (W - 4) / 2;
            onBtn.setBounds (8, 28, 40, 22); type.setBounds (52, 28, W - 44 - 40, 22);
            curveArea = { getWidth() - 44, 26, 36, 26 };
            lo.setBounds (8, 54, cw, 28); hi.setBounds (12 + cw, 54, cw, 28);
            drive.setBounds (8, 84, cw, 28); mix.setBounds (12 + cw, 84, cw, 28);
            os.setBounds (8, 118, 64, 18);
            deltaBtn.setBounds (76, 116, 38, 22); autoBtn.setBounds (118, 116, W - 110, 22);
            makeupArea = {};      // the makeup shows in the Auto button
            textArea = {};
            return;
        }
        if (getHeight() > 80)
        {
            const int W = getWidth() - 16, cw = (W - 9) / 4;
            onBtn.setBounds (8, 8, 44, 22); type.setBounds (56, 8, W - 48 - 132, 22);
            deltaBtn.setBounds (getWidth() - 136, 8, 56, 22); autoBtn.setBounds (getWidth() - 76, 8, 68, 22);
            lo.setBounds (8, 36, cw, 30); hi.setBounds (11 + cw, 36, cw, 30); drive.setBounds (14 + 2 * cw, 36, cw, 30); mix.setBounds (17 + 3 * cw, 36, cw, 30);
            os.setBounds (8, 76, 96, 20);
            makeupArea = { 112, 72, 50, 28 };
            curveArea = { getWidth() - 52, 70, 44, 32 };
            textArea = { 8, 104, W, getHeight() - 110 };
            return;
        }
        int x = 8; const int y = 5, h = getHeight() - 10;
        onBtn.setBounds (x, y + 3, 44, h - 6); x += 48;
        type.setBounds (x, y + 3, 92, h - 6); x += 96;
        lo.setBounds (x, y, 74, h); x += 78;
        hi.setBounds (x, y, 74, h); x += 78;
        drive.setBounds (x, y, 66, h); x += 70;
        mix.setBounds (x, y, 54, h); x += 58;
        os.setBounds (x, y + 5, 72, h - 10); x += 76;
        deltaBtn.setBounds (x, y + 3, 52, h - 6); x += 56;
        autoBtn.setBounds (x, y + 3, 66, h - 6); x += 70;
        makeupArea = { x, y, 46, h }; x += 50;
        curveArea = { x, y, 44, h }; x += 50;
        textArea = { x, y, getWidth() - x - 8, h };
    }
    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (theme::panel); g.fillRoundedRectangle (r, 6.0f);
        g.setColour (ph.value (pid::satOn) > 0.5f ? theme::sat.withAlpha (0.53f) : theme::line); g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.0f);
        const juce::String mk = fmtDb (ph.value (pid::satOn) > 0.5f && ph.value (pid::satAuto) > 0.5f ? ph.proc.satMakeupDb.load() : 0.0f);
        const juce::Colour mc = ph.value (pid::satAuto) > 0.5f && ph.value (pid::satOn) > 0.5f ? theme::sat : theme::faint;
        if (makeupArea.isEmpty()) {}
        else if (makeupArea.getHeight() < 24)      // narrow layout: one line
        {
            g.setColour (theme::muted); g.setFont (theme::ui (9.5f)); g.drawText ("MAKEUP", makeupArea, juce::Justification::centredLeft);
            g.setColour (mc); g.setFont (theme::mono (11.0f, true)); g.drawText (mk, makeupArea, juce::Justification::centredRight);
        }
        else
        {
            g.setColour (theme::muted); g.setFont (theme::ui (9.5f));
            g.drawText ("MAKEUP", makeupArea.withHeight (14), juce::Justification::centredLeft);
            g.setColour (mc); g.setFont (theme::mono (12.0f, true));
            g.drawText (mk, makeupArea.withTrimmedTop (14), juce::Justification::centredLeft);
        }
        auto c = curveArea.toFloat();
        g.setColour (theme::well); g.fillRoundedRectangle (c, 3.0f);
        g.setColour (theme::grid); g.drawLine (c.getX(), c.getCentreY(), c.getRight(), c.getCentreY()); g.drawLine (c.getCentreX(), c.getY(), c.getCentreX(), c.getBottom());
        g.setColour (theme::faint); g.drawLine (c.getX(), c.getBottom(), c.getRight(), c.getY(), 0.8f);
        const float gd = juce::Decibels::decibelsToGain (ph.value (pid::satDrive)); const int t = (int) ph.value (pid::satType);
        juce::Path p;
        for (int i = 0; i <= 40; ++i)
        {
            const float x = -1.0f + 2.0f * i / 40.0f, y = juce::jlimit (-1.0f, 1.0f, BandSaturator::curve (t, x, gd));
            const float px = c.getX() + (x + 1) * 0.5f * c.getWidth(), py = c.getCentreY() - y * 0.5f * c.getHeight();
            if (i == 0) p.startNewSubPath (px, py); else p.lineTo (px, py);
        }
        g.setColour (theme::sat); g.strokePath (p, juce::PathStrokeType (1.5f));
        static const char* ja[] = { "対称でなめらか。奇数倍音が中心", "非対称。偶数倍音が出て、強く入れるほど太くなる", "硬く角のある歪み",
                                    "低域ほど飽和しやすい。低音の太さと密度感", "軽く入れても少し色が付く。中低域の厚み", "倍音だけを LOW より上に足す。こもらずに抜けが増える" };
        static const char* en[] = { "Symmetric and smooth; mostly odd harmonics", "Asymmetric; even harmonics, fatter when pushed", "Hard, edgy clipping",
                                    "Lows saturate first: weight and density", "A little colour even when light: low-mid body", "Adds harmonics above LOW only: more air without mud" };
        if (! textArea.isEmpty())
        {
            g.setColour (theme::muted); g.setFont (theme::ui (10.0f));
            g.drawFittedText (juce::String::fromUTF8 (ph.ja() ? ja[juce::jlimit (0, 5, t)] : en[juce::jlimit (0, 5, t)]), textArea, juce::Justification::centredLeft, 3, 0.9f);
        }
    }
private:
    ParamHelp& ph;
    juce::TextButton onBtn, deltaBtn, autoBtn; ResetCombo type;
    DragValue lo, hi, drive, mix; Seg os;
    juce::Rectangle<int> curveArea, textArea, makeupArea;
};

// =============================================================================
//  The big graph: magnitude, phase / group delay overlay, analyzer, nodes
// =============================================================================
class GraphView : public juce::Component
{
public:
    explicit GraphView (ParamHelp& h) : ph (h), popup (h), popupFull (h, true), scSens (h, "SENS dB"), scWidth (h, "WIDTH dB"), scAtt (h, "ATTACK ms"),
        scLc (h, "LOW CUT"), scE1f (h, "EQ1 Hz"), scE1g (h, "EQ1 dB"), scE1q (h, "EQ1 Q"), scE2f (h, "EQ2 Hz"), scE2g (h, "EQ2 dB"), scE2q (h, "EQ2 Q"),
        scMethod (h, { "Ratio", "Slope" })
    {
        addChildComponent (popup);
        // T/B scope: the input's attack (Transient) and body share, scrolling, with the detector and its Sens / Width band
        scopeBtn.setClickingTogglesState (false); scopeBtn.setButtonText ("T/B scope"); addAndMakeVisible (scopeBtn);
        // T/B Scope and Stereo field share the lower panel: one of them (or neither) is shown
        scopeBtn.onClick = [this] { lowerToggle (0); };
        scSens.setParam (pid::tbSens); scWidth.setParam (pid::tbWidth); scAtt.setParam (pid::tbAtt);
        scSens.format = [] (float v) { return fmtDb (v); };
        scWidth.format = [] (float v) { return v == std::round (v) ? juce::String ((int) v) : juce::String (v, 1); };
        scAtt.format = [] (float v) { return juce::String (v, 2); };
        for (auto* d : { &scSens, &scWidth, &scAtt }) { d->compact = true; addChildComponent (*d); }
        // detector-only EQ and the method: change what the detector hears, never the sound
        scLc.setParam (pid::tbLc); scE1f.setParam (pid::tbE1f); scE1g.setParam (pid::tbE1g); scE1q.setParam (pid::tbE1q);
        scE2f.setParam (pid::tbE2f); scE2g.setParam (pid::tbE2g); scE2q.setParam (pid::tbE2q); scMethod.setParam (pid::tbMethod);
        scLc.format = [] (float v) { return v <= 20.5f ? juce::String ("Off") : fmtHz (v); };
        for (auto* d : { &scLc, &scE1f, &scE2f }) d->parse = parseHz;
        scE1f.format = scE2f.format = [] (float v) { return fmtHz (v); };
        scE1g.format = scE2g.format = [] (float v) { return std::abs (v) < 0.05f ? juce::String ("0") : fmtDb (v); };
        scE1q.format = scE2q.format = [] (float v) { return juce::String (v, 2); };
        for (auto* d : { &scLc, &scE1f, &scE1g, &scE1q, &scE2f, &scE2g, &scE2q }) { d->compact = true; addChildComponent (*d); }
        addChildComponent (scMethod);
        setHelp (scLc, "DET LOW CUT", "検出器だけに掛かるローカット（12 dB/oct、Off＝20 Hz）。出力の音は変わらない。キックやベースの低域の揺れでアタック判定が乱れる時に 80〜150 Hz", "Low cut for the detector only (12 dB/oct, Off = 20 Hz); the output is untouched. Try 80–150 Hz when low-end swells confuse the attack detection");
        for (auto* d : { &scE1f, &scE1g, &scE1q, &scE2f, &scE2g, &scE2q })
            setHelp (*d, "DET EQ 1 / 2", "検出器だけに掛かるベル EQ 2 つ（ゲイン 0 で無効）。出力の音は変わらない。例：スネアのアタックを拾いたい時は 2〜5 kHz を +6〜12 dB、ハイハットに反応させたくない時は 8 kHz 付近を −12 dB", "Two bells for the detector only (0 dB = off); the output is untouched. E.g. +6 to +12 dB at 2–5 kHz to catch the snare's crack, −12 dB near 8 kHz to ignore hi-hats");
        setHelp (scMethod, "DET METHOD", "検出の方式／Ratio：速い包絡÷遅い包絡（従来）。音量が上がった分だけを見る。持続音の中のアタックにも反応する／Slope：速い包絡が 5 ms で何 dB 上がったか。立ち上がりの速さだけを見るので、ゆっくり膨らむ音はアタックにならない。注意：Slope は SENS・WIDTH の効き方が変わる（WIDTH は dB／5 ms）",
                 "Detection method / Ratio: fast ÷ slow envelope (the original); how far the level jumps above the recent body / Slope: how many dB the fast envelope rose in 5 ms; only the speed of the rise counts, so slow swells never become attack. Careful: SENS and WIDTH act differently with Slope (WIDTH is dB per 5 ms)");
        for (int i = 0; i < 4; ++i) scSpan.addItem (juce::String (spans()[i], 0) + " s", i + 1);
        scSpan.setSelectedId (1 + juce::jlimit (0, 3, (int) ph.proc.uiState.getProperty ("scopeSpan", 1)), juce::dontSendNotification);
        scSpan.onChange = [this] { ph.proc.uiState.setProperty ("scopeSpan", scSpan.getSelectedId() - 1, nullptr); };
        scPause.setClickingTogglesState (false); scPause.setButtonText (juce::String::fromUTF8 ("\xe2\x9d\x9a\xe2\x9d\x9a Pause"));
        scPause.onClick = [this] { setPaused (! scPaused); };
        addChildComponent (scSpan); addChildComponent (scPause);
        setHelp (scSpan, "SCOPE SPAN", "スコープに出す時間の長さ。1 s は 1 本ずつのアタックの形、8 s はフレーズ全体の傾向を見る", "How much time the scope shows. 1 s for the shape of single hits, 8 s for the whole phrase");
        setHelp (scPause, "SCOPE PAUSE", "スコープを止める。止めたまま SENS／WIDTH／ATTACK を動かすと、同じ音で色分けがどう変わるかが分かる。音は止まらない", "Freezes the scope. Move SENS / WIDTH / ATTACK while paused to see how the same audio is split. Playback is not affected");
        setHelp (scopeBtn, "T/B SCOPE", "入力の波形を約 2 秒分スクロール表示し、検出器がアタック（Transient＝オレンジ）と胴（Body＝青緑）に分けた割合で色分けする。下の線は検出器の値（速い包絡÷遅い包絡、dB）、黄色の帯はマスクが 0→1 に上がる範囲。帯の下の線（青緑）をドラッグで SENS、上の線（オレンジ）で WIDTH。グラフの下の部分に重なる（枠の上端をドラッグで高さを変更）",
                 "Scrolls about 2 s of the input and colours it by how the detector splits it: attack (Transient, orange) and body (Body, teal). The line below is the detector (fast ÷ slow envelope, dB); the yellow band is where the mask rises from 0 to 1. Drag its lower (teal) line for SENS and its upper (orange) line for WIDTH. It lies over the lower part of the graph; drag the panel's top edge to change its height");
        setHelp (scSens, "T/B SENS", "検出器の感度（dB）。＋で帯が下がり、アタックと判定されやすくなる。全バンド共通", "Detector sensitivity (dB). Plus lowers the band, so more counts as attack. Shared by all bands");
        setHelp (scWidth, "T/B WIDTH", "マスクが 0→1 に上がる幅（dB）。小さいほどハード", "Width over which the mask rises 0 → 1 (dB). Smaller is harder");
        setHelp (scAtt, "T/B ATTACK", "マスクが上がる速さ（ms、既定 2）。小さいほどアタックの頭が Body に漏れにくい", "How fast the mask rises (ms, default 2). Smaller keeps the front of the attack out of Body");
        // one button each: it shows the current choice and a click switches to the other one (its help says which)
        // two-way switches (always "on" in one of two states): outlined, not filled, so they never read as an active on/off
        for (auto* b : { &phaseBtn, &overlayBtn }) { b->setClickingTogglesState (false); b->setToggleState (false, juce::dontSendNotification); b->getProperties().set ("selector", true); addAndMakeVisible (*b); }
        overlayBtn.onClick = [this] { ph.proc.uiState.setProperty ("split", ! (bool) ph.proc.uiState.getProperty ("split", false), nullptr); resized(); };
        // the lower panel while the scope is open: T/B scope or the stereo field (width and correlation per band)
        fdTab.setClickingTogglesState (false); addAndMakeVisible (fdTab);
        fdTab.setButtonText ("Stereo field");
        fdTab.onClick = [this] { lowerToggle (1); };
        setHelp (fdTab, "STEREO FIELD", "下の枠を帯域ごとのステレオ表示に切り替える。上＝L／R の像（音量に左右されない。中央の線＝左右バランス、上が L・下が R、目盛は dB。線の上下の開き＝広がり＝1−コヒーレンス。モノの音はどこに定位しても細い線、Mid を上げると狭まり、無相関の Side や逆相で開く）、下＝相関（+1 同相、0 無相関、−1 逆相）。灰色が EQ 前、色付きが EQ 後。1/6 オクターブ、約 0.3 s の平均",
                 "Shows the stereo picture per band in the lower panel. Top: the L / R image, independent of level (the line is the balance, L up / R down in dB; the opening around it is the spread, 1 - coherence: a mono source stays a thin line wherever it is panned, raising Mid narrows it, decorrelated Side or out-of-phase content opens it). Bottom: correlation (+1 in phase, 0 uncorrelated, -1 out of phase). Grey = before the EQ, coloured = after. 1/6 octave, about 0.3 s average");
        phaseBtn.onClick = [this] { ph.proc.uiState.setProperty ("view", (int) ph.proc.uiState.getProperty ("view", 0) == 1 ? 0 : 1, nullptr); };
        setHelp (*this, "MOUSE", mouseHelpJa(), mouseHelpEn());
        for (int i = 0; i <= 400; ++i) fr.push_back (20.0 * std::pow (1000.0, i / 400.0));
        spectrum.assign (fr.size(), -140.0f); spectrumB.assign (fr.size(), -140.0f);

        // analyzer layers: A is filled, B (optional) is a line on top
        static const char* names[] = { "Stereo", "Mid", "Side", "Trans", "Body", "Input", "SC" };
        anaB.addItem ("B off", 1);
        for (int i = 0; i < 7; ++i) { anaA.addItem (juce::String ("A ") + names[i], i + 1); anaB.addItem (juce::String ("B ") + names[i], i + 2); }
        anaA.setSelectedId (1 + juce::jlimit (0, 6, (int) ph.proc.uiState.getProperty ("anA", 0)), juce::dontSendNotification);
        anaB.setSelectedId (1 + juce::jlimit (0, 7, (int) ph.proc.uiState.getProperty ("anB", 0)), juce::dontSendNotification);
        // changing A or B by hand picks the pair that matches, or Custom
        anaA.onChange = [this]
        {
            auto& ui = ph.proc.uiState; const int a = anaA.getSelectedId() - 1;
            ui.setProperty ("anA", a, nullptr); ui.setProperty ("anPair", matchPair (a, (int) ui.getProperty ("anB", 0)), nullptr);
        };
        anaB.onChange = [this]
        {
            const int id = anaB.getSelectedId();
            if (id > 100 && id - 101 < linkNames.size())
            { ph.proc.uiState.setProperty ("anBLink", linkNames[id - 101], nullptr); ph.proc.uiState.setProperty ("anB", 8, nullptr); }
            else if (id > 0) ph.proc.uiState.setProperty ("anB", id - 1, nullptr);
            auto& ui = ph.proc.uiState; ui.setProperty ("anPair", matchPair ((int) ui.getProperty ("anA", 0), (int) ui.getProperty ("anB", 0)), nullptr);
        };
        addAndMakeVisible (anaA); addAndMakeVisible (anaB);
        addChildComponent (popupFull);
        panelBtn.onClick = [this]
        {
            auto& ui = ph.proc.uiState; ui.setProperty ("lowerOpen", ! (bool) ui.getProperty ("lowerOpen", true), nullptr);
            if (auto* p = getParentComponent()) p->resized();
        };
        setHelp (panelBtn, "PANEL", "下段（サチュレーター・インパルス・出力・レッスン）の開閉。閉じるとグラフとメーターが下まで広がる",
                 "Open or close the lower panel (saturator, impulse, output, lessons). Closed, the graph and meters take the whole height");
        // analyzer pairs: one menu sets A (filled) and B (line) together; Custom frees the two menus
        addAndMakeVisible (pairBtn);
        pairBtn.onClick = [this]
        {
            juce::PopupMenu m; const int cur = (int) ph.proc.uiState.getProperty ("anPair", 0);
            for (int i = 0; i < 9; ++i) m.addItem (1 + i, pairName (i), true, cur == i);
            juce::PopupMenu other; refreshLinks(); lastLinkScan = 0;
            for (int i = 0; i < linkNames.size(); ++i)
                other.addItem (200 + i, linkNames[i], true, cur == 9 && ph.proc.uiState.getProperty ("anBLink", "").toString() == linkNames[i]);
            if (linkNames.isEmpty()) other.addItem (199, ph.tr ("（他の EQ Anatomy Lab 2 がありません）", "(no other EQ Anatomy Lab 2 found)"), false);
            m.addSubMenu (pairName (9), other, true, juce::Image(), cur == 9);
            m.addSeparator();
            m.addItem (11, pairName (10), true, cur == 10);
            m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&pairBtn), [this] (int r)
            {
                auto& ui = ph.proc.uiState;
                if (r >= 200 && r - 200 < linkNames.size()) { ui.setProperty ("anBLink", linkNames[r - 200], nullptr); setPair (9); }
                else if (r >= 1 && r <= 11) setPair (r - 1);
            });
        };
        setHelp (pairBtn, "ANALYZER PAIR", "スペアナの A（塗り）と B（線）の組み合わせをまとめて選ぶ。In＝EQ 前の入力、Out＝出力、SC＝サイドチェイン、Other＝同じ DAW 内の別の EQ Anatomy Lab 2 の出力。Custom で右の A・B を自由に選べる",
                 "Pick the analyzer's A (filled) and B (line) together. In = the input before the EQ, Out = the output, SC = sidechain, Other = another EQ Anatomy Lab 2 in the same DAW. Custom frees the A and B menus on the right");
        const char* hj = "スペアナに出す音を選ぶ。A は塗り、B は線で重なる（B off で非表示）。Stereo＝Mid と Side のパワー和（L/R の平均）／Mid・Side＝出力の M/S／Trans・Body＝検出器が聞いている「アタック／胴」の音／Input＝EQ 前の入力（出力と同じ遅れに揃えてある。B に出すと前後比較）／SC＝サイドチェインの入力（ホストで接続した時だけ。キックとベースの帯域の重なりなどを見る）／B ← 名前＝同じ DAW 内の別の EQ Anatomy の出力（トラック名で表示。例：ボーカルの EQ でギターの出力を重ね、ぶつかる帯域を見る。AU と VST3 を混ぜると互いに見えない）。注意：Trans・Body は検出器の見立てを出力に掛けた表示で、厳密な分離ではない（掛け算なので、元の音との合計は一致しない）";
        const char* he = "Choose what the analyzer shows. A is filled, B is a line on top (B off hides it). Stereo = power sum of Mid and Side (mean of L and R) / Mid, Side = the output's M/S / Trans, Body = the part the detector hears as attack or body / Input = the input before the EQ (delayed to line up with the output; put it on B for a before/after view) / SC = the sidechain input (only when connected in the host; e.g. see where kick and bass overlap) / B ← name = another EQ Anatomy in the same DAW (named after its track; e.g. overlay the guitar on the vocal EQ to see where they clash. AU and VST3 instances cannot see each other). Careful: Trans and Body show the detector's guess applied to the output, not a strict separation";
        setHelp (anaA, "ANALYZER A", hj, he); setHelp (anaB, "ANALYZER B", hj, he);
    }

    void update (const std::vector<float>& specDb, const std::vector<float>& specDbB)    // called ~30x per second by the editor
    {
        {
            // SOLO M / S → Mid + Side, SOLO T / B → Trans + Body, shown but not stored; the saved choice returns after the solo
            int a = juce::jlimit (0, 6, (int) ph.proc.uiState.getProperty ("anA", 0)), b = juce::jlimit (0, 8, (int) ph.proc.uiState.getProperty ("anB", 0));
            refreshLinks();
            { const bool sc = ph.proc.sidechainConnected(); anaA.setItemEnabled (7, sc); anaB.setItemEnabled (8, sc); }   // SC needs the host's sidechain
            // a solo (M / S / T / B) puts B on the soloed sound once; A stays, and both can be changed by hand afterwards.
            // When the solo ends, B goes back to what it was unless it was changed meanwhile
            const int sv = ph.proc.soloViewEffective();
            if (sv != lastSoloView)
            {
                auto& ui = ph.proc.uiState;
                const bool was = lastSoloView >= 1 && lastSoloView <= 4, now = sv >= 1 && sv <= 4;
                if (now)
                {
                    if (! was) { savedB = b; savedPair = (int) ui.getProperty ("anPair", 0); }
                    autoB = sv + 1; ui.setProperty ("anB", autoB, nullptr); ui.setProperty ("anPair", matchPair (a, autoB), nullptr); b = autoB;
                }
                else if (was && b == autoB)
                { ui.setProperty ("anB", savedB, nullptr); ui.setProperty ("anPair", savedPair, nullptr); b = savedB; }
                lastSoloView = sv;
            }
            autoAna = false;
            if (anaA.getSelectedId() != a + 1) anaA.setSelectedId (a + 1, juce::dontSendNotification);
            const int bid = b == 8 ? 101 + linkNames.indexOf (ph.proc.uiState.getProperty ("anBLink", "").toString()) : b + 1;
            if (bid == 100) { if (anaB.getSelectedId() != 0) anaB.setText ("B (link lost)", juce::dontSendNotification); }
            else if (anaB.getSelectedId() != bid) anaB.setSelectedId (bid, juce::dontSendNotification);
            const int pr = juce::jlimit (0, 10, (int) ph.proc.uiState.getProperty ("anPair", 0));
            pairBtn.setButtonText (pairName (pr)); pairBtn.getProperties().set ("chevron", 1);
            srcA = a; srcB = b - 1;
        }
        {
            const bool sc = (bool) ph.proc.uiState.getProperty ("scope", false) && ! ph.proc.earActive();
            const bool fd = sc && (int) ph.proc.uiState.getProperty ("lowerView", 0) == 1;
            ph.proc.scopeNeed = sc && ! fd;
            scopeBtn.setToggleState (sc && ! fd, juce::dontSendNotification); fdTab.setToggleState (fd, juce::dontSendNotification);
            if (fd != lastField) { lastField = fd; resized(); }
            if (sc != lastScope) { lastScope = sc; if (sc) { scEnableW = ph.proc.scopeRawW.load(); scSig = {}; } else if (scPaused) setPaused (false); resized(); }
            if (sc && ! fd) scopeTick();
        }
        if (specDbB.size() == spectrumB.size()) spectrumB = specDbB;
        const bool gd = (int) ph.proc.uiState.getProperty ("view", 0) == 1;
        phaseBtn.setButtonText (gd ? juce::String ("GD") : juce::String ("Phase"));
        if (gd) setHelp (phaseBtn, "GROUP DELAY", "いまは黄色の点線が群遅延（周波数ごとの遅れ、ms）。クリックで位相（±360°）に切り替え", "The yellow dotted line now shows group delay (the delay at each frequency, ms). Click to switch to phase (±360°)");
        else    setHelp (phaseBtn, "PHASE", "いまは黄色の点線が位相（±360°）。クリックで群遅延（周波数ごとの遅れ、ms）に切り替え", "The yellow dotted line now shows phase (±360°). Click to switch to group delay (the delay at each frequency, ms)");
        // recompute curves only when something changed
        const bool split = (bool) ph.proc.uiState.getProperty ("split", false);
        overlayBtn.setButtonText (split ? juce::String ("Split") : juce::String ("Overlay"));
        if (split) setHelp (overlayBtn, "SPLIT", "いまは位相（群遅延）を下の枠にも実線で表示（カーブ上の点線も残る）。枠の上端をドラッグで高さを変えられる。クリックで Overlay（点線だけ）に切り替え", "Phase (or group delay) is also drawn solid in the lower panel (the dotted line over the curve stays). Drag the panel's top edge to change its height. Click for Overlay (dotted line only)");
        else       setHelp (overlayBtn, "OVERLAY", "いまは位相（群遅延）を振幅カーブに点線で重ねて表示。クリックで Split（下の枠にも実線で表示）に切り替え", "Phase (or group delay) is drawn dotted over the magnitude curve. Click for Split (also drawn solid in a lower panel)");
        if (split != lastSplit) { lastSplit = split; resized(); }
        juce::String sig; sig << (int) ph.proc.currentMode() << (int) ph.proc.currentQMode();
        const auto bands = ph.proc.currentBands();
        for (auto& b : bands) sig << (int) b.on << (int) b.type << b.slope << "_" << b.f << "_" << b.g << "_" << b.q << (int) b.lin << b.ch;
        sig << ph.selected();
        for (int i = 0; i < eqcore::kBands; ++i) sig << "d" << (int) (ph.value (pid::band (i, "dyn")) > 0.5f) << ph.value (pid::band (i, "range")) << "_" << ph.value (pid::band (i, "below"));
        if (sig != lastSig)
        {
            lastSig = sig;
            const double fs = ph.proc.getSampleRateOrDefault();
            const auto qm = ph.proc.currentQMode(); const auto mode = ph.proc.currentMode();
            anyMS = false; anyLR = false;
            for (auto& b : bands) { anyMS = anyMS || (b.on && b.ch != 0); anyLR = anyLR || (b.on && b.ch >= 3); }
            // with L/R bands the two traces are Left / Right (stereo + that side's bands; M/S bands are left out of this view)
            cur = eqcore::curves (eqcore::channelModel (bands, qm, fs, mode, anyLR ? 3 : 1), fr);
            curS = anyMS ? eqcore::curves (eqcore::channelModel (bands, qm, fs, mode, anyLR ? 4 : 2), fr) : eqcore::CurvesV2();
            ghost = eqcore::Curves();
            if (mode == eqcore::Mode::ZeroLatency)
            {
                auto allMin = bands; for (auto& b : allMin) b.lin = false;
                ghost = eqcore::curves (eqcore::channelModel (allMin, qm, fs, eqcore::Mode::Analog, 1), fr);
            }
            const int s = ph.selected();
            std::array<eqcore::Band, eqcore::kBands> one {}; one[0] = bands[(size_t) s]; one[0].ch = 0;
            selCurve = bands[(size_t) s].on ? eqcore::curves (eqcore::channelModel (one, qm, fs, mode, 1), fr).mag : std::vector<double>();
            // dynamic EQ range (0.1.1): the band alone at gain + ABOVE and at gain + BELOW; the area between is where it can move
            for (auto& d : dynArea) { d.lo.clear(); d.hi.clear(); d.st.clear(); d.u.clear(); d.peak = 0; }
            for (int i = 0; i < eqcore::kBands; ++i)
            {
                const auto& b = bands[(size_t) i];
                if (! b.on || ph.value (pid::band (i, "dyn")) < 0.5f) continue;
                if (! (b.type == eqcore::Type::Bell || b.type == eqcore::Type::LowShelf || b.type == eqcore::Type::HighShelf)) continue;
                const double a = ph.value (pid::band (i, "range")), bl = ph.value (pid::band (i, "below"));
                std::array<eqcore::Band, eqcore::kBands> e {}; e[0] = b; e[0].ch = 0;
                e[0].g = juce::jlimit (-30.0, 30.0, b.g + std::min ({ 0.0, a, bl }));
                auto lo = eqcore::curves (eqcore::channelModel (e, qm, fs, mode, 1), fr).mag;
                e[0].g = juce::jlimit (-30.0, 30.0, b.g + std::max ({ 0.0, a, bl }));
                auto hi = eqcore::curves (eqcore::channelModel (e, qm, fs, mode, 1), fr).mag;
                e[0].g = juce::jlimit (-30.0, 30.0, b.g);
                auto st = eqcore::curves (eqcore::channelModel (e, qm, fs, mode, 1), fr).mag;
                e[0].g = b.g >= 29.0 ? b.g - 1.0 : b.g + 1.0;
                auto u = eqcore::curves (eqcore::channelModel (e, qm, fs, mode, 1), fr).mag;
                for (size_t k = 0; k < u.size(); ++k) u[k] = std::abs (u[k] - st[k]);   // dB of curve per dB of gain
                size_t pk = 0; double best = -1.0, umax = 0.0;
                for (auto x : u) umax = std::max (umax, x);
                for (size_t k = 0; k < u.size(); ++k)
                    if (u[k] >= umax * 0.97)
                    {
                        const double d = -std::abs (std::log2 (fr[k] / b.f));   // nearest to the knob among the near-maximum points
                        if (best < -0.5 || d > best) { best = d; pk = k; }
                    }
                dynArea[(size_t) i] = { std::move (lo), std::move (hi), std::move (st), std::move (u), pk };
            }
        }
        // dynamic EQ: the curve as it is right now
        {
            auto live = bands; bool moving = false;
            for (int i = 0; i < eqcore::kBands; ++i)
            {
                const float d = ph.proc.dynGainDb[(size_t) i].load();
                if (std::abs (d) > 0.05f) { live[(size_t) i].g += d; moving = true; }
            }
            // the same call gives phase and group delay, so the phase trace follows the dynamic bands at no extra cost
            liveCur = moving ? eqcore::curves (eqcore::channelModel (live, ph.proc.currentQMode(), ph.proc.getSampleRateOrDefault(), ph.proc.currentMode(), 1), fr)
                             : eqcore::CurvesV2();
            liveMag = liveCur.mag;
        }
        if (specDb.size() == spectrum.size()) spectrum = specDb;
        const int s = ph.selected();
        const int keep = (int) ph.proc.uiState.getProperty ("keepOff", -1);
        if (keep >= 0 && (keep != s || bands[(size_t) s].on)) ph.proc.uiState.setProperty ("keepOff", -1, nullptr);
        const bool showPop = (bands[(size_t) s].on || keep == s) && (bool) ph.proc.uiState.getProperty ("popup", true) && ! ph.proc.earActive();
        const bool expand = (bool) ph.proc.uiState.getProperty ("popExpand", false);
        popup.setVisible (showPop); popupFull.setVisible (showPop && expand);
        // keep the panel still while one of its values is being dragged or typed
        if (popup.isVisible()) { popup.refresh (s); if (! popup.isBusy()) placePopup(); }
        if (popupFull.isVisible()) { popupFull.refresh (s); if (! popupFull.isBusy() && ! popup.isBusy()) placePopup(); }
        repaint();
    }

    void resized() override
    {
        // left to right: what to analyse (T/B Scope, Stereo field), where to show it (Split), how to read the phase (Phase / GD)
        phaseBtn.setBounds (getWidth() - 58, 6, 50, 20);
        overlayBtn.setBounds (getWidth() - 124, 6, 62, 20);
        fdTab.setBounds (getWidth() - 216, 6, 86, 20);
        scopeBtn.setBounds (getWidth() - 296, 6, 76, 20);
        pairBtn.setBounds (42, 6, 96, 20);
        anaA.setBounds (142, 6, 84, 20); anaB.setBounds (230, 6, 96, 20);
        {
            // Lab 2: every T/B Scope setting in one row under the graph (shown while the scope is open)
            const int yr = getHeight() - 32;
            int x = 74;
            for (auto* d : { &scSens, &scWidth, &scAtt }) { d->setBounds (x, yr, 66, 28); x += 69; d->setVisible (tbView()); }
            scMethod.setBounds (x + 4, yr + 4, 100, 20); scMethod.setVisible (tbView()); x += 112;
            for (auto* d : { &scLc, &scE1f, &scE1g, &scE1q, &scE2f, &scE2g, &scE2q }) { d->setBounds (x, yr, 60, 28); x += (d == &scLc || d == &scE1q) ? 70 : 62; d->setVisible (tbView()); }
            scSpan.setBounds (x + 4, yr + 4, 62, 20); x += 70;
            scPause.setBounds (x, yr + 4, 58, 20);
            scSpan.setVisible (tbView()); scPause.setVisible (tbView());
        }
        placePopup();
    }

    void paint (juce::Graphics& g) override
    {
        updateAutoRange();
        auto r = getLocalBounds().toFloat();
        g.setColour (theme::well); g.fillRoundedRectangle (r, 6.0f);
        g.setColour (theme::line); g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.0f);
        const auto pr = plot();
        const bool split = lastSplit;
        const bool ear = ph.proc.earActive();

        // analyzer (full height, under the panel)
        const auto ar = fullPlot();
        if (! ear)
        {
            juce::Path p; p.startNewSubPath (ar.getX(), ar.getBottom());
            for (size_t i = 0; i < fr.size(); ++i)
            {
                const float y = juce::jmap (juce::jlimit (-90.0f, -10.0f, spectrum[i]), -90.0f, -10.0f, ar.getBottom(), ar.getY());
                p.lineTo (xOf (fr[i]), y);
            }
            p.lineTo (ar.getRight(), ar.getBottom()); p.closeSubPath();
            // A is the main layer: brighter fill; B is a quieter line on top. Trans / Body / Side get their own colours
            const juce::Colour ca = srcColour (srcA, theme::analyzer.interpolatedWith (theme::text, 0.22f));
            g.setColour (ca.withAlpha (srcA >= 3 || srcA == 2 ? 0.55f : 0.85f)); g.fillPath (p);
                // band solo (button or Alt + drag): the part of the spectrum the band works on is lit in its colour, the rest dimmed
                if (const int sb = ph.proc.soloBand.load(); sb >= 0 && sb < eqcore::kBands)
                {
                    const auto bs = ph.proc.currentBands();
                    std::array<eqcore::Band, eqcore::kBands> one {}; one[0] = bs[(size_t) sb]; one[0].on = true; one[0].ch = 0;
                    const auto mag = eqcore::curves (eqcore::channelModel (one, ph.proc.currentQMode(), ph.proc.getSampleRateOrDefault(), ph.proc.currentMode(), 1), fr).mag;
                    if (mag.size() == fr.size())
                    {
                        const auto ty = one[0].type;
                        const bool cut = ty == eqcore::Type::LowCut || ty == eqcore::Type::HighCut;
                        double mx = 0; for (auto v : mag) mx = std::max (mx, std::abs (v));
                        auto wOf = [&] (size_t k)
                        {
                            if (cut) return std::pow (10.0, mag[k] / 20.0);                          // the band's pass region
                            if (ty == eqcore::Type::Notch) return 1.0 - std::pow (10.0, mag[k] / 20.0);
                            if (mx < 0.1) return 0.0;                                              // a 0 dB bell changes nothing
                            return std::abs (mag[k]) / mx;                                          // how much the band changes each frequency
                        };
                        juce::Graphics::ScopedSaveState ss (g);
                        g.reduceClipRegion (p);
                        const auto bc = theme::bandColour (sb);
                        for (size_t k = 0; k + 1 < fr.size(); ++k)
                        {
                            const float x0 = xOf (fr[k]), x1 = xOf (fr[k + 1]) + 0.6f;
                            const float w = (float) juce::jlimit (0.0, 1.0, wOf (k));
                            g.setColour (theme::well.withAlpha (0.55f * (1.0f - w))); g.fillRect (x0, ar.getY(), x1 - x0, ar.getHeight());
                            g.setColour (bc.withAlpha (0.75f * w)); g.fillRect (x0, ar.getY(), x1 - x0, ar.getHeight());
                        }
                    }
                }
            if (srcB >= 0)
            {
                juce::Path q;
                for (size_t i = 0; i < fr.size(); ++i)
                {
                    const float y = juce::jmap (juce::jlimit (-90.0f, -10.0f, spectrumB[i]), -90.0f, -10.0f, ar.getBottom(), ar.getY());
                    if (i == 0) q.startNewSubPath (xOf (fr[i]), y); else q.lineTo (xOf (fr[i]), y);
                }
                g.setColour (srcColour (srcB, theme::text).withAlpha (0.5f)); g.strokePath (q, juce::PathStrokeType (1.0f));
            }
        }
        // grid
        g.setFont (theme::mono (10.0f));
        auto vgrid = [&] (juce::Rectangle<float> a)
        {
            for (double f : { 30., 40., 50., 60., 70., 80., 90., 100., 200., 300., 400., 500., 600., 700., 800., 900., 1000., 2000., 3000., 4000., 5000., 6000., 7000., 8000., 9000., 10000. })
            { g.setColour (theme::grid); g.drawVerticalLine ((int) xOf (f), a.getY(), a.getBottom()); }
        };
        vgrid (fullPlot());
        const auto lp = labelRow();
        for (double f : { 50., 100., 200., 500., 1000., 2000., 5000., 10000. })
        { g.setColour (theme::faint); g.drawText (fmtHz (f), (int) xOf (f) - 20, (int) lp, 40, 14, juce::Justification::centred); }
        g.setColour (theme::faint);
        g.drawText ("20", (int) pr.getX() + 2, (int) lp, 30, 14, juce::Justification::centredLeft);
        g.drawText ("20k", (int) pr.getRight() - 32, (int) lp, 30, 14, juce::Justification::centredRight);
        const int dbStep = dbRange <= 6.5f ? 2 : dbRange <= 12.5f ? 4 : dbRange <= 20.5f ? 6 : 10;
        const int dbTop = ((int) (dbRange + 0.5f) / dbStep) * dbStep;
        for (int db = -dbTop; db <= dbTop; db += dbStep)
        {
            const float y = yOf (db);
            g.setColour (db == 0 ? theme::zero : theme::grid); g.drawHorizontalLine ((int) y, pr.getX(), pr.getRight());
            if (std::abs (db) < dbTop) { g.setColour (theme::faint); g.drawText (fmtDb (db, 0), 2, (int) y - 7, (int) pr.getX() - 6, 14, juce::Justification::centredRight); }
        }
        if (ear)
        {
            g.setColour (theme::accent.withAlpha (0.35f)); g.setFont (theme::mono (90.0f, true));
            g.drawText ("?", pr, juce::Justification::centred);
            g.setColour (theme::muted); g.setFont (theme::ui (12.0f));
            g.drawText (ph.tr ("耳トレ中：カーブとアナライザーは隠しています", "Ear training: curve and analyzer are hidden"), pr.withTrimmedTop (pr.getHeight() * 0.7f), juce::Justification::centredTop);
            return;
        }

        // saturator band
        if (ph.value (pid::satOn) > 0.5f)
        {
            const float x0 = xOf (ph.value (pid::satLo)), x1 = xOf (ph.value (pid::satHi));
            const juce::Colour sc (0xffe8a33a);
            g.setColour (sc.withAlpha (0.06f)); g.fillRect (x0, pr.getY(), x1 - x0, pr.getHeight());
            const float dash[] = { 3.0f, 4.0f };
            g.setColour (sc.withAlpha (0.55f));
            g.drawDashedLine ({ x0, pr.getY(), x0, pr.getBottom() }, dash, 2, 1.0f);
            g.drawDashedLine ({ x1, pr.getY(), x1, pr.getBottom() }, dash, 2, 1.0f);
            g.setFont (theme::mono (9.0f, true));
            for (auto x : { x0, x1 })
            {
                auto tab = juce::Rectangle<float> (x - 15, pr.getY() + 2, 30, 16);
                g.setColour (sc.withAlpha (0.85f)); g.fillRoundedRectangle (tab, 3.0f);
                g.setColour (theme::onAccent); g.drawText ("SAT", tab, juce::Justification::centred);
            }
        }
        // resonance suppressor: its range (tabs at the bottom) and what it takes right now, hanging from 0 dB
        if (ph.value (pid::rsOn) > 0.5f)
        {
            const float x0 = xOf (ph.value (pid::rsLo)), x1 = xOf (ph.value (pid::rsHi));
            g.setColour (theme::res.withAlpha (0.05f)); g.fillRect (x0, pr.getY(), x1 - x0, pr.getHeight());
            const float dash[] = { 3.0f, 4.0f };
            g.setColour (theme::res.withAlpha (0.55f));
            g.drawDashedLine ({ x0, pr.getY(), x0, pr.getBottom() }, dash, 2, 1.0f);
            g.drawDashedLine ({ x1, pr.getY(), x1, pr.getBottom() }, dash, 2, 1.0f);
            g.setFont (theme::mono (9.0f, true));
            for (auto x : { x0, x1 })
            {
                auto tab = juce::Rectangle<float> (x - 15, pr.getBottom() - 18, 30, 16);
                g.setColour (theme::res.withAlpha (0.85f)); g.fillRoundedRectangle (tab, 3.0f);
                g.setColour (theme::onAccent); g.drawText ("RES", tab, juce::Justification::centred);
            }
            constexpr int D = ResSuppressor::kDisp;
            juce::Path p; p.startNewSubPath (xOf (20.0), yOf (0));
            float mx = 0;
            for (int d = 0; d < D; ++d)
            {
                const float v = ph.proc.rs.dispDb[(size_t) d].load(); mx = std::max (mx, v);
                const double f = 20.0 * std::pow (1000.0, (d + 0.5) / D);
                p.lineTo (xOf (f), yOf (-juce::jlimit (0.0f, dbRange, v)));
            }
            p.lineTo (xOf (20000.0), yOf (0)); p.closeSubPath();
            if (mx > 0.05f) { g.setColour (theme::res.withAlpha (0.38f)); g.fillPath (p); g.setColour (theme::res); g.strokePath (p, juce::PathStrokeType (1.2f)); }
        }
        if (cur.mag.empty()) return;
        const int s = ph.selected();
        for (int i = 0; i < eqcore::kBands; ++i)
        {
            const auto& d = dynArea[(size_t) i];
            if (d.lo.size() != fr.size() || d.hi.size() != fr.size()) continue;
            auto y = [&] (double v) { return yOf (juce::jlimit (-(double) dbRange, (double) dbRange, v)); };
            juce::Path p; p.startNewSubPath (xOf (fr.front()), y (d.hi.front()));
            for (size_t k = 1; k < fr.size(); ++k) p.lineTo (xOf (fr[k]), y (d.hi[k]));
            for (size_t k = fr.size(); k-- > 0;) p.lineTo (xOf (fr[k]), y (d.lo[k]));
            p.closeSubPath();
            g.setColour (theme::bandColour (i).withAlpha (i == s ? 0.22f : 0.13f)); g.fillPath (p);
        }
        if (! selCurve.empty())
        {
            juce::Path p; p.startNewSubPath (xOf (fr.front()), yOf (0));
            for (size_t i = 0; i < fr.size(); ++i) p.lineTo (xOf (fr[i]), yOf (juce::jlimit (-(double) dbRange, (double) dbRange, selCurve[i])));
            p.lineTo (xOf (fr.back()), yOf (0)); p.closeSubPath();
            g.setColour (theme::bandColour (s).withAlpha (0.16f)); g.fillPath (p);
        }
        // phase / group delay: over the curve (right scale) or in its own panel
        const bool gd = (int) ph.proc.uiState.getProperty ("view", 0) == 1;
        const double R = gd ? 20.0 : 360.0;
        {
            const auto pp = pr;      // dotted over the curve in both modes; Split adds a solid copy in the panel
            auto trace = [&] (const eqcore::CurvesV2& c, juce::Colour col, float alpha)
            {
                juce::Path p; bool first = true;
                for (size_t i = 0; i < fr.size(); ++i)
                {
                    const double v = juce::jlimit (-R, R, gd ? c.gd[i] : c.phase[i]);
                    const float x = xOf (fr[i]), y = juce::jmap ((float) v, (float) -R, (float) R, pp.getBottom(), pp.getY());
                    if (first) { p.startNewSubPath (x, y); first = false; } else p.lineTo (x, y);
                }
                { juce::Path dashed; const float dash[] = { 2.0f, 4.0f }; juce::PathStrokeType (2.0f).createDashedStroke (dashed, p, dash, 2); g.setColour (col.withAlpha (alpha)); g.fillPath (dashed); }
            };
            if (anyMS && ! curS.mag.empty()) trace (curS, theme::side, 0.6f);
            if (! liveCur.phase.empty()) trace (cur, theme::phase, 0.3f);              // static phase, faint, under the live one
            trace (liveCur.phase.empty() ? cur : liveCur, theme::phase, 0.85f);
            g.setFont (theme::mono (10.0f));
            for (int k = -2; k <= 2; ++k)
            {
                const double v = R * k / 2.0;
                const float y = juce::jmap ((float) v, (float) -R, (float) R, pp.getBottom(), pp.getY());
                const juce::String t = (v > 0 ? "+" : v < 0 ? juce::String::fromUTF8 ("\xe2\x88\x92") : "") + juce::String ((int) std::abs (v)) + (gd ? " ms" : juce::String::fromUTF8 ("\xc2\xb0"));
                g.setColour (theme::phase.withAlpha (0.8f));
                g.drawText (t, (int) pp.getRight() - 64, juce::jlimit ((int) pp.getY() + 26, (int) (panelShown() ? panelArea().getY() : pp.getBottom()) - 14, (int) y - 7), 60, 14, juce::Justification::centredRight);
            }
        }
        // analog ideal (zero latency only): shows the cramping near Nyquist
        if (! ghost.mag.empty())
        {
            juce::Path p; for (size_t i = 0; i < fr.size(); ++i) { const float x = xOf (fr[i]), y = yOf (juce::jlimit (-(double) dbRange, (double) dbRange, ghost.mag[i])); if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y); }
            juce::Path d; const float dash[] = { 4.0f, 3.0f }; juce::PathStrokeType (1.2f).createDashedStroke (d, p, dash, 2);
            g.setColour (theme::muted.withAlpha (0.6f)); g.fillPath (d);
        }
        // side channel
        if (anyMS && ! curS.mag.empty())
        {
            juce::Path p; for (size_t i = 0; i < fr.size(); ++i) { const float x = xOf (fr[i]), y = yOf (juce::jlimit (-(double) dbRange, (double) dbRange, curS.mag[i])); if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y); }
            juce::Path d; const float dash[] = { 6.0f, 3.0f }; juce::PathStrokeType (2.0f).createDashedStroke (d, p, dash, 2);
            g.setColour (theme::side); g.fillPath (d);
        }
        // magnitude (mid, or both when there are no M/S bands)
        {
            juce::Path p, fill;
            for (size_t i = 0; i < fr.size(); ++i)
            {
                const float x = xOf (fr[i]), y = yOf (juce::jlimit (-(double) dbRange, (double) dbRange, cur.mag[i]));
                if (i == 0) { p.startNewSubPath (x, y); fill.startNewSubPath (x, yOf (0)); }
                else p.lineTo (x, y);
                fill.lineTo (x, y);
            }
            fill.lineTo (xOf (fr.back()), yOf (0)); fill.closeSubPath();
            g.setColour (theme::curve.withAlpha (0.08f)); g.fillPath (fill);
            g.setColour (liveMag.empty() ? theme::curve : theme::curve.withAlpha (0.45f));
            g.strokePath (p, juce::PathStrokeType (liveMag.empty() ? 1.9f : 1.3f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
        if (! liveMag.empty())
        {
            juce::Path p;
            for (size_t i = 0; i < fr.size(); ++i) { const float x = xOf (fr[i]), y = yOf (juce::jlimit (-(double) dbRange, (double) dbRange, liveMag[i])); if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y); }
            g.setColour (theme::live); g.strokePath (p, juce::PathStrokeType (2.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
        // Delta: what the Delta button plays, |H − 1|
        if (ph.value (pid::delta) > 0.5f)
        {
            juce::Path p;
            for (size_t i = 0; i < fr.size(); ++i) { const float x = xOf (fr[i]), y = yOf (juce::jlimit (-(double) dbRange, (double) dbRange, cur.delta[i])); if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y); }
            juce::Path d; const float dash[] = { 6.0f, 3.0f }; juce::PathStrokeType (2.2f).createDashedStroke (d, p, dash, 2);
            g.setColour (theme::pre); g.fillPath (d);
            auto box = juce::Rectangle<float> (pr.getX() + 8, pr.getY() + 24, 330, 40);
            g.setColour (theme::pre.withAlpha (0.14f)); g.fillRoundedRectangle (box, 5.0f);
            g.setColour (theme::pre.withAlpha (0.5f)); g.drawRoundedRectangle (box, 5.0f, 1.0f);
            g.setColour (theme::pre); g.setFont (theme::ui (10.5f, true));
            g.drawText (juce::String::fromUTF8 ("Delta |H \xe2\x88\x92 1|"), box.reduced (8, 4).withHeight (14), juce::Justification::centredLeft);
            g.setColour (theme::text); g.setFont (theme::ui (10.0f));
            g.drawFittedText (ph.tr ("位相が 60° 以上回る帯域と +6 dB を超えるブーストでは、差分が原音（0 dB）より大きくなる。異常ではない",
                                     "Where the phase turns more than 60°, or a boost exceeds +6 dB, the difference is louder than the input (0 dB). That is expected"),
                              box.reduced (8, 4).withTrimmedTop (14).toNearestInt(), juce::Justification::centredLeft, 2, 0.9f);
        }
        // dynamic bands: ABOVE (filled) / BELOW (ring) marks on the edges of the range area, drag up/down
        for (int i = 0; i < eqcore::kBands; ++i)
            for (int w = 0; w < 2; ++w)
                if (auto pt = dynMark (i, w))
                {
                    const bool hot = dynHover == i * 2 + w || dynDrag == i * 2 + w;
                    const float r = hot ? 6.0f : 4.5f;
                    juce::Path tri;   // points away from the static curve: up for +, down for −
                    const float v = ph.value (pid::band (i, w == 0 ? "range" : "below")), dir = v >= 0 ? -1.0f : 1.0f;
                    tri.addTriangle (pt->x - r, pt->y - dir * r * 0.55f, pt->x + r, pt->y - dir * r * 0.55f, pt->x, pt->y + dir * r * 0.75f);
                    g.setColour (theme::well); g.strokePath (tri, juce::PathStrokeType (2.5f));
                    g.setColour (theme::bandColour (i));
                    if (w == 0) g.fillPath (tri); else { g.setColour (theme::well); g.fillPath (tri); g.setColour (theme::bandColour (i)); g.strokePath (tri, juce::PathStrokeType (1.4f)); }
                }
        // width handles
        if (auto h = handles (s); h.has_value())
        {
            g.setColour (theme::bandColour (s));
            const float dash[] = { 3.0f, 3.0f };
            g.drawDashedLine ({ h->first, h->second }, dash, 2, 1.0f);
            for (auto pt : { h->first, h->second })
            {
                g.setColour (theme::well); g.fillRoundedRectangle (pt.x - 4, pt.y - 9, 8, 18, 3);
                g.setColour (theme::bandColour (s)); g.drawRoundedRectangle (pt.x - 4, pt.y - 9, 8, 18, 3, 2.0f);
            }
        }
        paintPanel (g, split);      // the lower panel (phase in Split, or T/B scope / stereo field) over the curve, under the nodes
        // nodes, with Lin / M / S / D tags underneath
        const auto bands = ph.proc.currentBands();
        const int solo = ph.proc.soloBand.load();
        for (int i = 0; i < eqcore::kBands; ++i)
        {
            if (hiddenBand (i, bands[(size_t) i])) continue;
            const auto c = nodePos (i, bands[(size_t) i]);
            const float rad = i == s ? 11.0f : 8.0f;
            if (bands[(size_t) i].on)
            {
                g.setColour (theme::bandColour (i)); g.fillEllipse (c.x - rad, c.y - rad, rad * 2, rad * 2);
                g.setColour (theme::well); g.drawEllipse (c.x - rad, c.y - rad, rad * 2, rad * 2, 2.5f);
                if (i == s) { g.setColour (theme::bandColour (i)); g.drawEllipse (c.x - rad - 3, c.y - rad - 3, rad * 2 + 6, rad * 2 + 6, 1.5f); }
                if (i == (int) ph.proc.uiState.getProperty ("focus", -1) || (i == ph.selected() && (double) ph.proc.uiState.getProperty ("focusUntil", 0.0) > juce::Time::getMillisecondCounterHiRes()))
                    if ((double) ph.proc.uiState.getProperty ("focusUntil", 0.0) > juce::Time::getMillisecondCounterHiRes())
                    {
                        const float ph2 = (float) std::fmod (juce::Time::getMillisecondCounterHiRes() / 600.0, 1.0);
                        const float rr = rad + 6 + 10 * ph2;
                        g.setColour (theme::bandColour (i).withAlpha (0.8f * (1 - ph2))); g.drawEllipse (c.x - rr, c.y - rr, rr * 2, rr * 2, 2.0f);
                    }
                if (i == solo) { g.setColour (theme::pre); g.drawEllipse (c.x - rad - 6, c.y - rad - 6, rad * 2 + 12, rad * 2 + 12, 1.5f); }
                g.setColour (theme::onAccent);
            }
            else
            {
                g.setColour (theme::well); g.fillEllipse (c.x - rad, c.y - rad, rad * 2, rad * 2);
                g.setColour (theme::bandColour (i).withAlpha (0.7f)); g.drawEllipse (c.x - rad, c.y - rad, rad * 2, rad * 2, 1.5f);
            }
            g.setFont (theme::mono (10.0f, true));
            g.drawText (juce::String (i + 1), juce::Rectangle<float> (c.x - rad, c.y - rad, rad * 2, rad * 2), juce::Justification::centred);
            if (bands[(size_t) i].on)
            {
                const auto tag = bandTag (i, bands[(size_t) i]);
                if (tag.isNotEmpty())
                {
                    g.setColour (theme::bandColour (i)); g.setFont (theme::mono (8.5f, true));
                    g.drawText (tag, juce::Rectangle<float> (c.x - 40, c.y + rad + 2, 80, 11), juce::Justification::centred);
                }
            }
        }
        // labels
        g.setFont (theme::ui (10.5f, true));
        g.setColour (theme::muted);
        g.drawText (anyLR ? juce::String ("Magnitude dB  -  Left (solid) / Right (dashed)") : anyMS ? juce::String ("Magnitude dB  -  Mid (solid) / Side (dashed)") : juce::String ("Magnitude dB (left)"), (int) pr.getX() + 6, 8, 300, 16, juce::Justification::centredLeft);
        g.setColour (theme::phase.withAlpha (0.85f));
        const juce::String ptxt = gd ? juce::String ("Group delay") : juce::String ("Phase");
        if (autoAna) { g.setColour (theme::accent); g.setFont (theme::ui (9.5f, true)); g.drawText ("SOLO auto", 300, 6, 70, 20, juce::Justification::centredLeft); g.setColour (theme::phase.withAlpha (0.85f)); g.setFont (theme::ui (10.5f, true)); }
        g.drawText (ptxt + " (right, dotted)", getWidth() - 512, 8, 210, 16, juce::Justification::centredRight);
    }

    // ---- mouse ----
    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu())
        {
            ph.proc.uiState.setProperty ("popup", ! (bool) ph.proc.uiState.getProperty ("popup", true), nullptr);
            lastRightClick = juce::Time::getMillisecondCounter();
            drag = {}; return;
        }
        if (ph.proc.earActive()) return;
        if (panelEdgeHit (e.position)) { panelDrag = true; repaint(); return; }
        if (const int k = scopeHit (e.position); k != 0)
        {
            if (e.mods.isAltDown()) { ph.resetDefault (k == 1 ? pid::tbSens : pid::tbWidth); return; }
            scDrag = k; scStartY = e.position.y; scStartSens = ph.value (pid::tbSens); scStartWidth = ph.value (pid::tbWidth);
            ph.proc.undo.beginNewTransaction(); ph.begin (k == 1 ? pid::tbSens : pid::tbWidth); return;
        }
        if (const int m = dynHit (e.position); m >= 0)
        {
            const juce::String id = pid::band (m / 2, (m % 2) == 0 ? "range" : "below");
            ph.select (m / 2);
            if (e.mods.isAltDown()) { ph.proc.undo.beginNewTransaction(); ph.resetDefault (id); return; }
            dynDrag = m; ph.proc.undo.beginNewTransaction(); ph.begin (id); repaint(); return;
        }
        drag = pick (e.position);
        if (drag.what >= 3) { ph.begin (edgeId (drag.what)); return; }
        if (drag.band < 0) return;
        ph.select (drag.band);
        if (e.mods.isAltDown()) { altSolo = true; ph.proc.soloBand = drag.band; }
        downPos = e.position; axis = 0; scaleHold = true;   // drag above the plot to go past the current scale; it widens on release
        startF = ph.value (pid::band (drag.band, "freq")); startG = ph.value (pid::band (drag.band, "gain"));
        startQ = ph.value (pid::band (drag.band, "q")); startY = e.position.y;
        for (auto w : { "freq", "gain", "q" }) ph.begin (pid::band (drag.band, w));
        if (ph.value (pid::band (drag.band, "on")) < 0.5f) ph.setGesture (pid::band (drag.band, "on"), 1.0f);
    }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu()) return;
        if (panelDrag)
        {
            const auto fp = fullPlot();
            ph.proc.uiState.setProperty ("panelFrac", juce::jlimit (0.15f, 0.85f, (e.position.y - fp.getY()) / fp.getHeight()), nullptr);
            resized(); repaint(); return;
        }
        if (scDrag != 0)
        {
            const float dd = (scStartY - e.position.y) / dbPx() * (e.mods.isShiftDown() ? 0.2f : 1.0f);   // dB moved, up = +
            if (scDrag == 1) ph.set (pid::tbSens, juce::jlimit (-12.0f, 12.0f, std::round ((scStartSens - dd) * 10.0f) / 10.0f));   // lower line = −Sens
            else ph.set (pid::tbWidth, juce::jlimit (1.0f, 18.0f, std::round ((scStartWidth + dd) * 10.0f) / 10.0f));
            return;
        }
        if (dynDrag >= 0)
        {
            // the mark follows the mouse along the area's edge: value = (mouse dB − static curve) / sensitivity
            const int i = dynDrag / 2; const auto& d = dynArea[(size_t) i];
            if (d.st.size() != fr.size()) return;
            const size_t k = juce::jlimit<size_t> (0, fr.size() - 1, d.peak);
            const auto pr = plot();
            const double db = juce::jmap ((double) e.position.y, (double) pr.getBottom(), (double) pr.getY(), -(double) dbRange, (double) dbRange);
            const double v = (db - d.st[k]) / std::max (0.05, d.u[k]);
            ph.set (pid::band (i, (dynDrag % 2) == 0 ? "range" : "below"), (float) juce::jlimit (-18.0, 18.0, std::round (v * 10.0) / 10.0));
            return;
        }
        if (drag.what >= 3)
        {
            const char* id = edgeId (drag.what);
            float f = (float) fOf (e.position.x);
            const bool low = drag.what == 3 || drag.what == 5;
            const char* other = edgeId (low ? drag.what + 1 : drag.what - 1);
            if (low) f = std::min (f, ph.value (other) / 1.06f); else f = std::max (f, ph.value (other) * 1.06f);
            ph.set (id, juce::jlimit (20.0f, 20000.0f, f)); return;
        }
        if (drag.band < 0) return;
        const int b = drag.band; const auto pr = plot();
        const int t = (int) ph.value (pid::band (b, "type"));
        if (drag.what == 0)
        {
            if (e.mods.isShiftDown()) { ph.set (pid::band (b, "q"), (float) juce::jlimit (0.1, 18.0, startQ * std::pow (1.015, startY - e.position.y))); return; }
            startY = e.position.y; startQ = ph.value (pid::band (b, "q"));
            // Cmd (Ctrl on Windows) + drag: lock to the first direction moved
            if (e.mods.isCommandDown())
            {
                const auto d = e.position - downPos;
                if (axis == 0 && d.getDistanceFromOrigin() > 4.0f) axis = std::abs (d.x) >= std::abs (d.y) ? 1 : 2;
                if (axis == 0) return;
            }
            else axis = 0;
            const bool noGain = t == (int) eqcore::Type::LowCut || t == (int) eqcore::Type::HighCut || t == (int) eqcore::Type::Notch || t == (int) eqcore::Type::AllPass;
            ph.set (pid::band (b, "freq"), axis == 2 ? startF : (float) fOf (e.position.x));
            if (! noGain) ph.set (pid::band (b, "gain"), axis == 1 ? startG : juce::jlimit (-30.0f, 30.0f, juce::jmap (e.position.y, pr.getBottom(), pr.getY(), -dbRange, dbRange)));
        }
        else
        {
            // width handle: each one stays on its own side of the centre
            const double f = ph.value (pid::band (b, "freq"));
            const double side = drag.what == 2 ? 1.0 : -1.0;
            const double d = side * std::log2 (fOf (e.position.x) / f);
            const double oct = juce::jlimit (0.04, 6.0, 2.0 * std::max (0.02, d));
            const double qt = 1.0 / (2.0 * std::sinh (std::log (2.0) / 2.0 * oct));
            eqcore::Band eb; eb.type = (eqcore::Type) t; eb.g = ph.value (pid::band (b, "gain")); eb.q = ph.value (pid::band (b, "q"));
            const double k = eqcore::effectiveQ (eb, ph.proc.currentQMode()) / eb.q;
            ph.set (pid::band (b, "q"), (float) juce::jlimit (0.1, 18.0, qt / k));
        }
    }
    void mouseUp (const juce::MouseEvent&) override
    {
        if (scaleHold) { scaleHold = false; repaint(); }
        if (panelDrag) { panelDrag = false; repaint(); return; }
        if (scDrag != 0) { ph.end (scDrag == 1 ? pid::tbSens : pid::tbWidth); scDrag = 0; return; }
        if (dynDrag >= 0) { ph.end (pid::band (dynDrag / 2, (dynDrag % 2) == 0 ? "range" : "below")); dynDrag = -1; repaint(); return; }
        if (drag.what >= 3) ph.end (edgeId (drag.what));
        else if (drag.band >= 0) for (auto w : { "freq", "gain", "q" }) ph.end (pid::band (drag.band, w));
        drag = {}; axis = 0;
        if (altSolo) { altSolo = false; ph.proc.soloBand = -1; }
    }
    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        if (ph.proc.earActive()) return;
        const auto p = pick (e.position);
        if (p.what >= 3) return;
        if (p.band >= 0 && p.what == 0) { const int b = p.band; ph.setGesture (pid::band (b, "on"), ph.value (pid::band (b, "on")) > 0.5f ? 0.0f : 1.0f); return; }
        for (int i = 0; i < eqcore::kBands; ++i)
            if (ph.value (pid::band (i, "on")) < 0.5f)
            {
                const auto pr = plot();
                // the shape follows the frequency clicked: < 45 Hz low cut, < 180 Hz low shelf, < 6 kHz bell, < 14 kHz high shelf, above that high cut
                const float f = (float) fOf (e.position.x);
                const auto ty = f < 45.0f ? eqcore::Type::LowCut : f < 180.0f ? eqcore::Type::LowShelf : f < 6000.0f ? eqcore::Type::Bell
                              : f < 14000.0f ? eqcore::Type::HighShelf : eqcore::Type::HighCut;
                const bool cut = ty == eqcore::Type::LowCut || ty == eqcore::Type::HighCut;
                ph.proc.undo.beginNewTransaction();
                ph.setGesture (pid::band (i, "type"), (float) (int) ty); ph.setGesture (pid::band (i, "freq"), f);
                ph.setGesture (pid::band (i, "gain"), cut ? 0.0f : juce::jlimit (-30.0f, 30.0f, juce::jmap (e.position.y, pr.getBottom(), pr.getY(), -dbRange, dbRange)));
                ph.setGesture (pid::band (i, "slope"), cut ? 3.0f : 1.0f);   // cuts 24 dB/oct, shelves 12 dB/oct
                ph.setGesture (pid::band (i, "q"), ty == eqcore::Type::Bell ? 1.0f : 0.707f); ph.setGesture (pid::band (i, "on"), 1.0f); ph.select (i);
                return;
            }
    }
    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override
    {
        // trackpads send zero-length and momentum events (also around a two-finger click): ignore them
        if (w.isInertial || std::abs (w.deltaY) < 0.01f || ph.proc.earActive()) return;
        if (juce::Time::getMillisecondCounter() - lastRightClick < 500) return;
        auto p = pick (e.position); if (p.what >= 3) return;
        const int b = p.band >= 0 ? p.band : ph.selected();
        if (ph.value (pid::band (b, "on")) < 0.5f) return;
        const float q = ph.value (pid::band (b, "q"));
        const float dir = (w.isReversed ? -1.0f : 1.0f) * w.deltaY;
        const float stepPow = juce::jlimit (-1.0f, 1.0f, dir * 4.0f);       // proportional, max ±10 % per event
        ph.setGesture (pid::band (b, "q"), juce::jlimit (0.1f, 18.0f, q * std::pow (1.1f, stepPow)));
    }
    void mouseMove (const juce::MouseEvent& e) override
    {
        if (scopeHit (e.position) != 0 || panelEdgeHit (e.position)) { setMouseCursor (juce::MouseCursor::UpDownResizeCursor); return; }
        if (const int m = dynHit (e.position); m != dynHover) { dynHover = m; repaint(); }
        if (dynHover >= 0)
        {
            setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
            return;
        }
        const auto p = pick (e.position);
        setMouseCursor (p.what >= 1 ? juce::MouseCursor::LeftRightResizeCursor
                        : p.band < 0 ? juce::MouseCursor::CrosshairCursor
                        : e.mods.isCommandDown() ? juce::MouseCursor::UpDownLeftRightResizeCursor : juce::MouseCursor::DraggingHandCursor);
    }

private:
    // dynamic range marks: w 0 = ABOVE (range), 1 = BELOW. They sit at the area's peak, on its edge.
    size_t frIndex (double f) const { const auto it = std::lower_bound (fr.begin(), fr.end(), f); return (size_t) juce::jlimit<long> (0, (long) fr.size() - 1, (long) (it - fr.begin())); }
    std::optional<juce::Point<float>> dynMark (int i, int w) const
    {
        const auto& d = dynArea[(size_t) i];
        if (d.st.size() != fr.size() || fr.empty()) return std::nullopt;
        const size_t k = juce::jlimit<size_t> (0, fr.size() - 1, d.peak);
        const float x = xOf (fr[k]);
        const double v = d.st[k] + ph.value (pid::band (i, w == 0 ? "range" : "below")) * d.u[k];
        return juce::Point<float> (x, yOf (juce::jlimit (-(double) dbRange, (double) dbRange, v)));
    }
    int dynHit (juce::Point<float> p) const
    {
        for (int i = eqcore::kBands; --i >= 0;)
            for (int w = 0; w < 2; ++w)
                if (auto m = dynMark (i, w))
                    if (m->getDistanceFrom (p) < 8.0f && nodePos (i, ph.proc.currentBands()[(size_t) i]).getDistanceFrom (p) > 9.0f) return i * 2 + w;
        return -1;
    }
    int dynHover = -1, dynDrag = -1;
    struct Pick { int band = -1; int what = 0; };   // what: 0 node, 1/2 width handles, 3/4 saturator band edges, 5/6 suppressor range edges
    static const char* edgeId (int what) { return what == 3 ? pid::satLo : what == 4 ? pid::satHi : what == 5 ? pid::rsLo : pid::rsHi; }
    bool tbView() const { return lastScope && ! lastField; }        // the T/B scope (not the stereo field) is in the lower panel
    juce::Rectangle<float> fullPlot() const { return getLocalBounds().toFloat().withTrimmedLeft (34).withTrimmedRight (8).withTrimmedTop (30).withTrimmedBottom (tbView() ? 54.0f : 18.0f); }
    bool panelShown() const { return (lastSplit || lastScope) && ! ph.proc.earActive(); }
    float panelFrac() const { return juce::jlimit (0.15f, 0.85f, (float) (double) ph.proc.uiState.getProperty ("panelFrac", 0.56)); }
    juce::Rectangle<float> panelArea() const { auto p = fullPlot(); return p.withTrimmedTop (p.getHeight() * panelFrac()); }
    // in Split the EQ curve uses the part above the panel; the analyzer and the grid run under the (translucent) panel to the bottom
    juce::Rectangle<float> plot() const { auto p = fullPlot(); return panelShown() && lastSplit ? p.withBottom (panelArea().getY() - 6) : p; }   // Split: fit above the panel, Overlay: fixed full height
    juce::Rectangle<float> phasePlot() const { return panelArea().withTrimmedTop (20).withTrimmedBottom (6); }
    // where the scope goes: the phase panel in Split, the lower part of the graph in Overlay
    juce::Rectangle<float> scopeArea() const { return panelArea(); }
    bool panelDrag = false;
    bool panelEdgeHit (juce::Point<float> p) const { return panelShown() && std::abs (p.y - panelArea().getY()) <= 4.0f && fullPlot().getHorizontalRange().contains (p.x); }
    juce::Rectangle<float> scopeWave() const { auto a = scopeArea().withTrimmedTop (22); return a.withHeight (a.getHeight() * 0.58f); }
    juce::Rectangle<float> scopeDet() const { auto a = scopeArea().withTrimmedTop (22); return a.withTrimmedTop (a.getHeight() * 0.58f + 4); }
    static constexpr float detLo = -12.0f, detHi = 30.0f;       // detector strip range, dB
    float dbPx() const { return scopeDet().getHeight() / (detHi - detLo); }
    float yDet (float db) const { const auto r = scopeDet(); return juce::jmap (juce::jlimit (detLo, detHi, db), detLo, detHi, r.getBottom(), r.getY()); }
    static juce::Colour colT() { return juce::Colour (0xffff7a59); }
    static juce::Colour colB() { return juce::Colour (0xff3fc9bd); }
    static juce::Colour srcColour (int src, juce::Colour fallback) { return src == 3 ? colT() : src == 4 ? colB() : src == 2 ? theme::side : fallback; }
    int scopeHit (juce::Point<float> p) const      // 1 = lower line (Sens), 2 = upper line (Width)
    {
        if (! tbView() || ph.proc.earActive() || ! scopeDet().expanded (0, 6).contains (p)) return 0;
        const float s = ph.value (pid::tbSens), w = ph.value (pid::tbWidth);
        const float y0 = yDet (-s), y1 = yDet (w - s);
        if (std::abs (p.y - y1) <= 5.0f) return 2;
        if (std::abs (p.y - y0) <= 5.0f) return 1;
        return 0;
    }
    static const double* spans() { static const double s[] = { 1.0, 2.0, 4.0, 8.0 }; return s; }
    double spanSec() const { return spans()[juce::jlimit (0, 3, (int) ph.proc.uiState.getProperty ("scopeSpan", 1))]; }
    int colLen() const { return std::max (1, (int) std::round (ph.proc.getSampleRateOrDefault() * spanSec() / scCols)); }
    float rawAt (juce::uint64 p) const
    {
        if (scPaused) return frozen[(size_t) (p - (frozenEnd - frozen.size()))];
        return ph.proc.scopeRaw[(size_t) (p & (juce::uint64) (EQAnatomyProcessor::scopeRawN - 1))];
    }
    void scopeFeed (float x)
    {
        const double m = sdet.process (sdeq.active() ? sdeq.process ((double) x) : (double) x), ax = std::abs ((double) x);
        sPk = std::max (sPk, ax); sAcc += ax; sAccT += ax * m; sDmax = std::max (sDmax, sdet.lastD);
        if (++sCnt >= colLen())
        {
            cPk[(size_t) cW] = (float) sPk; cT[(size_t) cW] = (float) (sAcc > 1e-12 ? sAccT / sAcc : 0.0); cD[(size_t) cW] = (float) sDmax;
            cW = (cW + 1) % scCols; sAcc = sAccT = sPk = 0; sDmax = -100; sCnt = 0;
        }
    }
    // run the detector again over what is stored (after a change of span or Sens / Width / Attack, or while paused)
    void scopeRebuild()
    {
        const double fs = ph.proc.getSampleRateOrDefault();
        const juce::uint64 end = scPaused ? frozenEnd : ph.proc.scopeRawW.load (std::memory_order_acquire);
        const juce::uint64 first = scPaused ? frozenEnd - frozen.size() : std::max (scEnableW, end > (juce::uint64) (EQAnatomyProcessor::scopeRawN - 16384) ? end - (juce::uint64) (EQAnatomyProcessor::scopeRawN - 16384) : (juce::uint64) 0);
        const juce::uint64 L = (juce::uint64) colLen(), warm = (juce::uint64) (fs * 0.3);
        const juce::uint64 start = std::max (first, end > L * scCols + warm ? end - (L * scCols + warm) : (juce::uint64) 0);
        cPk.fill (0.0f); cT.fill (0.0f); cD.fill (-100.0f); cW = 0;
        sdet.prepare (fs);
        sdet.sens = ph.value (pid::tbSens); sdet.width = ph.value (pid::tbWidth); sdet.setAttack (ph.value (pid::tbAtt)); sdet.method = (int) ph.value (pid::tbMethod);
        sdeq.prepare (fs);
        sdeq.set (ph.value (pid::tbLc), ph.value (pid::tbE1f), ph.value (pid::tbE1g), ph.value (pid::tbE1q), ph.value (pid::tbE2f), ph.value (pid::tbE2g), ph.value (pid::tbE2q));
        sAcc = sAccT = sPk = 0; sDmax = -100; sCnt = 0;
        juce::uint64 p = start;
        const juce::uint64 colsFrom = std::max (start + std::min (warm, end - start), end > L * scCols ? end - L * scCols : (juce::uint64) 0);
        const juce::uint64 aligned = colsFrom + (end - colsFrom) % L;          // the last column ends exactly at "now"
        for (; p < aligned; ++p) { const double x = (double) rawAt (p); sdet.process (sdeq.active() ? sdeq.process (x) : x); }
        // leave empty columns on the left when there is not enough audio yet
        cW = (int) ((scCols - (int) ((end - aligned) / L) % scCols) % scCols);
        for (; p < end; ++p) scopeFeed (rawAt (p));
        sRead = end;
    }
    void scopeTick()
    {
        juce::String sig; sig << ph.value (pid::tbSens) << "_" << ph.value (pid::tbWidth) << "_" << ph.value (pid::tbAtt) << "_" << spanSec() << "_" << ph.value (pid::tbMethod);
        for (auto* id : { pid::tbLc, pid::tbE1f, pid::tbE1g, pid::tbE1q, pid::tbE2f, pid::tbE2g, pid::tbE2q }) sig << "_" << ph.value (id);
        sig << "_" << ph.proc.getSampleRateOrDefault();
        if (sig != scSig) { scSig = sig; scopeRebuild(); return; }
        if (scPaused) return;
        const auto w = ph.proc.scopeRawW.load (std::memory_order_acquire);
        if (w < sRead || w - sRead > (juce::uint64) (EQAnatomyProcessor::scopeRawN / 2)) { scopeRebuild(); return; }
        for (auto p = sRead; p < w; ++p) scopeFeed (rawAt (p));
        sRead = w;
    }
    void setPaused (bool p)
    {
        if (p == scPaused) return;
        if (p)
        {
            const juce::uint64 end = ph.proc.scopeRawW.load (std::memory_order_acquire);
            const juce::uint64 keep = std::min ((juce::uint64) (ph.proc.getSampleRateOrDefault() * 8.4), (juce::uint64) (EQAnatomyProcessor::scopeRawN - 16384));
            const juce::uint64 first = std::max (scEnableW, end > keep ? end - keep : (juce::uint64) 0);
            frozen.resize ((size_t) (end - first));
            for (auto q = first; q < end; ++q) frozen[(size_t) (q - first)] = ph.proc.scopeRaw[(size_t) (q & (juce::uint64) (EQAnatomyProcessor::scopeRawN - 1))];
            frozenEnd = end;
        }
        scPaused = p;
        scPause.setButtonText (p ? juce::String::fromUTF8 ("\xe2\x96\xb6 Play") : juce::String::fromUTF8 ("\xe2\x9d\x9a\xe2\x9d\x9a Pause"));
        scPause.setToggleState (p, juce::dontSendNotification);
        scopeRebuild();
    }
    void paintPanel (juce::Graphics& g, bool split) const
    {
        if (! panelShown()) return;
        if (lastScope) { if (lastField) paintField (g); else paintScope (g); }
        else if (split)
        {
            const auto a = panelArea(), pp = phasePlot();
            g.setColour (theme::splitWell.withAlpha (0.7f)); g.fillRect (a);
            g.setColour (theme::line); g.drawHorizontalLine ((int) a.getY(), a.getX(), a.getRight());
            for (double f : { 50., 100., 200., 500., 1000., 2000., 5000., 10000. }) { g.setColour (theme::grid); g.drawVerticalLine ((int) xOf (f), pp.getY(), pp.getBottom()); }
            g.setColour (theme::zero); g.drawHorizontalLine ((int) pp.getCentreY(), pp.getX(), pp.getRight());
            const bool gd = (int) ph.proc.uiState.getProperty ("view", 0) == 1;
            const double R = gd ? 20.0 : 360.0;
            auto trace = [&] (const eqcore::CurvesV2& c, juce::Colour col, float alpha)
            {
                if (c.phase.size() != fr.size()) return;
                juce::Path p;
                for (size_t i = 0; i < fr.size(); ++i)
                {
                    const double v = juce::jlimit (-R, R, gd ? c.gd[i] : c.phase[i]);
                    const float x = xOf (fr[i]), y = juce::jmap ((float) v, (float) -R, (float) R, pp.getBottom(), pp.getY());
                    if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
                }
                g.setColour (col.withAlpha (alpha)); g.strokePath (p, juce::PathStrokeType (2.0f));
            };
            if (anyMS && ! curS.mag.empty()) trace (curS, theme::side, 0.6f);
            if (! liveCur.phase.empty()) trace (cur, theme::phase, 0.3f);
            trace (liveCur.phase.empty() ? cur : liveCur, theme::phase, 0.9f);
            g.setFont (theme::mono (10.0f));
            for (int k = -2; k <= 2; ++k)
            {
                const double v = R * k / 2.0;
                const float y = juce::jmap ((float) v, (float) -R, (float) R, pp.getBottom(), pp.getY());
                const juce::String t = (v > 0 ? "+" : v < 0 ? "-" : "") + juce::String ((int) std::abs (v)) + (gd ? " ms" : juce::String::fromUTF8 ("\xc2\xb0"));
                g.setColour (theme::phase.withAlpha (0.8f));
                g.drawText (t, (int) pp.getRight() - 64, juce::jlimit ((int) pp.getY(), (int) pp.getBottom() - 14, (int) y - 7), 60, 14, juce::Justification::centredRight);
            }
            g.setColour (theme::phase.withAlpha (0.85f)); g.setFont (theme::ui (10.5f, true));
            g.drawText (gd ? "Group delay" : "Phase", (int) a.getX() + 6, (int) a.getY() + 3, 200, 14, juce::Justification::centredLeft);
        }
        // the edge: drag to share the height between the curve and the panel
        const auto a = panelArea();
        g.setColour (panelDrag ? theme::accent : theme::muted.withAlpha (0.8f));
        g.fillRoundedRectangle (a.getCentreX() - 18, a.getY() - 2, 36, 4, 2);
    }
    void paintField (juce::Graphics& g) const
    {
        if (ph.proc.earActive()) return;
        const auto a = scopeArea();
        g.setColour (theme::splitWell.withAlpha (0.7f)); g.fillRect (a);
        g.setColour (theme::line); g.drawHorizontalLine ((int) a.getY(), a.getX(), a.getRight());
        g.setFont (theme::ui (9.5f)); g.setColour (theme::muted);
        g.setFont (theme::ui (10.0f, true)); g.drawText ("STEREO FIELD", (int) a.getX() + 6, (int) a.getY() + 2, 100, 17, juce::Justification::centredLeft);
        g.setFont (theme::ui (9.5f));
        g.drawText ("per band  /  grey: before EQ   colour: after EQ", (int) a.getX() + 100, (int) a.getY() + 2, 300, 17, juce::Justification::centredLeft);
        auto body = a.withTrimmedTop (22).withTrimmedBottom (2);
        const float hh = (body.getHeight() - 6) * 0.62f;
        const auto rw = body.withHeight (hh), rc = body.withTrimmedTop (hh + 6);
        auto grid = [&] (juce::Rectangle<float> r) { g.setColour (theme::grid); for (double f : { 50.0, 100.0, 200.0, 500.0, 1000.0, 2000.0, 5000.0, 10000.0 }) g.drawVerticalLine ((int) xOf (f), r.getY(), r.getBottom()); };
        // L / R image per band, independent of level: the centre line is the balance (L up, R down, ±18 dB),
        // the opening around it is the spread (1 − coherence; narrowed toward the side the band leans to).
        // grey = before the EQ, colour = after
        {
            const auto r = rw; grid (r);
            auto y = [&] (float v) { return juce::jmap (juce::jlimit (-1.0f, 1.0f, v), -1.0f, 1.0f, r.getBottom() - 1.0f, r.getY() + 1.0f); };
            g.setFont (theme::mono (9.0f));
            for (float db : { -12.0f, -6.0f, 6.0f, 12.0f })
            {
                g.setColour (theme::grid.brighter (0.15f)); g.drawHorizontalLine ((int) y (db / 18.0f), r.getX(), r.getRight());
                g.setColour (theme::muted); g.drawText ((db > 0 ? "L+" : "R+") + juce::String ((int) std::abs (db)), (int) r.getRight() - 44, (int) y (db / 18.0f) - 7, 40, 14, juce::Justification::centredRight);
            }
            g.setColour (theme::zero); g.drawHorizontalLine ((int) y (0), r.getX(), r.getRight());
            g.setColour (theme::muted); g.drawText ("C", (int) r.getRight() - 44, (int) y (0) - 7, 40, 14, juce::Justification::centredRight);
            g.setFont (theme::ui (10.0f, true)); g.setColour (theme::side);
            g.drawText ("L", (int) r.getX() + 6, (int) r.getY() + 1, 20, 13, juce::Justification::centredLeft);
            g.drawText ("R", (int) r.getX() + 6, (int) r.getBottom() - 14, 20, 13, juce::Justification::centredLeft);
            g.setColour (theme::muted); g.setFont (theme::ui (9.0f));
            g.drawText ("line = balance, opening = spread (1 - coherence)", (int) r.getX() + 22, (int) r.getY() + 1, 300, 13, juce::Justification::centredLeft);
            for (int k = 0; k < 2; ++k)
            {
                if (fieldW[(size_t) k].size() != fieldF.size() || fieldP[(size_t) k].size() != fieldF.size()) continue;
                juce::Path top, bot, ctr, fill; bool open = false;
                for (size_t i = 0; i < fieldF.size(); ++i)
                {
                    const float sp = fieldW[(size_t) k][i], c = fieldP[(size_t) k][i];
                    if (! std::isfinite (sp) || ! std::isfinite (c)) { open = false; continue; }
                    const float h = sp * (1.0f - std::abs (c));
                    const float x = xOf (fieldF[i]), yt = y (c + h), yb = y (c - h), yc = y (c);
                    if (! open) { top.startNewSubPath (x, yt); bot.startNewSubPath (x, yb); ctr.startNewSubPath (x, yc); open = true; }
                    else { top.lineTo (x, yt); bot.lineTo (x, yb); ctr.lineTo (x, yc); }
                    if (k == 1) { const float w = std::max (1.0f, (float) (xOf (fieldF[std::min (i + 1, fieldF.size() - 1)]) - x)); fill.addRectangle (x, yt, w, std::max (1.0f, yb - yt)); }
                }
                if (k == 1)
                {
                    g.setColour (theme::side.withAlpha (0.25f)); g.fillPath (fill);
                    g.setColour (theme::side); g.strokePath (top, juce::PathStrokeType (1.3f)); g.strokePath (bot, juce::PathStrokeType (1.3f));
                    g.setColour (theme::text.withAlpha (0.8f)); g.strokePath (ctr, juce::PathStrokeType (1.2f));
                }
                else
                {
                    g.setColour (theme::muted.withAlpha (0.9f)); g.strokePath (top, juce::PathStrokeType (1.0f)); g.strokePath (bot, juce::PathStrokeType (1.0f));
                    juce::Path d; const float dash[] = { 3.0f, 3.0f }; juce::PathStrokeType (1.0f).createDashedStroke (d, ctr, dash, 2); g.fillPath (d);
                }
            }
        }
        auto panel = [&] (juce::Rectangle<float> r, const std::array<std::vector<float>, 2>& v, float lo, float hi, std::initializer_list<float> marks,
                          std::function<juce::String (float)> lab, juce::Colour col, float base, const char* title)
        {
            grid (r);
            auto y = [&] (float x) { return juce::jmap (juce::jlimit (lo, hi, x), lo, hi, r.getBottom(), r.getY()); };
            g.setFont (theme::mono (9.0f));
            for (float m : marks) { g.setColour (theme::grid.brighter (0.25f)); g.drawHorizontalLine ((int) y (m), r.getX(), r.getRight()); g.setColour (theme::muted); g.drawText (lab (m), (int) r.getRight() - 58, (int) y (m) - (m == hi ? 0 : m == lo ? 14 : 7), 56, 14, juce::Justification::centredRight); }
            g.setColour (col); g.setFont (theme::ui (10.0f, true)); g.drawText (title, (int) r.getX() + 6, (int) r.getY() + 1, 160, 13, juce::Justification::centredLeft);
            for (int k = 0; k < 2; ++k)
            {
                if (v[(size_t) k].size() != fieldF.size()) continue;
                juce::Path p, fill; bool open = false;
                for (size_t i = 0; i < fieldF.size(); ++i)
                {
                    const float val = v[(size_t) k][i];
                    const float x = xOf (fieldF[i]);
                    if (! std::isfinite (val)) { open = false; continue; }
                    if (! open) { p.startNewSubPath (x, y (val)); open = true; } else p.lineTo (x, y (val));
                    if (k == 1) { const float w = (float) (xOf (fieldF[std::min (i + 1, fieldF.size() - 1)]) - x); fill.addRectangle (x, std::min (y (val), y (base)), std::max (1.0f, w), std::abs (y (val) - y (base))); }
                }
                if (k == 1) { g.setColour (col.withAlpha (0.18f)); g.fillPath (fill); g.setColour (col); g.strokePath (p, juce::PathStrokeType (1.6f)); }
                else { g.setColour (theme::muted.withAlpha (0.9f)); g.strokePath (p, juce::PathStrokeType (1.2f)); }
            }
        };
        panel (rc, fieldC, -1.0f, 1.0f, { -1.0f, 0.0f, 1.0f }, [] (float m) { return m > 0 ? juce::String ("+1") : m < 0 ? juce::String ("-1") : juce::String ("0"); }, theme::accent, 1.0f, "CORRELATION");
    }
    void paintScope (juce::Graphics& g) const
    {
        if (ph.proc.earActive()) return;
        const auto a = scopeArea(), wv = scopeWave(), dt = scopeDet();
        g.setColour (theme::splitWell.withAlpha (0.7f)); g.fillRect (a);
        g.setColour (theme::line); g.drawHorizontalLine ((int) a.getY(), a.getX(), a.getRight());
        g.setFont (theme::ui (10.0f, true)); g.setColour (theme::muted);
        g.drawText ("T/B SCOPE  input", (int) a.getX() + 6, (int) a.getY() + 2, 140, 14, juce::Justification::centredLeft);
        g.setColour (ph.proc.detEq.active() ? theme::accent : theme::muted);
        g.drawText ("DETECTOR", (int) a.getX() + 6, (int) a.getY() + 33, 54, 12, juce::Justification::centredLeft);
        g.setFont (theme::ui (8.5f)); g.setColour (theme::muted);
        g.drawText ("not in output", (int) a.getX() + 6, (int) a.getY() + 45, 54, 12, juce::Justification::centredLeft);
        // legend
        int lx = (int) a.getX() + 150 + 3 * 74 + 66 + 62 + 10;
        for (int k = 0; k < 2; ++k)
        {
            g.setColour (k == 0 ? colT() : colB()); g.fillRoundedRectangle ((float) lx, a.getY() + 10, 9, 9, 2);
            g.setFont (theme::ui (10.0f, true)); g.drawText (k == 0 ? "Transient" : "Body", lx + 12, (int) a.getY() + 6, 60, 16, juce::Justification::centredLeft);
            lx += 74;
        }
        // waveform: dB level per column, attack share from the centre out, body beyond; the lower half mirrors it dimmer
        const int N = scCols;
        const float cy = wv.getCentreY(), H = wv.getHeight() * 0.5f, cw = wv.getWidth() / (float) N;
        for (int k = 0; k < N; ++k)
        {
            const int idx = (cW + k) % N;
            const float pk = cPk[(size_t) idx], t = juce::jlimit (0.0f, 1.0f, cT[(size_t) idx]);
            const float lv = juce::jlimit (0.0f, 1.0f, (juce::Decibels::gainToDecibels (pk, -60.0f) + 60.0f) / 60.0f);
            if (lv <= 0.0f) continue;
            const float x = wv.getX() + k * cw, ht = H * lv * t, hb = H * lv * (1.0f - t), w = std::max (1.0f, cw);
            g.setColour (colT()); g.fillRect (x, cy - ht, w, ht);
            g.setColour (colB()); g.fillRect (x, cy - ht - hb, w, hb);
            g.setColour (colT().withAlpha (0.4f)); g.fillRect (x, cy, w, ht);
            g.setColour (colB().withAlpha (0.4f)); g.fillRect (x, cy + ht, w, hb);
        }
        g.setColour (theme::zero); g.drawHorizontalLine ((int) cy, wv.getX(), wv.getRight());
        // detector strip: the Sens / Width band, then the detector's peak per column
        const float s = ph.value (pid::tbSens), wd = ph.value (pid::tbWidth);
        const float y0 = yDet (-s), y1 = yDet (wd - s);
        g.setColour (juce::Colour (0xffffd98a).withAlpha (0.16f)); g.fillRect (dt.getX(), y1, dt.getWidth(), y0 - y1);
        g.setColour (colB()); g.drawLine (dt.getX(), y0, dt.getRight(), y0, scDrag == 1 ? 2.2f : 1.4f);
        g.setColour (colT()); g.drawLine (dt.getX(), y1, dt.getRight(), y1, scDrag == 2 ? 2.2f : 1.4f);
        g.setColour (colB()); g.fillRoundedRectangle (dt.getRight() - 18, y0 - 4, 16, 8, 3);
        g.setColour (colT()); g.fillRoundedRectangle (dt.getRight() - 18, y1 - 4, 16, 8, 3);
        juce::Path d;
        for (int k = 0; k < N; ++k)
        {
            const int idx = (cW + k) % N;
            const float y = yDet (cD[(size_t) idx]), x = dt.getX() + k * cw;
            if (k == 0) d.startNewSubPath (x, y); else d.lineTo (x, y);
        }
        g.setColour (theme::text.withAlpha (0.8f)); g.strokePath (d, juce::PathStrokeType (1.1f));
        g.setColour (theme::muted); g.setFont (theme::ui (9.0f));
        if (scPaused)
        {
            g.setColour (theme::accent); g.setFont (theme::ui (11.0f, true));
            g.drawText ("PAUSED", (int) wv.getRight() - 70, (int) wv.getY() + 2, 64, 14, juce::Justification::centredRight);
        }
        g.setColour (theme::muted); g.setFont (theme::ui (9.0f));
        g.drawText (juce::String::fromUTF8 ("detector dB  (mask 0 \xe2\x86\x92 1 in the yellow band)"), (int) dt.getX() + 4, (int) dt.getY() - 2, 260, 12, juce::Justification::centredLeft);
    }
    float labelRow() const { return fullPlot().getBottom() + 2; }
    juce::String bandTag (int i, const eqcore::Band& b) const
    {
        juce::StringArray t;
        if (ph.proc.bandIsLin (i)) t.add ("Lin");
        if (b.ch == 1) t.add ("M"); else if (b.ch == 2) t.add ("S"); else if (b.ch == 3) t.add ("Lt"); else if (b.ch == 4) t.add ("Rt");
        if (ph.value (pid::band (i, "dyn")) > 0.5f) t.add ("D");
        if (std::abs (ph.value (pid::band (i, "tb"))) > 0.5f) t.add ("T/B");
        return t.joinIntoString (juce::String::fromUTF8 (" \xc2\xb7 "));
    }
    float xOf (double f) const { const auto p = plot(); return p.getX() + (float) (std::log (f / 20.0) / std::log (1000.0)) * p.getWidth(); }
    double fOf (float x) const { const auto p = plot(); return juce::jlimit (20.0, 20000.0, 20.0 * std::pow (1000.0, (x - p.getX()) / p.getWidth())); }
    float yOf (double db) const { const auto p = plot(); return juce::jmap ((float) db, -dbRange, dbRange, p.getBottom(), p.getY()); }
public:
    float dbRange = 20.0f;      // vertical scale in use, +-dB (follows userRange, widened automatically)
    float userRange = 20.0f;    // scale chosen in the menu (6 / 12 / 20 / 30); 20 = as before
    bool scaleHold = false;     // frozen while a dot is dragged, so the mapping under the mouse does not jump
    void setDbRange (float r) { userRange = r; updateAutoRange(); repaint(); }
    // magnitude scale only (the phase scale stays manual): widen to the smallest step that shows the
    // largest band gain with 1 dB headroom, and fall back to the chosen scale when gains come down again
    void updateAutoRange()
    {
        if (scaleHold) return;
        float need = 0.0f;
        for (int i = 0; i < eqcore::kBands; ++i)
        {
            if (ph.value (pid::band (i, "on")) < 0.5f) continue;
            const int t = (int) ph.value (pid::band (i, "type"));
            if (t == (int) eqcore::Type::LowCut || t == (int) eqcore::Type::HighCut || t == (int) eqcore::Type::Notch || t == (int) eqcore::Type::AllPass) continue;
            need = std::max (need, std::abs (ph.value (pid::band (i, "gain"))) + 1.0f);
        }
        float r = userRange;
        for (float st : { 6.0f, 12.0f, 20.0f, 30.0f }) if (st >= userRange && st >= need) { r = st; break; }
        if (need > 30.0f) r = 30.0f;
        dbRange = r;
    }
private:

    // Lab 2: bands 7 and 8 are extras — not drawn (nor clickable) in the graph until switched on
    static bool hiddenBand (int i, const eqcore::Band& b) { return i >= 6 && ! b.on; }
    juce::Point<float> nodePos (int i, const eqcore::Band& b) const
    {
        const bool noGain = b.type == eqcore::Type::LowCut || b.type == eqcore::Type::HighCut || b.type == eqcore::Type::Notch || b.type == eqcore::Type::AllPass;
        return { xOf (b.f), yOf (noGain ? 0.0 : juce::jlimit (-(double) dbRange, (double) dbRange, b.g)) };
        juce::ignoreUnused (i);
    }
    std::optional<std::pair<juce::Point<float>, juce::Point<float>>> handles (int s) const
    {
        const auto bands = ph.proc.currentBands(); const auto& b = bands[(size_t) s];
        if (! b.on || ! (b.type == eqcore::Type::Bell || b.type == eqcore::Type::Notch)) return std::nullopt;
        const double q = eqcore::effectiveQ (b, ph.proc.currentQMode());
        const double oct = 2.0 / std::log (2.0) * std::asinh (1.0 / (2.0 * q));
        const float y = yOf (b.type == eqcore::Type::Notch ? -6.0 : juce::jlimit (-(double) dbRange, (double) dbRange, b.g / 2.0));
        return std::make_pair (juce::Point<float> (xOf (std::max (20.0, b.f * std::pow (2.0, -oct / 2))), y),
                               juce::Point<float> (xOf (std::min (20000.0, b.f * std::pow (2.0, oct / 2))), y));
    }
    Pick pick (juce::Point<float> p) const
    {
        if (ph.value (pid::rsOn) > 0.5f && p.y > plot().getBottom() - 20 && p.y < plot().getBottom() + 2)
        {
            if (std::abs (p.x - xOf (ph.value (pid::rsLo))) < 10) return { -1, 5 };
            if (std::abs (p.x - xOf (ph.value (pid::rsHi))) < 10) return { -1, 6 };
        }
        if (ph.value (pid::satOn) > 0.5f && p.y > plot().getY() - 2 && p.y < plot().getY() + 20)
        {
            if (std::abs (p.x - xOf (ph.value (pid::satLo))) < 10) return { -1, 3 };
            if (std::abs (p.x - xOf (ph.value (pid::satHi))) < 10) return { -1, 4 };
        }
        if (auto h = handles (ph.selected()); h.has_value())
        {
            if (std::abs (h->first.x - p.x) < 9 && std::abs (h->first.y - p.y) < 14) return { ph.selected(), 1 };
            if (std::abs (h->second.x - p.x) < 9 && std::abs (h->second.y - p.y) < 14) return { ph.selected(), 2 };
        }
        const auto bands = ph.proc.currentBands(); int best = -1; float bd = 16.0f;
        for (int i = 0; i < eqcore::kBands; ++i) { if (hiddenBand (i, bands[(size_t) i])) continue; const float d = nodePos (i, bands[(size_t) i]).getDistanceFrom (p); if (d < bd) { bd = d; best = i; } }
        return { best, 0 };
    }
    void placePopup()
    {
        const int s = ph.selected();
        const auto bands = ph.proc.currentBands();
        const auto c = nodePos (s, bands[(size_t) s]);
        const bool ex = (bool) ph.proc.uiState.getProperty ("popExpand", false);
        auto& pp = popup;
        const int w = pp.idealWidth(), h = pp.idealHeight() + (ex ? popupFull.idealHeight() + 2 : 0);   // the group: compact + extension
        auto& ui = ph.proc.uiState;
        if ((bool) ui.getProperty ("popPinned", false))       // parked where the user dragged it
        {
            pp.setBounds (juce::jlimit (4, getWidth() - w - 4, (int) ui.getProperty ("popX", 40)), juce::jlimit (4, getHeight() - h - 4, (int) ui.getProperty ("popY", 40)), w, pp.idealHeight());
            popupFull.setBounds (pp.getX(), pp.getBottom() + 2, w, popupFull.idealHeight());
            return;
        }
        int x = (int) c.x + 22, y = (int) c.y - h - 14;
        if (x + w > getWidth() - 8) x = (int) c.x - w - 22;
        if (y < 32) y = (int) c.y + 22;
        if (y + h > getHeight() - 20) y = getHeight() - 20 - h;
        pp.setBounds (juce::jlimit (8, getWidth() - w - 8, x), juce::jmax (30, y), w, pp.idealHeight());
        popupFull.setBounds (pp.getX(), pp.getBottom() + 2, w, popupFull.idealHeight());
    }

    ParamHelp& ph;
    BandPopup popup, popupFull;
    juce::TextButton panelBtn;
    juce::TextButton phaseBtn, overlayBtn;     // Phase ⇄ GD, Overlay ⇄ Split
    std::vector<double> fr; eqcore::CurvesV2 cur, curS; eqcore::Curves ghost; std::vector<double> selCurve, liveMag; eqcore::CurvesV2 liveCur;
    struct DynArea { std::vector<double> lo, hi, st, u; size_t peak = 0; }; std::array<DynArea, eqcore::kBands> dynArea;   // dynamic EQ: where each band can move
    bool anyMS = false, anyLR = false, lastSplit = false, altSolo = false;
    std::vector<float> spectrum, spectrumB; juce::String lastSig;
    juce::ComboBox anaA, anaB;
    juce::TextButton pairBtn;
    // pair index → (A source, B id). Sources: 0 Out, 1 Mid, 2 Side, 3 Trans, 4 Body, 5 Input, 6 SC, 7 link. B id = source + 1
    static juce::String pairName (int i)
    {
        static const char* n[] = { "In / Out", "In / Mid", "In / Side", "In / Trans", "In / Body", "Mid / Side", "Trans / Body", "In / SC", "Out / SC", "Out / Other", "Custom" };
        return n[juce::jlimit (0, 10, i)];
    }
    static const int* pairA() { static const int A[] = { 5, 5, 5, 5, 5, 1, 3, 5, 0, 0 }; return A; }
    static const int* pairB() { static const int B[] = { 1, 2, 3, 4, 5, 3, 5, 7, 7, 8 }; return B; }
    static int matchPair (int a, int b) { for (int i = 0; i < 10; ++i) if (pairA()[i] == a && pairB()[i] == b) return i; return 10; }
    int lastSoloView = 0, savedB = 1, savedPair = 0, autoB = -1;
    void lowerToggle (int view)
    {
        auto& ui = ph.proc.uiState;
        const bool on = (bool) ui.getProperty ("scope", false) && (int) ui.getProperty ("lowerView", 0) == view;
        if (on) ui.setProperty ("scope", false, nullptr);
        else { ui.setProperty ("scope", true, nullptr); ui.setProperty ("lowerView", view, nullptr); }
        resized();
    }
    void setPair (int i)
    {
        const int* A = pairA(); const int* B = pairB();
        auto& ui = ph.proc.uiState; ui.setProperty ("anPair", i, nullptr);
        if (i < 10) { ui.setProperty ("anA", A[i], nullptr); ui.setProperty ("anB", B[i], nullptr); }
    }
    juce::StringArray linkNames; juce::uint32 lastLinkScan = 0;
    // other EQ Anatomy instances in this process, listed in analyzer B as "B ← name" (rescanned twice a second)
    void refreshLinks()
    {
        const auto now = juce::Time::getMillisecondCounter();
        if (now - lastLinkScan < 500 && lastLinkScan != 0) return;
        lastLinkScan = now;
        juce::StringArray names;
        for (auto& t : LinkRegistry::list()) if (t != ph.proc.linkTap) names.addIfNotAlreadyThere (t->name());
        if (names == linkNames) return;
        linkNames = names;
        const int sel = anaB.getSelectedId();
        anaB.clear (juce::dontSendNotification);
        static const char* nm[] = { "Stereo", "Mid", "Side", "Trans", "Body", "Input", "SC" };
        anaB.addItem ("B off", 1);
        for (int i = 0; i < 7; ++i) anaB.addItem (juce::String ("B ") + nm[i], i + 2);
        if (linkNames.size() > 0) anaB.addSectionHeading (ph.tr ("他の EQ Anatomy の出力", "Other EQ Anatomy outputs"));
        for (int i = 0; i < linkNames.size(); ++i) anaB.addItem (juce::String::fromUTF8 ("B \xe2\x86\x90 ") + linkNames[i], 101 + i);
        if (sel > 0 && sel <= 8) anaB.setSelectedId (sel, juce::dontSendNotification);
    }
    juce::TextButton scopeBtn; DragValue scSens, scWidth, scAtt;
    juce::ComboBox scSpan; juce::TextButton scPause;
    DragValue scLc, scE1f, scE1g, scE1q, scE2f, scE2g, scE2q; Seg scMethod;
    DetectorEQ sdeq;
    static constexpr int scCols = 1024;
    std::array<float, scCols> cPk {}, cT {}, cD {}; int cW = 0;            // scope columns, oldest at cW
    TransientDetector sdet; juce::uint64 sRead = 0, scEnableW = 0, frozenEnd = 0;
    double sAcc = 0, sAccT = 0, sPk = 0, sDmax = -100; int sCnt = 0;
    bool scPaused = false; std::vector<float> frozen; juce::String scSig;
    juce::TextButton tbTab, fdTab;
    bool lastField = false;
public:
    // stereo field per band, filled by the editor: [0] before the EQ, [1] after. NaN = too quiet to show
    std::vector<double> fieldF; std::array<std::vector<float>, 2> fieldW, fieldC, fieldP;   // W = spread (1 - coherence), P = balance (+1 = L 18 dB louder), C = correlation
    bool fieldWanted() const { return lastScope && lastField && ! ph.proc.earActive(); }
private:
    bool lastScope = false, autoAna = false; int srcA = 0, srcB = -1, scDrag = 0; float scStartY = 0, scStartSens = 0, scStartWidth = 6;
    Pick drag; double startQ = 1; float startY = 0, startF = 1000, startG = 0; juce::Point<float> downPos; int axis = 0; juce::uint32 lastRightClick = 0;
};

// =============================================================================
//  Band chips (2 × 3): ● toggles a band, the rest selects it
// =============================================================================
class BandChips : public juce::Component
{
public:
    explicit BandChips (ParamHelp& h) : ph (h)
    {
        setHelp (*this, "BANDS", "クリック：選択して浮き窓を表示（選択中なら表示の切り替え）／ダブルクリック・色の丸：オン・オフ／周波数やゲインの数字：上下ドラッグで変更（Shift で微調整）。右端の印は L＝Lin、M・S＝チャンネル、D＝ダイナミック。キー 1〜8 でも選べる",
                 "Click: select and show the panel (on the selected band: show/hide) / double-click or the dot: on/off / drag the frequency or gain number up/down to change it (Shift for fine). Tags: L = Lin, M/S = channel, D = dynamic. Keys 1–8 select too");
    }
    void paint (juce::Graphics& g) override
    {
        const int s = ph.selected();
        for (int i = 0; i < eqcore::kBands; ++i)
        {
            auto r = cell (i).toFloat();
            const bool on = ph.value (pid::band (i, "on")) > 0.5f;
            g.setColour (theme::panel); g.fillRoundedRectangle (r, 5.0f);
            g.setColour (i == s ? theme::bandColour (i) : theme::line); g.drawRoundedRectangle (r.reduced (0.5f), 5.0f, i == s ? 1.5f : 1.0f);
            auto dot = juce::Rectangle<float> (r.getX() + 8, r.getCentreY() - 5, 10, 10);
            g.setColour (theme::bandColour (i));
            if (on) g.fillEllipse (dot); else g.drawEllipse (dot, 1.5f);
            g.setColour (on ? theme::text : theme::muted); g.setFont (theme::mono (11.0f, true));
            g.drawText (juce::String (i + 1), (int) r.getX() + 22, (int) r.getY(), 12, (int) r.getHeight(), juce::Justification::centredLeft);
            const int t = (int) ph.value (pid::band (i, "type"));
            juce::String txt = juce::String (typeAbbr (t)) + " " + fmtHz (ph.value (pid::band (i, "freq")));
            if (t <= 2) txt << " " << fmtDb (ph.value (pid::band (i, "gain")));
            juce::String tag;
            if (on)
            {
                if (ph.proc.bandIsLin (i)) tag << "L";
                const int ch = (int) ph.value (pid::band (i, "ch")); if (ch == 1) tag << "M"; else if (ch == 2) tag << "S"; else if (ch == 3) tag << "Lt"; else if (ch == 4) tag << "Rt";
                if (ph.value (pid::band (i, "dyn")) > 0.5f) tag << "D";
            }
            g.setFont (theme::mono (10.0f)); g.setColour (on ? theme::text : theme::muted);
            g.drawText (on ? txt : juce::String ("off  ") + fmtHz (ph.value (pid::band (i, "freq"))),
                        (int) r.getX() + 36, (int) r.getY(), (int) r.getWidth() - 40 - tag.length() * 6, (int) r.getHeight(), juce::Justification::centredLeft, true);
            if (tag.isNotEmpty())
            {
                g.setColour (theme::bandColour (i)); g.setFont (theme::mono (8.5f, true));
                g.drawText (tag, (int) r.getRight() - 30, (int) r.getY(), 26, (int) r.getHeight(), juce::Justification::centredRight);
            }
        }
    }
    void mouseDown (const juce::MouseEvent& e) override
    {
        dragBand = -1;
        for (int i = 0; i < eqcore::kBands; ++i)
            if (cell (i).contains (e.getPosition()))
            {
                ph.proc.undo.beginNewTransaction();
                if (e.x < cell (i).getX() + 22) { toggleOn (i); ph.select (i); repaint(); return; }
                if (e.getNumberOfClicks() >= 2) return;            // the double-click handles it
                // on the frequency or gain number: a click still selects (on mouse-up), a drag changes the value
                const int k = e.mods.isPopupMenu() ? 0 : zoneAt (i, e.x);
                if (k > 0) { dragBand = i; dragKind = k; dragged = false; lastY = e.getScreenPosition().toFloat().y; return; }
                clickCell (i); return;
            }
    }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (dragBand < 0) return;
        auto* p = ph.get (pid::band (dragBand, dragKind == 1 ? "freq" : "gain")); if (p == nullptr) return;
        if (! dragged)
        {
            if (e.getDistanceFromDragStart() < 3) return;
            dragged = true; p->beginChangeGesture(); startNorm = p->getValue(); ph.select (dragBand);
            setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
        }
        // like DragValue: screen coordinates, 200 px = full range, Shift = fine
        const float y = e.getScreenPosition().toFloat().y, sens = e.mods.isShiftDown() ? 0.0008f : 0.005f;
        startNorm = juce::jlimit (0.0f, 1.0f, startNorm + (lastY - y) * sens); lastY = y;
        p->setValueNotifyingHost (startNorm); repaint();
    }
    void mouseUp (const juce::MouseEvent&) override
    {
        if (dragBand < 0) return;
        const int i = dragBand; dragBand = -1;
        if (dragged)
        {
            if (auto* p = ph.get (pid::band (i, dragKind == 1 ? "freq" : "gain"))) p->endChangeGesture();
            dragged = false; setMouseCursor (juce::MouseCursor::NormalCursor); repaint();
        }
        else clickCell (i);                                         // no drag: it was a click on the card
    }
    void mouseMove (const juce::MouseEvent& e) override
    {
        int k = 0;
        for (int i = 0; i < eqcore::kBands && k == 0; ++i) if (cell (i).contains (e.getPosition()) && e.x >= cell (i).getX() + 22) k = zoneAt (i, e.x);
        setMouseCursor (k > 0 ? juce::MouseCursor::UpDownResizeCursor : juce::MouseCursor::NormalCursor);
    }
    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        for (int i = 0; i < eqcore::kBands; ++i)
            if (cell (i).contains (e.getPosition()) && e.x >= cell (i).getX() + 22)
            {
                auto& ui = ph.proc.uiState;
                if (toggledPopup) ui.setProperty ("popup", ! (bool) ui.getProperty ("popup", true), nullptr);   // undo the first click's toggle
                toggledPopup = false;
                toggleOn (i); repaint(); return;
            }
    }
private:
    void clickCell (int i)
    {
        auto& ui = ph.proc.uiState;
        toggledPopup = false;
        if (ph.selected() != i) { ph.select (i); ui.setProperty ("popup", true, nullptr); }
        else { ui.setProperty ("popup", ! (bool) ui.getProperty ("popup", true), nullptr); toggledPopup = true; }
        if (ph.value (pid::band (i, "on")) < 0.5f) ui.setProperty ("keepOff", i, nullptr);   // show the panel for an off band too
        repaint();
    }
    static const char* typeAbbr (int t) { static const char* ab[] = { "Bell", "LS", "HS", "LC", "HC", "Notch", "AP" }; return ab[juce::jlimit (0, 6, t)]; }
    // which number of card i is under x: 1 = frequency, 2 = gain, 0 = none (measured from the same text that paint() draws)
    int zoneAt (int i, int x) const
    {
        const auto r = cell (i);
        const bool on = ph.value (pid::band (i, "on")) > 0.5f;
        const int t = (int) ph.value (pid::band (i, "type"));
        const juce::Font f = theme::mono (10.0f);
        const juce::String prefix = on ? juce::String (typeAbbr (t)) + " " : juce::String ("off  "), fs = fmtHz (ph.value (pid::band (i, "freq")));
        const float fx0 = (float) r.getX() + 36.0f + juce::GlyphArrangement::getStringWidth (f, prefix), fx1 = fx0 + juce::GlyphArrangement::getStringWidth (f, fs);
        if ((float) x >= fx0 - 2.0f && (float) x <= fx1 + 2.0f) return 1;
        if (on && t <= 2)
        {
            const float gx0 = fx1 + juce::GlyphArrangement::getStringWidth (f, " "), gx1 = gx0 + juce::GlyphArrangement::getStringWidth (f, fmtDb (ph.value (pid::band (i, "gain"))));
            if ((float) x >= gx0 - 2.0f && (float) x <= gx1 + 2.0f) return 2;
        }
        return 0;
    }
    void toggleOn (int i)
    {
        const bool on = ph.value (pid::band (i, "on")) > 0.5f;
        if (on) ph.proc.uiState.setProperty ("keepOff", i, nullptr);
        ph.setGesture (pid::band (i, "on"), on ? 0.0f : 1.0f);
    }
    juce::Rectangle<int> cell (int i) const
    {
        const int cw = (getWidth() - 5) / 2, ch = (getHeight() - 10) / 3;
        return { (i % 2) * (cw + 5), (i / 2) * (ch + 5), cw, ch };
    }
    ParamHelp& ph; bool toggledPopup = false;
    int dragBand = -1, dragKind = 0; bool dragged = false; float startNorm = 0, lastY = 0;
};

// =============================================================================
//  Impulse response (dB envelope, ±40 ms around the main tap)
// =============================================================================
class IRView : public juce::Component
{
public:
    explicit IRView (ParamHelp& h) : ph (h) { setHelp (*this, "IMPULSE", "インパルスレスポンス（±40 ms）。0 ms より左の赤い部分がプリリンギング＝音より前に出る成分", "Impulse response (±40 ms). Red, left of 0 ms, is pre-ringing: output before the sound"); }
    void refresh()
    {
        if (ph.proc.displayVersion() == version) return;
        ph.proc.copyDisplayIR (ir); version = ir.version; repaint();
    }
    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (theme::panel); g.fillRoundedRectangle (r, 6.0f);
        g.setColour (theme::line); g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.0f);
        g.setFont (theme::ui (9.5f)); g.setColour (theme::muted);
        g.drawText (juce::String ("IMPULSE"), 8, 4, 80, 14, juce::Justification::centredLeft);
        g.setColour (theme::pre);
        g.drawText (ir.preDb > -200 ? "PRE " + juce::String ((int) std::round (ir.preMs)) + " ms" : juce::String ("PRE none"),
                    getWidth() - 98, 4, 90, 14, juce::Justification::centredRight);
        auto w = r.reduced (8).withTrimmedTop (16).withTrimmedBottom (12);
        g.setColour (theme::well); g.fillRoundedRectangle (w, 4.0f);
        g.setColour (theme::pre.withAlpha (0.12f)); g.fillRect (w.withWidth (w.getWidth() / 2));
        g.setFont (theme::mono (9.0f)); g.setColour (theme::muted);
        g.drawText (juce::String::fromUTF8 ("\xe2\x88\x92") + "40", (int) w.getX(), (int) w.getBottom(), 40, 12, juce::Justification::centredLeft);
        g.drawText ("0 ms", (int) w.getCentreX() - 20, (int) w.getBottom(), 40, 12, juce::Justification::centred);
        g.drawText ("+40", (int) w.getRight() - 40, (int) w.getBottom(), 40, 12, juce::Justification::centredRight);
        if (ir.h.empty()) return;
        float pk = 1e-9f; for (auto v : ir.h) pk = std::max (pk, std::abs (v));
        const int span = (int) (0.04 * ir.fs), cols = (int) w.getWidth();
        juce::Path pre, post; bool fp = true, fq = true;
        for (int x = 0; x < cols; ++x)
        {
            const int a = ir.latency - span + (int) ((double) x / cols * 2 * span), b = ir.latency - span + (int) ((double) (x + 1) / cols * 2 * span);
            float m = 0; for (int i = a; i < std::max (a + 1, b); ++i) if (i >= 0 && i < (int) ir.h.size()) m = std::max (m, std::abs (ir.h[(size_t) i]));
            const float db = juce::jmax (-100.0f, 20.0f * std::log10 (m / pk + 1e-12f));
            const float y = juce::jmap (db, -100.0f, 0.0f, w.getBottom() - 1, w.getY() + 2), px = w.getX() + (float) x;
            auto& p = a < ir.latency - 12 ? pre : post; bool& f = a < ir.latency - 12 ? fp : fq;
            if (f) { p.startNewSubPath (px, y); f = false; } else p.lineTo (px, y);
        }
        g.setColour (theme::pre); g.strokePath (pre, juce::PathStrokeType (1.3f));
        g.setColour (theme::accent); g.strokePath (post, juce::PathStrokeType (1.3f));
    }
private:
    ParamHelp& ph; DisplayIR ir; int version = -1;
};


// =============================================================================
//  RES: the resonance suppressor's controls (shares the lower-left slot with SAT; tabs above)
// =============================================================================
class ResStrip : public juce::Component
{
public:
    explicit ResStrip (ParamHelp& h) : ph (h), lo (h, "LOW"), hi (h, "HIGH"), depth (h, "DEPTH"), thr (h, "THRESH"), sel (h, "SELECT"), att (h, "ATT"), rel (h, "REL")
    {
        onBtn.setClickingTogglesState (false); deltaBtn.setClickingTogglesState (false);
        onBtn.onClick = [this] { ph.toggleGesture (pid::rsOn); }; markReset (onBtn);
        deltaBtn.onClick = [this] { ph.toggleGesture (pid::rsDelta); }; markReset (deltaBtn);
        onBtn.setColour (juce::TextButton::buttonOnColourId, theme::res); deltaBtn.setColour (juce::TextButton::buttonOnColourId, theme::res);
        lo.setParam (pid::rsLo); hi.setParam (pid::rsHi); depth.setParam (pid::rsDepth); thr.setParam (pid::rsThr); sel.setParam (pid::rsSel); att.setParam (pid::rsAtt); rel.setParam (pid::rsRel);
        for (auto* d : { &lo, &hi }) { d->format = [] (float v) { return fmtHz (v) + " Hz"; }; d->parse = parseHz; }
        depth.format = thr.format = [] (float v) { return juce::String (v, 1) + " dB"; };
        sel.format = [] (float v) { return juce::String ((int) std::round (v)) + " %"; };
        att.format = rel.format = [] (float v) { return v < 10 ? juce::String (v, 1) : juce::String ((int) std::round (v)); };
        for (auto* d : { &lo, &hi, &depth, &thr, &sel, &att, &rel }) { d->compact = true; d->valueColour = theme::res; addAndMakeVisible (*d); }
        addAndMakeVisible (onBtn); addAndMakeVisible (deltaBtn);
        setHelp (onBtn, "RES", "レゾナンス・サプレッサーのオン・オフ。EQ の後ろ（SAT の前）で、周りより突き出た帯域だけを、その瞬間だけ下げる。固定の EQ では追いきれない、音程や声で動く共振に効く。注意：オンの間はレイテンシーが 1024 サンプル（48 kHz で約 21 ms）増える",
                 "Resonance suppressor on/off. After the EQ (before SAT) it turns down only the frequencies that stick out of their surroundings, only while they do: for resonances that move with pitch or voice, which a static EQ cannot follow. Careful: adds 1024 samples of latency (about 21 ms at 48 kHz) while on");
        setHelp (deltaBtn, "RES Δ", "サプレッサーが削っている成分だけを聴く。何を削っているかを耳で確かめ、削りすぎ（音色の芯まで削っていないか）を判断する", "Hear only what the suppressor removes: check by ear what it takes, and whether it eats into the core of the tone");
        setHelp (lo, "LOW", "適応帯域の下端。ここより下は触らない（1/6 オクターブでなだらかに切れる）。グラフ下の RES タブでも動かせる。例：ボーカルは 200〜300 Hz、ギターは 150 Hz 前後", "Low edge of the adaptive range; nothing below is touched (1/6-octave soft edge). Also the RES tab at the bottom of the graph. E.g. 200–300 Hz for vocals, about 150 Hz for guitar");
        setHelp (hi, "HIGH", "適応帯域の上端。歯擦音まで抑えたい時は 10〜12 kHz、空気感を残したい時は 6〜8 kHz", "High edge of the adaptive range: 10–12 kHz to reach sibilance, 6–8 kHz to keep the air");
        setHelp (depth, "DEPTH", "1 つの周波数から下げる最大量（dB）。3〜6 dB で自然、9 dB を超えると音色が変わり始める", "Most it takes from one frequency (dB). 3–6 dB is natural; past about 9 dB the tone starts to change");
        setHelp (thr, "THRESHOLD", "周り（SELECT で決まる幅の平均）よりどれだけ突き出たら削るか（dB）。小さいほど多くの帯域を削る。グラフのピンクの面が削っている量", "How far above its surroundings (the average over the SELECT width) a frequency must stick out before it is taken down (dB). Lower takes more. The pink area on the graph is the reduction");
        setHelp (sel, "SELECT", "選択度。何を「共振」とみなすか。0 %：2 オクターブの平均と比べる（なだらかな山も削る＝トーン補正寄り）／100 %：1/12 オクターブと比べる（針のように鋭いピークだけ）。既定 50 %（約 0.4 オクターブ）", "Selectivity: what counts as a resonance. 0 %: compared with a 2-octave average (broad bumps too, closer to tone shaping) / 100 %: with 1/12 octave (needle-sharp peaks only). Default 50 % (about 0.4 octave)");
        setHelp (att, "ATTACK", "削り始める速さ（ms）。短いほどアタックの頭の共振も捕まえるが、トランジェントも丸くなる", "How fast it starts to cut (ms). Shorter catches the resonance at the very front, but also softens the transient");
        setHelp (rel, "RELEASE", "戻る速さ（ms）。短いと音程の変化に追従しやすいが、揺れ（ポンピング）が聴こえやすい。長いと滑らかだが、別の音まで削り続ける", "How fast it lets go (ms). Short follows changing notes but can flutter; long is smooth but keeps cutting into the next note");
    }
    void refresh()
    {
        const bool on = ph.value (pid::rsOn) > 0.5f;
        onBtn.setButtonText ("RES"); onBtn.setToggleState (on, juce::dontSendNotification);
        deltaBtn.setButtonText (juce::String::fromUTF8 ("Res \xce\x94")); deltaBtn.setToggleState (ph.value (pid::rsDelta) > 0.5f, juce::dontSendNotification);
        for (auto* c : { (juce::Component*) &lo, (juce::Component*) &hi, (juce::Component*) &depth, (juce::Component*) &thr, (juce::Component*) &sel, (juce::Component*) &att, (juce::Component*) &rel, (juce::Component*) &deltaBtn })
            c->setAlpha (on ? 1.0f : 0.45f);
        float m = 0; for (auto& d : ph.proc.rs.dispDb) m = std::max (m, d.load());
        peak = on ? std::max (m, peak * 0.9f) : 0.0f;
        for (auto* d : { &lo, &hi, &depth, &thr, &sel, &att, &rel }) d->repaint();
        repaint();
    }
    void resized() override
    {
        const int W = getWidth() - 16, cw = (W - 4) / 2, c3 = (W - 8) / 3;
        onBtn.setBounds (8, 28, 40, 22); deltaBtn.setBounds (52, 28, 46, 22);
        readout = { 102, 28, W - 94, 22 };
        lo.setBounds (8, 54, cw, 28); hi.setBounds (12 + cw, 54, cw, 28);
        depth.setBounds (8, 84, cw, 28); thr.setBounds (12 + cw, 84, cw, 28);
        sel.setBounds (8, 114, c3, 28); att.setBounds (12 + c3, 114, c3, 28); rel.setBounds (16 + 2 * c3, 114, W - 8 - 2 * c3, 28);
    }
    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (theme::panel); g.fillRoundedRectangle (r, 6.0f);
        g.setColour (ph.value (pid::rsOn) > 0.5f ? theme::res.withAlpha (0.53f) : theme::line); g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.0f);
        g.setColour (theme::muted); g.setFont (theme::ui (8.5f)); g.drawText ("MAX", readout.withWidth (24), juce::Justification::centredLeft);
        g.setColour (peak > 0.05f ? theme::res : theme::faint); g.setFont (theme::mono (11.0f, true));
        g.drawText (peak > 0.05f ? juce::String::fromUTF8 ("\xe2\x88\x92") + juce::String (peak, 1) + " dB" : juce::String ("0.0 dB"), readout, juce::Justification::centredRight);
    }
private:
    ParamHelp& ph;
    juce::TextButton onBtn, deltaBtn;
    DragValue lo, hi, depth, thr, sel, att, rel;
    juce::Rectangle<int> readout; float peak = 0;
};

// =============================================================================
//  OUT: drag up / down, double-click resets
// =============================================================================
class OutputDrag : public juce::Component
{
public:
    // Lab 2: OUT and Auto gain in one cell. Left: drag the output gain. Right: Auto gain on/off with its current amount
    explicit OutputDrag (ParamHelp& h) : ph (h)
    {
        markReset (*this);
        setHelp (*this, "OUT / AUTO GAIN", "左＝出力ゲイン（上下ドラッグ、Shift で微調整、ダブルクリックで 0 dB）。右＝Auto gain のオン／オフ（EQ による音量変化を打ち消して音色だけを比べる。オンの時は打ち消している量を表示）",
                 "Left: output gain (drag up/down, Shift for fine, double-click for 0 dB). Right: Auto gain on/off (cancels the EQ's level change so you compare tone only; shows the amount while on)");
    }
    juce::Rectangle<int> agArea() const { return getLocalBounds().withTrimmedLeft (getWidth() / 2 + 1); }
    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (theme::panel); g.fillRoundedRectangle (r, 4.0f);
        const bool ag = ph.value (pid::autoGain) > 0.5f;
        const auto a = agArea().toFloat();
        if (ag) { g.setColour (theme::accent); g.fillRoundedRectangle (a.reduced (1.0f), 3.0f); }
        g.setColour (theme::line); g.drawRoundedRectangle (r.reduced (0.5f), 4.0f, 1.0f);
        g.drawVerticalLine (getWidth() / 2, 3.0f, (float) getHeight() - 3.0f);
        const int hw = getWidth() / 2;
        g.setColour (theme::muted); g.setFont (theme::ui (9.0f)); g.drawText ("OUT", 6, 0, 30, getHeight(), juce::Justification::centredLeft);
        g.setColour (theme::text); g.setFont (theme::mono (10.5f, true));
        g.drawText (fmtDb (ph.value (pid::output)), 0, 0, hw - 5, getHeight(), juce::Justification::centredRight);
        g.setColour (ag ? theme::onAccent : theme::text); g.setFont (theme::ui (9.5f, ag));
        g.drawText (ag ? "AG " + fmtDb (ph.proc.currentAutoGainDb()) : juce::String ("Auto gain"), agArea(), juce::Justification::centred);
    }
    void mouseDown (const juce::MouseEvent& e) override
    {
        onAg = agArea().contains (e.getPosition());
        if (onAg) { if (e.mods.isAltDown()) ph.resetDefault (pid::autoGain); else ph.setGesture (pid::autoGain, ph.value (pid::autoGain) > 0.5f ? 0.0f : 1.0f); repaint(); return; }
        resetDone = e.mods.isAltDown();
        if (resetDone) { ph.resetDefault (pid::output); repaint(); return; }
        start = ph.value (pid::output); ph.begin (pid::output);
    }
    void mouseDrag (const juce::MouseEvent& e) override { if (resetDone || onAg) return; ph.set (pid::output, juce::jlimit (-18.0f, 18.0f, start - e.getDistanceFromDragStartY() * (e.mods.isShiftDown() ? 0.02f : 0.1f))); repaint(); }
    void mouseUp (const juce::MouseEvent&) override { if (onAg) { onAg = false; return; } if (resetDone) { resetDone = false; return; } ph.end (pid::output); }
    void mouseDoubleClick (const juce::MouseEvent& e) override { if (! agArea().contains (e.getPosition())) { ph.setGesture (pid::output, 0.0f); repaint(); } }
private:
    ParamHelp& ph; float start = 0; bool resetDone = false, onAg = false;
};

// =============================================================================
//  SOLO M | S | T | B: listen to one part of the finished output. Not a parameter and never saved.
//  Click selects, click again clears. Dimmed while EAR / Bypass / Delta / Pre-ring solo / band solo take priority.
// =============================================================================
class SoloView : public juce::Component
{
public:
    explicit SoloView (ParamHelp& h) : ph (h)
    {
        setHelp (*this, "SOLO  M | S | T | B | P",
                 "P＝プリリンギングソロ（リニア位相で音より前に出る成分だけ。Lin のバンドがある時に働く）／出力のどの部分だけを聴くか。M＝Mid（中央の音。左右同じ）／S＝Side（左右に広がる成分。左右が逆相なので、モノラルにすると消える）／T＝検出器がアタックと見る部分（出力×マスク）／B＝胴と見る部分（出力×(1−マスク)）。もう一度押すと解除。切り替えは約 15 ms のクロスフェード。優先順位：EAR ＞ Bypass ＞ Delta ＞ Pre-ring solo ＞ バンドソロ ＞ この SOLO（上位がオンの間は働かず、灰色になる）。保存されない（開き直すとオフ）。スペアナはこの SOLO の影響を受けない。注意：T／B は検出器の見立てで、厳密な分離ではない。T と B を足しても元の音になるのは各瞬間のマスクが 0〜1 のときだけで、聞こえ方は目安",
                 "Which part of the finished output you hear. M = Mid (the centre, identical in both ears) / S = Side (the widening part: left and right are opposite in polarity, so it vanishes in mono) / T = what the detector hears as attack (output × mask) / B = as body (output × (1 − mask)). Click again to clear. Switching cross-fades over about 15 ms. Priority: EAR > Bypass > Delta > Pre-ring solo > band solo > this SOLO (it is ignored and dimmed while any of those is on). Not saved: off after reopening. The analyzer is not affected by it. Careful: T and B are the detector's guess, not a strict separation");
    }
    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (theme::panel); g.fillRoundedRectangle (r, 4.0f);
        g.setColour (theme::line); g.drawRoundedRectangle (r.reduced (0.5f), 4.0f, 1.0f);
        const int sel = ph.proc.soloView.load();
        const bool blocked = ph.proc.soloViewBlocked();
        g.setColour (blocked ? theme::faint : theme::muted); g.setFont (theme::ui (9.5f));
        g.setFont (theme::ui (8.5f, true));
        g.drawText ("SOLO", 5, 0, (int) labelW - 4, getHeight(), juce::Justification::centredLeft);
        static const char* t[] = { "M", "S", "T", "B", "P" };       // P = pre-ring solo (needs a Lin band)
        const float w = cellW();
        for (int i = 0; i < 5; ++i)
        {
            auto c = juce::Rectangle<float> (r.getX() + labelW + (float) i * w, r.getY() + 3.0f, w, r.getHeight() - 6.0f).reduced (1.0f, 0.0f);
            const bool on = i == 4 ? ph.value (pid::preSolo) > 0.5f : sel == i + 1;
            if (on) { g.setColour ((i == 4 ? theme::pre : theme::accent).withAlpha (blocked && i < 4 ? 0.3f : 1.0f)); g.fillRoundedRectangle (c, 3.0f); }
            g.setColour (on ? theme::onAccent : (blocked ? theme::faint : theme::muted)); g.setFont (theme::mono (11.0f, true));
            g.drawText (t[i], c, juce::Justification::centred);
        }
    }
    void mouseDown (const juce::MouseEvent& e) override
    {
        const int i = (int) std::floor (((float) e.x - labelW) / cellW());
        if (i < 0 || i > 4 || e.mods.isPopupMenu()) return;
        if (i == 4) { ph.toggleGesture (pid::preSolo); repaint(); return; }
        ph.proc.soloView = ph.proc.soloView.load() == i + 1 ? 0 : i + 1;
        repaint();
    }
private:
    static constexpr float labelW = 32.0f;
    float cellW() const { return ((float) getWidth() - labelW - 3.0f) / 5.0f; }
    ParamHelp& ph;
};

// =============================================================================
//  Lesson card (Learn on) / mode notes (Learn off)
// =============================================================================
class LessonCard : public juce::Component
{
public:
    explicit LessonCard (ParamHelp& h) : ph (h)
    {
        for (auto* b : { &prev, &load, &next }) addAndMakeVisible (*b);
        load.setColour (juce::TextButton::buttonColourId, theme::accent);
        load.setColour (juce::TextButton::textColourOffId, theme::onAccent);
        prev.onClick = [this] { step (-1); };
        next.onClick = [this] { step (1); };
        load.onClick = [this] { ph.proc.applyLesson (index()); };
    }
    int index() const { return juce::jlimit (0, (int) lessons().size() - 1, (int) ph.proc.uiState.getProperty ("lesson", 0)); }
    void step (int d) { ph.proc.uiState.setProperty ("lesson", juce::jlimit (0, (int) lessons().size() - 1, index() + d), nullptr); repaint(); }
    void refreshTexts()
    {
        prev.setButtonText (ph.tr ("前へ", "Back")); next.setButtonText (ph.tr ("次へ", "Next"));
        load.setButtonText (ph.tr ("この設定を読み込む", "Load this setting"));
        const bool learn = (bool) ph.proc.uiState.getProperty ("learn", true);
        for (auto* b : { &prev, &load, &next }) b->setVisible (learn);
    }
    void resized() override
    {
        const int y = getHeight() - 30;
        next.setBounds (getWidth() - 62, y, 52, 22);
        prev.setBounds (getWidth() - 118, y, 52, 22);
        load.setBounds (getWidth() - 262, y, 140, 22);
    }
    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        const bool learn = (bool) ph.proc.uiState.getProperty ("learn", true);
        g.setColour (learn ? theme::lessonBg : theme::panel); g.fillRoundedRectangle (r, 6.0f);
        g.setColour (learn ? theme::lessonLn : theme::line); g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.0f);
        auto a = getLocalBounds().reduced (12, 8);
        if (! learn) { paintInfo (g, a); return; }
        const auto& L = lessons(); const int i = index(); const auto& ls = L[(size_t) i];
        g.setFont (theme::mono (10.0f, true)); g.setColour (theme::accent);
        const juce::String head = "LESSON " + juce::String (i + 1).paddedLeft ('0', 2) + " / " + juce::String ((int) L.size());
        g.drawText (head, a.getX(), a.getY(), 120, 14, juce::Justification::centredLeft);
        auto bar = juce::Rectangle<float> ((float) a.getX() + 124, (float) a.getY() + 6, (float) a.getWidth() - 124, 3.0f);
        g.setColour (theme::lessonLn.withAlpha (0.45f)); g.fillRoundedRectangle (bar, 1.5f);
        g.setColour (theme::accent); g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * (i + 1) / (float) L.size()), 1.5f);
        g.setColour (theme::text); g.setFont (theme::ui (14.0f, true));
        g.drawText (juce::String::fromUTF8 (ph.ja() ? ls.titleJa : ls.titleEn), a.getX(), a.getY() + 18, a.getWidth(), 20, juce::Justification::centredLeft);
        const auto& steps = ph.ja() ? ls.stepsJa : ls.stepsEn;
        g.setFont (theme::ui (11.0f)); g.setColour (theme::text.withAlpha (0.82f));
        int y = a.getY() + 42;
        for (size_t k = 0; k < steps.size(); ++k)
        {
            g.drawFittedText (juce::String ((int) k + 1) + ".  " + juce::String::fromUTF8 (steps[k]), a.getX(), y, a.getWidth(), 16, juce::Justification::centredLeft, 1, 0.85f);
            y += 17;
        }
        g.setColour (theme::muted); g.setFont (theme::ui (10.0f));
        g.drawFittedText (ph.tr ("なぜ？ ", "Why? ") + juce::String::fromUTF8 (ph.ja() ? ls.whyJa : ls.whyEn),
                          a.getX(), getHeight() - 30, getWidth() - 290, 22, juce::Justification::centredLeft, 2, 0.85f);
    }
private:
    // Hover help: one line per item ("Key：text" items separated by ／ or " / "), the "Careful" part on its own line in red,
    // the Option+click hint muted, and the largest font (16 → 10 px) that fits the card without cutting anything.
    // the "Key" of a list item: where "Key：", "Key＝", "Key: " or "Key = " ends (0 = none). Keys are short.
    // the "Key" of a list item: where "Key：", "Key＝", "Key: " or "Key = " ends (0 = none). Keys are short and have no sentence mark.
    // (character based: String::indexOf with full-width text is not reliable here)
    static int keyEnd (const juce::String& s)
    {
        const int n = juce::jmin (s.length() - 1, 16);
        for (int i = 1; i < n; ++i)
        {
            const auto c = s[i];
            if (c == 0x3002 || c == 0x3001 || c == '.' || c == ',') return 0;
            if (c == 0xff1a || c == 0xff1d) return i + 1;
            if (c == ':' && s[i + 1] == ' ') return i + 2;
            if (c == ' ' && s[i + 1] == '=' && i + 2 < s.length() && s[i + 2] == ' ') return i + 3;
        }
        return 0;
    }
    // end of a sentence: "。" (ja) or ". " before a capital (en); returns the index just after it, or -1
    static int sentenceEnd (const juce::String& t, int from, bool ja, bool last)
    {
        int found = -1, depth = 0;            // a mark inside brackets does not end the sentence
        for (int i = juce::jmax (0, from); i < t.length(); ++i)
        {
            if (t[i] == '(' || t[i] == 0xff08) { ++depth; continue; }
            if (t[i] == ')' || t[i] == 0xff09) { depth = juce::jmax (0, depth - 1); continue; }
            if (depth > 0) continue;
            const bool hit = ja ? t[i] == 0x3002 : (t[i] == '.' && i + 2 < t.length() && t[i + 1] == ' ' && juce::CharacterFunctions::isUpperCase (t[i + 2]));
            if (hit) { found = i + 1; if (! last) break; }
        }
        return found;
    }
    juce::AttributedString helpLayout (const juce::String& raw, float px) const
    {
        const bool ja = ph.ja();
        const juce::String sep = ja ? juce::String::fromUTF8 ("\xef\xbc\x8f") : juce::String (" / ");
        juce::AttributedString as;
        as.setWordWrap (ja ? juce::AttributedString::byChar : juce::AttributedString::byWord);
        as.setLineSpacing (px * 0.18f);
        const auto fN = theme::ui (px), fS = theme::ui (px - 1.0f);
        bool firstPara = true;
        auto para = [&] (const juce::String& txt, juce::Colour c, bool bullet, bool small)
        {
            auto t = txt.trim(); if (t.isEmpty()) return;
            if (! firstPara) as.append ("\n", fN, c);
            firstPara = false;
            if (bullet) as.append (juce::String::fromUTF8 (ja ? "\xe3\x83\xbb" : "\xe2\x80\xa2 "), fN, theme::muted);
            const int k = bullet ? keyEnd (t) : 0;
            if (k > 0) { as.append (t.substring (0, k), fN, theme::accent); as.append (t.substring (k), fN, c); }   // one font per line: mixing bold breaks the wrap
            else as.append (t, small ? fS : fN, c);
        };
        for (auto line : juce::StringArray::fromLines (raw))
        {
            if (line.trim().isEmpty()) continue;
            if (line.startsWith ("Option")) { para (line, theme::muted, false, true); continue; }
            juce::String main = line, careful;
            {
                int c = line.indexOf ("Careful:");
                for (int i = 0; c < 0 && i + 2 < line.length(); ++i)
                    if (line[i] == 0x6ce8 && line[i + 1] == 0x610f && line[i + 2] == 0xff1a) c = i;      // 注意：
                if (c >= 0) { main = line.substring (0, c); careful = line.substring (c); }
            }
            // "lead. A：… ／ B：… ／ C：…. tail" → lead / one item per line / tail
            juce::StringArray parts;
            if (ja)
            {
                juce::String cur;
                for (auto p = main.getCharPointer(); ! p.isEmpty();) { const auto ch = p.getAndAdvance(); if (ch == (juce::juce_wchar) 0xff0f) { parts.add (cur); cur.clear(); } else cur += juce::String::charToString (ch); }
                parts.add (cur);
            }
            else for (auto rest = main;;) { const int i = rest.indexOf (sep); if (i < 0) { parts.add (rest); break; } parts.add (rest.substring (0, i)); rest = rest.substring (i + sep.length()); }
            juce::StringArray items;
            for (auto& p : parts)
            {
                if (items.isEmpty()) { items.add (p); continue; }
                if (keyEnd (p.trim()) > 0) items.add (p.trim()); else items.set (items.size() - 1, items[items.size() - 1] + sep + p);
            }
            int keyed = 0; for (auto& it : items) if (keyEnd (it.trim()) > 0) ++keyed;
            // the first piece may start with a sentence ("What it listens to. In: …")
            if (items.size() > 1 && keyEnd (items[0].trim()) == 0)
            {
                const int st = sentenceEnd (items[0], 0, ja, false);
                if (st > 0) { const auto key = items[0].substring (st).trim(); if (keyEnd (key) > 0) { const auto lead = items[0].substring (0, st); items.set (0, key); items.insert (0, "\x01" + lead); ++keyed; } }
            }
            if (keyed < 2) { para (main, theme::text, false, false); }
            else
            {
                for (auto& it : items)
                {
                    if (it.startsWithChar (1)) { para (it.substring (1), theme::text, false, false); continue; }
                    const int k = keyEnd (it.trim());
                    if (k == 0) { para (it, theme::text, false, false); continue; }
                    // a following sentence that is not about this item goes on its own line
                    const auto t = it.trim(); const int st = sentenceEnd (t, k, ja, false);
                    if (st > 0 && t.substring (st).trim().length() > 8)
                    { para (t.substring (0, st), theme::text, true, false); para (t.substring (st), theme::text, false, false); }
                    else para (t, theme::text, true, false);
                }
            }
            if (careful.isNotEmpty()) para (careful, theme::pre, false, false);
        }
        return as;
    }
    void drawHelp (juce::Graphics& g, const juce::String& raw, juce::Rectangle<int> r, float maxPx = 16.0f) const
    {
        juce::TextLayout tl;
        for (float px = maxPx; px >= 9.5f; px -= 0.5f)
        {
            tl.createLayout (helpLayout (raw, px), (float) r.getWidth());
            if (tl.getHeight() <= (float) r.getHeight()) break;
        }
        tl.draw (g, r.toFloat());
    }
    void paintInfo (juce::Graphics& g, juce::Rectangle<int> a)
    {
        const int m = (int) ph.proc.currentMode();
        static const char* ja[] = { "ゼロレイテンシー：デジタル IIR。遅れゼロで、音より先に何も出ない。ゲインが変わる帯域で位相が回る。",
                                    "アナログ一致：アナログ回路と同じ振幅と位相を持つ最小位相 FIR。高域を大きく触っても形が崩れない。",
                                    "リニア位相：左右対称の FIR。位相は回らないが、全帯域が遅れ、アタックの前にプリリンギングが出る。" };
        static const char* en[] = { "Zero latency: a digital IIR. No delay and nothing before the sound; the phase turns where the gain changes.",
                                    "Analog: a minimum-phase FIR with the analog magnitude and phase. High-frequency curves keep their shape.",
                                    "Linear phase: a symmetric FIR. No phase shift, but everything is delayed and pre-ringing appears before the attack." };
        auto header = [&] (const juce::String& t, int y, juce::Colour c)
        { g.setColour (c); g.setFont (theme::mono (10.0f, true)); g.drawText (t, a.getX(), y, a.getWidth(), 14, juce::Justification::centredLeft); };
        if (ph.hoverTitle.isNotEmpty())
        {
            header (ph.hoverTitle, a.getY(), theme::accent);
            drawHelp (g, ph.hoverText, a.withTrimmedTop (18));
            return;
        }
        if (const int pi = (int) ph.proc.uiState.getProperty ("preset", -1); pi >= 0 && pi < (int) presets().size())
        {
            const auto& pr = presets()[(size_t) pi];
            const bool ja = ph.ja();
            header ("PRESET  " + juce::String::fromUTF8 (ja ? pr.nameJa : pr.nameEn), a.getY(), theme::accent);
            const juce::String raw = juce::String::fromUTF8 (ja ? pr.purposeJa : pr.purposeEn) + "\n"
                + (ja ? juce::String::fromUTF8 ("まず：") : juce::String ("First: ")) + juce::String::fromUTF8 (ja ? pr.firstJa : pr.firstEn) + "\n"
                + (ja ? juce::String::fromUTF8 ("注意：") : juce::String ("Careful: ")) + juce::String::fromUTF8 (ja ? pr.cautionJa : pr.cautionEn);
            drawHelp (g, raw, a.withTrimmedTop (18));
            return;
        }
        const bool two = a.getWidth() >= 520;
        auto left = two ? a.removeFromLeft (a.getWidth() * 34 / 100) : a.removeFromTop (60);
        if (two) a.removeFromLeft (16); else a.removeFromTop (6);
        g.setColour (theme::muted); g.setFont (theme::mono (10.0f, true)); g.drawText ("CURRENT MODE", left.getX(), left.getY(), left.getWidth(), 14, juce::Justification::centredLeft);
        drawHelp (g, juce::String::fromUTF8 (ph.ja() ? ja[m] : en[m]), left.withTrimmedTop (18), 13.0f);
        if (two) { g.setColour (theme::line); g.drawVerticalLine (a.getX() - 8, (float) a.getY(), (float) a.getBottom()); }
        header ("MOUSE", a.getY(), theme::muted);
        drawHelp (g, juce::String::fromUTF8 (ph.ja() ? mouseHelpJa() : mouseHelpEn()), a.withTrimmedTop (18), 14.0f);
    }
    ParamHelp& ph;
    juce::TextButton prev, load, next;
};

// =============================================================================
//  IN · OUT meters (peak with 1.5 s hold, RMS) and output correlation
// =============================================================================
class IOMeter : public juce::Component
{
public:
    explicit IOMeter (ParamHelp& h) : ph (h)
    {
        setHelp (*this, "IN / OUT", "入出力のピーク（細い線は 1.5 秒ホールド）と RMS（300 ms）。Auto gain の効き具合や、次のプラグインへの送り量の確認に。CREST＝直近 1 秒のピーク − RMS（dB）。上から入力・出力・変化量（マイナス＝密度が上がった、プラス＝メリハリが増えた）。下の CORR は出力の位相相関：+1 で左右同じ、0 で無相関、マイナスはモノラルで打ち消し合う",
                 "Input and output peak (thin line: 1.5 s hold) and RMS (300 ms): check Auto gain and what you send to the next plugin. CREST = peak (last 1 s) minus RMS, dB: input, output, change (minus = the EQ / saturation made it denser, plus = more punch). CORR below is output correlation: +1 identical, 0 unrelated, negative cancels in mono");
    }
    void tick()
    {
        const double dt = 1.0 / 30.0;
        auto pk = [] (std::atomic<float>& a) { return 20.0 * std::log10 (std::max (1e-6f, a.exchange (0.0f))); };
        auto rm = [] (std::atomic<float>& a) { return 10.0 * std::log10 (std::max (1e-12f, a.load())); };
        const double ip = std::max (pk (ph.proc.inPeak[0]), pk (ph.proc.inPeak[1])), op = std::max (pk (ph.proc.outPeak[0]), pk (ph.proc.outPeak[1]));
        ir = std::max (rm (ph.proc.inRms[0]), rm (ph.proc.inRms[1])); orr = std::max (rm (ph.proc.outRms[0]), rm (ph.proc.outRms[1]));
        auto ball = [dt] (double in, double& disp, double& hold, double& t)
        {
            disp = in >= disp ? in : std::max (in, disp - 20.0 * dt);
            if (in >= hold) { hold = in; t = 0; } else if ((t += dt) > 1.5) hold = std::max (in, hold - 20.0 * dt);
        };
        ball (ip, iDisp, iHold, iT); ball (op, oDisp, oHold, oT);
        // crest factor: the highest peak of the last second over the 300 ms RMS, smoothed for reading
        pkHist[(size_t) histPos] = { ip, op }; histPos = (histPos + 1) % (int) pkHist.size();
        double ip1 = -200, op1 = -200; for (auto& h : pkHist) { ip1 = std::max (ip1, h.first); op1 = std::max (op1, h.second); }
        auto cr = [] (double p, double r, double& s) { const double c = (p < -90 || r < -90) ? -1000.0 : p - r; s = c < -999 ? -1000.0 : (s < -999 ? c : s + (c - s) * 0.15); };
        cr (ip1, ir, iCrest); cr (op1, orr, oCrest);
        corr += (ph.proc.correlation.load() - corr) * 0.3;
        repaint();
    }
    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (theme::panel); g.fillRoundedRectangle (r, 6.0f);
        g.setColour (theme::line); g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.0f);
        g.setColour (theme::muted); g.setFont (theme::ui (9.0f, true));
        g.drawText ("IN  OUT", 0, 4, getWidth(), 12, juce::Justification::centred);
        const float top = 20, bottom = (float) getHeight() - 160;
        auto yOf = [&] (double db) { return juce::jmap ((float) juce::jlimit (-60.0, 3.0, db), -60.0f, 3.0f, bottom, top); };
        auto bar = [&] (float x, double disp, double rms, double hold, juce::Colour c)
        {
            auto well = juce::Rectangle<float> (x, top, 9, bottom - top);
            g.setColour (theme::well); g.fillRoundedRectangle (well, 2.0f);
            g.setColour (c.withAlpha (0.45f)); g.fillRect (well.withTop (yOf (disp)));
            g.setColour (c); g.fillRect (well.withTop (yOf (rms)));
            g.setColour (hold > -0.1 ? theme::pre : theme::text); g.fillRect (x - 1, yOf (hold) - 1, 11.0f, 2.0f);
        };
        const float cx = r.getCentreX();
        bar (cx - 12, iDisp, ir, iHold, theme::muted);
        bar (cx + 3, oDisp, orr, oHold, theme::accent);
        g.setColour (theme::faint);
        for (int db : { 0, -12, -24, -36, -48 })                 // 12 dB ticks
        { g.fillRect (cx - 18, yOf (db), 4.0f, 1.0f); g.fillRect (cx + 14, yOf (db), 4.0f, 1.0f); }
        auto num = [] (double v) { return v < -99 ? juce::String::fromUTF8 ("\xe2\x80\x94") : fmtDb (v); };
        float y = bottom + 6;
        g.setFont (theme::mono (9.0f, true));
        for (auto row : { std::make_pair ("PK", std::make_pair (iHold, oHold)), std::make_pair ("RMS", std::make_pair (ir, orr)) })
        {
            g.setColour (theme::muted); g.drawText (row.first, 0, (int) y, getWidth(), 11, juce::Justification::centred); y += 11;
            g.setColour (theme::text); g.drawText (num (row.second.first), 0, (int) y, getWidth(), 11, juce::Justification::centred); y += 11;
            g.setColour (theme::accent); g.drawText (num (row.second.second), 0, (int) y, getWidth(), 11, juce::Justification::centred); y += 13;
        }
        // CREST: input, output, change (between RMS and CORR)
        {
            auto cnum = [] (double v) { return v < -999 ? juce::String::fromUTF8 ("\xe2\x80\x94") : juce::String (v, 1); };
            g.setColour (theme::muted); g.drawText ("CREST", 0, (int) y, getWidth(), 11, juce::Justification::centred); y += 11;
            g.setColour (theme::text); g.drawText (cnum (iCrest), 0, (int) y, getWidth(), 11, juce::Justification::centred); y += 11;
            g.setColour (theme::accent); g.drawText (cnum (oCrest), 0, (int) y, getWidth(), 11, juce::Justification::centred); y += 11;
            const bool ok = iCrest > -999 && oCrest > -999; const double d = ok ? oCrest - iCrest : 0.0;
            g.setColour (! ok ? theme::muted : d < -0.05 ? theme::pre : d > 0.05 ? theme::ok : theme::text);
            g.drawText (! ok ? juce::String::fromUTF8 ("\xe2\x80\x94") : (d >= 0 ? "+" : juce::String::fromUTF8 ("\xe2\x88\x92")) + juce::String (std::abs (d), 1),
                        0, (int) y, getWidth(), 11, juce::Justification::centred); y += 15;
        }
        g.setColour (theme::muted); g.drawText ("CORR", 0, (int) y, getWidth(), 11, juce::Justification::centred); y += 12;
        auto cb = juce::Rectangle<float> (6, y, (float) getWidth() - 12, 6);
        g.setColour (theme::well); g.fillRoundedRectangle (cb, 2.0f);
        g.setColour (theme::faint); g.fillRect (cb.getCentreX() - 0.5f, cb.getY() - 2, 1.0f, cb.getHeight() + 4);
        const float px = cb.getX() + cb.getWidth() * (float) ((corr + 1.0) / 2.0);
        const juce::Colour cc = corr < 0 ? theme::pre : theme::ok;
        g.setColour (cc); g.fillRoundedRectangle (px - 3, cb.getY() - 1, 6, cb.getHeight() + 2, 2.0f);
        g.drawText ((corr >= 0 ? "+" : juce::String::fromUTF8 ("\xe2\x88\x92")) + juce::String (std::abs (corr), 2), 0, (int) y + 8, getWidth(), 11, juce::Justification::centred);
    }
private:
    ParamHelp& ph;
    double iDisp = -100, oDisp = -100, iHold = -100, oHold = -100, iT = 0, oT = 0, ir = -100, orr = -100, corr = 1;
    std::array<std::pair<double, double>, 30> pkHist = [] { std::array<std::pair<double, double>, 30> a; a.fill ({ -200.0, -200.0 }); return a; }(); int histPos = 0; double iCrest = -1000, oCrest = -1000;   // 1 s of peaks at 30 Hz
};

// =============================================================================
//  Ear training: a hidden bell on the DAW signal, guess the band
// =============================================================================
class EarCard : public juce::Component
{
public:
    explicit EarCard (ParamHelp& h) : ph (h)
    {
        for (auto* b : { &easyB, &normB, &hardB, &boostB, &cutB, &bothB, &aB, &bB, &nextB, &showB })
        { addAndMakeVisible (*b); b->setClickingTogglesState (false); }
        easyB.onClick = [this] { level = 0; newQuestion(); }; normB.onClick = [this] { level = 1; newQuestion(); }; hardB.onClick = [this] { level = 2; newQuestion(); };
        boostB.onClick = [this] { mode = 0; newQuestion(); }; cutB.onClick = [this] { mode = 1; newQuestion(); }; bothB.onClick = [this] { mode = 2; newQuestion(); };
        aB.onClick = [this] { playEq = false; ph.proc.setEarPlayEQ (false); refresh(); };
        bB.onClick = [this] { playEq = true;  ph.proc.setEarPlayEQ (true);  refresh(); };
        nextB.onClick = [this] { newQuestion(); };
        for (auto* b : { &kBandB, &kPreB }) { addAndMakeVisible (*b); b->setClickingTogglesState (false); }
        kBandB.onClick = [this] { kind = 0; newQuestion(); }; kPreB.onClick = [this] { kind = 1; newQuestion(); };
        showB.onClick = [this] { if (! answered) { answered = true; chosen = -1; ++total; streak = 0; refresh(); } };
        setHelp (*this, "EAR TRAINING", "DAW の音に、隠れたベル EQ をかけて出題。B（EQ）と A（原音）を聴き比べて、持ち上がっている（または削られている）帯域を当てる。出題中は今の EQ 設定を一時的に外し、音量はそろえている。終了すると元に戻る",
                 "A hidden bell is applied to the DAW signal. Compare B (EQ) and A (original) and pick the band that is boosted (or cut). Your EQ is bypassed while you play and levels are matched; leaving restores it");
    }
    void start() { newQuestion(); }
    // 0: guess the boosted / cut band, 1: which of A / B pre-rings (same filter, min vs linear phase)
    void setKind (int k) { kind = juce::jlimit (0, 1, k); }
    int getKind() const { return kind; }
    void stop() { ph.proc.setEar (false, {}); }
    void refresh()
    {
        auto tog = [] (juce::TextButton& b, bool on) { b.setToggleState (on, juce::dontSendNotification); };
        tog (easyB, level == 0); tog (normB, level == 1); tog (hardB, level == 2);
        tog (boostB, mode == 0); tog (cutB, mode == 1); tog (bothB, mode == 2);
        tog (aB, ! playEq); tog (bB, playEq);
        tog (kBandB, kind == 0); tog (kPreB, kind == 1);
        kBandB.setButtonText (ph.tr ("帯域当て", "Band")); kPreB.setButtonText (ph.tr ("プリリンギング", "Pre-ring"));
        easyB.setButtonText ("Easy"); normB.setButtonText ("Normal"); hardB.setButtonText ("Hard");
        boostB.setButtonText (kind == 1 ? "Low cut" : "Boost"); cutB.setButtonText (kind == 1 ? "High cut" : "Cut"); bothB.setButtonText ("Both");
        if (kind == 1) { aB.setButtonText (ph.tr ("A を聴く", "Play A")); bB.setButtonText (ph.tr ("B を聴く", "Play B")); }
        else { aB.setButtonText (ph.tr ("A 原音", "A original")); bB.setButtonText (ph.tr ("B EQ", "B EQ")); }
        nextB.setButtonText (ph.tr ("次の問題", "Next")); showB.setButtonText (ph.tr ("答えを見る", "Show answer"));
        showB.setVisible (! answered);
        repaint();
    }
    void resized() override
    {
        const int lx = 12;
        easyB.setBounds (lx + 54, 30, 56, 22); normB.setBounds (lx + 112, 30, 60, 22); hardB.setBounds (lx + 174, 30, 52, 22);
        boostB.setBounds (lx + 54, 58, 56, 22); cutB.setBounds (lx + 112, 58, 60, 22); bothB.setBounds (lx + 174, 58, 52, 22);
        kBandB.setBounds (lx + 96, 5, 60, 20); kPreB.setBounds (lx + 158, 5, 92, 20);
        const int qx = 280;
        bB.setBounds (getWidth() - 96, 8, 84, 24); aB.setBounds (getWidth() - 184, 8, 84, 24);
        nextB.setBounds (getWidth() - 96, getHeight() - 32, 84, 24);
        showB.setBounds (getWidth() - 190, getHeight() - 32, 90, 24);
        answerArea = { qx, 38, getWidth() - qx - 12, buttonsHeight() };
    }
    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (theme::lessonBg); g.fillRoundedRectangle (r, 6.0f);
        g.setColour (theme::lessonLn); g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.0f);
        g.setColour (theme::muted); g.setFont (theme::mono (10.0f, true));
        g.drawText ("EAR TRAINING", 12, 8, 96, 14, juce::Justification::centredLeft);
        g.setFont (theme::ui (11.0f));
        g.drawText (ph.tr ("難易度", "Level"), 12, 30, 50, 22, juce::Justification::centredLeft);
        g.drawText (ph.tr ("種類", "Type"), 12, 58, 50, 22, juce::Justification::centredLeft);
        g.setColour (theme::text.withAlpha (0.82f)); g.setFont (theme::ui (10.0f));
        static const char* lj[] = { "Easy：9 オクターブ帯、±9 dB・Q 1.4", "Normal：9 オクターブ帯、±6 dB・Q 2", "Hard：1/3 オクターブ帯、±3 dB・Q 4" };
        static const char* le[] = { "Easy: 9 octave bands, ±9 dB, Q 1.4", "Normal: 9 octave bands, ±6 dB, Q 2", "Hard: 1/3-octave bands, ±3 dB, Q 4" };
        static const char* pj[] = { "Easy：48 dB/oct、ローカット 250〜700 Hz", "Normal：24〜36 dB/oct、ローカット 120〜300 Hz", "Hard：18〜24 dB/oct、ローカット 60〜120 Hz" };
        static const char* pe[] = { "Easy: 48 dB/oct, low cut 250–700 Hz", "Normal: 24–36 dB/oct, low cut 120–300 Hz", "Hard: 18–24 dB/oct, low cut 60–120 Hz" };
        g.drawFittedText (juce::String::fromUTF8 (kind == 1 ? (ph.ja() ? pj[level] : pe[level]) : (ph.ja() ? lj[level] : le[level])), 12, 86, 250, 16, juce::Justification::centredLeft, 1, 0.9f);
        g.setColour (theme::text); g.setFont (theme::mono (12.0f, true));
        g.drawText ("SCORE " + juce::String (score) + " / " + juce::String (total) + "   STREAK " + juce::String (streak), 12, getHeight() - 30, 260, 18, juce::Justification::centredLeft);
        g.setColour (theme::faint); g.setFont (theme::ui (9.5f));
        g.drawFittedText (ph.tr ("出題中は今の EQ を外して聴かせる（終了で戻る）", "Your EQ is bypassed while playing (restored when you leave)"), 12, getHeight() - 50, 260, 16, juce::Justification::centredLeft, 1, 0.9f);

        // question
        g.setColour (theme::accent); g.setFont (theme::mono (10.0f, true));
        g.drawText ("QUESTION " + juce::String (total + (answered ? 0 : 1)).paddedLeft ('0', 2), 280, 8, 110, 24, juce::Justification::centredLeft);
        g.setColour (theme::text); g.setFont (theme::ui (13.0f, true));
        const juce::String verb = q.g > 0 ? ph.tr ("持ち上がっている", "boosted") : ph.tr ("削られている", "cut");
        if (kind == 1)
            g.drawText (ph.tr ("A と B、プリリンギング（アタックの前ににじむ音）があるのはどっち？", "A or B: which one pre-rings (a smear before the attack)?"),
                        390, 8, getWidth() - 390 - 190, 24, juce::Justification::centredLeft);
        else
        g.drawText (ph.ja() ? juce::String::fromUTF8 ("どの帯域が ") + fmtDb (q.g, 0) + " dB " + verb + juce::String::fromUTF8 ("？")
                            : "Which band is " + verb + " by " + fmtDb (std::abs (q.g), 0) + " dB?",
                    390, 8, getWidth() - 390 - 190, 24, juce::Justification::centredLeft);
        const auto& fs = freqs();
        for (int i = 0; i < (int) fs.size(); ++i)
        {
            auto b = cellRect (i).toFloat();
            juce::Colour bg = theme::panel, fg = theme::text, ln = theme::line;
            if (answered && i == answer) { bg = theme::ok.withAlpha (0.19f); fg = theme::ok; ln = fg; }
            else if (answered && i == chosen) { bg = theme::pre.withAlpha (0.15f); fg = theme::pre; ln = fg; }
            else if (i == hover) bg = bg.brighter (0.1f);
            g.setColour (bg); g.fillRoundedRectangle (b, 4.0f);
            g.setColour (ln); g.drawRoundedRectangle (b.reduced (0.5f), 4.0f, 1.0f);
            g.setColour (fg); g.setFont (theme::mono (level == 2 ? 10.0f : 12.0f, answered && i == answer));
            g.drawText (kind == 1 ? juce::String (i == 0 ? "A" : "B") : fmtHz (fs[(size_t) i]), b, juce::Justification::centred);
        }
        if (answered)
        {
            const int ty = answerArea.getBottom() + 6;
            g.setColour (theme::text); g.setFont (theme::ui (11.5f));
            juce::String t;
            if (kind == 1)
            {
                const juce::String lin = answer == 1 ? "B" : "A", mn = answer == 1 ? "A" : "B";
                const juce::String what = juce::String (q.type == (int) eqcore::Type::LowCut ? "Low cut " : "High cut ") + fmtHz (q.f) + " Hz " + slopeNames()[q.slope];
                t = (chosen == answer ? ph.tr ("正解！ ", "Correct! ") : ph.tr ("不正解。", "Not quite. "))
                  + (ph.ja() ? lin + juce::String::fromUTF8 (" がリニア位相（音の前にリンギング）、") + mn + juce::String::fromUTF8 (" が最小位相（リンギングは音の後ろ）。フィルター：") + what
                             : lin + " is linear phase (ringing before the sound), " + mn + " is minimum phase (ringing after it). Filter: " + what)
                  + ph.tr ("。キックやスネアなど、アタックの鋭い音ほど違いが分かりやすい", ". Sharp attacks (kick, snare) make it easiest to hear");
                g.drawFittedText (t, 280, ty, getWidth() - 280 - 200, getHeight() - ty - 4, juce::Justification::topLeft, 3, 0.85f);
                return;
            }
            if (chosen == answer) t = ph.tr ("正解！ ", "Correct! ");
            else if (chosen >= 0) t = ph.tr ("正解は ", "It was ") + fmtHz (q.f) + " Hz. ";
            else t = ph.tr ("正解は ", "It was ") + fmtHz (q.f) + " Hz. ";
            t += describe (q.f);
            if (chosen >= 0 && chosen != answer) t += "  /  " + fmtHz (freqs()[(size_t) chosen]) + ": " + describe (freqs()[(size_t) chosen]);
            g.drawFittedText (t, 280, ty, getWidth() - 280 - 200, getHeight() - ty - 4, juce::Justification::topLeft, 3, 0.85f);
        }
    }
    void mouseMove (const juce::MouseEvent& e) override { const int h = hit (e.getPosition()); if (h != hover) { hover = h; repaint(); } }
    void mouseExit (const juce::MouseEvent&) override { hover = -1; repaint(); }
    void mouseDown (const juce::MouseEvent& e) override
    {
        const int h = hit (e.getPosition());
        if (h < 0 || answered) return;
        chosen = h; answered = true; ++total;
        if (h == answer) { ++score; ++streak; } else streak = 0;
        refresh();
    }
private:
    const std::vector<double>& freqs() const
    {
        static const std::vector<double> ab { 0, 1 };      // pre-ring question: A, B
        if (kind == 1) return ab;
        static const std::vector<double> oct { 63, 125, 250, 500, 1000, 2000, 4000, 8000, 16000 };
        static const std::vector<double> third { 50, 63, 80, 100, 125, 160, 200, 250, 315, 400, 500, 630, 800, 1000, 1250, 1600, 2000, 2500, 3150, 4000, 5000, 6300, 8000, 10000, 12500, 16000 };
        return level == 2 ? third : oct;
    }
    int buttonsHeight() const { return level == 2 && kind == 0 ? 58 : 40; }
    juce::Rectangle<int> cellRect (int i) const
    {
        const int n = (int) freqs().size(), perRow = level == 2 && kind == 0 ? 13 : n, rows = level == 2 && kind == 0 ? 2 : 1;
        const int w = (answerArea.getWidth() - (perRow - 1) * 4) / perRow, h = (answerArea.getHeight() - (rows - 1) * 4) / rows;
        return { answerArea.getX() + (i % perRow) * (w + 4), answerArea.getY() + (i / perRow) * (h + 4), w, h };
    }
    int hit (juce::Point<int> p) const { for (int i = 0; i < (int) freqs().size(); ++i) if (cellRect (i).contains (p)) return i; return -1; }
    juce::String describe (double f) const
    {
        static const double c[] = { 63, 125, 250, 500, 1000, 2000, 4000, 8000, 16000 };
        static const char* ja[] = { "重さ・床鳴り", "太さ・ボディ", "こもり・箱鳴り", "鼻声っぽさ・ホンキー", "前に出る・電話っぽさ", "硬さ・鼻にかかる", "輪郭・シャリッとした感じ", "歯擦音・きらめき", "空気感" };
        static const char* en[] = { "weight, rumble", "body, warmth", "mud, boxiness", "nasal, honky", "forward, telephone-like", "hard, nasal edge", "definition, crunch", "sibilance, sparkle", "air" };
        int best = 0; for (int i = 1; i < 9; ++i) if (std::abs (std::log2 (f / c[i])) < std::abs (std::log2 (f / c[best]))) best = i;
        return juce::String::fromUTF8 (ph.ja() ? ja[best] : en[best]);
    }
    void newQuestion()
    {
        auto& rnd = juce::Random::getSystemRandom();
        if (kind == 1)
        {
            // the same cut in min and linear phase; which one is linear (pre-rings) is the answer
            static const std::vector<float> lc[] = { { 250, 350, 500, 700 }, { 120, 160, 200, 300 }, { 60, 80, 100, 120 } };
            static const std::vector<float> hc[] = { { 1500, 2000, 3000 }, { 3000, 4000, 5000 }, { 6000, 8000, 10000 } };
            const bool high = mode == 1 || (mode == 2 && rnd.nextBool());
            const auto& set = high ? hc[level] : lc[level];
            q = {}; q.kind = 1; q.type = (int) (high ? eqcore::Type::HighCut : eqcore::Type::LowCut);
            q.f = set[(size_t) rnd.nextInt ((int) set.size())]; q.g = 0; q.q = 0.7071f;
            q.slope = level == 0 ? 5 : level == 1 ? 3 + rnd.nextInt (2) : 2 + rnd.nextInt (2);   // 48 / 24–36 / 18–24 dB/oct
            q.linOnB = rnd.nextBool(); answer = q.linOnB ? 1 : 0;
            answered = false; chosen = -1; playEq = false;
            ph.proc.setEarPlayEQ (false); ph.proc.setEar (true, q);
            resized(); refresh();
            return;
        }
        const auto& fs = freqs();
        answer = rnd.nextInt ((int) fs.size());
        const double gain = level == 0 ? 9.0 : level == 1 ? 6.0 : 3.0;
        const bool cut = mode == 1 || (mode == 2 && rnd.nextBool());
        q.f = (float) fs[(size_t) answer]; q.g = (float) (cut ? -gain : gain); q.q = level == 0 ? 1.4f : level == 1 ? 2.0f : 4.0f;
        answered = false; chosen = -1; playEq = true;
        ph.proc.setEar (true, q); ph.proc.setEarPlayEQ (true);
        resized(); refresh();
    }
    ParamHelp& ph;
    juce::TextButton easyB, normB, hardB, boostB, cutB, bothB, aB, bB, nextB, showB, kBandB, kPreB;
    EQAnatomyProcessor::EarQuestion q;
    int kind = 0, level = 1, mode = 0, answer = 0, chosen = -1, hover = -1, score = 0, total = 0, streak = 0; bool answered = false, playEq = true;
    juce::Rectangle<int> answerArea;
};
