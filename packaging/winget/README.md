# winget

Manifests for the Windows Package Manager (`winget install FrancescoBianco.PragmaChess`),
submitted to [microsoft/winget-pkgs](https://github.com/microsoft/winget-pkgs) under
`manifests/f/FrancescoBianco/PragmaChess/<version>/`. Three files per version, schema 1.6.0:
the version manifest, the installer manifest (the `-setup.exe` of the release, Inno Setup,
with its SHA-256 and the product code, which is the installer's AppId) and the English locale.

`make-winget.sh <version>` writes them from a published release (it downloads the installer
to hash it); then fork winget-pkgs, copy the folder, open a PR titled
"New package: FrancescoBianco.PragmaChess version <version>" (or "New version: …").
The AppId never changes, so updates find the installed copy.
