#!/usr/bin/env bash
# Development loop for the desktop client: build, launch, and on every source
# change rebuild and restart the app. If a build fails the running instance is
# left alone so you can fix the error and save again.
set -u

BUILD_DIR="${BUILD_DIR:-build}"
APP="$BUILD_DIR/gui/qt/pragma-chess"
[[ "$(uname)" == Darwin ]] && APP="$APP.app/Contents/MacOS/pragma-chess" # a bundle
WATCH_PATHS=(gui/qt smart CMakeLists.txt) # smart/: the SMART programs are built into the app
POLL_INTERVAL="${POLL_INTERVAL:-1}"
# make fresh-start: every launch is a first launch, in a home of its own
# emptied each time (settings, data, the chess folder), as if just installed.
FRESH_HOME="${FRESH_HOME:-}"

app_pid=""

log() { printf '\033[1;34m[dev]\033[0m %s\n' "$*"; }

stop_app() {
    if [[ -n "$app_pid" ]] && kill -0 "$app_pid" 2>/dev/null; then
        kill "$app_pid" 2>/dev/null
        wait "$app_pid" 2>/dev/null
    fi
    app_pid=""
}

# Empties FRESH_HOME and lays out a new user's home in it. The desktop's own
# settings (theme, dark mode, fonts, the folder names) are linked from the
# real home: a new user of the application is not a new user of the desktop.
fresh_home() {
    local real="$HOME" config="${XDG_CONFIG_HOME:-$HOME/.config}" item
    rm -rf "$FRESH_HOME"
    mkdir -p "$FRESH_HOME/.config" "$FRESH_HOME/.local/share" "$FRESH_HOME/.cache"
    for item in dconf gtk-3.0 gtk-4.0 fontconfig user-dirs.dirs user-dirs.locale kdeglobals; do
        [[ -e "$config/$item" ]] && ln -s "$config/$item" "$FRESH_HOME/.config/$item"
    done
    log "fresh home: $FRESH_HOME (the real one, $real, is not touched)"
}

start_app() {
    local env_fresh=()
    if [[ -n "$FRESH_HOME" ]]; then
        fresh_home
        env_fresh=(-u PRAGMA_CHESS_DIR HOME="$FRESH_HOME" XDG_CONFIG_HOME="$FRESH_HOME/.config"
                   XDG_DATA_HOME="$FRESH_HOME/.local/share" XDG_CACHE_HOME="$FRESH_HOME/.cache"
                   XDG_STATE_HOME="$FRESH_HOME/.local/state")
    fi
    # Another build (make build, a test run) may still be writing the binary:
    # launching it then fails with "Text file busy" (exit 126), so wait and retry.
    local attempt status
    for attempt in $(seq 1 40); do
        log "starting $APP (development API on 127.0.0.1:${PRAGMA_DEV_API_PORT:-7457})"
        # An activation token is good for one launch, and this one came with
        # the shell (a terminal, an IDE): handed to every restart, GNOME
        # refuses it and marks the window as demanding attention, which keeps
        # the Ubuntu Dock out over it.
        env -u XDG_ACTIVATION_TOKEN -u DESKTOP_STARTUP_ID \
            ${env_fresh[@]+"${env_fresh[@]}"} PRAGMA_DEV_API=1 "$APP" "$@" &
        app_pid=$!
        sleep 0.3
        if kill -0 "$app_pid" 2>/dev/null; then
            return 0
        fi
        wait "$app_pid" 2>/dev/null
        status=$?
        app_pid=""
        if [[ $status -ne 126 ]]; then
            log "the app exited at once (status $status)"
            return 1
        fi
        log "the binary is busy (another build is writing it), retrying…"
        sleep 0.5
    done
    log "could not start $APP: still busy"
    return 1
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

# Fingerprint of the watched files: path + modification time. BSD find (macOS)
# has no -printf, so stat prints the times there.
snapshot() {
    find "${WATCH_PATHS[@]}" -type f \( -name '*.cpp' -o -name '*.h' -o -name '*.txt' \
        -o -name '*.qrc' -o -name '*.ui' -o -name '*.svg' -o -name '*.png' -o -name '*.smart' \) \
        -print0 2>/dev/null | sort -z | xargs -0 stat $STAT_FORMAT 2>/dev/null | cksum
}
if stat -c '%Y' / >/dev/null 2>&1; then STAT_FORMAT="-c %Y:%n"; else STAT_FORMAT="-f %m:%N"; fi

wait_for_change() {
    if command -v inotifywait >/dev/null; then # Linux
        inotifywait -qq -r -e close_write,create,delete,move "${WATCH_PATHS[@]}"
        sleep 0.2 # let editors finish writing (atomic saves, formatters)
        return
    fi
    if command -v fswatch >/dev/null; then # macOS (FSEvents)
        fswatch -1 -r "${WATCH_PATHS[@]}" >/dev/null
        sleep 0.2
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
