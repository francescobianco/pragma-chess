#include "PhoneLinkSession.h"

#include "PhoneGameStore.h"
#include "PhoneLink.h"
#include "WebRtcPeer.h"
#include "app/ShippedOpeningNames.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QPointer>
#include <QTemporaryFile>
#include <QTimer>
#include <QUuid>

namespace {

// Closed if the channel has not opened by then, or has been silent that long.
constexpr int kConnectTimeoutMs = 90 * 1000;
constexpr int kIdleTimeoutMs = 10 * 60 * 1000;
// Queued on the channel before waiting for bufferedAmountLow.
constexpr qint64 kHighBufferedAmount = 1024 * 1024;

} // namespace

PhoneLinkSession::PhoneLinkSession(PhoneLink *link, QString sessionId, QString phoneKey, QString phoneName,
                                   const QStringList &iceServers)
    : QObject(link)
    , m_link(link)
    , m_id(std::move(sessionId))
    , m_phoneKey(std::move(phoneKey))
    , m_phoneName(std::move(phoneName))
    , m_peer(new WebRtcPeer(iceServers, this))
    , m_idleTimer(new QTimer(this))
{
    m_idleTimer->setSingleShot(true);
    connect(m_idleTimer, &QTimer::timeout, this, &PhoneLinkSession::finish);
    m_idleTimer->start(kConnectTimeoutMs);

    connect(m_peer, &WebRtcPeer::localDescriptionReady, this, &PhoneLinkSession::answerReady);
    connect(m_peer, &WebRtcPeer::channelOpened, this, [this] {
        m_open = true;
        m_idleTimer->start(kIdleTimeoutMs);
        m_link->touchDevice(m_phoneKey);
        Q_EMIT m_link->statusChanged();
    });
    connect(m_peer, &WebRtcPeer::textReceived, this, &PhoneLinkSession::onText);
    connect(m_peer, &WebRtcPeer::bufferedAmountLow, this, &PhoneLinkSession::pump);
    connect(m_peer, &WebRtcPeer::channelClosed, this, &PhoneLinkSession::finish);
    connect(m_peer, &WebRtcPeer::failed, this, &PhoneLinkSession::finish);
}

PhoneLinkSession::~PhoneLinkSession()
{
    m_peer->close();
}

bool PhoneLinkSession::start(const QString &offerSdp)
{
    return m_peer->acceptOffer(offerSdp);
}

void PhoneLinkSession::finish()
{
    if (m_finished)
        return;
    m_finished = true;
    m_idleTimer->stop();
    m_peer->close();
    m_snapshot.reset();
    // Leaves the peer's queued callbacks behind before going away.
    QMetaObject::invokeMethod(
        m_link, [link = m_link, self = QPointer<PhoneLinkSession>(this)] { if (self) link->sessionFinished(self); },
        Qt::QueuedConnection);
}

void PhoneLinkSession::onText(const QByteArray &text)
{
    m_idleTimer->start(kIdleTimeoutMs);
    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(text, &error);
    if (!document.isObject()) {
        sendError(QStringLiteral("not a JSON object"));
        return;
    }
    m_requests.append(document.object());
    if (!m_snapshot)
        processNext();
}

void PhoneLinkSession::processNext()
{
    while (!m_snapshot && !m_requests.isEmpty() && !m_finished)
        handle(m_requests.takeFirst());
}

void PhoneLinkSession::handle(const QJsonObject &request)
{
    const QString op = request.value(QStringLiteral("op")).toString();
    if (op == QLatin1String("list")) {
        QJsonArray files;
        for (const PhoneFiles::Entry &entry : m_link->listFiles())
            files.append(entry.toJson());
        sendJson({{QStringLiteral("op"), op},
                  {QStringLiteral("name"), m_link->computerName()},
                  {QStringLiteral("files"), files}});
        m_link->touchDevice(m_phoneKey);
    } else if (op == QLatin1String("get")) {
        startTransfer(request.value(QStringLiteral("name")).toString());
    } else if (op == QLatin1String("put")) {
        handlePut(request);
    } else {
        sendError(QStringLiteral("unknown op: %1").arg(op));
    }
}

