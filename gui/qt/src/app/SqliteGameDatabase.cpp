#include "SqliteGameDatabase.h"

#include "DatabaseMigrations.h"
#include "GameIdentity.h"
#include "GameVariations.h"
#include "MoveAnnotation.h"
#include "MoveComment.h"
#include "Pgn.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QSet>
#include <QJsonDocument>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>
#include <QVariant>

#include <algorithm>
#include <iterator>

namespace {

// "PRAG" — lets other tools (and `file`) identify Pragma databases.
constexpr int kApplicationId = 0x50524147;
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

/// How many plies of a game the lists show (GameRecord::linePreview), and how
/// much of the stored SAN is read to have them.
constexpr int kPreviewPlies = 24;
constexpr int kPreviewChars = 240;

QString previewOf(const GameRecord &game)
{
    QStringList san;
    for (qsizetype i = 0; i < game.moves.size() && i < kPreviewPlies; ++i)
        san << game.moves.at(i).san + MoveAnnotation::storedSuffix(game.moves.at(i).nags);
    return Pgn::preview(game.startFen, san, int(game.moves.size()));
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
        , m_uidTaken(db)
        , m_revive(db)
    {
        m_insert.prepare(QStringLiteral(
            "INSERT INTO games (white_id, black_id, event_id, site_id, date, round, result,"
            " white_elo, black_elo, eco, ply_count, start_fen, moves_san, moves_uci, variations, tags, comments,"
            " uid, modified)"
            " VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"));
        m_uidTaken.prepare(QStringLiteral("SELECT 1 FROM games WHERE uid = ?"));
        m_revive.prepare(QStringLiteral("UPDATE game_states SET state = 'live', modified = ? WHERE uid = ? AND state = 'purged'"));
    }

    /// Returns the id of the new game, or 0 on failure. A game without a uid
    /// gets one from its content (the next free occurrence, for a game the
    /// database already holds); one with a uid keeps it, and fails if taken.
    qint64 insert(const GameRecord &game)
    {
        m_lastUid = game.uid;
        for (int occurrence = 1; m_lastUid.isEmpty(); ++occurrence) {
            const QString candidate = GameIdentity::uid(game, occurrence);
            m_uidTaken.addBindValue(candidate);
            if (!m_uidTaken.exec())
                return 0;
            if (!m_uidTaken.next())
                m_lastUid = candidate;
            m_uidTaken.finish();
        }
        m_lastModified = game.modified.isEmpty() ? GameIdentity::now() : game.modified;
        QStringList san;
        QStringList uci;
        for (const MoveRecord &move : game.moves) {
            san << move.san + MoveAnnotation::storedSuffix(move.nags);
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
        m_insert.addBindValue(GameVariations::toText(game.variations));
        m_insert.addBindValue(Pgn::tagsText(game.tags));
        m_insert.addBindValue(MoveComment::toJson(game));
        m_insert.addBindValue(m_lastUid);
        m_insert.addBindValue(m_lastModified);
        if (!m_insert.exec())
            return 0;
        const qint64 id = m_insert.lastInsertId().toLongLong();
        // A purged game stored again is a game again, and the newer state
        // says so to the other copies. (A merge leaves out the games purged
        // here before it inserts.)
        m_revive.addBindValue(GameIdentity::now());
        m_revive.addBindValue(m_lastUid);
        if (!m_revive.exec())
            return 0;
        return id;
    }

    /// The header of the game just inserted, as the game list caches it.
    GameRecord header(const GameRecord &game, qint64 id) const
    {
        GameRecord header = game;
        header.id = id;
        header.plyCount = int(game.moves.size());
        header.linePreview = previewOf(game);
        header.moves.clear();
        header.uid = m_lastUid;
        header.modified = m_lastModified;
        header.state = GameState::Live;
        header.stateModified.clear();
        return header;
    }

    QString errorText() const
    {
        for (const QSqlQuery *query : {&m_insert, &m_uidTaken, &m_revive}) {
            if (query->lastError().isValid())
                return query->lastError().text();
        }
        return {};
    }

private:
    NameTable m_players;
    NameTable m_events;
    NameTable m_sites;
    QSqlQuery m_insert;
    QSqlQuery m_uidTaken;
    QSqlQuery m_revive;
    QString m_lastUid;
    QString m_lastModified;
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
    // A new file runs every migration, like an old one opened now.
    QString migrationError;
    if (!DatabaseMigrations::migrate(db, 0, &migrationError))
        return fail(migrationError);
    db.transaction();
    // Every database is born with its universal id (see GameIdentity).
    query.prepare(QStringLiteral("INSERT INTO properties (key, value) VALUES ('id', ?)"));
    query.addBindValue(GameIdentity::newLineageId());
    if (!query.exec())
        return fail(query.lastError().text());

    GameInserter inserter(db);
    for (const GameRecord &game : games) {
        if (inserter.insert(game) == 0)
            return fail(inserter.errorText());
    }
    if (!db.commit())
        return fail(db.lastError().text());

    if (!database->loadBriefs(errorMessage) || !database->loadProperties(errorMessage))
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
        || query.value(0).toInt() > DatabaseMigrations::latestVersion()) {
        setError(errorMessage, QObject::tr("“%1” was created by a newer version of Pragma Chess: "
                                           "update Pragma Chess to open it.")
                                   .arg(info.fileName()));
        return nullptr;
    }
    const int version = query.value(0).toInt();
    query.finish();

    // An older file runs, in place, the migrations it is missing.
    QString migrationError;
    if (!DatabaseMigrations::migrate(db, version, &migrationError)) {
        setError(errorMessage, QObject::tr("Could not upgrade “%1”: %2").arg(info.fileName(), migrationError));
        return nullptr;
    }

    if (!database->loadBriefs(errorMessage) || !database->loadPlayerRoles(errorMessage)
        || !database->loadProperties(errorMessage))
        return nullptr;
    return database;
}

DatabaseProperties SqliteGameDatabase::readProperties(const QString &path)
{
    if (!QFileInfo(path).isFile())
        return {};
    const QString connection = QUuid::createUuid().toString(QUuid::WithoutBraces);
    QHash<QString, QString> values;
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connection);
        db.setDatabaseName(path);
        db.setConnectOptions(QStringLiteral("QSQLITE_OPEN_READONLY"));
        if (db.open()) {
            QSqlQuery query(db);
            if (query.exec(QStringLiteral("PRAGMA application_id")) && query.next()
                && query.value(0).toInt() == kApplicationId
                && query.exec(QStringLiteral("SELECT key, value FROM properties"))) {
                while (query.next())
                    values.insert(query.value(0).toString(), query.value(1).toString());
            }
        }
    }
    QSqlDatabase::removeDatabase(connection);
    return DatabaseProperties::fromValues(values);
}

