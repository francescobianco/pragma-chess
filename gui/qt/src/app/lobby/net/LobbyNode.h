#pragma once

#include "app/lobby/LobbyLedger.h"
#include "app/lobby/LobbyService.h"
#include "app/phone/NostrKey.h"

#include <QHash>
#include <QJsonObject>

/// This computer in the lobby: the user's key, the ledger it keeps (a file
/// of signed events, one per line), the plans of the user (another file,
/// never sent) and the lobby they make. What the user does becomes a signed
/// event; what comes from the network is checked and added; every new
/// event, either way, is offered to the network (`eventAdded`) to be handed
/// on to the other peers. After each change the user's plans are played
/// where the games reached a position they answer.
class LobbyNode : public LobbyService {
    Q_OBJECT

public:
    /// `ledgerPath` and `plansPath` are the files it keeps; they are read now.
    LobbyNode(const NostrKey &key, const QString &ledgerPath, const QString &plansPath, QObject *parent = nullptr);

    /// The name the user goes by, put in the events that seat them.
    void setName(const QString &name) { m_name = name; }

    const Lobby &lobby() const override { return m_lobby; }
    QString me() const override { return m_key.publicKeyHex(); }
    QString openRoom(quint32 seed) override;
    bool joinRoom(const QString &roomId) override;
    bool sendMove(const QString &roomId, const QString &white, const QString &black, const QString &uci) override;
    void setPlan(const QString &roomId, const QString &white, const QString &black, const LobbyPlan &plan) override;
    LobbyPlan plan(const QString &roomId, const QString &white, const QString &black) const override;
    bool resign(const QString &roomId, const QString &white, const QString &black) override;
    int relayCount() const override { return m_relayCount; }
    int peerCount() const override { return m_peerCount; }
    bool isOnline() const override { return m_online; }

    /// An event from the network: added when it is a valid, signed event
    /// of the ledger not known yet. True when it was new.
    bool receive(const QJsonObject &event);
    const LobbyLedger &ledger() const { return m_ledger; }
    /// What the transport says about the network, shown to the user.
    void setNetworkState(int relays, int peers);

Q_SIGNALS:
    /// A new event in the ledger, made here or received: to hand on.
    void eventAdded(const QJsonObject &event);

private:
    /// Signs `content` as an event of the user and adds it.
    bool publish(const QJsonObject &content, const QString &room);
    bool add(const QJsonObject &event, bool save);
    void rebuild();
    /// Plays the user's prepared answers where the games wait for them.
    void runPlans();
    void loadLedger();
    void loadPlans();
    void savePlans() const;
    static QString gameKey(const QString &roomId, const QString &white, const QString &black);

    NostrKey m_key;
    QString m_name;
    QString m_ledgerPath;
    QString m_plansPath;
    LobbyLedger m_ledger;
    Lobby m_lobby;
    QHash<QString, LobbyPlan> m_plans;
    bool m_runningPlans = false;
    bool m_online = false;
    int m_relayCount = 0;
    int m_peerCount = 0;
};
