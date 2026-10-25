#!/bin/bash
# Crea NHSE.app (autónoma, co runtime de .NET dentro), un .dmg e un .zip por arquitectura
# (o .zip é o que descarga a propia app para actualizarse).
#   tools/empaquetar.sh            → arm64 (Apple Silicon) e x64 (Intel)
#   tools/empaquetar.sh arm64      → só unha
# Saída en dist/. A app vai asinada ad hoc (sen conta de Apple Developer).
set -euo pipefail

RAIZ="$(cd "$(dirname "$0")/.." && pwd)"
cd "$RAIZ"
export DOTNET_CLI_TELEMETRY_OPTOUT=1 DOTNET_NOLOGO=1
DOTNET="${DOTNET:-$(command -v dotnet || echo "$HOME/.dotnet/dotnet")}"

VERSION="$(sed -n 's:.*<Version>\(.*\)</Version>.*:\1:p' Directory.Build.props)"
ARQUITECTURAS=("${@:-arm64 x64}")
read -r -a ARQUITECTURAS <<< "${ARQUITECTURAS[*]}"
DIST="$RAIZ/dist"
OBRA="$DIST/obra"
mkdir -p "$DIST"

# Icona: o icon.ico de NHSE.WinForms, en todos os tamaños que pide iconutil.
ICNS="$OBRA/NHSE.icns"
if [ ! -f "$ICNS" ]; then
    mkdir -p "$OBRA/NHSE.iconset"
    BASE="$OBRA/icon-1024.png"
    sips -s format png -z 1024 1024 NHSE/NHSE.WinForms/icon.ico --out "$BASE" >/dev/null
    for t in 16 32 128 256 512; do
        sips -z $t $t "$BASE" --out "$OBRA/NHSE.iconset/icon_${t}x${t}.png" >/dev/null
        d=$((t * 2))
        sips -z $d $d "$BASE" --out "$OBRA/NHSE.iconset/icon_${t}x${t}@2x.png" >/dev/null
    done
    iconutil -c icns "$OBRA/NHSE.iconset" -o "$ICNS"
fi

for ARQ in "${ARQUITECTURAS[@]}"; do
    echo "== NHSE $VERSION para $ARQ"
    PUB="$OBRA/publish-$ARQ"
    rm -rf "$PUB"
    "$DOTNET" publish src/NHSE.Mac/NHSE.Mac.csproj -c Release -r "osx-$ARQ" --self-contained \
        -p:UseAppHost=true -p:DebugType=none -o "$PUB" --nologo -v quiet

    APP="$OBRA/$ARQ/NHSE.app"
    rm -rf "$OBRA/$ARQ"
    mkdir -p "$APP/Contents/MacOS" "$APP/Contents/Resources"
    cp -R "$PUB"/. "$APP/Contents/MacOS/"
    cp "$ICNS" "$APP/Contents/Resources/NHSE.icns"
    sed "s/@VERSION@/$VERSION/g" tools/macos/Info.plist > "$APP/Contents/Info.plist"
    codesign --force --deep --sign - "$APP"
    codesign --verify --deep "$APP"

    # Zip só coa app: o que descarga NHSE cando se actualiza só.
    ZIP="$DIST/NHSE-$VERSION-macOS-$ARQ.zip"
    rm -f "$ZIP"
    ditto -c -k --keepParent "$APP" "$ZIP"

    # DMG coa app e un atallo a Aplicacións.
    DMG="$DIST/NHSE-$VERSION-macOS-$ARQ.dmg"
    rm -f "$DMG"
    ln -s /Applications "$OBRA/$ARQ/Applications"
    hdiutil create -volname "NHSE $VERSION" -srcfolder "$OBRA/$ARQ" -ov -format UDZO "$DMG" >/dev/null
    echo "   $DMG ($(du -h "$DMG" | cut -f1))"
done