QHash<QString, QString> SqliteGameDatabase::readRevisions(const QString &path)
{
    QHash<QString, QString> revisions;
    if (!QFileInfo(path).isFile())
        return revisions;
    const QString connection = QUuid::createUuid().toString(QUuid::WithoutBraces);
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connection);
        db.setDatabaseName(path);
        db.setConnectOptions(QStringLiteral("QSQLITE_OPEN_READONLY"));
        if (db.open()) {
            QSqlQuery query(db);
            query.setForwardOnly(true);
            if (query.exec(QStringLiteral("PRAGMA application_id")) && query.next()
                && query.value(0).toInt() == kApplicationId
                && query.exec(QStringLiteral("SELECT uid, modified FROM games"))) {
                while (query.next())
                    revisions.insert(query.value(0).toString(), query.value(1).toString());
            }
        }
    }
    QSqlDatabase::removeDatabase(connection);
    return revisions;
}

QByteArray SqliteGameDatabase::fileStamp(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    const QByteArray head = file.read(100);
    if (head.size() < 100 || !head.startsWith("SQLite format 3"))
        return {};
    // Bytes 24 to 27: the file change counter, big-endian.
    const quint32 counter = quint32(uchar(head[24])) << 24 | quint32(uchar(head[25])) << 16
                            | quint32(uchar(head[26])) << 8 | quint32(uchar(head[27]));
    return QByteArrayLiteral("sqlite-change ") + QByteArray::number(counter) + QByteArrayLiteral(" size ")
         + QByteArray::number(file.size());
}

QList<GameLine> SqliteGameDatabase::readGameLines(const QString &path)
{
    QList<GameLine> lines;
    const QString connection = QUuid::createUuid().toString(QUuid::WithoutBraces);
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connection);
        db.setDatabaseName(path);
        db.setConnectOptions(QStringLiteral("QSQLITE_OPEN_READONLY"));
        if (db.open()) {
            QSqlQuery query(db);
            query.setForwardOnly(true);
            // Only the games in the lists: the trash is not searched.
            query.prepare(QStringLiteral("SELECT g.id, g.start_fen, g.moves_uci, g.result FROM games g"
                                         " LEFT JOIN game_states st ON st.uid = g.uid"
                                         " WHERE g.id > ? AND (st.state IS NULL OR st.state = 'live')"
                                         " ORDER BY g.id LIMIT 50000"));
            for (qint64 after = -1;;) {
                query.addBindValue(after);
                if (!query.exec())
                    break;
                qsizetype read = 0;
                while (query.next()) {
                    lines << GameLine{query.value(0).toLongLong(), query.value(1).toString(),
                                      query.value(2).toString(), query.value(3).toString()};
                    ++read;
                }
                query.finish();
                if (read == 0)
                    break;
                after = lines.constLast().id;
            }
        }
    }
    QSqlDatabase::removeDatabase(connection);
    return lines;
}

bool SqliteGameDatabase::adoptLineage(const QString &path, const QString &id)
{
    if (!readProperties(path).id.isEmpty())
        return true;
    QString error;
    const std::unique_ptr<SqliteGameDatabase> database = open(path, &error);
    if (!database)
        return false;
    DatabaseProperties properties = database->properties();
    if (!properties.id.isEmpty())
        return true;
    properties.id = id;
    return database->setProperties(properties, &error);
}

