#!/usr/bin/env bash
# Writes the winget manifests of a published release into packaging/winget/<version>/.
#   packaging/winget/make-winget.sh 0.2.0
set -euo pipefail
version="${1:?version, e.g. 0.2.0}"
here="$(cd "$(dirname "$0")" && pwd)"
url="https://github.com/francescobianco/pragma-chess/releases/download/v${version}/PragmaChess-${version}-windows-x64-setup.exe"
tmp="$(mktemp)"
trap 'rm -f "$tmp"' EXIT
curl -sSL -o "$tmp" "$url"
sha="$(sha256sum "$tmp" | cut -d' ' -f1 | tr a-f A-F)"
date="$(gh release view "v${version}" --json publishedAt -q '.publishedAt' 2>/dev/null | cut -c1-10 || date +%F)"
out="$here/$version"
mkdir -p "$out"
cat > "$out/FrancescoBianco.PragmaChess.yaml" <<YAML
# yaml-language-server: \$schema=https://aka.ms/winget-manifest.version.1.6.0.schema.json
PackageIdentifier: FrancescoBianco.PragmaChess
PackageVersion: ${version}
DefaultLocale: en-US
ManifestType: version
ManifestVersion: 1.6.0
YAML
cat > "$out/FrancescoBianco.PragmaChess.installer.yaml" <<YAML
# yaml-language-server: \$schema=https://aka.ms/winget-manifest.installer.1.6.0.schema.json
PackageIdentifier: FrancescoBianco.PragmaChess
PackageVersion: ${version}
InstallerType: inno
Scope: user
InstallModes:
- interactive
- silent
- silentWithProgress
UpgradeBehavior: install
ProductCode: '{6F1B7C2E-4A8D-4E57-9B0C-3D2A8E5F7C41}_is1'
ReleaseDate: ${date}
Installers:
- Architecture: x64
  InstallerUrl: ${url}
  InstallerSha256: ${sha}
ManifestType: installer
ManifestVersion: 1.6.0
YAML
cat > "$out/FrancescoBianco.PragmaChess.locale.en-US.yaml" <<YAML
# yaml-language-server: \$schema=https://aka.ms/winget-manifest.defaultLocale.1.6.0.schema.json
PackageIdentifier: FrancescoBianco.PragmaChess
PackageVersion: ${version}
PackageLocale: en-US
Publisher: Francesco Bianco
PublisherUrl: https://yafb.net/pragma-chess/
PublisherSupportUrl: https://github.com/francescobianco/pragma-chess/issues
PackageName: Pragma Chess
PackageUrl: https://yafb.net/pragma-chess/
License: MIT
LicenseUrl: https://github.com/francescobianco/pragma-chess/blob/main/LICENSE
Copyright: Copyright (c) Francesco Bianco
ShortDescription: Chess database for studying, training and playing, with Explain
Description: |-
  An open source chess database for studying, training and playing: light, simple, with what really matters.
  Keep your games in databases (PGN, lichess.org and chess.com import, ChessBase files) and study them with Stockfish or any UCI engine. Explain shows on the board why the last move is good or bad. Train against the engine, which plays from an opening book whose weights you tune, with a tutor that stops you on mistakes. Play on lichess.org from the same board. Variations, annotations, Polyglot books, sync between computers. English and Italian.
Moniker: pragma-chess
Tags:
- chess
- chess-database
- pgn
- stockfish
- uci
- lichess
- polyglot
- qt
ReleaseNotesUrl: https://github.com/francescobianco/pragma-chess/releases/tag/v${version}
ManifestType: defaultLocale
ManifestVersion: 1.6.0
YAML
echo "written $out"
