#include "PluginEditor.h"
#include "Lessons.h"
#include "Presets.h"
#include "FolderTree.h"

EQAnatomyView::EQAnatomyView (EQAnatomyProcessor& p) : proc (p)
{
    // the look is shared by every instance on this computer
    theme::setSkin ((int) ZoomPrefs::file().getIntValue ("eqa2v1.skin", 0)); lnf.apply();
    for (int i = 0; i < theme::numSkins(); ++i) zoom.skinNames.add (theme::skinName (i));
    zoom.getSkin = [] { return theme::current; };
    zoom.setSkin = [this] (int i) { applySkin (i); };
    theme::surface = juce::jlimit (0, theme::numSurfaces() - 1, ZoomPrefs::file().getIntValue ("eqa2v1.surface", 0));
    for (int i = 0; i < theme::numSurfaces(); ++i) zoom.surfaceNames.add (theme::surfaceName (i));
    zoom.getSurface = [] { return theme::surface; };
    zoom.setSurface = [this] (int i) { applySurface (i); };
    zoom.getScale = [] { return juce::jlimit (0, 4, ZoomPrefs::file().getIntValue ("eqa2v1.gscale3", 0)); };
    graph.onScale = [this] (int i) { applyScale (i); };   // the scale menu is on the graph (top left), not in the zoom menu
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
    // Panel / EAR / Learn: one of them fills the lower panel; turning the shown one off closes the panel
    earBtn.onClick  = [this] { if (proc.earActive()) { setEar (false); setLower (false); } else { earCard.setKind (0); setEar (true); } };
    learnBtn.onClick   = [this]
    {
        if ((bool) proc.uiState.getProperty ("learn", true) && ! proc.earActive() && lowerOpen()) { proc.uiState.setProperty ("learn", false, nullptr); setLower (false); return; }
        if (proc.earActive()) setEar (false);                     // EAR, Learn and Panel are exclusive
        proc.uiState.setProperty ("lesson", 0, nullptr); proc.uiState.setProperty ("learn", true, nullptr); setLower (true);
    };
    // long press: the list of every lesson
    learnBtn.onLongPress = [this]
    {
        const bool ja = ph.ja(); const bool on = (bool) proc.uiState.getProperty ("learn", true) && ! proc.earActive();
        const int cur = (int) proc.uiState.getProperty ("lesson", 0);
        juce::PopupMenu m;
        const auto& L = lessons();
        for (int i = 0; i < (int) L.size(); ++i)
            m.addItem (100 + i, juce::String (i + 1).paddedLeft ('0', 2) + "  " + juce::String::fromUTF8 (ja ? L[(size_t) i].titleJa : L[(size_t) i].titleEn), true, on && i == cur);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&learnBtn), [this] (int r)
        {
            if (r < 100) return;
            if (proc.earActive()) setEar (false);
            proc.uiState.setProperty ("lesson", r - 100, nullptr); proc.uiState.setProperty ("learn", true, nullptr); setLower (true);
        });
    };
    helpBtn.onClick    = [this]
    {
        if (lowerOpen() && ! proc.earActive() && ! (bool) proc.uiState.getProperty ("learn", true)) { setLower (false); return; }
        if (proc.earActive()) setEar (false);
        proc.uiState.setProperty ("learn", false, nullptr); setLower (true);
    };
    addAndMakeVisible (helpBtn);
    setHelp (helpBtn, "PANEL", "下段（サチュレーター・インパルス・説明欄・出力のボタン）を開き、説明欄にヘルプを出す（今の方式、マウス操作、ポインターを置いた部分の説明）。もう一度で下段を閉じ、グラフとメーターが下まで広がる。Panel・EAR・Learn はどれか1つ。どれもオフなら下段は閉じる",
             "Opens the lower panel (saturator, impulse, text panel, output buttons) with help in the text panel (the current mode, mouse controls, whatever you point at). Click again to close it; the graph and meters then take the whole height. Panel, EAR and Learn are exclusive; with all three off the lower panel is closed");
    langBtn.onClick    = [this] { proc.uiState.setProperty ("lang", ph.ja() ? "en" : "ja", nullptr); };
    resetBtn.onClick   = [this]
    {
        juce::PopupMenu m;
        const bool ja = ph.ja();
        m.addItem (1, ja ? juce::String::fromUTF8 ("Flat（初期化）") : juce::String ("Flat (reset)"));
        m.addSeparator();
        const auto& P = presets();
        const int cur = (int) proc.uiState.getProperty ("preset", -1);
        m.addSectionHeader (ja ? juce::String::fromUTF8 ("用途別") : juce::String ("By use"));
        for (int c = 0; c < 6; ++c)
        {
            juce::PopupMenu sub;
            for (int i = 0; i < (int) P.size(); ++i)
                if (P[(size_t) i].category == c)
                    sub.addItem (100 + i, juce::String::fromUTF8 (ja ? P[(size_t) i].nameJa : P[(size_t) i].nameEn), true, i == cur);
            if (sub.getNumItems() > 0) m.addSubMenu (juce::String::fromUTF8 (presetCategoryName (c, ja)), sub);
        }
        // 1.0: the analog EQ models. "Model only" switches the model (with its curve and saturator type) and keeps the rest
        m.addSectionHeader (ja ? juce::String::fromUTF8 ("アナログEQタイプ別") : juce::String ("By analog EQ type"));
        const bool anOn = ph.value (pid::anOn) > 0.5f; const int anM = (int) ph.value (pid::anModel);
        for (int k = 0; k < analog::kModels; ++k)
        {
            const auto& M = analog::models()[(size_t) k];
            juce::PopupMenu sub;
            sub.addItem (500 + k, ja ? juce::String::fromUTF8 ("モデルだけ切り替える（つまみは初期値、他はそのまま）") : juce::String ("Switch the model only (controls at defaults, the rest kept)"), true, anOn && anM == k);
            sub.addSeparator();
            for (int i = 0; i < (int) P.size(); ++i)
                if (P[(size_t) i].model == k)
                    sub.addItem (100 + i, juce::String::fromUTF8 (ja ? P[(size_t) i].nameJa : P[(size_t) i].nameEn), true, i == cur);
            m.addSubMenu (juce::String (M.name) + juce::String::fromUTF8 ("   \xe2\x80\x94 ") + analog::hint (k, ja), sub, true, nullptr, anOn && anM == k);
        }
        // user presets, mirroring the folders on disk
        const auto curFile = proc.uiState.getProperty ("userPresetFile", "").toString();
        const auto curUser = proc.uiState.getProperty ("userPreset", "").toString();
        m.addSeparator();
        m.addSectionHeader (ja ? juce::String::fromUTF8 ("ユーザープリセット") : juce::String ("User presets"));
        juce::Array<juce::File> users;
        ftree::addToMenu (m, EQAnatomyProcessor::userPresetDir(), "*.xml", users, 10000,
                          [curFile] (const juce::File& f) { return f.getFullPathName() == curFile; }, ja ? juce::String::fromUTF8 ("（空）") : juce::String ("(empty)"));
        if (users.isEmpty()) m.addItem (-1, ja ? juce::String::fromUTF8 ("（まだありません）") : juce::String ("(none yet)"), false);
        juce::PopupMenu manage;
        manage.addItem (2, ja ? juce::String::fromUTF8 ("今の設定を新しく保存…") : juce::String ("Save current settings as…"));
        manage.addItem (3, (ja ? juce::String::fromUTF8 ("上書き保存：") : juce::String ("Overwrite: ")) + (curUser.isEmpty() ? juce::String::fromUTF8 ("\xe2\x80\x94") : curUser), curUser.isNotEmpty());
        manage.addItem (4, (ja ? juce::String::fromUTF8 ("名前とメモを変更：") : juce::String ("Rename / edit note: ")) + (curUser.isEmpty() ? juce::String::fromUTF8 ("\xe2\x80\x94") : curUser), curUser.isNotEmpty());
        manage.addItem (5, (ja ? juce::String::fromUTF8 ("削除：") : juce::String ("Delete: ")) + (curUser.isEmpty() ? juce::String::fromUTF8 ("\xe2\x80\x94") : curUser), curUser.isNotEmpty());
        manage.addSeparator();
        manage.addItem (7, ja ? juce::String::fromUTF8 ("新しいフォルダを作る…") : juce::String ("New folder..."));
        manage.addItem (6, ja ? juce::String::fromUTF8 ("保存フォルダを開く（Finder でフォルダを作る・移動するとメニューにも反映）") : juce::String ("Show the preset folder (folders made or moved there show up here)"));
        m.addSubMenu (ja ? juce::String::fromUTF8 ("保存・編集") : juce::String ("Save / edit"), manage);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&resetBtn), [this, users] (int r)
        {
            if (r >= 10000 && r - 10000 < users.size()) { proc.loadUserPreset (users[r - 10000]); morph.setValue (0.0, juce::dontSendNotification); return; }
            if (r >= 500 && r < 500 + analog::kModels) { proc.undo.beginNewTransaction(); proc.applyAnalogModel (r - 500); return; }
            if (r >= 2 && r <= 7) { userPresetAction (r); return; }
            if (r == 1) proc.applyPreset (-1);
            else if (r >= 100) proc.applyPreset (r - 100);
            morph.setValue (0.0, juce::dontSendNotification);
        });
    };
    for (juce::TextButton* b : { &resBtn, (juce::TextButton*) &learnBtn, &langBtn, &resetBtn, &undoBtn, &redoBtn, &earBtn, &morphTo })
        addAndMakeVisible (*b);
    addChildComponent (curveBox); addAndMakeVisible (morph);
    // 1.0: step through the presets (factory by use, then the user files)
    prevBtn.setButtonText (juce::String::fromUTF8 ("\xe2\x97\x80")); nextBtn.setButtonText (juce::String::fromUTF8 ("\xe2\x96\xb6"));
    prevBtn.onClick = [this] { proc.stepPreset (-1); morph.setValue (0.0, juce::dontSendNotification); };
    nextBtn.onClick = [this] { proc.stepPreset (1); morph.setValue (0.0, juce::dontSendNotification); };
    for (auto* b : { &prevBtn, &nextBtn }) { b->getProperties().set ("fontPx", 9.0); addAndMakeVisible (*b); }
    setHelp (prevBtn, "PRESET", "前のプリセットへ。用途別の順、続けてユーザープリセット（フォルダ順）", "Previous preset: by use, then the user presets (folder order)");
    setHelp (nextBtn, "PRESET", "次のプリセットへ。用途別の順、続けてユーザープリセット（フォルダ順）", "Next preset: by use, then the user presets (folder order)");
    paramToggle (deltaBtn, pid::delta);
    paramToggle (bypassBtn, pid::bypass);

    addAndMakeVisible (graph); addAndMakeVisible (satStrip); addChildComponent (resStrip); addAndMakeVisible (irView);
    addAndMakeVisible (outDrag); addAndMakeVisible (viewSolo); addAndMakeVisible (lesson); addAndMakeVisible (io); addChildComponent (earCard);

    addAndMakeVisible (zoom);
    for (auto* b : { &satTab, &resTab }) { b->onSelect = [this] { resized(); repaint(); }; addAndMakeVisible (*b); }
    setHelp (satTab, "SAT", "左の丸＝サチュレーターのオン・オフ（表示を切り替えなくても操作できる）。それ以外の部分＝設定を表示", "Left circle: saturator on / off (no need to switch the view). Rest of the tab: show its controls");
    setHelp (resTab, "RES", "左の丸＝レゾナンス・サプレッサーのオン・オフ。それ以外の部分＝設定を表示。信号の流れは EQ → RES → SAT", "Left circle: resonance suppressor on / off. Rest of the tab: show its controls. Signal flow: EQ → RES → SAT");
    setHelp (modeBox, "MODE", "処理の方式。Zero latency：全体の既定。Min のバンドはデジタル IIR（遅れゼロ、位相が回る、ナイキスト付近は形が詰まる）。Lin にしたバンドだけ FIR になり、そのときは全体が FIR 分遅れる。DYN と TRANSIENT/BODY はこのモードの Min バンドだけ／Analog：Min のバンドを、アナログ回路と同じ振幅と位相の最小位相 FIR で処理。高域も形が崩れない。Lin のバンドがなければレイテンシーなし／Linear：全バンドをリニア位相にする（バンドごとの Min/Lin は無視）。位相は回らないが、全体が遅れ、アタックの前にプリリンギングが出る",
             "Processing mode. Zero latency: the default. Min bands run as digital IIR (no delay, phase turns, cramping near Nyquist); bands set to Lin become FIR, which delays everything. DYN and TRANSIENT/BODY work on Min bands in this mode only / Analog: Min bands become a minimum-phase FIR with the analog magnitude and phase; highs keep their shape; no latency unless a band is Lin / Linear: all bands linear phase (per-band Min/Lin is ignored); no phase shift, but everything is delayed and pre-ringing appears before the attack");
    setHelp (resBtn, "RESOLUTION", "リニア位相の FIR の長さ（Low / Mid / High）。長いほど低域が正確だが、レイテンシーとプリリンギングも伸びる", "Linear-phase FIR length. Longer is more accurate in the lows, with more latency and pre-ringing");
    setHelp (curveBox, "CURVE", "全バンドのカーブの性格。Clean Const／Clean Prop：従来の一定 Q と比例 Q／70s British Inductor：広いベルと肩の盛り上がり／American Discrete：強い比例 Q／80s British Console：比例寄りのベルとくっきりしたシェルフ／Vintage Tube：ブーストとカットが干渉する低域／Precision：色付けのない一定 Q。注意：カーブの形の傾向だけで機材の音そのものではない。質感まで比べるなら SAT の Transformer・Tube・Class A と組み合わせる。名称は一般名",
             "Curve character for all bands. Clean Const/Prop: the original constant and proportional Q / 70s British Inductor: broad bells, shelf shoulders / American Discrete: strongly proportional / 80s British Console: mildly proportional bells, crisp shelves / Vintage Tube: interacting low boost and cut / Precision: uncoloured constant Q. Careful: shape tendencies only, not the sound of the hardware; pair with SAT Transformer / Tube / Class A for texture. Generic names");
    setHelp (resetBtn, "PRESET", "作業テンプレート。読み込むと、目的・まず動かす所・注意点がこの欄に出て、動かすバンドが光る。比較用のプリセットは A〜D スロットにも設定が入る。Flat で初期化（Undo で戻せる）。\n| メニュー | 中身 |\n| 用途別 | 掃除・音色づくり・バスとマスタリングなど |\n| アナログEQタイプ別 | 9 モデルごとのプリセットと「モデルだけ切り替える」。全バンドのカーブと SAT のタイプがモデルに合わせて変わる |\n| ユーザープリセット | 保存フォルダの階層がそのままメニューになる（Finder でフォルダを作る・移動しても反映） |\n◀ ▶ で順に送る。注意：数値は出発点。必ず耳で合わせる",
          "Task templates: loading one shows its purpose, the first control to move and a caution here, and highlights the band. Comparison presets also fill slots A–D. Flat resets (undoable).\n| Menu | Holds |\n| By use | cleanup, tone, bus and mastering and so on |\n| By analog EQ type | presets for each of the 9 models, and 'switch the model only'; every band's curve and the SAT type follow the model |\n| User presets | the folders in the preset folder become the menu (folders made or moved in the Finder show up) |\n< > step through them. Careful: values are starting points; always adjust by ear");
    setHelp (undoBtn, "UNDO", "ひとつ前の操作に戻す（Cmd＋Z）", "Undo the last change (Cmd + Z)");
    setHelp (redoBtn, "REDO", "戻した操作をやり直す（Shift＋Cmd＋Z）", "Redo (Shift + Cmd + Z)");
    setHelp (morphTo, "MORPH TARGET", "モーフの行き先のスロット。クリックで切り替え", "The slot the morph slider heads for. Click to change");
    setHelp (morph, "MORPH", "今のスロットから行き先のスロットへ連続で変える。周波数と Q は対数、ゲインは dB で補間。ダブルクリックで 0。注意：タイプや Min/Lin、M/S のように連続で変えられないものは 50% で切り替わる。Lin のバンドは IR を作り直すため少し遅れて追従する。動かしたままスロットを切り替えると、その状態が今のスロットに保存される",
             "Morph from the current slot to the target. Frequency and Q interpolate logarithmically, gain in dB. Double-click resets. Careful: type, Min/Lin and M/S switch at 50%; Lin bands follow with a short delay while the IR rebuilds; switching slots stores the morphed state in the current slot");
    setHelp (earBtn, "EAR", "耳トレモード。クリックで種類を選ぶ：EQ の帯域当て／プリリンギングの聴き分け（同じカットを最小位相とリニア位相で鳴らし、音の前ににじむ方を当てる。出題中はレイテンシーが 4096 サンプル増える）。Learn とは同時に使えない。注意：出題中は今の EQ を一時的に外す（終了すると戻る）", "Ear training. Click to choose: guess the EQ band, or hear the pre-ringing (the same cut in minimum and linear phase; pick the one that smears before the attack; adds 4096 samples of latency while it plays). Not at the same time as Learn. Careful: your EQ is bypassed while playing (restored when you leave)");
    setHelp (learnBtn, "LEARN", "クリック：レッスン 1 を表示（もう一度でヘルプへ）。長押し：全レッスンの一覧から選ぶ。クリックでレッスン一覧（全タイトル）。選ぶとそのレッスンを表示。EAR とは同時に使えない（片方を選ぶともう片方は閉じる）。「レッスンを閉じる」にすると、この欄に方式とマウス操作、ポインターを置いた部分の説明が出る", "Click: lesson 1 (again: back to help). Long press: the list of every lesson. Not at the same time as EAR (choosing one closes the other). \"Close lessons\" shows the mode, mouse controls and help for whatever you point at here");
    setHelp (langBtn, "LANGUAGE", "説明とレッスンの言語（日本語 / English）", "Language of help and lessons");
    setHelp (zoom, "SIZE / LOOK", "画面の大きさ、外観（10種類）、隙間の質感（6種類）。大きさは右下の角のドラッグでも変えられる。外観と質感はこの Mac のすべてのインスタンスで共通", "Window size, look (ten skins) and surface texture for the gaps (six). Drag the bottom-right corner to resize. Look and surface are shared by every instance on this computer");
    setHelp (deltaBtn, "DELTA", "EQ で変わった成分だけを聴く（出力 − 原音）。グラフに赤い破線で |H − 1| を表示。注意：位相が 60° 以上回る帯域や +6 dB を超えるブーストでは、原音より大きく聴こえる（異常ではない）",
             "Hear only what the EQ changed (output minus input); the red dashed line shows |H − 1|. Careful: where the phase turns more than 60°, or boosts exceed +6 dB, it is louder than the input — that is expected");
    setHelp (bypassBtn, "BYPASS", "EQ を通さない原音（レイテンシーはそろえたまま）", "The unprocessed signal (latency kept aligned)");
    addMouseListener (this, true);
    setWantsKeyboardFocus (true);
    for (auto* p : proc.getParameters()) p->addListener (this);
    setSize (1160, 760);
    refreshTexts();
    startTimerHz (30);
}

