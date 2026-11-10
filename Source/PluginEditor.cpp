#include "PluginEditor.h"
#include "Lessons.h"
#include "Presets.h"

EQAnatomyView::EQAnatomyView (EQAnatomyProcessor& p) : proc (p)
{
    // the look is shared by every instance on this computer
    theme::setSkin ((int) ZoomPrefs::file().getIntValue ("eqanatomy2.skin", 0)); lnf.apply();
    for (int i = 0; i < theme::numSkins(); ++i) zoom.skinNames.add (theme::skinName (i));
    zoom.getSkin = [] { return theme::current; };
    zoom.setSkin = [this] (int i) { applySkin (i); };
    theme::surface = juce::jlimit (0, theme::numSurfaces() - 1, ZoomPrefs::file().getIntValue ("eqanatomy2.surface", 0));
    for (int i = 0; i < theme::numSurfaces(); ++i) zoom.surfaceNames.add (theme::surfaceName (i));
    zoom.getSurface = [] { return theme::surface; };
    zoom.setSurface = [this] (int i) { applySurface (i); };
    for (const char* n : { "+-6", "+-12", "+-20 (default)", "+-30" }) zoom.scaleNames.add (n);
    zoom.getScale = [] { return juce::jlimit (0, 3, ZoomPrefs::file().getIntValue ("eqanatomy2.gscale", 2)); };
    zoom.setScale = [this] (int i) { applyScale (i); };
    applyScale (zoom.getScale());
    setLookAndFeel (&lnf);
    juce::LookAndFeel::setDefaultLookAndFeel (&lnf);

    auto paramToggle = [this] (juce::TextButton& b, const char* id)
    {
        b.onClick = [this, id] { ph.toggleGesture (id); };
        markReset (b);
        addAndMakeVisible (b);
    };
    modeBox.addItemList ({ "Zero latency", "Analog", "Linear" }, 1);
    markReset (modeBox); modeBox.onAlt = [this] { ph.resetDefault (pid::mode); };
    modeBox.onChange = [this] { proc.undo.beginNewTransaction(); ph.setGesture (pid::mode, (float) (modeBox.getSelectedId() - 1)); };
    addAndMakeVisible (modeBox);
    resBtn.onClick     = [this] { ph.setGesture (pid::res, (float) (((int) ph.value (pid::res) + 1) % 3)); };
    for (int i = 0; i < 7; ++i) curveBox.addItem (eqcore::curveName (i), i + 1);
    markReset (curveBox); curveBox.onAlt = [this] { ph.resetDefault (pid::qmode); };
    curveBox.onChange  = [this] { proc.undo.beginNewTransaction(); ph.setGesture (pid::qmode, (float) (curveBox.getSelectedId() - 1)); };
    for (int i = 0; i < 4; ++i)
    {
        slotBtn[i].setButtonText (juce::String::charToString ((juce::juce_wchar) ('A' + i)));
        slotBtn[i].onClick = [this, i] { proc.switchSlot (i); morph.setValue (0.0, juce::dontSendNotification); };
        addAndMakeVisible (slotBtn[i]);
        setHelp (slotBtn[i], "A / B / C / D", "4つの設定を切り替えて比べる。切り替える前の設定はそのスロットに保存される",
                 "Switch between four settings; the one you leave is stored in its slot");
    }
    morphTo.onClick = [this]
    {
        int t = (int) proc.uiState.getProperty ("morphTo", 1);
        do t = (t + 1) % 4; while (t == proc.currentSlot());
        proc.uiState.setProperty ("morphTo", t, nullptr);
    };
    morph.setSliderStyle (juce::Slider::LinearHorizontal); morph.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    morph.setRange (0.0, 1.0, 0.0); morph.setDoubleClickReturnValue (true, 0.0);
    morph.setDoubleClickReturnValue (true, 0.0, juce::ModifierKeys::altModifier); markReset (morph);   // Option+click: back to 0
    morph.setColour (juce::Slider::thumbColourId, theme::accent); morph.setColour (juce::Slider::trackColourId, theme::accent.withAlpha (0.4f));
    morph.onDragStart = [this] { proc.storeCurrentSlot(); proc.undo.beginNewTransaction(); };
    morph.onValueChange = [this]
    {
        const int to = (int) proc.uiState.getProperty ("morphTo", 1);
        if (to != proc.currentSlot()) proc.morph (proc.currentSlot(), to, (float) morph.getValue());
    };
    undoBtn.onClick = [this] { proc.undo.undo(); };
    redoBtn.onClick = [this] { proc.undo.redo(); };
    earBtn.onClick  = [this] { setEar (! proc.earActive()); };
    learnBtn.onClick   = [this]
    {
        const bool ja = ph.ja(); const bool on = (bool) proc.uiState.getProperty ("learn", true);
        const int cur = (int) proc.uiState.getProperty ("lesson", 0);
        juce::PopupMenu m;
        m.addItem (1, ja ? juce::String::fromUTF8 ("レッスンを閉じる（ヘルプ表示）") : juce::String ("Close lessons (show help)"), true, ! on);
        m.addSeparator();
        const auto& L = lessons();
        for (int i = 0; i < (int) L.size(); ++i)
            m.addItem (100 + i, juce::String (i + 1).paddedLeft ('0', 2) + "  " + juce::String::fromUTF8 (ja ? L[(size_t) i].titleJa : L[(size_t) i].titleEn), true, on && i == cur);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&learnBtn), [this] (int r)
        {
            if (r == 1) { proc.uiState.setProperty ("learn", false, nullptr); return; }
            if (r >= 100)
            {
                if (proc.earActive()) setEar (false);                  // EAR and Learn are exclusive
                proc.uiState.setProperty ("lesson", r - 100, nullptr);
                proc.uiState.setProperty ("learn", true, nullptr);
            }
        });
    };
    langBtn.onClick    = [this] { proc.uiState.setProperty ("lang", ph.ja() ? "en" : "ja", nullptr); };
    resetBtn.onClick   = [this]
    {
        juce::PopupMenu m;
        const bool ja = ph.ja();
        m.addItem (1, ja ? juce::String::fromUTF8 ("Flat（初期化）") : juce::String ("Flat (reset)"));
        m.addSeparator();
        const auto& P = presets();
        const int cur = (int) proc.uiState.getProperty ("preset", -1);
        for (int c = 0; c < 6; ++c)
        {
            juce::PopupMenu sub;
            for (int i = 0; i < (int) P.size(); ++i)
                if (P[(size_t) i].category == c)
                    sub.addItem (100 + i, juce::String::fromUTF8 (ja ? P[(size_t) i].nameJa : P[(size_t) i].nameEn), true, i == cur);
            if (sub.getNumItems() > 0) m.addSubMenu (juce::String::fromUTF8 (presetCategoryName (c, ja)), sub);
        }
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&resetBtn), [this] (int r)
        {
            if (r == 1) proc.applyPreset (-1);
            else if (r >= 100) proc.applyPreset (r - 100);
            morph.setValue (0.0, juce::dontSendNotification);
        });
    };
    for (auto* b : { &resBtn, &learnBtn, &langBtn, &resetBtn, &undoBtn, &redoBtn, &earBtn, &morphTo })
        addAndMakeVisible (*b);
    addAndMakeVisible (curveBox); addAndMakeVisible (morph);
    paramToggle (autoBtn, pid::autoGain);
    paramToggle (deltaBtn, pid::delta);
    paramToggle (bypassBtn, pid::bypass);
    paramToggle (soloBtn, pid::preSolo);
    soloBtn.setColour (juce::TextButton::buttonOnColourId, theme::pre);

    addAndMakeVisible (graph); addAndMakeVisible (satStrip); addAndMakeVisible (detail); addAndMakeVisible (irView);
    addAndMakeVisible (outDrag); addAndMakeVisible (viewSolo); addAndMakeVisible (lesson); addAndMakeVisible (io); addChildComponent (earCard);

    addAndMakeVisible (zoom);
    setHelp (modeBox, "MODE", "処理の方式。Zero latency：全体の既定。Min のバンドはデジタル IIR（遅れゼロ、位相が回る、ナイキスト付近は形が詰まる）。Lin にしたバンドだけ FIR になり、そのときは全体が FIR 分遅れる。DYN と TRANSIENT/BODY はこのモードの Min バンドだけ／Analog：Min のバンドを、アナログ回路と同じ振幅と位相の最小位相 FIR で処理。高域も形が崩れない。Lin のバンドがなければレイテンシーなし／Linear：全バンドをリニア位相にする（バンドごとの Min/Lin は無視）。位相は回らないが、全体が遅れ、アタックの前にプリリンギングが出る",
             "Processing mode. Zero latency: the default. Min bands run as digital IIR (no delay, phase turns, cramping near Nyquist); bands set to Lin become FIR, which delays everything. DYN and TRANSIENT/BODY work on Min bands in this mode only / Analog: Min bands become a minimum-phase FIR with the analog magnitude and phase; highs keep their shape; no latency unless a band is Lin / Linear: all bands linear phase (per-band Min/Lin is ignored); no phase shift, but everything is delayed and pre-ringing appears before the attack");
    setHelp (resBtn, "RESOLUTION", "リニア位相の FIR の長さ（Low / Mid / High）。長いほど低域が正確だが、レイテンシーとプリリンギングも伸びる", "Linear-phase FIR length. Longer is more accurate in the lows, with more latency and pre-ringing");
    setHelp (curveBox, "CURVE", "全バンドのカーブの性格。Clean Const／Clean Prop：従来の一定 Q と比例 Q／British A：広いベルと肩の盛り上がり／US 500：強い比例 Q／Console E：比例寄りのベルとくっきりしたシェルフ／Tube Passive：ブーストとカットが干渉する低域／Precision：色付けのない一定 Q。注意：カーブの形の傾向だけで機材の音そのものではない。質感まで比べるなら SAT の Transformer・Tube・Class A と組み合わせる。名称は一般名",
             "Curve character for all bands. Clean Const/Prop: the original constant and proportional Q / British A: broad bells, shelf shoulders / US 500: strongly proportional / Console E: mildly proportional bells, crisp shelves / Tube Passive: interacting low boost and cut / Precision: uncoloured constant Q. Careful: shape tendencies only, not the sound of the hardware; pair with SAT Transformer / Tube / Class A for texture. Generic names");
    setHelp (resetBtn, "PRESET", "作業テンプレート。読み込むと、目的・まず動かす所・注意点がこの欄に出て、動かすバンドが光る。比較用のプリセットは A〜D スロットにも設定が入る。Flat で初期化（Undo で戻せる）。注意：数値は出発点。必ず耳で合わせる",
          "Task templates: loading one shows its purpose, the first control to move and a caution here, and highlights the band. Comparison presets also fill slots A–D. Flat resets (undoable). Careful: values are starting points; always adjust by ear");
    setHelp (undoBtn, "UNDO", "ひとつ前の操作に戻す（Cmd＋Z）", "Undo the last change (Cmd + Z)");
    setHelp (redoBtn, "REDO", "戻した操作をやり直す（Shift＋Cmd＋Z）", "Redo (Shift + Cmd + Z)");
    setHelp (morphTo, "MORPH TARGET", "モーフの行き先のスロット。クリックで切り替え", "The slot the morph slider heads for. Click to change");
    setHelp (morph, "MORPH", "今のスロットから行き先のスロットへ連続で変える。周波数と Q は対数、ゲインは dB で補間。ダブルクリックで 0。注意：タイプや Min/Lin、M/S のように連続で変えられないものは 50% で切り替わる。Lin のバンドは IR を作り直すため少し遅れて追従する。動かしたままスロットを切り替えると、その状態が今のスロットに保存される",
             "Morph from the current slot to the target. Frequency and Q interpolate logarithmically, gain in dB. Double-click resets. Careful: type, Min/Lin and M/S switch at 50%; Lin bands follow with a short delay while the IR rebuilds; switching slots stores the morphed state in the current slot");
    setHelp (earBtn, "EAR", "耳トレモード。隠れた EQ の帯域を当てる。注意：出題中は今の EQ を一時的に外す（終了すると戻る）", "Ear training: guess the hidden EQ band. Careful: your EQ is bypassed while playing (restored when you leave)");
    setHelp (learnBtn, "LEARN", "クリックでレッスン一覧（全タイトル）。選ぶとそのレッスンを表示。EAR とは同時に使えない（片方を選ぶともう片方は閉じる）。「レッスンを閉じる」にすると、この欄に方式とマウス操作、ポインターを置いた部分の説明が出る", "Click for the list of every lesson; pick one to show it. Not at the same time as EAR (choosing one closes the other). \"Close lessons\" shows the mode, mouse controls and help for whatever you point at here");
    setHelp (langBtn, "LANGUAGE", "説明とレッスンの言語（日本語 / English）", "Language of help and lessons");
    setHelp (zoom, "SIZE / LOOK", "画面の大きさ、外観（10種類）、隙間の質感（6種類）。大きさは右下の角のドラッグでも変えられる。外観と質感はこの Mac のすべてのインスタンスで共通", "Window size, look (ten skins) and surface texture for the gaps (six). Drag the bottom-right corner to resize. Look and surface are shared by every instance on this computer");
    setHelp (autoBtn, "AUTO GAIN", "EQ による音量変化を打ち消して、音色の違いだけを比べる", "Cancels the level change of the EQ so you compare tone only");
    setHelp (deltaBtn, "DELTA", "EQ で変わった成分だけを聴く（出力 − 原音）。グラフに赤い破線で |H − 1| を表示。注意：位相が 60° 以上回る帯域や +6 dB を超えるブーストでは、原音より大きく聴こえる（異常ではない）",
             "Hear only what the EQ changed (output minus input); the red dashed line shows |H − 1|. Careful: where the phase turns more than 60°, or boosts exceed +6 dB, it is louder than the input — that is expected");
    setHelp (bypassBtn, "BYPASS", "EQ を通さない原音（レイテンシーはそろえたまま）", "The unprocessed signal (latency kept aligned)");
    setHelp (soloBtn, "PRE-RING SOLO", "リニア位相のプリリンギング（音より前に出る成分）だけを聴く。Lin のバンドがあるときに働く", "Hear only the linear-phase pre-ringing, the part before the sound (needs a Lin band)");
    addMouseListener (this, true);
    setWantsKeyboardFocus (true);
    setSize (1160, 760);
    refreshTexts();
    startTimerHz (30);
}

