#include "LobbyNetwork.h"

#include "LobbyNode.h"
#include "app/lobby/LobbyLedger.h"
#include "app/phone/Nip44.h"
#include "app/phone/NostrEvent.h"
#include "app/phone/NostrRelayPool.h"
#include "app/phone/WebRtcPeer.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTimer>

namespace {

constexpr char kLedgerSubscription[] = "pragma-lobby-ledger";
constexpr char kAnnounceSubscription[] = "pragma-lobby-peers";
constexpr char kSignalSubscription[] = "pragma-lobby-signal";
/// Events sent to a peer in one message.
constexpr int kBatch = 100;
/// Offers and announcements older than this are stale.
constexpr qint64 kMaxAgeSecs = 180;

} // namespace

struct LobbyNetwork::Peer {
    WebRtcPeer *rtc = nullptr;
    bool open = false;
    /// We made the offer (the node whose session key sorts first does).
    bool offering = false;
    qint64 started = 0;
};

LobbyNetwork::LobbyNetwork(LobbyNode *node, QObject *parent)
    : QObject(parent)
    , m_node(node)
    , m_session(NostrKey::generate())
    , m_relays(defaultRelays())
    , m_iceServers(defaultIceServers())
{
    connect(m_node, &LobbyNode::eventAdded, this, &LobbyNetwork::spread);
}

LobbyNetwork::~LobbyNetwork()
{
    stop();
}

QStringList LobbyNetwork::defaultRelays()
{
    return {QStringLiteral("wss://relay.damus.io"), QStringLiteral("wss://nos.lol"),
            QStringLiteral("wss://relay.primal.net")};
}

QStringList LobbyNetwork::defaultIceServers()
{
    return {QStringLiteral("stun:stun.l.google.com:19302"), QStringLiteral("stun:stun.cloudflare.com:3478")};
}

void LobbyNetwork::setRelays(const QStringList &relays)
{
    m_relays = relays;
    if (m_pool)
        m_pool->setRelays(m_relays);
}

void LobbyNetwork::start()
{
    if (m_pool)
        return;
    m_pool = new NostrRelayPool(this);
    connect(m_pool, &NostrRelayPool::eventReceived, this, &LobbyNetwork::onRelayEvent);
    connect(m_pool, &NostrRelayPool::connectedCountChanged, this, [this] {
        updateState();
        if (m_pool->connectedCount() > 0)
            announce(); // As soon as one relay hears it.
    });
    const QString topic = QString::fromLatin1(LobbyLedger::kTopic);
    // The whole ledger the relays keep, then what comes.
    m_pool->subscribe(QString::fromLatin1(kLedgerSubscription),
                      {{QStringLiteral("kinds"), QJsonArray{LobbyLedger::kKind}},
                       {QStringLiteral("#t"), QJsonArray{topic}},
                       {QStringLiteral("limit"), 5000}});
    m_pool->subscribe(QString::fromLatin1(kAnnounceSubscription),
                      {{QStringLiteral("kinds"), QJsonArray{kAnnounceKind}},
                       {QStringLiteral("#t"), QJsonArray{topic}},
                       {QStringLiteral("since"), QDateTime::currentSecsSinceEpoch() - kMaxAgeSecs}});
    m_pool->subscribe(QString::fromLatin1(kSignalSubscription),
                      {{QStringLiteral("kinds"), QJsonArray{kSignalKind}},
                       {QStringLiteral("#p"), QJsonArray{m_session.publicKeyHex()}},
                       {QStringLiteral("since"), QDateTime::currentSecsSinceEpoch() - 60}});
    m_pool->setRelays(m_relays);

    m_announceTimer = new QTimer(this);
    connect(m_announceTimer, &QTimer::timeout, this, &LobbyNetwork::announce);
    m_announceTimer->start(m_announceMs);
    // The relays may have dropped events of live games: they get them again
    // once they are connected (what they have already they keep once).
    QTimer::singleShot(qMin(15000, m_announceMs), this, &LobbyNetwork::republish);
    updateState();
}

void LobbyNetwork::stop()
{
    // Deleted now, not later: when the application ends there is no later.
    for (Peer *peer : std::as_const(m_peers)) {
        peer->rtc->close();
        delete peer->rtc;
        delete peer;
    }
    m_peers.clear();
    delete m_announceTimer;
    m_announceTimer = nullptr;
    delete m_pool;
    m_pool = nullptr;
    updateState();
}

