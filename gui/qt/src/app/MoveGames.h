#pragma once

#include "app/GameDatabase.h"

#include <QString>
#include <QStringList>

/// The games list's Move…: games taken from one database to another. Each
/// game is written there whole (moves, variations, comments, tags) before it
/// leaves here, and it leaves as every deletion does, softly
/// (GameState::Deleted: not listed, gone from the file when the database is
/// optimized, the deletion synced): a failure never loses a game.
namespace MoveGames {

struct Result {
    /// Games written to the other database and taken out of this one.
    int moved = 0;
    /// Games the other database already held, live, by uid: only taken out of this one.
    int alreadyThere = 0;
    /// The failure that stopped the move, empty if none; the games before it are moved.
    QString error;
};

/// Moves the games with `uids` from `from` to `to`. A game keeps its uid,
/// unless `to` has that uid in its trash or among its deleted games: then it
/// arrives as a new game, the one there staying as it is.
Result move(GameDatabase &from, const QStringList &uids, GameDatabase &to);

/// Where a new database called `name` goes in `folder`; empty, with
/// `problem` said, for a name that cannot be a file there or is taken.
QString newDatabasePath(const QString &folder, const QString &name, QString *problem);

} // namespace MoveGames
