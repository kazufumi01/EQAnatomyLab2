# EQ Anatomy Lab（JUCE プラグイン）

by Johan Plus Kei — EQ の中身を「見て・聴いて・触って」学べる VST・AU 対応プラグイン（VST3 / AU / Standalone）

![EQ Anatomy Lab](docs/screenshot-main.png)

## ⬇️ ダウンロード（ベータ版）/ Download (Beta)

**[最新版をダウンロード / Download latest](https://github.com/kazufumi01/EQAnatomyLab/releases/latest)** — 期間限定ベータ版無料配布中 / Free beta for a limited time

| | 手動インストール（おすすめ） | インストーラー |
|---|---|---|
| **Mac**（macOS 11+, Apple Silicon / Intel） | `EQAnatomyLab-x.x.x-mac.zip` | `EQAnatomyLab-x.x.x-mac.pkg` |
| **Windows**（10 / 11, 64bit）⚠️未検証 | `EQAnatomyLab-x.x.x-win64.zip` | `EQAnatomyLab-x.x.x-win64.exe` |

- インストール方法：**[INSTALL.md](INSTALL.md)**（日本語 / English）
- ⚠️ **Windows 版は実機での動作確認ができていません**（自動ビルドで作成。VST3 と Standalone）。 / The Windows build is untested on real hardware.
- Apple の公証・Windows のコード署名をしていないため、初回のみ警告が出ます。
- 不具合・要望は [Issues](https://github.com/kazufumi01/EQAnatomyLab/issues) へ。

---

## 主な機能

- **6 バンド EQ**：Bell / Shelf / Cut など。動作モードは Zero latency（ゼロレイテンシー）／Analog／Linear（リニアフェイズ）。位相はバンドごとに Min / Lin を選択
- **ダイナミック EQ**：バンドごとに DYN。検出ソースは In / SC / Mid / Side から選択
- **Mid/Side 処理**：バンドごとに St / M / S。SOLO M・S で「その成分だけ」を試聴（Side は左右に広がった音で確認）
- **Transient / Body 分離**：アタック部と胴鳴りを分けて EQ。スコープで検出の様子を見ながら Sens / Width / Attack を調整、検出専用の Low Cut と 2 バンド EQ、検出方式（Ratio / Slope）
- **周波数特性・位相・群遅延**を重ね描き／上下分割で同時表示。インパルス応答でプリリンギングを確認、Pre-ring solo
- **サチュレーター**（帯域指定・Tape など・オーバーサンプリング）、Auto gain、Delta（差分）試聴
- **A / B / C / D スロット**とモーフ
- **IN / OUT メーター**（ピーク・RMS）と **CORR（位相相関）メーター**
- **Ear Training（耳トレ）**と **22 本のレッスン**（Learn）。日本語 / English 切り替え

![Transient / Body スコープ](docs/screenshot-tb-scope.png)

---

## ビルド

```bash
./build.sh            # 手元の JUCE を使う（build.sh 内の JUCE_PATH を編集）
```

または JUCE 9.0.4 を自動取得：

```bash
cmake -B build -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release -j
```

- 配布ビルドでは `-DEQA_COPY_AFTER_BUILD=OFF`（ビルド後のコピーを無効化）
- Mac のインストーラー作成：`packaging/mac/build_pkg.sh <version>` ／ `packaging/mac/build_zip.sh <version>`（出力は `dist/`）
- Windows は GitHub Actions（`.github/workflows/build-win.yml`、手動実行）で VST3 / Standalone をビルドし、Inno Setup でインストーラーを作成

## ライセンス

JUCE は **Starter ライセンス** で使用。配布物はフリーウェア使用許諾（`packaging/mac/resources/license.txt`）に従います。
ソースは学習目的で公開していますが、無断での再配布・販売はできません。

---

**Johan Plus Kei** — [Spotify](https://open.spotify.com/artist/30PV2gqUbDDwvuWtKaok0x) ・ [Apple Music](https://music.apple.com/us/artist/johan-plus-kei/519360150) ・ [X](https://x.com/johanpluskei) ・ [Instagram](https://www.instagram.com/johanpluskei) ・ [Learning labs](https://kazufumi01.github.io/)

## Support / 支援

EQ Anatomy Lab 2 is free. If it helps your mixing or your classes, you can support development with a one-time tip via [PayPal](https://paypal.me/johanpluskei). More ways to help: <https://kazufumi01.github.io/support/>

EQ Anatomy Lab 2 は無料です。ミックスや授業で役立ったら、[PayPal](https://paypal.me/johanpluskei) からの投げ銭で開発を支えていただけるとうれしいです。
