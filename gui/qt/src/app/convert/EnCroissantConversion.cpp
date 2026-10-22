#include "EnCroissantConversion.h"

#include "app/SqliteGameDatabase.h"
#include "app/extensions/EnCroissantGames.h"

#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QThread>
#include <QUuid>

#include <algorithm>
#include <optional>
#include <thread>
#include <vector>

namespace EnCroissantConversion {

namespace {

struct Text {
    Q_DECLARE_TR_FUNCTIONS(EnCroissantConversion)
};

constexpr int kBatchGames = 4000;

/// A row of Games, with the names its ids stand for.
struct Row {
    GameRecord game;
    QByteArray moves;
};

/// "?" and En Croissant's "Unknown" (the row 0 of its Players, Events and
/// Sites) are no name.
QString known(qint64 id, const QString &value)
{
    const QString text = value.trimmed();
    if (id == 0 || text == QLatin1String("?") || text == QLatin1String("-") || text == QLatin1String("Unknown"))
        return {};
    return text;
}

/// Replays the moves of `rows` on `threads` threads: the games read, in order.
std::vector<std::optional<GameRecord>> readGames(std::vector<Row> &rows, int threads)
{
    std::vector<std::optional<GameRecord>> games(rows.size());
    std::atomic<size_t> next{0};
    const auto work = [&] {
        for (size_t i = next++; i < rows.size(); i = next++) {
            Row &row = rows.at(i);
            std::optional<ChessPosition> start = row.game.startFen.isEmpty()
                                                     ? ChessPosition::startingPosition()
                                                     : ChessPosition::fromFen(row.game.startFen, ChessPosition::Kings::Optional);
            if (!start)
                continue;
            std::optional<QList<MoveRecord>> moves = EnCroissantGames::mainLine(row.moves, *start);
            if (!moves || moves->isEmpty())
                continue;
            row.game.moves = std::move(*moves);
            row.game.plyCount = int(row.game.moves.size());
            games[i] = std::move(row.game);
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

Result convert(QSqlDatabase &source, const QString &db3Path, const QString &pdbPath,
               const DatabaseProperties &properties, const std::function<void(const Progress &)> &progress,
               const std::atomic_bool *cancel)
{
    Result result;
    QElapsedTimer clock;
    clock.start();
    const QString fileName = QFileInfo(db3Path).fileName();
    if (QFileInfo::exists(pdbPath)) {
        result.error = Text::tr("A file named “%1” already exists.").arg(QFileInfo(pdbPath).fileName());
        return result;
    }
    source.setDatabaseName(QFileInfo(db3Path).absoluteFilePath());
    source.setConnectOptions(QStringLiteral("QSQLITE_OPEN_READONLY"));
    if (!source.open()) {
        result.error = Text::tr("Could not read “%1”: %2").arg(fileName, source.lastError().text());
        return result;
    }
    QSqlQuery query(source);
    query.setForwardOnly(true);
    if (!query.exec(QStringLiteral("SELECT count(*) FROM Games")) || !query.next()) {
        result.error = Text::tr("“%1” is not a database of En Croissant's.").arg(fileName);
        return result;
    }
    Progress &done = result.progress;
    done.total = query.value(0).toLongLong();
    if (!query.exec(QStringLiteral(
            "SELECT g.WhiteID, w.Name, g.BlackID, b.Name, g.EventID, e.Name, g.SiteID, s.Name,"
            " g.Date, g.Round, g.WhiteElo, g.BlackElo, g.Result, g.TimeControl, g.ECO, g.FEN, g.Moves, g.UTCTime"
            " FROM Games g LEFT JOIN Players w ON w.ID = g.WhiteID LEFT JOIN Players b ON b.ID = g.BlackID"
            " LEFT JOIN Events e ON e.ID = g.EventID LEFT JOIN Sites s ON s.ID = g.SiteID ORDER BY g.ID"))) {
        result.error = Text::tr("“%1” is not a database of En Croissant's: %2").arg(fileName, query.lastError().text());
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
    bool more = true;
    while (more) {
        std::vector<Row> rows;
        rows.reserve(kBatchGames);
        while (rows.size() < size_t(kBatchGames) && (more = query.next())) {
            Row row;
            GameRecord &game = row.game;
            game.white = known(query.value(0).toLongLong(), query.value(1).toString());
            game.black = known(query.value(2).toLongLong(), query.value(3).toString());
            game.event = known(query.value(4).toLongLong(), query.value(5).toString());
            game.site = known(query.value(6).toLongLong(), query.value(7).toString());
            const QString date = query.value(8).toString().trimmed();
            game.date = date == QLatin1String("????.??.??") ? QString() : date;
            game.round = known(-1, query.value(9).toString());
            game.whiteElo = query.value(10).toInt();
            game.blackElo = query.value(11).toInt();
            const QString outcome = query.value(12).toString().trimmed();
            game.result = outcome == QLatin1String("*") ? QString() : outcome;
            if (const QString timeControl = known(-1, query.value(13).toString()); !timeControl.isEmpty())
                game.tags << PgnTag{QStringLiteral("TimeControl"), timeControl};
            game.eco = known(-1, query.value(14).toString());
            game.startFen = query.value(15).toString().trimmed();
            row.moves = query.value(16).toByteArray();
            if (const QString time = known(-1, query.value(17).toString()); !time.isEmpty())
                game.tags << PgnTag{QStringLiteral("UTCTime"), time};
            rows.push_back(std::move(row));
        }
        if (rows.empty())
            break;
        QList<GameRecord> games;
        games.reserve(qsizetype(rows.size()));
        for (std::optional<GameRecord> &game : readGames(rows, threads)) {
            if (game)
                games << std::move(*game);
            else
                ++done.skipped;
        }
        QString error;
        if (!writer->add(games, &error))
            return fail(error);
        done.games += games.size();
        done.elapsedMs = clock.elapsed();
        if (progress)
            progress(done);
        if (cancel && cancel->load()) {
            writer.reset();
            QFile::remove(partPath);
            result.cancelled = true;
            return result;
        }
    }
    if (query.lastError().isValid())
        return fail(Text::tr("Could not read “%1”: %2").arg(fileName, query.lastError().text()));
    QString error;
    if (!writer->setProperties(properties, &error) || !writer->finish(&error))
        return fail(error);
    writer.reset();
    if (!QFile::rename(partPath, pdbPath))
        return fail(Text::tr("Could not name the database “%1”.").arg(target.fileName()));
    done.elapsedMs = clock.elapsed();
    result.ok = true;
    return result;
}

} // namespace

Result run(const QString &db3Path, const QString &pdbPath, const DatabaseProperties &properties,
           const std::function<void(const Progress &)> &progress, const std::atomic_bool *cancel)
{
    const QString connection = QUuid::createUuid().toString(QUuid::WithoutBraces);
    Result result;
    {
        QSqlDatabase source = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connection);
        result = convert(source, db3Path, pdbPath, properties, progress, cancel);
        source.close();
    }
    QSqlDatabase::removeDatabase(connection);
    return result;
}

} // namespace EnCroissantConversion
