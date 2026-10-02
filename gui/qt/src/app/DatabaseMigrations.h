#pragma once

#include <QList>
#include <QString>

class QSqlDatabase;

/// The schema of a `.pdb` file as an ordered list of migrations, as in web
/// frameworks: each one takes the file from the version before to its own,
/// and `PRAGMA user_version` says which was the last one applied. A new file
/// is made by running them all; an older file runs those it is missing when
/// it is opened. There is no way back: a file newer than the application is
/// refused.
///
/// To change the schema, append a migration here. Never edit one that was
/// released: files out there have already run it.
namespace DatabaseMigrations {

struct Migration {
    /// The schema version the file has once it ran: 1, 2, 3… without gaps.
    int version;
    /// What it does, as recorded in the file ("create_game_states").
    const char *name;
    /// Runs inside a transaction; false (with `error`) rolls it back.
    bool (*up)(QSqlDatabase &db, QString *error);
};

const QList<Migration> &all();
/// The version of a file that ran every migration.
int latestVersion();

/// Runs, in order, the migrations after version `from` (0 for an empty file),
/// each in its own transaction with its entry in the `migrations` table and
/// the new `user_version`: a failure leaves the file at the last version that
/// worked.
bool migrate(QSqlDatabase &db, int from, QString *errorMessage);

} // namespace DatabaseMigrations
