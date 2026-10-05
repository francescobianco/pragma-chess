#include "DatabaseMigrations.h"

#include "GameIdentity.h"
#include "GameRecord.h"

#include <QHash>
#include <QSet>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

namespace DatabaseMigrations {

namespace {

bool run(QSqlDatabase &db, const QList<const char *> &statements, QString *error)
{
    QSqlQuery query(db);
    for (const char *statement : statements) {
        if (!query.exec(QString::fromLatin1(statement))) {
            *error = query.lastError().text();
            return false;
        }
    }
    return true;
}

// 1: players, events and sites by name, and the games.
bool createGames(QSqlDatabase &db, QString *error)
{
    return run(db, {
        "CREATE TABLE players (id INTEGER PRIMARY KEY, name TEXT NOT NULL UNIQUE)",
        "CREATE TABLE events (id INTEGER PRIMARY KEY, name TEXT NOT NULL UNIQUE)",
        "CREATE TABLE sites (id INTEGER PRIMARY KEY, name TEXT NOT NULL UNIQUE)",
        "CREATE TABLE games ("
        " id INTEGER PRIMARY KEY,"
        " white_id INTEGER REFERENCES players(id),"
        " black_id INTEGER REFERENCES players(id),"
        " event_id INTEGER REFERENCES events(id),"
        " site_id INTEGER REFERENCES sites(id),"
        " date TEXT, round TEXT, result TEXT,"
        " white_elo INTEGER, black_elo INTEGER, eco TEXT,"
        " ply_count INTEGER NOT NULL DEFAULT 0,"
        " start_fen TEXT,"
        // Main line as space-separated SAN and UCI. A compact binary encoding
        // produced by the engine will replace these.
        " moves_san TEXT NOT NULL DEFAULT '',"
        " moves_uci TEXT NOT NULL DEFAULT '')",
        "CREATE INDEX games_white ON games(white_id)",
        "CREATE INDEX games_black ON games(black_id)",
    }, error);
}

// 2: external sources of games and where each imported game came from.
bool createSources(QSqlDatabase &db, QString *error)
{
    return run(db, {
        "CREATE TABLE IF NOT EXISTS sources ("
        " id INTEGER PRIMARY KEY,"
        " uuid TEXT NOT NULL UNIQUE,"
        " kind TEXT NOT NULL,"
        " account TEXT NOT NULL,"
        " settings TEXT NOT NULL DEFAULT '{}',"
        " state TEXT NOT NULL DEFAULT '{}',"
        " enabled INTEGER NOT NULL DEFAULT 1,"
        " created_at TEXT NOT NULL,"
        " last_sync_at TEXT,"
        " last_error TEXT)",
        "CREATE TABLE IF NOT EXISTS game_sources ("
        " game_id INTEGER NOT NULL REFERENCES games(id),"
        " source_id INTEGER NOT NULL REFERENCES sources(id),"
        " external_id TEXT NOT NULL,"
        " UNIQUE (source_id, external_id))",
    }, error);
}

// 3: who players are to the user (see PlayerRole).
bool createPlayerRoles(QSqlDatabase &db, QString *error)
{
    return run(db, {
        "CREATE TABLE IF NOT EXISTS player_roles ("
        " player_id INTEGER PRIMARY KEY REFERENCES players(id),"
        " role TEXT NOT NULL)",
    }, error);
}

// 4: properties of the database (see DatabaseProperties).
bool createProperties(QSqlDatabase &db, QString *error)
{
    return run(db, {
        "CREATE TABLE IF NOT EXISTS properties (key TEXT PRIMARY KEY, value TEXT NOT NULL)",
    }, error);
}

/// Fills in the uid of games stored before version 5, from their content as
/// if they were created now, so every copy of a database gets the same ones.
bool fillGameUids(QSqlDatabase &db, QString *error)
{
    QSqlQuery select(db);
    select.setForwardOnly(true);
    if (!select.exec(QStringLiteral(
            "SELECT g.id, w.name, b.name, e.name, s.name, g.date, g.round, g.result, g.start_fen, g.moves_uci"
            " FROM games g"
            " LEFT JOIN players w ON w.id = g.white_id"
            " LEFT JOIN players b ON b.id = g.black_id"
            " LEFT JOIN events e ON e.id = g.event_id"
            " LEFT JOIN sites s ON s.id = g.site_id"
            " ORDER BY g.id"))) {
        *error = select.lastError().text();
        return false;
    }
    QList<std::pair<qint64, QString>> uids;
    QHash<QString, int> occurrences;
    while (select.next()) {
        GameRecord game;
        game.white = select.value(1).toString();
        game.black = select.value(2).toString();
        game.event = select.value(3).toString();
        game.site = select.value(4).toString();
        game.date = select.value(5).toString();
        game.round = select.value(6).toString();
        game.result = select.value(7).toString();
        game.startFen = select.value(8).toString();
        for (const QString &uci : select.value(9).toString().split(QLatin1Char(' '), Qt::SkipEmptyParts))
            game.moves << MoveRecord{QString(), uci};
        const QString first = GameIdentity::uid(game);
        const int occurrence = ++occurrences[first];
        uids.append({select.value(0).toLongLong(), occurrence == 1 ? first : GameIdentity::uid(game, occurrence)});
    }
    select.finish();
    QSqlQuery update(db);
    update.prepare(QStringLiteral("UPDATE games SET uid = ? WHERE id = ?"));
    for (const auto &[id, uid] : uids) {
        update.addBindValue(uid);
        update.addBindValue(id);
        if (!update.exec()) {
            *error = update.lastError().text();
            return false;
        }
    }
    return true;
}

// 5: universal game ids and revisions (see GameIdentity). The unique index
// comes after the uids are filled in.
bool addGameIdentity(QSqlDatabase &db, QString *error)
{
    QSqlQuery query(db);
    // Files downgraded by hand (and tests) may have the columns already.
    QSet<QString> columns;
    if (query.exec(QStringLiteral("PRAGMA table_info(games)"))) {
        while (query.next())
            columns.insert(query.value(1).toString());
    }
    query.finish();
    for (const char *column : {"uid", "modified"}) {
        if (columns.contains(QLatin1String(column)))
            continue;
        if (!query.exec(QStringLiteral("ALTER TABLE games ADD COLUMN %1 TEXT").arg(QLatin1String(column)))) {
            *error = query.lastError().text();
            return false;
        }
    }
    return fillGameUids(db, error)
           && run(db, {"CREATE UNIQUE INDEX IF NOT EXISTS games_uid ON games(uid)"}, error);
}

// 6: the trash (see GameState). A game with no row here is live; the row of a
// purged game outlives the game, so a copy that still has it lets it go.
bool createGameStates(QSqlDatabase &db, QString *error)
{
    return run(db, {
        "CREATE TABLE IF NOT EXISTS game_states ("
        " uid TEXT PRIMARY KEY,"
        " state TEXT NOT NULL,"
        " modified TEXT NOT NULL)",
    }, error);
}

// 7: the variations of a game, as GameVariations::toText writes them; the
// main line stays in moves_san/moves_uci, so everything that reads only the
// main line (the indexes, the phone) goes on as before.
bool addVariations(QSqlDatabase &db, QString *error)
{
    QSqlQuery query(db);
    if (query.exec(QStringLiteral("PRAGMA table_info(games)"))) {
        while (query.next())
            if (query.value(1).toString() == QLatin1String("variations"))
                return true; // Files made by hand (and tests) may have it already.
    }
    query.finish();
    return run(db, {"ALTER TABLE games ADD COLUMN variations TEXT NOT NULL DEFAULT ''"}, error);
}

/// The PGN tags that have no column (`[StudyName "…"]`, one per line,
/// Pgn::tagsText) and the comments of the game (MoveComment::toJson).
bool addTagsAndComments(QSqlDatabase &db, QString *error)
{
    // Files made by hand (and tests) may have them already.
    QStringList columns;
    QSqlQuery query(db);
    if (query.exec(QStringLiteral("PRAGMA table_info(games)"))) {
        while (query.next())
            columns << query.value(1).toString();
    }
    query.finish();
    if (!columns.contains(QLatin1String("tags"))
        && !run(db, {"ALTER TABLE games ADD COLUMN tags TEXT NOT NULL DEFAULT ''"}, error))
        return false;
    return columns.contains(QLatin1String("comments"))
        || run(db, {"ALTER TABLE games ADD COLUMN comments TEXT NOT NULL DEFAULT ''"}, error);
}

} // namespace

const QList<Migration> &all()
{
    static const QList<Migration> migrations = {
        {1, "create_games", &createGames},
        {2, "create_sources", &createSources},
        {3, "create_player_roles", &createPlayerRoles},
        {4, "create_properties", &createProperties},
        {5, "add_game_identity", &addGameIdentity},
        {6, "create_game_states", &createGameStates},
        {7, "add_variations", &addVariations},
        {8, "add_tags_and_comments", &addTagsAndComments},
    };
    return migrations;
}

int latestVersion()
{
    return all().constLast().version;
}

bool migrate(QSqlDatabase &db, int from, QString *errorMessage)
{
    for (const Migration &migration : all()) {
        if (migration.version <= from)
            continue;
        QString error;
        bool done = db.transaction();
        done = done && migration.up(db, &error);
        QSqlQuery query(db);
        // The log of what ran and when; files older than the log only list
        // the migrations run since.
        done = done && query.exec(QStringLiteral(
                   "CREATE TABLE IF NOT EXISTS migrations ("
                   " version INTEGER PRIMARY KEY, name TEXT NOT NULL, applied_at TEXT NOT NULL)"));
        if (done) {
            query.prepare(QStringLiteral("INSERT OR REPLACE INTO migrations (version, name, applied_at) VALUES (?, ?, ?)"));
            query.addBindValue(migration.version);
            query.addBindValue(QString::fromLatin1(migration.name));
            query.addBindValue(GameIdentity::now());
            done = query.exec();
        }
        done = done && query.exec(QStringLiteral("PRAGMA user_version = %1").arg(migration.version));
        if (!done || !db.commit()) {
            if (error.isEmpty())
                error = query.lastError().isValid() ? query.lastError().text() : db.lastError().text();
            db.rollback();
            if (errorMessage)
                *errorMessage = QStringLiteral("%1 (%2): %3").arg(migration.version).arg(QLatin1String(migration.name), error);
            return false;
        }
    }
    return true;
}

} // namespace DatabaseMigrations
