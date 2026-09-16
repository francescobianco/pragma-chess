#pragma once

#include "EngineEvaluation.h"

#include <QList>

#include <optional>

/// Finds where an advantage stops being a deep engine insight and shows on
/// the board. A +3 often needs several forced, good moves before anything
/// visible happens; the explanation should point at that moment, the theme
/// that makes the evaluation concrete.
///
/// Two probes, both fed with engine searches by the caller:
/// - by depth: one position searched at increasing depths; the smallest depth
///   from which the score stays with the deepest one tells how far ahead the
///   advantage lies;
/// - along the line: every position of the principal variation searched at a
///   small fixed depth; the first ply from which that shallow score agrees
///   with the deep one is where the advantage becomes concrete.
namespace AdvantageProbe {

/// Winning-chance points (0–100) within which two evaluations agree.
inline constexpr double kAgreement = 10.0;

/// Whether two evaluations agree within `agreement` winning-chance points.
bool agrees(const EngineEvaluation &a, const EngineEvaluation &b, double agreement = kAgreement);

/// `byDepth` holds evaluations of one position by increasing depth, the last
/// being the reference. Returns the depth from which all agree with it.
std::optional<int> settledDepth(const QList<EngineEvaluation> &byDepth, double agreement = kAgreement);

/// `shallow[k]` is the shallow evaluation of the position after k moves of the
/// line whose deep evaluation is `deep`. Returns the first ply from which all
/// shallow evaluations agree with `deep`.
std::optional<int> concretePly(const QList<EngineEvaluation> &shallow, const EngineEvaluation &deep,
                               double agreement = kAgreement);

} // namespace AdvantageProbe