int LobbyNetwork::relayCount() const
{
    return m_pool ? m_pool->connectedCount() : 0;
}

int LobbyNetwork::peerCount() const
{
    int open = 0;
    for (const Peer *peer : m_peers)
        open += peer->open ? 1 : 0;
    return open;
}

void LobbyNetwork::updateState()
{
    m_node->setNetworkState(relayCount(), peerCount());
    Q_EMIT stateChanged();
}

void LobbyNetwork::onRelayEvent(const QString &subscription, const NostrEvent &event)
{
    if (subscription == QLatin1String(kLedgerSubscription)) {
        m_fromRelays.insert(event.id);
        m_node->receive(event.toJson());
    } else if (subscription == QLatin1String(kAnnounceSubscription)) {
        onAnnounce(event);
    } else if (subscription == QLatin1String(kSignalSubscription)) {
        onSignal(event);
    }
}

void LobbyNetwork::announce()
{
    if (!m_pool || m_pool->connectedCount() == 0)
        return;
    const QJsonObject content{{QStringLiteral("events"), m_node->ledger().size()}};
    m_pool->publish(NostrEvent::create(m_session, kAnnounceKind,
                                       {{QStringLiteral("t"), QString::fromLatin1(LobbyLedger::kTopic)}},
                                       QString::fromUtf8(QJsonDocument(content).toJson(QJsonDocument::Compact))));
}

void LobbyNetwork::onAnnounce(const NostrEvent &event)
{
    const QString session = event.pubkey;
    if (session == m_session.publicKeyHex() || m_peers.contains(session) || m_peers.size() >= kMaxPeers)
        return;
    if (qAbs(QDateTime::currentSecsSinceEpoch() - event.createdAt) > kMaxAgeSecs)
        return;
    // One of the two offers: the one whose session key sorts first. The
    // other waits for the offer, so two nodes make one connection.
    if (m_session.publicKeyHex() > session) {
        announce(); // So that it hears of us now, not in a minute.
        return;
    }
    Peer *peer = addPeer(session);
    peer->offering = true;
    peer->rtc->createOffer();
}

void LobbyNetwork::onSignal(const NostrEvent &event)
{
    if (qAbs(QDateTime::currentSecsSinceEpoch() - event.createdAt) > kMaxAgeSecs)
        return;
    const std::optional<QByteArray> shared = m_session.sharedX(QByteArray::fromHex(event.pubkey.toLatin1()));
    if (!shared)
        return;
    const std::optional<QByteArray> plaintext = Nip44::decrypt(event.content, Nip44::conversationKey(*shared));
    if (!plaintext)
        return;
    const QJsonObject message = QJsonDocument::fromJson(*plaintext).object();
    const QString type = message.value(QStringLiteral("t")).toString();
    const QString sdp = message.value(QStringLiteral("sdp")).toString();
    if (type == QLatin1String("offer")) {
        if (m_peers.contains(event.pubkey) || m_peers.size() >= kMaxPeers)
            return;
        Peer *peer = addPeer(event.pubkey);
        if (!peer->rtc->acceptOffer(sdp))
            removePeer(event.pubkey);
    } else if (type == QLatin1String("answer")) {
        Peer *peer = m_peers.value(event.pubkey);
        if (peer && peer->offering && !peer->rtc->acceptAnswer(sdp))
            removePeer(event.pubkey);
    }
}

