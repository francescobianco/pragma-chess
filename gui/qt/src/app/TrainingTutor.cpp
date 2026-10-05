#include "TrainingTutor.h"

#include "smart/SmartPrograms.h"

namespace TrainingTutor {

Alert judge(const EngineEvaluation &before, const EngineEvaluation &after, Side user, const ChessMove &played,
            const std::optional<ChessPosition> &start)
{
    // The judgement is smart/TUTOR.smart's: the same in every client.
    SmartProgram *tutor = SmartPrograms::program(QStringLiteral("TUTOR.smart"));
    QString error;
    const std::optional<SmartValue> alert =
        tutor ? tutor->interpreter.call(QStringLiteral("Judge"), {SmartChess::evaluation(before), SmartChess::evaluation(after),
                                                     SmartChess::side(user), SmartValue(played.uci()),
                                                     start ? SmartChess::position(*start) : SmartValue()},
                                          &error)
              : std::nullopt;
    if (!alert) {
        if (tutor)
            qWarning("SMART TUTOR.smart Judge: %s", qPrintable(error));
        return Alert::None;
    }
    const QString name = alert->toText();
    if (name == QLatin1String("missed-chance"))
        return Alert::MissedChance;
    if (name == QLatin1String("inaccuracy"))
        return Alert::Inaccuracy;
    if (name == QLatin1String("mistake"))
        return Alert::Mistake;
    if (name == QLatin1String("blunder"))
        return Alert::Blunder;
    return Alert::None;
}

} // namespace TrainingTutor
