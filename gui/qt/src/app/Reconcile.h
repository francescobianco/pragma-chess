#pragma once

#include "GameRecord.h"

#include <QList>
#include <QString>

/// How two copies of a database are merged, game by game (docs/phone-link.md,
/// "Merging two copies of a database"). Pure: the caller applies the plan.
namespace Reconcile {

struct Plan {
    /// Incoming games the local copy lacks (indexes into `incoming`).
    QList<int> insert;
    /// Incoming games that replace a local one: a conflict the incoming side
    /// won, being newer. `second` is the index into `local`.
    QList<std::pair<int, int>> update;
    /// Incoming games the local copy already has, as they are or newer.
    int known = 0;
    /// Uids whose two versions differ; the newer was kept, the user may review them.
    QStringList conflicts;
};

/// `local` and `incoming` need `uid`, `modified` and (for comparing a uid
/// found on both sides) moves. A newer `modified` wins; on a tie the local
/// copy stays. Games only in `local` are kept: the other side receives them.
Plan plan(const QList<GameRecord> &local, const QList<GameRecord> &incoming);

} // namespace Reconcile
