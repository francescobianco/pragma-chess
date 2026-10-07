#pragma once

#include "app/BoardState.h"
#include "app/ChessPosition.h"
#include "app/EngineEvaluation.h"
#include "app/MoveExplanation.h"
#include "SmartValue.h"

class SmartInterpreter;

/// The chess a SMART program is given by this client: the sides and pieces,
/// positions, the engine's evaluations, and the commands that collect what
/// a program shows — what smart/TUTOR.smart and smart/EXPLAIN.smart list at
/// their top. Every client provides the same, with the same results.
namespace SmartChess {

/// A position as a program sees it.
class PositionObject : public SmartObject {
public:
    explicit PositionObject(ChessPosition position)
        : position(std::move(position))
    {
    }
    QString typeName() const override { return QStringLiteral("position"); }
    ChessPosition position;
};

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

/// What a program's commands collected (ARROW, LOST, SAY, VERDICT,
/// PLAYBACK, NOTE), and how it wants moves written. Cleared by the caller
/// before each call.
struct Output {
    SanStyle sanStyle = SanStyle::Letters;
    /// Keep the NOTEs in `explanation.trace`.
    bool trace = false;
    /// VIEWER(): who asked, WHITE (1), BLACK (-1) or 0.
    int viewer = 0;
    MoveExplanation explanation;

    void clear() { explanation = MoveExplanation(); }
};

/// Defines the constants, the chess functions and the commands on `smart`;
/// the commands write into `output`, which must outlive it.
void define(SmartInterpreter &smart, Output &output);

SmartValue side(Side side);
SmartValue position(const ChessPosition &position);
SmartValue evaluation(const EngineEvaluation &evaluation);

/// A sentence of a program translated (context "MoveExplanation", where the
/// desktop's tr() put them), %1, %2… replaced in one pass.
QString text(const QString &sentence, const QStringList &args);

} // namespace SmartChess
