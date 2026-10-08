#include "LobbyNode.h"

#include "app/ChessPosition.h"
#include "app/phone/NostrEvent.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QSaveFile>

LobbyNode::LobbyNode(const NostrKey &key, const QString &ledgerPath, const QString &plansPath, QObject *parent)
    : LobbyService(parent)
    , m_key(key)
    , m_ledgerPath(ledgerPath)
    , m_plansPath(plansPath)
{
    loadLedger();
    loadPlans();
    rebuild();
}

namespace {

/// Whether `uci` is legal after `moves`: a move every peer would refuse is not signed.
bool isLegalAfter(const QStringList &moves, const QString &uci)
{
    ChessPosition position = ChessPosition::startingPosition();
    for (const QString &played : moves) {
        const std::optional<ChessMove> move = position.moveFromUci(played);
        if (!move)
            return false;
        position.play(*move);
    }
    return position.moveFromUci(uci).has_value();
}

} // namespace

QString LobbyNode::gameKey(const QString &roomId, const QString &white, const QString &black)
{
    return roomId + QLatin1Char('|') + white + QLatin1Char('|') + black;
}

void LobbyNode::loadLedger()
{
    QFile file(m_ledgerPath);
    if (!file.open(QIODevice::ReadOnly))
        return;
    while (!file.atEnd()) {
        const QByteArray line = file.readLine().trimmed();
        if (line.isEmpty())
            continue;
        const QJsonDocument document = QJsonDocument::fromJson(line);
        if (document.isObject())
            add(document.object(), false);
    }
}

void LobbyNode::loadPlans()
{
    QFile file(m_plansPath);
    if (!file.open(QIODevice::ReadOnly))
        return;
    const QJsonObject games = QJsonDocument::fromJson(file.readAll()).object();
    for (auto game = games.constBegin(); game != games.constEnd(); ++game) {
        LobbyPlan plan;
        const QJsonObject answers = game.value().toObject();
        for (auto answer = answers.constBegin(); answer != answers.constEnd(); ++answer)
            plan.insert(answer.key(), answer.value().toString());
        m_plans.insert(game.key(), plan);
    }
}

void LobbyNode::savePlans() const
{
    QJsonObject games;
    for (auto game = m_plans.cbegin(); game != m_plans.cend(); ++game) {
        QJsonObject answers;
        for (auto answer = game->cbegin(); answer != game->cend(); ++answer)
            answers.insert(answer.key(), answer.value());
        games.insert(game.key(), answers);
    }
    QDir().mkpath(QFileInfo(m_plansPath).absolutePath());
    QSaveFile file(m_plansPath);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(QJsonDocument(games).toJson(QJsonDocument::Compact));
        file.commit();
    }
}

bool LobbyNode::add(const QJsonObject &event, bool save)
{
    const std::optional<NostrEvent> signedEvent = NostrEvent::fromJson(event);
    if (!signedEvent || signedEvent->kind != LobbyLedger::kKind || m_ledger.contains(signedEvent->id)
        || !signedEvent->verify())
        return false;
    const std::optional<LedgerEvent> entry = LedgerEvent::fromSigned(signedEvent->toJson());
    if (!entry || !m_ledger.add(*entry))
        return false;
    if (save) {
        QDir().mkpath(QFileInfo(m_ledgerPath).absolutePath());
        QFile file(m_ledgerPath);
        if (file.open(QIODevice::Append))
            file.write(QJsonDocument(entry->signedEvent).toJson(QJsonDocument::Compact) + '\n');
    }
    return true;
}

bool LobbyNode::receive(const QJsonObject &event)
{
    if (!add(event, true))
        return false;
    Q_EMIT eventAdded(event);
    rebuild();
    runPlans();
    return true;
}

bool LobbyNode::publish(const QJsonObject &content, const QString &room)
{
    QList<QStringList> tags{{QStringLiteral("t"), QString::fromLatin1(LobbyLedger::kTopic)}};
    if (!room.isEmpty())
        tags << QStringList{QStringLiteral("r"), room};
    const NostrEvent event = NostrEvent::create(m_key, LobbyLedger::kKind, tags,
                                                QString::fromUtf8(QJsonDocument(content).toJson(QJsonDocument::Compact)));
    const QJsonObject json = event.toJson();
    if (!add(json, true))
        return false;
    Q_EMIT eventAdded(json);
    rebuild();
    return true;
}

