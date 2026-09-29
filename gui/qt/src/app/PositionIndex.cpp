#include "PositionIndex.h"

#include "PolyglotBook.h"

#include <algorithm>

namespace {

/// SplitMix64 finalizer: spreads every bit of the input over the output.
quint64 mix(quint64 value)
{
    value += 0x9e3779b97f4a7c15ULL;
    value = (value ^ (value >> 30)) * 0xbf58476d1ce4e5b9ULL;
    value = (value ^ (value >> 27)) * 0x94d049bb133111ebULL;
    return value ^ (value >> 31);
}

void sortUnique(std::vector<std::pair<quint64, qint64>> &entries)
{
    std::sort(entries.begin(), entries.end());
    entries.erase(std::unique(entries.begin(), entries.end()), entries.end());
    entries.shrink_to_fit();
}

} // namespace

quint64 PositionIndex::extendLine(quint64 line, const ChessMove &move)
{
    const quint64 code = quint64(move.from) | quint64(move.to) << 6 | quint64(move.promotion) << 12;
    return mix(line ^ mix(code + 1));
}

quint64 PositionIndex::lineKey(const ChessPosition &start, const QList<ChessMove> &moves)
{
    quint64 line = mix(PolyglotBook::key(start));
    for (const ChessMove &move : moves)
        line = extendLine(line, move);
    return line;
}

PositionIndex PositionIndex::build(const QList<GameLine> &games, const std::atomic_bool *cancelled)
{
    PositionIndex index;
    index.m_gameCount = int(games.size());
    for (const GameLine &game : games) {
        if (cancelled && cancelled->load(std::memory_order_relaxed))
            break;
        std::optional<ChessPosition> start;
        if (!game.startFen.isEmpty())
            start = ChessPosition::fromFen(game.startFen);
        ChessPosition position = start.value_or(ChessPosition::startingPosition());

        quint64 line = mix(PolyglotBook::key(position));
        index.m_positions.emplace_back(PolyglotBook::key(position), game.id);
        index.m_lines.emplace_back(line, game.id);
        for (const QStringView uci : QStringView(game.movesUci).split(QLatin1Char(' '), Qt::SkipEmptyParts)) {
            const std::optional<ChessMove> move = position.moveFromUci(uci);
            if (!move)
                break;
            position.play(*move);
            line = extendLine(line, *move);
            index.m_positions.emplace_back(PolyglotBook::key(position), game.id);
            index.m_lines.emplace_back(line, game.id);
        }
    }
    sortUnique(index.m_positions);
    sortUnique(index.m_lines);
    return index;
}

QSet<qint64> PositionIndex::idsOf(const Entries &entries, quint64 key)
{
    const auto first = std::lower_bound(entries.begin(), entries.end(), std::pair<quint64, qint64>(key, 0),
                                        [](const auto &a, const auto &b) { return a.first < b.first; });
    QSet<qint64> ids;
    for (auto it = first; it != entries.end() && it->first == key; ++it)
        ids.insert(it->second);
    return ids;
}

int PositionIndex::countOf(const Entries &entries, quint64 key)
{
    const auto byKey = [](const auto &a, const auto &b) { return a.first < b.first; };
    const auto range = std::equal_range(entries.begin(), entries.end(), std::pair<quint64, qint64>(key, 0), byKey);
    return int(range.second - range.first);
}

QSet<qint64> PositionIndex::gamesWithPosition(const ChessPosition &position) const
{
    return idsOf(m_positions, PolyglotBook::key(position));
}

int PositionIndex::countWithPosition(const ChessPosition &position) const
{
    return countOf(m_positions, PolyglotBook::key(position));
}

QSet<qint64> PositionIndex::gamesWithLine(const ChessPosition &start, const QList<ChessMove> &moves) const
{
    return idsOf(m_lines, lineKey(start, moves));
}

int PositionIndex::countWithLine(const ChessPosition &start, const QList<ChessMove> &moves) const
{
    return countOf(m_lines, lineKey(start, moves));
}
