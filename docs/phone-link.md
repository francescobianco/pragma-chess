# Phone Link

The Android app (`mobile/android/`) keeps a copy of the databases of a paired
computer and reads them offline. The computer and the phone find each other
through public **Nostr** relays and exchange the files over a **WebRTC data
channel**, which crosses NATs with STUN (hole punching) and works on the same
LAN, on two different networks, and anything in between that ICE can connect.

Nostr is only the rendezvous: it carries two small encrypted, signed messages
(the WebRTC offer and answer). No database byte ever goes through a relay.

```text
Phone ──┐  kind 25050 (ephemeral, NIP-44 encrypted, signed)
        ├── wss://relay.damus.io, wss://nos.lol, wss://relay.primal.net
PC ─────┘

Phone ── STUN ──> public address        PC ── STUN ──> public address
Phone <═══════ WebRTC data channel "pragma" (DTLS) ═══════> PC
```

## Keys and pairing

- The computer has a Nostr key pair (secp256k1, BIP-340), created on first
  use and kept per device in AppLocalData (`phone-link.json`), never in the
  synced Pragma folder. The phone has its own key pair in app storage.
- Options ▸ Connect Mobile App… on the computer opens a small dialog with a
  QR code to scan and the list of connected phones, each of which can be
  unpaired. There is no on/off switch: the computer listens on the relays
  while the dialog is open or at least one phone is paired. The QR code is a
  **pairing link**:

  ```text
  pragma-chess://pair?k=<computer pubkey, 64 hex>&s=<pairing secret, 64 hex>
      &n=<computer name, URL-encoded>&r=<relay URL>&r=<relay URL>…
  ```

  The pairing secret is 32 random bytes made when the dialog shows the code,
  valid while the dialog is open and for one phone only.
- The phone scans the code (or the link is pasted) and stores the computer:
  pubkey, name, relays. Its first offer carries the secret; the computer
  checks it, remembers the phone's pubkey and name in its paired devices, and
  from then on accepts offers signed by that key alone. Removing a phone in
  the dialog revokes it, and with no phone left the computer stops listening.
- Everything the computer accepts is signed by a paired key, encrypted to the
  computer's key, and the SDP inside carries the DTLS fingerprints: the data
  channel is end-to-end authenticated, relays can neither read nor forge it.

## Signaling events

A Nostr event (NIP-01) of **kind 25050**: ephemeral (20000–29999), relays
forward it to subscribers and do not store it.

```json
{"kind": 25050, "tags": [["p", "<recipient pubkey hex>"]],
 "content": "<NIP-44 v2 payload>", "created_at": <now>, "pubkey": …, "id": …, "sig": …}
```

- `content` is NIP-44 **version 2** (secp256k1 ECDH → HKDF-SHA256 with salt
  `nip44-v2` → ChaCha20 + HMAC-SHA256, padded, base64) of a JSON message.
  Both implementations are tested against the official NIP-44 test vectors.
- Receivers verify the event id and the signature, drop events whose
  `created_at` is more than 120 s away from their clock, and ignore sessions
  they already answered.
- The computer, while it listens, subscribes on every relay:
  `["REQ","pragma-link",{"kinds":[25050],"#p":["<computer pubkey>"],"since":<now-60>}]`.
  The phone subscribes to its own pubkey the same way before publishing.
- Events are published to every relay of the pairing link (duplicates are
  expected and ignored by session).

Messages (JSON inside the encrypted content):

| From → to | Message |
|-----------|---------|
| phone → computer | `{"t":"offer","session":"<32 hex>","sdp":"<SDP>","name":"<phone name>","pair":"<secret hex, first time only>"}` |
| computer → phone | `{"t":"answer","session":"…","sdp":"<SDP>","name":"<computer name>"}` |
| computer → phone | `{"t":"refused","session":"…","reason":"not-paired"}` |

SDP is sent whole, after ICE gathering has finished (no trickle ICE): one
message each way. The phone offers; the computer answers.

## WebRTC

- ICE servers: `stun:stun.l.google.com:19302`, `stun:stun.cloudflare.com:3478`.
  TURN is not configured yet: a pair of symmetric NATs (some mobile carriers,
  CGNAT) will not connect, and the app says so.
- One data channel, label `pragma`, reliable and ordered, created by the phone.

## File protocol (on the data channel)

Text frames are JSON; binary frames are file data.

| From | Message |
|------|---------|
| phone | `{"op":"list"}` |
| computer | `{"op":"list","name":"<computer name>","files":[{"name":"Classic Games.pdb","size":123,"sha256":"<hex>","modified":"<ISO 8601 UTC>"}]}` |
| phone | `{"op":"get","name":"Classic Games.pdb"}` |
| computer | `{"op":"file","name":"…","size":123,"sha256":"<hex>"}`, then binary frames of at most 16 KiB, then `{"op":"done","name":"…"}` |
| computer | `{"op":"error","message":"…"}` (for a `get` it cannot serve) |

