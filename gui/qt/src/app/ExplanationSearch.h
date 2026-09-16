#pragma once

#include "ChessPosition.h"
#include "MoveExplanation.h"

#include <QObject>
#include <QStringList>

#include <optional>

class UciEngine;

/// How the engine is searched for an explanation. The desktop client and the
/// command line tool use the same values, so they explain a move the same way.
struct ExplainSettings {
    /// Depth of the searches of the positions before and after the move.
    int depth = 20;
    /// Depth and length of the line probe (AdvantageProbe); depth 0 skips it.
    int probeDepth = 2;
    int probePlies = 12;
    /// A single thread and a cleared hash make fixed-depth searches reproducible.
    int threads = 1;
    int hashMb = 64;
};

/// Everything the engine said about a move to explain.
struct ExplanationAnalysis {
    std::optional<ChessPosition> before;
    std::optional<ChessMove> played;
    ChessPosition after = ChessPosition::startingPosition();

    /// Evaluations by increasing depth; the last one is the result.
    QList<EngineEvaluation> beforeByDepth;
    QList<EngineEvaluation> afterByDepth;
    /// Shallow evaluations of the positions along the principal variation of
    /// `after` (index 0 = `after` itself).
    QList<EngineEvaluation> probe;

    std::optional<EngineEvaluation> beforeEvaluation() const;
    std::optional<EngineEvaluation> afterEvaluation() const;
    /// Input for explainPosition(), with the concrete ply from the probe.
    ExplanationInput input(SanStyle style = SanStyle::Letters, bool trace = false) const;
};

/// Runs the searches for an explanation on its own engine process: the
/// position before the move, the position after it, then the line probe.
class ExplanationSearch : public QObject {
    Q_OBJECT

public:
    explicit ExplanationSearch(const ExplainSettings &settings = {}, QObject *parent = nullptr);
    ~ExplanationSearch() override;

    const ExplainSettings &settings() const { return m_settings; }
    /// Name the engine reports ("Stockfish 16"), empty until it is known.
    QString engineName() const;

    /// Starts the engine process if needed.
    bool start(const QString &engineExecutable);
    void shutdown();

    /// Analyzes a move (or, without `before`, a position), replacing any
    /// analysis in progress.
    void analyze(const std::optional<ChessPosition> &before, const std::optional<ChessMove> &played,
                 const ChessPosition &after);
    void cancel();
    bool isBusy() const { return m_stage != Stage::Idle; }

Q_SIGNALS:
    void finished(const ExplanationAnalysis &analysis);
    void failed(const QString &message);

private:
    enum class Stage { Idle, Before, After, Probe };

    void searchNext();
    void search(const ChessPosition &position, int depth);
    void onEvaluation(const EngineEvaluation &evaluation);
    void onSearchFinished();

    ExplainSettings m_settings;
    UciEngine *m_engine;
    Stage m_stage = Stage::Idle;
    ExplanationAnalysis m_analysis;
    QList<EngineEvaluation> m_current;
    /// Position of the line probe being searched.
    ChessPosition m_probePosition = ChessPosition::startingPosition();
};