void EQAnatomyView::parameterGestureChanged (int index, bool starting)
{
    if (! starting || ! juce::MessageManager::getInstance()->isThisTheMessageThread()) return;
    if (auto* p = dynamic_cast<juce::RangedAudioParameter*> (proc.getParameters()[index])) ph.lastId = p->paramID;
}

EQAnatomyView::~EQAnatomyView()
{
    for (auto* p : proc.getParameters()) p->removeListener (this);
    if (proc.earActive()) earCard.stop();
    proc.soloBand = -1;
    proc.soloView = 0; proc.analyzerNeedTB = false; proc.scopeNeed = false;      // no hidden solo state once the window is gone (bounce safety)
    removeMouseListener (this);
    juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
    setLookAndFeel (nullptr);
}

void EQAnatomyView::mouseDown (const juce::MouseEvent& e)
{
    proc.undo.beginNewTransaction();     // every click starts a new undo step
    // the arrow keys belong to the editor: menus, sliders and buttons never keep the keyboard focus
    std::function<void (juce::Component&)> noFocus = [&] (juce::Component& c)
    {
        for (auto* ch : c.getChildren())
        {
            if (dynamic_cast<juce::TextEditor*> (ch) != nullptr) continue;
            if (dynamic_cast<juce::ComboBox*> (ch) != nullptr || dynamic_cast<juce::Slider*> (ch) != nullptr || dynamic_cast<juce::Button*> (ch) != nullptr)
            { ch->setWantsKeyboardFocus (false); ch->setMouseClickGrabsKeyboardFocus (false); }
            noFocus (*ch);
        }
    };
    noFocus (*this);
    if (dynamic_cast<juce::TextEditor*> (e.originalComponent) == nullptr) grabKeyboardFocus();
}

