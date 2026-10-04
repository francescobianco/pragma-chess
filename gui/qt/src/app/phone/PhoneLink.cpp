#include "PhoneLink.h"

#include "Nip44.h"
#include "NostrEvent.h"
#include "NostrRelayPool.h"
#include "PhoneGameStore.h"
#include "PhoneLinkSession.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRandomGenerator>
#include <QSaveFile>
#include <QSysInfo>

namespace {

constexpr qint64 kMaxClockSkewSecs = 120;
constexpr char kSubscription[] = "pragma-link";

bool equalConstantTime(const QByteArray &a, const QByteArray &b)
{
    if (a.size() != b.size())
        return false;
    unsigned char difference = 0;
    for (qsizetype i = 0; i < a.size(); ++i)
        difference |= static_cast<unsigned char>(a.at(i) ^ b.at(i));
    return difference == 0;
}

bool isSessionId(const QString &id)
{
    if (id.size() < 16 || id.size() > 64)
        return false;
    for (const QChar c : id) {
        if (!c.isLetterOrNumber() && c != QLatin1Char('-'))
            return false;
    }
    return true;
}

} // namespace

PhoneLink::PhoneLink(QString stateFile, QString databasesDir, QObject *parent)
    : QObject(parent)
    , m_stateFile(std::move(stateFile))
    , m_files(std::move(databasesDir))
    , m_key(NostrKey::generate())
    , m_relays(defaultRelays())
    , m_iceServers(defaultIceServers())
    , m_computerName(QSysInfo::machineHostName())
{
    loadState();
    updateListening();
}

PhoneLink::~PhoneLink()
{
    // Sessions close their peers before the relays go.
    qDeleteAll(m_sessions);
    m_sessions.clear();
}

QStringList PhoneLink::defaultRelays()
{
    return {QStringLiteral("wss://relay.damus.io"), QStringLiteral("wss://nos.lol"),
            QStringLiteral("wss://relay.primal.net")};
}

QStringList PhoneLink::defaultIceServers()
{
    return {QStringLiteral("stun:stun.l.google.com:19302"), QStringLiteral("stun:stun.cloudflare.com:3478")};
}

void PhoneLink::setRelays(const QStringList &relays)
{
    m_relays = relays;
    if (m_pool)
        m_pool->setRelays(m_relays);
    Q_EMIT pairingLinkChanged();
}

void PhoneLink::setIceServers(const QStringList &servers)
{
    m_iceServers = servers;
}

QList<PhoneFiles::Entry> PhoneLink::listFiles()
{
    PhoneFiles::Describe describe;
    if (m_gameStore)
        describe = [store = m_gameStore](const QString &path) { return store->describe(path); };
    QList<PhoneFiles::Entry> entries = m_files.list(describe);
    // A database deleted here after a phone deleted it is not offered again,
    // even if a copy came back (from a device that did not know yet).
    entries.removeIf([this](const PhoneFiles::Entry &entry) { return m_deletedLineages.contains(entry.id); });
    return entries;
}

QString PhoneLink::databasePath(const QString &lineage)
{
    if (lineage.isEmpty())
        return {};
    for (const PhoneFiles::Entry &entry : listFiles()) {
        if (entry.id == lineage)
            return PhoneFiles::resolve(m_files.root(), entry.name).value_or(QString());
    }
    return {};
}

bool PhoneLink::requestDeletion(const DeletionRequest &request)
{
    if (m_deletedLineages.contains(request.lineage))
        return true; // Gone already.
    for (DeletionRequest &known : m_deletionRequests) {
        if (known.lineage == request.lineage) {
            known = request; // The latest phone to say so.
            saveState();
            Q_EMIT deletionRequested(request.lineage);
            return true;
        }
    }
    m_deletionRequests.append(request);
    saveState();
    Q_EMIT deletionRequested(request.lineage);
    return true;
}

void PhoneLink::resolveDeletion(const QString &lineage, bool deletedHere)
{
    m_deletionRequests.removeIf([&lineage](const DeletionRequest &request) { return request.lineage == lineage; });
    if (deletedHere && !lineage.isEmpty())
        m_deletedLineages.insert(lineage);
    saveState();
}

void PhoneLink::setComputerName(const QString &name)
{
    m_computerName = name;
    Q_EMIT pairingLinkChanged();
}

