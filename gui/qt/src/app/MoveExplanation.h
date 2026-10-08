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
        /// A capture threatened, not played: a piece left attacked (dashed red).
        Threat,
        /// A piece's trip along the engine's line, a plan (INSIGHT.smart).
        Plan,
    };

    int from = -1;
    int to = -1;
    Kind kind = Kind::Idea;
    /// Position in the line, starting at 1; 0 for arrows outside a sequence.
    /// For a Plan, its rank (1 the strongest), which chooses its colour.
    int step = 0;
    /// The piece the arrow moves, drawn small and faint where it goes: given
    /// for the better move, which starts from the position before the one on
    /// the board, where that piece may no longer stand.
    Piece piece = {};
    /// Squares the arrow passes through between `from` and `to`, in order:
    /// a route (Nb1-d2-f1-g3) rather than a single move.
    QList<int> via = {};

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
    /// Squares of pieces attacked along the line that do not fall (a dashed ring).
    QList<int> threatenedPieces;
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

    /// Who asks: the side the user plays (training) or sees from below.
    /// The other side's plan is drawn as theirs, not as a good idea.
    std::optional<Side> viewer;

    /// How moves are written in the summary.
    SanStyle sanStyle = SanStyle::Letters;

    /// Fill MoveExplanation::trace.
    bool trace = false;
};

/// Explains the evaluation of a position by comparing it with the position
/// before the last move: smart/EXPLAIN.smart's Explain, whose comments say
/// how, run by every client.
MoveExplanation explainPosition(const ExplanationInput &input);

/// What one tick of the engine made of the explanation (explainTick).
struct ExplanationTick {
    /// Whether `explanation` is to be shown; otherwise what was shown stays.
    bool shown = false;
    MoveExplanation explanation;
    /// The program's mistake, if it stopped on one.
    QString error;
};

/// Explaining reacts to the engine: startExplanation() when Explain turns to
/// a move, then explainTick() for every line the engine reports on
/// `input.after` (in `input.afterEvaluation`), with the deepest evaluation
/// known of the position before (`input.beforeEvaluation`, if any).
/// EXPLAIN.smart's Start and Tick: every client, and the tests, go through
/// these two, so they explain alike.
void startExplanation();
ExplanationTick explainTick(const ExplanationInput &input);

/// The sentences EXPLAIN.smart says, kept here for their translations.
QStringList explanationSentences();

/// Classifies `played` by how much of its side's expected share of the game
/// it gave away, or by the material `played` hands over from `start` (when
/// given): smart/TUTOR.smart's Classify, shared with the tutor. None if the
/// program cannot run (logged).
MoveExplanation::Verdict classifyMove(const EngineEvaluation &before, const EngineEvaluation &after,
                                      Side mover, const ChessMove &played,
                                      const std::optional<ChessPosition> &start = std::nullopt);
