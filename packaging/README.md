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
  asks little — the language is the system's, it installs for the user
  (`/ALLUSERS` for everyone), the folder only on a first install, no license
  to accept (`LICENSE.txt` goes with the program), no summary: welcome,
  folder, options (desktop shortcut, `.pch`/`.pdb` associations), the
  installation, the end, in its own words in its six languages. Every page
  carries the welcome window's shoulder (`[Code]`: the window grows by the
  wizard image and the rest moves right of it). Its `AppId` must never
  change, or upgrades stop finding the installed copy.
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
analyses right away. It is **our own build**, to keep the packages small: the
official one is about 100 MB, almost all of it the big evaluation network, and
the packages must stay under 20 MB. `stockfish.env` pins the release (tag,
SHA-256 of its source archive) and its small network (3.5 MB);
`scripts/build-stockfish.sh <dir> [platform]` downloads and checks both,
applies `stockfish/small-net.patch` (embed and use only the small network;
the big one is never loaded and its unused structures are shrunk, so the
engine also needs less memory), builds it and stages the executable (about
4 MB) with `Copying.txt`, `AUTHORS` and `README.txt` (from
`stockfish/README.txt`). Every build script runs it and checks that the
engine answers; `make stockfish` does the same for a development build.

| Platform | Built with | Where the engine goes |
|---|---|---|
| Windows | MinGW on Linux (job `engine-windows`), `x86-64-sse41-popcnt` | `<app>\engines\stockfish.exe` |
| macOS | clang, `apple-silicon` | `Contents/MacOS/stockfish` (signed with the app), texts in `Contents/Resources/engines/` |
| Linux | gcc, `x86-64-sse41-popcnt` | `/usr/lib/pragma-chess/engines/stockfish` |

One binary per platform, for every processor of it: SSE4.1 and POPCNT are in
every x86-64 processor since 2008, AVX2 is not.

`EngineCatalog::bundledEngineDirs` looks in these places
(`PRAGMA_ENGINES_DIR` overrides them); a build without the engine falls back
to a `stockfish` installed on the system.

**License.** Pragma Chess is MIT, Stockfish is GPL v3. Stockfish runs as a
separate program spoken to over UCI, which the GPL treats as mere
aggregation. What the GPL asks, and what we do:

- the GPL text and a notice ship next to the binary (`Copying.txt`,
  `README.txt`), saying that it is modified and how;
- the complete corresponding source (`stockfish-<tag>-pragma-source.tar.gz`:
  the release's source with the patch applied, the patch itself as
  `PRAGMA-CHESS.patch`, and the network the binary embeds) is attached to
  every release by the workflow (`build-stockfish.sh --source`), from the
  same place as the installers.

To move to a new Stockfish, update `stockfish.env` (tag, SHA-256 of
`archive/refs/tags/<tag>.tar.gz`, the small network named in `src/evaluate.h`)
and the Flatpak manifest, and refresh the patch against the new source. A
release that has no small network any more (Stockfish 19 has only one, of
100 MB) cannot be shipped this way.

## Packages that start

A package is checked before it is published, because a DLL that the build
machine has and a user does not is invisible to the tests: 0.2.0 shipped a
Windows client that did not start (`libssl-3-x64.dll` missing, needed by
Phone Link's libdatachannel).

- **Windows** (`build.ps1`): every `.exe` and `.dll` of the folder is read
  with `dumpbin /dependents`; whatever it imports must be in the folder or in
  System32, and the Visual C++ runtime and OpenSSL are always copied in from
  the build machine (never assumed to be in System32). A DLL that cannot be
  found fails the build. Then the engine must answer `uci` and
  `pragma-chess.exe` must still be running after 15 seconds, both started
  with a PATH holding only Windows and the "DLL not found" dialog turned into
  an exit code.
- **macOS** (`build.sh`): no executable or library of the bundle may load
  anything outside it but macOS's own (`otool -L`), the engine must answer,
  and the signed application must still be running after 15 seconds.
- **Linux** (`build-packages.sh`): the engine must answer; the libraries are
  the distribution's, declared by the package.

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
wizard pictures — the shoulder of every page is the welcome window's: the
study's picture, the rich logo, "Pragma Chess" in the book face and the
motto, in English, the picture being one for every language —; the results
are committed.

**Trying the Windows installer on Linux.** Inno Setup runs under Wine: in a
prefix of its own (`WINEPREFIX=…`), install `innosetup-6.x.exe /VERYSILENT
/DIR=C:\Inno`, unzip a release's portable zip as the program, and compile
from `packaging/windows`: `wine 'C:\Inno\ISCC.exe' /DAppVersion=x.y.z
"/DSourceDir=Z:<unzipped>/Pragma Chess" /DOutputDir=Z:<folder>
pragma-chess.iss`. Run the setup with `LANG=it_IT.UTF-8 wine …` in a fresh
prefix to see a first install as an Italian user does (Wine's fonts are not
Windows', so a last look on Windows is still worth it).
