#include "PositionIndex.h"

#include "PolyglotBook.h"

#include <QThread>

#include <algorithm>
#include <thread>

namespace {

/// SplitMix64 finalizer: spreads every bit of the input over the output.
quint64 mix(quint64 value)
{
    value += 0x9e3779b97f4a7c15ULL;
    value = (value ^ (value >> 30)) * 0xbf58476d1ce4e5b9ULL;
    value = (value ^ (value >> 27)) * 0x94d049bb133111ebULL;
    return value ^ (value >> 31);
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

PositionIndex PositionIndex::build(const QList<GameLine> &games, const std::atomic_bool *cancelled, int threads)
{
    if (threads <= 0)
        threads = std::max(1, QThread::idealThreadCount() - 1);
    threads = int(std::min<qsizetype>(threads, std::max<qsizetype>(1, games.size() / 1000)));

    // Each thread indexes a share of the games and sorts what it found.
    struct Share {
        Entries positions;
        Entries lines;
    };
    std::vector<Share> shares(threads);
    const auto work = [&](int share) {
        Share &out = shares[share];
        const qsizetype first = games.size() * share / threads;
        const qsizetype last = games.size() * (share + 1) / threads;
        for (qsizetype i = first; i < last; ++i) {
            if (cancelled && cancelled->load(std::memory_order_relaxed))
                return;
            const GameLine &game = games.at(i);
            quint32 result = 0;
            if (game.result == QLatin1String("1-0"))
                result = 1;
            else if (game.result == QLatin1String("1/2-1/2"))
                result = 2;
            else if (game.result == QLatin1String("0-1"))
                result = 3;
            if (game.id < 0 || game.id >= (qint64(1) << 30))
                continue; // Out of what an entry holds: a database of a billion games.
            const quint32 value = quint32(game.id) << 2 | result;
            std::optional<ChessPosition> start;
            if (!game.startFen.isEmpty())
                start = ChessPosition::fromFen(game.startFen, ChessPosition::Kings::Optional);
            ChessPosition position = start.value_or(ChessPosition::startingPosition());

            quint64 line = mix(PolyglotBook::key(position));
            out.positions.push_back({PolyglotBook::key(position), value});
            out.lines.push_back({line, value});
            for (const QStringView uci : QStringView(game.movesUci).split(QLatin1Char(' '), Qt::SkipEmptyParts)) {
                const std::optional<ChessMove> move = position.moveFromUci(uci, ChessPosition::NullMoves::Allowed);
                if (!move)
                    break;
                position.play(*move);
                line = extendLine(line, *move);
                out.positions.push_back({PolyglotBook::key(position), value});
                out.lines.push_back({line, value});
            }
        }
        std::sort(out.positions.begin(), out.positions.end());
        std::sort(out.lines.begin(), out.lines.end());
    };
    std::vector<std::thread> workers;
    for (int share = 1; share < threads; ++share)
        workers.emplace_back(work, share);
    work(0);
    for (std::thread &worker : workers)
        worker.join();

    // The sorted shares merged two by two, on threads too, then made unique.
    const auto merged = [&](Entries Share::*member) {
        std::vector<Entries> runs;
        for (Share &share : shares)
            runs.push_back(std::move(share.*member));
        while (runs.size() > 1) {
            std::vector<Entries> next((runs.size() + 1) / 2);
            std::vector<std::thread> mergers;
            for (size_t i = 0; i + 1 < runs.size(); i += 2) {
                mergers.emplace_back([&runs, &next, i] {
                    Entries &out = next[i / 2];
                    out.resize(runs[i].size() + runs[i + 1].size());
                    std::merge(runs[i].begin(), runs[i].end(), runs[i + 1].begin(), runs[i + 1].end(), out.begin());
                    Entries().swap(runs[i]);
                    Entries().swap(runs[i + 1]);
                });
            }
            if (runs.size() % 2)
                next.back() = std::move(runs.back());
            for (std::thread &merger : mergers)
                merger.join();
            runs = std::move(next);
        }
        Entries entries = runs.empty() ? Entries() : std::move(runs.front());
        entries.erase(std::unique(entries.begin(), entries.end()), entries.end());
        entries.shrink_to_fit();
        return entries;
    };
    PositionIndex index;
    index.m_gameCount = int(games.size());
    index.m_positions = merged(&Share::positions);
    index.m_lines = merged(&Share::lines);
    return index;
}

std::pair<PositionIndex::Entries::const_iterator, PositionIndex::Entries::const_iterator>
PositionIndex::range(const Entries &entries, quint64 key)
{
    const auto first = std::lower_bound(entries.begin(), entries.end(), key,
                                        [](const Entry &entry, quint64 value) { return entry.key < value; });
    const auto last = std::upper_bound(first, entries.end(), key,
                                       [](quint64 value, const Entry &entry) { return value < entry.key; });
    return {first, last};
}

QSet<qint64> PositionIndex::idsOf(const Entries &entries, quint64 key)
{
    const auto [first, last] = range(entries, key);
    QSet<qint64> ids;
    ids.reserve(last - first);
    for (auto it = first; it != last; ++it)
        ids.insert(it->game >> 2);
    return ids;
}

PositionIndex::Stats PositionIndex::statsWithPosition(const ChessPosition &position) const
{
    const auto [first, last] = range(m_positions, PolyglotBook::key(position));
    Stats stats;
    for (auto it = first; it != last; ++it) {
        ++stats.games;
        switch (it->game & 3) {
        case 1: ++stats.whiteWins; break;
        case 2: ++stats.draws; break;
        case 3: ++stats.blackWins; break;
        default: break;
        }
    }
    return stats;
}

int PositionIndex::countOf(const Entries &entries, quint64 key)
{
    const auto [first, last] = range(entries, key);
    return int(last - first);
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
