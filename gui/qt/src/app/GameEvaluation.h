#pragma once

#include "app/ChessPosition.h"
#include "app/EngineEvaluation.h"

#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>

#include <functional>
#include <optional>

class QTimer;
class UciEngine;

/// The Evaluation of the Game ("valutazione della partita"): every position
/// of a finished game evaluated by the engine, so the course of the game can
/// be drawn whole. It works in passes: a quick one over the whole game
/// first, so the course is complete at once, then deeper ones, each
/// refining it (passDepths()), for at most kTimeBudgetMs. It drives an
/// engine process of its own, never
/// the live analysis, and only on a game that is over — never while a game
/// is played online (docs/tech/game-evaluation.md).
class GameEvaluation : public QObject {
    Q_OBJECT

public:
    /// The depth of each pass, shallow to deep.
    static const QList<int> &passDepths();
    /// How long an evaluation may run: then it stops where it is, so its
    /// values (the course, the strength of each side) settle and stay.
    static constexpr int kTimeBudgetMs = 30000;

    /// The order of the work, pure: given the depth known of each position
    /// (0 none), the next position to search and how deep, the shallowest
    /// pass first and the positions in game order within it; none when every
    /// position is known as deep as the last pass.
    struct Step {
        int ply = 0;
        int depth = 0;
    };
    static std::optional<Step> nextStep(const QList<int> &knownDepths);

    /// What a position with no legal moves is worth without asking an engine:
    /// mate for the side that gave it, or a draw; none for any other position.
    static std::optional<EngineEvaluation> finalEvaluation(const ChessPosition &position);

    /// The engine to run and the share of the machine it may take, as the
    /// analysis is given (EnginePower).
    struct Engine {
        QString executable;
        int threads = 1;
        int hashMb = 0;
        bool lowPriority = true;
        int cpuPercent = 0;
        int quotaOfOneCore = 0;
    };

    explicit GameEvaluation(QObject *parent = nullptr);
    ~GameEvaluation() override;

    /// How deep a position is already known elsewhere (the Explainer's
    /// evaluations): no pass searches it again at a depth it has.
    void setKnownDepth(std::function<int(const ChessPosition &)> knownDepth) { m_knownDepth = std::move(knownDepth); }

    /// Starts evaluating the game from `startFen` (standard position when
    /// empty) through `uciMoves`; a running evaluation stops first. False
    /// when the engine cannot start or the game cannot be replayed.
    /// `fresh`: every position searched again, whatever is known of it
    /// (setKnownDepth is not asked) — another engine's turn.
    bool start(const Engine &engine, const QString &startFen, const QStringList &uciMoves, bool fresh = false);
    void stop();
    bool isRunning() const { return m_running; }
    /// The positions of the game evaluated, ply 0 the start.
    int positionCount() const { return int(m_positions.size()); }
    const QList<ChessPosition> &positions() const { return m_positions; }
    /// The depth reached by every position so far (the pass completed).
    int completedDepth() const;

Q_SIGNALS:
    /// A position evaluated (or evaluated deeper).
    void evaluated(int ply, const ChessPosition &position, const EngineEvaluation &evaluation);
    /// Every position known at least as deep as `depth`.
    void passFinished(int depth);
    /// The last pass is done.
    void finished();
    void failed(const QString &message);

private:
    void searchNext();
    void searchDone();

    UciEngine *m_engine = nullptr;
    QTimer *m_budget = nullptr;
    bool m_running = false;
    QString m_startFen;
    QStringList m_moves;
    QList<ChessPosition> m_positions;
    QList<int> m_depths;
    std::optional<Step> m_step;
    std::optional<EngineEvaluation> m_last;
    std::function<int(const ChessPosition &)> m_knownDepth;
};
