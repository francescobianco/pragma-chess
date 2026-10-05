#!/usr/bin/env bash
# Talks to a Pragma Chess started by `make start`, through its development
# API (127.0.0.1:7457; PRAGMA_DEV_API_URL for another address).
#
#   scripts/pragma-api.sh GET /api                     the routes
#   scripts/pragma-api.sh GET /api/state               position, engine, Explain
#   scripts/pragma-api.sh GET /api/screenshot > a.png  the window as drawn
#   scripts/pragma-api.sh POST /api/ply '{"ply": 16}'
#   scripts/pragma-api.sh POST /api/explain '{"on": true}'
#   scripts/pragma-api.sh POST /api/line '{"moves": "1.e4 e5 2.Nf3"}'
set -euo pipefail
url="${PRAGMA_DEV_API_URL:-http://127.0.0.1:${PRAGMA_DEV_API_PORT:-7457}}"
method="${1:-GET}"; path="${2:-/api/state}"; body="${3:-}"
if [[ -n "$body" ]]; then
    curl -sS -X "$method" -H 'Content-Type: application/json' --data "$body" "$url$path"
else
    curl -sS -X "$method" "$url$path"
fi
