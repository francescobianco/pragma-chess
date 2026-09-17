#include "SqliteGameDatabase.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QJsonDocument>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>
#include <QVariant>

#include <iterator>

namespace {

// "PRAG" — lets other tools (and `file`) identify Pragma databases.
constexpr int kApplicationId = 0x50524147;
constexpr int kSchemaVersion = 3;

const char *const kSchema[] = {
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
};

// Version 2: external sources of games and where each imported game came from.
const char *const kSourcesSchema[] = {
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
};

// Version 3: who players are to the user (see PlayerRole).
const char *const kPlayerRolesSchema[] = {
    "CREATE TABLE IF NOT EXISTS player_roles ("
    " player_id INTEGER PRIMARY KEY REFERENCES players(id),"
    " role TEXT NOT NULL)",
};

QString toJsonText(const QJsonObject &object)
{
    return QString::fromUtf8(QJsonDocument(object).toJson(QJsonDocument::Compact));
}

QJsonObject fromJsonText(const QString &text)
{
    return QJsonDocument::fromJson(text.toUtf8()).object();
}

void setError(QString *errorMessage, const QString &message)
{
    if (errorMessage)
        *errorMessage = message;
}

QVariant nullIfEmpty(const QString &value)
{
    return value.isEmpty() ? QVariant() : QVariant(value);
}

/// Joined moves; a game without moves stores '' (the columns are NOT NULL, and
/// Qt binds a null QString, which joining an empty list gives, as NULL).
QString movesText(const QStringList &moves)
{
    return moves.isEmpty() ? QStringLiteral("") : moves.join(QLatin1Char(' '));
}

QVariant nullIfZero(int value)
{
    return value > 0 ? QVariant(value) : QVariant();
}

class NameTable {
public:
    NameTable(QSqlDatabase db, const QString &table)
        : m_insert(db)
        , m_select(db)
    {
        m_insert.prepare(QStringLiteral("INSERT OR IGNORE INTO %1 (name) VALUES (?)").arg(table));
        m_select.prepare(QStringLiteral("SELECT id FROM %1 WHERE name = ?").arg(table));
    }

    QVariant idFor(const QString &name)
    {
        if (name.isEmpty())
            return {};
        if (auto it = m_cache.constFind(name); it != m_cache.cend())
            return *it;
        m_insert.addBindValue(name);
        m_insert.exec();
        m_select.addBindValue(name);
        if (!m_select.exec() || !m_select.next())
            return {};
        const qint64 id = m_select.value(0).toLongLong();
        m_select.finish();
        m_cache.insert(name, id);
        return id;
    }

private:
    QSqlQuery m_insert;
    QSqlQuery m_select;
    QHash<QString, qint64> m_cache;
};

/// Inserts games with their players, events and sites.
class GameInserter {
public:
    explicit GameInserter(QSqlDatabase db)
        : m_players(db, QStringLiteral("players"))
        , m_events(db, QStringLiteral("events"))
        , m_sites(db, QStringLiteral("sites"))
        , m_insert(db)
    {
        m_insert.prepare(QStringLiteral(
            "INSERT INTO games (white_id, black_id, event_id, site_id, date, round, result,"
            " white_elo, black_elo, eco, ply_count, start_fen, moves_san, moves_uci)"
            " VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"));
    }

    /// Returns the id of the new game, or 0 on failure.
    qint64 insert(const GameRecord &game)
    {
        QStringList san;
        QStringList uci;
        for (const MoveRecord &move : game.moves) {
            san << move.san;
            uci << move.uci;
        }
        m_insert.addBindValue(m_players.idFor(game.white));
        m_insert.addBindValue(m_players.idFor(game.black));
        m_insert.addBindValue(m_events.idFor(game.event));
        m_insert.addBindValue(m_sites.idFor(game.site));
        m_insert.addBindValue(nullIfEmpty(game.date));
        m_insert.addBindValue(nullIfEmpty(game.round));
        m_insert.addBindValue(nullIfEmpty(game.result));
        m_insert.addBindValue(nullIfZero(game.whiteElo));
        m_insert.addBindValue(nullIfZero(game.blackElo));
        m_insert.addBindValue(nullIfEmpty(game.eco));
        m_insert.addBindValue(int(game.moves.size()));
        m_insert.addBindValue(nullIfEmpty(game.startFen));
        m_insert.addBindValue(movesText(san));
        m_insert.addBindValue(movesText(uci));
        if (!m_insert.exec())
            return 0;
        return m_insert.lastInsertId().toLongLong();
    }

