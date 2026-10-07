#include "Lobby.h"
#include "RoomName.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <algorithm>

QString LobbyRoom::name() const
{
    return RoomName::text(seed);
}

int LobbyRoom::players() const
{
    return int(std::count_if(seats.cbegin(), seats.cend(), [](const QString &seat) { return !seat.isEmpty(); }));
}

bool LobbyGame::play(const QString &player, const QString &uci)
{
    if (isOver() || toMove() != player || uci.isEmpty())
        return false;
    moves << uci;
    return true;
}

int LobbyGame::advance()
{
    int played = 0;
    // A plan answers each position once, so a cascade ends; the bound only guards a broken plan.
    while (!isOver() && played < 1000) {
        const QString move = plans.value(toMove()).value(lineKey(moves));
        if (move.isEmpty())
            break;
        moves << move;
        ++played;
    }
    return played;
}

QList<int> LobbyRoom::gamesWaitingFor(const QString &player) const
{
    QList<int> waiting;
    for (int i = 0; i < games.size(); ++i) {
        if (games.at(i).waitsFor(player))
            waiting << i;
    }
    return waiting;
}

QList<LobbyStanding> LobbyRoom::standings() const
{
    QList<LobbyStanding> table;
    for (const QString &player : seats) {
        if (player.isEmpty())
            continue;
        LobbyStanding line;
        line.player = player;
        for (const LobbyGame &game : games) {
            if (!game.isOver() || !game.involves(player))
                continue;
            ++line.played;
            const bool white = game.white == player;
            if (game.result == QLatin1String("1/2-1/2"))
                ++line.draws;
            else if ((game.result == QLatin1String("1-0")) == white)
                ++line.wins;
            else
                ++line.losses;
        }
        line.halfPoints = 2 * line.wins + line.draws;
        table << line;
    }
    std::sort(table.begin(), table.end(), [](const LobbyStanding &a, const LobbyStanding &b) {
        if (a.halfPoints != b.halfPoints)
            return a.halfPoints > b.halfPoints;
        if (a.wins != b.wins)
            return a.wins > b.wins;
        return a.player.localeAwareCompare(b.player) < 0;
    });
    for (int i = 0; i < table.size(); ++i) {
        const bool level = i > 0 && table.at(i).halfPoints == table.at(i - 1).halfPoints
            && table.at(i).wins == table.at(i - 1).wins;
        table[i].place = level ? table.at(i - 1).place : i + 1;
    }
    return table;
}

bool LobbyRoom::seat(const QString &player)
{
    if (player.isEmpty() || isSeated(player))
        return false;
    const qsizetype free = seats.indexOf(QString());
    if (free < 0)
        return false;
    for (const QString &other : std::as_const(seats)) {
        if (other.isEmpty())
            continue;
        LobbyGame game;
        game.white = player;
        game.black = other;
        games << game;
        std::swap(game.white, game.black);
        games << game;
    }
    seats[free] = player;
    return true;
}