EQAnatomyView::~EQAnatomyView()
{
    if (proc.earActive()) earCard.stop();
    proc.soloBand = -1;
    proc.soloView = 0; proc.analyzerNeedTB = false; proc.scopeNeed = false;      // no hidden solo state once the window is gone (bounce safety)
    removeMouseListener (this);
    juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
    setLookAndFeel (nullptr);
}

void EQAnatomyView::mouseDown (const juce::MouseEvent&)
{
    proc.undo.beginNewTransaction();     // every click starts a new undo step
}

void EQAnatomyView::applySkin (int i)
{
    theme::setSkin (i);
    ZoomPrefs::file().setValue ("eqanatomy2.skin", i); ZoomPrefs::file().saveIfNeeded();
    lnf.apply();
    sendLookAndFeelChange();
    refreshTexts();
    repaint();
}

void EQAnatomyView::applySurface (int i)
{
    theme::surface = juce::jlimit (0, theme::numSurfaces() - 1, i);
    ZoomPrefs::file().setValue ("eqanatomy2.surface", theme::surface); ZoomPrefs::file().saveIfNeeded();
    repaint();
}

void EQAnatomyView::applyScale (int i)
{
    i = juce::jlimit (0, 3, i);
    ZoomPrefs::file().setValue ("eqanatomy2.gscale", i); ZoomPrefs::file().saveIfNeeded();
    graph.setDbRange (i == 0 ? 6.0f : i == 1 ? 12.0f : i == 2 ? 20.0f : 30.0f);
    repaint();
}