void LobbyNode::rebuild()
{
    m_lobby = Lobby::withRooms(m_ledger.rooms(), m_lobby.offeredRooms());
    Q_EMIT changed();
}

QString LobbyNode::openRoom(quint32 seed)
{
    if (!m_lobby.mayJoin(me()))
        return QString(); // Every peer would refuse the room.
    const int before = m_ledger.size();
    if (!publish(LobbyLedger::openContent(seed, m_name), QString()) || m_ledger.size() == before)
        return QString();
    // The room is the event just made: the newest of the user's opens with that seed.
    QString id;
    for (const LobbyRoom &room : m_lobby.rooms()) {
        if (room.seed == seed && !room.seats.isEmpty() && room.seats.constFirst() == me())
            id = room.id;
    }
    return id;
}

bool LobbyNode::joinRoom(const QString &roomId)
{
    const int index = m_lobby.indexOfRoom(roomId);
    if (index < 0 || !m_lobby.rooms().at(index).isJoinable() || m_lobby.rooms().at(index).isSeated(me())
        || !m_lobby.mayJoin(me()))
        return false;
    return publish(LobbyLedger::joinContent(roomId, m_name), roomId);
}

bool LobbyNode::sendMove(const QString &roomId, const QString &white, const QString &black, const QString &uci)
{
    const int index = m_lobby.indexOfRoom(roomId);
    if (index < 0)
        return false;
    const LobbyRoom &room = m_lobby.rooms().at(index);
    const int game = room.indexOfGame(white, black);
    if (game < 0 || !room.games.at(game).waitsFor(me()) || !isLegalAfter(room.games.at(game).moves, uci))
        return false;
    const int ply = int(room.games.at(game).moves.size()) + 1;
    if (!publish(LobbyLedger::moveContent(roomId, white, black, ply, uci), roomId))
        return false;
    runPlans(); // A plan may answer what comes next, once the opponent has moved.
    return true;
}

void LobbyNode::setPlan(const QString &roomId, const QString &white, const QString &black, const LobbyPlan &plan)
{
    m_plans.insert(gameKey(roomId, white, black), plan);
    savePlans();
    runPlans();
    Q_EMIT changed();
}

LobbyPlan LobbyNode::plan(const QString &roomId, const QString &white, const QString &black) const
{
    return m_plans.value(gameKey(roomId, white, black));
}

bool LobbyNode::resign(const QString &roomId, const QString &white, const QString &black)
{
    const int index = m_lobby.indexOfRoom(roomId);
    if (index < 0)
        return false;
    const LobbyRoom &room = m_lobby.rooms().at(index);
    const int game = room.indexOfGame(white, black);
    if (game < 0 || room.games.at(game).isOver() || !room.games.at(game).involves(me()))
        return false;
    return publish(LobbyLedger::resignContent(roomId, white, black), roomId);
}

void LobbyNode::runPlans()
{
    if (m_runningPlans)
        return;
    m_runningPlans = true;
    // Each answer played may let the next one come only after the opponent
    // moves, so one pass finds them all; the loop is for a plan that answers
    // in several games at once.
    bool played = true;
    while (played) {
        played = false;
        for (const LobbyRoom &room : m_lobby.rooms()) {
            for (const LobbyGame &game : room.games) {
                if (!game.waitsFor(me()))
                    continue;
                const QString move = m_plans.value(gameKey(room.id, game.white, game.black))
                                         .value(LobbyGame::lineKey(game.moves));
                if (move.isEmpty() || !isLegalAfter(game.moves, move))
                    continue;
                const QString roomId = room.id;
                const QString white = game.white;
                const QString black = game.black;
                const int ply = int(game.moves.size()) + 1;
                if (publish(LobbyLedger::moveContent(roomId, white, black, ply, move), roomId)) {
                    Q_EMIT planPlayed(roomId, white, black, move);
                    played = true;
                }
                break; // m_lobby was rebuilt: look again from the start.
            }
            if (played)
                break;
        }
    }
    m_runningPlans = false;
}

void LobbyNode::setNetworkState(int relays, int peers)
{
    if (relays == m_relayCount && peers == m_peerCount)
        return;
    m_relayCount = relays;
    m_peerCount = peers;
    m_online = relays > 0 || peers > 0;
    Q_EMIT networkChanged();
}
