#pragma once

#include "EngineEvaluation.h"

#include <QString>

/// The ways the Engine panel shows a score, turned by a click on it: as the
/// engine gives it (White's view), for the colour at the bottom of the
/// board, as that colour's chances, or as the symbol a chess book prints.
/// Pure, unit-tested.
namespace ScoreView {

enum class Kind { Absolute, ForBottom, Chances, Judgement };
constexpr int kKinds = 4;

/// The kind after `kind`, round again after the last.
Kind next(Kind kind);
/// The kind stored as `key` ("absolute"…), Absolute for anything else.
Kind fromKey(const QString &key);
QString key(Kind kind);

/// The score as `kind` shows it, for the colour at the bottom `bottom`:
/// "+1.5", "−1.5" (for Black), "62%", "±". Mates: "M3", "−M3" for the side
/// mated, "#" when it is on the board.
QString text(const EngineEvaluation &evaluation, Kind kind, Side bottom);
/// What the kind is, under the score: "Absolute", "For Black",
/// "Black's chances", "Judgement".
QString label(Kind kind, Side bottom);

/// Where an evaluation sits on the game's course, from −1 (Black wins) to
/// 1 (White wins), 0 the balance: logarithmic in pawns, so a pawn stands
/// out from the midline and two are less than twice as far — log(1 + p)
/// over log(1 + 10), ten pawns and a mate at the edge.
double courseHeight(const EngineEvaluation &evaluation);

/// The Informant's symbol from White's side: = up to 0.3 pawns, ⩲/⩱ up to
/// 0.8, ±/∓ up to 2, +−/−+ beyond and for a mate.
QString judgement(const EngineEvaluation &evaluation);

} // namespace ScoreView