LobbyNetwork::Peer *LobbyNetwork::addPeer(const QString &session)
{
    auto *peer = new Peer;
    peer->rtc = new WebRtcPeer(m_iceServers, this);
    peer->started = QDateTime::currentSecsSinceEpoch();
    m_peers.insert(session, peer);
    connect(peer->rtc, &WebRtcPeer::localDescriptionReady, this, [this, session](const QString &sdp) {
        const Peer *peer = m_peers.value(session);
        if (peer)
            sendSignal(session, {{QStringLiteral("t"), peer->offering ? QStringLiteral("offer") : QStringLiteral("answer")},
                                 {QStringLiteral("sdp"), sdp}});
    });
    connect(peer->rtc, &WebRtcPeer::channelOpened, this, [this, session] {
        Peer *peer = m_peers.value(session);
        if (!peer)
            return;
        peer->open = true;
        updateState();
        // What we have, so the other sends what we lack (and we what it lacks).
        peer->rtc->sendText(QJsonDocument(QJsonObject{{QStringLiteral("t"), QStringLiteral("have")},
                                                      {QStringLiteral("ids"), QJsonArray::fromStringList(m_node->ledger().ids())}})
                                .toJson(QJsonDocument::Compact));
    });
    connect(peer->rtc, &WebRtcPeer::textReceived, this,
            [this, session](const QByteArray &text) { onPeerText(session, text); });
    connect(peer->rtc, &WebRtcPeer::channelClosed, this, [this, session] { removePeer(session); });
    connect(peer->rtc, &WebRtcPeer::failed, this, [this, session] { removePeer(session); });
    // A connection that never opens is given up, so another try can come.
    QTimer::singleShot(45000, this, [this, session] {
        if (const Peer *peer = m_peers.value(session); peer && !peer->open)
            removePeer(session);
    });
    return peer;
}

void LobbyNetwork::removePeer(const QString &session)
{
    Peer *peer = m_peers.take(session);
    if (!peer)
        return;
    peer->rtc->close();
    peer->rtc->deleteLater();
    delete peer;
    updateState();
}

void LobbyNetwork::sendSignal(const QString &session, const QJsonObject &message)
{
    if (!m_pool)
        return;
    const std::optional<QByteArray> shared = m_session.sharedX(QByteArray::fromHex(session.toLatin1()));
    if (!shared)
        return;
    const std::optional<QString> content =
        Nip44::encrypt(QJsonDocument(message).toJson(QJsonDocument::Compact), Nip44::conversationKey(*shared));
    if (content)
        m_pool->publish(NostrEvent::create(m_session, kSignalKind, {{QStringLiteral("p"), session}}, *content));
}

void LobbyNetwork::onPeerText(const QString &session, const QByteArray &text)
{
    Peer *peer = m_peers.value(session);
    if (!peer)
        return;
    const QJsonObject message = QJsonDocument::fromJson(text).object();
    const QString type = message.value(QStringLiteral("t")).toString();
    if (type == QLatin1String("have")) {
        // The other's ids: send it every event it lacks.
        QSet<QString> theirs;
        for (const QJsonValue &id : message.value(QStringLiteral("ids")).toArray())
            theirs.insert(id.toString());
        QList<QJsonObject> missing;
        for (const LedgerEvent &event : m_node->ledger().events()) {
            if (!theirs.contains(event.id))
                missing << event.signedEvent;
        }
        sendEvents(*peer, missing);
    } else if (type == QLatin1String("events")) {
        for (const QJsonValue &event : message.value(QStringLiteral("events")).toArray())
            m_node->receive(event.toObject()); // New ones come back through spread() to the others.
    }
}

void LobbyNetwork::sendEvents(Peer &peer, const QList<QJsonObject> &events)
{
    for (int from = 0; from < events.size(); from += kBatch) {
        QJsonArray batch;
        for (int i = from; i < qMin(int(events.size()), from + kBatch); ++i)
            batch.append(events.at(i));
        peer.rtc->sendText(QJsonDocument(QJsonObject{{QStringLiteral("t"), QStringLiteral("events")},
                                                     {QStringLiteral("events"), batch}})
                               .toJson(QJsonDocument::Compact));
    }
}

void LobbyNetwork::spread(const QJsonObject &event)
{
    const QString id = event.value(QStringLiteral("id")).toString();
    if (m_pool && !m_fromRelays.contains(id)) {
        if (const std::optional<NostrEvent> signedEvent = NostrEvent::fromJson(event))
            m_pool->publish(*signedEvent);
    }
    for (Peer *peer : std::as_const(m_peers)) {
        if (peer->open)
            sendEvents(*peer, {event});
    }
}

void LobbyNetwork::republish()
{
    if (!m_pool)
        return;
    for (const LedgerEvent &event : m_node->ledger().events()) {
        if (m_fromRelays.contains(event.id))
            continue; // A relay has it.
        if (const std::optional<NostrEvent> signedEvent = NostrEvent::fromJson(event.signedEvent))
            m_pool->publish(*signedEvent);
    }
}
