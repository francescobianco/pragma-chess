#pragma once

#include "app/BoardState.h"
#include "app/EngineEvaluation.h"
#include "SmartValue.h"

class SmartInterpreter;

/// The chess a SMART program is given by this client: the sides, the
/// engine's evaluations, and the functions on them that smart/TUTOR.smart
/// and smart/EXPLAIN.smart list at their top. Every client provides the same
/// ones, with the same results.
namespace SmartChess {

/// An evaluation as a program sees it.
class EvaluationObject : public SmartObject {
public:
    explicit EvaluationObject(EngineEvaluation evaluation)
        : evaluation(std::move(evaluation))
    {
    }
    QString typeName() const override { return QStringLiteral("evaluation"); }
    EngineEvaluation evaluation;
};

/// Defines the sides (WHITE 1, BLACK -1) and the chess functions on `smart`.
void define(SmartInterpreter &smart);

SmartValue side(Side side);
SmartValue evaluation(const EngineEvaluation &evaluation);

} // namespace SmartChess
