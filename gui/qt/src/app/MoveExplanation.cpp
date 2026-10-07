#include "MoveExplanation.h"

#include "smart/SmartPrograms.h"

#include <QCoreApplication>

namespace {

struct Text {
    Q_DECLARE_TR_FUNCTIONS(MoveExplanation)
};

/// Every sentence smart/EXPLAIN.smart says, so that lupdate keeps their
/// translations (the program's TEXT() looks them up in this context).
const char *const kSentences[] = {
    QT_TRANSLATE_NOOP("MoveExplanation", "White"),
    QT_TRANSLATE_NOOP("MoveExplanation", "Black"),
    QT_TRANSLATE_NOOP("MoveExplanation", "a pawn"),
    QT_TRANSLATE_NOOP("MoveExplanation", "two pawns"),
    QT_TRANSLATE_NOOP("MoveExplanation", "%1 pawns"),
    QT_TRANSLATE_NOOP("MoveExplanation", "a knight"),
    QT_TRANSLATE_NOOP("MoveExplanation", "%1 knights"),
    QT_TRANSLATE_NOOP("MoveExplanation", "a bishop"),
    QT_TRANSLATE_NOOP("MoveExplanation", "%1 bishops"),
    QT_TRANSLATE_NOOP("MoveExplanation", "a rook"),
    QT_TRANSLATE_NOOP("MoveExplanation", "%1 rooks"),
    QT_TRANSLATE_NOOP("MoveExplanation", "the queen"),
    QT_TRANSLATE_NOOP("MoveExplanation", "%1 queens"),
    QT_TRANSLATE_NOOP("MoveExplanation", "%1 and %2"),
    QT_TRANSLATE_NOOP("MoveExplanation", "wins the exchange"),
    QT_TRANSLATE_NOOP("MoveExplanation", "wins material"),
    QT_TRANSLATE_NOOP("MoveExplanation", "wins %1"),
    QT_TRANSLATE_NOOP("MoveExplanation", "wins %1 for %2"),
    QT_TRANSLATE_NOOP("MoveExplanation", "Best move"),
    QT_TRANSLATE_NOOP("MoveExplanation", "Good move"),
    QT_TRANSLATE_NOOP("MoveExplanation", "Inaccuracy"),
    QT_TRANSLATE_NOOP("MoveExplanation", "Mistake"),
    QT_TRANSLATE_NOOP("MoveExplanation", "Blunder"),
    QT_TRANSLATE_NOOP("MoveExplanation", "%1 is winning."),
    QT_TRANSLATE_NOOP("MoveExplanation", "%1 is better."),
    QT_TRANSLATE_NOOP("MoveExplanation", "%1 is slightly better."),
    QT_TRANSLATE_NOOP("MoveExplanation", "The position is balanced."),
    QT_TRANSLATE_NOOP("MoveExplanation", "Checkmate."),
    QT_TRANSLATE_NOOP("MoveExplanation", "Stalemate."),
    QT_TRANSLATE_NOOP("MoveExplanation", " No material explains it: the assessment is positional, clear after %1."),
    QT_TRANSLATE_NOOP("MoveExplanation", "%1 (%2 → %3). "),
    QT_TRANSLATE_NOOP("MoveExplanation", "%1 (%2). "),
    QT_TRANSLATE_NOOP("MoveExplanation", " Better was %1."),
    QT_TRANSLATE_NOOP("MoveExplanation", "%1 mates in %2: %3."),
    QT_TRANSLATE_NOOP("MoveExplanation", "%1 %2: %3."),
    QT_TRANSLATE_NOOP("MoveExplanation", "Missed mate in %1: %2."),
    QT_TRANSLATE_NOOP("MoveExplanation", "Missed: %1 %2."),
    QT_TRANSLATE_NOOP("MoveExplanation", " Main line: %1."),
    QT_TRANSLATE_NOOP("MoveExplanation", "the pawn"),
    QT_TRANSLATE_NOOP("MoveExplanation", "the knight"),
    QT_TRANSLATE_NOOP("MoveExplanation", "the bishop"),
    QT_TRANSLATE_NOOP("MoveExplanation", "the rook"),
    QT_TRANSLATE_NOOP("MoveExplanation", "%1 attacks %2 on %3"),
    QT_TRANSLATE_NOOP("MoveExplanation", "%1: %2 parries it."),
    QT_TRANSLATE_NOOP("MoveExplanation", "%1 leaves %2 on %3 attacked: %4."),
    QT_TRANSLATE_NOOP("MoveExplanation", "%1: %2 takes the attacker."),
    QT_TRANSLATE_NOOP("MoveExplanation", "%1: %2 moves it again, and %3 gains time."),
    QT_TRANSLATE_NOOP("MoveExplanation", "%1 attacks %2 on %3 and %4 on %5."),
    QT_TRANSLATE_NOOP("MoveExplanation", "%1 threatens %2, and the king cannot take back."),
    QT_TRANSLATE_NOOP("MoveExplanation", "%1 threatens %2."),
    QT_TRANSLATE_NOOP("MoveExplanation", " No material is lost: the evaluation is positional."),
};

MoveExplanation::Verdict verdictNamed(const QString &name)
{
    if (name == QLatin1String("best"))
        return MoveExplanation::Verdict::Best;
    if (name == QLatin1String("blunder"))
        return MoveExplanation::Verdict::Blunder;
    if (name == QLatin1String("mistake"))
        return MoveExplanation::Verdict::Mistake;
    if (name == QLatin1String("inaccuracy"))
        return MoveExplanation::Verdict::Inaccuracy;
    return MoveExplanation::Verdict::Good;
}

} // namespace

