#pragma once

#include "NostrKey.h"

#include <QByteArray>
#include <QJsonObject>
#include <QObject>
#include <QStringList>

struct NostrEvent;
class NostrRelayPool;
class QTimer;
class WebRtcPeer;

/// The phone's side of Phone Link: offers a WebRTC connection to a computer
/// through the relays and talks to it on the data channel. The Android app
/// has its own implementation; this one drives `pragma-phone` and tests.
class PhoneLinkClient : public QObject {
    Q_OBJECT

public:
    PhoneLinkClient(const NostrKey &key, const QByteArray &computerKey, const QStringList &relays,
                    const QStringList &iceServers, const QString &phoneName, QObject *parent = nullptr);
    ~PhoneLinkClient() override;

    /// The secret of a pairing link, sent with the first offer only.
    void setPairingSecret(const QByteArray &secret) { m_pairingSecret = secret; }
    /// Subscribes, gathers and publishes the offer; `connected` or `failed` follows.
    void start();
    bool sendJson(const QJsonObject &message);

Q_SIGNALS:
    void connected(const QString &computerName);
    void messageReceived(const QJsonObject &message);
    void binaryReceived(const QByteArray &data);
    void failed(const QString &reason);

private:
    void onEvent(const NostrEvent &event);

    NostrKey m_key;
    QByteArray m_computerKey;
    QStringList m_relays;
    QString m_phoneName;
    QByteArray m_pairingSecret;
    QString m_session;
    QString m_computerName;
    NostrRelayPool *m_pool;
    WebRtcPeer *m_peer;
    QTimer *m_timeout;
    bool m_answered = false;
};
