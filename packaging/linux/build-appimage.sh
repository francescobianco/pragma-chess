#!/usr/bin/env bash
# Builds the AppImage of Pragma Chess: one file that runs on most Linux
# distributions without installing, for those the .deb and .rpm do not reach
# (Arch, openSUSE, Mint, …). Built against the Qt of the machine, which
# linuxdeploy copies inside with the libraries it needs; it runs where the
# C library is at least as new as the build machine's (CI: Ubuntu 24.04).
#
#   packaging/linux/build-appimage.sh [version]
#
# The AppImage lands in dist/. linuxdeploy and its Qt plugin are downloaded,
# pinned, into the build folder.
set -euo pipefail

version=${1:-}
root=$(cd "$(dirname "$0")/../.." && pwd)
build=${BUILD_DIR:-$root/build-appimage}
dist=$root/dist
appdir=$build/AppDir
app_id=io.github.francescobianco.PragmaChess

linuxdeploy_tag=1-alpha-20251107-1
plugin_qt_tag=1-alpha-20250213-1

args=(-S "$root" -B "$build" -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr)
[ -n "$version" ] && args+=("-DPRAGMA_VERSION=$version")
cmake "${args[@]}"
[ -x "$build/gui/qt/engines/stockfish" ] || "$root/scripts/build-stockfish.sh" "$build/gui/qt/engines"
cmake --build "$build"
rm -rf "$appdir"
DESTDIR="$appdir" cmake --install "$build"

tools=$build/appimage-tools
mkdir -p "$tools"
fetch() {
    [ -x "$tools/$2" ] || { curl -fsSL -o "$tools/$2" "$1" && chmod +x "$tools/$2"; }
}
fetch "https://github.com/linuxdeploy/linuxdeploy/releases/download/$linuxdeploy_tag/linuxdeploy-x86_64.AppImage" linuxdeploy-x86_64.AppImage
fetch "https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/$plugin_qt_tag/linuxdeploy-plugin-qt-x86_64.AppImage" linuxdeploy-plugin-qt-x86_64.AppImage

# The Qt plugin finds Qt through qmake. Wayland's platform plugin is added
# when the build machine has it (qt6-wayland), else the AppImage runs on X11
# and XWayland; offscreen too, so the AppImage can be tried without a screen.
export QMAKE=${QMAKE:-$(command -v qmake6 || echo /usr/lib/qt6/bin/qmake6)}
export EXTRA_QT_MODULES="svg"
qt_plugins=$("$QMAKE" -query QT_INSTALL_PLUGINS)
platform_plugins=libqoffscreen.so
[ -e "$qt_plugins/platforms/libqwayland-generic.so" ] && platform_plugins="libqwayland-generic.so;$platform_plugins"
export EXTRA_PLATFORM_PLUGINS=$platform_plugins
export APPIMAGE_EXTRACT_AND_RUN=1
export LDAI_OUTPUT="PragmaChess-${version:-dev}-linux-x86_64.AppImage"
# The bundled engine is a program of its own: its libraries come along too.
(cd "$build" && "$tools/linuxdeploy-x86_64.AppImage" --appdir "$appdir" \
    --desktop-file "$appdir/usr/share/applications/$app_id.desktop" \
    --executable "$appdir/usr/lib/pragma-chess/engines/stockfish" \
    --plugin qt --output appimage)

mkdir -p "$dist"
mv "$build/$LDAI_OUTPUT" "$dist/"
ls -l "$dist/$LDAI_OUTPUT"