void EQAnatomyView::applySkin (int i)
{
    theme::setSkin (i);
    ZoomPrefs::file().setValue ("eqa2v1.skin", i); ZoomPrefs::file().saveIfNeeded();
    lnf.apply();
    sendLookAndFeelChange();
    refreshTexts();
    repaint();
}

void EQAnatomyView::applySurface (int i)
{
    theme::surface = juce::jlimit (0, theme::numSurfaces() - 1, i);
    ZoomPrefs::file().setValue ("eqa2v1.surface", theme::surface); ZoomPrefs::file().saveIfNeeded();
    repaint();
}

void EQAnatomyView::applyScale (int i)
{
    i = juce::jlimit (0, 4, i);
    ZoomPrefs::file().setValue ("eqa2v1.gscale3", i); ZoomPrefs::file().saveIfNeeded();
    static const float r[] = { 3.0f, 6.0f, 12.0f, 20.0f, 30.0f };
    graph.setDbRange (r[i]);
    repaint();
}

// user presets: save as / overwrite / rename + note / delete / show folder
void EQAnatomyView::userPresetAction (int r)
{
    const bool ja = ph.ja();
    const auto cur = proc.uiState.getProperty ("userPreset", "").toString();
    const auto note = proc.uiState.getProperty ("userPresetNote", "").toString();
    const auto root = EQAnatomyProcessor::userPresetDir(); root.createDirectory();
    juce::File curFile (proc.uiState.getProperty ("userPresetFile", "").toString());
    if (! curFile.existsAsFile() && cur.isNotEmpty()) curFile = root.getChildFile (juce::File::createLegalFileName (cur) + ".xml");
    if (r == 6) { root.revealToUser(); return; }
    if (r == 3) { proc.saveUserPreset (curFile.getParentDirectory(), cur, note); return; }
    if (r == 7)
    {
        auto* w = new juce::AlertWindow (ja ? juce::String::fromUTF8 ("新しいフォルダ") : juce::String ("New folder"),
                                         ja ? juce::String::fromUTF8 ("場所と名前。A/B と書くと階層で作る") : juce::String ("Where, and a name. A/B makes levels"), juce::MessageBoxIconType::NoIcon, this);
        ftree::addFolderFields (*w, root, ftree::relFolder (root, curFile), ja);
        w->addButton ("OK", 1, juce::KeyPress (juce::KeyPress::returnKey));
        w->addButton (ja ? juce::String::fromUTF8 ("キャンセル") : juce::String ("Cancel"), 0, juce::KeyPress (juce::KeyPress::escapeKey));
        w->enterModalState (true, juce::ModalCallbackFunction::create ([w, root] (int b) { if (b == 1) ftree::chosenFolder (*w, root); }), true);
        return;
    }
    if (r == 5)
    {
        auto opts = juce::MessageBoxOptions().withIconType (juce::MessageBoxIconType::WarningIcon)
                        .withTitle (ja ? juce::String::fromUTF8 ("プリセットを削除") : juce::String ("Delete preset"))
                        .withMessage ((ja ? juce::String::fromUTF8 ("ゴミ箱に移します：") : juce::String ("Move to the Trash: ")) + cur)
                        .withButton (ja ? juce::String::fromUTF8 ("削除") : juce::String ("Delete")).withButton (ja ? juce::String::fromUTF8 ("キャンセル") : juce::String ("Cancel"))
                        .withAssociatedComponent (this);
        juce::AlertWindow::showAsync (opts, [this, curFile] (int b)
        {
            if (b != 1) return;
            curFile.moveToTrash();
            proc.uiState.setProperty ("userPresetFile", juce::String(), nullptr);
            proc.uiState.setProperty ("userPreset", juce::String(), nullptr); proc.uiState.setProperty ("userPresetNote", juce::String(), nullptr);
        });
        return;
    }
    // 2 = save as, 4 = rename / edit note: ask for a name and a note
    auto* w = new juce::AlertWindow (r == 2 ? (ja ? juce::String::fromUTF8 ("ユーザープリセットを保存") : juce::String ("Save user preset"))
                                            : (ja ? juce::String::fromUTF8 ("名前とメモを変更") : juce::String ("Rename / edit note")),
                                     ja ? juce::String::fromUTF8 ("名前と、用途のメモ（説明欄に出る）") : juce::String ("A name, and a note on what it is for (shown in the text panel)"),
                                     juce::MessageBoxIconType::NoIcon, this);
    w->addTextEditor ("name", r == 2 ? juce::String() : cur, ja ? juce::String::fromUTF8 ("名前") : juce::String ("Name"));
    w->addTextEditor ("note", r == 2 ? juce::String() : note, ja ? juce::String::fromUTF8 ("メモ") : juce::String ("Note"));
    ftree::addFolderFields (*w, root, ftree::relFolder (root, curFile), ja);
    w->addButton ("OK", 1, juce::KeyPress (juce::KeyPress::returnKey));
    w->addButton (ja ? juce::String::fromUTF8 ("キャンセル") : juce::String ("Cancel"), 0, juce::KeyPress (juce::KeyPress::escapeKey));
    w->enterModalState (true, juce::ModalCallbackFunction::create ([this, w, r, curFile, root] (int b)
    {
        if (b != 1) return;
        const auto name = w->getTextEditorContents ("name").trim(), nt = w->getTextEditorContents ("note");
        if (name.isEmpty()) return;
        const auto dir = ftree::chosenFolder (*w, root);
        auto dst = dir.getChildFile (juce::File::createLegalFileName (name) + ".xml");
        if (r == 4 && curFile.existsAsFile() && curFile != dst) curFile.moveFileTo (dst);      // rename and / or move to another folder
        proc.saveUserPreset (dir, name, nt);
        proc.uiState.setProperty ("userPresetNote", nt, nullptr);
    }), true);
}

