#pragma once

#include <QByteArray>
#include <QHash>
#include <QList>
#include <QString>
#include <QStringList>

#include <optional>

/// A player's plan in a game: conditional moves, the move to play (UCI) in
/// the position each line leads to (LobbyGame::lineKey: the UCI moves from
/// the start). A player may only answer for themselves: a plan never holds
/// the opponent's moves, only the lines they might choose.
using LobbyPlan = QHash<QString, QString>;

/// A game of a tournament room: its players and the moves played so far.
struct LobbyGame {
    QString white;
    QString black;
    /// The moves played, UCI.
    QStringList moves;
    /// PGN result; "*" while it goes on.
    QString result = QStringLiteral("*");

    bool isOver() const { return result != QLatin1String("*"); }
    /// Whose move it is, while the game goes on.
    QString toMove() const { return moves.size() % 2 == 0 ? white : black; }
    bool involves(const QString &player) const { return white == player || black == player; }
    QString opponentOf(const QString &player) const { return white == player ? black : white; }

    /// The plans the players sent, by player.
    QHash<QString, LobbyPlan> plans;

    /// The key of the position `moves` lead to, in a plan.
    static QString lineKey(const QStringList &moves) { return moves.join(QLatin1Char(' ')); }
    /// `player` plays `uci`, when it is their move; false otherwise.
    bool play(const QString &player, const QString &uci);
    /// Plays the answers the players prepared, from where the game stands,
    /// until the one to move has none ready. Returns how many were played.
    int advance();
    /// The game waits for `player`'s move.
    bool waitsFor(const QString &player) const { return !isOver() && toMove() == player; }
};

/// A player's line in a room's standings.
struct LobbyStanding {
    QString player;
    /// 1 for the first; players level on points and wins share a place.
    int place = 0;
    int played = 0;
    int wins = 0;
    int draws = 0;
    int losses = 0;
    /// Half points: a win is 2, a draw 1, so that 1½ is exact.
    int halfPoints = 0;
};

/// A tournament of the lobby (IDEA.md, "Tornei asincroni a 4 giocatori"):
/// four seats, a double round robin among whoever sits — two games, one with
/// each colour, for every pair —, so it starts with two players and grows as
/// the others come.
struct LobbyRoom {
    static constexpr int kSeats = 4;

    /// Names the room in every language (RoomName).
    quint32 seed = 0;
    /// The seats in order, an empty name a free one.
    QStringList seats = QStringList(kSeats, QString());
    QList<LobbyGame> games;

    /// The room's name in the interface language.
    QString name() const;
    int players() const;
    bool isJoinable() const { return players() < kSeats; }
    bool isSeated(const QString &player) const { return seats.contains(player); }
    /// The indexes of the games waiting for `player`'s move.
    QList<int> gamesWaitingFor(const QString &player) const;
    /// The seated players by points, then wins, then name; finished games only.
    QList<LobbyStanding> standings() const;
    /// Seats `player` at the first free seat and adds their games with each
    /// player already there, the newcomer's White game first. False when the
    /// room is full or they sit there already.
    bool seat(const QString &player);
};

/// The rooms of the lobby, for now the user experience only: the rooms are
/// examples and live in memory (`sample()`); the network comes later.
///
/// The lobby keeps at least kMinJoinableRooms rooms open to newcomers: when
/// fewer have a free seat, it offers new rooms, which exist only once
/// someone sits there (IDEA.md, "Lobby dinamica").
class Lobby {
public:
    static constexpr int kMinJoinableRooms = 2;

    /// Example rooms with games under way: two with free seats, two full.
    /// `me`, when given, sits in two of them, one with two games waiting for
    /// their move and one with a single game.
    static Lobby sample(const QString &me = QString());

    Lobby();

    const QList<LobbyRoom> &rooms() const { return m_rooms; }
    /// Room `index`, to play in it.
    LobbyRoom &room(int index) { return m_rooms[index]; }
    void addRoom(const LobbyRoom &room);
    /// The indexes of the rooms with a free seat.
    QList<int> joinableRooms() const;
    /// The new rooms the lobby offers besides them, by seed: each has its
    /// name already, and keeps it when someone sits there. Offers stay the
    /// same until taken or no longer needed.
    const QList<quint32> &offeredRooms() const { return m_offered; }
    int newRooms() const { return int(m_offered.size()); }
    /// The whole lobby as JSON, and back: until the network comes, the
    /// client keeps it between runs. fromJson gives nothing for text it
    /// cannot read.
    QByteArray toJson() const;
    static std::optional<Lobby> fromJson(const QByteArray &json);

    /// Sits `player` in room `index`; a negative index, -1 - k, opens
    /// offered room k for them. Returns the room's index, or -1 when they
    /// could not sit.
    int join(int index, const QString &player);

private:
    /// Offers as many new rooms as are missing (RoomName::newSeed, a name not in use).
    void offer();

    QList<LobbyRoom> m_rooms;
    QList<quint32> m_offered;
};