- `files` are the `.pdb` databases under the computer's Databases folder,
  `name` relative to it with `/` separators. A `get` for anything not in the
  list is refused.
- The computer sends a snapshot copy (the database may be open and written),
  hashes what it sends and respects the channel's buffered amount.
- The phone asks for the files whose `sha256` differs from its copy, writes
  to a temporary file, checks size and hash and then replaces its copy. A
  database gone from the computer stays on the phone until the user removes it.

### Games from the phone (the other direction)

The sync is two-way, but by **games**, not by files: the phone never
overwrites a computer database. Games played or entered on the phone are
pushed first, then the phone pulls the files that changed (which now contain
those games).

| From | Message |
|------|---------|
| phone | `{"op":"put","name":"Le mie partite.pdb","games":[<game>…]}` |
| computer | `{"op":"put","name":"…","stored":<n>,"known":<n>}` or `{"op":"error","message":"…"}` |

```json
{"id": "<uuid v4, made on the phone>", "white": "…", "black": "…",
 "event": "…", "site": "…", "date": "2026.09.29", "round": "…",
 "result": "1-0|0-1|1/2-1/2|*", "white_elo": 0, "black_elo": 0, "eco": "",
 "start_fen": "" , "moves_san": "e4 e5 Nf3", "moves_uci": "e2e4 e7e5 g1f3"}
```

- The computer creates the database if it does not exist (a phone database
  is new to the computer until its first sync), and records each game in
  `sources` / `game_sources`: one source of kind `phone` per paired phone
  (uuid = the phone's pubkey), `external_id` = the game's `id`. A game it
  already has is counted in `known` and not stored again, so a push that is
  repeated after a lost answer never duplicates games.
- The computer trusts `moves_uci` only: it replays the moves from
  `start_fen` (or the standard position), refuses the whole put with an
  `error` if one is illegal, and writes the SAN again from them, so
  `moves_san` may be empty. `id` is required (at most 64 characters);
  `result` other than the four values becomes `*`; text fields are trimmed.
- `name` follows the same rules as for `get` (relative, `.pdb`, no `..` or
  hidden parts). While the computer is running its folder sync a put is
  answered with `{"op":"error","message":"busy, try again later"}`: the
  outbox keeps the games for the next time.
- A put for the database open in the desktop window goes through that open
  instance (same thread, so its game list refreshes); any other database is
  opened on its own for the put and closed again.
- The phone keeps its games in an outbox until the computer's `put` answer
  counts them (`stored + known`), and applies the outbox again on top of any
  file it pulls, so nothing typed on the phone is lost by a sync.

## The phone on its own

Without any computer the app has one local database, **Le mie partite**
(localized, "My Games" in English), and the user can create more. Local
databases are ordinary `.pdb` files (same schema as the desktop, version 4).
Once a computer is paired, they are pushed to it like any other database.

The phone reads files of schema version 1 to 4 (a newer one is refused) and
upgrades the ones it writes to by adding the missing tables. A database whose
`properties` row `type` is `opening-book` (the Opening Names database) is
reference data: the side menu lists it under a small "Opening books" title
after the game collections, and games cannot be saved into it.

## Code

- Computer: `gui/qt/src/app/phone/`, the static library `pragma-phone-link`:
  `NostrKey` (secp256k1), `Nip44`, `NostrEvent`, `NostrRelayPool`
  (libdatachannel WebSockets), `WebRtcPeer` (one peer connection and its
  channel), `PairingLink`, `PhoneFiles` (listing, name checks), `PhoneGames`
  (parsing and storing puts), `PhoneLink` and `PhoneLinkSession` (the
  computer), `PhoneLinkClient` (a phone, for the tool and the tests).
  `DatabaseFolderStore` and `dialogs/ConnectMobileDialog` are in the app.
  libdatachannel (WebRTC and the WebSocket client for relays) and
  libsecp256k1 come from the system or are fetched by CMake (libdatachannel
  needs OpenSSL); `PRAGMA_PHONE_LINK=OFF` or a missing dependency builds the
  client without it (`PRAGMA_HAS_PHONE_LINK`).
- `pragma-phone` plays either side on the command line, e.g.
  `pragma-phone --serve --databases DIR --state FILE` prints a pairing link,
  and `pragma-phone --pair LINK --list | --get NAME -o DIR | --put NAME games.json`
  acts as a phone. `tst_phonelink` runs the NIP-44 vectors and a whole
  pairing, list, get and put through a relay on localhost.
- Phone: `mobile/android/`, Kotlin + Jetpack Compose (see its README).