void EQAnatomyView::setEar (bool on)
{
    if (on) proc.uiState.setProperty ("learn", false, nullptr);   // EAR and Learn are exclusive
    if (on) earCard.start(); else earCard.stop();
    resized();
}

void EQAnatomyView::refreshTexts()
{
    const int mode = (int) ph.value (pid::mode);
    if (modeBox.getSelectedId() != mode + 1) modeBox.setSelectedId (mode + 1, juce::dontSendNotification);
    static const char* re[] = { "Res Low", "Res Mid", "Res High" };
    const int r = (int) ph.value (pid::res);
    resBtn.setButtonText (re[r]);
    bool anyLin = mode == 2;
    for (int i = 0; i < eqcore::kBands; ++i) anyLin = anyLin || (ph.value (pid::band (i, "on")) > 0.5f && proc.bandIsLin (i));
    resBtn.setAlpha (anyLin ? 1.0f : 0.4f);
    const int cv = (int) ph.value (pid::qmode);
    if (curveBox.getSelectedId() != cv + 1) curveBox.setSelectedId (cv + 1, juce::dontSendNotification);
    for (int i = 0; i < 4; ++i)
    {
        slotBtn[i].setToggleState (proc.currentSlot() == i, juce::dontSendNotification);
        slotBtn[i].setAlpha (i == proc.currentSlot() || proc.slotState (i).isValid() ? 1.0f : 0.55f);
    }
    int to = (int) proc.uiState.getProperty ("morphTo", 1);
    if (to == proc.currentSlot()) { to = (to + 1) % 4; proc.uiState.setProperty ("morphTo", to, nullptr); }
    morphTo.setButtonText (juce::String::fromUTF8 ("\xe2\x86\x92") + juce::String::charToString ((juce::juce_wchar) ('A' + to)));
    morph.setEnabled (proc.slotState (to).isValid());
    morph.setAlpha (morph.isEnabled() ? 1.0f : 0.4f);
    undoBtn.setButtonText (juce::String::fromUTF8 ("\xe2\x86\xb6")); redoBtn.setButtonText (juce::String::fromUTF8 ("\xe2\x86\xb7"));
    undoBtn.setEnabled (proc.undo.canUndo()); redoBtn.setEnabled (proc.undo.canRedo());
    undoBtn.setAlpha (proc.undo.canUndo() ? 1.0f : 0.4f); redoBtn.setAlpha (proc.undo.canRedo() ? 1.0f : 0.4f);
    morph.setColour (juce::Slider::thumbColourId, theme::accent); morph.setColour (juce::Slider::trackColourId, theme::accent.withAlpha (0.4f));
    soloBtn.setColour (juce::TextButton::buttonOnColourId, theme::pre);
    earBtn.setButtonText ("EAR"); earBtn.setToggleState (proc.earActive(), juce::dontSendNotification);
    learnBtn.setButtonText ("Learn");
    learnBtn.setToggleState ((bool) proc.uiState.getProperty ("learn", true), juce::dontSendNotification);
    langBtn.setButtonText (ph.ja() ? "EN" : juce::String::fromUTF8 ("日本語"));
    {
        const int pi = (int) proc.uiState.getProperty ("preset", -1);
        resetBtn.setButtonText ((pi >= 0 && pi < (int) presets().size() ? juce::String (presets()[(size_t) pi].nameEn) : juce::String::fromUTF8 ("\xe2\x80\x94"))
                                + juce::String::fromUTF8 (" \xe2\x96\xbe"));
    }
    autoBtn.getProperties().set ("fontPx", 11.0);
    autoBtn.setButtonText (ph.value (pid::autoGain) > 0.5f ? "Auto gain " + fmtDb (proc.currentAutoGainDb()) + " dB" : juce::String ("Auto gain"));
    deltaBtn.setButtonText ("Delta");
    bypassBtn.setButtonText ("Bypass");
    soloBtn.setButtonText ("Pre-ring solo");
    autoBtn.setToggleState (ph.value (pid::autoGain) > 0.5f, juce::dontSendNotification);
    deltaBtn.setToggleState (ph.value (pid::delta) > 0.5f, juce::dontSendNotification);
    bypassBtn.setToggleState (ph.value (pid::bypass) > 0.5f, juce::dontSendNotification);
    soloBtn.setToggleState (ph.value (pid::preSolo) > 0.5f, juce::dontSendNotification);
    soloBtn.setAlpha (anyLin ? 1.0f : 0.4f);
    lesson.refreshTexts();
    if (earCard.isVisible()) earCard.refresh();
}

