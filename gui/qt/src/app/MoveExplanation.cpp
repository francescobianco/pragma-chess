#include "MoveExplanation.h"

#include <QCoreApplication>

#include <array>
#include <limits>

namespace {

/// How far into a principal variation to look for the advantage to become concrete.
constexpr int kMaxSearchPlies = 16;
/// Plies after the realization during which the material must not come back.
constexpr int kHoldPlies = 4;
constexpr int kMaxArrows = 8;
/// Mates are replayed to the end, so that they can be played on the board.
constexpr int kMaxMatePlies = 100;
/// Realizations longer than this are drawn focused on their decisive moves:
/// a long sequence of arrows says nothing at a glance.
constexpr int kFocusAbove = 3;
/// The biggest material jump and the move right before it.
constexpr int kFocusPlies = 2;
/// Advantages smaller than this (centipawns) are not explained with material.
constexpr int kMinimumMaterial = 80;
/// Share of an evaluation swing that material must account for. Engine scores
/// value pieces above the classic 1/3/3/5/9, so this is well below one.
constexpr double kRealizedShare = 0.4;
/// Swings are capped so that mates and huge scores still ask for sensible material.
constexpr int kMaxSwing = 1500;

QString tr(const char *text)
{
    return QCoreApplication::translate("MoveExplanation", text);
}

Side opposite(Side side)
{
    return side == Side::White ? Side::Black : Side::White;
}

int signFor(Side side)
{
    return side == Side::White ? 1 : -1;
}

QString sideName(Side side)
{
    return side == Side::White ? tr("White") : tr("Black");
}

/// A principal variation replayed on the board: positions[k] is the position
/// before moves[k], and the last position follows the last move.
struct Line {
    QList<ChessMove> moves;
    QList<ChessPosition> positions;

