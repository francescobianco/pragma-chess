#!/usr/bin/env bash
# Where Pragma Chess stands: stars, downloads, visits and where they come
# from, and every pull request and discussion we opened elsewhere
# (DISTRIBUTING.md, "What to measure"). Prints Markdown; with --csv FILE it
# also appends the day's numbers to FILE (its header written when new).
#   scripts/distribution-status.sh [--csv distribution.csv]
# Needs gh, signed in. Visits and clones need push access to the repository
# (GitHub keeps them 14 days only, hence the CSV): without it they are left out.
set -euo pipefail
repo="${PRAGMA_REPO:-francescobianco/pragma-chess}"
owner="${repo%/*}"
csv=
[[ "${1:-}" == --csv ]] && csv="${2:?--csv needs a file}"
today="$(date -u +%F)"

read -r stars forks watchers issues < <(gh api "repos/$repo" \
    -q '"\(.stargazers_count) \(.forks_count) \(.subscribers_count) \(.open_issues_count)"')

# Downloads of the installers (not checksums, sources or version.json), per
# release and per system; version.json counts the copies asking for updates.
releases="$(gh api "repos/$repo/releases?per_page=100")"
downloads() { jq -r "$1" <<< "$releases"; }
total="$(downloads '[.[].assets[] | select(.name | test("\\.(exe|zip|dmg|deb|rpm|apk|AppImage)$")) | .download_count] | add // 0')"
latest="$(downloads '[.[] | select(.prerelease | not)][0].tag_name // ""')"
per_system="$(jq -r --arg latest "$latest" '
    [.[] | select(.tag_name == $latest) | .assets[]] as $a
    | def n(r): [$a[] | select(.name | test(r)) | .download_count] | add // 0;
    "Windows \(n("windows")), macOS \(n("macos")), Linux \(n("\\.(deb|rpm|AppImage)$")), Android \(n("\\.apk$")), update checks \(n("^version\\.json$"))"' <<< "$releases")"

views= clones= referrers=
if traffic="$(gh api "repos/$repo/traffic/views" 2>/dev/null)"; then
    views="$(jq -r '"\(.count) (\(.uniques))"' <<< "$traffic")"
    clones="$(gh api "repos/$repo/traffic/clones" -q '"\(.count) (\(.uniques))"')"
    referrers="$(gh api "repos/$repo/traffic/popular/referrers" -q 'map("\(.referrer) \(.count) (\(.uniques))") | join(", ")')"
fi

echo "# Pragma Chess, $today"
echo
echo "| Stars | Forks | Watchers | Open issues | Downloads (all) | Visits, 14 days | Clones, 14 days |"
echo "|---|---|---|---|---|---|---|"
echo "| $stars | $forks | $watchers | $issues | $total | ${views:-—} | ${clones:-—} |"
echo
echo "**$latest**: $per_system."
[[ -n "$referrers" ]] && echo && echo "**Referrers**: $referrers."

# What we asked of others about Pragma Chess and is still open: pull requests
# and discussions that name it (the author's other work stays out).
echo
echo "## Waiting elsewhere"
echo
gh api -X GET search/issues -f q="\"Pragma Chess\" in:title,body author:$owner is:pr is:open -user:$owner" -f per_page=50 \
    -q '.items[] | "- [\(.repository_url | sub(".*/repos/"; ""))#\(.number)](\(.html_url)) \(.title) — opened \(.created_at[:10]), \(.comments) comments"'
gh api graphql -f query='query($q:String!){search(query:$q,type:DISCUSSION,first:20){nodes{... on Discussion{url title createdAt closed comments{totalCount} repository{nameWithOwner}}}}}' \
    -f q="\"Pragma Chess\" in:title,body author:$owner -user:$owner" \
    -q '.data.search.nodes[] | select(.closed | not) | "- [\(.repository.nameWithOwner) discussion](\(.url)) \(.title) — opened \(.createdAt[:10]), \(.comments.totalCount) comments"'

if [[ -n "$csv" ]]; then
    [[ -s "$csv" ]] || echo "date,stars,forks,watchers,open_issues,downloads,views,view_uniques,clones,clone_uniques" > "$csv"
    split() { [[ -n "$1" ]] && sed -E 's/^([0-9]+) \(([0-9]+)\)$/\1,\2/' <<< "$1" || echo ","; }
    echo "$today,$stars,$forks,$watchers,$issues,$total,$(split "$views"),$(split "$clones")" >> "$csv"
fi
