#pragma once

#include "GameDatabase.h"

#include <QHash>
#include <QList>

#include <memory>
#include <vector>

/// A `.pdb` database file: an SQLite database tagged with Pragma's
/// application id and a schema version.
///
/// Interim implementation living in the GUI; the schema is meant to be owned
/// by the chess database engine once it is connected.
class SqliteGameDatabase final : public GameDatabase {
public:
    ~SqliteGameDatabase() override;

    /// Creates a new database file (which must not exist) holding `games`.
    static std::unique_ptr<SqliteGameDatabase> create(const QString &path,
                                                      const QList<GameRecord> &games,
                                                      QString *errorMessage);
    static std::unique_ptr<SqliteGameDatabase> open(const QString &path, QString *errorMessage);
    /// The properties of a database file without loading its games (defaults
    /// for a file that is not a readable Pragma Chess database).
    static DatabaseProperties readProperties(const QString &path);
    /// Every game of a database file as uid → modified, without loading the
    /// games: two files with the same revisions hold the same games. Empty for
    /// a file that is not a readable version 5 database.
    static QHash<QString, QString> readRevisions(const QString &path);
    /// Gives the database at `path` the universal id `id` if it has none yet
    /// (the databases we ship, seeded before they carried one). False if the
    /// file cannot be opened or written.
    static bool adoptLineage(const QString &path, const QString &id);
    /// gameLines() of the database file at `path`, read on any thread with a
    /// connection of its own, a chunk at a time (each a short read, so the
    /// window can write meanwhile). Empty if the file cannot be read.
    static QList<GameLine> readGameLines(const QString &path);
    /// What the file at `path` is now, as SQLite counts its writes (the file
    /// change counter of its header, moved by every write) and its size: an
    /// index made from the file holds while the stamp is the same.
    static QByteArray fileStamp(const QString &path);

    QString name() const override;
    QString location() const override { return m_path; }
    qint64 gameCount() const override { return qint64(m_rows.size()); }
    GameRecord header(qint64 index) const override;
    GameRecord brief(qint64 index) const override;
    GameState stateOf(qint64 index) const override { return m_rows.at(index).state; }
    qint64 indexOfId(qint64 id) const override;
    qint64 indexOfUid(const QString &uid) const override;
    std::optional<GameRecord> loadGame(qint64 index) const override;
    QList<GameLine> gameLines() const override;
    qint64 addGame(const GameRecord &game, QString *errorMessage) override;
    bool updateHeader(qint64 index, const GameRecord &header, QString *errorMessage) override;
    bool replaceGame(qint64 index, const GameRecord &game, QString *errorMessage) override;
    PlayerRoles playerRoles() const override { return m_roles; }
    bool setPlayerRole(const QString &player, PlayerRole role, QString *errorMessage) override;
    QList<GameSource> sources() const override;
    bool addSource(GameSource &source, QString *errorMessage) override;
    bool updateSource(const GameSource &source, QString *errorMessage) override;
    bool removeSource(qint64 sourceId, QString *errorMessage) override;
    QSet<qint64> sourceGameIds(qint64 sourceId) const override;
    int importGames(qint64 sourceId, const QList<ImportedGame> &games, QString *errorMessage) override;
    QList<SourceLink> sourceLinks() const override;
    bool mergeSourceLinks(const QList<SourceLink> &incoming, QString *errorMessage) override;
    DatabaseProperties properties() const override { return m_properties; }
    bool setProperties(const DatabaseProperties &properties, QString *errorMessage) override;
    bool setGameState(qint64 index, GameState state, QString *errorMessage) override;
    QList<GameStateRecord> gameStates() const override;
    bool mergeGameStates(const QList<GameStateRecord> &incoming, QString *errorMessage) override;
    int optimize(QString *errorMessage) override;
    bool isModified() const override { return false; }
    bool saveCopy(const QString &path, QString *errorMessage) const override;

private:
    SqliteGameDatabase(QString path, QString connectionName);

    bool loadBriefs(QString *errorMessage);
    bool loadPlayerRoles(QString *errorMessage);
    bool loadProperties(QString *errorMessage);

    QString m_path;
    QString m_connectionName;
    /// What every game keeps in memory, a few dozen bytes whatever its size:
    /// indexes into m_strings and m_tagSets, and numbers. The rest of a
    /// header is read from the file a page at a time, when it is shown.
    struct Brief {
        qint64 id = 0;
        int white = 0;
        int black = 0;
        int event = 0;
        int date = 0;
        int result = 0;
        int eco = 0;
        int startFen = 0;
        int stateModified = 0;
        int tags = 0;
        int whiteElo = 0;
        int blackElo = 0;
        int plyCount = 0;
        GameState state = GameState::Live;
    };
    /// What only showing a game needs, read with its page.
    struct Details {
        QString site;
        QString round;
        QString uid;
        QString modified;
        QString linePreview;
        QList<PgnTag> tags;
    };

    int intern(const QString &text);
    int internTags(const QList<PgnTag> &tags);
    Brief briefOf(const GameRecord &header);
    GameRecord fromBrief(const Brief &brief) const;
    const Details &details(qint64 index) const;
    /// Forgets the cached page holding `index`, or every page.
    void dropPage(qint64 index) const;
    void dropPages() const;

    /// One per game, in the order of their ids (the list's order).
    std::vector<Brief> m_rows;
    /// Each text once: names, events, dates, results, codes (0 is empty).
    QList<QString> m_strings;
    QHash<QString, int> m_stringIndex;
    /// Each set of brief tags once (0 is none).
    QList<QList<PgnTag>> m_tagSets;
    QHash<QString, int> m_tagSetIndex;
    /// Pages of details by page number, the most recent last in m_pageOrder.
    mutable QHash<qint64, QList<Details>> m_pages;
    mutable QList<qint64> m_pageOrder;
    PlayerRoles m_roles;
    DatabaseProperties m_properties;
};

/// Writes many games into a new database file, fast: one connection and one
/// transaction per batch, no game kept in memory, no journal (the file is
/// new: what a failure leaves is thrown away). What Tools ▸ Convert uses.
class SqliteGameWriter {
public:
    ~SqliteGameWriter();

    /// Creates the database file `path` (which must not exist), empty.
    static std::unique_ptr<SqliteGameWriter> create(const QString &path, QString *errorMessage);

    /// Appends `games`, all or none.
    bool add(const QList<GameRecord> &games, QString *errorMessage);
    /// Ends the writing: the file is complete and can be opened.
    bool finish(QString *errorMessage);

private:
    SqliteGameWriter() = default;

    struct Private;
    std::unique_ptr<Private> d;
};
