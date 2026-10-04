# Flatpak

`io.github.francescobianco.PragmaChess.yml` builds the desktop client for Flathub on the
KDE Qt 6 runtime: yaml-cpp as a module (CMake takes the system one over fetching it),
the official Stockfish binary installed where the bundled engine is looked for, Phone Link
off (libdatachannel is not in the runtime). Permissions: Wayland/X11, network (sources,
sync, online play) and the home folder (the chess folder, ChessBase files, other engines).

Build and install locally (needs `flatpak-builder` and the runtime):

```bash
flatpak install flathub org.kde.Platform//6.9 org.kde.Sdk//6.9
flatpak-builder --user --install --force-clean build-dir packaging/flatpak/io.github.francescobianco.PragmaChess.yml
flatpak run io.github.francescobianco.PragmaChess
```

Submitting: fork <https://github.com/flathub/flathub>, branch `new-pr` from the `new-pr`
branch, add the manifest at the repository root, open a PR; Flathub's bot builds it. Each
release then bumps the `tag` in the manifest of the `flathub/io.github.francescobianco.PragmaChess`
repository (Flathub creates it on acceptance). The metainfo in `gui/qt/data` must keep a
`<releases>` entry per version and screenshots with public URLs for the store pages.
