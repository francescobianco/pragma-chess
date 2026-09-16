#pragma once

#include "GameRecord.h"

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

    /// Whether there are changes not yet written to `location()`.
    virtual bool isModified() const = 0;

    /// Writes a complete, consistent copy of the database to `path`.
    virtual bool saveCopy(const QString &path, QString *errorMessage) const = 0;
};
