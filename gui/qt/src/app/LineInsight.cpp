#include "LineInsight.h"

#include "smart/SmartPrograms.h"

LineInsight lineInsight(const ChessPosition &start, const QStringList &line, bool trace)
{
    LineInsight insight;
    SmartProgram *program = SmartPrograms::program(QStringLiteral("INSIGHT.smart"));
    if (!program) {
        insight.error = QStringLiteral("INSIGHT.smart cannot be loaded");
        return insight;
    }
    program->output.clear();
    program->output.trace = trace;
    std::vector<SmartValue> moves;
    for (const QString &move : line)
        moves.emplace_back(move);
    const std::optional<SmartValue> done = program->interpreter.call(
        QStringLiteral("Insight"), {SmartChess::position(start), SmartValue(std::move(moves))}, &insight.error);
    insight.trace = program->output.explanation.trace;
    if (!done) {
        qWarning("SMART INSIGHT.smart Insight: %s", qPrintable(insight.error));
        return insight;
    }
    insight.arrows = program->output.explanation.arrows;
    return insight;
}

QString InsightCase::toText() const
{
    QStringList lines{QStringLiteral("insight ") + name};
    if (start.fen() != ChessPosition::startingPosition().fen())
        lines << QStringLiteral("fen ") + start.fen();
    lines << QStringLiteral("line ") + line.join(QLatin1Char(' '));
    for (const QString &expectation : expected)
        lines << QStringLiteral("expect ") + expectation;
    lines << QStringLiteral("end");
    return lines.join(QLatin1Char('\n')) + QLatin1Char('\n');
}

std::optional<QList<InsightCase>> InsightCase::fromText(const QString &text, QString *error)
{
    QList<InsightCase> cases;
    std::optional<InsightCase> current;
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
        if (field == QLatin1String("insight")) {
            if (current)
                return fail(number, QStringLiteral("\"insight\" before the \"end\" of the case before"));
            current = InsightCase();
            current->name = value;
            continue;
        }
        if (!current)
            return fail(number, QStringLiteral("\"%1\" outside a case (\"insight\" … \"end\")").arg(field));
        if (field == QLatin1String("fen")) {
            const std::optional<ChessPosition> position = ChessPosition::fromFen(value);
            if (!position)
                return fail(number, QStringLiteral("not a FEN: %1").arg(value));
            current->start = *position;
        } else if (field == QLatin1String("line")) {
            current->line = value.split(QLatin1Char(' '), Qt::SkipEmptyParts);
        } else if (field == QLatin1String("expect")) {
            current->expected << value;
        } else if (field == QLatin1String("end")) {
            ChessPosition position = current->start;
            for (const QString &uci : std::as_const(current->line)) {
                const std::optional<ChessMove> move = position.moveFromUci(uci);
                if (!move)
                    return fail(number, QStringLiteral("%1 is not a legal move of the line").arg(uci));
                position.play(*move);
            }
            if (current->expected.isEmpty())
                return fail(number, QStringLiteral("a case without \"expect\" (\"expect -\": no arrow)"));
            cases << *current;
            current.reset();
        } else {
            return fail(number, QStringLiteral("unknown field \"%1\"").arg(field));
        }
    }
    if (current)
        return fail(int(lines.size()), QStringLiteral("the last case has no \"end\""));
    return cases;
}

QStringList InsightCase::outcome(const QList<BoardArrow> &arrows)
{
    QStringList lines;
    for (const BoardArrow &arrow : arrows) {
        QString text = BoardState::squareName(arrow.from) + BoardState::squareName(arrow.to);
        if (!arrow.via.isEmpty()) {
            text += QStringLiteral(" via");
            for (int square : arrow.via)
                text += QLatin1Char(' ') + BoardState::squareName(square);
        }
        lines << text;
    }
    if (lines.isEmpty())
        lines << QStringLiteral("-");
    return lines;
}
