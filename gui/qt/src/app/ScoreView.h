#pragma once

#include "EngineEvaluation.h"

#include <QString>

/// The ways the Engine panel shows a score, turned by a click on it: as the
/// engine gives it (White's view), for the side to move, as the side to
/// move's chances (its expected score: what it brings home on average from
/// such a position, a win 1 and a draw ½, from the score by lichess's
/// curve), or as the symbol a chess book prints. The panel says which by a
/// dot before the value, no words: two colours for White's view (absolute),
/// the side to move's colour for its own; the book's symbol needs none.
/// Pure, unit-tested.
namespace ScoreView {

enum class Kind { Absolute, ForMover, Chances, Judgement };
constexpr int kKinds = 4;

/// The kind after `kind`, round again after the last.
Kind next(Kind kind);
/// The kind stored as `key` ("absolute"…), Absolute for anything else.
Kind fromKey(const QString &key);
QString key(Kind kind);

/// The score as `kind` shows it, `mover` the side to move: "+1.5", "−1.5"
/// (for Black), "62%", "±". Mates: "M3", "−M3" for the side mated, "#" when
/// it is on the board.
QString text(const EngineEvaluation &evaluation, Kind kind, Side mover);
/// What the kind is, for the tooltip: "Absolute", "For Black",
/// "Black's chances", "Judgement".
QString label(Kind kind, Side mover);
/// Whether the kind is seen from the side to move (its dot of one colour)
/// rather than from White's (a dot of two).
bool fromMover(Kind kind);

/// Where an evaluation sits on the game's course, from −1 (Black wins) to
/// 1 (White wins), 0 the balance: logarithmic in pawns, so a pawn stands
/// out from the midline and two are less than twice as far — log(1 + 3p)
/// over log(1 + 30): half a pawn a quarter of the way, a pawn 40%, two 57%,
/// five 81%, ten pawns and a mate at the edge.
double courseHeight(const EngineEvaluation &evaluation);

/// The Informant's symbol from White's side: = up to 0.3 pawns, ⩲/⩱ up to
/// 0.8, ±/∓ up to 2, +−/−+ beyond and for a mate.
QString judgement(const EngineEvaluation &evaluation);

} // namespace ScoreView