bool SqliteGameDatabase::loadProperties(QString *errorMessage)
{
    QSqlQuery query(QSqlDatabase::database(m_connectionName));
    if (!query.exec(QStringLiteral("SELECT key, value FROM properties"))) {
        setError(errorMessage, query.lastError().text());
        return false;
    }
    QHash<QString, QString> values;
    while (query.next())
        values.insert(query.value(0).toString(), query.value(1).toString());
    m_properties = DatabaseProperties::fromValues(values);
    // Written before the two names were told apart: the file is set right.
    if (m_properties.isDistributed() && values.contains(QStringLiteral("name"))
        && !values.contains(QStringLiteral("name.en")))
        setProperties(m_properties, nullptr);
    return true;
}

bool SqliteGameDatabase::setProperties(const DatabaseProperties &properties, QString *errorMessage)
{
    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    db.transaction();
    QSqlQuery query(db);
    // A distributed database's name may be taken away again (its own comes back).
    if (properties.isDistributed() && properties.name.isEmpty()
        && !query.exec(QStringLiteral("DELETE FROM properties WHERE key = 'name'"))) {
        setError(errorMessage, query.lastError().text());
        db.rollback();
        return false;
    }
    query.prepare(QStringLiteral("INSERT OR REPLACE INTO properties (key, value) VALUES (?, ?)"));
    const QHash<QString, QString> values = properties.values();
    for (auto it = values.cbegin(); it != values.cend(); ++it) {
        query.addBindValue(it.key());
        query.addBindValue(it.value().isNull() ? QStringLiteral("") : it.value()); // NOT NULL
        if (!query.exec()) {
            setError(errorMessage, query.lastError().text());
            db.rollback();
            return false;
        }
    }
    if (!db.commit()) {
        setError(errorMessage, db.lastError().text());
        db.rollback();
        return false;
    }
    m_properties = properties;
    return true;
}

namespace {

/// What the briefs read of every game, in the order of the index that
/// holds them all (kBriefIndex): read from the index alone, the scan never
/// touches the games' rows, whose moves come before their tags. Of the tags,
/// SQL cuts out the time control; the whole text only for the games of a
/// study or a puzzle. The same text in the index and in the query, or SQLite
/// does not use it.
const QString &briefColumns()
{
    static const QString columns = [] {
        const QString timeControl = QStringLiteral("instr(tags, '[TimeControl \"')");
        return QStringLiteral("id, white_id, black_id, event_id, date, result, eco, white_elo, black_elo, ply_count,"
                              " start_fen, (CASE WHEN %1 > 0 THEN substr(tags, %1 + 14, instr(substr(tags, %1 + 14), '\"') - 1) END),"
                              " (CASE WHEN instr(tags, '[Study') > 0 OR instr(tags, '[Chapter') > 0 OR instr(tags, '[Themes') > 0"
                              " THEN tags END)")
            .arg(timeControl);
    }();
    return columns;
}

/// Not part of the schema: an index changes nothing for whoever reads the
/// file (older versions, the phone), so it is made when a database is
/// opened, if missing, and the schema's version stays.
bool ensureBriefIndex(QSqlDatabase db)
{
    QSqlQuery query(db);
    return query.exec(QStringLiteral("CREATE INDEX IF NOT EXISTS games_brief ON games(%1)").arg(briefColumns()));
}

/// Games of a page: read together when one of them is shown.
constexpr qint64 kPageRows = 256;
/// Pages kept: what a few screens of a list need, whatever the database's size.
constexpr qsizetype kPagesKept = 64;

QString previewFromSan(const QString &startFen, const QString &san, int plyCount)
{
    // The beginning of the moves, for the lists: the last one may be cut short.
    QStringList moves = san.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    if (san.size() == kPreviewChars && !moves.isEmpty())
        moves.removeLast();
    return Pgn::preview(startFen, moves.first(qMin(moves.size(), qsizetype(kPreviewPlies))), plyCount);
}

} // namespace

int SqliteGameDatabase::intern(const QString &text)
{
    if (text.isEmpty())
        return 0;
    const auto found = m_stringIndex.constFind(text);
    if (found != m_stringIndex.constEnd())
        return *found;
    m_strings << text;
    m_stringIndex.insert(text, int(m_strings.size() - 1));
    return int(m_strings.size() - 1);
}

int SqliteGameDatabase::internTags(const QList<PgnTag> &tags)
{
    QList<PgnTag> brief;
    for (const PgnTag &tag : tags) {
        if (isBriefTag(tag.name))
            brief << tag;
    }
    if (brief.isEmpty())
        return 0;
    const QString key = Pgn::tagsText(brief);
    const auto found = m_tagSetIndex.constFind(key);
    if (found != m_tagSetIndex.constEnd())
        return *found;
    m_tagSets << brief;
    m_tagSetIndex.insert(key, int(m_tagSets.size() - 1));
    return int(m_tagSets.size() - 1);
}

