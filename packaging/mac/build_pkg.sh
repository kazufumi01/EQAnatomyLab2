#!/bin/zsh
# EQ Anatomy Lab 2 の Mac 用インストーラー（.pkg）を作る
# 使い方： packaging/mac/build_pkg.sh [バージョン] [ビルド成果物フォルダ]
#   例）packaging/mac/build_pkg.sh 0.2.1 build/EQAnatomyLab2_artefacts/Release
# 出力：dist/EQAnatomyLab2-<ver>-mac.pkg
# ※ 署名・公証なし（Apple Developer Program 未加入でも作れる）
set -e
cd "$(dirname "$0")/../.."

VERSION="${1:-0.2.1}"
ART="${2:-build/EQAnatomyLab2_artefacts/Release}"
NAME="EQ Anatomy Lab 2"
ID="com.jpk.eqanatomylab2"
WORK="build/pkg"
OUT="dist/EQAnatomyLab2-${VERSION}-mac.pkg"

rm -rf "$WORK"; mkdir -p "$WORK"/{au,vst3,app} dist

# 1) ステージング（quarantine 等の拡張属性を消し、アドホック署名をかけ直す）
ditto "$ART/AU/$NAME.component"         "$WORK/au/$NAME.component"
ditto "$ART/VST3/$NAME.vst3"            "$WORK/vst3/$NAME.vst3"
ditto "$ART/Standalone/$NAME.app"       "$WORK/app/$NAME.app"
for b in "$WORK/au/$NAME.component" "$WORK/vst3/$NAME.vst3" "$WORK/app/$NAME.app"; do
  xattr -cr "$b"
  codesign --force --deep --sign - "$b"
done

# 2) 既存の同名バンドルへの「再配置」を防ぐ設定（これがないと別の場所に入ることがある）
for d in au vst3 app; do
  pkgbuild --analyze --root "$WORK/$d" "$WORK/$d.plist" >/dev/null
  /usr/libexec/PlistBuddy -c "Delete :0:BundleIsRelocatable" "$WORK/$d.plist" 2>/dev/null || true
  /usr/libexec/PlistBuddy -c "Add :0:BundleIsRelocatable bool false" "$WORK/$d.plist"
done

# 3) コンポーネントごとの pkg
pkgbuild --root "$WORK/au"   --component-plist "$WORK/au.plist" \
  --identifier "$ID.au"   --version "$VERSION" \
  --install-location "/Library/Audio/Plug-Ins/Components" \
  --scripts packaging/mac/scripts "$WORK/au.pkg"
pkgbuild --root "$WORK/vst3" --component-plist "$WORK/vst3.plist" \
  --identifier "$ID.vst3" --version "$VERSION" \
  --install-location "/Library/Audio/Plug-Ins/VST3" "$WORK/vst3.pkg"
pkgbuild --root "$WORK/app"  --component-plist "$WORK/app.plist" \
  --identifier "$ID.app"  --version "$VERSION" \
  --install-location "/Applications" "$WORK/app.pkg"

# 4) 選択式インストーラーにまとめる
sed "s/@VERSION@/$VERSION/g" packaging/mac/distribution.xml > "$WORK/distribution.xml"
productbuild --distribution "$WORK/distribution.xml" \
  --resources packaging/mac/resources \
  --package-path "$WORK" "$OUT"

echo "作成：$OUT"
