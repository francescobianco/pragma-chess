# AUR

`pragma-chess/` is the package for the Arch User Repository: it builds the
tagged release from source against Arch's Qt, and offers Arch's Stockfish as
the bundled engine (a link in `/usr/lib/pragma-chess/engines`).

Each release: set `pkgver`, put the new tarball's `sha256sum` in
`sha256sums`, reset `pkgrel` to 1, and write `.SRCINFO` again
(`makepkg --printsrcinfo > .SRCINFO` on an Arch machine; by hand otherwise,
it mirrors the PKGBUILD). Try it with `makepkg -si` before publishing.

To publish (an AUR account with an SSH key):

```bash
git clone ssh://aur@aur.archlinux.org/pragma-chess.git aur-pragma-chess
cp packaging/aur/pragma-chess/{PKGBUILD,.SRCINFO} aur-pragma-chess/
cd aur-pragma-chess && git add PKGBUILD .SRCINFO && git commit -m "pragma-chess 0.3.0" && git push
```