SqliteGameDatabase::Brief SqliteGameDatabase::briefOf(const GameRecord &header)
{
    Brief brief;
    brief.id = header.id;
    brief.white = intern(header.white);
    brief.black = intern(header.black);
    brief.event = intern(header.event);
    brief.date = intern(header.date);
    brief.result = intern(header.result);
    brief.eco = intern(header.eco);
    brief.startFen = intern(header.startFen);
    brief.stateModified = intern(header.stateModified);
    brief.tags = internTags(header.tags);
    brief.whiteElo = header.whiteElo;
    brief.blackElo = header.blackElo;
    brief.plyCount = header.plyCount;
    brief.state = header.state;
    return brief;
}

GameRecord SqliteGameDatabase::fromBrief(const Brief &brief) const
{
    GameRecord game;
    game.id = brief.id;
    game.white = m_strings.at(brief.white);
    game.black = m_strings.at(brief.black);
    game.event = m_strings.at(brief.event);
    game.date = m_strings.at(brief.date);
    game.result = m_strings.at(brief.result);
    game.eco = m_strings.at(brief.eco);
    game.startFen = m_strings.at(brief.startFen);
    game.stateModified = m_strings.at(brief.stateModified);
    game.tags = m_tagSets.at(brief.tags);
    game.whiteElo = brief.whiteElo;
    game.blackElo = brief.blackElo;
    game.plyCount = brief.plyCount;
    game.state = brief.state;
    return game;
}

GameRecord SqliteGameDatabase::brief(qint64 index) const
{
    return fromBrief(m_rows.at(index));
}

GameRecord SqliteGameDatabase::header(qint64 index) const
{
    GameRecord game = fromBrief(m_rows.at(index));
    const Details &more = details(index);
    game.site = more.site;
    game.round = more.round;
    game.uid = more.uid;
    game.modified = more.modified;
    game.linePreview = more.linePreview;
    game.tags = more.tags;
    return game;
}

const SqliteGameDatabase::Details &SqliteGameDatabase::details(qint64 index) const
{
    const qint64 page = index / kPageRows;
    if (auto found = m_pages.find(page); found != m_pages.end()) {
        if (m_pageOrder.constLast() != page) {
            m_pageOrder.removeOne(page);
            m_pageOrder << page;
        }
        return found->at(index - page * kPageRows);
    }
    const qint64 first = page * kPageRows;
    const qint64 last = qMin(first + kPageRows, qint64(m_rows.size())) - 1;
    QList<Details> rows(last - first + 1);
    QSqlQuery query(QSqlDatabase::database(m_connectionName));
    query.setForwardOnly(true);
    query.prepare(QStringLiteral("SELECT g.id, s.name, g.round, g.uid, g.modified, substr(g.moves_san, 1, %1), g.tags"
                                 " FROM games g LEFT JOIN sites s ON s.id = g.site_id"
                                 " WHERE g.id BETWEEN ? AND ? ORDER BY g.id")
                      .arg(kPreviewChars));
    query.addBindValue(m_rows.at(first).id);
    query.addBindValue(m_rows.at(last).id);
    if (query.exec()) {
        // The rows are in the order of their ids, as the page's games.
        qint64 row = first;
        while (query.next() && row <= last) {
            const qint64 id = query.value(0).toLongLong();
            while (row <= last && m_rows.at(row).id < id)
                ++row;
            if (row > last || m_rows.at(row).id != id)
                continue;
            const Brief &brief = m_rows.at(row);
            Details &more = rows[row - first];
            more.site = query.value(1).toString();
            more.round = query.value(2).toString();
            more.uid = query.value(3).toString();
            more.modified = query.value(4).toString();
            more.linePreview = previewFromSan(m_strings.at(brief.startFen), query.value(5).toString(), brief.plyCount);
            // The tags with no column: which study and chapter a game is, for the tree.
            more.tags = Pgn::tagsFromText(query.value(6).toString());
            ++row;
        }
    }
    m_pages.insert(page, rows);
    m_pageOrder << page;
    while (m_pageOrder.size() > kPagesKept)
        m_pages.remove(m_pageOrder.takeFirst());
    return m_pages.find(page)->at(index - first);
}

void SqliteGameDatabase::dropPage(qint64 index) const
{
    const qint64 page = index / kPageRows;
    if (m_pages.remove(page))
        m_pageOrder.removeOne(page);
}

void SqliteGameDatabase::dropPages() const
{
    m_pages.clear();
    m_pageOrder.clear();
}

qint64 SqliteGameDatabase::indexOfId(qint64 id) const
{
    // The rows are in the order of their ids.
    const auto found = std::lower_bound(m_rows.cbegin(), m_rows.cend(), id,
                                        [](const Brief &brief, qint64 value) { return brief.id < value; });
    return found != m_rows.cend() && found->id == id ? qint64(found - m_rows.cbegin()) : -1;
}

qint64 SqliteGameDatabase::indexOfUid(const QString &uid) const
{
    if (uid.isEmpty())
        return -1;
    QSqlQuery query(QSqlDatabase::database(m_connectionName));
    query.prepare(QStringLiteral("SELECT id FROM games WHERE uid = ?"));
    query.addBindValue(uid);
    return query.exec() && query.next() ? indexOfId(query.value(0).toLongLong()) : -1;
}

