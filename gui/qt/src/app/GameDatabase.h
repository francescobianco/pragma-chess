#pragma once

#include "DatabaseProperties.h"
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

    /// Every game stored, the trashed and deleted ones included
    /// (GameRecord::state says which): what the indexes run over.
    virtual qint64 gameCount() const = 0;
    /// How many games have `state`: Live for those the lists show.
    qint64 countGames(GameState state) const
    {
        qint64 count = 0;
        for (qint64 index = 0; index < gameCount(); ++index)
            count += header(index).state == state;
        return count;
    }

    /// Header information for the game at `index` (0 <= index < gameCount()).
    /// A reference into the database: valid until the database changes.
    virtual const GameRecord &header(qint64 index) const = 0;

    /// Full game including moves.
    virtual std::optional<GameRecord> loadGame(qint64 index) const = 0;

    /// The moves of every game, in index order, read at once: what the
    /// position search indexes.
    virtual QList<GameLine> gameLines() const = 0;

    /// Appends a game (header and moves) and returns its index, or -1 on failure.
    virtual qint64 addGame(const GameRecord &game, QString *errorMessage) = 0;

    /// Replaces the header information (players, event, date, result, …, and
    /// the other PGN tags, the time control among them) of the game at
    /// `index`. Moves are left untouched.
    virtual bool updateHeader(qint64 index, const GameRecord &header, QString *errorMessage) = 0;

    /// Replaces the whole game at `index` (header and moves) with another
    /// version of it, e.g. a newer one from another device. Its uid stays;
    /// `modified` is taken from `game` (now, if empty).
    virtual bool replaceGame(qint64 index, const GameRecord &game, QString *errorMessage) = 0;

    /// Puts the game at `index` in the trash (Trashed), takes it back (Live)
    /// or deletes it from the trash (Deleted). Nothing leaves the file: a
    /// deleted game is only hidden everywhere, until optimize().
    virtual bool setGameState(qint64 index, GameState state, QString *errorMessage) = 0;
    /// The state of every game that was ever trashed, by uid, including the
    /// games optimize() purged: what another copy of the database follows.
    virtual QList<GameStateRecord> gameStates() const = 0;
    /// Takes the states of another copy that are newer than ours
    /// (GameStates::incomingChanges); a game it purged goes for good. The
    /// indexes of the games change when one goes.
    virtual bool mergeGameStates(const QList<GameStateRecord> &incoming, QString *errorMessage) = 0;
    /// Removes for good the deleted games and the names only they used, and
    /// compacts the file. The indexes of the games change. Returns how many
    /// games went, or -1 on failure.
    virtual int optimize(QString *errorMessage) = 0;

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
    /// What every source imported, so another copy does not import it again.
    virtual QList<SourceLink> sourceLinks() const = 0;
    /// Records the links of another copy this one lacks, for the sources it
    /// has (by uuid); a game it does not hold is recorded as purged.
    virtual bool mergeSourceLinks(const QList<SourceLink> &incoming, QString *errorMessage) = 0;

    /// Properties stored in the database (universal id, type, description).
    virtual DatabaseProperties properties() const = 0;
    virtual bool setProperties(const DatabaseProperties &properties, QString *errorMessage) = 0;

    /// Whether there are changes not yet written to `location()`.
    virtual bool isModified() const = 0;

    /// Writes a complete, consistent copy of the database to `path`.
    virtual bool saveCopy(const QString &path, QString *errorMessage) const = 0;
};
