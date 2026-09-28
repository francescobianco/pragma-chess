## Install

| System | Download | |
|---|---|---|
| **Windows** 10/11 (64-bit) | `PragmaChess-@VERSION@-windows-x64-setup.exe` | Installer. A portable `.zip` is also available. |
| **macOS** 12+ on Apple Silicon | `PragmaChess-@VERSION@-macos-arm64.dmg` | Open it and drag Pragma Chess to Applications. |
| **Ubuntu** 24.04+ / **Debian** 13+ | `pragma-chess_@VERSION@_amd64.deb` | `sudo apt install ./pragma-chess_@VERSION@_amd64.deb` |
| **Fedora** | `pragma-chess-@VERSION@-1.*.x86_64.rpm` | `sudo dnf install ./pragma-chess-@VERSION@-1.*.x86_64.rpm` |

An engine is not bundled: install [Stockfish](https://stockfishchess.org/download/)
(or any UCI engine) and Pragma Chess finds it. The Linux packages recommend it.

**macOS:** the app is not notarized by Apple yet. The first time, macOS says it
cannot check it: open *System Settings ▸ Privacy & Security* and click
*Open Anyway* (or run `xattr -dr com.apple.quarantine "/Applications/Pragma Chess.app"`).

**Windows:** SmartScreen may warn about an unrecognized app: click
*More info ▸ Run anyway*.

`SHA256SUMS.txt` lists the checksums of every file.
