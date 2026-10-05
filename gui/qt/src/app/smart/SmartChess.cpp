#include "SmartChess.h"

#include "SmartInterpreter.h"

namespace SmartChess {

namespace {

// Names as plain C strings: a QString temporary would make GCC fear for the returned references.
const EngineEvaluation &evaluationArgument(const char *function, const std::vector<SmartValue> &args, int index)
{
    const std::shared_ptr<EvaluationObject> object =
        index < int(args.size()) ? args.at(index).as<EvaluationObject>() : nullptr;
    if (!object)
        SmartInterpreter::fail(QStringLiteral("%1 needs an evaluation as argument %2, not %3")
                                   .arg(QLatin1String(function)).arg(index + 1)
                                   .arg(index < int(args.size()) ? args.at(index).typeName() : QStringLiteral("nothing")));
    return object->evaluation;
}

Side sideArgument(const char *function, const std::vector<SmartValue> &args, int index)
{
    const double value = SmartInterpreter::numberArgument(QLatin1String(function), args, index);
    if (value != 1 && value != -1)
        SmartInterpreter::fail(QStringLiteral("%1 needs a side (WHITE or BLACK) as argument %2")
                                   .arg(QLatin1String(function)).arg(index + 1));
    return value > 0 ? Side::White : Side::Black;
}

} // namespace

SmartValue side(Side side)
{
    return SmartValue(side == Side::White ? 1 : -1);
}

SmartValue evaluation(const EngineEvaluation &evaluation)
{
    return SmartValue(std::make_shared<EvaluationObject>(evaluation));
}

void define(SmartInterpreter &smart)
{
    smart.defineConstant(QStringLiteral("WHITE"), side(Side::White));
    smart.defineConstant(QStringLiteral("BLACK"), side(Side::Black));

    smart.define(QStringLiteral("SHARE"), [](const std::vector<SmartValue> &args) {
        SmartInterpreter::expectArguments(QStringLiteral("SHARE"), args, 2);
        const EngineEvaluation &evaluation = evaluationArgument("SHARE", args, 0);
        return SmartValue(evaluation.shareFor(sideArgument("SHARE", args, 1)));
    });
    smart.define(QStringLiteral("BESTMOVE"), [](const std::vector<SmartValue> &args) {
        SmartInterpreter::expectArguments(QStringLiteral("BESTMOVE"), args, 1);
        const EngineEvaluation &evaluation = evaluationArgument("BESTMOVE", args, 0);
        return SmartValue(evaluation.pv.isEmpty() ? QString() : evaluation.pv.first());
    });
}

} // namespace SmartChess
