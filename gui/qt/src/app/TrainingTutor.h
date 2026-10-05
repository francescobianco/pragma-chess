#pragma once

#include "ChessPosition.h"
#include "EngineEvaluation.h"

/// The tutor of a training game: says when the move the user just played was
/// an error, from two evaluations the game already has, so that no analysis
/// is run for it. The judgement is smart/TUTOR.smart's, run by every client.
namespace TrainingTutor {

enum class Alert {
    None,
    /// The user was better and no longer is, without being worse: a chance went by.
    MissedChance,
    Inaccuracy,
    Mistake,
    Blunder,
};

/// Judges `played` by the jump between `before`, the evaluation of the
/// position the user moved from, and `after`, what the engine found looking
/// for its answer: smart/TUTOR.smart's Judge, whose comments say why. None
/// if the program cannot run (logged).
Alert judge(const EngineEvaluation &before, const EngineEvaluation &after, Side user, const ChessMove &played);

} // namespace TrainingTutor