void EQAnatomyView::paint (juce::Graphics& g)
{
    theme::paintSurface (g, getLocalBounds());
    g.setColour (theme::header); g.fillRect (0, 0, getWidth(), 44);
    g.setColour (theme::line); g.drawHorizontalLine (44, 0, (float) getWidth());

    g.setColour (theme::text); g.setFont (theme::mono (13.0f, true));
    g.setFont (theme::mono (11.5f, true));
    g.setFont (theme::mono (10.5f, true));
    g.drawText ("EQ ANATOMY LAB 2", 12, 6, 104, 20, juce::Justification::centredLeft);
    g.setColour (theme::faint); g.setFont (theme::ui (9.0f));
    g.drawText ("by Johan Plus Kei", 12, 24, 100, 14, juce::Justification::centredLeft);
    g.setColour (theme::accent); g.setFont (theme::mono (8.5f, true));
    g.drawText (juce::String::fromUTF8 ("\xce\xb2"), 88, 24, 12, 14, juce::Justification::centredLeft);   // beta mark

    const int lat = proc.currentLatency();
    const double fs = proc.getSampleRateOrDefault();
    g.setFont (theme::mono (10.0f));
    g.setColour (lat > 0 ? theme::pre : theme::muted);
    g.drawText ("LAT " + (lat > 0 ? juce::String (lat * 1000.0 / fs, 1) + " ms" : juce::String ("0 ms")), 718, 6, 60, 16, juce::Justification::centredLeft);
    g.setColour (theme::muted); g.setFont (theme::ui (8.5f));
    g.drawText ("MORPH", 918, 2, 80, 10, juce::Justification::centredLeft);
    // captions for the two menus whose text is a name (curve character, preset)
    g.setColour (theme::muted); g.setFont (theme::ui (8.5f, true));
    g.drawText ("CURVE", curveBox.getX() - 38, 10, 35, 24, juce::Justification::centredRight);
    g.drawText ("PRESET", resetBtn.getX() - 42, 10, 39, 24, juce::Justification::centredRight);
}

