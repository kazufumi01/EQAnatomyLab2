# EQ Anatomy Lab 2 β — インストール方法 / Installation

> **ベータ版です。** Mac の Ableton Live で動作確認済み。
> **Windows 版は実機での動作確認ができていません**（自動ビルドで作成）。
> **Beta.** Tested with Ableton Live on Mac.
> **The Windows build of Lab 2 has NOT been tested on a real Windows machine.**
> 不具合は / Problems: [Issues](https://github.com/kazufumi01/EQAnatomyLab2/issues)

ダウンロード / Download：[Releases ページ](https://github.com/kazufumi01/EQAnatomyLab2/releases/latest)

| ファイル | 内容 |
|---|---|
| `EQAnatomyLab2-0.2.0-mac.zip` | Mac・手動インストール（おすすめ） |
| `EQAnatomyLab2-0.2.0-mac.pkg` | Mac・インストーラー（ターミナルを使いたくない人向け） |
| `EQAnatomyLab2-0.2.0-win64.zip` | Windows・手動インストール（未検証） |
| `EQAnatomyLab2-0.2.0-win64.exe` | Windows・インストーラー（未検証） |

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


### Windows（10 / 11・64bit）— ⚠️ 未検証

**手動（zip）**
1. 展開前に zip を右クリック › プロパティ › **「許可する」** にチェック › OK。
2. 展開した `EQ Anatomy Lab 2.vst3` フォルダを丸ごと `C:\Program Files\Common Files\VST3` へコピー。
3. DAW で再スキャン（Live は「VST3 プラグイン システムフォルダ」をオン）。単体で使う場合は `EQ Anatomy Lab 2.exe` を起動。

**インストーラー（exe）**
1. `.exe` を実行。「Windows によって PC が保護されました」→ **「詳細情報」→「実行」**。
2. VST3 は `C:\Program Files\Common Files\VST3` に入ります。DAW で再スキャン。

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

### Windows (10 / 11, 64-bit) — UNTESTED

**Manual (zip):** before extracting, right-click the zip › Properties › tick "Unblock" › OK. Copy the whole `EQ Anatomy Lab 2.vst3` folder to `C:\Program Files\Common Files\VST3` and rescan in your DAW. For standalone use, run `EQ Anatomy Lab 2.exe`.

**Installer (exe):** run it; if "Windows protected your PC" appears, click More info › Run anyway. VST3 is installed to `C:\Program Files\Common Files\VST3`.