    int plies() const { return int(moves.size()); }
};

Line replay(const ChessPosition &start, const QStringList &uciMoves, int maxPlies)
{
    Line line;
    line.positions << start;
    for (const QString &uci : uciMoves) {
        if (line.plies() >= maxPlies)
            break;
        ChessPosition position = line.positions.last();
        const std::optional<ChessMove> move = position.moveFromUci(uci);
        if (!move)
            break;
        position.play(*move);
        line.moves << *move;
        line.positions << position;
    }
    return line;
}

/// Whether the exchanges are over at ply `k`: no check to answer and no
/// capture or promotion coming next.
bool isSettled(const Line &line, int k)
{
    const ChessPosition &position = line.positions.at(k);
    if (position.inCheck())
        return position.legalMoves().isEmpty();
    if (k < line.plies()) {
        const ChessMove &next = line.moves.at(k);
        if (!position.capturedPiece(next).isNull() || next.promotion != PieceType::None)
            return false;
    }
    return true;
}

/// First ply from `firstPly` at which `beneficiary` is up at least `needed`
/// centipawns of material compared with `baseline`, the exchanges are over,
/// and the gain holds for the next few plies of the line. Intermediate
/// checks, recaptures and quiet in-between moves are skipped this way.
std::optional<int> findRealization(const Line &line, int firstPly, Side beneficiary, int baseline, int needed,
                                   QStringList *trace = nullptr)
{
    const auto gainAt = [&](int k) {
        return (line.positions.at(k).material() - baseline) * signFor(beneficiary);
    };
    if (trace) {
        *trace << QStringLiteral("  material for %1, needs +%2 cp from ply %3 (settled and held %4 plies):")
                      .arg(sideName(beneficiary)).arg(needed).arg(firstPly).arg(kHoldPlies);
    }
    std::optional<int> found;
    for (int k = firstPly; k <= line.plies() && !found; ++k) {
        const bool settled = isSettled(line, k);
        bool holds = gainAt(k) >= needed && settled;
        for (int later = k + 1; holds && later <= qMin(line.plies(), k + kHoldPlies); ++later)
            holds = gainAt(later) >= needed;
        if (trace) {
            const QString move = line.positions.at(k - 1).moveNumberText()
                + line.positions.at(k - 1).san(line.moves.at(k - 1));
            *trace << QStringLiteral("    ply %1 %2 gain %3%4%5")
                          .arg(k, 2).arg(move, -10).arg(gainAt(k) >= 0 ? QStringLiteral("+") : QString())
                          .arg(gainAt(k)).arg(holds ? QStringLiteral("  <- realized")
                                                    : settled ? QString() : QStringLiteral("  (not settled)"));
        }
        if (holds)
            found = k;
    }
    if (trace && !found)
        *trace << QStringLiteral("    not realized within %1 plies").arg(line.plies());
    return found;
}

/// Adds arrows for moves [from, to) of `line`, or only for [focusFrom, focusTo)
/// when given. Moves of `ideaSide` get `ideaKind`, the others are replies.
/// Pieces taken by `ideaSide` in the drawn moves are traced back to where
/// they stand on the board at ply `from`.
void addArrows(MoveExplanation &explanation, const Line &line, int from, int to, Side ideaSide,
               BoardArrow::Kind ideaKind, int focusFrom = -1, int focusTo = -1)
{
    if (focusFrom < 0) {
        focusFrom = from;
        focusTo = to;
    }
    std::array<int, 64> origin{};
    for (int square = 0; square < 64; ++square)
        origin[square] = line.positions.at(from).at(square).isNull() ? -1 : square;

    int step = 1;
    for (int k = from; k < qMin(to, line.plies()) && step <= kMaxArrows; ++k) {
        const ChessPosition &position = line.positions.at(k);
        const ChessMove &move = line.moves.at(k);
        const Side side = position.sideToMove();
        const bool drawn = k >= focusFrom && k < focusTo;
        if (drawn) {
            explanation.arrows << BoardArrow{move.from, move.to,
                                             side == ideaSide ? ideaKind : BoardArrow::Kind::Reply, step++};
        }

        if (!position.capturedPiece(move).isNull()) {
            const int square = position.at(move.to).isNull() ? (move.from / 8) * 8 + move.to % 8 : move.to;
            if (drawn && side == ideaSide && origin[square] >= 0 && !explanation.lostPieces.contains(origin[square]))
                explanation.lostPieces << origin[square];
            origin[square] = -1;
        }
        const int fileDelta = move.to % 8 - move.from % 8;
        if (position.at(move.from).type == PieceType::King && qAbs(fileDelta) == 2) {
            const int rank = move.from / 8;
            const int rookFrom = rank * 8 + (fileDelta > 0 ? 7 : 0);
            const int rookTo = rank * 8 + (fileDelta > 0 ? 5 : 3);
            origin[rookTo] = origin[rookFrom];
            origin[rookFrom] = -1;
        }
        origin[move.to] = origin[move.from];
        origin[move.from] = -1;
    }
}

/// Whether `square` lies strictly between `from` and `to` on a line or diagonal.
bool isBetween(int square, int from, int to)
{
    const int fileStep = (to % 8 > from % 8) - (to % 8 < from % 8);
    const int rankStep = (to / 8 > from / 8) - (to / 8 < from / 8);
    const bool aligned = from % 8 == to % 8 || from / 8 == to / 8
        || qAbs(to % 8 - from % 8) == qAbs(to / 8 - from / 8);
    if (!aligned || from == to)
        return false;
    for (int s = from + fileStep + 8 * rankStep; s != to; s += fileStep + 8 * rankStep) {
        if (s == square)
            return true;
    }
    return false;
}

/// Moves to draw for a realization [from, to): all of them when short,
/// otherwise the move with the biggest material jump for `beneficiary` (the
/// latest on ties: the piece that finally falls), preceded by the move before
/// it only when that move is part of the tactic: a capture, a check, a piece
/// stepping onto the square that is then taken, or a square cleared for the capture.
std::pair<int, int> focusWindow(const Line &line, int from, int to, Side beneficiary, QStringList *trace)
{
    if (to - from <= kFocusAbove)
        return {from, to};
    int decisive = from;
    int biggest = std::numeric_limits<int>::min();
    for (int k = from; k < to; ++k) {
        const int jump = (line.positions.at(k + 1).material() - line.positions.at(k).material()) * signFor(beneficiary);
        if (jump >= biggest) {
            biggest = jump;
            decisive = k;
        }
    }

    int focusFrom = decisive;
    if (decisive > from) {
        const ChessPosition &previousPosition = line.positions.at(decisive - 1);
        const ChessMove &previous = line.moves.at(decisive - 1);
        const ChessMove &capture = line.moves.at(decisive);
        const bool related = !previousPosition.capturedPiece(previous).isNull()
            || line.positions.at(decisive).inCheck() || previous.to == capture.to
            || isBetween(previous.from, capture.from, capture.to);
        if (related)
            focusFrom = qMax(from, decisive + 1 - kFocusPlies);
    }
    if (trace) {
        *trace << QStringLiteral("  focus: %1 plies are too many arrows; biggest jump +%2 at %3, drawing plies %4-%5")
                      .arg(to - from).arg(biggest)
                      .arg(line.positions.at(decisive).moveNumberText()
                           + line.positions.at(decisive).san(line.moves.at(decisive)))
                      .arg(focusFrom + 1).arg(decisive + 1);
    }
    return {focusFrom, decisive + 1};
}

/// The moves of `line` when they end in checkmate, to be played on the board.
QStringList mateToPlay(const Line &line)
{
    QStringList moves;
    if (line.plies() == 0 || !line.positions.last().isCheckmate())
        return moves;
    for (const ChessMove &move : line.moves)
        moves << move.uci();
    return moves;
}

QString countedPiece(PieceType type, int count)
{
    // Spelled out per case so translators get whole phrases.
    switch (type) {
    case PieceType::Pawn:
        return count == 1 ? tr("a pawn") : count == 2 ? tr("two pawns") : tr("%1 pawns").arg(count);
    case PieceType::Knight:
        return count == 1 ? tr("a knight") : tr("%1 knights").arg(count);
    case PieceType::Bishop:
        return count == 1 ? tr("a bishop") : tr("%1 bishops").arg(count);
    case PieceType::Rook:
        return count == 1 ? tr("a rook") : tr("%1 rooks").arg(count);
    case PieceType::Queen:
        return count == 1 ? tr("the queen") : tr("%1 queens").arg(count);
    case PieceType::King:
    case PieceType::None:
        break;
    }
    return {};
}

QString joinPieces(const std::array<int, 7> &counts)
{
    QStringList parts;
    for (PieceType type : {PieceType::Queen, PieceType::Rook, PieceType::Bishop, PieceType::Knight, PieceType::Pawn}) {
        if (counts[int(type)] > 0)
            parts << countedPiece(type, counts[int(type)]);
    }
    if (parts.size() <= 1)
        return parts.value(0);
    const QString last = parts.takeLast();
    return tr("%1 and %2").arg(parts.join(QStringLiteral(", ")), last);
}

/// "wins a knight", "wins the exchange", "wins a rook for a pawn".
QString materialPhrase(const ChessPosition &from, const ChessPosition &to, Side beneficiary)
{
    const auto count = [](const ChessPosition &position, Side side, PieceType type) {
        int n = 0;
        for (int square = 0; square < 64; ++square)
            n += position.at(square) == Piece{type, side} ? 1 : 0;
        return n;
    };

    std::array<int, 7> won{};
    std::array<int, 7> given{};
    const Side loser = opposite(beneficiary);
    for (PieceType type : {PieceType::Pawn, PieceType::Knight, PieceType::Bishop, PieceType::Rook, PieceType::Queen}) {
        const int lostByLoser = count(from, loser, type) - count(to, loser, type);
        const int lostByBeneficiary = count(from, beneficiary, type) - count(to, beneficiary, type);
        const int net = lostByLoser - lostByBeneficiary;
        (net > 0 ? won : given)[int(type)] = qAbs(net);
    }

    const int minorsGiven = given[int(PieceType::Knight)] + given[int(PieceType::Bishop)];
    const bool onlyExchange = won == std::array<int, 7>{0, 0, 0, 0, 1, 0, 0} && minorsGiven == 1
        && given[int(PieceType::Pawn)] == 0 && given[int(PieceType::Rook)] == 0 && given[int(PieceType::Queen)] == 0;
    if (onlyExchange)
        return tr("wins the exchange");

    const QString gains = joinPieces(won);
    if (gains.isEmpty())
        return tr("wins material");
    const QString losses = joinPieces(given);
    return losses.isEmpty() ? tr("wins %1").arg(gains) : tr("wins %1 for %2").arg(gains, losses);
}

QString verdictName(MoveExplanation::Verdict verdict)
{
    switch (verdict) {
    case MoveExplanation::Verdict::Best: return tr("Best move");
    case MoveExplanation::Verdict::Good: return tr("Good move");
    case MoveExplanation::Verdict::Inaccuracy: return tr("Inaccuracy");
    case MoveExplanation::Verdict::Mistake: return tr("Mistake");
    case MoveExplanation::Verdict::Blunder: return tr("Blunder");
    case MoveExplanation::Verdict::None: break;
    }
    return {};
}

bool isError(MoveExplanation::Verdict verdict)
{
    return verdict == MoveExplanation::Verdict::Inaccuracy || verdict == MoveExplanation::Verdict::Mistake
        || verdict == MoveExplanation::Verdict::Blunder;
}

QString assessment(const EngineEvaluation &evaluation)
{
    const int score = qAbs(evaluation.centipawns);
    const QString side = sideName(evaluation.centipawns >= 0 ? Side::White : Side::Black);
    if (score >= 300)
        return tr("%1 is winning.").arg(side);
    if (score >= 150)
        return tr("%1 is better.").arg(side);
    if (score >= 50)
        return tr("%1 is slightly better.").arg(side);
    return tr("The position is balanced.");
}

} // namespace