void EQAnatomyView::resized()
{
    zoom.setBounds (104, 12, 42, 20);
    undoBtn.setBounds (150, 11, 26, 22); redoBtn.setBounds (178, 11, 26, 22);
    int x = 210;
    modeBox.setBounds (x, 10, 112, 24); x += 116;
    resBtn.setBounds (x, 10, 60, 24); x += 64;
    x += 38; curveBox.setBounds (x, 10, 108, 24); x += 112;      // "CURVE" caption on the left
    x += 42; resetBtn.setBounds (x, 10, 712 - x, 24);           // "PRESET" caption on the left
    for (int i = 0; i < 4; ++i) slotBtn[i].setBounds (778 + i * 26, 10, 24, 24);
    morphTo.setBounds (884, 12, 30, 20);
    morph.setBounds (916, 10, 70, 26);
    earBtn.setBounds (990, 10, 40, 24);
    learnBtn.setBounds (1034, 10, 52, 24);
    langBtn.setBounds (1090, 10, 58, 24);

    graph.setBounds (12, 54, 1076, 470);
    io.setBounds (1096, 54, 52, 470);
    satStrip.setBounds (268, 530, 880, 44);

    const int y = 582, h = 166;
    const bool ear = proc.earActive();
    for (auto* c : { (juce::Component*) &detail, (juce::Component*) &irView, (juce::Component*) &outDrag, (juce::Component*) &autoBtn,
                     (juce::Component*) &deltaBtn, (juce::Component*) &viewSolo, (juce::Component*) &bypassBtn, (juce::Component*) &soloBtn, (juce::Component*) &lesson })
        c->setVisible (! ear);
    earCard.setVisible (ear);
    earCard.setBounds (12, y, getWidth() - 24, h);
    detail.setBounds (12, 530, 248, y + h - 530);     // grows up into the SAT row
    irView.setBounds (268, y, 196, h);
    const int bx = 472, bw = 104, bh = (h - 20) / 6;
    outDrag.setBounds (bx, y, bw, bh);
    autoBtn.setBounds (bx, y + (bh + 4) * 1, bw, bh);
    deltaBtn.setBounds (bx, y + (bh + 4) * 2, bw, bh);
    viewSolo.setBounds (bx, y + (bh + 4) * 3, bw, bh);
    bypassBtn.setBounds (bx, y + (bh + 4) * 4, bw, bh);
    soloBtn.setBounds (bx, y + (bh + 4) * 5, bw, bh);
    lesson.setBounds (584, y, getWidth() - 584 - 12, h);
}

