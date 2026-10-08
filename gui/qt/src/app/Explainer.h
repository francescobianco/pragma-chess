#pragma once

#include "ChessPosition.h"
#include "EngineEvaluation.h"
#include "ExplainTicks.h"
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
    ~Explainer() override;

    /// Keeps the deepest evaluation of every position in `path` too, read
    /// now and written a few seconds after it changes: a restart (make start
    /// at every change, or opening the app again) explains with what the
    /// engine found before, not with a search started over. None by default.
    void setStorage(const QString &path);
    /// The deepest evaluation known of `position`.
    std::optional<EngineEvaluation> known(const ChessPosition &position) const;

    bool isEnabled() const { return m_enabled; }
    /// Starts explaining the position on the board, or stops.
    void setEnabled(bool enabled);

    /// The position on the board and, unless it is the starting position of
    /// the game, the position before the last move together with that move.
    void setPosition(const ChessPosition &position, const std::optional<ChessPosition> &before,
                     const std::optional<ChessMove> &played);

    /// A line of the live analysis of the position on the board.
    void setLiveEvaluation(const EngineEvaluation &evaluation);
    /// Who asks (ExplanationInput::viewer); a change redraws the explanation.
    void setViewer(std::optional<Side> viewer);
    /// The ticks of the move explained last, as `pragma-explain --replay`
    /// reads them, what was shown in comments above; empty when none.
    QString recordedTicks() const;
    /// A line of an analysis of `position`, which need not be on the board:
    /// the position before the move, searched so that the move can be judged.
    void setEvaluation(const ChessPosition &position, const EngineEvaluation &evaluation);
    /// The position before the move explained, when it has no evaluation as
    /// deep as `depth` yet: the client searches it first (one engine, one
    /// position after the other), or the move cannot be judged.
    std::optional<ChessPosition> unjudgedBefore(int depth) const;

Q_SIGNALS:
    /// The explanation for the current position; a waiting summary until the
    /// search is deep enough.
    void explanationChanged(const MoveExplanation &explanation);

private:
    void start();
    void tick();
    void show(const MoveExplanation &explanation);
    /// Writes the ticks of the move explained into PRAGMA_EXPLAIN_RECORD, if set.
    void record(const ExplanationInput &input);
    /// The key of the move recorded last (recordedTicks).
    QString m_recordingKey;

    bool m_enabled = false;
    std::optional<Side> m_viewer;
    ChessPosition m_position = ChessPosition::startingPosition();
    std::optional<ChessPosition> m_before;
    std::optional<ChessMove> m_played;

    QHash<QString, EngineEvaluation> m_evaluations;
    QString m_storage;
    class QTimer *m_saveTimer = nullptr;
    void save() const;
    std::optional<MoveExplanation> m_shown;
    /// The ticks of each move explained, by move (PRAGMA_EXPLAIN_RECORD).
    QHash<QString, ExplainTicks> m_recordings;
};
