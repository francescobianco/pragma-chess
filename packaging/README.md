# Packaging

Everything that turns the source into installers. The GitHub workflow
[`.github/workflows/release.yml`](../.github/workflows/release.yml) runs these
scripts; they work the same on a machine of the right kind.

| Package | Script | Built on | Output (`dist/`) |
|---|---|---|---|
| Windows installer + portable zip | `windows/build.ps1` | Windows, MSVC 2022, Qt 6 for MSVC, Inno Setup 6 | `PragmaChess-<v>-windows-x64-setup.exe`, `…-portable.zip` |
| macOS disk image (Apple Silicon) | `macos/build.sh` | macOS, Qt 6, `brew install create-dmg` | `PragmaChess-<v>-macos-arm64.dmg` |
| Debian / Ubuntu | `linux/build-packages.sh deb` | Ubuntu 24.04 (`make deps`) | `pragma-chess_<v>_amd64.deb` |
| Fedora | `linux/build-packages.sh rpm` | Fedora, `qt6-qtbase-devel qt6-qtsvg-devel qt6-qttools-devel rpm-build` | `pragma-chess-<v>-1.<dist>.x86_64.rpm` |

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

## Making a release

1. Set the version in `PRAGMA_VERSION` (top-level `CMakeLists.txt`) and commit.
2. `git tag v0.2.0 && git push origin v0.2.0`
3. The workflow builds the four packages and publishes release `v0.2.0` with
   them, `SHA256SUMS.txt` and the notes of `release-notes.md` followed by
   the changes since the previous tag. A tag with a suffix (`v0.2.0-beta.1`)
   makes a pre-release.

Check it with `gh run watch` and `gh release view v0.2.0`. To try the
packaging without releasing, run the workflow by hand
(`gh workflow run release.yml`) and download the artifacts of the run.

## Artwork

`assets/make-installer-art.py` draws the disk image background and the
wizard pictures from the application icon; the results are committed.
