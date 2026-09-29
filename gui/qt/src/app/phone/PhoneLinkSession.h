#pragma once

#include <QByteArray>
#include <QJsonObject>
#include <QList>
#include <QObject>
#include <QString>

#include <memory>

class PhoneLink;
class QTemporaryFile;
class QTimer;
class WebRtcPeer;

/// One phone connected to the computer: its WebRTC peer and the requests it
/// sends on the data channel ("list", "get", "put"), answered one at a time.
class PhoneLinkSession : public QObject {
    Q_OBJECT

public:
    PhoneLinkSession(PhoneLink *link, QString sessionId, QString phoneKey, QString phoneName,
                     const QStringList &iceServers);
    ~PhoneLinkSession() override;

    /// Takes the phone's offer; the answer comes with answerReady.
    bool start(const QString &offerSdp);
    QString id() const { return m_id; }
    QString phoneKey() const { return m_phoneKey; }
    bool isOpen() const { return m_open; }

    static constexpr int kChunkSize = 16 * 1024;

Q_SIGNALS:
    void answerReady(const QString &sdp);

private:
    void onText(const QByteArray &text);
    void processNext();
    void handle(const QJsonObject &request);
    void sendJson(const QJsonObject &message);
    void sendError(const QString &message);
    void startTransfer(const QString &name);
    void pump();
    void finish();

    PhoneLink *m_link;
    QString m_id;
    QString m_phoneKey;
    QString m_phoneName;
    WebRtcPeer *m_peer;
    QTimer *m_idleTimer;
    bool m_open = false;
    bool m_finished = false;
    QList<QJsonObject> m_requests;
    // The file being sent: a snapshot, how much of it is sent.
    std::unique_ptr<QTemporaryFile> m_snapshot;
    QString m_sendingName;
    qint64 m_sent = 0;
};
