#include "TrainingTutor.h"

#include "MoveExplanation.h"

namespace TrainingTutor {

namespace {

// Shares of the game (EngineEvaluation::shareFor): about +1.5 and −0.5 pawns.
constexpr double kBetter = 0.63;
constexpr double kNotWorse = 0.45;

} // namespace

Alert judge(const EngineEvaluation &before, const EngineEvaluation &after, Side user, const ChessMove &played)
{
    const MoveExplanation::Verdict verdict = classifyMove(before, after, user, played);
    if (verdict != MoveExplanation::Verdict::Mistake && verdict != MoveExplanation::Verdict::Blunder)
        return Alert::None;
    // Nothing was lost that the user had on the board: the advantage was
    // there to take, and the position is still playable.
    if (before.shareFor(user) >= kBetter && after.shareFor(user) >= kNotWorse)
        return Alert::MissedChance;
    return verdict == MoveExplanation::Verdict::Blunder ? Alert::Blunder : Alert::Mistake;
}

} // namespace TrainingTutor
