#include "PositionIndex.h"

#include "PolyglotBook.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
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
    struct Built {
        Entries positions;
        Entries lines;
    };
    auto built = std::make_shared<Built>(Built{merged(&Share::positions), merged(&Share::lines)});
    PositionIndex index;
    index.m_gameCount = int(games.size());
    index.m_positions = {built->positions.data(), built->positions.size()};
    index.m_lines = {built->lines.data(), built->lines.size()};
    index.m_storage = built;
    return index;
}

namespace {

/// The file's head: what it is, what it was made from, how much it holds.
struct FileHead {
    char magic[8] = {'P', 'R', 'A', 'G', 'P', 'I', 'X', '1'};
    quint32 entrySize = sizeof(quint64) + sizeof(quint32);
    quint32 stampSize = 0;
    char stamp[48] = {};
    qint64 gameCount = 0;
    qint64 positions = 0;
    qint64 lines = 0;
};

} // namespace

bool PositionIndex::save(const QString &path, const QByteArray &stamp) const
{
    FileHead head;
    if (stamp.size() > qsizetype(sizeof(head.stamp)))
        return false;
    head.stampSize = quint32(stamp.size());
    std::copy(stamp.begin(), stamp.end(), head.stamp);
    head.gameCount = m_gameCount;
    head.positions = qint64(m_positions.size);
    head.lines = qint64(m_lines.size);
    QDir().mkpath(QFileInfo(path).absolutePath());
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return false;
    const auto write = [&file](const void *data, qint64 size) {
        // In pieces: QIODevice writes at most 2 GB at once on some systems.
        const char *bytes = static_cast<const char *>(data);
        for (qint64 done = 0; done < size;) {
            const qint64 written = file.write(bytes + done, qMin<qint64>(size - done, qint64(1) << 28));
            if (written <= 0)
                return false;
            done += written;
        }
        return true;
    };
    return write(&head, sizeof head) && write(m_positions.data, qint64(m_positions.size * sizeof(Entry)))
        && write(m_lines.data, qint64(m_lines.size * sizeof(Entry))) && file.commit();
}

std::optional<PositionIndex> PositionIndex::load(const QString &path, const QByteArray &stamp)
{
    auto file = std::make_shared<QFile>(path);
    if (!file->open(QIODevice::ReadOnly) || file->size() < qint64(sizeof(FileHead)))
        return std::nullopt;
    FileHead head;
    const FileHead expected;
    if (file->read(reinterpret_cast<char *>(&head), sizeof head) != qint64(sizeof head)
        || !std::equal(std::begin(head.magic), std::end(head.magic), std::begin(expected.magic))
        || head.entrySize != sizeof(Entry) || QByteArray(head.stamp, head.stampSize) != stamp
        || head.positions < 0 || head.lines < 0
        || file->size() != qint64(sizeof head) + (head.positions + head.lines) * qint64(sizeof(Entry)))
        return std::nullopt;
    uchar *mapped = file->map(0, file->size());
    if (!mapped)
        return std::nullopt;
    const auto *entries = reinterpret_cast<const Entry *>(mapped + sizeof head);
    PositionIndex index;
    index.m_gameCount = int(head.gameCount);
    index.m_positions = {entries, size_t(head.positions)};
    index.m_lines = {entries + head.positions, size_t(head.lines)};
    index.m_storage = file; // Unmapped when the last copy of the index goes.
    index.m_mapped = true;
    return index;
}

std::pair<const PositionIndex::Entry *, const PositionIndex::Entry *> PositionIndex::range(const Table &entries,
                                                                                       quint64 key)
{
    const auto first = std::lower_bound(entries.begin(), entries.end(), key,
                                        [](const Entry &entry, quint64 value) { return entry.key < value; });
    const auto last = std::upper_bound(first, entries.end(), key,
                                       [](quint64 value, const Entry &entry) { return value < entry.key; });
    return {first, last};
}

QSet<qint64> PositionIndex::idsOf(const Table &entries, quint64 key)
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

int PositionIndex::countOf(const Table &entries, quint64 key)
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
