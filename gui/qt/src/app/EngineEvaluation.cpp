#include "EngineEvaluation.h"

#include <cmath>

double EngineEvaluation::whiteShare() const
{
    if (isMate)
        return mating == Side::White ? 1.0 : 0.0;
    // Winning chances curve used by lichess, mapped from [-1, 1] to [0, 1].
    const double winningChances = 2.0 / (1.0 + std::exp(-0.00368208 * centipawns)) - 1.0;
    return 0.5 + 0.5 * winningChances;
}

int EngineEvaluation::centipawnsFor(Side side) const
{
    // A closer mate is worth more; any mate outweighs every material balance.
    const int white = isMate ? (mating == Side::White ? 1 : -1) * (10000 - 10 * mateIn) : centipawns;
    return side == Side::White ? white : -white;
}

QString EngineEvaluation::text() const
{
    if (isMate)
        return mateIn == 0 ? QStringLiteral("#") : QStringLiteral("M%1").arg(mateIn);
    const double pawns = centipawns / 100.0;
    const QString number = std::abs(pawns) >= 10 ? QString::number(std::abs(pawns), 'f', 0)
                                                 : QString::number(std::abs(pawns), 'f', 1);
    if (number == QLatin1String("0.0"))
        return number;
    return (pawns > 0 ? QStringLiteral("+") : QStringLiteral("−")) + number;
}
