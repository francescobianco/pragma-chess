#!/usr/bin/env bash
# Builds the macOS disk image of Pragma Chess (Apple Silicon).
#
#   packaging/macos/build.sh [version]
#
# Needs Qt 6 for macOS on PATH (macdeployqt), CMake, Ninja and create-dmg
# (brew install create-dmg). The disk image lands in dist/.
#
# Signing: without MACOS_SIGN_IDENTITY the app is signed ad hoc, which Apple
# Silicon requires to run at all; Gatekeeper then asks the user to allow it
# once (System Settings ▸ Privacy & Security ▸ Open Anyway). With a Developer
# ID identity in the keychain it is signed with the hardened runtime, and with
# APPLE_ID, APPLE_TEAM_ID and APPLE_APP_PASSWORD also notarized.
set -euo pipefail

version=${1:-}
root=$(cd "$(dirname "$0")/../.." && pwd)
here=$root/packaging/macos
build=${BUILD_DIR:-$root/build-macos}
stage=$build/stage
dist=$root/dist
app_name="Pragma Chess"

args=(-S "$root" -B "$build" -G Ninja
      -DCMAKE_BUILD_TYPE=Release
      -DCMAKE_OSX_ARCHITECTURES=arm64
      -DCMAKE_OSX_DEPLOYMENT_TARGET=12.0
      # Always the bundled yaml-cpp, linked statically: nothing from Homebrew in the bundle.
      -DCMAKE_DISABLE_FIND_PACKAGE_yaml-cpp=ON)
[ -n "$version" ] && args+=("-DPRAGMA_VERSION=$version")
cmake "${args[@]}"
cmake --build "$build"
if [ "${SKIP_TESTS:-0}" != 1 ]; then
    QT_QPA_PLATFORM=offscreen ctest --test-dir "$build" --output-on-failure
fi
version=$(sed -n 's/^PRAGMA_VERSION:STRING=//p' "$build/CMakeCache.txt")

# The bundle, named as the user sees it, with Qt inside.
rm -rf "$stage"
mkdir -p "$stage"
cp -R "$build/gui/qt/pragma-chess.app" "$stage/$app_name.app"
bundle="$stage/$app_name.app"
cp "$build/gui/qt/pragma-explain" "$build/gui/qt/pragma-book" "$bundle/Contents/MacOS/"
macdeployqt "$bundle" -executable="$bundle/Contents/MacOS/pragma-explain" \
    -executable="$bundle/Contents/MacOS/pragma-book"
# The bundled engine: the executable in Contents/MacOS, where EngineCatalog
# looks and the signature expects code, its license and README in Resources.
"$root/scripts/fetch-stockfish.sh" "$build/engines" macos
cp "$build/engines/stockfish" "$bundle/Contents/MacOS/"
mkdir -p "$bundle/Contents/Resources/engines"
cp "$build/engines/Copying.txt" "$build/engines/AUTHORS" "$build/engines/README.txt" \
    "$bundle/Contents/Resources/engines/"
# Only SQLite is used: the other drivers need client libraries we do not ship.
find "$bundle/Contents/PlugIns/sqldrivers" -type f ! -name 'libqsqlite*' -delete

identity=${MACOS_SIGN_IDENTITY:--}
sign_args=(--force --sign "$identity" --timestamp=none)
if [ "$identity" != - ]; then
    sign_args=(--force --sign "$identity" --timestamp --options runtime)
fi
# Inside out: frameworks and plugins first, the bundle last.
find "$bundle/Contents" \( -name '*.dylib' -o -name '*.framework' \) -print0 |
    xargs -0 -n1 codesign "${sign_args[@]}"
codesign "${sign_args[@]}" "$bundle/Contents/MacOS/pragma-explain"
codesign "${sign_args[@]}" "$bundle/Contents/MacOS/pragma-book"
codesign "${sign_args[@]}" "$bundle/Contents/MacOS/stockfish"
codesign "${sign_args[@]}" "$bundle"
codesign --verify --deep --strict --verbose=2 "$bundle"

# The disk image: the app on the left, Applications on the right, over a
# background that says what to do (packaging/assets/make-installer-art.py).
mkdir -p "$dist"
dmg="$dist/PragmaChess-$version-macos-arm64.dmg"
rm -f "$dmg"
tiffutil -cathidpicheck "$here/dmg-background.png" "$here/dmg-background@2x.png" \
    -out "$build/dmg-background.tiff"
make_dmg() {
    create-dmg \
        --volname "$app_name" \
        --volicon "$root/gui/qt/data/icons/pragma-chess.icns" \
        --background "$build/dmg-background.tiff" \
        --window-pos 200 120 \
        --window-size 660 400 \
        --icon-size 128 \
        --text-size 13 \
        --icon "$app_name.app" 170 190 \
        --hide-extension "$app_name.app" \
        --app-drop-link 490 190 \
        --no-internet-enable \
        "$dmg" "$stage"
}
# Finder scripting on CI machines sometimes times out: try again.
for attempt in 1 2 3; do
    make_dmg && break
    rm -f "$dmg" "$dist"/rw.*.dmg
    [ "$attempt" = 3 ] && exit 1
    sleep 5
done
if [ "$identity" != - ]; then
    codesign --force --sign "$identity" --timestamp "$dmg"
    if [ -n "${APPLE_ID:-}" ] && [ -n "${APPLE_TEAM_ID:-}" ] && [ -n "${APPLE_APP_PASSWORD:-}" ]; then
        xcrun notarytool submit "$dmg" --apple-id "$APPLE_ID" --team-id "$APPLE_TEAM_ID" \
            --password "$APPLE_APP_PASSWORD" --wait
        xcrun stapler staple "$dmg"
    fi
fi
ls -l "$dmg"
