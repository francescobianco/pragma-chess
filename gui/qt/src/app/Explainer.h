#pragma once

#include "ChessPosition.h"
#include "ExplanationSearch.h"
#include "MoveExplanation.h"

#include <QHash>
#include <QObject>

#include <optional>

/// Drives the "Explain" command of the desktop client.
///
/// The move on the board is analyzed by an ExplanationSearch with the same
/// settings as the command line tool, so both explain a move identically and
/// the explanation does not change while an analysis deepens. Finished
/// analyses are kept, so explaining a move again is immediate.
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

    /// The live analysis of the position on the board. A mate or a 0.00 it
    /// finds after a long search guides the explanation (see
    /// ExplanationAnalysis::acceptsHint); anything else is ignored.
    void setLiveEvaluation(const EngineEvaluation &evaluation);

Q_SIGNALS:
    /// The explanation for the current position; without arrows and with a
    /// progress summary while the engine is still searching.
    void explanationChanged(const MoveExplanation &explanation);

private:
    QString currentKey() const;
    static QString keyFor(const ExplanationAnalysis &analysis);
    void explain();
    void show(const MoveExplanation &explanation);

    ExplanationSearch *m_search;
    QString m_engineExecutable;
    bool m_enabled = false;

    ChessPosition m_position = ChessPosition::startingPosition();
    std::optional<ChessPosition> m_before;
    std::optional<ChessMove> m_played;

    QHash<QString, ExplanationAnalysis> m_analyses;
    std::optional<EngineEvaluation> m_hint;
    /// Score of the hint the shown explanation used, to explain again only when it changes.
    QString m_usedHint;
    std::optional<MoveExplanation> m_shown;
};
