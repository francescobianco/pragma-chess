#pragma once

#include "ChessPosition.h"
#include "MoveExplanation.h"

#include <QHash>
#include <QObject>

#include <optional>

class QTimer;
class UciEngine;

/// Drives the "Explain" command: collects the evaluation of the position on
/// the board and of the position before the last move, and turns the two into
/// a MoveExplanation.
///
/// The position on the board is evaluated by the main analysis, which feeds
/// its results in through setCurrentEvaluation(). The previous position is
/// searched to a fixed depth by a second engine process, unless it was
/// already evaluated deeply enough (e.g. while stepping through the game).
class Explainer : public QObject {
    Q_OBJECT

public:
    explicit Explainer(QObject *parent = nullptr);
    ~Explainer() override;

    bool isEnabled() const { return m_enabled; }
    /// Starts explaining, using the UCI engine at `engineExecutable`, or stops.
    void setEnabled(bool enabled, const QString &engineExecutable = QString());

    /// The position on the board and, unless it is the starting position of
    /// the game, the position before the last move together with that move.
    void setPosition(const ChessPosition &position, const std::optional<ChessPosition> &before,
                     const std::optional<ChessMove> &played);

    /// Evaluation of the position on the board, from the main analysis.
    void setCurrentEvaluation(const EngineEvaluation &evaluation);

Q_SIGNALS:
    /// The explanation for the current position; without arrows and with a
    /// progress summary while the evaluations are not deep enough yet.
    void explanationChanged(const MoveExplanation &explanation);

private:
    void remember(const QString &positionKey, const EngineEvaluation &evaluation);
    std::optional<EngineEvaluation> deepEnough(const ChessPosition &position) const;
    void searchPreviousPosition();
    void scheduleUpdate();
    void update();

    UciEngine *m_engine;
    QString m_engineExecutable;
    bool m_enabled = false;

    ChessPosition m_position = ChessPosition::startingPosition();
    std::optional<ChessPosition> m_before;
    std::optional<ChessMove> m_played;

    /// Deepest evaluation seen for each position, by ChessPosition::positionKey().
    QHash<QString, EngineEvaluation> m_evaluations;
    /// Position the second engine is searching.
    QString m_searchingKey;

    QTimer *m_updateTimer;
    std::optional<MoveExplanation> m_shown;
};
