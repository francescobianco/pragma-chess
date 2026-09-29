#!/usr/bin/env bash
# Downloads the Stockfish release pinned in packaging/stockfish.env, checks its
# SHA-256 and stages what Pragma Chess ships as its default engine:
#
#   <dest>/stockfish[.exe]   the engine
#   <dest>/Copying.txt       its license (GPL v3)
#   <dest>/AUTHORS
#   <dest>/README.txt        what it is and where its source is
#
#   scripts/fetch-stockfish.sh <dest> [linux-x86-64|linux-arm64|macos|windows-x86-64]
#   scripts/fetch-stockfish.sh --source <dest-dir>   source archive with the networks
#
# The platform defaults to the machine running the script. Downloads are
# cached in ${STOCKFISH_CACHE:-~/.cache/pragma-chess}.
set -euo pipefail

root=$(cd "$(dirname "$0")/.." && pwd)
# shellcheck source=../packaging/stockfish.env
. "$root/packaging/stockfish.env"
cache=${STOCKFISH_CACHE:-${XDG_CACHE_HOME:-$HOME/.cache}/pragma-chess}
base=https://github.com/official-stockfish/Stockfish
mkdir -p "$cache"

sha256() {
    if command -v sha256sum > /dev/null; then sha256sum "$1" | cut -d' ' -f1
    else shasum -a 256 "$1" | cut -d' ' -f1; fi
}

# download <url> <file> [sha256]
download() {
    local url=$1 file=$2 expected=${3:-}
    if [ ! -f "$file" ] || { [ -n "$expected" ] && [ "$(sha256 "$file")" != "$expected" ]; }; then
        echo "Downloading $url" >&2
        curl -fL --retry 3 -o "$file.part" "$url"
        mv "$file.part" "$file"
    fi
    if [ -n "$expected" ] && [ "$(sha256 "$file")" != "$expected" ]; then
        echo "SHA-256 mismatch for $file" >&2
        rm -f "$file"
        exit 1
    fi
}

# The source of the release, with the networks the binaries embed (the build
# downloads them; the archive includes them so it is complete on its own).
if [ "${1:-}" = --source ]; then
    dest=${2:?usage: fetch-stockfish.sh --source <dest-dir>}
    mkdir -p "$dest"
    work=$(mktemp -d)
    trap 'rm -rf "$work"' EXIT
    download "$base/archive/refs/tags/$STOCKFISH_TAG.tar.gz" "$cache/$STOCKFISH_TAG-source.tar.gz"
    tar -xzf "$cache/$STOCKFISH_TAG-source.tar.gz" -C "$work"
    dir=$(find "$work" -mindepth 1 -maxdepth 1 -type d | head -n1)
    make -C "$dir/src" net > /dev/null
    mv "$dir" "$work/stockfish-$STOCKFISH_TAG"
    tar -czf "$dest/stockfish-$STOCKFISH_TAG-source.tar.gz" -C "$work" "stockfish-$STOCKFISH_TAG"
    ls -l "$dest/stockfish-$STOCKFISH_TAG-source.tar.gz"
    exit 0
fi

dest=${1:?usage: fetch-stockfish.sh <dest> [platform]}
platform=${2:-}
if [ -z "$platform" ]; then
    case "$(uname -s)-$(uname -m)" in
        Linux-x86_64) platform=linux-x86-64 ;;
        Linux-aarch64) platform=linux-arm64 ;;
        Darwin-*) platform=macos ;;
        MINGW*|MSYS*|CYGWIN*) platform=windows-x86-64 ;;
        *) echo "no Stockfish build for $(uname -s) $(uname -m)" >&2; exit 1 ;;
    esac
fi
case $platform in
    linux-x86-64) asset=$STOCKFISH_LINUX_X86_64; sum=$STOCKFISH_LINUX_X86_64_SHA256; exe=stockfish ;;
    linux-arm64) asset=$STOCKFISH_LINUX_ARM64; sum=$STOCKFISH_LINUX_ARM64_SHA256; exe=stockfish ;;
    macos) asset=$STOCKFISH_MACOS; sum=$STOCKFISH_MACOS_SHA256; exe=stockfish ;;
    windows-x86-64) asset=$STOCKFISH_WINDOWS_X86_64; sum=$STOCKFISH_WINDOWS_X86_64_SHA256; exe=stockfish.exe ;;
    *) echo "unknown platform: $platform" >&2; exit 2 ;;
esac

download "$base/releases/download/$STOCKFISH_TAG/$asset" "$cache/$asset" "$sum"
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
case $asset in
    *.zip) unzip -q "$cache/$asset" -d "$work" ;;
    *) tar -xzf "$cache/$asset" -C "$work" ;;
esac
binary=$(find "$work" -maxdepth 2 -type f -name "${asset%%.*}*" | head -n1)
[ -n "$binary" ] || { echo "no engine binary in $asset" >&2; exit 1; }

mkdir -p "$dest"
cp "$binary" "$dest/$exe"
chmod +x "$dest/$exe"
cp "$work"/stockfish/Copying.txt "$work"/stockfish/AUTHORS "$dest"/
sed -e "s/@VERSION@/$STOCKFISH_VERSION/g" -e "s/@TAG@/$STOCKFISH_TAG/g" -e "s/@ASSET@/$asset/g" \
    "$root/packaging/stockfish-README.txt" > "$dest/README.txt"
echo "Stockfish $STOCKFISH_VERSION ($platform) staged in $dest" >&2
