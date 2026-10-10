#include "PgnConversion.h"

#include "PgnSplitter.h"
#include "app/SqliteGameDatabase.h"
#include "app/sources/PgnFile.h"

#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QThread>

#include <algorithm>
#include <optional>
#include <thread>
#include <vector>

namespace PgnConversion {

namespace {

struct Text {
    Q_DECLARE_TR_FUNCTIONS(PgnConversion)
};

/// Read from the file at a time, and games read and written together.
constexpr qint64 kChunkBytes = 4 << 20;
constexpr qsizetype kBatchGames = 4000;

/// Reads the games of `entries` on `threads` threads, in order.
std::vector<std::optional<GameRecord>> readGames(const QList<QByteArray> &entries, int threads)
{
    std::vector<std::optional<GameRecord>> games(entries.size());
    std::atomic<qsizetype> next{0};
    const auto work = [&] {
        for (qsizetype i = next++; i < entries.size(); i = next++) {
            QString ignored;
            games[i] = PgnFile::read(entries.at(i), &ignored);
        }
    };
    std::vector<std::thread> workers;
    for (int i = 1; i < threads; ++i)
        workers.emplace_back(work);
    work();
    for (std::thread &worker : workers)
        worker.join();
    return games;
}

} // namespace

Result run(const QString &pgnPath, const QString &pdbPath, const std::function<void(const Progress &)> &progress,
           const std::atomic_bool *cancel)
{
    Result result;
    QElapsedTimer clock;
    clock.start();
    QFile pgn(pgnPath);
    if (!pgn.open(QIODevice::ReadOnly)) {
        result.error = Text::tr("Could not read “%1”: %2").arg(QFileInfo(pgnPath).fileName(), pgn.errorString());
        return result;
    }
    if (QFileInfo::exists(pdbPath)) {
        result.error = Text::tr("A file named “%1” already exists.").arg(QFileInfo(pdbPath).fileName());
        return result;
    }
    const QFileInfo target(pdbPath);
    // Hidden, so the folder sync leaves it alone until it is complete.
    const QString partPath = target.absoluteDir().filePath(QLatin1Char('.') + target.fileName()
                                                            + QStringLiteral(".converting"));
    QFile::remove(partPath);
    std::unique_ptr<SqliteGameWriter> writer = SqliteGameWriter::create(partPath, &result.error);
    if (!writer)
        return result;
    const auto fail = [&](const QString &error) {
        writer.reset();
        QFile::remove(partPath);
        result.error = error;
        return result;
    };

    const int threads = std::max(1, QThread::idealThreadCount() - 1);
    Progress &done = result.progress;
    done.totalBytes = pgn.size();
    PgnSplitter splitter;
    QList<QByteArray> entries;
    const auto write = [&](bool all) {
        while (entries.size() >= kBatchGames || (all && !entries.isEmpty())) {
            const QList<QByteArray> batch = entries.mid(0, kBatchGames);
            entries.remove(0, batch.size());
            QList<GameRecord> games;
            games.reserve(batch.size());
            for (std::optional<GameRecord> &game : readGames(batch, threads)) {
                if (game && !game->moves.isEmpty())
                    games << std::move(*game);
                else
                    ++done.skipped;
            }
            QString error;
            if (!writer->add(games, &error)) {
                result.error = error;
                return false;
            }
            done.games += games.size();
            done.elapsedMs = clock.elapsed();
            if (progress)
                progress(done);
            if (cancel && cancel->load())
                return false;
        }
        return true;
    };

    while (!pgn.atEnd()) {
        const QByteArray chunk = pgn.read(kChunkBytes);
        if (chunk.isEmpty() && pgn.error() != QFileDevice::NoError)
            return fail(Text::tr("Could not read “%1”: %2").arg(QFileInfo(pgnPath).fileName(), pgn.errorString()));
        done.bytesRead += chunk.size();
        entries += splitter.feed(chunk);
        if (!write(false))
            break;
    }
    if (result.error.isEmpty() && !(cancel && cancel->load())) {
        entries += splitter.finish();
        write(true);
    }
    if (!result.error.isEmpty())
        return fail(result.error);
    if (cancel && cancel->load()) {
        writer.reset();
        QFile::remove(partPath);
        result.cancelled = true;
        return result;
    }
    QString error;
    if (!writer->finish(&error))
        return fail(error);
    writer.reset();
    if (!QFile::rename(partPath, pdbPath))
        return fail(Text::tr("Could not name the database “%1”.").arg(target.fileName()));
    done.elapsedMs = clock.elapsed();
    result.ok = true;
    return result;
}

} // namespace PgnConversion