bool EQAnatomyView::keyPressed (const juce::KeyPress& k)
{
    const auto c = k.getTextCharacter();
    if (k == juce::KeyPress ('z', juce::ModifierKeys::commandModifier, 0)) { proc.undo.undo(); return true; }
    if (k == juce::KeyPress ('z', juce::ModifierKeys::commandModifier | juce::ModifierKeys::shiftModifier, 0)) { proc.undo.redo(); return true; }
    if (c >= '1' && c <= '0' + eqcore::kBands) { ph.select (c - '1'); return true; }
    if (k == juce::KeyPress::deleteKey || k == juce::KeyPress::backspaceKey)
    { ph.setGesture (pid::band (ph.selected(), "on"), 0.0f); return true; }
    return false;
}

void EQAnatomyView::pullAnalyzer()
{
    auto& f = proc.analyzerFifo;
    const int n = f.getNumReady();
    if (n <= 0) return;
    int s1, n1, s2, n2;
    f.prepareToRead (n, s1, n1, s2, n2);
    const std::array<const std::vector<float>*, 7> srcBuf { &proc.analyzerBuffer, &proc.analyzerS, &proc.analyzerT, &proc.analyzerB,
                                                            &proc.analyzerInM, &proc.analyzerInS, &proc.analyzerSC };
    const int ringN = (int) rings[0].size();
    for (int k = 0; k < 7; ++k)
    {
        int pos = ringPos;
        auto push = [&] (const float* d, int cnt) { for (int i = 0; i < cnt; ++i) { rings[(size_t) k][(size_t) pos] = d[i]; pos = (pos + 1) % ringN; } };
        if (n1 > 0) push (srcBuf[(size_t) k]->data() + s1, n1);
        if (n2 > 0) push (srcBuf[(size_t) k]->data() + s2, n2);
        if (k == 6) ringPos = pos;
    }
    f.finishedRead (n1 + n2);

    // layer A is always drawn; layer B is optional (0 = off, otherwise source + 1)
    int a = juce::jlimit (0, 6, (int) proc.uiState.getProperty ("anA", 0));
    int b = juce::jlimit (0, 8, (int) proc.uiState.getProperty ("anB", 0)) - 1;
    linkTap.reset();
    if (b == 7)
    {
        const auto want = proc.uiState.getProperty ("anBLink", "").toString();
        for (auto& t : LinkRegistry::list()) if (t != proc.linkTap && t->name() == want) { linkTap = t; break; }
        if (linkTap == nullptr) b = -1;                // that instance is gone (or not loaded yet)
    }
    // SOLO M / S shows Mid + Side, SOLO T / B shows Trans + Body; the saved choice comes back when the solo ends
    const int sv = proc.soloViewEffective();
    if (sv == 1 || sv == 2) { a = 1; b = 2; } else if (sv >= 3) { a = 3; b = 4; }
    proc.analyzerNeedTB = a == 3 || a == 4 || b == 3 || b == 4;
    layerSpectrum (a, spec);
    if (b >= 0) layerSpectrum (b, specB); else std::fill (specB.begin(), specB.end(), -140.0f);
}

