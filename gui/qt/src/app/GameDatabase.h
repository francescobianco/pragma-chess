#pragma once

#include "GameRecord.h"
#include "PlayerRole.h"
#include "sources/GameSource.h"

#include <QList>
#include <QSet>

#include <optional>

/// Boundary between the desktop client and the chess database engine.
///
/// The GUI only ever talks to this interface. The current implementation
/// reads `.pdb` files (SQLite) directly; the Rust engine will provide the real
/// one through its C API without the widgets having to change.
class GameDatabase {
public:
    virtual ~GameDatabase() = default;

    /// Display name, derived from the file name.
    virtual QString name() const = 0;
    /// Absolute path of the database file, empty for in-memory databases.
    virtual QString location() const = 0;

    virtual qint64 gameCount() const = 0;

    /// Header information for the game at `index` (0 <= index < gameCount()).
    virtual GameRecord header(qint64 index) const = 0;

    /// Full game including moves.
    virtual std::optional<GameRecord> loadGame(qint64 index) const = 0;

    /// Appends a game (header and moves) and returns its index, or -1 on failure.
    virtual qint64 addGame(const GameRecord &game, QString *errorMessage) = 0;

    /// Replaces the header information (players, event, date, result, …) of
    /// the game at `index`. Moves are left untouched.
    virtual bool updateHeader(qint64 index, const GameRecord &header, QString *errorMessage) = 0;

    /// Who the players are to the user (me, friends, opponents), by name.
    virtual PlayerRoles playerRoles() const = 0;
    /// Says who a player is; PlayerRole::None forgets it.
    virtual bool setPlayerRole(const QString &player, PlayerRole role, QString *errorMessage) = 0;

    /// External sources of games connected to the database.
    virtual QList<GameSource> sources() const = 0;
    /// Connects a source; fills in its id (and uuid and creation time if empty).
    virtual bool addSource(GameSource &source, QString *errorMessage) = 0;
    /// Stores the settings, state, errors and sync time of a source.
    virtual bool updateSource(const GameSource &source, QString *errorMessage) = 0;
    /// Disconnects a source. Games already imported from it stay.
    virtual bool removeSource(qint64 sourceId, QString *errorMessage) = 0;
    /// Database ids of the games imported from a source.
    virtual QSet<qint64> sourceGameIds(qint64 sourceId) const = 0;
    /// Appends the games not imported from the source before (by external id).
    /// Returns how many were added, or -1 on failure.
    virtual int importGames(qint64 sourceId, const QList<ImportedGame> &games, QString *errorMessage) = 0;

    /// Whether there are changes not yet written to `location()`.
    virtual bool isModified() const = 0;

    /// Writes a complete, consistent copy of the database to `path`.
    virtual bool saveCopy(const QString &path, QString *errorMessage) const = 0;
};
