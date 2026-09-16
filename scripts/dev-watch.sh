#!/usr/bin/env bash
# Development loop for the desktop client: build, launch, and on every source
# change rebuild and restart the app. If a build fails the running instance is
# left alone so you can fix the error and save again.
set -u

BUILD_DIR="${BUILD_DIR:-build}"
APP="$BUILD_DIR/gui/qt/pragma-chess"
WATCH_PATHS=(gui/qt CMakeLists.txt)
POLL_INTERVAL="${POLL_INTERVAL:-1}"

app_pid=""

log() { printf '\033[1;34m[dev]\033[0m %s\n' "$*"; }

stop_app() {
    if [[ -n "$app_pid" ]] && kill -0 "$app_pid" 2>/dev/null; then
        kill "$app_pid" 2>/dev/null
        wait "$app_pid" 2>/dev/null
    fi
    app_pid=""
}

start_app() {
    log "starting $APP"
    "$APP" "$@" &
    app_pid=$!
}

MARKER="$BUILD_DIR/.dev-watch-build-start"

# Files changed after the marker, i.e. while the last build was running.
changed_during_build() {
    [[ -n "$(find "${WATCH_PATHS[@]}" -type f -newer "$MARKER" -print -quit 2>/dev/null)" ]]
}

# Builds until the sources stop changing: edits saved while a build runs are
# not seen by the watcher, and would otherwise leave a half-updated app running.
build() {
    while true; do
        log "building…"
        touch "$MARKER"
        if ! cmake --build "$BUILD_DIR"; then
            log "build FAILED — keeping the previous instance running"
            return 1
        fi
        if changed_during_build; then
            log "sources changed during the build, building again"
            sleep 0.2
            continue
        fi
        log "build ok"
        return 0
    done
}

# Fingerprint of the watched files: path + modification time.
snapshot() {
    if command -v inotifywait >/dev/null; then
        return 0
    fi
    find "${WATCH_PATHS[@]}" -type f \( -name '*.cpp' -o -name '*.h' -o -name '*.txt' \
        -o -name '*.qrc' -o -name '*.ui' -o -name '*.svg' -o -name '*.png' \) \
        -printf '%T@ %p\n' 2>/dev/null | sort | md5sum
}

wait_for_change() {
    if command -v inotifywait >/dev/null; then
        inotifywait -qq -r -e close_write,create,delete,move "${WATCH_PATHS[@]}"
        sleep 0.2 # let editors finish writing (atomic saves, formatters)
        return
    fi
    local before
    before=$(snapshot)
    while [[ "$(snapshot)" == "$before" ]]; do
        sleep "$POLL_INTERVAL"
    done
}

trap 'echo; stop_app; exit 0' INT TERM

build && start_app "$@"
log "watching ${WATCH_PATHS[*]} (Ctrl+C to stop)"

while true; do
    wait_for_change
    log "change detected"
    if build; then
        stop_app
        start_app "$@"
    fi
done