void PhoneLink::loadState()
{
    QFile file(m_stateFile);
    if (!file.open(QIODevice::ReadOnly))
        return; // Never used: the new key is kept when pairing begins.
    const QJsonObject state = QJsonDocument::fromJson(file.readAll()).object();
    if (const std::optional<NostrKey> key =
            NostrKey::fromSecret(QByteArray::fromHex(state.value(QStringLiteral("secret")).toString().toLatin1())))
        m_key = *key;
    else
        saveState();
    for (const QJsonValue &value : state.value(QStringLiteral("devices")).toArray()) {
        const QJsonObject object = value.toObject();
        Device device;
        device.key = object.value(QStringLiteral("key")).toString();
        device.name = object.value(QStringLiteral("name")).toString();
        device.pairedAt = QDateTime::fromString(object.value(QStringLiteral("pairedAt")).toString(), Qt::ISODate);
        device.lastSyncAt = QDateTime::fromString(object.value(QStringLiteral("lastSyncAt")).toString(), Qt::ISODate);
        if (device.key.size() == 64)
            m_devices.append(device);
    }
    for (const QJsonValue &value : state.value(QStringLiteral("deletionRequests")).toArray()) {
        const QJsonObject object = value.toObject();
        DeletionRequest request;
        request.lineage = object.value(QStringLiteral("db")).toString();
        request.name = object.value(QStringLiteral("name")).toString();
        request.phoneKey = object.value(QStringLiteral("phoneKey")).toString();
        request.phoneName = object.value(QStringLiteral("phoneName")).toString();
        request.when = QDateTime::fromString(object.value(QStringLiteral("when")).toString(), Qt::ISODate);
        if (!request.lineage.isEmpty())
            m_deletionRequests.append(request);
    }
    for (const QJsonValue &value : state.value(QStringLiteral("deletedDatabases")).toArray())
        m_deletedLineages.insert(value.toString());
}

void PhoneLink::saveState() const
{
    QJsonArray devices;
    for (const Device &device : m_devices) {
        devices.append(QJsonObject{
            {QStringLiteral("key"), device.key},
            {QStringLiteral("name"), device.name},
            {QStringLiteral("pairedAt"), device.pairedAt.toUTC().toString(Qt::ISODate)},
            {QStringLiteral("lastSyncAt"),
             device.lastSyncAt.isValid() ? device.lastSyncAt.toUTC().toString(Qt::ISODate) : QString()},
        });
    }
    QJsonArray requests;
    for (const DeletionRequest &request : m_deletionRequests) {
        requests.append(QJsonObject{
            {QStringLiteral("db"), request.lineage},
            {QStringLiteral("name"), request.name},
            {QStringLiteral("phoneKey"), request.phoneKey},
            {QStringLiteral("phoneName"), request.phoneName},
            {QStringLiteral("when"), request.when.isValid() ? request.when.toUTC().toString(Qt::ISODate) : QString()},
        });
    }
    QStringList deleted = m_deletedLineages.values();
    deleted.sort();
    QJsonObject state{
        {QStringLiteral("secret"), QString::fromLatin1(m_key.secret().toHex())},
        {QStringLiteral("devices"), devices},
    };
    if (!requests.isEmpty())
        state.insert(QStringLiteral("deletionRequests"), requests);
    if (!deleted.isEmpty())
        state.insert(QStringLiteral("deletedDatabases"), QJsonArray::fromStringList(deleted));
    QDir().mkpath(QFileInfo(m_stateFile).absolutePath());
    QSaveFile file(m_stateFile);
    if (!file.open(QIODevice::WriteOnly))
        return;
    file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner); // Holds a secret key.
    file.write(QJsonDocument(state).toJson());
    file.commit();
}

PairingLink PhoneLink::beginPairing()
{
    if (!QFileInfo::exists(m_stateFile))
        saveState(); // The key the phone will know.
    if (m_pairingSecret.isEmpty()) {
        m_pairingSecret = QByteArray(32, Qt::Uninitialized);
        QRandomGenerator::system()->generate(m_pairingSecret.begin(), m_pairingSecret.end());
    }
    updateListening();
    return pairingLink();
}

void PhoneLink::endPairing()
{
    m_pairingSecret.clear();
    updateListening();
}

PairingLink PhoneLink::pairingLink() const
{
    if (m_pairingSecret.isEmpty())
        return {};
    return {m_key.publicKey(), m_pairingSecret, m_computerName, m_relays};
}

int PhoneLink::deviceIndex(const QString &key) const
{
    for (qsizetype i = 0; i < m_devices.size(); ++i) {
        if (m_devices.at(i).key == key)
            return int(i);
    }
    return -1;
}

void PhoneLink::removeDevice(const QString &key)
{
    const int index = deviceIndex(key);
    if (index < 0)
        return;
    m_devices.removeAt(index);
    saveState();
    for (PhoneLinkSession *session : m_sessions.values()) {
        if (session->phoneKey() == key) {
            m_sessions.remove(session->id());
            delete session;
        }
    }
    Q_EMIT devicesChanged();
    updateListening();
    Q_EMIT statusChanged();
}

bool PhoneLink::isListening() const
{
    return m_pool != nullptr;
}

int PhoneLink::connectedRelays() const
{
    return m_pool ? m_pool->connectedCount() : 0;
}

int PhoneLink::activeSessions() const
{
    int count = 0;
    for (const PhoneLinkSession *session : m_sessions)
        count += session->isOpen() ? 1 : 0;
    return count;
}