    QString errorText() const { return m_insert.lastError().text(); }

private:
    NameTable m_players;
    NameTable m_events;
    NameTable m_sites;
    QSqlQuery m_insert;
};

} // namespace

SqliteGameDatabase::SqliteGameDatabase(QString path, QString connectionName)
    : m_path(std::move(path))
    , m_connectionName(std::move(connectionName))
{
}

SqliteGameDatabase::~SqliteGameDatabase()
{
    {
        QSqlDatabase db = QSqlDatabase::database(m_connectionName, false);
        db.close();
    }
    QSqlDatabase::removeDatabase(m_connectionName);
}

QString SqliteGameDatabase::name() const
{
    return QFileInfo(m_path).completeBaseName();
}

std::unique_ptr<SqliteGameDatabase> SqliteGameDatabase::create(const QString &path,
                                                               const QList<GameRecord> &games,
                                                               QString *errorMessage)
{
    if (QFileInfo::exists(path)) {
        setError(errorMessage, QObject::tr("A file named “%1” already exists.")
                                   .arg(QFileInfo(path).fileName()));
        return nullptr;
    }

    const QString connection = QUuid::createUuid().toString(QUuid::WithoutBraces);
    std::unique_ptr<SqliteGameDatabase> database(
        new SqliteGameDatabase(QFileInfo(path).absoluteFilePath(), connection));

    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connection);
    db.setDatabaseName(database->m_path);
    if (!db.open()) {
        setError(errorMessage, db.lastError().text());
        return nullptr;
    }

    const auto fail = [&](const QString &message) {
        db.rollback();
        database.reset();
        QFile::remove(path);
        setError(errorMessage, message);
        return nullptr;
    };

    QSqlQuery query(db);
    query.exec(QStringLiteral("PRAGMA application_id = %1").arg(kApplicationId));
    query.exec(QStringLiteral("PRAGMA user_version = %1").arg(kSchemaVersion));
    db.transaction();
    for (const char *statement : kSchema) {
        if (!query.exec(QString::fromLatin1(statement)))
            return fail(query.lastError().text());
    }
    for (const char *statement : kSourcesSchema) {
        if (!query.exec(QString::fromLatin1(statement)))
            return fail(query.lastError().text());
    }
    for (const char *statement : kPlayerRolesSchema) {
        if (!query.exec(QString::fromLatin1(statement)))
            return fail(query.lastError().text());
    }

    GameInserter inserter(db);
    for (const GameRecord &game : games) {
        if (inserter.insert(game) == 0)
            return fail(inserter.errorText());
    }
    if (!db.commit())
        return fail(db.lastError().text());

    if (!database->loadHeaders(errorMessage))
        return nullptr;
    return database;
}

