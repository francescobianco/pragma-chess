#!/usr/bin/env bash
# Talks to a running Pragma Chess through its local API (Options ▸ Local API,
# or PRAGMA_API=1): the port and the token are read from api.json.
#
#   scripts/pragma-api.sh GET /api                     the routes
#   scripts/pragma-api.sh GET /api/state               position, engine, Explain
#   scripts/pragma-api.sh GET /api/screenshot > a.png  the window as drawn
#   scripts/pragma-api.sh POST /api/ply '{"ply": 16}'
#   scripts/pragma-api.sh POST /api/explain '{"on": true}'
#   scripts/pragma-api.sh POST /api/line '{"moves": "1.e4 e5 2.Nf3"}'
#
# PRAGMA_API_INFO points at another api.json (a test instance).
set -euo pipefail
info="${PRAGMA_API_INFO:-${XDG_DATA_HOME:-$HOME/.local/share}/Pragma/pragma-chess/api.json}"
[[ -r "$info" ]] || { echo "No $info: is Pragma Chess running with the local API on?" >&2; exit 2; }
read -r url token < <(python3 -c 'import json,sys; d=json.load(open(sys.argv[1])); print(d["url"], d["token"])' "$info")
method="${1:-GET}"; path="${2:-/api/state}"; body="${3:-}"
if [[ -n "$body" ]]; then
    curl -sS -X "$method" -H "Authorization: Bearer $token" -H 'Content-Type: application/json' --data "$body" "$url$path"
else
    curl -sS -X "$method" -H "Authorization: Bearer $token" "$url$path"
fi
