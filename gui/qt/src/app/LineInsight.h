#pragma once

#include "ChessPosition.h"
#include "MoveExplanation.h"

#include <QList>
#include <QString>
#include <QStringList>

#include <optional>

/// The plans of a line of the engine (smart/INSIGHT.smart): the trips of
/// the pieces that make one — a knight's route, a rook lift, the king's
/// march in an endgame, a pawn running —, drawn as arrows over the position
/// at the end of the line. A reading of the line only: no search. Pure,
/// unit-tested.
struct LineInsight {
    /// The arrows, of kind Plan, with the squares they pass through.
    QList<BoardArrow> arrows;
    /// How they were chosen, when asked (the program's NOTEs).
    QStringList trace;
    /// The program's mistake, if it stopped on one (nothing is drawn then).
    QString error;
};

/// The insight of `line` (UCI moves) played from `start`.
LineInsight lineInsight(const ChessPosition &start, const QStringList &line, bool trace = false);

/// A line with the arrows INSIGHT.smart must draw for it: the cases of
/// smart/tests/*.insight, which every client replays. As text:
///
///     insight a knight's route in the Spanish
///     fen r1bqkbnr/pppp1ppp/2n5/4p3/4P3/5N2/PPPP1PPP/RNBQKB1R w KQkq - 2 3
///     line b1d2 g8f6 d2f1 d7d6 f1g3
///     expect b1g3 via d2 f1
///     end
///
/// `fen` may be missing (the starting position); one `expect` line per
/// arrow, in the order they are drawn (outcome()), or `expect -` for none.
struct InsightCase {
    QString name;
    ChessPosition start = ChessPosition::startingPosition();
    QStringList line;
    QStringList expected;

    QString toText() const;
    /// Every case of `text`; nothing, with `error` ("line 3: …"), on a mistake.
    static std::optional<QList<InsightCase>> fromText(const QString &text, QString *error = nullptr);
    /// The arrows as the `expect` lines write them: "b1g3 via d2 f1", or "-".
    static QStringList outcome(const QList<BoardArrow> &arrows);
};
