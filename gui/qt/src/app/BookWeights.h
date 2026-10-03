#pragma once

#include <QList>

/// The weights of the moves of one book position, as shares of a whole: how
/// the user shifts them from the Opening Tree. Every change keeps the sum,
/// taking from or giving to the other moves in proportion to what they have,
/// so the heavy ones move most. Weights are Polyglot's 16-bit integers; the
/// shares are what they mean.
namespace BookWeights {

/// The weights with the move at `index` changed by `percent` of its own
/// share (+25 makes it a quarter heavier). A move at zero gains nothing by
/// a percentage, so an increase first takes one per cent from the others
/// and grows from there; a decrease of a move at zero takes nothing.
QList<int> adjusted(const QList<int> &weights, int index, int percent);

/// The weights with the move at `index` at zero, its share given to the
/// other moves that have some, in proportion.
QList<int> zeroed(const QList<int> &weights, int index);

} // namespace BookWeights
