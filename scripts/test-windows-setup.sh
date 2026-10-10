#!/usr/bin/env bash
# Builds the Windows installer from packaging/windows with Inno Setup under
# Wine, and runs it in a fresh Wine prefix: the first install, as a Windows
# user sees it, in the language of this system (LANG). Used by
# `make test-windows-setup`.
#
# Everything lives in build/windows-setup/ (BUILD_DIR): a Wine prefix with
# Inno Setup (installed once), the program to package — the portable zip of
# the latest release, downloaded once (the installer's pages, not the
# program, are what is tried) —, the installer built, and the prefix the
# installer runs in, made anew at every run. Nothing touches ~/.wine.
set -euo pipefail

INNO_VERSION="6.7.3" # The series the release's CI installs (packaging/windows/build.ps1: Inno Setup 6).
REPO="francescobianco/pragma-chess"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
WORK="$ROOT/${BUILD_DIR:-build}/windows-setup"
TOOLS="$WORK/tools-prefix"   # Inno Setup
TRIAL="$WORK/trial-prefix"   # where the installer runs, fresh every time
export WINEDEBUG=-all

log() { printf '\033[1;34m[setup]\033[0m %s\n' "$*"; }

command -v wine >/dev/null || { echo "wine is needed (apt install wine)"; exit 1; }
mkdir -p "$WORK/downloads"

# Inno Setup, once.
ISCC="$TOOLS/drive_c/Inno/ISCC.exe"
if [[ ! -f "$ISCC" ]]; then
    installer="$WORK/downloads/innosetup-$INNO_VERSION.exe"
    if [[ ! -f "$installer" ]]; then
        log "downloading Inno Setup $INNO_VERSION…"
        curl -fsSL -o "$installer" \
            "https://github.com/jrsoftware/issrc/releases/download/is-${INNO_VERSION//./_}/innosetup-$INNO_VERSION.exe"
    fi
    log "installing Inno Setup in its own Wine prefix…"
    WINEPREFIX="$TOOLS" wineboot -i >/dev/null 2>&1
    WINEPREFIX="$TOOLS" wine "$installer" /VERYSILENT /SUPPRESSMSGBOXES /NORESTART '/DIR=C:\Inno' >/dev/null 2>&1
fi

# The program to package: the latest release's portable zip, once.
PROGRAM="$WORK/program/Pragma Chess"
if [[ ! -f "$PROGRAM/pragma-chess.exe" ]]; then
    zip="$WORK/downloads/portable.zip"
    if [[ ! -f "$zip" ]]; then
        log "downloading the latest release's portable zip…"
        url=$(curl -fsSL "https://api.github.com/repos/$REPO/releases/latest" |
            python3 -c 'import json, sys
for asset in json.load(sys.stdin)["assets"]:
    if asset["name"].endswith("-windows-x64-portable.zip"):
        print(asset["browser_download_url"]); break')
        [[ -n "$url" ]] || { echo "no portable zip in the latest release"; exit 1; }
        curl -fsSL -o "$zip" "$url"
    fi
    rm -rf "$WORK/program" && mkdir -p "$WORK/program"
    (cd "$WORK/program" && unzip -q "$zip")
fi

# The installer, from the sources as they are now.
VERSION=$(sed -n 's/^set(PRAGMA_VERSION \([0-9.]*\).*/\1/p' "$ROOT/CMakeLists.txt")
mkdir -p "$WORK/out"
rm -f "$WORK/out/"*.exe
log "building the installer $VERSION…"
(cd "$ROOT/packaging/windows" &&
    WINEPREFIX="$TOOLS" wine 'C:\Inno\ISCC.exe' /Q "/DAppVersion=$VERSION" "/DSourceDir=Z:$PROGRAM" \
        "/DOutputDir=Z:$WORK/out" pragma-chess.iss)
SETUP=$(ls "$WORK/out/"*.exe)

# A fresh prefix: a first install.
log "a fresh Windows (Wine prefix) for the installer…"
rm -rf "$TRIAL"
WINEPREFIX="$TRIAL" wineboot -i >/dev/null 2>&1
language="${LANG:-C}"
log "running $(basename "$SETUP") — in $language"
WINEPREFIX="$TRIAL" wine "$SETUP"
log "done; the trial stays in $TRIAL until the next run"
