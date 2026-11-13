#!/bin/zsh
# EQ Anatomy Lab 2 の Mac 用「手動インストール」zip を作る
# 使い方： packaging/mac/build_zip.sh [バージョン] [ビルド成果物フォルダ]
# 出力：dist/EQAnatomyLab2-<ver>-mac.zip
#   中身：.component / .vst3 / .app / HOW_TO_INSTALL.txt / license.txt
set -e
cd "$(dirname "$0")/../.."

VERSION="${1:-0.2.1}"
ART="${2:-build/EQAnatomyLab2_artefacts/Release}"
NAME="EQ Anatomy Lab 2"
DIR="build/zip/EQAnatomyLab2-${VERSION}-mac"
OUT="dist/EQAnatomyLab2-${VERSION}-mac.zip"

rm -rf "$DIR"; mkdir -p "$DIR" dist
ditto "$ART/AU/$NAME.component"   "$DIR/$NAME.component"
ditto "$ART/VST3/$NAME.vst3"      "$DIR/$NAME.vst3"
ditto "$ART/Standalone/$NAME.app" "$DIR/$NAME.app"
for b in "$DIR/$NAME.component" "$DIR/$NAME.vst3" "$DIR/$NAME.app"; do
  xattr -cr "$b"
  codesign --force --deep --sign - "$b"
done
cp packaging/mac/resources/license.txt "$DIR/license.txt"
cp packaging/manual/README-mac.txt "$DIR/HOW_TO_INSTALL.txt"

rm -f "$OUT"
ditto -c -k --sequesterRsrc --keepParent "$DIR" "$OUT"
echo "作成：$OUT"
