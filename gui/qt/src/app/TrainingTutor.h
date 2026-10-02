#pragma once

#include "ChessPosition.h"
#include "EngineEvaluation.h"

/// The tutor of a training game: says when the move the user just played was
/// an error, from two evaluations the game already has, so that no analysis
/// is run for it.
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
/// position the user moved from (the engine's search for its own last move,
/// or the analysis that ran while the user was thinking), and `after`, what
/// the engine found looking for its answer. The scale and the names are
/// Explain's (classifyMove), so the tutor and the explanation agree: anything
/// Explain calls an error stops the game, from an inaccuracy up. (An
/// inaccuracy is already a jump of a pawn or more near equality: 1.e4 e5
/// 2.f4 exf4 3.a4, from −0.5 to −2.1, is one.)
Alert judge(const EngineEvaluation &before, const EngineEvaluation &after, Side user, const ChessMove &played);

} // namespace TrainingTutor
