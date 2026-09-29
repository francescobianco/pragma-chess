#include "NostrRelayPool.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTimer>

#include <rtc/websocket.hpp>

#include <chrono>

namespace {

constexpr int kFirstRetryMs = 2000;
constexpr int kLastRetryMs = 60000;
constexpr qint64 kPendingLifetimeSecs = 60;
constexpr int kSeenEvents = 512;

QByteArray compact(const QJsonArray &array)
{
    return QJsonDocument(array).toJson(QJsonDocument::Compact);
}

} // namespace

struct NostrRelayPool::Relay {
    QString url;
    std::shared_ptr<rtc::WebSocket> socket;
    quint64 generation = 0;
    bool open = false;
    int retryMs = kFirstRetryMs;
    QTimer *retryTimer = nullptr;
    // Events published while the relay was not connected: {sent at, text}.
    QList<QPair<qint64, QByteArray>> pending;
};

NostrRelayPool::NostrRelayPool(QObject *parent)
    : QObject(parent)
{
}

NostrRelayPool::~NostrRelayPool()
{
    clear();
}

void NostrRelayPool::setRelays(const QStringList &urls)
{
    for (const QString &url : m_relays.keys()) {
        if (!urls.contains(url)) {
            closeRelay(*m_relays.value(url));
            m_relays.remove(url);
        }
    }
    for (const QString &url : urls) {
        if (!m_relays.contains(url))
            connectRelay(url);
    }
    Q_EMIT connectedCountChanged(connectedCount());
}

QStringList NostrRelayPool::relays() const
{
    return m_relays.keys();
}

void NostrRelayPool::clear()
{
    for (const std::shared_ptr<Relay> &relay : std::as_const(m_relays))
        closeRelay(*relay);
    m_relays.clear();
    if (m_lastConnected != 0) {
        m_lastConnected = 0;
        Q_EMIT connectedCountChanged(0);
    }
}

void NostrRelayPool::closeRelay(Relay &relay)
{
    if (relay.retryTimer) {
        relay.retryTimer->stop();
        relay.retryTimer->deleteLater();
        relay.retryTimer = nullptr;
    }
    if (relay.socket) {
        // Waits for a callback in progress; none runs after this.
        relay.socket->resetCallbacks();
        relay.socket->close();
        relay.socket.reset();
    }
    relay.open = false;
}

void NostrRelayPool::connectRelay(const QString &url)
{
    std::shared_ptr<Relay> &relay = m_relays[url];
    if (!relay) {
        relay = std::make_shared<Relay>();
        relay->url = url;
    }
    const quint64 generation = ++m_generation;
    relay->generation = generation;
    relay->open = false;

    rtc::WebSocketConfiguration config;
    config.connectionTimeout = std::chrono::milliseconds(15000);
    config.pingInterval = std::chrono::milliseconds(30000);
    auto socket = std::make_shared<rtc::WebSocket>(config);
    // Callbacks come from libdatachannel's threads: hop to ours at once.
    socket->onOpen([this, url, generation] {
        QMetaObject::invokeMethod(this, [this, url, generation] { onOpen(url, generation); }, Qt::QueuedConnection);
    });
    socket->onClosed([this, url, generation] {
        QMetaObject::invokeMethod(this, [this, url, generation] { onClosed(url, generation); }, Qt::QueuedConnection);
    });
    socket->onError([this, url, generation](const std::string &) {
        QMetaObject::invokeMethod(this, [this, url, generation] { onClosed(url, generation); }, Qt::QueuedConnection);
    });
    socket->onMessage([this, url, generation](rtc::message_variant data) {
        if (const auto *text = std::get_if<std::string>(&data)) {
            QByteArray bytes(text->data(), qsizetype(text->size()));
            QMetaObject::invokeMethod(
                this, [this, url, generation, bytes] { onMessage(url, generation, bytes); }, Qt::QueuedConnection);
        }
    });
    relay->socket = socket;
    try {
        socket->open(url.toStdString());
    } catch (const std::exception &) {
        socket->resetCallbacks();
        relay->socket.reset();
        scheduleReconnect(url);
    }
}