bool EQAnatomyView::lowerOpen() const { return (bool) proc.uiState.getProperty ("lowerOpen", true); }
void EQAnatomyView::setLower (bool on) { proc.uiState.setProperty ("lowerOpen", on, nullptr); resized(); repaint(); }

void EQAnatomyView::setEar (bool on)
{
    if (on) proc.uiState.setProperty ("learn", false, nullptr);   // EAR and Learn are exclusive
    if (on) proc.uiState.setProperty ("lowerOpen", true, nullptr);   // the quiz lives in the lower panel
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
    earBtn.setButtonText ("EAR"); earBtn.setToggleState (proc.earActive(), juce::dontSendNotification);
    learnBtn.setButtonText ("Learn");
    helpBtn.setButtonText ("Panel"); helpBtn.setClickingTogglesState (false);
    helpBtn.setToggleState (lowerOpen() && ! (bool) proc.uiState.getProperty ("learn", true) && ! proc.earActive(), juce::dontSendNotification);
    learnBtn.setToggleState (lowerOpen() && (bool) proc.uiState.getProperty ("learn", true) && ! proc.earActive(), juce::dontSendNotification);
    langBtn.setButtonText (ph.ja() ? "EN" : juce::String::fromUTF8 ("日本語"));
    {
        const int pi = (int) proc.uiState.getProperty ("preset", -1);
        const auto up = proc.uiState.getProperty ("userPreset", "").toString();
        resetBtn.setButtonText (pi >= 0 && pi < (int) presets().size() ? juce::String::fromUTF8 (ph.ja() ? presets()[(size_t) pi].nameJa : presets()[(size_t) pi].nameEn)
                                : up.isNotEmpty() ? juce::String::fromUTF8 ("★ ") + up : juce::String::fromUTF8 ("\xe2\x80\x94"));
        resetBtn.getProperties().set ("chevron", 1);
    }
    deltaBtn.setButtonText ("Delta");
    bypassBtn.setButtonText ("Bypass");
    deltaBtn.setToggleState (ph.value (pid::delta) > 0.5f, juce::dontSendNotification);
    bypassBtn.setToggleState (ph.value (pid::bypass) > 0.5f, juce::dontSendNotification);
    juce::ignoreUnused (anyLin);
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

    const int lat = proc.currentLatency();
    const double fs = proc.getSampleRateOrDefault();
    g.setFont (theme::mono (10.0f));
    g.setColour (lat > 0 ? theme::pre : theme::muted);
    const juce::String latTxt = "LAT " + (lat > 0 ? juce::String (lat * 1000.0 / fs, 1) + " ms" : juce::String ("0 ms"));
    if (latRect.isEmpty() || ! outDrag.isVisible()) g.drawText (latTxt, graph.getRight() - 64, graph.getBottom() + 2, 64, 12, juce::Justification::centredRight);
    else
    {
        g.setColour (theme::well); g.fillRoundedRectangle (latRect.toFloat(), 4.0f);
        g.setColour (theme::line); g.drawRoundedRectangle (latRect.toFloat().reduced (0.5f), 4.0f, 1.0f);
        g.setColour (theme::muted); g.setFont (theme::ui (9.0f)); g.drawText (ph.tr ("レイテンシー", "Latency"), latRect.withTrimmedLeft (6), juce::Justification::centredLeft);
        g.setColour (lat > 0 ? theme::pre : theme::text); g.setFont (theme::mono (10.5f, true));
        g.drawText (lat > 0 ? juce::String (lat * 1000.0 / fs, 1) + " ms" : juce::String ("0 ms"), latRect.withTrimmedRight (6), juce::Justification::centredRight);
    }
    // the preset group's frame (preset, slots, morph belong together)
    g.setColour (theme::panel); g.fillRoundedRectangle (presetGroup.toFloat(), 6.0f);
    g.setColour (theme::line.brighter (0.25f)); g.drawRoundedRectangle (presetGroup.toFloat().reduced (0.5f), 6.0f, 1.0f);
    g.setColour (theme::muted); g.setFont (theme::ui (8.0f));
    g.drawText ("MORPH", morph.getX(), 3, 60, 9, juce::Justification::centredLeft);
    // captions for the two menus whose text is a name (curve character, preset)
    g.setColour (theme::muted); g.setFont (theme::ui (8.5f, true));
    g.setColour (theme::text); g.setFont (theme::ui (9.5f, true));
    g.drawText ("PRESET", prevBtn.getX() - 46, 10, 43, 24, juce::Justification::centredRight);
    g.setColour (theme::accent); g.drawRoundedRectangle (resetBtn.getBounds().toFloat().expanded (1.5f), 5.0f, 1.5f);
}

