#pragma once

#include "Lobby.h"

#include <QObject>

/// The lobby as the window sees it: the rooms, who the user is in it, and
/// what they can do there. LobbyNode is the real one, on the ledger of the
/// network; the window knows only this.
class LobbyService : public QObject {
    Q_OBJECT

public:
    using QObject::QObject;

    /// The lobby as the ledger makes it now.
    virtual const Lobby &lobby() const = 0;
    /// The user in the lobby: their public key.
    virtual QString me() const = 0;
    /// Opens a new room named by `seed`, the user in its first seat; its id.
    virtual QString openRoom(quint32 seed) = 0;
    /// Sits the user in room `roomId`.
    virtual bool joinRoom(const QString &roomId) = 0;
    /// The user's move in a game of theirs, when it is their turn.
    virtual bool sendMove(const QString &roomId, const QString &white, const QString &black, const QString &uci) = 0;
    /// The user's plan for a game, kept on this computer and played by it
    /// whenever the game reaches a position it answers (IDEA.md: plans stay
    /// private, the user's own node plays them).
    virtual void setPlan(const QString &roomId, const QString &white, const QString &black, const LobbyPlan &plan) = 0;
    virtual LobbyPlan plan(const QString &roomId, const QString &white, const QString &black) const = 0;
    /// The user resigns a game of theirs.
    virtual bool resign(const QString &roomId, const QString &white, const QString &black) = 0;

    /// The network as it is (IDEA.md §38: it does not pretend): the relays
    /// and the peers this node is connected to, and whether it is on at all.
    virtual int relayCount() const { return 0; }
    virtual int peerCount() const { return 0; }
    virtual bool isOnline() const { return false; }

Q_SIGNALS:
    /// The lobby changed: an event came in or the user did something.
    void changed();
    /// The user's plan played a move by itself.
    void planPlayed(const QString &roomId, const QString &white, const QString &black, const QString &uci);
    /// The connections changed.
    void networkChanged();
};