bool SqliteGameDatabase::loadBriefs(QString *errorMessage)
{
    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    m_rows.clear();
    m_strings = {QString()};
    m_stringIndex.clear();
    m_tagSets = {QList<PgnTag>()};
    m_tagSetIndex.clear();
    dropPages();

    // Players and events by id, each name once.
    QHash<qint64, int> players;
    QHash<qint64, int> events;
    QSqlQuery names(db);
    names.setForwardOnly(true);
    for (const auto &[table, ids] : {std::pair<const char *, QHash<qint64, int> *>{"players", &players},
                                     std::pair<const char *, QHash<qint64, int> *>{"events", &events}}) {
        if (!names.exec(QStringLiteral("SELECT id, name FROM %1").arg(QLatin1String(table)))) {
            setError(errorMessage, names.lastError().text());
            return false;
        }
        while (names.next())
            ids->insert(names.value(0).toLongLong(), intern(names.value(1).toString()));
    }

    // A file that cannot be written (read only) is read without the index.
    ensureBriefIndex(db);
    QSqlQuery query(db);
    query.setForwardOnly(true);
    if (!query.exec(QStringLiteral("SELECT %1 FROM games ORDER BY id").arg(briefColumns()))) {
        setError(errorMessage, query.lastError().text());
        return false;
    }
    QHash<QString, int> timeControls;
    while (query.next()) {
        Brief brief;
        brief.id = query.value(0).toLongLong();
        brief.white = players.value(query.value(1).toLongLong());
        brief.black = players.value(query.value(2).toLongLong());
        brief.event = events.value(query.value(3).toLongLong());
        brief.date = intern(query.value(4).toString());
        brief.result = intern(query.value(5).toString());
        brief.eco = intern(query.value(6).toString());
        brief.whiteElo = query.value(7).toInt();
        brief.blackElo = query.value(8).toInt();
        brief.plyCount = query.value(9).toInt();
        brief.startFen = intern(query.value(10).toString());
        if (const QString tags = query.value(12).toString(); !tags.isEmpty()) {
            brief.tags = internTags(Pgn::tagsFromText(tags));
        } else if (const QString timeControl = query.value(11).toString(); !timeControl.isEmpty()) {
            auto found = timeControls.constFind(timeControl);
            if (found == timeControls.constEnd())
                found = timeControls.insert(timeControl, internTags({{QStringLiteral("TimeControl"), timeControl}}));
            brief.tags = *found;
        }
        m_rows.push_back(brief);
    }
    query.finish();
    // Where the games are that are not live: the few the trash knows.
    if (!query.exec(QStringLiteral("SELECT g.id, st.state, st.modified FROM game_states st JOIN games g ON g.uid = st.uid"))) {
        setError(errorMessage, query.lastError().text());
        return false;
    }
    while (query.next()) {
        const qint64 index = indexOfId(query.value(0).toLongLong());
        if (index < 0)
            continue;
        Brief &brief = m_rows[index];
        brief.state = gameStateFromKey(query.value(1).toString());
        // A purged game whose row is still here (it came back from a copy
        // that had it) waits, hidden, for the next optimize().
        if (brief.state == GameState::Purged)
            brief.state = GameState::Deleted;
        brief.stateModified = intern(query.value(2).toString());
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
    if (index < 0 || index >= gameCount())
        return std::nullopt;

    GameRecord game = header(index);
    QSqlQuery query(QSqlDatabase::database(m_connectionName));
    query.prepare(QStringLiteral("SELECT moves_san, moves_uci, variations, tags, comments FROM games WHERE id = ?"));
    query.addBindValue(game.id);
    if (!query.exec() || !query.next())
        return std::nullopt;
    game.variations = GameVariations::fromText(query.value(2).toString()); // UCI filled by GameSession.

    const QStringList san = query.value(0).toString().split(QLatin1Char(' '), Qt::SkipEmptyParts);
    const QStringList uci = query.value(1).toString().split(QLatin1Char(' '), Qt::SkipEmptyParts);
    for (qsizetype i = 0; i < qMin(san.size(), uci.size()); ++i) {
        // Annotations travel glued to the SAN ("Nf3!$16"), so they need no column.
        QList<int> nags;
        const QString bare = MoveAnnotation::split(san.at(i), &nags);
        game.moves << MoveRecord{bare, uci.at(i), nags, {}};
    }
    game.tags = Pgn::tagsFromText(query.value(3).toString());
    // After the moves and the variations: the comments go on them.
    MoveComment::fromJson(game, query.value(4).toString());
    return game;
}

QList<GameLine> SqliteGameDatabase::gameLines() const
{
    QList<GameLine> lines;
    lines.reserve(qsizetype(m_rows.size()));
    QSqlQuery query(QSqlDatabase::database(m_connectionName));
    query.setForwardOnly(true);
    // Only the games in the lists: the trash is not searched.
    if (!query.exec(QStringLiteral("SELECT g.id, g.start_fen, g.moves_uci, g.result FROM games g"
                                   " LEFT JOIN game_states st ON st.uid = g.uid"
                                   " WHERE st.state IS NULL OR st.state = 'live' ORDER BY g.id")))
        return lines;
    while (query.next())
        lines << GameLine{query.value(0).toLongLong(), query.value(1).toString(), query.value(2).toString(),
                          query.value(3).toString()};
    return lines;
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
    m_rows.push_back(briefOf(inserter.header(game, id)));
    dropPage(gameCount() - 1); // A page read before held one game less.
    return gameCount() - 1;
}

bool SqliteGameDatabase::updateHeader(qint64 index, const GameRecord &header, QString *errorMessage)
{
    if (index < 0 || index >= gameCount()) {
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
        " round = ?, result = ?, white_elo = ?, black_elo = ?, eco = ?, tags = ?, modified = ? WHERE id = ?"));
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
    update.addBindValue(Pgn::tagsText(header.tags)); // The time control among them.
    const QString modified = GameIdentity::now();
    update.addBindValue(modified);
    update.addBindValue(m_rows.at(index).id);
    if (!update.exec() || !db.commit()) {
        setError(errorMessage, update.lastError().isValid() ? update.lastError().text() : db.lastError().text());
        db.rollback();
        return false;
    }

    GameRecord cached = this->header(index);
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
    cached.tags = header.tags;
    cached.modified = modified;
    m_rows[index] = briefOf(cached);
    dropPage(index);
    return true;
}

