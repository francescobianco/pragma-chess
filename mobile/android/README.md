# Pragma Chess for Android

A small companion app: it keeps the databases of a paired Pragma Chess
computer on the phone, reads them offline, and is a place to enter games
(played or typed on the board) that go back into the computer's databases.
Without a computer it has one local database, "My Games" ("Le mie partite"),
and more can be created.

The protocol with the computer is [docs/phone-link.md](../../docs/tech/phone-link.md):
pairing by QR code (Options ▸ Connect Mobile App… on the computer), signaling
over public Nostr relays, files and games over a WebRTC data channel.

## Build

Needs JDK 17 and the Android SDK (compileSdk 36, minSdk 26); `local.properties`
points at the SDK (`sdk.dir=…`).

```bash
./gradlew assembleRelease     # app/build/outputs/apk/release/app-arm64-v8a-release.apk (and armeabi-v7a, universal)
./gradlew testDebugUnitTest   # JVM unit tests
adb install -r app/build/outputs/apk/release/app-arm64-v8a-release.apk
./gradlew assembleDebug       # debug build: much slower to draw, don't judge smoothness on it
```

The release build is minified (R8, resources shrunk), phones only (arm64-v8a
and armeabi-v7a, one APK each plus a universal one), and for now signed with
the debug key, so it updates an installed debug build. About 9 MB for arm64;
most of it is WebRTC (~5.4 MB) and secp256k1 (~1.3 MB).

## Engine

No engine is bundled. Like DroidFish and other chess apps, the app uses the
engines installed as separate apps through the **Open Exchange (OEX)**
protocol (`engine/OexEngines.kt`):

- it finds the activities for `intent.chess.provider.ENGINE` (declared in
  `<queries>` for Android 11+), reads each provider's `res/xml/enginelist.xml`
  and keeps the engines whose `target` matches `Build.SUPPORTED_ABIS`;
- it runs the binary straight from the provider's `nativeLibraryDir` (Android
  10+ does not let an app execute files it copied into its own storage), as
  a UCI process on 1–2 threads, so the board stays responsive;
- Settings lists the engines found and remembers the choice; with none, the
  analysis area offers "Install Stockfish" (Play Store,
  `com.stockfish141`, Stockfish 19 Chess Engine, free, OEX).

Engines are looked for again whenever the app comes back to the foreground.

## Architecture

Kotlin 2.2, Jetpack Compose with Material 3, AGP 8.13. Packages under
`app/src/main/java/org/pragmachess/mobile/`:

| Package | What |
|---------|------|
| `chess` | `Position` (legal moves, check/mate/stalemate, SAN, FEN; perft-tested), `GameLine` |
| `crypto` | secp256k1 keys and BIP-340 (secp256k1-kmp), `Nip44` v2 with a hand-written `ChaCha20` (official vectors), NIP-01 `NostrEvent` |
| `link` | `PairingLink`, `PhoneIdentity` (the phone's key), `Signaling` (relays over OkHttp WebSockets), `PeerLink` (WebRTC, stream-webrtc-android), `ComputerSync` (list, get and merge by lineage, put what the computer lacks, pull again; skips pairs unchanged since the last sync) |
| `data` | `PdbDatabase` (the desktop's `.pdb` schema, version 6, android.database.sqlite), `GameIdentity` (UUIDv5 game uids, lineages), `Reconciler` (merge by uid, newest wins), `Corpus` and `Library` (one flat folder of databases), `AppStore` (paired computers, origins, last reconciled hashes) |
| `engine` | `OexEngines` (engines installed as apps), `UciEngine` (a UCI process), `Analysis` (scores from White's point of view) |
| `ui` | `AppViewModel`, side menu (`PragmaApp`), `BoardScreen` with `Board`, `EvaluationBar`, `MoveList` (SkakNew figurines), games list, computers, settings, licenses |

All databases live in one folder of the app's private files,
`databases/local/`: the phone and every paired computer form one corpus
(docs/phone-link.md, "One corpus"), and the phone carries what it learnt from
one computer to the others. The per-computer folders of older versions are
moved in on startup.

## Not done yet

- The keys are in app-private storage, not in the Android Keystore.
- No PGN import/export on the phone, no variations or comments in the move list.
- A release signing key: the release APK is signed with the debug key.
- `SignalingRelayTest` needs public relays and runs only with `PRAGMA_RELAY_TEST=1`.
