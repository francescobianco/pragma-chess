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

## Explain

The bulb between the previous and next move explains the move on the board,
as Explain does on the desktop (AGENTS.md, "Explain"): arrows that justify the
evaluation, judged against the position before the move, the pieces that fall
ringed in red, and the sentence under the engine's line. Moving on turns it
off; the user asks again at the next move.

- `explain/` is a port of the desktop's pure logic — `MoveExplanation`
  (`explainPosition`, `classifyMove`), `AdvantageProbe`, `ExplanationAnalysis`
  and `ExplainSettings` — with the same thresholds, arrow kinds and colours
  (red only when material falls) and the desktop's texts and Italian
  translations (`ExplainText`, string resources `explain_*`). Tuning happens on
  the desktop (`docs/tech/explain-tuning.md`): bring its changes here, and the
  tests of `MoveExplanationTest` with them.
- `ExplanationSearch` runs the desktop's searches on an engine process of its
  own (depth 20 before and after the move, then the depth-2 line probe, one
  thread, hash cleared before each search), so the phone shows what
  `pragma-explain` shows. The live analysis is paused meanwhile and resumes
  after; a mate or a draw it finds deeper guides the explanation, as on the
  desktop. `ui/ExplainController` keeps finished analyses by move.
- The board's frame says where Explain is: it breathes towards blue while the
  engine searches and turns blue with the answer (`BoardBorder`).
- `ExplanationSearchTest` runs the whole search on a real engine only with
  `PRAGMA_EXPLAIN_ENGINE=/path/to/stockfish`.

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
| `explain` | Explain, ported from the desktop: `MoveExplanation`, `AdvantageProbe`, `ExplanationSearch`, `EngineEvaluation` |
| `ui` | `AppViewModel`, side menu (`PragmaApp`), `BoardScreen` with `Board`, `EvaluationBar`, `MoveList` (SkakNew figurines), games list, computers, settings, licenses |

A database deleted on the phone is remembered by lineage (`AppStore`,
table `deleted`): it is never downloaded again, and each computer that lists
it is told once with `deleted` (docs/phone-link.md), so that computer asks its
user whether to delete it on every synced device or keep it.

All databases live in one folder of the app's private files,
`databases/local/`: the phone and every paired computer form one corpus
(docs/phone-link.md, "One corpus"), and the phone carries what it learnt from
one computer to the others. The per-computer folders of older versions are
moved in on startup.

## Not done yet

- The keys are in app-private storage, not in the Android Keystore.
- Explain does not play a forced mate on the board yet (the desktop's red-framed sequence); the line is in the sentence.
- No PGN import/export on the phone, no variations or comments in the move list.
- A release signing key: the release APK is signed with the debug key.
- `SignalingRelayTest` needs public relays and runs only with `PRAGMA_RELAY_TEST=1`.
