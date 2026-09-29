#include "PhoneLinkClient.h"

#include "Nip44.h"
#include "NostrEvent.h"
#include "NostrRelayPool.h"
#include "PhoneLink.h"
#include "WebRtcPeer.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRandomGenerator>
#include <QTimer>

namespace {

constexpr int kConnectTimeoutMs = 60 * 1000;

} // namespace

PhoneLinkClient::PhoneLinkClient(const NostrKey &key, const QByteArray &computerKey, const QStringList &relays,
                                 const QStringList &iceServers, const QString &phoneName, QObject *parent)
    : QObject(parent)
    , m_key(key)
    , m_computerKey(computerKey)
    , m_relays(relays)
    , m_phoneName(phoneName)
    , m_pool(new NostrRelayPool(this))
    , m_peer(new WebRtcPeer(iceServers, this))
    , m_timeout(new QTimer(this))
{
    QByteArray session(16, Qt::Uninitialized);
    QRandomGenerator::system()->generate(session.begin(), session.end());
    m_session = QString::fromLatin1(session.toHex());

    m_timeout->setSingleShot(true);
    connect(m_timeout, &QTimer::timeout, this, [this] {
        Q_EMIT failed(m_answered ? tr("The connection could not be established (NAT)")
                                 : tr("The computer did not answer"));
    });
    connect(m_pool, &NostrRelayPool::eventReceived, this,
            [this](const QString &, const NostrEvent &event) { onEvent(event); });
    connect(m_peer, &WebRtcPeer::localDescriptionReady, this, [this](const QString &sdp) {
        QJsonObject offer{{QStringLiteral("t"), QStringLiteral("offer")},
                          {QStringLiteral("session"), m_session},
                          {QStringLiteral("sdp"), sdp},
                          {QStringLiteral("name"), m_phoneName}};
        if (!m_pairingSecret.isEmpty())
            offer.insert(QStringLiteral("pair"), QString::fromLatin1(m_pairingSecret.toHex()));
        const std::optional<QByteArray> shared = m_key.sharedX(m_computerKey);
        const std::optional<QString> content =
            shared ? Nip44::encrypt(QJsonDocument(offer).toJson(QJsonDocument::Compact),
                                    Nip44::conversationKey(*shared))
                   : std::nullopt;
        if (!content) {
            Q_EMIT failed(tr("Invalid computer key"));
            return;
        }
        m_pool->publish(NostrEvent::create(m_key, PhoneLink::kSignalingKind,
                                           {{QStringLiteral("p"), QString::fromLatin1(m_computerKey.toHex())}},
                                           *content));
    });
    connect(m_peer, &WebRtcPeer::channelOpened, this, [this] {
        m_timeout->stop();
        Q_EMIT connected(m_computerName);
    });
    connect(m_peer, &WebRtcPeer::textReceived, this, [this](const QByteArray &text) {
        Q_EMIT messageReceived(QJsonDocument::fromJson(text).object());
    });
    connect(m_peer, &WebRtcPeer::binaryReceived, this, &PhoneLinkClient::binaryReceived);
    connect(m_peer, &WebRtcPeer::channelClosed, this, [this] { Q_EMIT failed(tr("The connection was closed")); });
    connect(m_peer, &WebRtcPeer::failed, this, [this] { Q_EMIT failed(tr("The connection failed")); });
}

PhoneLinkClient::~PhoneLinkClient()
{
    m_peer->close();
}

void PhoneLinkClient::start()
{
    m_pool->subscribe(QStringLiteral("pragma-phone"),
                      {{QStringLiteral("kinds"), QJsonArray{PhoneLink::kSignalingKind}},
                       {QStringLiteral("#p"), QJsonArray{m_key.publicKeyHex()}},
                       {QStringLiteral("since"), QDateTime::currentSecsSinceEpoch() - 60}});
    m_pool->setRelays(m_relays);
    m_peer->createOffer();
    m_timeout->start(kConnectTimeoutMs);
}

bool PhoneLinkClient::sendJson(const QJsonObject &message)
{
    return m_peer->sendText(QJsonDocument(message).toJson(QJsonDocument::Compact));
}

void PhoneLinkClient::onEvent(const NostrEvent &event)
{
    if (event.kind != PhoneLink::kSignalingKind || QByteArray::fromHex(event.pubkey.toLatin1()) != m_computerKey)
        return;
    const std::optional<QByteArray> shared = m_key.sharedX(m_computerKey);
    const std::optional<QByteArray> plaintext =
        shared ? Nip44::decrypt(event.content, Nip44::conversationKey(*shared)) : std::nullopt;
    if (!plaintext)
        return;
    const QJsonObject message = QJsonDocument::fromJson(*plaintext).object();
    if (message.value(QStringLiteral("session")).toString() != m_session || m_answered)
        return;
    const QString type = message.value(QStringLiteral("t")).toString();
    if (type == QLatin1String("refused")) {
        m_timeout->stop();
        Q_EMIT failed(tr("The computer refused the connection (%1)")
                          .arg(message.value(QStringLiteral("reason")).toString()));
    } else if (type == QLatin1String("answer")) {
        m_answered = true;
        m_computerName = message.value(QStringLiteral("name")).toString();
        if (!m_peer->acceptAnswer(message.value(QStringLiteral("sdp")).toString()))
            Q_EMIT failed(tr("Invalid answer"));
    }
}