void PhoneLink::updateListening()
{
    const bool listen = isPairing() || !m_devices.isEmpty();
    if (listen == (m_pool != nullptr))
        return;
    if (listen) {
        m_pool = new NostrRelayPool(this);
        connect(m_pool, &NostrRelayPool::connectedCountChanged, this, &PhoneLink::statusChanged);
        connect(m_pool, &NostrRelayPool::eventReceived, this,
                [this](const QString &, const NostrEvent &event) { onEvent(event); });
        m_pool->subscribe(QString::fromLatin1(kSubscription),
                          {{QStringLiteral("kinds"), QJsonArray{kSignalingKind}},
                           {QStringLiteral("#p"), QJsonArray{m_key.publicKeyHex()}},
                           {QStringLiteral("since"), QDateTime::currentSecsSinceEpoch() - 60}});
        m_pool->setRelays(m_relays);
    } else {
        qDeleteAll(m_sessions);
        m_sessions.clear();
        delete m_pool;
        m_pool = nullptr;
    }
    Q_EMIT statusChanged();
}

void PhoneLink::onEvent(const NostrEvent &event)
{
    if (event.kind != kSignalingKind || event.tagValue(QStringLiteral("p")) != m_key.publicKeyHex())
        return;
    if (qAbs(QDateTime::currentSecsSinceEpoch() - event.createdAt) > kMaxClockSkewSecs)
        return;
    const QByteArray sender = QByteArray::fromHex(event.pubkey.toLatin1());
    const std::optional<QByteArray> shared = m_key.sharedX(sender);
    if (!shared)
        return;
    const std::optional<QByteArray> plaintext = Nip44::decrypt(event.content, Nip44::conversationKey(*shared));
    if (!plaintext)
        return;
    const QJsonObject message = QJsonDocument::fromJson(*plaintext).object();
    if (message.value(QStringLiteral("t")).toString() != QLatin1String("offer"))
        return;
    const QString session = message.value(QStringLiteral("session")).toString();
    if (!isSessionId(session) || m_answeredSessions.contains(session) || m_sessions.contains(session))
        return;
    m_answeredSessions.insert(session);

    QString phoneName = message.value(QStringLiteral("name")).toString().trimmed().left(80);
    if (deviceIndex(event.pubkey) < 0) {
        const QByteArray pair = QByteArray::fromHex(message.value(QStringLiteral("pair")).toString().toLatin1());
        if (!isPairing() || !equalConstantTime(pair, m_pairingSecret)) {
            sendSignal(sender, {{QStringLiteral("t"), QStringLiteral("refused")},
                                {QStringLiteral("session"), session},
                                {QStringLiteral("reason"), QStringLiteral("not-paired")}});
            return;
        }
        if (phoneName.isEmpty())
            phoneName = tr("Phone");
        m_devices.append({event.pubkey, phoneName, QDateTime::currentDateTimeUtc(), {}});
        saveState();
        // One phone per code: the next one gets a new secret.
        QRandomGenerator::system()->generate(m_pairingSecret.begin(), m_pairingSecret.end());
        Q_EMIT devicesChanged();
        Q_EMIT devicePaired(phoneName);
        Q_EMIT pairingLinkChanged();
    } else {
        Device &device = m_devices[deviceIndex(event.pubkey)];
        if (!phoneName.isEmpty() && device.name != phoneName) {
            device.name = phoneName;
            saveState();
            Q_EMIT devicesChanged();
        } else {
            phoneName = device.name;
        }
    }

    auto *peerSession = new PhoneLinkSession(this, session, event.pubkey, phoneName, m_iceServers);
    m_sessions.insert(session, peerSession);
    connect(peerSession, &PhoneLinkSession::answerReady, this, [this, sender, session](const QString &sdp) {
        sendSignal(sender, {{QStringLiteral("t"), QStringLiteral("answer")},
                            {QStringLiteral("session"), session},
                            {QStringLiteral("sdp"), sdp},
                            {QStringLiteral("name"), m_computerName}});
    });
    if (!peerSession->start(message.value(QStringLiteral("sdp")).toString())) {
        m_sessions.remove(session);
        delete peerSession;
        return;
    }
    Q_EMIT statusChanged();
}

void PhoneLink::sendSignal(const QByteArray &recipient, const QJsonObject &message)
{
    if (!m_pool)
        return;
    const std::optional<QByteArray> shared = m_key.sharedX(recipient);
    if (!shared)
        return;
    const std::optional<QString> content =
        Nip44::encrypt(QJsonDocument(message).toJson(QJsonDocument::Compact), Nip44::conversationKey(*shared));
    if (!content)
        return;
    m_pool->publish(NostrEvent::create(m_key, kSignalingKind,
                                       {{QStringLiteral("p"), QString::fromLatin1(recipient.toHex())}}, *content));
}

void PhoneLink::touchDevice(const QString &key)
{
    const int index = deviceIndex(key);
    if (index < 0)
        return;
    m_devices[index].lastSyncAt = QDateTime::currentDateTimeUtc();
    saveState();
    Q_EMIT devicesChanged();
}

void PhoneLink::setActivity(const QString &activity)
{
    if (m_activity == activity)
        return;
    m_activity = activity;
    Q_EMIT statusChanged();
}

void PhoneLink::sessionFinished(PhoneLinkSession *session)
{
    // Removed already if its phone was unpaired.
    if (m_sessions.value(session->id()) != session)
        return;
    m_sessions.remove(session->id());
    session->deleteLater();
    if (m_sessions.isEmpty())
        setActivity(QString());
    Q_EMIT statusChanged();
}
