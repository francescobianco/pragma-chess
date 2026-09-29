# Pragma Chess for Android

A small companion app: it keeps the databases of a paired Pragma Chess
computer on the phone, reads them offline, and is a place to enter games
(played or typed on the board) that go back into the computer's databases.
Without a computer it has one local database, "My Games" ("Le mie partite"),
and more can be created.

The protocol with the computer is [docs/phone-link.md](../../docs/phone-link.md):
pairing by QR code (Options ▸ Connect Mobile App… on the computer), signaling
over public Nostr relays, files and games over a WebRTC data channel.

## Build

Needs JDK 17 and the Android SDK (compileSdk 36, minSdk 26); `local.properties`
points at the SDK (`sdk.dir=…`).

```bash
./gradlew assembleDebug                      # app/build/outputs/apk/debug/app-debug.apk
./gradlew testDebugUnitTest                  # JVM unit tests
adb install -r app/build/outputs/apk/debug/app-debug.apk
./gradlew assembleDebug -Ppragma.stockfish=false   # without the engine (smaller, offline build)
```

Stockfish (`sf_19`, the official `stockfish-android-arm64-universal` build,
GPL v3) is downloaded by the `fetchStockfish` task, checked against its
SHA-256, cached in `~/.gradle/caches/pragma-chess/` and shipped as
`jniLibs/arm64-v8a/libstockfish.so`, executed from `nativeLibraryDir`. It is
never committed. Its license is shown in Settings ▸ Licenses.

## Architecture

Kotlin 2.2, Jetpack Compose with Material 3, AGP 8.13. Packages under
`app/src/main/java/org/pragmachess/mobile/`:

| Package | What |
|---------|------|
| `chess` | `Position` (legal moves, check/mate/stalemate, SAN, FEN; perft-tested), `GameLine` |
| `crypto` | secp256k1 keys and BIP-340 (secp256k1-kmp), `Nip44` v2 with a hand-written `ChaCha20` (official vectors), NIP-01 `NostrEvent` |
| `link` | `PairingLink`, `PhoneIdentity` (the phone's key), `Signaling` (relays over OkHttp WebSockets), `PeerLink` (WebRTC, stream-webrtc-android), `ComputerSync` (put the outbox, then pull changed files: temp file, size + SHA-256, atomic replace, outbox reapplied) |
| `data` | `PdbDatabase` (the desktop's `.pdb` schema, version 4, android.database.sqlite), `Library` (files per location), `AppStore` (paired computers, outbox) |
| `engine` | `UciEngine` (Stockfish as a UCI process), `Analysis` (scores from White's point of view) |
| `ui` | `AppViewModel`, side menu (`PragmaApp`), `BoardScreen` with `Board`, `EvaluationBar`, `MoveList` (SkakNew figurines), games list, computers, settings, licenses |

Databases live in the app's private files: `databases/local/` for the ones
made on the phone, `databases/<computer pubkey>/` for each computer's copies.

## Not done yet

- The keys are in app-private storage, not in the Android Keystore.
- No PGN import/export on the phone, no variations or comments in the move list.
- Engine only on arm64 devices (no 32-bit or x86 build is bundled).
- `SignalingRelayTest` needs public relays and runs only with `PRAGMA_RELAY_TEST=1`.