void PhoneLinkSession::handlePut(const QJsonObject &request)
{
    // Addressed by lineage (docs/phone-link.md, "The sync, from the phone");
    // a put with only a name is from a phone that predates it.
    QString name = request.value(QStringLiteral("name")).toString();
    const QString lineageText = request.value(QStringLiteral("db")).toString().trimmed();
    DatabaseProperties properties;
    if (!lineageText.isEmpty()) {
        const QUuid lineage = QUuid::fromString(lineageText);
        if (lineage.isNull()) {
            sendError(QStringLiteral("invalid database id: %1").arg(lineageText));
            return;
        }
        QHash<QString, QString> values;
        const QJsonObject sent = request.value(QStringLiteral("properties")).toObject();
        for (auto it = sent.begin(); it != sent.end(); ++it)
            values.insert(it.key(), it.value().toString().left(4096));
        properties = DatabaseProperties::fromValues(values);
        properties.id = lineage.toString(QUuid::WithoutBraces);
        // The opening names we ship are reference data kept with the books,
        // updated by releases, not by phones: they are acknowledged as known and
        // never recreated among the databases of games.
        if (ShippedOpeningNames::byLineage(properties.id)) {
            const int count = int(request.value(QStringLiteral("games")).toArray().size());
            sendJson({{QStringLiteral("op"), QStringLiteral("put")}, {QStringLiteral("name"), name},
                      {QStringLiteral("db"), properties.id}, {QStringLiteral("stored"), 0},
                      {QStringLiteral("updated"), 0}, {QStringLiteral("known"), count},
                      {QStringLiteral("conflicts"), QJsonArray()}});
            m_link->touchDevice(m_phoneKey);
            return;
        }
        if (PhoneFiles::resolve(m_link->m_files.root(), name))
            name = PhoneFiles::target(m_link->listFiles(), properties.id, name, m_phoneName).name;
    }
    const std::optional<QString> path = PhoneFiles::resolve(m_link->m_files.root(), name);
    if (!path) {
        sendError(QStringLiteral("invalid database name: %1").arg(name));
        return;
    }
    QString error;
    const std::optional<QList<ImportedGame>> games =
        PhoneGames::parse(request.value(QStringLiteral("games")).toArray(), &error);
    if (!games) {
        sendError(error);
        return;
    }
    if (!m_link->m_gameStore) {
        sendError(QStringLiteral("this computer does not accept games"));
        return;
    }
    m_link->setActivity(PhoneLink::tr("Receiving games from %1…").arg(m_phoneName));
    const std::optional<PhoneGames::PutResult> result =
        m_link->m_gameStore->storeGames(*path, properties, m_phoneKey, m_phoneName, *games, &error);
    m_link->setActivity(QString());
    if (!result) {
        sendError(error.isEmpty() ? QStringLiteral("could not store the games") : error);
        return;
    }
    QJsonArray conflicts;
    for (const QString &uid : result->conflicts)
        conflicts.append(QJsonObject{{QStringLiteral("uid"), uid}});
    QJsonObject answer{{QStringLiteral("op"), QStringLiteral("put")},
                       {QStringLiteral("name"), name},
                       {QStringLiteral("stored"), result->stored},
                       {QStringLiteral("updated"), result->updated},
                       {QStringLiteral("known"), result->known},
                       {QStringLiteral("conflicts"), conflicts}};
    if (!properties.id.isEmpty())
        answer.insert(QStringLiteral("db"), properties.id);
    sendJson(answer);
    m_link->touchDevice(m_phoneKey);
    if (result->stored + result->updated > 0)
        Q_EMIT m_link->gamesStored(*path, result->stored + result->updated);
}

void PhoneLinkSession::sendJson(const QJsonObject &message)
{
    m_peer->sendText(QJsonDocument(message).toJson(QJsonDocument::Compact));
}

void PhoneLinkSession::sendError(const QString &message)
{
    sendJson({{QStringLiteral("op"), QStringLiteral("error")}, {QStringLiteral("message"), message}});
}

void PhoneLinkSession::startTransfer(const QString &name)
{
    // Only what "list" offers can be fetched.
    bool listed = false;
    for (const PhoneFiles::Entry &entry : m_link->listFiles())
        listed = listed || entry.name == name;
    const std::optional<QString> path = PhoneFiles::resolve(m_link->m_files.root(), name);
    if (!listed || !path) {
        sendError(QStringLiteral("no such database: %1").arg(name));
        return;
    }

    // A snapshot: the database may be written while it is sent.
    QFile source(*path);
    auto snapshot = std::make_unique<QTemporaryFile>(QDir::temp().filePath(QStringLiteral("pragma-phone-XXXXXX")));
    if (!source.open(QIODevice::ReadOnly) || !snapshot->open()) {
        sendError(QStringLiteral("cannot read %1").arg(name));
        return;
    }
    QCryptographicHash hash(QCryptographicHash::Sha256);
    while (!source.atEnd()) {
        const QByteArray chunk = source.read(1024 * 1024);
        hash.addData(chunk);
        if (snapshot->write(chunk) != chunk.size()) {
            sendError(QStringLiteral("cannot read %1").arg(name));
            return;
        }
    }
    snapshot->flush();
    snapshot->seek(0);

    sendJson({{QStringLiteral("op"), QStringLiteral("file")},
              {QStringLiteral("name"), name},
              {QStringLiteral("size"), snapshot->size()},
              {QStringLiteral("sha256"), QString::fromLatin1(hash.result().toHex())}});
    m_snapshot = std::move(snapshot);
    m_sendingName = name;
    m_sent = 0;
    m_link->setActivity(PhoneLink::tr("Sending %1 to %2…").arg(name, m_phoneName));
    pump();
}

void PhoneLinkSession::pump()
{
    if (!m_snapshot || m_finished)
        return;
    while (m_peer->bufferedAmount() < kHighBufferedAmount) {
        const QByteArray chunk = m_snapshot->read(kChunkSize);
        if (chunk.isEmpty()) {
            sendJson({{QStringLiteral("op"), QStringLiteral("done")}, {QStringLiteral("name"), m_sendingName}});
            m_snapshot.reset();
            m_link->setActivity(QString());
            m_link->touchDevice(m_phoneKey);
            processNext();
            return;
        }
        if (!m_peer->sendBinary(chunk)) {
            finish();
            return;
        }
        m_sent += chunk.size();
    }
    m_idleTimer->start(kIdleTimeoutMs);
}
