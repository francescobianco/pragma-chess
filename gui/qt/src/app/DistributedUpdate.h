#pragma once

#include "app/GameDatabase.h"

#include <QList>
#include <QString>

/// A database we distribute (Classic Games, the training sets) is migrated
/// to a new version, never replaced: the copy in the user's folder is the
/// user's, with the games they loaded into it, changed or threw away.
namespace DistributedUpdate {

/// Adds to `database` the games of `shipped` it lacks — known by uid, or by
/// their start position (a puzzle given before uids were stable) — and
/// nothing else: no game there is removed or changed (the user's own, one
/// the user edited), and a game the user threw away (its uid among the
/// database's game states, trashed or deleted) is not brought back. Returns
/// how many were added; `error` gets the last failure, if any.
int addMissingGames(GameDatabase &database, const QList<GameRecord> &shipped, QString *error = nullptr);

} // namespace DistributedUpdate