MoveExplanation::Verdict classifyMove(const EngineEvaluation &before, const EngineEvaluation &after,
                                      Side mover, const ChessMove &played)
{
    if (!before.pv.isEmpty() && before.pv.first() == played.uci())
        return MoveExplanation::Verdict::Best;
    // Same scale as lichess: points of winning chances (0–100) given away.
    const double drop = 100.0 * (before.shareFor(mover) - after.shareFor(mover));
    if (drop >= 30)
        return MoveExplanation::Verdict::Blunder;
    if (drop >= 20)
        return MoveExplanation::Verdict::Mistake;
    if (drop >= 10)
        return MoveExplanation::Verdict::Inaccuracy;
    return MoveExplanation::Verdict::Good;
}

MoveExplanation explainPosition(const ExplanationInput &input)
{
    MoveExplanation explanation;
    const ChessPosition &after = input.after;
    const EngineEvaluation &afterEvaluation = input.afterEvaluation;
    QStringList *trace = input.trace ? &explanation.trace : nullptr;
    const auto note = [&](const QString &text) {
        if (trace)
            *trace << text;
    };

    if (after.isCheckmate()) {
        explanation.summary = tr("Checkmate.");
        return explanation;
    }
    if (after.isStalemate()) {
        explanation.summary = tr("Stalemate.");
        return explanation;
    }

    const bool comparable = input.before && input.played && input.beforeEvaluation;
    const Side toMove = after.sideToMove();
    const Side mover = opposite(toMove);
    if (!input.evaluationNote.isEmpty())
        note(input.evaluationNote);
    if (comparable) {
        explanation.verdict = classifyMove(*input.beforeEvaluation, afterEvaluation, mover, *input.played);
        note(QStringLiteral("verdict %1: %2's winning chances %3 -> %4 (drop %5)")
                 .arg(verdictName(explanation.verdict), sideName(mover))
                 .arg(100 * input.beforeEvaluation->shareFor(mover), 0, 'f', 1)
                 .arg(100 * afterEvaluation.shareFor(mover), 0, 'f', 1)
                 .arg(100 * (input.beforeEvaluation->shareFor(mover) - afterEvaluation.shareFor(mover)), 0, 'f', 1));
    } else {
        note(QStringLiteral("no previous evaluation: explaining the position alone"));
    }
    // Moves of the principal variation to show when nothing more concrete is found.
    const int concretePly = input.concretePly.value_or(0);
    const int fallbackPlies = concretePly > 0 ? qMin(concretePly, kMaxArrows) : 2;
    // Only the branches that found neither material nor a mate ever say this,
    // so it must not promise a win that never comes: the line probe says the
    // evaluation is already on the board, not that something is about to fall.
    // Pieces may well be captured in the line — what the probe found is that
    // no lasting gain explains the score, not that nothing is ever taken.
    const QString concreteText = concretePly > 0
        ? tr(" No material explains it: the assessment is positional, clear after %1.")
              .arg(after.lineText(afterEvaluation.pv, concretePly, input.sanStyle))
        : QString();
    if (input.concretePly)
        note(QStringLiteral("line probe: concrete at ply %1").arg(*input.concretePly));

    QString prefix;
    if (isError(explanation.verdict)) {
        prefix = tr("%1 (%2 → %3). ").arg(verdictName(explanation.verdict), input.beforeEvaluation->text(),
                                          afterEvaluation.text());
    } else if (explanation.verdict != MoveExplanation::Verdict::None) {
        prefix = tr("%1 (%2). ").arg(verdictName(explanation.verdict), afterEvaluation.text());
    } else {
        prefix = afterEvaluation.text() + QStringLiteral(". ");
    }

    const Line current = replay(after, afterEvaluation.pv, kMaxSearchPlies);

    if (isError(explanation.verdict)) {
        const ChessPosition &before = *input.before;
        const EngineEvaluation &beforeEvaluation = *input.beforeEvaluation;
        const ChessMove &played = *input.played;
        const Line best = replay(before, beforeEvaluation.pv, kMaxSearchPlies);
        const bool betterExists = best.plies() > 0 && best.moves.first() != played;
        const QString better = betterExists ? tr(" Better was %1.").arg(before.lineText(beforeEvaluation.pv, 1, input.sanStyle))
                                            : QString();
        const auto addAlternative = [&] {
            if (betterExists)
                explanation.arrows << BoardArrow{best.moves.first().from, best.moves.first().to,
                                                 BoardArrow::Kind::Alternative, 0};
        };

        // The move allows a mate.
        if (afterEvaluation.isMate && afterEvaluation.mating == toMove && afterEvaluation.mateIn > 0) {
            note(QStringLiteral("branch: the move allows mate"));
            addAlternative();
            addArrows(explanation, current, 0, current.plies(), toMove, BoardArrow::Kind::Refutation);
            explanation.playback = mateToPlay(replay(after, afterEvaluation.pv, kMaxMatePlies));
            explanation.summary = prefix
                + tr("%1 mates in %2: %3.").arg(sideName(toMove)).arg(afterEvaluation.mateIn)
                      .arg(after.lineText(afterEvaluation.pv, kMaxArrows, input.sanStyle))
                + better;
            return explanation;
        }

        // The move loses material: replay it together with the refutation, so
        // that a capture made by the move itself counts against what follows.
        const int drop = qMin(beforeEvaluation.centipawnsFor(mover) - afterEvaluation.centipawnsFor(mover), kMaxSwing);
        note(QStringLiteral("refutation: evaluation drop %1 cp (minimum %2)").arg(drop).arg(kMinimumMaterial));
        if (drop >= kMinimumMaterial) {
            QStringList refutation{played.uci()};
            refutation += afterEvaluation.pv;
            const Line line = replay(before, refutation, kMaxSearchPlies + 1);
            const int needed = qMax(kMinimumMaterial, int(drop * kRealizedShare));
            if (const std::optional<int> ply = findRealization(line, 2, toMove, before.material(), needed, trace)) {
                note(QStringLiteral("branch: the move loses material"));
                const auto [focusFrom, focusTo] = focusWindow(line, 1, *ply, toMove, trace);
                // A focused refutation is clearer without the better move on top.
                if (focusFrom == 1)
                    addAlternative();
                addArrows(explanation, line, 1, *ply, toMove, BoardArrow::Kind::Refutation, focusFrom, focusTo);
                explanation.summary = prefix
                    + tr("%1 %2: %3.").arg(sideName(toMove), materialPhrase(before, line.positions.at(*ply), toMove),
                                           after.lineText(afterEvaluation.pv, *ply - 1, input.sanStyle))
                    + better;
                return explanation;
            }
        }

        // The move misses a mate or a win of material.
        if (betterExists && beforeEvaluation.isMate && beforeEvaluation.mating == mover) {
            note(QStringLiteral("branch: the move misses a mate"));
            addAlternative();
            explanation.summary = prefix
                + tr("Missed mate in %1: %2.").arg(beforeEvaluation.mateIn)
                      .arg(before.lineText(beforeEvaluation.pv, kMaxArrows, input.sanStyle));
            return explanation;
        }
        const int missed = qMin(beforeEvaluation.centipawnsFor(mover) - before.material() * signFor(mover), kMaxSwing);
        note(QStringLiteral("missed win: %1 cp above the material before the move").arg(missed));
        if (betterExists && missed >= kMinimumMaterial) {
            const int needed = qMax(kMinimumMaterial, int(missed * kRealizedShare));
            if (const std::optional<int> ply = findRealization(best, 1, mover, before.material(), needed, trace)) {
                note(QStringLiteral("branch: the move misses a win of material"));
                addAlternative();
                explanation.summary = prefix
                    + tr("Missed: %1 %2.").arg(before.lineText(beforeEvaluation.pv, *ply, input.sanStyle),
                                               materialPhrase(before, best.positions.at(*ply), mover));
                return explanation;
            }
        }

        // Nothing concrete within reach: show how the opponent takes over.
        note(QStringLiteral("branch: no material or mate, showing %1 plies of the line").arg(fallbackPlies));
        addAlternative();
        // Not a refutation: nothing is won here, so the opponent's continuation
        // is drawn as a reply. Red is the colour of material falling.
        addArrows(explanation, current, 0, fallbackPlies, toMove, BoardArrow::Kind::Reply);
        explanation.summary = prefix + better.trimmed();
        if (!concreteText.isEmpty())
            explanation.summary += concreteText;
        else if (current.plies() > 0)
            explanation.summary += tr(" Main line: %1.").arg(after.lineText(afterEvaluation.pv, 4, input.sanStyle));
        explanation.summary = explanation.summary.trimmed();
        return explanation;
    }

    // Not a mistake: explain why the evaluation is what it is.
    const Side favored = afterEvaluation.isMate ? afterEvaluation.mating
                                                : afterEvaluation.centipawns >= 0 ? Side::White : Side::Black;
    // A line won by the opponent of the side that just moved reads as a threat.
    const BoardArrow::Kind favoredKind = comparable && favored != mover ? BoardArrow::Kind::Refutation
                                                                        : BoardArrow::Kind::Idea;
    if (afterEvaluation.isMate && afterEvaluation.mateIn > 0) {
        note(QStringLiteral("branch: mate on the board"));
        addArrows(explanation, current, 0, current.plies(), favored, favoredKind);
        explanation.playback = mateToPlay(replay(after, afterEvaluation.pv, kMaxMatePlies));
        explanation.summary = prefix
            + tr("%1 mates in %2: %3.").arg(sideName(favored)).arg(afterEvaluation.mateIn)
                  .arg(after.lineText(afterEvaluation.pv, kMaxArrows, input.sanStyle));
        return explanation;
    }

    const int score = qMin(afterEvaluation.centipawnsFor(favored), kMaxSwing);

    // The last move itself won (or starts winning) material.
    if (comparable && favored == mover) {
        const ChessPosition &before = *input.before;
        const int extra = score - before.material() * signFor(favored);
        note(QStringLiteral("won by the move: %1 cp above the material before the move").arg(extra));
        if (extra >= kMinimumMaterial) {
            QStringList moves{input.played->uci()};
            moves += afterEvaluation.pv;
            const Line line = replay(before, moves, kMaxSearchPlies + 1);
            const int needed = qMax(kMinimumMaterial, int(extra * kRealizedShare));
            if (const std::optional<int> ply = findRealization(line, 1, favored, before.material(), needed, trace)) {
                note(QStringLiteral("branch: the move wins material"));
                const auto [focusFrom, focusTo] = focusWindow(line, 1, *ply, favored, trace);
                addArrows(explanation, line, 1, *ply, favored, BoardArrow::Kind::Idea, focusFrom, focusTo);
                explanation.summary = prefix
                    + tr("%1 %2: %3.").arg(sideName(favored), materialPhrase(before, line.positions.at(*ply), favored),
                                           before.lineText(moves, *ply, input.sanStyle));
                return explanation;
            }
        }
    }

    // Material still to be won in the principal variation.
    const int extra = score - after.material() * signFor(favored);
    note(QStringLiteral("still to win: %1 cp above the material on the board").arg(extra));
    if (extra >= kMinimumMaterial) {
        const int needed = qMax(kMinimumMaterial, int(extra * kRealizedShare));
        if (const std::optional<int> ply = findRealization(current, 1, favored, after.material(), needed, trace)) {
            note(QStringLiteral("branch: material won in the line"));
            const auto [focusFrom, focusTo] = focusWindow(current, 0, *ply, favored, trace);
            addArrows(explanation, current, 0, *ply, favored, favoredKind, focusFrom, focusTo);
            explanation.summary = prefix
                + tr("%1 %2: %3.").arg(sideName(favored), materialPhrase(after, current.positions.at(*ply), favored),
                                       after.lineText(afterEvaluation.pv, *ply, input.sanStyle));
            return explanation;
        }
    }

    note(QStringLiteral("branch: no material or mate, showing %1 plies of the line").arg(fallbackPlies));
    addArrows(explanation, current, 0, fallbackPlies, toMove, BoardArrow::Kind::Idea);
    explanation.summary = prefix + assessment(afterEvaluation);
    if (!concreteText.isEmpty())
        explanation.summary += concreteText;
    else if (current.plies() > 0)
        explanation.summary += tr(" Main line: %1.").arg(after.lineText(afterEvaluation.pv, 4, input.sanStyle));
    return explanation;
}
