#pragma once

#include "Lobby.h"

#include <QHash>
#include <QJsonObject>
#include <QList>
#include <QString>

/// One event of the lobby's ledger: something a player did — opened a room,
/// sat in one, played a move, resigned —, signed by them. On the wire it is
/// a Nostr event (NIP-01) of kind LobbyLedger::kKind whose content is the
/// JSON of `content`; its id is the hash of what it says, so any peer can
/// hand it on and every peer can check it (docs/tech/lobby-network.md).
struct LedgerEvent {
    QString id;
    /// The author's public key (hex): who did it.
    QString author;
    qint64 createdAt = 0;
    /// What they did: {"t": "open" | "join" | "move" | "resign", …}.
    QJsonObject content;
    /// The signed event as it travels, to hand on unchanged.
    QJsonObject signedEvent;

    /// The fields of a signed event, checked for shape only: the signature
    /// is checked by whoever has the keys (LobbyNode). Nothing for an event
    /// that is not of the ledger.
    static std::optional<LedgerEvent> fromSigned(const QJsonObject &event);
};

/// The ledger of the lobby: every event a peer knows of, and the lobby they
/// make. The lobby is a function of the set of events alone — not of the
/// order they arrived in —, so two peers with the same events see the same
/// rooms, seats and moves; there is no referee, every peer checks every
/// event against the rules (IDEA.md, "Validazione deterministica").
///
/// The rules, applied to the events in the order of (created_at, id):
/// - `open` {seed, name}: a room, named by the seed, with its author in the
///   first seat; its id is the event's id.
/// - `join` {room, name}: the author takes the next free seat, if they do
///   not sit there already; later joins of a full room are left out. Joins
///   are read after every opening, whatever their times.
/// - `move` {room, white, black, ply, uci}: the move of that ply in the game
///   white plays against black there, when it is its author's turn and the
///   move is legal; of two such moves for one ply (two devices of the same
///   player) the first counts.
/// - `resign` {room, white, black}: its author, one of the two, loses the
///   game, unless it ended before.
/// - `name` in any event: the name its author goes by, the latest one.
class LobbyLedger {
public:
    /// The Nostr kind of the ledger's events: regular, so relays keep them.
    static constexpr int kKind = 7457;
    /// The tag every event of the ledger carries, to find them: ["t", kTopic].
    static constexpr const char *kTopic = "pragma-lobby";

    /// Adds an event; false when it is known already or not of the ledger.
    bool add(const LedgerEvent &event);
    bool contains(const QString &id) const { return m_events.contains(id); }
    int size() const { return int(m_events.size()); }
    /// The events, in the order the rules read them.
    QList<LedgerEvent> events() const;
    /// The ids of the events, sorted: what a peer has, to compare with another.
    QStringList ids() const;

    /// The rooms the events make.
    QList<LobbyRoom> rooms() const;

    /// Content of the events a player makes.
    static QJsonObject openContent(quint32 seed, const QString &name);
    static QJsonObject joinContent(const QString &room, const QString &name);
    static QJsonObject moveContent(const QString &room, const QString &white, const QString &black, int ply,
                                   const QString &uci);
    static QJsonObject resignContent(const QString &room, const QString &white, const QString &black);

private:
    QHash<QString, LedgerEvent> m_events;
};
