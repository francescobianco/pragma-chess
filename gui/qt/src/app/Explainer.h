#pragma once

#include "ChessPosition.h"
#include "EngineEvaluation.h"
#include "MoveExplanation.h"

#include <QHash>
#include <QObject>

#include <optional>

/// Drives the "Explain" command of the desktop client.
///
/// It never runs an engine of its own: every line the live analysis reports
/// on the position on the board is a tick for smart/EXPLAIN.smart, which
/// answers with the explanation, so it grows and settles as the search goes
/// deeper. The deepest evaluation seen of each position is kept: the
/// position before the move is judged with it, and a position seen before
/// is explained at once when the board comes back to it.
class Explainer : public QObject {
    Q_OBJECT

public:
    explicit Explainer(QObject *parent = nullptr);

    bool isEnabled() const { return m_enabled; }
    /// Starts explaining the position on the board, or stops.
    void setEnabled(bool enabled);

    /// The position on the board and, unless it is the starting position of
    /// the game, the position before the last move together with that move.
    void setPosition(const ChessPosition &position, const std::optional<ChessPosition> &before,
                     const std::optional<ChessMove> &played);

    /// A line of the live analysis of the position on the board.
    void setLiveEvaluation(const EngineEvaluation &evaluation);

Q_SIGNALS:
    /// The explanation for the current position; a waiting summary until the
    /// search is deep enough.
    void explanationChanged(const MoveExplanation &explanation);

private:
    /// The deepest evaluation known of `position`.
    std::optional<EngineEvaluation> known(const ChessPosition &position) const;
    void start();
    void tick();
    void show(const MoveExplanation &explanation);

    bool m_enabled = false;
    ChessPosition m_position = ChessPosition::startingPosition();
    std::optional<ChessPosition> m_before;
    std::optional<ChessMove> m_played;

    QHash<QString, EngineEvaluation> m_evaluations;
    std::optional<MoveExplanation> m_shown;
};
