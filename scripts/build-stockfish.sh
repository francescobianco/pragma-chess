#!/usr/bin/env bash
# Builds the Stockfish pinned in packaging/stockfish.env from its source, with
# packaging/stockfish/small-net.patch (the small network only: an engine of
# about 4 MB instead of 100), and stages what Pragma Chess ships as its default
# engine:
#
#   <dest>/stockfish[.exe]   the engine
#   <dest>/Copying.txt       its license (GPL v3)
#   <dest>/AUTHORS
#   <dest>/README.txt        what it is, what we changed, where its source is
#
#   scripts/build-stockfish.sh <dest> [linux-x86-64|linux-arm64|macos|windows-x86-64]
#   scripts/build-stockfish.sh --source <dest-dir>   the source we build, patched,
#                                                    with the network
#
# The platform defaults to the machine running the script; windows-x86-64 also
# builds on Linux with MinGW (x86_64-w64-mingw32-g++). Needs make and a C++17
# compiler. Downloads are cached in ${STOCKFISH_CACHE:-~/.cache/pragma-chess}.
set -euo pipefail

root=$(cd "$(dirname "$0")/.." && pwd)
# shellcheck source=../packaging/stockfish.env
. "$root/packaging/stockfish.env"
cache=${STOCKFISH_CACHE:-${XDG_CACHE_HOME:-$HOME/.cache}/pragma-chess}
patch_file=$root/packaging/stockfish/small-net.patch
mkdir -p "$cache"

sha256() {
    if command -v sha256sum > /dev/null; then sha256sum "$1" | cut -d' ' -f1
    else shasum -a 256 "$1" | cut -d' ' -f1; fi
}

# download <file> <sha256> <url>...: the first URL that gives the right file
download() {
    local file=$1 expected=$2 url
    shift 2
    [ -f "$file" ] && [ "$(sha256 "$file")" = "$expected" ] && return
    for url in "$@"; do
        echo "Downloading $url" >&2
        if curl -fL --retry 3 -o "$file.part" "$url" && [ "$(sha256 "$file.part")" = "$expected" ]; then
            mv "$file.part" "$file"
            return
        fi
        echo "failed or wrong SHA-256: $url" >&2
    done
    rm -f "$file.part"
    exit 1
}

work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT

# The patched source tree, with the network in src/ where the build embeds it.
source_tree() {
    local archive=$cache/stockfish-$STOCKFISH_TAG.tar.gz
    download "$archive" "$STOCKFISH_SOURCE_SHA256" \
        "https://github.com/official-stockfish/Stockfish/archive/refs/tags/$STOCKFISH_TAG.tar.gz"
    download "$cache/$STOCKFISH_NET" "$STOCKFISH_NET_SHA256" \
        "https://tests.stockfishchess.org/api/nn/$STOCKFISH_NET" \
        "https://github.com/official-stockfish/networks/raw/master/$STOCKFISH_NET"
    tar -xzf "$archive" -C "$work"
    src=$work/stockfish-$STOCKFISH_TAG
    mv "$work/Stockfish-$STOCKFISH_TAG" "$src"
    patch -d "$src" -p1 --quiet < "$patch_file"
    cp "$patch_file" "$src/PRAGMA-CHESS.patch"
    cp "$cache/$STOCKFISH_NET" "$src/src/"
}

if [ "${1:-}" = --source ]; then
    dest=${2:?usage: build-stockfish.sh --source <dest-dir>}
    mkdir -p "$dest"
    source_tree
    out=$dest/stockfish-$STOCKFISH_TAG-pragma-source.tar.gz
    tar -czf "$out" -C "$work" "stockfish-$STOCKFISH_TAG"
    ls -l "$out"
    exit 0
fi

dest=${1:?usage: build-stockfish.sh <dest> [platform]}
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
# One binary for every processor of the platform: SSE4.1 and POPCNT are in
# every x86-64 processor since 2008, AVX2 is not.
ldflags=-s
case $platform in
    linux-x86-64) arch=x86-64-sse41-popcnt; comp=gcc; exe=stockfish ;;
    linux-arm64) arch=armv8; comp=gcc; exe=stockfish ;;
    macos) arch=apple-silicon; comp=clang; exe=stockfish; ldflags= ;;
    windows-x86-64) arch=x86-64-sse41-popcnt; comp=mingw; exe=stockfish.exe ;;
    *) echo "unknown platform: $platform" >&2; exit 2 ;;
esac

source_tree
jobs=$(getconf _NPROCESSORS_ONLN 2> /dev/null || echo 2)
# "all", not "build": "build" first runs net.sh, which downloads the big network.
make -C "$src/src" -j "$jobs" all ARCH="$arch" COMP="$comp" EXTRALDFLAGS="$ldflags" > "$work/build.log" 2>&1 ||
    { cat "$work/build.log" >&2; exit 1; }
[ "$platform" = macos ] && strip "$src/src/$exe"

mkdir -p "$dest"
cp "$src/src/$exe" "$dest/$exe"
chmod +x "$dest/$exe"
cp "$src/Copying.txt" "$src/AUTHORS" "$dest"/
sed -e "s/@VERSION@/$STOCKFISH_VERSION/g" -e "s/@TAG@/$STOCKFISH_TAG/g" -e "s/@NET@/$STOCKFISH_NET/g" \
    "$root/packaging/stockfish/README.txt" > "$dest/README.txt"
echo "Stockfish $STOCKFISH_VERSION ($platform, $arch) staged in $dest: $(wc -c < "$dest/$exe") bytes" >&2