std::unique_ptr<SqliteGameDatabase> SqliteGameDatabase::open(const QString &path, QString *errorMessage)
{
    const QFileInfo info(path);
    if (!info.isFile()) {
        setError(errorMessage, QObject::tr("“%1” does not exist.").arg(info.fileName()));
        return nullptr;
    }

    const QString connection = QUuid::createUuid().toString(QUuid::WithoutBraces);
    std::unique_ptr<SqliteGameDatabase> database(
        new SqliteGameDatabase(info.absoluteFilePath(), connection));

    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connection);
    db.setDatabaseName(database->m_path);
    if (!db.open()) {
        setError(errorMessage, db.lastError().text());
        return nullptr;
    }

    const QString notPragma = QObject::tr("“%1” is not a Pragma Chess database.").arg(info.fileName());
    QSqlQuery query(db);
    if (!query.exec(QStringLiteral("PRAGMA application_id")) || !query.next()
        || query.value(0).toInt() != kApplicationId) {
        setError(errorMessage, notPragma);
        return nullptr;
    }
    if (!query.exec(QStringLiteral("PRAGMA user_version")) || !query.next()
        || query.value(0).toInt() > kSchemaVersion) {
        setError(errorMessage, QObject::tr("“%1” was created by a newer version of Pragma Chess.")
                                   .arg(info.fileName()));
        return nullptr;
    }
    const int version = query.value(0).toInt();
    query.finish();

    // Upgrade older files in place; each step only adds tables.
    const auto upgrade = [&](int to, const QList<const char *> &statements) {
        db.transaction();
        bool upgraded = true;
        for (const char *statement : statements)
            upgraded = upgraded && query.exec(QString::fromLatin1(statement));
        upgraded = upgraded && query.exec(QStringLiteral("PRAGMA user_version = %1").arg(to));
        if (!upgraded || !db.commit()) {
            db.rollback();
            setError(errorMessage, QObject::tr("Could not upgrade “%1”: %2")
                                       .arg(info.fileName(), query.lastError().text()));
            return false;
        }
        return true;
    };
    if (version < 2 && !upgrade(2, {std::begin(kSourcesSchema), std::end(kSourcesSchema)}))
        return nullptr;
    if (version < 3 && !upgrade(3, {std::begin(kPlayerRolesSchema), std::end(kPlayerRolesSchema)}))
        return nullptr;

    if (!database->loadHeaders(errorMessage) || !database->loadPlayerRoles(errorMessage))
        return nullptr;
    return database;
}

bool SqliteGameDatabase::loadHeaders(QString *errorMessage)
{
    QSqlQuery query(QSqlDatabase::database(m_connectionName));
    query.setForwardOnly(true);
    if (!query.exec(QStringLiteral(
            "SELECT g.id, w.name, b.name, g.white_elo, g.black_elo, e.name, s.name,"
            " g.date, g.round, g.result, g.eco, g.ply_count, g.start_fen"
            " FROM games g"
            " LEFT JOIN players w ON w.id = g.white_id"
            " LEFT JOIN players b ON b.id = g.black_id"
            " LEFT JOIN events e ON e.id = g.event_id"
            " LEFT JOIN sites s ON s.id = g.site_id"
            " ORDER BY g.id"))) {
        setError(errorMessage, query.lastError().text());
        return false;
    }

    m_headers.clear();
    while (query.next()) {
        GameRecord g;
        g.id = query.value(0).toLongLong();
        g.white = query.value(1).toString();
        g.black = query.value(2).toString();
        g.whiteElo = query.value(3).toInt();
        g.blackElo = query.value(4).toInt();
        g.event = query.value(5).toString();
        g.site = query.value(6).toString();
        g.date = query.value(7).toString();
        g.round = query.value(8).toString();
        g.result = query.value(9).toString();
        g.eco = query.value(10).toString();
        g.plyCount = query.value(11).toInt();
        g.startFen = query.value(12).toString();
        m_headers << g;
    }
    return true;
}

bool SqliteGameDatabase::loadPlayerRoles(QString *errorMessage)
{
    QSqlQuery query(QSqlDatabase::database(m_connectionName));
    if (!query.exec(QStringLiteral("SELECT p.name, r.role FROM player_roles r JOIN players p ON p.id = r.player_id"))) {
        setError(errorMessage, query.lastError().text());
        return false;
    }
    m_roles.clear();
    while (query.next()) {
        if (const PlayerRole role = playerRoleFromKey(query.value(1).toString()); role != PlayerRole::None)
            m_roles.insert(query.value(0).toString(), role);
    }
    return true;
}

