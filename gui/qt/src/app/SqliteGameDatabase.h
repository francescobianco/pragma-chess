#pragma once

#include "GameDatabase.h"

#include <QList>

#include <memory>

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

    QString name() const override;
    QString location() const override { return m_path; }
    qint64 gameCount() const override { return m_headers.size(); }
    GameRecord header(qint64 index) const override { return m_headers.at(index); }
    std::optional<GameRecord> loadGame(qint64 index) const override;
    qint64 addGame(const GameRecord &game, QString *errorMessage) override;
    bool updateHeader(qint64 index, const GameRecord &header, QString *errorMessage) override;
    bool isModified() const override { return false; }
    bool saveCopy(const QString &path, QString *errorMessage) const override;

private:
    SqliteGameDatabase(QString path, QString connectionName);

    bool loadHeaders(QString *errorMessage);

    QString m_path;
    QString m_connectionName;
    // TODO: page headers from SQL instead of caching them for very large databases.
    QList<GameRecord> m_headers;
};
