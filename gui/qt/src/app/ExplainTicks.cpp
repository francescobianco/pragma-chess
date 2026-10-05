#include "ExplainTicks.h"

#include <QStringList>

QString ExplainTicks::evaluationText(const EngineEvaluation &evaluation)
{
    QString text = QStringLiteral("depth %1 ").arg(evaluation.depth);
    if (evaluation.isMate)
        text += QStringLiteral("mate %1").arg(evaluation.mating == Side::White ? evaluation.mateIn : -evaluation.mateIn);
    else
        text += QStringLiteral("cp %1").arg(evaluation.centipawns);
    if (!evaluation.pv.isEmpty())
        text += QStringLiteral(" pv ") + evaluation.pv.join(QLatin1Char(' '));
    return text;
}

std::optional<EngineEvaluation> ExplainTicks::parseEvaluation(const QString &text)
{
    const QStringList words = text.simplified().split(QLatin1Char(' '), Qt::SkipEmptyParts);
    EngineEvaluation evaluation;
    bool scored = false;
    for (qsizetype i = 0; i < words.size(); ++i) {
        const QString &word = words.at(i);
        bool number = true;
        if (word == QLatin1String("depth")) {
            evaluation.depth = words.value(++i).toInt(&number);
        } else if (word == QLatin1String("cp")) {
            evaluation.centipawns = words.value(++i).toInt(&number);
            scored = true;
        } else if (word == QLatin1String("mate")) {
            const int moves = words.value(++i).toInt(&number);
            evaluation.isMate = true;
            evaluation.mateIn = qAbs(moves);
            evaluation.mating = moves < 0 ? Side::Black : Side::White;
            scored = true;
        } else if (word == QLatin1String("pv")) {
            evaluation.pv = words.mid(i + 1);
            break;
        } else {
            return std::nullopt;
        }
        if (!number)
            return std::nullopt;
    }
    if (!scored)
        return std::nullopt;
    return evaluation;
}

QString ExplainTicks::toText() const
{
    QStringList lines{QStringLiteral("explain")};
    if (before)
        lines << QStringLiteral("before ") + before->fen();
    if (played)
        lines << QStringLiteral("played ") + played->uci();
    if (beforeEvaluation)
        lines << QStringLiteral("before-eval ") + evaluationText(*beforeEvaluation);
    lines << QStringLiteral("after ") + after.fen();
    for (const EngineEvaluation &tick : ticks)
        lines << QStringLiteral("tick ") + evaluationText(tick);
    for (const QString &expectation : expected)
        lines << QStringLiteral("expect ") + expectation;
    lines << QStringLiteral("end");
    return lines.join(QLatin1Char('\n')) + QLatin1Char('\n');
}

