#pragma once

#include "PgnFile.h"
#include "PgnFilePlan.h"
#include "SourceFetch.h"

#include <QDateTime>

class GameDatabase;

/// A PGN file on this computer, kept in step with the database both ways
/// (PgnFilePlan): its new and changed games come into the database (Read),
/// the database's go into the file (Write), or both. The file is read again
/// only when it changed: its index (PgnFile::Index) sits beside it.
///
/// Settings: {"path": "/…/games.pgn", "mode": "readwrite" | "read" | "write"}.
/// State: the base (PgnFilePlan::baseToJson).
class PgnFileFetch : public SourceFetch {
    Q_OBJECT

public:
    PgnFileFetch(const GameSource &source, GameDatabase *database, QObject *parent = nullptr);

    void start() override;
    void abort() override;

    static QString path(const GameSource &source);
    static PgnFilePlan::Mode mode(const GameSource &source);

private:
    struct Snapshot {
        QHash<QString, PgnFilePlan::DatabaseGame> games;
        QHash<QString, qint64> indexes;
        QStringList order;
    };
    Snapshot snapshot() const;
    /// External id → uid of what this source imported.
    QHash<QString, QString> linked() const;

    void importBatch();
    /// Updates, then the file, the index and the base.
    void finish();
    bool writeFile(const PgnFilePlan::ReadPlan &read, const PgnFilePlan::WritePlan &write, const Snapshot &now,
                   QString *errorMessage);

    GameSource m_source;
    GameDatabase *m_database;
    PgnFilePlan::Mode m_mode;
    bool m_aborted = false;

    QByteArray m_bytes;
    QDateTime m_fileTime;
    PgnFile::Index m_index;
    PgnFilePlan::Base m_base;
    PgnFilePlan::ReadPlan m_plan;
    QList<ImportedGame> m_toImport;
    int m_added = 0;
    int m_unreadable = 0;
};

namespace PgnFileSettings {
inline constexpr char path[] = "path";
inline constexpr char mode[] = "mode";
} // namespace PgnFileSettings