bool SqliteGameDatabase::setPlayerRole(const QString &player, PlayerRole role, QString *errorMessage)
{
    if (player.isEmpty()) {
        setError(errorMessage, QObject::tr("The game has no player name."));
        return false;
    }
    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    db.transaction();
    NameTable players(db, QStringLiteral("players"));
    const QVariant id = players.idFor(player);
    QSqlQuery query(db);
    if (role == PlayerRole::None) {
        query.prepare(QStringLiteral("DELETE FROM player_roles WHERE player_id = ?"));
        query.addBindValue(id);
    } else {
        query.prepare(QStringLiteral("INSERT OR REPLACE INTO player_roles (player_id, role) VALUES (?, ?)"));
        query.addBindValue(id);
        query.addBindValue(playerRoleKey(role));
    }
    if (!id.isValid() || !query.exec() || !db.commit()) {
        setError(errorMessage, query.lastError().isValid() ? query.lastError().text() : db.lastError().text());
        db.rollback();
        return false;
    }
    if (role == PlayerRole::None)
        m_roles.remove(player);
    else
        m_roles.insert(player, role);
    return true;
}

std::optional<GameRecord> SqliteGameDatabase::loadGame(qint64 index) const
{
    if (index < 0 || index >= m_headers.size())
        return std::nullopt;

    GameRecord game = m_headers.at(index);
    QSqlQuery query(QSqlDatabase::database(m_connectionName));
    query.prepare(QStringLiteral("SELECT moves_san, moves_uci FROM games WHERE id = ?"));
    query.addBindValue(game.id);
    if (!query.exec() || !query.next())
        return std::nullopt;

    const QStringList san = query.value(0).toString().split(QLatin1Char(' '), Qt::SkipEmptyParts);
    const QStringList uci = query.value(1).toString().split(QLatin1Char(' '), Qt::SkipEmptyParts);
    for (qsizetype i = 0; i < qMin(san.size(), uci.size()); ++i)
        game.moves << MoveRecord{san.at(i), uci.at(i)};
    return game;
}

bool SqliteGameDatabase::saveCopy(const QString &path, QString *errorMessage) const
{
    const QString target = QFileInfo(path).absoluteFilePath();
    if (target == m_path)
        return true;

    // VACUUM INTO writes a compact, consistent snapshot; it refuses to overwrite,
    // so write next to the target and swap it in.
    const QString temporary = target + QStringLiteral(".saving");
    QFile::remove(temporary);
    QSqlQuery query(QSqlDatabase::database(m_connectionName));
    query.prepare(QStringLiteral("VACUUM INTO ?"));
    query.addBindValue(temporary);
    if (!query.exec()) {
        setError(errorMessage, query.lastError().text());
        QFile::remove(temporary);
        return false;
    }
    if (QFileInfo::exists(target) && !QFile::remove(target)) {
        setError(errorMessage, QObject::tr("Could not replace “%1”.").arg(QFileInfo(target).fileName()));
        QFile::remove(temporary);
        return false;
    }
    if (!QFile::rename(temporary, target)) {
        setError(errorMessage, QObject::tr("Could not write “%1”.").arg(QFileInfo(target).fileName()));
        QFile::remove(temporary);
        return false;
    }
    return true;
}

qint64 SqliteGameDatabase::addGame(const GameRecord &game, QString *errorMessage)
{
    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    db.transaction();
    GameInserter inserter(db);
    const qint64 id = inserter.insert(game);
    if (id == 0 || !db.commit()) {
        setError(errorMessage, id == 0 ? inserter.errorText() : db.lastError().text());
        db.rollback();
        return -1;
    }

    GameRecord header = game;
    header.id = id;
    header.plyCount = int(game.moves.size());
    header.moves.clear();
    m_headers << header;
    return m_headers.size() - 1;
}

