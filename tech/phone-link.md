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
| phone | `{"op":"deleted","db":"<lineage>","name":"<file on the phone>","when":"<ISO 8601 UTC>"}` (see "A database deleted on the phone") |
| computer | `{"op":"deleted","db":"<lineage>"}` once recorded, or `{"op":"error",…}` |

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

## One corpus: reconciliation (schema version 5)

> Draft, to be refined. It replaces "the phone keeps a copy of the computer's
> databases" and "games from the phone" as the model; the ops above stay and
> are extended.

There are no sources and no sections: the databases of the phone and of every
computer paired with it are **one corpus** that is merged and reconciled.
A sync never deletes anything that has not been merged: a database that
exists only on one side is kept and copied to the other, in both directions,
and the only files that ever go are a duplicate whose games were merged into
the database it duplicates (see "Duplicates") and a database the user
deleted knowingly (see "A database deleted on the phone"). With two computers paired to
the same phone, the phone is the **bridge**: what it learnt from one computer
it brings to the other.

### Identity

- **Database lineage.** Every database has a universal id, `properties` row
  `id` (UUID, lowercase, with dashes), made when the database is created and
  kept inside the file: it survives renames, copies and syncs. Databases with
  the same id are the same database, whatever their file name; different ids
  are different databases even with the same name. A database without an id
  (older files) gets a random one the first time the computer lists it to a
  phone (upgrading a file to version 5 does not make one, so the seeds below
  can still take theirs). **Save Database As** makes a new database: the copy
  gets a new id, since two files of one lineage on one computer would be one
  database to a sync.
- Databases **we ship** have fixed ids, so every install and every update of
  them is the same lineage and reconciles. The desktop sets them when it seeds
  the file, and at startup on a seeded copy (by its seed file name) that has
  no id yet (`SqliteGameDatabase::adoptLineage`); the opening names `.pdb`
  files in the resources are version 5 and carry their id and type:
  - English opening names (`English.pdb`): `7b0c5a1e-3f0d-4a55-9e3c-6f1d0a9b4c01`
  - Classic Games: `7b0c5a1e-3f0d-4a55-9e3c-6f1d0a9b4c02`
  - Italian opening names (`Italian.pdb`): `7b0c5a1e-3f0d-4a55-9e3c-6f1d0a9b4c03`
- `properties` may also hold a display name, `name`, and its translations,
  `name.<language code>` (e.g. `name.it`); a phone that does not show them
  ignores them like any unknown key.
- The opening names we ship live in the computer's `Books/Opening Names`
  folder, not among the databases it lists to the phone. A put addressed to
  one of their lineages is answered with every game `known` and stores
  nothing: they are reference data updated by releases, and must not come
  back among the databases of games.
- **Game identity.** Every game has `games.uid` (TEXT, unique): a UUID
  version 5 in the namespace `7b0c5a1e-3f0d-4a55-9e3c-6f1d0a9b4c00` of the
  game's content when it is created — `white|black|event|site|date|round|result|start_fen|moves_uci`,
  each trimmed, joined by `\n` — so the same game created independently
  (the seed on two installs, the same lichess game imported twice) is one
  game. It never changes afterwards, even when the game is edited. Upgrading
  a file to version 5 fills `uid` of existing games the same way, in row
  order. Identical games in one database are told apart by appending
  `\n#2`, `\n#3`, … to the content (the next free one), both when upgrading
  and when such a game is added. Content-derived uids are lowercase.
- **Game revision.** `games.modified` (TEXT, ISO 8601 UTC, empty = never
  edited) is set when a game is created or changed.

### Merging two copies of a database

Pure and unit-tested on both sides (`Reconcile` / `Reconciler`): given the
local games and the incoming ones, by `uid`:

| Local | Incoming | Result |
|-------|----------|--------|
| missing | present | insert |
| present | missing | keep (the other side will receive it) |
| same content | same content | nothing |
| different content | different content | **conflict**: the newer `modified` is kept for now (a tie or an empty incoming `modified` keeps the local one), and the pair is recorded to be shown to the user |

"Content" compared here is the uid content plus ECO and both ratings;
timestamps compare as instants, whatever their offset. A uid sent twice in
one put counts once.