void EQAnatomyView::resized()
{
    zoom.setBounds (104, 12, 42, 20);
    undoBtn.setBounds (150, 11, 26, 22); redoBtn.setBounds (178, 11, 26, 22);
    int x = 210;
    modeBox.setBounds (x, 10, 112, 24); x += 116;
    resBtn.setBounds (x, 10, 60, 24); x += 64;
    curveBox.setBounds (0, 0, 0, 0);       // 1.0: per band, in the floating panel
    // the preset group, right-aligned in its own frame: PRESET caption, preset menu, slots A–D, morph target, MORPH
    {
        const int right = 958;
        morph.setBounds (right - 50, 12, 48, 24);
        morphTo.setBounds (right - 84, 12, 30, 20);
        for (int i = 0; i < 4; ++i) slotBtn[i].setBounds (right - 194 + i * 26, 10, 24, 24);
        nextBtn.setBounds (right - 194 - 8 - 22, 11, 22, 22);
        resetBtn.setBounds (nextBtn.getX() - 2 - 170, 10, 170, 24);
        prevBtn.setBounds (resetBtn.getX() - 24, 11, 22, 22);
        presetGroup = { prevBtn.getX() - 50, 4, right + 4 - (prevBtn.getX() - 50), 38 };
    }
    helpBtn.setBounds (970, 10, 42, 24);
    earBtn.setBounds (1016, 10, 40, 24);
    learnBtn.setBounds (1060, 10, 48, 24);
    langBtn.setBounds (1112, 10, 36, 24);

    // Lab 2: the lower panel can be closed; closed, the graph and meters take the whole height
    const bool lower = (bool) proc.uiState.getProperty ("lowerOpen", true);
    const int y = 600, h = 148;
    const int gh = lower ? y - 8 - 54 : getHeight() - 12 - 54;
    graph.setBounds (12, 54, 1076, gh);
    io.setBounds (1096, 54, 52, gh);
    const bool ear = proc.earActive();
    satStrip.setBounds (12, y, 180, h); resStrip.setBounds (12, y, 180, h);
    {
        const bool rsTab = (int) proc.uiState.getProperty ("postTab", 0) == 1;
        satTab.setBounds (8 + 12, y + 5, 78, 20); resTab.setBounds (8 + 12 + 82, y + 5, 78, 20);
        satTab.setVisible (! ear && lower); resTab.setVisible (! ear && lower);
        satStrip.setVisible (! ear && lower && ! rsTab); resStrip.setVisible (! ear && lower && rsTab);
    }
    for (auto* c : { (juce::Component*) &irView, (juce::Component*) &outDrag,
                     (juce::Component*) &deltaBtn, (juce::Component*) &viewSolo, (juce::Component*) &bypassBtn, (juce::Component*) &lesson,
                     (juce::Component*) &satStrip })
        c->setVisible (! ear && lower);
    {
        const bool rsTab = (int) proc.uiState.getProperty ("postTab", 0) == 1;
        satStrip.setVisible (! ear && lower && ! rsTab); resStrip.setVisible (! ear && lower && rsTab);
    }
    earCard.setVisible (ear && lower);
    earCard.setBounds (12, y, getWidth() - 24, h);
    irView.setBounds (200, y, 120, h);
    // the button column, packed to the bottom. Top to bottom: read-only first, the most-pressed last, nearest the corner:
    // latency (info) / SOLO (what to hear) / OUT | Auto gain (level match) / Delta | Bypass (the before-after check)
    const int bw = 104, bx = getWidth() - 12 - bw, bh = (h - 16) / 5, gap = 4;
    auto row = [&] (int k) { return juce::Rectangle<int> (bx, y + h - (4 - k) * bh - (3 - k) * gap, bw, bh); };
    latRect = row (0);
    viewSolo.setBounds (row (1));
    outDrag.setBounds (row (2));
    { auto r = row (3); deltaBtn.setBounds (r.removeFromLeft (bw / 2 - 2)); r.removeFromLeft (4); bypassBtn.setBounds (r); }
    lesson.setBounds (328, y, bx - 8 - 328, h);
}

