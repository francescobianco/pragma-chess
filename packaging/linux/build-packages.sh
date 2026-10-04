#!/usr/bin/env bash
# Builds the .deb or .rpm package of Pragma Chess against the distribution's Qt.
#
#   packaging/linux/build-packages.sh deb|rpm [version]
#
# Run it on the distribution the package is for (CI uses Ubuntu 24.04 for the
# .deb and Fedora for the .rpm). The packages land in dist/.
set -euo pipefail

kind=${1:?usage: build-packages.sh deb|rpm [version]}
version=${2:-}
root=$(cd "$(dirname "$0")/../.." && pwd)
build=${BUILD_DIR:-$root/build-package-$kind}
dist=$root/dist

case $kind in
    deb) generator=DEB ;;
    rpm) generator=RPM ;;
    *) echo "unknown package kind: $kind" >&2; exit 2 ;;
esac

args=(-S "$root" -B "$build" -G Ninja
      -DCMAKE_BUILD_TYPE=Release
      -DCMAKE_INSTALL_PREFIX=/usr)
[ -n "$version" ] && args+=("-DPRAGMA_VERSION=$version")

cmake "${args[@]}"
# The bundled engine, installed by CMake from where it is staged.
"$root/scripts/build-stockfish.sh" "$build/gui/qt/engines"
answer=$( (printf 'uci\nisready\ngo depth 10\n'; sleep 3; echo quit) | "$build/gui/qt/engines/stockfish")
grep -q '^bestmove' <<< "$answer" || { echo "the bundled engine does not answer: $answer" >&2; exit 1; }
cmake --build "$build"
if [ "${SKIP_TESTS:-0}" != 1 ]; then
    QT_QPA_PLATFORM=offscreen ctest --test-dir "$build" --output-on-failure
fi

(cd "$build" && cpack -G "$generator")
mkdir -p "$dist"
cp "$build"/*."$kind" "$dist"/
ls -l "$dist"/*."$kind"
