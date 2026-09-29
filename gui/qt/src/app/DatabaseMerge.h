#pragma once

#include "sync/FolderSync.h"

#include <QSet>
#include <QString>
#include <QStringList>

#include <functional>
#include <optional>

class GameDatabase;

/// Merging one database file into another, game by game (Reconcile): what the
/// folder sync does with duplicates (DatabaseDedupe) and merged files.
namespace DatabaseMerge {

struct Result {
    int stored = 0;
    int updated = 0;
    int known = 0;
    QStringList conflicts;
};

/// Merges every game of the database file `from` into `into` by uid: games it
/// lacks are added with their uid and revision, newer versions replace older
/// ones, and nothing of `into` is lost. `from` is only read.
std::optional<Result> mergeInto(GameDatabase &into, const QString &from, QString *errorMessage);

/// The same between two files; `into` then takes the id `lineage` if not empty.
bool mergeFile(const QString &from, const QString &into, const QString &lineage, QString *errorMessage);

/// FolderSync's database hooks, backed by SQLite. `canonical` says where a
/// database we ship lives (may be empty).
FolderSync::DatabaseHooks hooks(std::function<bool(const QString &relativePath)> canonical = {});

} // namespace DatabaseMerge