Lobby Lobby::sample(const QString &me)
{
    const auto moves = [](const char *line) { return QString::fromLatin1(line).split(QLatin1Char(' ')); };
    const auto room = [](quint32 seed, std::initializer_list<const char *> players) {
        LobbyRoom room;
        room.seed = seed;
        for (const char *player : players)
            room.seat(QString::fromUtf8(player));
        return room;
    };
    // A seed is a term and a champion: term + RoomName::termCount() × champion.
    const auto seed = [](int term, int champion) { return quint32(term + RoomName::termCount() * champion); };
    Lobby lobby;

    // Capablanca's Fortress, 3 / 4.
    LobbyRoom a = room(seed(1, 2), {"Alice", "Bruno", "Carla"});
    // Bruno–Alice, Alice–Bruno, Carla–Alice, Alice–Carla, Carla–Bruno, Bruno–Carla.
    a.games[0].moves = moves("e2e4 c7c5 g1f3 d7d6 d2d4 c5d4 f3d4 g8f6 b1c3 a7a6 c1e3 e7e5");
    a.games[1].moves = moves("d2d4 d7d5 c2c4 e7e6 b1c3 g8f6 c1g5 f8e7 e2e3 e8g8 g1f3");
    a.games[2].moves = moves("e2e4 e7e5 g1f3 b8c6 f1b5 a7a6 b5a4 g8f6 e1g1 f8e7 f1e1 b7b5 a4b3 d7d6 c2c3 e8g8");
    a.games[3].moves = moves("c2c4 e7e5 b1c3 g8f6 g2g3 d7d5 c4d5 f6d5 f1g2 d5b6");
    a.games[4].moves = moves("f2f4 e7e5 f4e5 d7d6 e5d6 f8d6 g1f3 g7g5 d2d4 g5g4 f3e5 d6e5 d4e5 d8d1");
    a.games[4].result = QStringLiteral("0-1");
    a.games[5].moves = moves("e2e4 e7e6 d2d4 d7d5");
    a.games[3].result = QStringLiteral("1-0");
    a.games[1].moves << moves("h7h6 g5h4 b7b6 c4d5 f6d5");
    a.games[1].result = QStringLiteral("1/2-1/2");
    lobby.m_rooms << a;

    // Fischer's Zugzwang, 2 / 4.
    LobbyRoom b = room(seed(2, 10), {"Dario", "Elena"});
    b.games[0].moves = moves("d2d4 g8f6 c2c4 g7g6 b1c3 f8g7 e2e4 d7d6 g1f3 e8g8 f1e2 e7e5");
    b.games[1].moves = moves("e2e4 c7c6 d2d4 d7d5 e4e5 c8f5");
    if (!me.isEmpty()) {
        // Me–Dario and Dario–Me wait for the user's move, Me–Elena and Elena–Me for Elena's.
        b.seat(me);
        b.games[2].moves = moves("e2e4 e7e5 g1f3 b8c6");
        b.games[3].moves = moves("d2d4 g8f6 c2c4");
        b.games[4].moves = moves("e2e4 c7c6 d2d4");
    }
    lobby.m_rooms << b;

    // Tal's Sacrifice, full.
    LobbyRoom c = room(seed(13, 7), {"Franco", "Giulia", "Hugo", "Irene"});
    c.games[0].moves = moves("e2e4 e7e5 f1c4 b8c6 d1h5 g8f6 h5f7");
    c.games[0].result = QStringLiteral("1-0");
    c.games[1].moves = moves("d2d4 d7d5 c2c4 c7c6");
    c.games[2].moves = moves("e2e4 c7c5 b1c3 b8c6 g2g3 g7g6 f1g2 f8g7");
    c.games[3].moves = moves("d2d4 g8f6 c2c4 e7e6 g1f3 b7b6 g2g3 c8a6");
    c.games[6].moves = moves("e2e4 e7e5 g1f3 b8c6 f1c4 f8c5 c2c3 g8f6 d2d4 e5d4 c3d4 c5b4");
    c.games[9].moves = moves("c2c4 c7c5 g1f3 g8f6 b1c3 d7d5 c4d5 f6d5 d2d4");
    c.games[1].result = QStringLiteral("1/2-1/2");
    c.games[2].result = QStringLiteral("0-1");
    c.games[6].result = QStringLiteral("1-0");
    lobby.m_rooms << c;

    // Morphy's Combination, full.
    // The user, when given, sits in Oscar's seat, with one game waiting for their move.
    LobbyRoom d = room(seed(12, 27), {"Luca", "Marta", "Nadia"});
    d.seat(me.isEmpty() ? QStringLiteral("Oscar") : me);
    d.games[0].moves = moves("e2e4 e7e5 g1f3 d7d6 d2d4 c8g4 d4e5 g4f3 d1f3 d6e5 f1c4 g8f6 f3b3 d8e7 b1c3 c7c6 c1g5 b7b5 c3b5 c6b5 c4b5 b8d7 e1c1 a8d8 d1d7 d8d7 h1d1 e7e6 b5d7 f6d7 b3b8 d7b8 d1d8");
    d.games[0].result = QStringLiteral("1-0");
    d.games[1].moves = moves("e2e4 e7e5 g1f3 b8c6 d2d4 e5d4 f3d4");
    d.games[4].moves = moves("d2d4 f7f5 g2g3 g8f6 f1g2 g7g6");
    d.games[7].moves = moves("e2e4 d7d5 e4d5 d8d5 b1c3 d5a5");
    d.games[10].moves = moves("g1f3 d7d5 g2g3 g8f6 f1g2 e7e6 e1g1 f8e7");
    d.games[6].moves = moves("c2c4 e7e5 b1c3");
    d.games[8].moves = moves("e2e4 c7c5 g1f3");
    // Nadia prepared her answers to the user's next move: whichever of these
    // they play, she castles at once.
    const QString afterBe7 = QStringLiteral("g1f3 d7d5 g2g3 g8f6 f1g2 e7e6 e1g1 f8e7 ");
    for (const char *move : {"d2d3", "c2c4", "d2d4", "b2b3"})
        d.games[10].plans[QStringLiteral("Nadia")].insert(afterBe7 + QLatin1String(move), QStringLiteral("e8g8"));
    d.games[4].result = QStringLiteral("1/2-1/2");
    d.games[7].result = QStringLiteral("1-0");
    lobby.m_rooms << d;
    lobby.offer();
    return lobby;
}

QList<int> Lobby::joinableRooms() const
{
    QList<int> joinable;
    for (int i = 0; i < m_rooms.size(); ++i) {
        if (m_rooms.at(i).isJoinable())
            joinable << i;
    }
    return joinable;
}

