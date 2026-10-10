#!/usr/bin/env bash
# Measures Pragma Chess on large databases (docs/tech/large-databases.md).
#
#   scripts/stress-databases.sh download 2013-01 2013-02 …   lichess months, into the corpus folder
#   scripts/stress-databases.sh join step5 2013-01 … 2013-10   one PGN of several months
#   scripts/stress-databases.sh convert step5                  PGN → .pdb, through Tools ▸ Convert
#   scripts/stress-databases.sh measure step5                  open it and time the board's filters
#   scripts/stress-databases.sh stop                           stops the client
#
# A Release client of its own (STRESS_BUILD, built here if missing), with an
# empty home (STRESS_HOME) emptied before each opening, offscreen, on its own
# port (STRESS_PORT): never the user's `make start`. The corpus is kept out of
# the repository (STRESS_CORPUS): it is never distributed.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
CORPUS="${STRESS_CORPUS:-$HOME/.cache/pragma-chess-stress}"
BUILD="${STRESS_BUILD:-$ROOT/build-release}"
HOME_DIR="${STRESS_HOME:-$CORPUS/home}"
PORT="${STRESS_PORT:-7492}"
API="127.0.0.1:$PORT"
CLIENT="$BUILD/gui/qt/pragma-chess"

now() { date +%s%N; }
since() { echo $((($(now) - $1) / 1000000)); }

build() {
    if [[ ! -x "$CLIENT" ]]; then
        cmake -S "$ROOT" -B "$BUILD" -G Ninja -DCMAKE_BUILD_TYPE=Release >/dev/null
    fi
    cmake --build "$BUILD" --target pragma-chess >/dev/null
}

stop() {
    for pid in $(pgrep -x pragma-chess || true); do
        tr '\0' '\n' <"/proc/$pid/environ" 2>/dev/null | grep -qx "PRAGMA_DEV_API_PORT=$PORT" || continue
        kill "$pid"
        for _ in $(seq 100); do kill -0 "$pid" 2>/dev/null || break; sleep 0.2; done
        kill -9 "$pid" 2>/dev/null || true
    done
}

# Starts a clean client: the home emptied once the old one has gone (it
# writes its session as it quits), the index cache kept as a user's is.
start() {
    build
    stop
    rm -rf "$HOME_DIR/.config" "$HOME_DIR/chess" "$HOME_DIR/.local"
    mkdir -p "$HOME_DIR"
    (cd "$HOME_DIR" && HOME="$HOME_DIR" XDG_CONFIG_HOME="$HOME_DIR/.config" PRAGMA_CHESS_DIR="$HOME_DIR/chess" \
        PRAGMA_LOBBY_RELAYS=ws://127.0.0.1:9 QT_QPA_PLATFORM=offscreen PRAGMA_DEV_API=1 PRAGMA_DEV_API_PORT="$PORT" \
        nohup "$CLIENT" >"$HOME_DIR/log" 2>&1 &)
    for _ in $(seq 60); do curl -s "$API/api" >/dev/null && return; sleep 1; done
    echo "the client did not start: see $HOME_DIR/log" >&2
    exit 1
}

json() { python3 -c "import json,sys; d=json.load(sys.stdin); print($1)"; }

case "${1:-}" in
download)
    shift
    mkdir -p "$CORPUS"
    for month in "$@"; do
        file="lichess_db_standard_rated_$month.pgn"
        [[ -f "$CORPUS/$file" ]] && continue
        curl -sf -o "$CORPUS/$file.zst" "https://database.lichess.org/standard/$file.zst"
        zstd -dq --rm "$CORPUS/$file.zst"
    done
    ;;
join)
    name="$2"
    shift 2
    files=()
    for month in "$@"; do files+=("$CORPUS/lichess_db_standard_rated_$month.pgn"); done
    cat "${files[@]}" >"$CORPUS/$name.pgn"
    ls -la "$CORPUS/$name.pgn"
    ;;
convert)
    pdb="$CORPUS/$2.pdb"
    rm -f "$pdb"
    start
    curl -s -X POST "$API/api/convert" -d "{\"pgn\": \"$CORPUS/$2.pgn\", \"pdb\": \"$pdb\"}" >/dev/null
    while curl -s "$API/api/convert" | grep -q '"running": true'; do sleep 2; done
    curl -s "$API/api/convert" | json "'ok', d['ok'], d['games'], 'games,', d['skipped'], 'skipped,', d['elapsedMs'] / 1000, 's,', int(d['gamesPerSecond']), 'a second', d['error']"
    ls -la "$pdb"
    ;;
measure)
    start
    begun=$(now)
    curl -s -X POST "$API/api/database" -d "{\"path\": \"$CORPUS/$2.pdb\"}" >/dev/null
    echo "window frozen on opening: $(since "$begun") ms"
    for _ in $(seq 1200); do curl -s "$API/api/profile" | grep -q '"ready": true' && break; sleep 0.5; done
    echo "position index ready after: $(since "$begun") ms"
    curl -s "$API/api/profile" | json "d['games'], 'games', d['ms'], d['index'], d['memory']"
    line() { curl -s "$API/api/profile" | json "'  listed', d['listed'], 'find', d['ms'].get('findGames'), 'filter', d['ms'].get('filterList')"; }
    for kind in position variant; do
        curl -s -X POST "$API/api/line" -d '{"moves": ""}' >/dev/null
        begun=$(now)
        curl -s -X POST "$API/api/category" -d "{\"kind\": \"$kind\"}" >/dev/null
        echo "$kind at the start: $(since "$begun") ms"; line
        # A Najdorf: 1.e4 c5 2.Nf3 d6 3.d4 cxd4 4.Nxd4 Nf6 5.Nc3 a6.
        for move in e2e4 c7c5 g1f3 d7d6 d2d4 c5d4 f3d4 g8f6 b1c3 a7a6; do
            begun=$(now)
            curl -s -X POST "$API/api/move" -d "{\"uci\": \"$move\"}" >/dev/null
            echo "$kind after $move: $(since "$begun") ms"; line
        done
    done
    begun=$(now)
    curl -s -X POST "$API/api/category" -d '{"kind": "all"}' >/dev/null
    echo "all games: $(since "$begun") ms"; line
    ;;
stop)
    stop
    ;;
*)
    sed -n '2,13p' "$0"
    exit 1
    ;;
esac
