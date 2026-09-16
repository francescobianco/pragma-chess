#include "SqliteGameDatabase.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>
#include <QVariant>

namespace {

// "PRAG" — lets other tools (and `file`) identify Pragma databases.
constexpr int kApplicationId = 0x50524147;
constexpr int kSchemaVersion = 1;

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

void setError(QString *errorMessage, const QString &message)
{
    if (errorMessage)
        *errorMessage = message;
}

QVariant nullIfEmpty(const QString &value)
{
    return value.isEmpty() ? QVariant() : QVariant(value);
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

    NameTable players(db, QStringLiteral("players"));
    NameTable events(db, QStringLiteral("events"));
    NameTable sites(db, QStringLiteral("sites"));
    QSqlQuery insert(db);
    insert.prepare(QStringLiteral(
        "INSERT INTO games (white_id, black_id, event_id, site_id, date, round, result,"
        " white_elo, black_elo, eco, ply_count, start_fen, moves_san, moves_uci)"
        " VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"));
    for (const GameRecord &game : games) {
        QStringList san;
        QStringList uci;
        for (const MoveRecord &move : game.moves) {
            san << move.san;
            uci << move.uci;
        }
        insert.addBindValue(players.idFor(game.white));
        insert.addBindValue(players.idFor(game.black));
        insert.addBindValue(events.idFor(game.event));
        insert.addBindValue(sites.idFor(game.site));
        insert.addBindValue(nullIfEmpty(game.date));
        insert.addBindValue(nullIfEmpty(game.round));
        insert.addBindValue(nullIfEmpty(game.result));
        insert.addBindValue(nullIfZero(game.whiteElo));
        insert.addBindValue(nullIfZero(game.blackElo));
        insert.addBindValue(nullIfEmpty(game.eco));
        insert.addBindValue(int(game.moves.size()));
        insert.addBindValue(nullIfEmpty(game.startFen));
        insert.addBindValue(san.join(QLatin1Char(' ')));
        insert.addBindValue(uci.join(QLatin1Char(' ')));
        if (!insert.exec())
            return fail(insert.lastError().text());
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
    query.finish();

    if (!database->loadHeaders(errorMessage))
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