bool SqliteGameDatabase::updateHeader(qint64 index, const GameRecord &header, QString *errorMessage)
{
    if (index < 0 || index >= m_headers.size()) {
        setError(errorMessage, QObject::tr("The game does not exist."));
        return false;
    }

    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    db.transaction();
    NameTable players(db, QStringLiteral("players"));
    NameTable events(db, QStringLiteral("events"));
    NameTable sites(db, QStringLiteral("sites"));

    QSqlQuery update(db);
    update.prepare(QStringLiteral(
        "UPDATE games SET white_id = ?, black_id = ?, event_id = ?, site_id = ?, date = ?,"
        " round = ?, result = ?, white_elo = ?, black_elo = ?, eco = ? WHERE id = ?"));
    update.addBindValue(players.idFor(header.white));
    update.addBindValue(players.idFor(header.black));
    update.addBindValue(events.idFor(header.event));
    update.addBindValue(sites.idFor(header.site));
    update.addBindValue(nullIfEmpty(header.date));
    update.addBindValue(nullIfEmpty(header.round));
    update.addBindValue(nullIfEmpty(header.result));
    update.addBindValue(nullIfZero(header.whiteElo));
    update.addBindValue(nullIfZero(header.blackElo));
    update.addBindValue(nullIfEmpty(header.eco));
    update.addBindValue(m_headers.at(index).id);
    if (!update.exec() || !db.commit()) {
        setError(errorMessage, update.lastError().isValid() ? update.lastError().text() : db.lastError().text());
        db.rollback();
        return false;
    }

    GameRecord &cached = m_headers[index];
    cached.white = header.white;
    cached.black = header.black;
    cached.whiteElo = header.whiteElo;
    cached.blackElo = header.blackElo;
    cached.event = header.event;
    cached.site = header.site;
    cached.date = header.date;
    cached.round = header.round;
    cached.result = header.result;
    cached.eco = header.eco;
    return true;
}

QList<GameSource> SqliteGameDatabase::sources() const
{
    QList<GameSource> result;
    QSqlQuery query(QSqlDatabase::database(m_connectionName));
    if (!query.exec(QStringLiteral(
            "SELECT s.id, s.uuid, s.kind, s.account, s.settings, s.state, s.enabled, s.created_at,"
            " s.last_sync_at, s.last_error, (SELECT COUNT(*) FROM game_sources g WHERE g.source_id = s.id)"
            " FROM sources s ORDER BY s.id")))
        return result;
    while (query.next()) {
        GameSource source;
        source.id = query.value(0).toLongLong();
        source.uuid = query.value(1).toString();
        source.kind = query.value(2).toString();
        source.account = query.value(3).toString();
        source.settings = fromJsonText(query.value(4).toString());
        source.state = fromJsonText(query.value(5).toString());
        source.enabled = query.value(6).toBool();
        source.createdAt = QDateTime::fromString(query.value(7).toString(), Qt::ISODate);
        source.lastSyncAt = QDateTime::fromString(query.value(8).toString(), Qt::ISODate);
        source.lastError = query.value(9).toString();
        source.importedGames = query.value(10).toLongLong();
        result << source;
    }
    return result;
}

bool SqliteGameDatabase::addSource(GameSource &source, QString *errorMessage)
{
    if (source.uuid.isEmpty())
        source.uuid = QUuid::createUuid().toString(QUuid::WithoutBraces);
    if (!source.createdAt.isValid())
        source.createdAt = QDateTime::currentDateTimeUtc();

    QSqlQuery insert(QSqlDatabase::database(m_connectionName));
    insert.prepare(QStringLiteral(
        "INSERT INTO sources (uuid, kind, account, settings, state, enabled, created_at)"
        " VALUES (?, ?, ?, ?, ?, ?, ?)"));
    insert.addBindValue(source.uuid);
    insert.addBindValue(source.kind);
    insert.addBindValue(source.account);
    insert.addBindValue(toJsonText(source.settings));
    insert.addBindValue(toJsonText(source.state));
    insert.addBindValue(source.enabled);
    insert.addBindValue(source.createdAt.toString(Qt::ISODate));
    if (!insert.exec()) {
        setError(errorMessage, insert.lastError().text());
        return false;
    }
    source.id = insert.lastInsertId().toLongLong();
    return true;
}

