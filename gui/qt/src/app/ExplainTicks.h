#pragma once

#include "ChessPosition.h"
#include "EngineEvaluation.h"
#include "MoveExplanation.h"

#include <QList>
#include <QString>

#include <optional>

/// The ticks of one move explained: what EXPLAIN.smart was fed, line after
/// line of the engine, so that it can be replayed anywhere with nothing but
/// the program (pragma-explain --replay, the tests). A wrong explanation
/// becomes a test this way. Pure, unit-tested.
///
/// As text, one record per move, a line per field:
///
///     explain
///     before rnbqkbnr/ppp2ppp/3p4/4p3/4P3/5N2/PPPP1PPP/RNBQKB1R w KQkq - 0 3
///     played f3e5
///     before-eval depth 20 cp 40 pv d2d4 g8f6
///     after rnbqkbnr/ppp2ppp/3p4/4N3/4P3/8/PPPP1PPP/RNBQKB1R b KQkq - 0 3
///     tick depth 9 cp -170 pv d6e5
///     tick depth 12 mate -3 pv …
///     expect verdict mistake
///     expect arrows d2d4 alternative, d6e5 refutation 1
///     expect lost e5
///     expect summary Mistake (+0.4 → −1.9). Black wins a knight for a pawn: 3…dxe5. Better was 3.d4.
///     end
///
/// Scores are White's (mate n: White mates in n; -n: Black does). Lines
/// starting with "#" are comments; `before`, `played` and `before-eval` may
/// be missing (the position explained alone). The `expect` lines, if any,
/// are what the last explanation shown must be (outcome(), in English and
/// letters): a record with them is a test, the same for every client
/// (smart/tests).
struct ExplainTicks {
    std::optional<ChessPosition> before;
    std::optional<ChessMove> played;
    std::optional<EngineEvaluation> beforeEvaluation;
    ChessPosition after = ChessPosition::startingPosition();
    /// Who asked (`viewer white`/`viewer black`), when known.
    std::optional<Side> viewer;
    QList<EngineEvaluation> ticks;
    /// The `expect` lines.
    QStringList expected;

    QString toText() const;
    /// Every record of `text`; nothing, with `error` ("line 3: …"), on a mistake.
    static std::optional<QList<ExplainTicks>> fromText(const QString &text, QString *error = nullptr);

    /// "depth 20 cp 40 pv d2d4 g8f6", as in the records.
    static QString evaluationText(const EngineEvaluation &evaluation);
    static std::optional<EngineEvaluation> parseEvaluation(const QString &text);

    /// Feeds the ticks to EXPLAIN.smart as the desktop client does
    /// (startExplanation, then explainTick for each): what each tick made.
    QList<ExplanationTick> replay(SanStyle style = SanStyle::Letters, bool trace = false) const;
    /// The explanation shown last after replay(), if any tick showed one.
    static std::optional<MoveExplanation> lastShown(const QList<ExplanationTick> &ticks);
    /// An explanation as the `expect` lines write it: "verdict …", "arrows …",
    /// "lost …" (when pieces fall), "summary …", "playback …" (a mate).
    static QStringList outcome(const MoveExplanation &explanation);
};
