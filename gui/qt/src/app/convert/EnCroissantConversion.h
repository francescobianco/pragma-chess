#pragma once

#include "app/DatabaseProperties.h"

#include <QString>

#include <atomic>
#include <functional>

/// A database of En Croissant's (`.db3`, its SQLite format: Games, Players,
/// Events, Sites; the moves as EnCroissantGames reads them) into a new
/// `.pdb`, on this computer: what Tools ▸ Manage Extensions does with the
/// databases it downloads. Read a batch at a time, the moves replayed on
/// every core but one, written by SqliteGameWriter, as PgnConversion does.
namespace EnCroissantConversion {

struct Progress {
    qint64 games = 0;   // Written to the database.
    qint64 skipped = 0; // Games without moves, or with a move that is not legal.
    qint64 total = 0;   // Games in the file.
    qint64 elapsedMs = 0;
};

struct Result {
    bool ok = false;
    bool cancelled = false;
    QString error;
    Progress progress;
};

/// Converts `db3Path` into `pdbPath`, which must not exist, on the calling
/// thread (a worker's), and gives the new database `properties` (its name,
/// a description saying where it came from). `progress` is called after
/// every batch, from that thread; setting `cancel` stops at the next one.
/// The database is written under a hidden name beside it and takes its
/// name only when complete: a failure or a cancel leaves nothing.
Result run(const QString &db3Path, const QString &pdbPath, const DatabaseProperties &properties,
           const std::function<void(const Progress &)> &progress, const std::atomic_bool *cancel);

} // namespace EnCroissantConversion