bool EQAnatomyView::keyPressed (const juce::KeyPress& k)
{
    const auto c = k.getTextCharacter();
    if (k == juce::KeyPress ('z', juce::ModifierKeys::commandModifier, 0)) { proc.undo.undo(); return true; }
    if (k == juce::KeyPress ('z', juce::ModifierKeys::commandModifier | juce::ModifierKeys::shiftModifier, 0)) { proc.undo.redo(); return true; }
    if (c >= '1' && c <= '0' + eqcore::kBands) { ph.select (c - '1'); return true; }
    // ↑ / ↓: step the parameter last touched (Shift: finer). Hz and ms in ratios, dB in 0.5 / 0.1, choices one at a time
    const int kc = k.getKeyCode();
    if (kc == juce::KeyPress::upKey || kc == juce::KeyPress::downKey || kc == juce::KeyPress::leftKey || kc == juce::KeyPress::rightKey)
    {
        juce::String id = ph.lastId.isNotEmpty() ? ph.lastId : pid::band (ph.selected(), "gain");
        const bool vert = kc == juce::KeyPress::upKey || kc == juce::KeyPress::downKey;
        // a band dot (freq / gain) and a RES node (f / g): up/down = height (dB), left/right = frequency
        const auto stem = id.upToLastOccurrenceOf ("_", true, false), leaf = id.fromLastOccurrenceOf ("_", false, false);
        const bool dot = id.startsWithChar ('b') && (leaf == "freq" || leaf == "gain");
        const bool node = id.startsWith ("rs_n") && (leaf == "f" || leaf == "g");
        if (dot) id = stem + (vert ? "gain" : "freq");
        else if (node) id = stem + (vert ? "g" : "f");
        else if (! vert) return false;              // left/right only move dots and nodes
        auto* p = ph.get (id); if (p == nullptr) return false;
        const float dir = (kc == juce::KeyPress::upKey || kc == juce::KeyPress::rightKey) ? 1.0f : -1.0f;
        const bool fine = k.getModifiers().isShiftDown();
        const auto lab = p->getLabel(); const float v = ph.value (id); const auto rg = p->getNormalisableRange();
        float nv;
        if (dynamic_cast<juce::AudioParameterBool*> (p) != nullptr) nv = dir > 0 ? 1.0f : 0.0f;     // on / off
        else if (auto* c = dynamic_cast<juce::AudioParameterChoice*> (p)) nv = (float) juce::jlimit (0, c->choices.size() - 1, c->getIndex() + (int) dir);
        else if ((lab == "Hz" || id.endsWith ("_freq") || (node && ! vert)) && rg.start > 0) nv = v * std::pow (2.0f, dir * (fine ? 1.0f / 48.0f : 1.0f / 12.0f));
        else if ((lab == "dB" || id.endsWith ("_gain") || (node && vert))) nv = v + dir * (fine ? 0.1f : 0.5f);
        else if ((lab == "ms" || lab.isEmpty()) && rg.start > 0) nv = v * std::pow (fine ? 1.02f : 1.1f, dir);
        else if (lab == "dB") nv = v + dir * (fine ? 0.1f : 0.5f);
        else nv = v + dir * (fine ? 0.2f : 1.0f) * std::max (0.01f, (rg.end - rg.start) / 100.0f);
        ph.setGesture (id, rg.snapToLegalValue (juce::jlimit (rg.start, rg.end, nv)));
        return true;
    }
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
    proc.analyzerNeedTB = a == 3 || a == 4 || b == 3 || b == 4;
    layerSpectrum (a, spec);
    if (b >= 0) layerSpectrum (b, specB); else std::fill (specB.begin(), specB.end(), -140.0f);
    if (graph.fieldWanted()) fieldUpdate();
}

