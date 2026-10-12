# The lobby's network

The lobby (Game ▸ Enter the Lobby…) holds IDEA.md's asynchronous tournaments
of four players. There is no server of ours: the lobby is a **ledger** — the
set of every signed event players made — that every node keeps a copy of
and hands on to the others, the way eMule shares a file. Each node makes the
lobby out of its copy by itself, with the same rules, so two nodes with the
same events see the same rooms, seats and moves.

```text
             relays (Nostr, NIP-01): keep the ledger's events
            ╱        │         ╲          for whoever is away
      node A ═══ WebRTC ═══ node B        nodes connected directly:
            ╲                ╱           compare ids, send what the
             ═══ node C ════             other lacks, then gossip
```

## Identity

- A player is a **secp256k1 key** (BIP-340), the same kind Phone Link uses.
  It signs every event they make; the public key (64 hex digits) is who
  they are in the lobby, and the name they give travels in their events.
- The key is kept per computer (QSettings `lobby/key`) and **never synced**:
  a secret key in the synced Pragma folder would go wherever the folder
  goes. Options ▸ Personal Settings… shows it (hidden until asked) with
  Copy, and takes one pasted from another computer: carried by hand, the
  same player plays from both. Two devices of one player may send a move
  for the same ply; the rules keep the first (below).

## Events

A ledger event is a Nostr event of **kind 7457** (regular: relays keep it),
tagged `["t","pragma-lobby"]` and, about a room, `["r","<room id>"]`. Its
content is JSON with a version and a type:

| Type | Content | Meaning |
|------|---------|---------|
| `open` | `{"v":1,"t":"open","seed":N,"name":"…"}` | A room, named by the seed (`RoomName`), its author in the first seat. Its id is the event's id. |
| `join` | `{"v":1,"t":"join","room":"<id>","name":"…"}` | The author takes the next free seat. |
| `move` | `{"v":1,"t":"move","room":"<id>","white":"<key>","black":"<key>","ply":n,"uci":"e2e4"}` | The move of ply n in the game white plays against black. |
| `resign` | `{"v":1,"t":"resign","room":"<id>","white":"<key>","black":"<key>"}` | The author, one of the two, loses. |

Moves are UCI, never SAN (IDEA.md §17). The id is the hash of the
serialized event (NIP-01): an event cannot be changed without changing its
id, and its signature says who made it. Any node can hand on any event;
none can forge one.

## The rules (the fold)

`LobbyLedger::rooms()` reads the events in the order of (created_at, id):

1. Every `open` makes a room, its author seated (unless rule 3 refuses it).
2. The `open`s and `join`s take seats in order: the author takes the next
   free seat unless they sit there already or the room is full. (A join is
   read right after its room's `open` when it carries an earlier time,
   clocks being apart.) When a player sits, the room gets their two games
   with each player already there — the newcomer's White game first.
3. **A player plays in at most two tournaments at once**
   (`Lobby::kMaxRoomsInPlay`): an `open` or `join` is left out when its
   author already sits in two rooms not finished at its place in the order.
   A room is finished when it is full and every game ended by events
   before that place (the latest move or resignation that ended each one).
   A refused `open` makes no room, and joins to it are left out. Clients
   refuse such a seat before signing it (`LobbyNode`), and the window says
   why (`Lobby::mayJoin`).
4. The `move`s of each game, by ply: a move counts when it is for the next
   ply, its author is the player to move, and it is legal. Of two moves for
   one ply, the first in the order counts; the others are left out.
5. Checkmate and stalemate end a game; otherwise a `resign` by one of its
   players does.
6. The name a player goes by is the latest they gave.
7. The winners of a finished room (`LobbyRoom::winners`: first in the
   standings, all of them when level) keep a medal, a yellow dot after
   their name in the lobby (`Lobby::medals`). It is read off the rooms, so
   no event carries it and every peer sees the same medals for good.

