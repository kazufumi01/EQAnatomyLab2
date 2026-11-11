# EQ Anatomy Lab 2 β — インストール方法 / Installation

> **ベータ版です。** Mac の Ableton Live で動作確認済み。
> **Windows 版は今のところありません。**
> **Beta.** Tested with Ableton Live on Mac.
> **There is no Windows build yet.**
> 不具合は / Problems: [Issues](https://github.com/kazufumi01/EQAnatomyLab2/issues)

ダウンロード / Download：[Releases ページ](https://github.com/kazufumi01/EQAnatomyLab2/releases/latest)

| ファイル | 内容 |
|---|---|
| `EQAnatomyLab2-0.2.0-mac.zip` | Mac・手動インストール（おすすめ） |
| `EQAnatomyLab2-0.2.0-mac.pkg` | Mac・インストーラー（ターミナルを使いたくない人向け） |

---

## 日本語

### Mac（macOS 11 以降・Apple Silicon / Intel）— 手動（zip）

1. zip をダブルクリックで展開。
2. Finder で option キーを押しながら「移動」メニュー › 「ライブラリ」を開き、次へドラッグでコピー（フォルダが無ければ作成）：
   - `EQ Anatomy Lab 2.component` → `ライブラリ/Audio/Plug-Ins/Components`
   - `EQ Anatomy Lab 2.vst3` → `ライブラリ/Audio/Plug-Ins/VST3`
   - `EQ Anatomy Lab 2.app` → `アプリケーション`（単体で使う場合のみ）
3. ターミナルで次の1行を実行（Apple の公証をしていないため、検疫フラグを外す）：
   ```bash
   xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/Components/"EQ Anatomy Lab 2.component" ~/Library/Audio/Plug-Ins/VST3/"EQ Anatomy Lab 2.vst3" /Applications/"EQ Anatomy Lab 2.app"
   ```
   .app を入れていない場合の「No such file」は無視して OK。**この手順を飛ばすと「壊れている」と表示され読み込めません。**
4. DAW を再起動して再スキャン。Live は「設定 › プラグイン › 再スキャン」。

### Mac — インストーラー（pkg）

1. `.pkg` をダブルクリック。「Apple は悪質なソフトウェアがないことを確認できません」と出たら **「完了」** で閉じる。
2. **システム設定 › プライバシーとセキュリティ** の下の方にある **「このまま開く」** をクリック → パスワード入力。
3. 入れたい形式（AU / VST3 / Standalone）を選んでインストール → DAW で再スキャン。


### 読み込めない時
- **AU が出ない**：ターミナルで `killall -9 AudioComponentRegistrar` → DAW 再起動。
- **「壊れている」と出る**：上の手順3のコマンドを実行（pkg 版で入れた場合は `~/Library` を `/Library` に読み替え）。
- **アンインストール**：入れたファイルを削除（pkg 版は `/Library/Audio/Plug-Ins/…` と `/Applications`）。

---

## English

### Mac (macOS 11+, Apple Silicon / Intel) — manual (zip)

1. Unzip.
2. In Finder, hold Option, open the Go menu › Library, and copy (create folders if missing):
   - `EQ Anatomy Lab 2.component` → `Library/Audio/Plug-Ins/Components`
   - `EQ Anatomy Lab 2.vst3` → `Library/Audio/Plug-Ins/VST3`
   - `EQ Anatomy Lab 2.app` → `Applications` (standalone only)
3. In Terminal, remove the quarantine flag (the build is not notarized):
   ```bash
   xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/Components/"EQ Anatomy Lab 2.component" ~/Library/Audio/Plug-Ins/VST3/"EQ Anatomy Lab 2.vst3" /Applications/"EQ Anatomy Lab 2.app"
   ```
   ("No such file" for the .app is fine if you skipped it.)
4. Restart your DAW and rescan plug-ins.

### Mac — installer (pkg)

1. Double-click the `.pkg`. If macOS says it cannot verify the developer, click **Done**.
2. Open **System Settings › Privacy & Security**, scroll down and click **Open Anyway**, then enter your password.
3. Choose the formats (AU / VST3 / Standalone), install, and rescan in your DAW.