QStringList explanationSentences()
{
    QStringList sentences;
    for (const char *sentence : kSentences)
        sentences << QString::fromUtf8(sentence);
    return sentences;
}

MoveExplanation::Verdict classifyMove(const EngineEvaluation &before, const EngineEvaluation &after,
                                      Side mover, const ChessMove &played, const std::optional<ChessPosition> &start)
{
    // The judgement is smart/TUTOR.smart's, shared with the tutor and the other clients.
    SmartProgram *tutor = SmartPrograms::program(QStringLiteral("TUTOR.smart"));
    QString error;
    const std::optional<SmartValue> verdict =
        tutor ? tutor->interpreter.call(QStringLiteral("Classify"),
                                        {SmartChess::evaluation(before), SmartChess::evaluation(after),
                                         SmartChess::side(mover), SmartValue(played.uci()),
                                         start ? SmartChess::position(*start) : SmartValue()},
                                        &error)
              : std::nullopt;
    if (!verdict) {
        if (tutor)
            qWarning("SMART TUTOR.smart Classify: %s", qPrintable(error));
        return MoveExplanation::Verdict::None;
    }
    return verdictNamed(verdict->toText());
}

void startExplanation()
{
    if (SmartProgram *explain = SmartPrograms::program(QStringLiteral("EXPLAIN.smart")))
        explain->interpreter.call(QStringLiteral("Start"));
}

ExplanationTick explainTick(const ExplanationInput &input)
{
    ExplanationTick tick;
    SmartProgram *explain = SmartPrograms::program(QStringLiteral("EXPLAIN.smart"));
    if (!explain) {
        tick.error = QStringLiteral("EXPLAIN.smart cannot be loaded");
        tick.explanation.summary = Text::tr("Explain cannot run: its program has a mistake (see the log).");
        tick.shown = true;
        return tick;
    }
    explain->output.clear();
    explain->output.sanStyle = input.sanStyle;
    explain->output.trace = input.trace;
    // The move is given even when its position before has no evaluation:
    // it cannot be judged, but the explanation is still about it.
    const bool moved = input.before && input.played;
    const std::optional<SmartValue> shown = explain->interpreter.call(
        QStringLiteral("Tick"),
        {moved ? SmartChess::position(*input.before) : SmartValue(),
         moved ? SmartValue(input.played->uci()) : SmartValue(),
         moved && input.beforeEvaluation ? SmartChess::evaluation(*input.beforeEvaluation) : SmartValue(),
         SmartChess::position(input.after), SmartChess::evaluation(input.afterEvaluation)},
        &tick.error);
    if (!shown) {
        qWarning("SMART EXPLAIN.smart Tick: %s", qPrintable(tick.error));
        tick.explanation.summary = Text::tr("Explain stopped on a mistake of its program: %1").arg(tick.error);
        tick.shown = true;
        return tick;
    }
    tick.shown = shown->isNumber() && shown->number() != 0;
    tick.explanation = explain->output.explanation;
    return tick;
}

MoveExplanation explainPosition(const ExplanationInput &input)
{
    // The explanation is smart/EXPLAIN.smart's: the same in every client.
    SmartProgram *explain = SmartPrograms::program(QStringLiteral("EXPLAIN.smart"));
    if (!explain) {
        MoveExplanation failed;
        failed.summary = Text::tr("Explain cannot run: its program has a mistake (see the log).");
        return failed;
    }
    explain->output.clear();
    explain->output.sanStyle = input.sanStyle;
    explain->output.trace = input.trace;
    const bool moved = input.before && input.played;
    QString error;
    const std::optional<SmartValue> done = explain->interpreter.call(
        QStringLiteral("Explain"),
        {moved ? SmartChess::position(*input.before) : SmartValue(),
         moved ? SmartValue(input.played->uci()) : SmartValue(),
         moved && input.beforeEvaluation ? SmartChess::evaluation(*input.beforeEvaluation) : SmartValue(),
         SmartChess::position(input.after), SmartChess::evaluation(input.afterEvaluation),
         input.concretePly ? SmartValue(*input.concretePly) : SmartValue(), SmartValue(input.evaluationNote)},
        &error);
    MoveExplanation explanation = explain->output.explanation;
    if (!done) {
        qWarning("SMART EXPLAIN.smart Explain: %s", qPrintable(error));
        explanation = MoveExplanation();
        explanation.summary = Text::tr("Explain stopped on a mistake of its program: %1").arg(error);
    }
    return explanation;
}
