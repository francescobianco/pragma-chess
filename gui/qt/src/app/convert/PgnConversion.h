#pragma once

#include <QString>

#include <atomic>
#include <functional>

/// Tools ▸ Convert ▸ PGN to Pragma Database: a PGN file of any size into a
/// new `.pdb`. The file is read a piece at a time (PgnSplitter), its games
/// are read on every core but one and written in batches (SqliteGameWriter),
/// so neither the file nor the games are ever held whole.
namespace PgnConversion {

struct Progress {
    qint64 bytesRead = 0;
    qint64 totalBytes = 0;
    qint64 games = 0;   // Written to the database.
    qint64 skipped = 0; // Entries that are no game PGN can read.
    qint64 elapsedMs = 0;
};

struct Result {
    bool ok = false;
    bool cancelled = false;
    QString error;
    Progress progress;
};

/// Converts `pgnPath` into `pdbPath`, which must not exist, on the calling
/// thread (a worker's). `progress` is called after every batch, from that
/// thread. Setting `cancel` stops at the next batch. The database is written
/// under a hidden name beside it and takes its name only when complete: a
/// failure or a cancel leaves nothing.
Result run(const QString &pgnPath, const QString &pdbPath, const std::function<void(const Progress &)> &progress,
           const std::atomic_bool *cancel);

} // namespace PgnConversion