bool SqliteGameDatabase::replaceGame(qint64 index, const GameRecord &game, QString *errorMessage)
{
    if (index < 0 || index >= gameCount()) {
        setError(errorMessage, QObject::tr("The game does not exist."));
        return false;
    }

    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    db.transaction();
    NameTable players(db, QStringLiteral("players"));
    NameTable events(db, QStringLiteral("events"));
    NameTable sites(db, QStringLiteral("sites"));
    QStringList san;
    QStringList uci;
    for (const MoveRecord &move : game.moves) {
        san << move.san + MoveAnnotation::storedSuffix(move.nags);
        uci << move.uci;
    }
    const QString modified = game.modified.isEmpty() ? GameIdentity::now() : game.modified;

    QSqlQuery update(db);
    update.prepare(QStringLiteral(
        "UPDATE games SET white_id = ?, black_id = ?, event_id = ?, site_id = ?, date = ?, round = ?,"
        " result = ?, white_elo = ?, black_elo = ?, eco = ?, ply_count = ?, start_fen = ?, moves_san = ?,"
        " moves_uci = ?, variations = ?, tags = ?, comments = ?, modified = ? WHERE id = ?"));
    update.addBindValue(players.idFor(game.white));
    update.addBindValue(players.idFor(game.black));
    update.addBindValue(events.idFor(game.event));
    update.addBindValue(sites.idFor(game.site));
    update.addBindValue(nullIfEmpty(game.date));
    update.addBindValue(nullIfEmpty(game.round));
    update.addBindValue(nullIfEmpty(game.result));
    update.addBindValue(nullIfZero(game.whiteElo));
    update.addBindValue(nullIfZero(game.blackElo));
    update.addBindValue(nullIfEmpty(game.eco));
    update.addBindValue(int(game.moves.size()));
    update.addBindValue(nullIfEmpty(game.startFen));
    update.addBindValue(movesText(san));
    update.addBindValue(movesText(uci));
    update.addBindValue(GameVariations::toText(game.variations));
    update.addBindValue(Pgn::tagsText(game.tags));
    update.addBindValue(MoveComment::toJson(game));
    update.addBindValue(modified);
    update.addBindValue(m_rows.at(index).id);
    if (!update.exec() || !db.commit()) {
        setError(errorMessage, update.lastError().isValid() ? update.lastError().text() : db.lastError().text());
        db.rollback();
        return false;
    }

    GameRecord cached = header(index);
    const qint64 id = cached.id;
    const QString uid = cached.uid;
    const GameState state = cached.state;
    const QString stateModified = cached.stateModified;
    cached = game;
    cached.id = id;
    cached.uid = uid; // The identity never changes.
    cached.state = state; // Where the game is has its own revision (game_states).
    cached.stateModified = stateModified;
    cached.modified = modified;
    cached.plyCount = int(game.moves.size());
    cached.moves.clear();
    m_rows[index] = briefOf(cached);
    dropPage(index);
    return true;
}