std::optional<QList<ExplainTicks>> ExplainTicks::fromText(const QString &text, QString *error)
{
    QList<ExplainTicks> records;
    std::optional<ExplainTicks> current;
    bool hasAfter = false;
    QString playedText;
    const QStringList lines = text.split(QLatin1Char('\n'));
    const auto fail = [&](int line, const QString &message) {
        if (error)
            *error = QStringLiteral("line %1: %2").arg(line).arg(message);
        return std::nullopt;
    };
    for (qsizetype n = 0; n < lines.size(); ++n) {
        const QString line = lines.at(n).trimmed();
        const int number = int(n) + 1;
        if (line.isEmpty() || line.startsWith(QLatin1Char('#')))
            continue;
        const QString field = line.section(QLatin1Char(' '), 0, 0);
        const QString value = line.section(QLatin1Char(' '), 1).trimmed();
        if (field == QLatin1String("explain")) {
            if (current)
                return fail(number, QStringLiteral("\"explain\" before the \"end\" of the record before"));
            current = ExplainTicks();
            hasAfter = false;
            playedText.clear();
            continue;
        }
        if (!current)
            return fail(number, QStringLiteral("\"%1\" outside a record (\"explain\" … \"end\")").arg(field));
        if (field == QLatin1String("before") || field == QLatin1String("after")) {
            const std::optional<ChessPosition> position = ChessPosition::fromFen(value, ChessPosition::Kings::Optional);
            if (!position)
                return fail(number, QStringLiteral("not a FEN: %1").arg(value));
            if (field == QLatin1String("before")) {
                current->before = position;
            } else {
                current->after = *position;
                hasAfter = true;
            }
        } else if (field == QLatin1String("played")) {
            playedText = value;
        } else if (field == QLatin1String("before-eval") || field == QLatin1String("tick")) {
            const std::optional<EngineEvaluation> evaluation = parseEvaluation(value);
            if (!evaluation)
                return fail(number, QStringLiteral("not an evaluation: %1").arg(value));
            if (field == QLatin1String("tick"))
                current->ticks << *evaluation;
            else
                current->beforeEvaluation = evaluation;
        } else if (field == QLatin1String("expect")) {
            current->expected << value;
        } else if (field == QLatin1String("end")) {
            if (!hasAfter)
                return fail(number, QStringLiteral("a record without \"after\""));
            if (!playedText.isEmpty()) {
                if (!current->before)
                    return fail(number, QStringLiteral("\"played\" without \"before\""));
                current->played = current->before->moveFromUci(playedText);
                if (!current->played)
                    return fail(number, QStringLiteral("%1 is not a legal move before").arg(playedText));
            }
            records << *current;
            current.reset();
        } else {
            return fail(number, QStringLiteral("unknown field \"%1\"").arg(field));
        }
    }
    if (current)
        return fail(int(lines.size()), QStringLiteral("the last record has no \"end\""));
    return records;
}

QList<ExplanationTick> ExplainTicks::replay(SanStyle style, bool trace) const
{
    QList<ExplanationTick> results;
    startExplanation();
    ExplanationInput input;
    input.before = before;
    input.played = played;
    input.beforeEvaluation = beforeEvaluation;
    input.after = after;
    input.sanStyle = style;
    input.trace = trace;
    for (const EngineEvaluation &tick : ticks) {
        input.afterEvaluation = tick;
        results << explainTick(input);
    }
    return results;
}

std::optional<MoveExplanation> ExplainTicks::lastShown(const QList<ExplanationTick> &ticks)
{
    for (qsizetype i = ticks.size() - 1; i >= 0; --i) {
        if (ticks.at(i).shown)
            return ticks.at(i).explanation;
    }
    return std::nullopt;
}

QStringList ExplainTicks::outcome(const MoveExplanation &explanation)
{
    static const char *const verdicts[] = {"none", "best", "good", "inaccuracy", "mistake", "blunder"};
    static const char *const kinds[] = {"refutation", "idea", "reply", "alternative"};
    QStringList arrows;
    for (const BoardArrow &arrow : explanation.arrows) {
        QString text = BoardState::squareName(arrow.from) + BoardState::squareName(arrow.to) + QLatin1Char(' ')
            + QLatin1String(kinds[int(arrow.kind)]);
        if (arrow.step > 0)
            text += QLatin1Char(' ') + QString::number(arrow.step);
        arrows << text;
    }
    QStringList lines{QStringLiteral("verdict ") + QLatin1String(verdicts[int(explanation.verdict)]),
                      QStringLiteral("arrows ") + (arrows.isEmpty() ? QStringLiteral("-") : arrows.join(QStringLiteral(", ")))};
    if (!explanation.lostPieces.isEmpty()) {
        QStringList lost;
        for (int square : explanation.lostPieces)
            lost << BoardState::squareName(square);
        lines << QStringLiteral("lost ") + lost.join(QStringLiteral(", "));
    }
    lines << QStringLiteral("summary ") + explanation.summary;
    if (!explanation.playback.isEmpty())
        lines << QStringLiteral("playback ") + explanation.playback.join(QLatin1Char(' '));
    return lines;
}
