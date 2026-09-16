#pragma once

#include "BoardState.h"

#include <QStringList>

/// An evaluation reported by an engine, always from White's point of view.
struct EngineEvaluation {
    bool isMate = false;
    /// Centipawns, positive when White is better (when !isMate).
    int centipawns = 0;
    /// Moves to mate (when isMate); 0 means the position is already checkmate.
    int mateIn = 0;
    /// Side delivering mate (when isMate).
    Side mating = Side::White;
    int depth = 0;
    /// Principal variation in UCI notation.
    QStringList pv;

    /// Expected share of the game for White in [0, 1], used by evaluation bars.
    double whiteShare() const;
    /// Expected share of the game for `side` in [0, 1].
    double shareFor(Side side) const { return side == Side::White ? whiteShare() : 1.0 - whiteShare(); }
    /// Centipawns from `side`'s point of view; mates count as a large, bounded score.
    int centipawnsFor(Side side) const;
    /// Short human-readable score, e.g. "+1.3", "−0.4", "M3", "#".
    QString text() const;
};
