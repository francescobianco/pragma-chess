#include "AdvantageProbe.h"

#include <cmath>

namespace AdvantageProbe {

bool agrees(const EngineEvaluation &a, const EngineEvaluation &b, double agreement)
{
    return 100.0 * std::abs(a.whiteShare() - b.whiteShare()) <= agreement;
}

std::optional<int> settledDepth(const QList<EngineEvaluation> &byDepth, double agreement)
{
    if (byDepth.isEmpty())
        return std::nullopt;
    const EngineEvaluation &reference = byDepth.last();
    std::optional<int> depth;
    for (qsizetype i = byDepth.size() - 1; i >= 0 && agrees(byDepth.at(i), reference, agreement); --i)
        depth = byDepth.at(i).depth;
    return depth;
}

std::optional<int> concretePly(const QList<EngineEvaluation> &shallow, const EngineEvaluation &deep, double agreement)
{
    std::optional<int> ply;
    for (qsizetype k = shallow.size() - 1; k >= 0 && agrees(shallow.at(k), deep, agreement); --k)
        ply = int(k);
    return ply;
}

} // namespace AdvantageProbe