Deleting follows its own, smaller reconciliation (schema version 6): see
"The trash" below. Conflicts are listed after the sync ("3 games
differed on the two devices; the most recent version was kept") with the
option, later, to choose per game. A later scan may renumber local ids and
sort games (e.g. by date) so both copies look alike; `id` is local, `uid` is
what matters.

### The trash (schema version 6)

A game is never removed by a merge of games; where it *is* travels apart, in
`game_states` (`uid`, `state`, `modified`):

| State | Meaning |
|-------|---------|
| no row, or `live` | in the lists (`live` is what restoring from the trash writes) |
| `trashed` | in the Trash only |
| `deleted` | deleted from the Trash: in no list, still in the file |
| `purged` | removed from the file by Optimize Database; only this row is left |

Merging two copies merges the states first, by uid: the newer `modified`
wins, a tie keeps the local one (desktop `GameStates::incomingChanges`, phone
`PdbDatabase.mergeStates`). Then a copy drops the rows of the games that are
`purged`, and the merge of games above leaves those out of its inserts. So a
game trashed, restored or deleted on one device is so on the others, and a
purged game does not come back from a copy that still has it: that copy lets
it go too. A purged game stored again by hand becomes `live`, with a newer
`modified`. The state has its own revision: trashing does not touch
`games.modified`, and editing a game does not take it out of the trash.

The phone has no trash of its own yet: it hides what a computer trashed or
deleted, drops what it purged, and never sends such a game back as new (the
computer leaves out of a `put` the games it purged).

### Duplicates

One database can end up in two files: two lineages for the same database
(files from before lineages existed), a copy kept aside, a conflict copy.
Both sides apply the same rule (desktop `DatabaseDedupe`, phone `Dedupe`),
so they keep the same lineage:

- Two files are one database when they have the same lineage, or the same
  name as shown — without the parenthesised suffix a sync, a conflict or a
  copy kept aside adds ("chess-com (Samsung SM-N960F)", "Opening Names
  (old 2)") — and exactly the same games (two empty ones count). The phone
  also groups two files of one name that came from the same device.
- The database keeps the **smallest lineage** of the group. On the desktop
  the file kept is the canonical one of a database we ship, else the one
  with the plain name, outside an `Old` folder, then the shortest path.
- The other files are merged into it by uid (the table above: nothing is
  lost, a conflict keeps the newer version) and removed.

On the desktop, a merge reaches the other computers through the folder sync:
the manifest's `merged` entries (`path`, its `lineage`, the `into` lineage and
`intoPath`) tell every device that still has that file with that lineage to
merge it into its own copy of `into` and delete it, and the file is removed
from the remote folder (a `git rm` for a Git repository). A file at a merged
path with another lineage is a new database and syncs normally. The phone
remembers the lineages it merged away as aliases, so a computer still
listing one is merged, not downloaded as new.

### A database deleted on the phone

Deleting a database on the phone is a decision the computers are asked
about, not a file that silently goes:

- The phone remembers the lineage it deleted, and from then on never `get`s
  it (whatever computer lists it) and never `put`s it.
- At each sync, after `list`, for every deleted lineage that computer lists
  and has not acknowledged yet, the phone sends
  `{"op":"deleted","db":"<lineage>","name":"<file name on the phone>","when":"<ISO 8601 UTC>"}`.
  The computer answers `{"op":"deleted","db":"<lineage>"}` once it has
  recorded the request, and the phone stops sending it to that computer.
  An error (an older computer answers "unknown op"; "busy, try again later"
  while a folder sync replaces files) leaves the notice for the next sync.
- The computer keeps the request in its `phone-link.json`
  (`deletionRequests`: lineage, its file, the phone and when) and asks its
  user, outside the phone's session, at once and at every start while it is
  unanswered: **Keep It** drops the request (the database stays on the
  computer; the phone still does not receive it), **Ask Me Later** asks
  again at the next start, **Delete Everywhere…** warns that the database
  goes from every synced device and, confirmed, moves it to the trash and
  records the deletion for the folder sync (AGENTS.md, "Folder sync"; the
  manifest's `deleted`), so the server and the other computers drop it too.
- A lineage deleted on a computer is remembered there (`deletedDatabases`):
  it is not listed to phones even if a copy comes back, and a `put` for it
  is answered with every game `known`, storing nothing, so another phone
  cannot bring it back.
- A lineage the computer does not have is acknowledged and nothing is asked.

### The sync, from the phone

1. `list`: each file also carries `"id":"<lineage>"` and `"games":<count>`.
   Then the `deleted` notices this computer has not acknowledged (see "A
   database deleted on the phone"); a deleted lineage is skipped below.
2. For every database of the computer: if the phone has the same lineage
   (whatever the name) and the same `sha256`, skip it. Otherwise `get` it to a
   temporary file and merge it into the phone's copy (or keep it as the
   phone's copy if the phone has no such lineage).
3. For every phone database (after step 2) the computer lacks games of, or
   does not have at all: `put` the games it lacks (or all of them), addressed
   by lineage:

   ```json
   {"op":"put","db":"<lineage id>","name":"Le mie partite.pdb",
    "properties":{"type":"games","description":""},"games":[<game>…]}
   ```

   Each game carries `"uid"` and `"modified"` besides the fields above; `id`
   (the old outbox id) is still accepted and kept as the `external_id` (a
   game with only an `id` gets the uid its content gives; with only a `uid`,
   the uid is the `external_id`). A put without `db` (an older phone) goes to
   `name` as before, merged the same way.
   The computer finds the database by `db` first, then by `name` only if that
   file has no id yet; it creates it with that id (and properties) when it
   has none, naming it `name`, or `name (phone name)` if a different lineage
   already uses the name (`name (phone name 2)`… if that is taken too; `/`,
   `\` and `:` in the phone name become `-`). It merges with the table above
   and answers
   `{"op":"put","db":"…","name":"<the file it went to>","stored":n,"updated":n,"known":n,"conflicts":[{"uid":"…"}]}`.
   The same game from two phones is one game: `known`, not stored twice.
4. Pull again the files the put changed, so both sides end with the same
   games (the file hashes may still differ: the order of rows is local).

Files the phone had before version 5 (its own and the copies it kept per
computer, now moved into its one folder) get a **provisional** id when the
phone upgrades them; at the next sync, a phone database with a provisional id
takes the id of the computer's database of the same file name, since before
ids a name was all that tied them. A file with a provisional id that no
computer has keeps it, and it becomes final after the first sync.

The phone's list of databases is flat: one list, sorted by name, whoever made
each database; two databases with the same name and different lineage show
where each came from under the name.

## The phone on its own

Without any computer the app has one local database, **Le mie partite**
(localized, "My Games" in English), and the user can create more. Local
databases are ordinary `.pdb` files (same schema as the desktop, version 5).
Once a computer is paired, they are pushed to it like any other database.

The phone reads files of schema version 1 to 5 (a newer one is refused) and
upgrades the ones it writes to (missing tables, game uids, an id). A database whose
`properties` row `type` is `opening-book` (an opening names database) is
reference data: it is listed with the others, with the same database icon, and games
cannot be saved into it.

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
