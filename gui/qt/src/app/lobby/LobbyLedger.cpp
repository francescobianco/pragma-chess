#include "LobbyLedger.h"

#include "app/ChessPosition.h"

#include <QJsonArray>
#include <QJsonDocument>

#include <algorithm>
#include <limits>
#include <map>
#include <optional>
#include <tuple>

namespace {

QString text(const QJsonObject &object, const char *key)
{
    return object.value(QLatin1String(key)).toString();
}

bool isHex(const QString &value, int length)
{
    if (value.size() != length)
        return false;
    return std::all_of(value.cbegin(), value.cend(), [](QChar c) {
        return (c >= QLatin1Char('0') && c <= QLatin1Char('9')) || (c >= QLatin1Char('a') && c <= QLatin1Char('f'));
    });
}

bool earlier(const LedgerEvent &a, const LedgerEvent &b)
{
    return std::tie(a.createdAt, a.id) < std::tie(b.createdAt, b.id);
}

} // namespace

std::optional<LedgerEvent> LedgerEvent::fromSigned(const QJsonObject &event)
{
    if (event.value(QStringLiteral("kind")).toInt() != LobbyLedger::kKind)
        return std::nullopt;
    LedgerEvent entry;
    entry.id = text(event, "id");
    entry.author = text(event, "pubkey");
    entry.createdAt = qint64(event.value(QStringLiteral("created_at")).toDouble());
    if (!isHex(entry.id, 64) || !isHex(entry.author, 64))
        return std::nullopt;
    const QJsonDocument content = QJsonDocument::fromJson(text(event, "content").toUtf8());
    if (!content.isObject() || content.object().value(QStringLiteral("t")).toString().isEmpty())
        return std::nullopt;
    entry.content = content.object();
    entry.signedEvent = event;
    return entry;
}

bool LobbyLedger::add(const LedgerEvent &event)
{
    if (event.id.isEmpty() || m_events.contains(event.id))
        return false;
    m_events.insert(event.id, event);
    return true;
}

QList<LedgerEvent> LobbyLedger::events() const
{
    QList<LedgerEvent> events = m_events.values();
    std::sort(events.begin(), events.end(), earlier);
    return events;
}

QStringList LobbyLedger::ids() const
{
    QStringList ids = m_events.keys();
    ids.sort();
    return ids;
}

namespace {

/// Where an event stands in the order the rules read them.
using EventKey = std::pair<qint64, QString>;
using GameKey = std::tuple<QString, QString, QString>;

EventKey keyOf(const LedgerEvent &event)
{
    return {event.createdAt, event.id};
}

/// A game played out from its events: the moves by ply (the first legal one
/// of the player to move counting), then mate, stalemate or a resignation.
/// `end` is set to the latest of the events that made the game end, when it did.
LobbyGame playGame(const GameKey &key, QList<LedgerEvent> candidates, const QList<LedgerEvent> &resignations,
                   std::optional<EventKey> *end)
{
    LobbyGame game;
    game.white = std::get<1>(key);
    game.black = std::get<2>(key);
    EventKey last{std::numeric_limits<qint64>::min(), QString()};
    ChessPosition position = ChessPosition::startingPosition();
    std::stable_sort(candidates.begin(), candidates.end(), [](const LedgerEvent &a, const LedgerEvent &b) {
        return a.content.value(QStringLiteral("ply")).toInt() < b.content.value(QStringLiteral("ply")).toInt();
    });
    for (const LedgerEvent &event : std::as_const(candidates)) {
        const int ply = event.content.value(QStringLiteral("ply")).toInt();
        if (ply != game.moves.size() + 1 || event.author != game.toMove())
            continue; // Another ply, a ply already played, or not their turn.
        if (position.legalMoves().isEmpty())
            break; // Checkmate or stalemate: nothing comes after.
        const std::optional<ChessMove> move = position.moveFromUci(text(event.content, "uci"));
        if (!move)
            continue; // Illegal: left out, as every peer does.
        position.play(*move);
        game.moves << move->uci();
        last = std::max(last, keyOf(event));
    }
    if (position.legalMoves().isEmpty()) {
        if (position.inCheck())
            game.result = position.sideToMove() == Side::White ? QStringLiteral("0-1") : QStringLiteral("1-0");
        else
            game.result = QStringLiteral("1/2-1/2");
    } else {
        for (const LedgerEvent &event : resignations) {
            if (!game.involves(event.author))
                continue;
            game.result = event.author == game.white ? QStringLiteral("0-1") : QStringLiteral("1-0");
            last = std::max(last, keyOf(event));
            break;
        }
    }
    *end = game.isOver() ? std::optional<EventKey>(last) : std::nullopt;
    return game;
}

} // namespace