QList<GameSource> SqliteGameDatabase::sources() const
{
    QList<GameSource> result;
    QSqlQuery query(QSqlDatabase::database(m_connectionName));
    if (!query.exec(QStringLiteral(
            "SELECT s.id, s.uuid, s.kind, s.account, s.settings, s.state, s.enabled, s.created_at,"
            " s.last_sync_at, s.last_error,"
            " (SELECT COUNT(*) FROM game_sources gs JOIN games g ON g.id = gs.game_id"
            "  LEFT JOIN game_states st ON st.uid = g.uid"
            "  WHERE gs.source_id = s.id AND (st.state IS NULL OR st.state = 'live'))"
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
        added << inserter.header(imported.game, id);
    }
    if (!db.commit()) {
        setError(errorMessage, db.lastError().text());
        db.rollback();
        return -1;
    }
    for (const GameRecord &header : std::as_const(added))
        m_rows.push_back(briefOf(header));
    dropPages();
    return int(added.size());
}

QList<SourceLink> SqliteGameDatabase::sourceLinks() const
{
    QList<SourceLink> links;
    QSqlQuery query(QSqlDatabase::database(m_connectionName));
    query.setForwardOnly(true);
    if (query.exec(QStringLiteral(
            "SELECT s.uuid, gs.external_id, g.uid FROM game_sources gs"
            " JOIN sources s ON s.id = gs.source_id LEFT JOIN games g ON g.id = gs.game_id"))) {
        while (query.next())
            links << SourceLink{query.value(0).toString(), query.value(1).toString(), query.value(2).toString()};
    }
    return links;
}

bool SqliteGameDatabase::mergeSourceLinks(const QList<SourceLink> &incoming, QString *errorMessage)
{
    QHash<QString, qint64> sourceIds;
    for (const GameSource &source : sources())
        sourceIds.insert(source.uuid, source.id);

    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    db.transaction();
    QSqlQuery insert(db);
    // A game this copy does not hold (purged here) gets 0, as optimize() leaves it.
    insert.prepare(QStringLiteral(
        "INSERT OR IGNORE INTO game_sources (game_id, source_id, external_id)"
        " VALUES (COALESCE((SELECT id FROM games WHERE uid = ?), 0), ?, ?)"));
    for (const SourceLink &link : incoming) {
        const auto source = sourceIds.constFind(link.sourceUuid);
        if (source == sourceIds.constEnd())
            continue;
        insert.addBindValue(link.gameUid);
        insert.addBindValue(*source);
        insert.addBindValue(link.externalId);
        if (!insert.exec()) {
            setError(errorMessage, insert.lastError().text());
            db.rollback();
            return false;
        }
    }
    if (!db.commit()) {
        setError(errorMessage, db.lastError().text());
        db.rollback();
        return false;
    }
    return true;
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

bool SqliteGameDatabase::setGameState(qint64 index, GameState state, QString *errorMessage)
{
    if (index < 0 || index >= gameCount() || state == GameState::Purged) {
        setError(errorMessage, QObject::tr("The game does not exist."));
        return false;
    }
    QSqlQuery query(QSqlDatabase::database(m_connectionName));
    query.prepare(QStringLiteral("INSERT OR REPLACE INTO game_states (uid, state, modified) VALUES (?, ?, ?)"));
    query.addBindValue(details(index).uid);
    query.addBindValue(gameStateKey(state));
    const QString now = GameIdentity::now();
    query.addBindValue(now);
    if (!query.exec()) {
        setError(errorMessage, query.lastError().text());
        return false;
    }
    m_rows[index].state = state;
    m_rows[index].stateModified = intern(now);
    return true;
}

QList<GameStateRecord> SqliteGameDatabase::gameStates() const
{
    QList<GameStateRecord> states;
    QSqlQuery query(QSqlDatabase::database(m_connectionName));
    query.setForwardOnly(true);
    if (!query.exec(QStringLiteral("SELECT uid, state, modified FROM game_states")))
        return states;
    while (query.next())
        states << GameStateRecord{query.value(0).toString(), gameStateFromKey(query.value(1).toString()),
                                  query.value(2).toString()};
    return states;
}

namespace {

/// Removes the rows of the games chosen by `where` (a condition on `games g`
/// and its state `st`). What a source imported stays known, by external id,
/// so the source does not import it again; it no longer points to a game.
bool removeGames(QSqlDatabase db, const QString &where, int *removed, QString *error)
{
    const QString ids = QStringLiteral("SELECT g.id FROM games g LEFT JOIN game_states st ON st.uid = g.uid WHERE ")
                        + where;
    QSqlQuery query(db);
    if (!query.exec(QStringLiteral("UPDATE game_sources SET game_id = 0 WHERE game_id IN (%1)").arg(ids))
        || !query.exec(QStringLiteral("DELETE FROM games WHERE id IN (%1)").arg(ids))) {
        *error = query.lastError().text();
        return false;
    }
    if (removed)
        *removed = query.numRowsAffected();
    return true;
}

} // namespace

bool SqliteGameDatabase::mergeGameStates(const QList<GameStateRecord> &incoming, QString *errorMessage)
{
    const QList<GameStateRecord> changes = GameStates::incomingChanges(gameStates(), incoming);
    if (changes.isEmpty())
        return true;

    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    db.transaction();
    QSqlQuery store(db);
    store.prepare(QStringLiteral("INSERT OR REPLACE INTO game_states (uid, state, modified) VALUES (?, ?, ?)"));
    QString error;
    bool done = true;
    for (const GameStateRecord &record : changes) {
        store.addBindValue(record.uid);
        store.addBindValue(gameStateKey(record.state));
        store.addBindValue(record.modified);
        if (!store.exec()) {
            error = store.lastError().text();
            done = false;
            break;
        }
    }
    // A game another copy purged goes here too: that is what purging means.
    done = done && removeGames(db, QStringLiteral("st.state = 'purged'"), nullptr, &error);
    if (!done || !db.commit()) {
        setError(errorMessage, error.isEmpty() ? db.lastError().text() : error);
        db.rollback();
        return false;
    }
    return loadBriefs(errorMessage);
}

int SqliteGameDatabase::optimize(QString *errorMessage)
{
    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    db.transaction();
    QSqlQuery query(db);
    QString error;
    int removed = 0;
    bool done = removeGames(db, QStringLiteral("st.state IN ('deleted', 'purged')"), &removed, &error);
    if (done) {
        // The state outlives the game, with the time of the purge: newer than
        // the deletion every other copy knows about.
        query.prepare(QStringLiteral("UPDATE game_states SET state = 'purged', modified = ? WHERE state = 'deleted'"));
        query.addBindValue(GameIdentity::now());
        done = query.exec();
    }
    // Names no game uses any more (players the user said who they are stay).
    done = done
           && query.exec(QStringLiteral(
                  "DELETE FROM players WHERE id NOT IN (SELECT white_id FROM games WHERE white_id IS NOT NULL)"
                  " AND id NOT IN (SELECT black_id FROM games WHERE black_id IS NOT NULL)"
                  " AND id NOT IN (SELECT player_id FROM player_roles)"))
           && query.exec(QStringLiteral(
                  "DELETE FROM events WHERE id NOT IN (SELECT event_id FROM games WHERE event_id IS NOT NULL)"))
           && query.exec(QStringLiteral(
                  "DELETE FROM sites WHERE id NOT IN (SELECT site_id FROM games WHERE site_id IS NOT NULL)"));
    if (!done || !db.commit()) {
        if (error.isEmpty())
            error = query.lastError().isValid() ? query.lastError().text() : db.lastError().text();
        setError(errorMessage, error);
        db.rollback();
        return -1;
    }
    // Gives the space back to the file system; it cannot run in a transaction.
    query.finish();
    if (!query.exec(QStringLiteral("VACUUM"))) {
        setError(errorMessage, query.lastError().text());
        return -1;
    }
    if (!loadBriefs(errorMessage))
        return -1;
    return removed;
}

struct SqliteGameWriter::Private {
    QString connectionName;
    std::unique_ptr<GameInserter> inserter;
};

SqliteGameWriter::~SqliteGameWriter()
{
    if (!d)
        return;
    d->inserter.reset();
    {
        QSqlDatabase db = QSqlDatabase::database(d->connectionName, false);
        db.close();
    }
    QSqlDatabase::removeDatabase(d->connectionName);
}

std::unique_ptr<SqliteGameWriter> SqliteGameWriter::create(const QString &path, QString *errorMessage)
{
    // The schema and the lineage as every new database has them.
    if (!SqliteGameDatabase::create(path, {}, errorMessage))
        return nullptr;
    std::unique_ptr<SqliteGameWriter> writer(new SqliteGameWriter);
    writer->d = std::make_unique<Private>();
    writer->d->connectionName = QUuid::createUuid().toString(QUuid::WithoutBraces);
    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), writer->d->connectionName);
    db.setDatabaseName(QFileInfo(path).absoluteFilePath());
    if (!db.open()) {
        setError(errorMessage, db.lastError().text());
        return nullptr;
    }
    QSqlQuery query(db);
    query.exec(QStringLiteral("PRAGMA journal_mode = OFF"));
    query.exec(QStringLiteral("PRAGMA synchronous = OFF"));
    query.exec(QStringLiteral("PRAGMA cache_size = -65536")); // 64 MB.
    // Made once at the end: faster than kept up to date game by game.
    query.exec(QStringLiteral("DROP INDEX IF EXISTS games_brief"));
    writer->d->inserter = std::make_unique<GameInserter>(db);
    return writer;
}

bool SqliteGameWriter::add(const QList<GameRecord> &games, QString *errorMessage)
{
    QSqlDatabase db = QSqlDatabase::database(d->connectionName, false);
    db.transaction();
    for (const GameRecord &game : games) {
        if (d->inserter->insert(game) == 0) {
            setError(errorMessage, d->inserter->errorText());
            db.rollback();
            return false;
        }
    }
    if (!db.commit()) {
        setError(errorMessage, db.lastError().text());
        db.rollback();
        return false;
    }
    return true;
}

bool SqliteGameWriter::finish(QString *errorMessage)
{
    QSqlDatabase db = QSqlDatabase::database(d->connectionName, false);
    QSqlQuery query(db);
    // The journal as every database has it.
    if (!ensureBriefIndex(db) || !query.exec(QStringLiteral("PRAGMA journal_mode = DELETE"))) {
        setError(errorMessage, query.lastError().text());
        return false;
    }
    return true;
}