The result depends on the set of events only, never on the order they
arrived in; `tst_chessrules::foldsTheLobbyLedger` checks it in three orders.
**Changing a rule changes what every client sees**: a change must be
backward compatible (a new type, a new field) or versioned (`v`).

## Plans stay private

A plan (Send Plan) is the user's answers to the opponent's possible replies:
in the position a line of moves leads to, the move to play. It is **never
sent**. The user's node keeps it (`AppLocalData/lobby/plans.json`) and,
whenever an event brings a game of theirs to a position the plan answers,
signs and sends that move at once (`LobbyNode::runPlans`). The opponent sees
moves arrive, never the plan (IDEA.md §9). So a plan answers only while one
of the player's nodes is on (IDEA.md §11, §39); commitments that prove a
plan was made earlier (IDEA.md §10) can come later as a new event type.

## The network

`LobbyNetwork` keeps a node on the network as long as the client runs (it
joins at startup when the computer has a ledger).

- **Relays**: `wss://relay.damus.io`, `wss://nos.lol`, `wss://relay.primal.net`
  (Phone Link's). Every new event — made here or received — is published to
  them; the node subscribes to `{"kinds":[7457],"#t":["pragma-lobby"]}` and
  takes what they keep, so a player who was away catches up even when no
  other node is on. Fifteen seconds after starting, the node publishes again the
  events of its ledger the relays did not send it (they may have dropped
  them).
- **Peers**: each node has a **session key** for the run (not the player's:
  two devices of one player are two nodes, and the network sees no
  identity in the announcements). Every minute it publishes an announcement
  (kind 25051, ephemeral, tagged `t=pragma-lobby`). On an announcement from
  another session, the node whose session key sorts first sends a WebRTC
  offer, the other answers; offer and answer are NIP-44 encrypted to the
  other's session key (kind 25052, tagged `p`), as Phone Link's signaling.
  At most 8 peers; a connection not open after 45 s is given up and tried
  again at the next announcement. ICE: STUN only (no TURN yet: two
  symmetric NATs do not connect, and the relays still carry the events).
- **On a peer's data channel** (JSON text): on opening each side sends
  `{"t":"have","ids":[…]}`, the ids of its ledger; the other answers with
  `{"t":"events","events":[…]}`, the signed events it lacks, in batches of
  100. After that every new event goes to every peer, which checks it, keeps
  it if new and hands it on (gossip; a known event stops there).
- Every event is checked on arrival (id, signature, kind) before it counts;
  the rules then decide whether it does anything.
- The lobby says how the node is connected — relays, peers — or that it is
  not (IDEA.md §38). What the user does while offline is kept and leaves
  when a relay or a peer is reached (the republication above).

## Trying it

- `tst_phonelink::playsTheLobbyBetweenTwoNodes`: two nodes exchanging events
  by hand — seats, a move, a plan answering, a forged event refused, a late
  node getting the whole ledger, files read again, a resignation.
- `tst_phonelink::replicatesTheLobbyLedgerOverTheNetwork`: two networks on a
  relay of the test's own — a room found on the relay, the two nodes
  connected over WebRTC, a plan answering across the network, an event going
  peer to peer only, a third node later getting the ledger from the relay.
- Two desktop clients: a local relay (any NIP-01 relay on
  `ws://127.0.0.1:…`) and `PRAGMA_LOBBY_RELAYS=ws://127.0.0.1:…` with a
  scratch `HOME`, `XDG_CONFIG_HOME`, `XDG_DATA_HOME` and `PRAGMA_CHESS_DIR`
  each. **Never test on the public relays**: the events stay there, and
  every user's lobby would show the test rooms.

## Not yet

- Draw offers, aborting, a game abandoned for good (IDEA.md §53: dormant
  games), pruning finished games from the live ledger.
- Commitments of plans (IDEA.md §10).
- TURN for symmetric NATs; peers keeping more than their own lobby (a home
  node that never sleeps, IDEA.md §39, is just a client left open).
- The Android app does not take part in the lobby yet.