// One analyzer layer: 8192-point FFT of the ring(s), max per display column, +3 dB/oct tilt, slow fall.
// Stereo is the power sum of Mid and Side (= the mean of the L and R powers), so it equals Mid for centred material.
void EQAnatomyView::layerSpectrum (int src, std::vector<float>& out)
{
    const int N = (int) rings[0].size();
    auto transform = [this, N] (int k)
    {
        auto& d = bins[(size_t) k];
        for (int i = 0; i < N; ++i) d[(size_t) i] = rings[(size_t) k][(size_t) ((ringPos + i) % N)];
        std::fill (d.begin() + N, d.end(), 0.0f);
        window.multiplyWithWindowingTable (d.data(), (size_t) N);
        fft.performFrequencyOnlyForwardTransform (d.data());
    };
    // Stereo = rings 0 + 1, Mid 0, Side 1, Trans 2, Body 3, Input = rings 4 + 5 (power sum, like Stereo), Sidechain 6
    // Link (7): the other instance's latest Mid / Side, power sum, into bins 0 / 1 (A is already drawn by then)
    if (src == 7)
    {
        if (linkTap == nullptr) return;
        for (int k = 0; k < 2; ++k) std::fill (bins[(size_t) k].begin(), bins[(size_t) k].end(), 0.0f);
        linkTap->read (bins[0].data(), bins[1].data(), N);
        for (int k = 0; k < 2; ++k) { window.multiplyWithWindowingTable (bins[(size_t) k].data(), (size_t) N); fft.performFrequencyOnlyForwardTransform (bins[(size_t) k].data()); }
    }
    const bool pair = src == 0 || src == 5 || src == 7;
    const int k0 = src == 0 ? 0 : src == 1 ? 0 : src == 2 ? 1 : src == 3 ? 2 : src == 4 ? 3 : src == 5 ? 4 : src == 6 ? 6 : 0, k1 = k0 + 1;
    if (src != 7) { transform (k0); if (pair) transform (k1); }

    const double fs = proc.getSampleRateOrDefault();
    const float norm = 4.0f / (float) N;
    for (size_t i = 0; i < out.size(); ++i)
    {
        const double fq = 20.0 * std::pow (1000.0, (double) i / (out.size() - 1));
        const double fq2 = 20.0 * std::pow (1000.0, (double) (i + 1) / (out.size() - 1));
        int b0 = (int) (fq * N / fs), b1 = std::max (b0 + 1, (int) (fq2 * N / fs));
        b0 = juce::jlimit (1, N / 2 - 1, b0); b1 = juce::jlimit (b0 + 1, N / 2, b1);
        float m = 0;
        if (pair) for (int b = b0; b < b1; ++b) { const float x = bins[(size_t) k0][(size_t) b], y = bins[(size_t) k1][(size_t) b]; m = std::max (m, std::sqrt (x * x + y * y)); }
        else          for (int b = b0; b < b1; ++b) m = std::max (m, bins[(size_t) k0][(size_t) b]);
        // +3 dB/oct tilt around 1 kHz so music reads flat-ish, like most analyzers
        const float db = 20.0f * std::log10 (m * norm + 1e-9f) + 3.0f * (float) std::log2 (fq / 1000.0);
        out[i] = db > out[i] ? db : std::max (db, out[i] - 1.6f);
    }
}