bool SqliteGameDatabase::updateSource(const GameSource &source, QString *errorMessage)
{
    QSqlQuery update(QSqlDatabase::database(m_connectionName));
    update.prepare(QStringLiteral(
        "UPDATE sources SET account = ?, settings = ?, state = ?, enabled = ?, last_sync_at = ?, last_error = ?"
        " WHERE id = ?"));
    update.addBindValue(source.account);
    update.addBindValue(toJsonText(source.settings));
    update.addBindValue(toJsonText(source.state));
    update.addBindValue(source.enabled);
    update.addBindValue(source.lastSyncAt.isValid() ? QVariant(source.lastSyncAt.toString(Qt::ISODate)) : QVariant());
    update.addBindValue(nullIfEmpty(source.lastError));
    update.addBindValue(source.id);
    if (!update.exec()) {
        setError(errorMessage, update.lastError().text());
        return false;
    }
    return true;
}

bool SqliteGameDatabase::removeSource(qint64 sourceId, QString *errorMessage)
{
    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    db.transaction();
    QSqlQuery query(db);
    query.prepare(QStringLiteral("DELETE FROM game_sources WHERE source_id = ?"));
    query.addBindValue(sourceId);
    bool ok = query.exec();
    query.prepare(QStringLiteral("DELETE FROM sources WHERE id = ?"));
    query.addBindValue(sourceId);
    ok = ok && query.exec();
    if (!ok || !db.commit()) {
        setError(errorMessage, query.lastError().isValid() ? query.lastError().text() : db.lastError().text());
        db.rollback();
        return false;
    }
    return true;
}

int SqliteGameDatabase::importGames(qint64 sourceId, const QList<ImportedGame> &games, QString *errorMessage)
{
    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    db.transaction();
    GameInserter inserter(db);
    QSqlQuery known(db);
    known.prepare(QStringLiteral("SELECT 1 FROM game_sources WHERE source_id = ? AND external_id = ?"));
    QSqlQuery link(db);
    link.prepare(QStringLiteral("INSERT INTO game_sources (game_id, source_id, external_id) VALUES (?, ?, ?)"));

    QList<GameRecord> added;
    for (const ImportedGame &imported : games) {
        known.addBindValue(sourceId);
        known.addBindValue(imported.externalId);
        if (!known.exec()) {
            setError(errorMessage, known.lastError().text());
            db.rollback();
            return -1;
        }
        const bool alreadyImported = known.next();
        known.finish();
        if (alreadyImported)
            continue;

        const qint64 id = inserter.insert(imported.game);
        link.addBindValue(id);
        link.addBindValue(sourceId);
        link.addBindValue(imported.externalId);
        if (id == 0 || !link.exec()) {
            setError(errorMessage, id == 0 ? inserter.errorText() : link.lastError().text());
            db.rollback();
            return -1;
        }
        GameRecord header = imported.game;
        header.id = id;
        header.plyCount = int(imported.game.moves.size());
        header.moves.clear();
        added << header;
    }
    if (!db.commit()) {
        setError(errorMessage, db.lastError().text());
        db.rollback();
        return -1;
    }
    m_headers += added;
    return int(added.size());
}

QSet<qint64> SqliteGameDatabase::sourceGameIds(qint64 sourceId) const
{
    QSet<qint64> ids;
    QSqlQuery query(QSqlDatabase::database(m_connectionName));
    query.setForwardOnly(true);
    query.prepare(QStringLiteral("SELECT game_id FROM game_sources WHERE source_id = ?"));
    query.addBindValue(sourceId);
    if (query.exec()) {
        while (query.next())
            ids.insert(query.value(0).toLongLong());
    }
    return ids;
}
