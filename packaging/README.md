# Packaging

Everything that turns the source into installers. The GitHub workflow
[`.github/workflows/release.yml`](../.github/workflows/release.yml) runs these
scripts; they work the same on a machine of the right kind.

| Package | Script | Built on | Output (`dist/`) |
|---|---|---|---|
| Windows installer + portable zip | `windows/build.ps1` | Windows, MSVC 2022, Qt 6 for MSVC, Inno Setup 6 | `PragmaChess-<v>-windows-x64-setup.exe`, `…-portable.zip` |
| macOS disk image (Apple Silicon) | `macos/build.sh` | macOS, Qt 6, `brew install create-dmg` | `PragmaChess-<v>-macos-arm64.dmg` |
| Debian / Ubuntu | `linux/build-packages.sh deb` | Ubuntu 24.04 (`make deps`) | `pragma-chess_<v>_amd64.deb` |
| Fedora | `linux/build-packages.sh rpm` | Fedora, `qt6-qtbase-devel qt6-qtsvg-devel qt6-qttools-devel openssl-devel rpm-build` | `pragma-chess-<v>-1.<dist>.x86_64.rpm` |
| Flathub | `flatpak/` (manifest, see its README) | `flatpak-builder`, KDE 6.9 runtime | Flathub app `io.github.francescobianco.PragmaChess` |
| winget | `winget/make-winget.sh <v>` (manifests, see its README) | `curl`, `gh` | PR to microsoft/winget-pkgs |

Each script takes the version as its argument (default: `PRAGMA_VERSION` in
the top-level `CMakeLists.txt`), builds in Release, runs the tests
(`SKIP_TESTS=1` skips them) and packages.

- **Windows** and **macOS** ship their own Qt (`windeployqt`, `macdeployqt`),
  keeping only the SQLite driver. The Windows installer (`windows/pragma-chess.iss`)
  installs per user or for everyone, adds Start menu and optional desktop
  shortcuts and can associate `.pch` projects; its `AppId` must never change,
  or upgrades stop finding the installed copy.
- **Linux** packages use the distribution's Qt; CPack settings are in
  `linux/Packaging.cmake`, and the files installed are those of
  `cmake --install` (binary, `.desktop`, icons, AppStream metainfo).
- **Signing.** The macOS app is signed ad hoc unless the repository has the
  secrets `MACOS_CERTIFICATE` (base64 `.p12` of a Developer ID Application
  certificate), `MACOS_CERTIFICATE_PASSWORD` and `MACOS_SIGN_IDENTITY`; with
  `APPLE_ID`, `APPLE_TEAM_ID` and `APPLE_APP_PASSWORD` it is also notarized.
  The Windows installer is not signed.

## Bundled engine (Stockfish)

Every package ships Stockfish as the default engine, so a new installation
analyses right away. The release is pinned in `stockfish.env` (version, tag,
asset names and SHA-256); `scripts/fetch-stockfish.sh <dir> [platform]`
downloads the official "universal" build (one binary with runtime CPU
dispatch), checks the hash and stages the executable with `Copying.txt`,
`AUTHORS` and `README.txt` (from `stockfish-README.txt`). Every build script
runs it; `make stockfish` does the same for a development build.

| Platform | Where the engine goes |
|---|---|
| Windows | `<app>\engines\stockfish.exe` |
| macOS | `Contents/MacOS/stockfish` (signed with the app), texts in `Contents/Resources/engines/` |
| Linux | `/usr/lib/pragma-chess/engines/stockfish` |

`EngineCatalog::bundledEngineDirs` looks in these places
(`PRAGMA_ENGINES_DIR` overrides them); a build without the engine falls back
to a `stockfish` installed on the system.

**License.** Pragma Chess is MIT, Stockfish is GPL v3. Stockfish runs as a
separate program spoken to over UCI, which the GPL treats as mere
aggregation. What the GPL asks, and what we do:

- the GPL text and a notice ship next to the binary (`Copying.txt`, `README.txt`);
- the binary is the unmodified official build;
- the complete corresponding source (`stockfish-<tag>-source.tar.gz`, with
  the network files the binary embeds) is attached to every release by the
  workflow (`fetch-stockfish.sh --source`), from the same place as the installers.

To move to a new Stockfish, update `stockfish.env` (tag, asset names, the
SHA-256 GitHub publishes for each asset).

## Making a release

1. Set the version in `PRAGMA_VERSION` (top-level `CMakeLists.txt`), add its
   section to [`CHANGELOG.md`](../CHANGELOG.md) (`## [0.2.0] - <date>`) and commit.
2. `git tag v0.2.0 && git push origin v0.2.0`
3. The workflow builds the four packages and publishes release `v0.2.0` with
   them, `SHA256SUMS.txt` and notes made of that version's section of
   `CHANGELOG.md` ("What's new"), `release-notes.md` and the changes since
   the previous tag. A tag with a suffix (`v0.2.0-beta.1`) makes a pre-release.

Each package is attached twice: with the version in its name and without
it (`latest-names.sh`), so `https://github.com/francescobianco/pragma-chess/releases/latest/download/<name>` always downloads the latest release
(the README links there). Keep those names stable: they are public links.

Check it with `gh run watch` and `gh release view v0.2.0`. To try the
packaging without releasing, run the workflow by hand
(`gh workflow run release.yml`) and download the artifacts of the run.

## Artwork

`assets/make-installer-art.py` draws the disk image background and the
wizard pictures from the application icon; the results are committed.