QList<LobbyRoom> LobbyLedger::rooms() const
{
    const QList<LedgerEvent> events = this->events();

    // The names players go by: the latest each gave.
    QHash<QString, QString> names;
    for (const LedgerEvent &event : events) {
        const QString name = text(event.content, "name").trimmed();
        if (!name.isEmpty())
            names.insert(event.author, name.left(40));
    }

    // The moves and resignations of each game, in the order of the events.
    std::map<GameKey, QList<LedgerEvent>> moves;
    std::map<GameKey, QList<LedgerEvent>> resignations;
    for (const LedgerEvent &event : events) {
        const QString kind = text(event.content, "t");
        const GameKey key{text(event.content, "room"), text(event.content, "white"), text(event.content, "black")};
        if (kind == QLatin1String("move"))
            moves[key] << event;
        else if (kind == QLatin1String("resign"))
            resignations[key] << event;
    }
    // When each game ended, played out once.
    std::map<GameKey, std::optional<EventKey>> ends;
    const auto endOf = [&](const GameKey &key) {
        auto found = ends.find(key);
        if (found == ends.end()) {
            std::optional<EventKey> end;
            playGame(key, moves[key], resignations[key], &end);
            found = ends.emplace(key, end).first;
        }
        return found->second;
    };

    // The rooms, then the seats in the order they were taken. A join may bear
    // an earlier time than its room's opening (the same second, or clocks
    // apart), and still names a room that exists: it is read right after it.
    QList<LobbyRoom> rooms;
    QHash<QString, int> roomIndex;
    for (const LedgerEvent &event : events) {
        if (text(event.content, "t") != QLatin1String("open"))
            continue;
        LobbyRoom room;
        room.id = event.id;
        room.seed = quint32(event.content.value(QStringLiteral("seed")).toDouble());
        roomIndex.insert(room.id, int(rooms.size()));
        rooms << room;
    }
    struct Sitting {
        EventKey at;
        bool open = false;
        LedgerEvent event;
        int room = -1;
    };
    QList<Sitting> sittings;
    QHash<QString, EventKey> openedAt;
    for (const LedgerEvent &event : events) {
        if (text(event.content, "t") == QLatin1String("open")) {
            openedAt.insert(event.id, keyOf(event));
            sittings << Sitting{keyOf(event), true, event, roomIndex.value(event.id)};
        }
    }
    for (const LedgerEvent &event : events) {
        if (text(event.content, "t") != QLatin1String("join"))
            continue;
        const QString room = text(event.content, "room");
        const auto found = roomIndex.constFind(room);
        if (found != roomIndex.cend())
            sittings << Sitting{std::max(keyOf(event), openedAt.value(room)), false, event, *found};
    }
    std::sort(sittings.begin(), sittings.end(), [](const Sitting &a, const Sitting &b) {
        return std::tie(a.at, b.open, a.event.id) < std::tie(b.at, a.open, b.event.id);
    });

    // A player plays in at most Lobby::kMaxRoomsInPlay tournaments at once: a
    // room counts until it is full and its last game ended before the seat
    // is taken. A room whose opening is refused is no room.
    QList<bool> opened(rooms.size(), false);
    const auto finishedBy = [&](const LobbyRoom &room, const EventKey &at) {
        if (room.isJoinable())
            return false;
        for (const LobbyGame &game : room.games) {
            const std::optional<EventKey> end = endOf(GameKey{room.id, game.white, game.black});
            if (!end || *end > at)
                return false;
        }
        return true;
    };
    for (const Sitting &sitting : std::as_const(sittings)) {
        if (!sitting.open && !opened.at(sitting.room))
            continue; // A room whose opening was refused.
        const QString &player = sitting.event.author;
        int inPlay = 0;
        for (int i = 0; i < rooms.size(); ++i) {
            if (opened.at(i) && rooms.at(i).isSeated(player) && !finishedBy(rooms.at(i), sitting.at))
                ++inPlay;
        }
        if (inPlay >= Lobby::kMaxRoomsInPlay)
            continue;
        if (rooms[sitting.room].seat(player) && sitting.open) // Refused when full or seated already.
            opened[sitting.room] = true;
    }

    QList<LobbyRoom> made;
    for (int i = 0; i < rooms.size(); ++i) {
        if (!opened.at(i))
            continue;
        LobbyRoom room = rooms.at(i);
        for (const QString &player : std::as_const(room.seats)) {
            if (!player.isEmpty() && names.contains(player))
                room.names.insert(player, names.value(player));
        }
        for (LobbyGame &game : room.games) {
            const GameKey key{room.id, game.white, game.black};
            std::optional<EventKey> end;
            game = playGame(key, moves[key], resignations[key], &end);
        }
        made << room;
    }
    return made;
}

QJsonObject LobbyLedger::openContent(quint32 seed, const QString &name)
{
    return {{QStringLiteral("v"), 1}, {QStringLiteral("t"), QStringLiteral("open")},
            {QStringLiteral("seed"), double(seed)}, {QStringLiteral("name"), name}};
}

QJsonObject LobbyLedger::joinContent(const QString &room, const QString &name)
{
    return {{QStringLiteral("v"), 1}, {QStringLiteral("t"), QStringLiteral("join")},
            {QStringLiteral("room"), room}, {QStringLiteral("name"), name}};
}

QJsonObject LobbyLedger::moveContent(const QString &room, const QString &white, const QString &black, int ply,
                                     const QString &uci)
{
    return {{QStringLiteral("v"), 1}, {QStringLiteral("t"), QStringLiteral("move")},
            {QStringLiteral("room"), room}, {QStringLiteral("white"), white}, {QStringLiteral("black"), black},
            {QStringLiteral("ply"), ply}, {QStringLiteral("uci"), uci}};
}

QJsonObject LobbyLedger::resignContent(const QString &room, const QString &white, const QString &black)
{
    return {{QStringLiteral("v"), 1}, {QStringLiteral("t"), QStringLiteral("resign")},
            {QStringLiteral("room"), room}, {QStringLiteral("white"), white}, {QStringLiteral("black"), black}};
}
