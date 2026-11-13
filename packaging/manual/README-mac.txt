EQ Anatomy Lab 2 v1  — Mac 手動インストール / Manual install
by Johan Plus Kei

【日本語】
対応：macOS 11 以降（Apple Silicon / Intel）

1. Finder で option キーを押しながら「移動」メニューを開き「ライブラリ」を選ぶ。
2. 次のフォルダへドラッグでコピー（フォルダが無ければ作成）：
   ・EQ Anatomy Lab 2 v1.component → ライブラリ/Audio/Plug-Ins/Components
   ・EQ Anatomy Lab 2 v1.vst3      → ライブラリ/Audio/Plug-Ins/VST3
   ・EQ Anatomy Lab 2 v1.app       → アプリケーション（単体で使う場合のみ）
3. ターミナルを開き、次の1行を貼り付けて Enter（未公証アプリの検疫フラグを外す）：

xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/Components/"EQ Anatomy Lab 2 v1.component" ~/Library/Audio/Plug-Ins/VST3/"EQ Anatomy Lab 2 v1.vst3" /Applications/"EQ Anatomy Lab 2 v1.app"

   （.app を入れていない場合は「No such file」と出ますが問題ありません）
4. DAW を再起動してプラグインを再スキャン。Live は「設定 › プラグイン › 再スキャン」。
   AU が出ない時はターミナルで killall -9 AudioComponentRegistrar を実行してから再スキャン。

手順3を飛ばすと「壊れている」「開発元を確認できません」と出て読み込めません。
ターミナルを使いたくない場合は、Releases ページの .pkg インストーラー版を使ってください。

アンインストール：上記3か所から削除するだけです。

【English】
Requires macOS 11+ (Apple Silicon / Intel)

1. In Finder, hold Option and open the Go menu, then choose Library.
2. Copy (drag) the files to these folders (create them if missing):
   - EQ Anatomy Lab 2 v1.component -> Library/Audio/Plug-Ins/Components
   - EQ Anatomy Lab 2 v1.vst3      -> Library/Audio/Plug-Ins/VST3
   - EQ Anatomy Lab 2 v1.app       -> Applications (optional, standalone)
3. Open Terminal, paste this line and press Return (removes the quarantine flag
   from this un-notarized build):

xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/Components/"EQ Anatomy Lab 2 v1.component" ~/Library/Audio/Plug-Ins/VST3/"EQ Anatomy Lab 2 v1.vst3" /Applications/"EQ Anatomy Lab 2 v1.app"

   ("No such file" for the .app is fine if you skipped it.)
4. Restart your DAW and rescan plug-ins. If the AU doesn't show up, run
   killall -9 AudioComponentRegistrar in Terminal and rescan.

If you skip step 3, macOS will say the plug-in is "damaged" or "cannot be verified".
Prefer not to use Terminal? Use the .pkg installer from the Releases page instead.

Uninstall: delete the files from the folders above.
