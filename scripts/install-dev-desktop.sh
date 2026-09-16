#!/usr/bin/env bash
# Registers the development build with the desktop, for the current user.
#
# On Wayland, GNOME and KDE ignore the icon a window sets: the dock and the
# window list show the icon of the .desktop entry named after the app id. Until
# the app is installed there is no such entry, and a generic icon appears.
# This writes one pointing at the build, plus the icons, under ~/.local/share.
# An entry installed by `make install` is left alone.
set -eu

[[ "$(uname)" == Linux ]] || exit 0

APP_ID="io.github.francescobianco.PragmaChess"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-build}"
DATA="${XDG_DATA_HOME:-$HOME/.local/share}"
ENTRY="$DATA/applications/$APP_ID.desktop"
MARKER="X-Pragma-Development=true"

if [[ -f "$ENTRY" ]] && ! grep -q "^$MARKER$" "$ENTRY"; then
    exit 0 # Installed for real.
fi

mkdir -p "$DATA/applications" "$DATA/icons"
cp -r "$ROOT/gui/qt/data/icons/hicolor" "$DATA/icons/"

APP="$ROOT/$BUILD_DIR/gui/qt/pragma-chess"
sed -e "s|^Exec=.*|Exec=\"$APP\" %F|" "$ROOT/gui/qt/data/$APP_ID.desktop" > "$ENTRY.tmp"
echo "$MARKER" >> "$ENTRY.tmp"
if ! cmp -s "$ENTRY.tmp" "$ENTRY" 2>/dev/null; then
    mv "$ENTRY.tmp" "$ENTRY"
    update-desktop-database -q "$DATA/applications" 2>/dev/null || true
    gtk-update-icon-cache -q -t "$DATA/icons/hicolor" 2>/dev/null || true
    echo "Registered the development build with the desktop ($ENTRY)"
else
    rm -f "$ENTRY.tmp"
fi
