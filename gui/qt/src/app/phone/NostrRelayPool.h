#pragma once

#include "NostrEvent.h"

#include <QHash>
#include <QJsonObject>
#include <QObject>
#include <QSet>
#include <QStringList>

#include <memory>

class QTimer;

namespace rtc {
class WebSocket;
}

/// Connections to a few Nostr relays (NIP-01 over WebSocket): keeps the
/// subscriptions open, reconnects with a growing delay, publishes to every
/// relay and delivers each verified event once.
///
/// The sockets are libdatachannel's; their callbacks run on its threads and
/// are handed to the Qt thread of this object before anything else happens.
class NostrRelayPool : public QObject {
    Q_OBJECT

public:
    explicit NostrRelayPool(QObject *parent = nullptr);
    ~NostrRelayPool() override;

    /// Connects to these relays (and drops the others).
    void setRelays(const QStringList &urls);
    QStringList relays() const;
    /// Closes every connection.
    void clear();

    /// Opens (or replaces) a subscription on every relay, now and after each reconnect.
    void subscribe(const QString &id, const QJsonObject &filter);
    void unsubscribe(const QString &id);
    /// Sends an event to the relays that are connected, and to the others as
    /// soon as they connect (within a minute).
    void publish(const NostrEvent &event);

    int connectedCount() const;

Q_SIGNALS:
    /// A new event with a valid id and signature, from one of the subscriptions.
    void eventReceived(const QString &subscription, const NostrEvent &event);
    void connectedCountChanged(int count);

private:
    struct Relay;

    void connectRelay(const QString &url);
    void scheduleReconnect(const QString &url);
    void onOpen(const QString &url, quint64 generation);
    void onClosed(const QString &url, quint64 generation);
    void onMessage(const QString &url, quint64 generation, const QByteArray &text);
    void sendTo(Relay &relay, const QByteArray &text);
    void closeRelay(Relay &relay);

    QHash<QString, std::shared_ptr<Relay>> m_relays;
    QHash<QString, QJsonObject> m_subscriptions;
    QSet<QString> m_seen;
    QStringList m_seenOrder;
    quint64 m_generation = 0;
    int m_lastConnected = 0;
};
