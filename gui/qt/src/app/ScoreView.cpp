#include "ScoreView.h"

#include <QCoreApplication>

#include <algorithm>
#include <cmath>

namespace ScoreView {

namespace {

struct Text {
    Q_DECLARE_TR_FUNCTIONS(ScoreView)
};

} // namespace

Kind next(Kind kind)
{
    return Kind((int(kind) + 1) % kKinds);
}

QString key(Kind kind)
{
    switch (kind) {
    case Kind::Absolute: return QStringLiteral("absolute");
    case Kind::ForBottom: return QStringLiteral("for-bottom");
    case Kind::Chances: return QStringLiteral("chances");
    case Kind::Judgement: return QStringLiteral("judgement");
    }
    return {};
}

Kind fromKey(const QString &text)
{
    for (int i = 0; i < kKinds; ++i) {
        if (key(Kind(i)) == text)
            return Kind(i);
    }
    return Kind::Absolute;
}

double courseHeight(const EngineEvaluation &evaluation)
{
    if (evaluation.isMate)
        return evaluation.mating == Side::White ? 1.0 : -1.0;
    constexpr double kEdgePawns = 10;
    const double pawns = std::abs(evaluation.centipawns) / 100.0;
    const double height = std::min(1.0, std::log1p(pawns) / std::log1p(kEdgePawns));
    return evaluation.centipawns < 0 ? -height : height;
}

QString judgement(const EngineEvaluation &evaluation)
{
    const bool white = evaluation.isMate ? evaluation.mating == Side::White : evaluation.centipawns > 0;
    const int magnitude = evaluation.isMate ? 100000 : std::abs(evaluation.centipawns);
    if (magnitude <= 30)
        return QStringLiteral("=");
    if (magnitude <= 80)
        return white ? QStringLiteral("⩲") : QStringLiteral("⩱");
    if (magnitude <= 200)
        return white ? QStringLiteral("±") : QStringLiteral("∓");
    return white ? QStringLiteral("+−") : QStringLiteral("−+");
}

QString text(const EngineEvaluation &evaluation, Kind kind, Side bottom)
{
    switch (kind) {
    case Kind::Absolute:
        return evaluation.text();
    case Kind::ForBottom: {
        if (evaluation.isMate) {
            if (evaluation.mateIn == 0)
                return QStringLiteral("#");
            return (evaluation.mating == bottom ? QString() : QStringLiteral("−")) + QStringLiteral("M%1").arg(evaluation.mateIn);
        }
        EngineEvaluation seen = evaluation;
        if (bottom == Side::Black)
            seen.centipawns = -seen.centipawns;
        return seen.text();
    }
    case Kind::Chances:
        return QStringLiteral("%1%").arg(qRound(evaluation.shareFor(bottom) * 100));
    case Kind::Judgement:
        return judgement(evaluation);
    }
    return {};
}

QString label(Kind kind, Side bottom)
{
    const bool white = bottom == Side::White;
    switch (kind) {
    case Kind::Absolute: return Text::tr("Absolute");
    case Kind::ForBottom: return white ? Text::tr("For White") : Text::tr("For Black");
    case Kind::Chances: return white ? Text::tr("White's chances") : Text::tr("Black's chances");
    case Kind::Judgement: return Text::tr("Judgement");
    }
    return {};
}

} // namespace ScoreView