// Stereo field per band. L = M + S, R = M - S, so per band
//   width % = 200 · Ps / (Pm + Ps)               (0 mono, 100 uncorrelated L / R, 200 out of phase)
//   corr    = (Pm − Ps) / √((Pm + Ps)² − 4X²)    with X = Σ Re(M · S*)
// Complex FFT (8192, Hann) of the input M / S (rings 4, 5) and the output M / S (rings 0, 1), powers smoothed over ~0.3 s.
void EQAnatomyView::fieldUpdate()
{
    const int N = (int) rings[0].size();
    const double fs = proc.getSampleRateOrDefault();
    auto& F = graph.fieldF;
    if (F.empty())
    {
        for (double f = frange::lo * std::pow (2.0, 1.0 / 12.0); f < frange::hi; f *= std::pow (2.0, 1.0 / 6.0)) F.push_back (f);   // from 10 Hz, like the axis (below ~40 Hz a band is one or two FFT bins)
        for (int k = 0; k < 2; ++k) { fPM[(size_t) k].assign (F.size(), 0.0); fPS[(size_t) k].assign (F.size(), 0.0); fX[(size_t) k].assign (F.size(), 0.0); fY[(size_t) k].assign (F.size(), 0.0);
                                      graph.fieldW[(size_t) k].assign (F.size(), std::numeric_limits<float>::quiet_NaN()); graph.fieldC[(size_t) k].assign (F.size(), std::numeric_limits<float>::quiet_NaN()); graph.fieldP[(size_t) k].assign (F.size(), std::numeric_limits<float>::quiet_NaN()); }
    }
    auto load = [&] (int ring, std::vector<float>& d)
    {
        for (int i = 0; i < N; ++i) d[(size_t) i] = rings[(size_t) ring][(size_t) ((ringPos + i) % N)];
        std::fill (d.begin() + N, d.end(), 0.0f);
        window.multiplyWithWindowingTable (d.data(), (size_t) N);
        fft.performRealOnlyForwardTransform (d.data(), true);      // interleaved re / im, bins 0 .. N/2
    };
    const double a = 0.2;               // ~0.3 s at the 30 Hz refresh
    const double floorP = std::pow (10.0, -140.0 / 10.0) * (N * N / 16.0);  // about −140 dBFS per band: only silence is left out
    for (int k = 0; k < 2; ++k)
    {
        auto& m = bins[0]; auto& s = bins[1];
        load (k == 0 ? 4 : 0, m); load (k == 0 ? 5 : 1, s);
        for (size_t i = 0; i < F.size(); ++i)
        {
            const double lo = F[i] * std::pow (2.0, -1.0 / 12.0), hi = F[i] * std::pow (2.0, 1.0 / 12.0);
            int b0 = juce::jlimit (1, N / 2 - 1, (int) std::ceil (lo * N / fs)), b1 = juce::jlimit (b0, N / 2 - 1, (int) std::floor (hi * N / fs));
            double pm = 0, ps = 0, x = 0, yy = 0;
            for (int b = b0; b <= b1; ++b)
            {
                const double mr = m[(size_t) (2 * b)], mi = m[(size_t) (2 * b + 1)], sr = s[(size_t) (2 * b)], si = s[(size_t) (2 * b + 1)];
                pm += mr * mr + mi * mi; ps += sr * sr + si * si; x += mr * sr + mi * si; yy += si * mr - sr * mi;
            }
            const double n = (double) (b1 - b0 + 1);
            pm /= n; ps /= n; x /= n; yy /= n;
            auto& PM = fPM[(size_t) k][i]; auto& PS = fPS[(size_t) k][i]; auto& X = fX[(size_t) k][i]; auto& Y = fY[(size_t) k][i];
            PM += a * (pm - PM); PS += a * (ps - PS); X += a * (x - X); Y += a * (yy - Y);
            const double t = PM + PS;
            if (t < floorP) { graph.fieldW[(size_t) k][i] = graph.fieldC[(size_t) k][i] = graph.fieldP[(size_t) k][i] = std::numeric_limits<float>::quiet_NaN(); continue; }
            // balance = (PL - PR) / (PL + PR) = 2X / (Pm + Ps); spread = (1 - corr) / 2
            const double corr = juce::jlimit (-1.0, 1.0, (PM - PS) / std::sqrt (std::max (1e-30, t * t - 4.0 * X * X)));
            graph.fieldC[(size_t) k][i] = (float) corr;
            // L / R levels of the band (L = M + S, R = M − S), scaled like the analyzer (+3 dB/oct around 1 kHz)
            const double sc = 20.0 * std::log10 (4.0 / N) + 3.0 * std::log2 (F[i] / 1000.0);
            graph.fieldW[(size_t) k][i] = (float) (10.0 * std::log10 (std::max (1e-20, t + 2.0 * X)) + sc);
            graph.fieldP[(size_t) k][i] = (float) (10.0 * std::log10 (std::max (1e-20, t - 2.0 * X)) + sc);
        }
    }
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
        const double fq = frange::lo * std::pow (frange::ratio, (double) i / (out.size() - 1));
        const double fq2 = frange::lo * std::pow (frange::ratio, (double) (i + 1) / (out.size() - 1));
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
    bool region = false;
    if (auto* c = juce::Desktop::getInstance().getMainMouseSource().getComponentUnderMouse())
        if (auto* hh = dynamic_cast<HoverHelp*> (c); hh != nullptr && isParentOf (c))
            region = hh->hoverHelp (c->getLocalPoint (nullptr, juce::Desktop::getInstance().getMainMouseSource().getScreenPosition()), t, x);
    if (! region)
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
    // test hook (screenshots): "graph:x,y" or "io:x,y" shows the region help at that point
    if (const auto hp = proc.uiState.getProperty ("hoverPt", {}).toString(); hp.isNotEmpty())
    {
        juce::StringArray v; v.addTokens (hp.fromFirstOccurrenceOf (":", false, false), ",;", "");
        const juce::Point<float> pt (v[0].getFloatValue(), v[1].getFloatValue());
        if (hp.startsWith ("graph")) graph.hoverHelp (pt, t, x); else io.hoverHelp (pt, t, x);
    }
    // the graph itself is covered by the MOUSE section that is always shown
    if (t == "MOUSE") t = {};
    ph.hoverTitle = t; ph.hoverText = x;
}

void EQAnatomyView::timerCallback()
{
    // 60 fps while something on the graph is dragged (the curve and the dot move smoothly), 30 fps otherwise
    if (const int ms = graph.isDragging() ? 16 : 33; getTimerInterval() != ms) startTimer (ms);
    updateHover();
    pullAnalyzer();
    refreshTexts();
    graph.update (spec, specB);
    satStrip.refresh(); resStrip.refresh();
    // the practice loop belongs to Learn: it stops when Learn is no longer shown
    if (proc.sampleSrc.load() >= 0 && ! (lowerOpen() && (bool) proc.uiState.getProperty ("learn", true) && ! proc.earActive())) { proc.sampleSrc = -1; lesson.refreshTexts(); }
    {
        const bool rsTab = (int) proc.uiState.getProperty ("postTab", 0) == 1;
        satTab.repaint(); resTab.repaint(); juce::ignoreUnused (rsTab);
    }
    irView.refresh();
    outDrag.repaint();
    viewSolo.repaint();
    lesson.repaint();
    io.tick();
    repaint (latRect); repaint (graph.getRight() - 70, graph.getBottom(), 72, 16);   // LAT readout
}