Lobby::Lobby()
{
    offer();
}

void Lobby::addRoom(const LobbyRoom &room)
{
    m_rooms << room;
    offer();
}

void Lobby::offer()
{
    const int missing = std::max(0, kMinJoinableRooms - int(joinableRooms().size()));
    while (m_offered.size() > missing)
        m_offered.removeLast();
    QSet<quint32> taken(m_offered.cbegin(), m_offered.cend());
    for (const LobbyRoom &room : std::as_const(m_rooms))
        taken.insert(room.seed);
    while (m_offered.size() < missing) {
        const quint32 seed = RoomName::newSeed(taken);
        taken.insert(seed);
        m_offered << seed;
    }
}

int Lobby::join(int index, const QString &player)
{
    if (index < 0) {
        const qsizetype offered = -1 - qsizetype(index);
        if (offered >= m_offered.size())
            return -1;
        LobbyRoom room;
        room.seed = m_offered.at(offered);
        if (!room.seat(player))
            return -1;
        m_offered.removeAt(offered);
        m_rooms << room;
        offer();
        return int(m_rooms.size()) - 1;
    }
    if (index >= m_rooms.size() || !m_rooms[index].seat(player))
        return -1;
    offer();
    return index;
}

QByteArray Lobby::toJson() const
{
    QJsonArray rooms;
    for (const LobbyRoom &room : m_rooms) {
        QJsonArray games;
        for (const LobbyGame &game : room.games) {
            QJsonObject plans;
            for (auto plan = game.plans.cbegin(); plan != game.plans.cend(); ++plan) {
                QJsonObject answers;
                for (auto answer = plan->cbegin(); answer != plan->cend(); ++answer)
                    answers.insert(answer.key(), answer.value());
                plans.insert(plan.key(), answers);
            }
            games.append(QJsonObject{{QStringLiteral("white"), game.white},
                                     {QStringLiteral("black"), game.black},
                                     {QStringLiteral("moves"), LobbyGame::lineKey(game.moves)},
                                     {QStringLiteral("result"), game.result},
                                     {QStringLiteral("plans"), plans}});
        }
        rooms.append(QJsonObject{{QStringLiteral("seed"), double(room.seed)},
                                 {QStringLiteral("seats"), QJsonArray::fromStringList(room.seats)},
                                 {QStringLiteral("games"), games}});
    }
    QJsonArray offered;
    for (quint32 seed : m_offered)
        offered.append(double(seed));
    return QJsonDocument(QJsonObject{{QStringLiteral("rooms"), rooms}, {QStringLiteral("offered"), offered}})
        .toJson(QJsonDocument::Compact);
}

std::optional<Lobby> Lobby::fromJson(const QByteArray &json)
{
    const QJsonDocument document = QJsonDocument::fromJson(json);
    if (!document.isObject() || !document.object().value(QStringLiteral("rooms")).isArray())
        return std::nullopt;
    Lobby lobby;
    lobby.m_offered.clear();
    for (const QJsonValue &value : document.object().value(QStringLiteral("rooms")).toArray()) {
        const QJsonObject object = value.toObject();
        LobbyRoom room;
        room.seed = quint32(object.value(QStringLiteral("seed")).toDouble());
        QStringList seats;
        for (const QJsonValue &seat : object.value(QStringLiteral("seats")).toArray())
            seats << seat.toString();
        if (seats.size() != LobbyRoom::kSeats)
            return std::nullopt;
        room.seats = seats;
        for (const QJsonValue &entry : object.value(QStringLiteral("games")).toArray()) {
            const QJsonObject gameObject = entry.toObject();
            LobbyGame game;
            game.white = gameObject.value(QStringLiteral("white")).toString();
            game.black = gameObject.value(QStringLiteral("black")).toString();
            game.moves = gameObject.value(QStringLiteral("moves")).toString().split(QLatin1Char(' '), Qt::SkipEmptyParts);
            game.result = gameObject.value(QStringLiteral("result")).toString(QStringLiteral("*"));
            const QJsonObject plans = gameObject.value(QStringLiteral("plans")).toObject();
            for (auto plan = plans.constBegin(); plan != plans.constEnd(); ++plan) {
                const QJsonObject answers = plan.value().toObject();
                for (auto answer = answers.constBegin(); answer != answers.constEnd(); ++answer)
                    game.plans[plan.key()].insert(answer.key(), answer.value().toString());
            }
            room.games << game;
        }
        lobby.m_rooms << room;
    }
    for (const QJsonValue &seed : document.object().value(QStringLiteral("offered")).toArray())
        lobby.m_offered << quint32(seed.toDouble());
    lobby.offer(); // As many offers as the rooms now need.
    return lobby;
}