void NostrRelayPool::scheduleReconnect(const QString &url)
{
    const std::shared_ptr<Relay> relay = m_relays.value(url);
    if (!relay)
        return;
    if (!relay->retryTimer) {
        relay->retryTimer = new QTimer(this);
        relay->retryTimer->setSingleShot(true);
        connect(relay->retryTimer, &QTimer::timeout, this, [this, url] {
            if (const std::shared_ptr<Relay> current = m_relays.value(url)) {
                if (current->socket) {
                    current->socket->resetCallbacks();
                    current->socket->close();
                    current->socket.reset();
                }
                connectRelay(url);
            }
        });
    }
    relay->retryTimer->start(relay->retryMs);
    relay->retryMs = qMin(relay->retryMs * 2, kLastRetryMs);
}

void NostrRelayPool::onOpen(const QString &url, quint64 generation)
{
    const std::shared_ptr<Relay> relay = m_relays.value(url);
    if (!relay || relay->generation != generation)
        return;
    relay->open = true;
    relay->retryMs = kFirstRetryMs;
    for (auto it = m_subscriptions.cbegin(); it != m_subscriptions.cend(); ++it)
        sendTo(*relay, compact({QStringLiteral("REQ"), it.key(), it.value()}));
    const qint64 now = QDateTime::currentSecsSinceEpoch();
    for (const auto &[sentAt, text] : std::as_const(relay->pending)) {
        if (now - sentAt <= kPendingLifetimeSecs)
            sendTo(*relay, text);
    }
    relay->pending.clear();
    if (connectedCount() != m_lastConnected) {
        m_lastConnected = connectedCount();
        Q_EMIT connectedCountChanged(m_lastConnected);
    }
}

void NostrRelayPool::onClosed(const QString &url, quint64 generation)
{
    const std::shared_ptr<Relay> relay = m_relays.value(url);
    if (!relay || relay->generation != generation)
        return;
    ++relay->generation; // Ignore whatever the old socket still reports.
    relay->open = false;
    scheduleReconnect(url);
    if (connectedCount() != m_lastConnected) {
        m_lastConnected = connectedCount();
        Q_EMIT connectedCountChanged(m_lastConnected);
    }
}

void NostrRelayPool::onMessage(const QString &url, quint64 generation, const QByteArray &text)
{
    const std::shared_ptr<Relay> relay = m_relays.value(url);
    if (!relay || relay->generation != generation)
        return;
    const QJsonArray message = QJsonDocument::fromJson(text).array();
    if (message.size() < 3 || message.at(0).toString() != QLatin1String("EVENT"))
        return; // EOSE, OK, NOTICE, CLOSED: nothing to do.
    const QString subscription = message.at(1).toString();
    if (!m_subscriptions.contains(subscription))
        return;
    const std::optional<NostrEvent> event = NostrEvent::fromJson(message.at(2).toObject());
    if (!event || m_seen.contains(event->id) || !event->verify())
        return;
    m_seen.insert(event->id);
    m_seenOrder << event->id;
    while (m_seenOrder.size() > kSeenEvents)
        m_seen.remove(m_seenOrder.takeFirst());
    Q_EMIT eventReceived(subscription, *event);
}

void NostrRelayPool::sendTo(Relay &relay, const QByteArray &text)
{
    if (!relay.socket || !relay.open)
        return;
    try {
        relay.socket->send(text.toStdString());
    } catch (const std::exception &) {
        // The close callback reconnects.
    }
}

void NostrRelayPool::subscribe(const QString &id, const QJsonObject &filter)
{
    m_subscriptions.insert(id, filter);
    for (const std::shared_ptr<Relay> &relay : std::as_const(m_relays))
        sendTo(*relay, compact({QStringLiteral("REQ"), id, filter}));
}

void NostrRelayPool::unsubscribe(const QString &id)
{
    if (!m_subscriptions.remove(id))
        return;
    for (const std::shared_ptr<Relay> &relay : std::as_const(m_relays))
        sendTo(*relay, compact({QStringLiteral("CLOSE"), id}));
}

void NostrRelayPool::publish(const NostrEvent &event)
{
    const QByteArray text = compact({QStringLiteral("EVENT"), event.toJson()});
    const qint64 now = QDateTime::currentSecsSinceEpoch();
    for (const std::shared_ptr<Relay> &relay : std::as_const(m_relays)) {
        if (relay->open)
            sendTo(*relay, text);
        else
            relay->pending.append({now, text});
    }
}

int NostrRelayPool::connectedCount() const
{
    int count = 0;
    for (const std::shared_ptr<Relay> &relay : m_relays)
        count += relay->open ? 1 : 0;
    return count;
}
