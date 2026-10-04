#pragma once

#include "PgnFile.h"

#include <QHash>
#include <QJsonObject>
#include <QList>
#include <QPair>
#include <QSet>
#include <QString>

/// What a sync between a database and a PGN file does, decided apart from
/// both (pure, unit-tested); PgnFileFetch reads, applies and writes.
///
/// A game of the file and one of the database are the same game when the
/// file's game carries the database game's uid (the PragmaUid tag). The base
/// says how each was at the last sync — the hash of the file's game and when
/// the database's was modified — so a side that changed is told from one
/// that did not, as the folder sync's base does for files. Nothing removed
/// on one side is removed on the other: a game gone from the file stays in
/// the database (and is not written back), a game in the trash stays in the
/// file. A game changed on both sides is kept twice: the database's version
/// keeps the uid, the file's comes in as a game of its own.
namespace PgnFilePlan {

enum class Mode { ReadWrite, Read, Write };

/// "readwrite", "read", "write": how the source's settings store it.
QString modeKey(Mode mode);
Mode modeFromKey(const QString &key);
inline bool reads(Mode mode) { return mode != Mode::Write; }
inline bool writes(Mode mode) { return mode != Mode::Read; }

struct BaseEntry {
    /// The file's game at the last sync; empty once it left the file.
    QString hash;
    /// The database game's `modified` at the last sync.
    QString modified;
};
using Base = QHash<QString, BaseEntry>;

/// The base as the source's state stores it, and back.
QJsonObject baseToJson(const Base &base);
Base baseFromJson(const QJsonObject &state);

/// A game of the database, by uid.
struct DatabaseGame {
    QString modified;
    bool live = true;
};

/// External id of a file's game for game_sources: its uid, or its content.
QString externalId(const PgnFile::Entry &entry);

struct ReadPlan {
    /// Entries to import as new games (with their uid, when they have one).
    QList<int> imports;
    /// Entries whose game in the database (the uid) takes the file's version.
    QList<QPair<int, QString>> updates;
    /// Entries changed on both sides: imported as a new game, without uid.
    QList<int> conflicts;
    /// Entries to rewrite with the database's game (it changed, or won).
    QList<QPair<int, QString>> rewrites;
};

/// `known` are the external ids the source already imported.
ReadPlan planRead(Mode mode, const QList<PgnFile::Entry> &entries, const Base &base,
                  const QHash<QString, DatabaseGame> &games, const QSet<QString> &known);

struct WritePlan {
    /// Untagged entries given the uid of the game they became.
    QList<QPair<int, QString>> tags;
    /// Live games of the database the file has never had, in database order.
    QStringList appends;
};

/// After the read: `linked` maps the external ids of the source to uids,
/// `order` lists the database's uids in order.
WritePlan planWrite(Mode mode, const QList<PgnFile::Entry> &entries, const Base &base,
                    const QHash<QString, DatabaseGame> &games, const QHash<QString, QString> &linked,
                    const QStringList &order);

/// The base after a sync, from the file as it now is and the database's
/// games. `pending` are the uids whose rewrite did not reach the file: they
/// keep their old base, so the next sync tries again.
Base nextBase(const Base &before, const QList<PgnFile::Entry> &entries, const QHash<QString, DatabaseGame> &games,
              const QSet<QString> &pending);

} // namespace PgnFilePlan
