#include "PlayStrength.h"

#include <algorithm>
#include <cmath>

namespace PlayStrength {

namespace {

/// A mate counts as a large advantage, no larger than this: one move into
/// or out of a mate does not swamp the average.
constexpr int kBound = 1000;

/// A move under half a pawn of loss is a good one.
constexpr int kGoodMove = 50;

/// How much a move counts in the average, from the advantage the mover had
/// before it (the user's rule, 2026-10-11): ahead, a good move was easy and
/// counts less — 1 / (1 + (advantage / 1 pawn)²) —, and a mistake was
/// sloppiness and counts more — 1 + advantage / 10 pawns, up to twice. Level
/// or behind, every move counts once. Not part of the calibration: a player
/// never ahead is measured as the curve was fitted.
double weight(int advantage, int loss)
{
    if (advantage <= 0)
        return 1;
    if (loss < kGoodMove)
        return 1 / (1 + std::pow(advantage / 100.0, 2));
    return 1 + advantage / 1000.0;
}

int bounded(const EngineEvaluation &evaluation, Side side)
{
    return std::clamp(evaluation.centipawnsFor(side), -kBound, kBound);
}

} // namespace

double winChances(int centipawns)
{
    // Lichess's curve, fitted on its games.
    return 50 + 50 * (2 / (1 + std::exp(-0.00368208 * centipawns)) - 1);
}

double moveAccuracy(double winBefore, double winAfter)
{
    const double lost = std::max(0.0, winBefore - winAfter);
    return std::clamp(103.1668 * std::exp(-0.04354 * lost) - 3.1669, 0.0, 100.0);
}

int eloFromLoss(double averageLoss)
{
    // Calibrated on 14 307 rated players' games of lichess (2013, rating
    // 800–2343, the server's own [%eval]): the curve through the typical
    // loss of each rating band, so a player who loses what 1100s lose is
    // called 1100 (scripts/calibrate-play-strength.py,
    // docs/tech/game-evaluation.md). Bounded from 100 to 2850: no game is
    // worth more than the best players reach.
    const double elo = 5189 - 849 * std::log(std::max(averageLoss, 1.0));
    return std::clamp(int(std::lround(elo)), 100, 2850);
}

int eloFromGame(double averageLoss, int moves)
{
    const int elo = eloFromLoss(averageLoss);
    if (elo <= kAverageElo)
        return elo;
    const double weight = double(moves) / (moves + kPriorMoves);
    return int(std::lround(kAverageElo + (elo - kAverageElo) * weight));
}

Estimate estimate(const QList<std::optional<EngineEvaluation>> &evaluations, Side firstMover)
{
    struct Sums {
        int moves = 0;
        double weights = 0;
        double loss = 0;
        double accuracy = 0;
    } white, black;
    Side mover = firstMover;
    for (qsizetype ply = 1; ply < evaluations.size(); ++ply, mover = mover == Side::White ? Side::Black : Side::White) {
        const std::optional<EngineEvaluation> &before = evaluations.at(ply - 1);
        const std::optional<EngineEvaluation> &after = evaluations.at(ply);
        if (!before || !after)
            continue;
        const int was = bounded(*before, mover);
        const int is = bounded(*after, mover);
        // Decided before and after alike: a further mistake would not show
        // (the bound), and an easy move in a won game says little.
        if ((was >= kBound && is >= kBound) || (was <= -kBound && is <= -kBound))
            continue;
        Sums &sums = mover == Side::White ? white : black;
        const int loss = std::max(0, was - is);
        const double counts = weight(was, loss);
        ++sums.moves;
        sums.weights += counts;
        sums.loss += counts * loss;
        sums.accuracy += moveAccuracy(winChances(was), winChances(is));
    }
    const auto player = [](const Sums &sums) {
        Player result;
        result.moves = sums.moves;
        if (sums.moves == 0)
            return result;
        result.averageLoss = sums.loss / sums.weights;
        result.accuracy = sums.accuracy / sums.moves;
        if (sums.moves >= kMinimumMoves)
            result.elo = eloFromGame(result.averageLoss, sums.moves);
        return result;
    };
    return Estimate{player(white), player(black)};
}

} // namespace PlayStrength
