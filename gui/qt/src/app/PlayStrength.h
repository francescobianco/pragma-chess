#pragma once

#include "app/BoardState.h"
#include "app/EngineEvaluation.h"

#include <QList>

#include <optional>

/// How well each side played a game, from the evaluation of every position
/// (the Evaluation of the Game): the loss of each move against the
/// evaluation before it, for the side that moved. The estimated Elo is a
/// curve of the average loss, provisional until it is calibrated on rated
/// games (docs/tech/game-evaluation.md); from one game it is rough, some
/// hundreds of points either way.
namespace PlayStrength {

/// The fewest moves of a side for an estimate of its play.
inline constexpr int kMinimumMoves = 10;
/// Where an estimate starts from, with no moves seen: an average player.
inline constexpr int kAverageElo = 1500;
/// How many moves weigh as much as that starting point: a side's own play
/// counts for moves / (moves + kPriorMoves) of the estimate, so a miniature
/// of easy moves stays near the average and a long game speaks for itself.
inline constexpr int kPriorMoves = 12;

struct Player {
    /// The moves judged: those with an evaluation before and after.
    int moves = 0;
    /// Average centipawn loss (ACPL), evaluations bounded to ±1000.
    double averageLoss = 0;
    /// Mean accuracy, 0–100, from the loss in winning chances (as lichess counts it).
    double accuracy = 0;
    /// Estimated Elo; none under kMinimumMoves.
    std::optional<int> elo;
};

struct Estimate {
    Player white;
    Player black;
};

/// The estimate from `evaluations`, one per position, 0 the start (none
/// where unknown); `firstMover` moves from the start.
Estimate estimate(const QList<std::optional<EngineEvaluation>> &evaluations, Side firstMover);

/// The Elo the curve gives an average centipawn loss, however many moves.
int eloFromLoss(double averageLoss);
/// The estimate for a side: the curve's Elo drawn towards kAverageElo the
/// fewer moves it rests on.
int eloFromGame(double averageLoss, int moves);

/// A move's accuracy, 0–100, from the mover's winning chances (0–100) before and after it.
double moveAccuracy(double winBefore, double winAfter);

/// The winning chances, 0–100, of centipawns for the side that has them.
double winChances(int centipawns);

} // namespace PlayStrength
