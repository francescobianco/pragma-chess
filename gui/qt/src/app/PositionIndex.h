#pragma once

#include "ChessPosition.h"
#include "GameRecord.h"

#include <QHash>
#include <QList>
#include <QSet>

#include <atomic>
#include <utility>
#include <vector>

/// Which games of a database reach a position, and which begin with a line of
/// moves: the "Position" and "Variant" filters of the database tree.
///
/// Positions are identified by their Polyglot key (pieces, side to move,
/// castling, en passant), so a position reached by another move order is the
/// same position. Lines are identified by their start position and the exact
/// moves played from it. Pure and immutable once built, so it can be built on
/// another thread and read from the GUI.
class PositionIndex {
public:
    /// Indexes the games; moves are replayed from each start position and a
    /// game is cut at its first illegal move (as GameSession does). The games
    /// are shared among `threads` threads (0: every core but one). Stops
    /// early, with a partial index, once `cancelled` (if given) turns true.
    static PositionIndex build(const QList<GameLine> &games, const std::atomic_bool *cancelled = nullptr,
                               int threads = 0);

    int gameCount() const { return m_gameCount; }
    /// Positions and lines held, a pair each: what the index weighs.
    qsizetype entryCount() const { return qsizetype(m_positions.size() + m_lines.size()); }

    /// How the games reaching a position ended (the Database column of the Opening Tree).
    struct Stats {
        int games = 0;
        int whiteWins = 0;
        int draws = 0;
        int blackWins = 0;

        bool operator==(const Stats &) const = default;
    };
    Stats statsWithPosition(const ChessPosition &position) const;

    /// Ids of the games in which `position` occurs, at any ply.
    QSet<qint64> gamesWithPosition(const ChessPosition &position) const;
    int countWithPosition(const ChessPosition &position) const;

    /// Ids of the games starting from `start` whose moves begin with `moves`.
    QSet<qint64> gamesWithLine(const ChessPosition &start, const QList<ChessMove> &moves) const;
    int countWithLine(const ChessPosition &start, const QList<ChessMove> &moves) const;

    /// Identity of a line: its start position and the moves played from it.
    static quint64 lineKey(const ChessPosition &start, const QList<ChessMove> &moves);

private:
#pragma pack(push, 4)
    /// A key and the game it is found in: 12 bytes, the index's whole weight.
    struct Entry {
        quint64 key;
        /// The game's id times four, plus its result: 1 White won, 2 draw,
        /// 3 Black won, 0 none.
        quint32 game;
        bool operator<(const Entry &other) const { return key < other.key || (key == other.key && game < other.game); }
        bool operator==(const Entry &) const = default;
    };
#pragma pack(pop)
    using Entries = std::vector<Entry>;

    static std::pair<Entries::const_iterator, Entries::const_iterator> range(const Entries &entries, quint64 key);
    static QSet<qint64> idsOf(const Entries &entries, quint64 key);
    static int countOf(const Entries &entries, quint64 key);
    static quint64 extendLine(quint64 line, const ChessMove &move);

    /// (position key, game) and (line key, game), sorted and unique.
    Entries m_positions;
    Entries m_lines;
    int m_gameCount = 0;
};
