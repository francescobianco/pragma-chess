#pragma once

#include "ChessPosition.h"
#include "EngineEvaluation.h"

#include <QList>
#include <QString>

#include <optional>

/// An arrow drawn on the board to explain a line.
struct BoardArrow {
    enum class Kind {
        /// A move of the side punishing a mistake.
        Refutation,
        /// A move of the side whose idea is being shown (a winning line, the better move).
        Idea,
        /// A reply of the other side within the line.
        Reply,
        /// The move that should have been played instead, from the previous position.
        Alternative,
    };

    int from = -1;
    int to = -1;
    Kind kind = Kind::Idea;
    /// Position in the line, starting at 1; 0 for arrows outside a sequence.
    int step = 0;

    bool operator==(const BoardArrow &) const = default;
};

/// What the "Explain" command shows for the position on the board: arrows for
/// the moves that justify the evaluation, the pieces that fall, and a summary.
struct MoveExplanation {
    enum class Verdict { None, Best, Good, Inaccuracy, Mistake, Blunder };

    Verdict verdict = Verdict::None;
    QList<BoardArrow> arrows;
    /// Squares of pieces lost along the explained line.
    QList<int> lostPieces;
    /// Plain-text summary, e.g. "Blunder (+0.3 → −2.9). Black wins a knight: 14…Bxf2+ 15.Kxf2 Ng4+".
    QString summary;
    /// Moves (UCI) to play on the board, from the position shown, to demonstrate
    /// the explanation: a forced mate, played to the end.
    QStringList playback;
    /// How the explanation was reached, when ExplanationInput::trace is set (for tuning).
    QStringList trace;

    bool operator==(const MoveExplanation &) const = default;
};

struct ExplanationInput {
    /// Position before the last move and the move itself; empty at the start of a game.
    std::optional<ChessPosition> before;
    std::optional<ChessMove> played;
    /// Evaluation of `before`, when known.
    std::optional<EngineEvaluation> beforeEvaluation;

    /// Position on the board and its evaluation.
    ChessPosition after;
    EngineEvaluation afterEvaluation;

    /// Optional enrichment from AdvantageProbe: the ply of `afterEvaluation.pv`
    /// from which a shallow search already agrees with the deep evaluation,
    /// i.e. where the advantage shows on the board. Used when no material or
    /// mate explains the evaluation.
    std::optional<int> concretePly;

    /// Why `afterEvaluation` is not the explanation's own search, for the trace.
    QString evaluationNote;

    /// How moves are written in the summary.
    SanStyle sanStyle = SanStyle::Letters;

    /// Fill MoveExplanation::trace.
    bool trace = false;
};

/// Explains the evaluation of a position by comparing it with the position
/// before the last move.
///
/// The engine's principal variation is replayed until the evaluation turns
/// into something concrete on the board — material won after the exchanges
/// settle (intermediate checks and recaptures included) or a mate — and the
/// moves up to that point become the arrows. When the last move threw away
/// the advantage without losing material, the better move's line is shown.
MoveExplanation explainPosition(const ExplanationInput &input);

/// Classifies `played` by how much of its side's expected share of the game it gave away.
MoveExplanation::Verdict classifyMove(const EngineEvaluation &before, const EngineEvaluation &after,
                                      Side mover, const ChessMove &played);
