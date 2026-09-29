#pragma once

#include "NostrKey.h"
#include "PairingLink.h"
#include "PhoneFiles.h"

#include <QDateTime>
#include <QHash>
#include <QList>
#include <QObject>
#include <QSet>
#include <QStringList>

class NostrEvent;
class NostrRelayPool;
class PhoneGameStore;
class PhoneLinkSession;

/// The computer's side of Phone Link (docs/phone-link.md): answers the
/// WebRTC offers of paired phones found on Nostr relays, serves them the
/// databases of the Databases folder and stores the games they send.
///
/// It listens on the relays while pairing (the Connect Mobile App dialog is
/// open) or while at least one phone is paired, and not otherwise.
class PhoneLink : public QObject {
    Q_OBJECT

public:
    /// Nostr event kind of the offers and answers (ephemeral).
    static constexpr int kSignalingKind = 25050;

    struct Device {
        QString key; // Public key, hex.
        QString name;
        QDateTime pairedAt;
        QDateTime lastSyncAt;
    };

    /// `stateFile` keeps the computer's key and the paired phones (per
    /// device, e.g. AppLocalData/phone-link.json); `databasesDir` is served.
    PhoneLink(QString stateFile, QString databasesDir, QObject *parent = nullptr);
    ~PhoneLink() override;

    static QStringList defaultRelays();
    static QStringList defaultIceServers();
    void setRelays(const QStringList &relays);
    void setIceServers(const QStringList &servers);
    void setComputerName(const QString &name);
    QString computerName() const { return m_computerName; }
    /// Receives the games of "put"; without one, puts are refused.
    void setGameStore(PhoneGameStore *store) { m_gameStore = store; }

    QString publicKeyHex() const { return m_key.publicKeyHex(); }

    /// Starts accepting one new phone and returns the link to show it.
    PairingLink beginPairing();
    /// Stops accepting new phones (paired ones still connect).
    void endPairing();
    bool isPairing() const { return !m_pairingSecret.isEmpty(); }
    /// The link for the current pairing secret (empty when not pairing).
    PairingLink pairingLink() const;

    QStringList relays() const { return m_relays; }

    QList<Device> devices() const { return m_devices; }
    /// Unpairs a phone: its offers are refused from now on.
    void removeDevice(const QString &key);

    bool isListening() const;
    int connectedRelays() const;
    int activeSessions() const;
    /// What is being sent or received now, for a status line (empty when idle).
    QString activity() const { return m_activity; }

Q_SIGNALS:
    void devicesChanged();
    /// A phone paired with the secret of the current link, which is then
    /// replaced by a new one (pairingLinkChanged).
    void devicePaired(const QString &name);
    void pairingLinkChanged();
    /// Relays, sessions or activity changed.
    void statusChanged();
    /// Games from a phone were stored in (or replaced in) the database at `path`.
    void gamesStored(const QString &path, int count);

private:
    friend class PhoneLinkSession;

    void loadState();
    void saveState() const;
    void updateListening();
    void onEvent(const NostrEvent &event);
    void sendSignal(const QByteArray &recipient, const QJsonObject &message);
    void touchDevice(const QString &key);
    /// The databases offered, described by the game store (ids, game counts).
    QList<PhoneFiles::Entry> listFiles();
    void setActivity(const QString &activity);
    void sessionFinished(PhoneLinkSession *session);
    int deviceIndex(const QString &key) const;

    QString m_stateFile;
    PhoneFiles m_files;
    NostrKey m_key;
    QStringList m_relays;
    QStringList m_iceServers;
    QString m_computerName;
    PhoneGameStore *m_gameStore = nullptr;
    QByteArray m_pairingSecret;
    QList<Device> m_devices;
    NostrRelayPool *m_pool = nullptr;
    QHash<QString, PhoneLinkSession *> m_sessions;
    QSet<QString> m_answeredSessions;
    QString m_activity;
};
