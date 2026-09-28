#!/usr/bin/env bash
# Adds, next to each package in a folder, a copy under a name without the
# version, so that
#   https://github.com/francescobianco/pragma-chess/releases/latest/download/<name>
# always downloads the latest release. The versioned files stay as they are.
#
#   packaging/latest-names.sh dist
set -euo pipefail
cd "${1:?usage: latest-names.sh <folder>}"

copy() {
    local pattern=$1 name=$2 files
    files=($pattern)
    [ -e "${files[0]}" ] || { echo "no file matches $pattern" >&2; exit 1; }
    cp "${files[0]}" "$name"
    echo "${files[0]} -> $name"
}

copy 'PragmaChess-*-windows-x64-setup.exe'    PragmaChess-windows-x64-setup.exe
copy 'PragmaChess-*-windows-x64-portable.zip' PragmaChess-windows-x64-portable.zip
copy 'PragmaChess-*-macos-arm64.dmg'          PragmaChess-macos-arm64.dmg
copy 'pragma-chess_*_amd64.deb'               pragma-chess_amd64.deb
copy 'pragma-chess-*.x86_64.rpm'              pragma-chess.x86_64.rpm