void EQAnatomyView::updateHover()
{
    juce::String t, x;
    if (auto* c = juce::Desktop::getInstance().getMainMouseSource().getComponentUnderMouse())
        if (c == this || isParentOf (c))
            for (auto* k = c; k != nullptr && k != this; k = k->getParentComponent())
                if (k->getProperties().contains ("helpT"))
                {
                    t = k->getProperties()["helpT"].toString();
                    x = k->getProperties()[ph.ja() ? "helpJ" : "helpE"].toString();
                    for (auto* r = c; r != nullptr && r != this; r = r->getParentComponent())      // Option+click hint on resettable controls
                        if (r->getProperties().contains ("resettable")) { x += ph.tr ("\nOption＋クリック：初期値に戻す", "\nOption+click: back to the default value"); break; }
                    break;
                }
    // test hook (screenshots): show the help of the control whose title is in "hoverTest"
    if (const auto ht = proc.uiState.getProperty ("hoverTest", {}).toString(); t.isEmpty() && ht.isNotEmpty())
    {
        std::function<juce::Component* (juce::Component*)> find = [&] (juce::Component* p) -> juce::Component*
        { for (auto* ch : p->getChildren()) { if (ch->getProperties()["helpT"].toString() == ht) return ch; if (auto* f = find (ch)) return f; } return nullptr; };
        if (auto* k = find (this))
        {
            t = ht; x = k->getProperties()[ph.ja() ? "helpJ" : "helpE"].toString();
            if (k->getProperties().contains ("resettable")) x += ph.tr ("\nOption＋クリック：初期値に戻す", "\nOption+click: back to the default value");
        }
    }
    // the graph itself is covered by the MOUSE section that is always shown
    if (t == "MOUSE") t = {};
    ph.hoverTitle = t; ph.hoverText = x;
}

void EQAnatomyView::timerCallback()
{
    updateHover();
    pullAnalyzer();
    refreshTexts();
    graph.update (spec, specB);
    satStrip.refresh();
    if (detail.isVisible()) detail.refresh (ph.selected());
    irView.refresh();
    outDrag.repaint();
    viewSolo.repaint();
    lesson.repaint();
    io.tick();
    repaint (716, 0, 62, 44);   // LAT readout
}
