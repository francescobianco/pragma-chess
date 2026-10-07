#pragma once

#include "app/phone/NostrKey.h"

#include <QHash>
#include <QJsonObject>
#include <QObject>
#include <QSet>
#include <QStringList>

class LobbyNode;
class NostrRelayPool;
struct NostrEvent;
class QTimer;
class WebRtcPeer;

/// The lobby's ledger on the network, replicated the way eMule shares a
/// file (docs/tech/lobby-network.md): every node keeps the whole ledger of
/// the live games and gives other nodes the events they lack.
///
/// - The relays (Nostr, NIP-01) are a source that is always there: every
///   event of the ledger is published to them and read back from them, so a
///   player who was away catches up even when nobody else is on.
/// - Nodes find each other on the relays — each announces itself every
///   minute with a key of its own for the session, not the user's — and
///   connect directly, over WebRTC (the same as Phone Link, NAT crossed
///   with STUN). Two connected nodes compare the ids of what they have and
///   send each other only what the other lacks; after that every new event
///   goes to every connected node, which hands it on (gossip).
///
/// Every event is checked by LobbyNode before it counts: a node cannot
/// change what another signed, only hand it on.
class LobbyNetwork : public QObject {
    Q_OBJECT

public:
    LobbyNetwork(LobbyNode *node, QObject *parent = nullptr);
    ~LobbyNetwork() override;

    static QStringList defaultRelays();
    static QStringList defaultIceServers();
    /// The relays to use; a network started switches to them at once.
    void setRelays(const QStringList &relays);
    void setIceServers(const QStringList &servers) { m_iceServers = servers; }
    /// How often the node says it is there; shorter in tests.
    void setAnnounceInterval(int ms) { m_announceMs = ms; }

    void start();
    void stop();
    bool isStarted() const { return m_pool != nullptr; }
    /// Relays connected, and nodes connected directly.
    int relayCount() const;
    int peerCount() const;

    /// The kinds of the network's own messages: a node saying it is there,
    /// and the WebRTC offer and answer between two nodes (ephemeral: relays
    /// hand them on and do not keep them).
    static constexpr int kAnnounceKind = 25051;
    static constexpr int kSignalKind = 25052;
    /// Direct connections a node keeps at most.
    static constexpr int kMaxPeers = 8;

Q_SIGNALS:
    void stateChanged();

private:
    struct Peer;

    void onRelayEvent(const QString &subscription, const NostrEvent &event);
    void onAnnounce(const NostrEvent &event);
    void onSignal(const NostrEvent &event);
    void announce();
    /// A new event of the ledger, here or from the network: to the relays and the peers.
    void spread(const QJsonObject &event);
    /// Every event of the ledger again, to the relays (they may have dropped some).
    void republish();
    Peer *addPeer(const QString &session);
    void removePeer(const QString &session);
    void sendSignal(const QString &session, const QJsonObject &message);
    void onPeerText(const QString &session, const QByteArray &text);
    void sendEvents(Peer &peer, const QList<QJsonObject> &events);
    void updateState();

    LobbyNode *m_node;
    /// This run's key: says who connects, not who plays.
    NostrKey m_session;
    QStringList m_relays;
    QStringList m_iceServers;
    int m_announceMs = 60000;
    NostrRelayPool *m_pool = nullptr;
    QTimer *m_announceTimer = nullptr;
    QHash<QString, Peer *> m_peers;
    /// Events received from the relays, so they are not published back.
    QSet<QString> m_fromRelays;
};
