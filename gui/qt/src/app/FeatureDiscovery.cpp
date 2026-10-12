#include "FeatureDiscovery.h"

#include "TrainingTutor.h"

const QList<FeatureDiscovery::Feature> &FeatureDiscovery::all()
{
    static const QList<Feature> features{Feature::Explain};
    return features;
}

QString FeatureDiscovery::name(Feature feature)
{
    switch (feature) {
    case Feature::Explain: return QStringLiteral("explain");
    }
    return QString();
}

std::optional<FeatureDiscovery::Feature> FeatureDiscovery::fromName(const QString &name)
{
    for (Feature feature : all()) {
        if (FeatureDiscovery::name(feature) == name)
            return feature;
    }
    return std::nullopt;
}

FeatureDiscovery::FeatureDiscovery(const QList<Feature> &dismissed)
    : m_dismissed(dismissed)
{
}

void FeatureDiscovery::dismiss(Feature feature)
{
    if (!m_dismissed.contains(feature))
        m_dismissed << feature;
}

void FeatureDiscovery::undismiss(Feature feature)
{
    m_dismissed.removeAll(feature);
}

void FeatureDiscovery::markPresented(Feature feature)
{
    if (!m_presented.contains(feature))
        m_presented << feature;
}

void FeatureDiscovery::clearPresented(Feature feature)
{
    m_presented.removeAll(feature);
}

bool FeatureDiscovery::leavesBook(const QList<PolyglotBook::Move> &bookMoves, const ChessMove &played)
{
    if (bookMoves.isEmpty())
        return false;
    for (const PolyglotBook::Move &move : bookMoves) {
        if (move.move == played)
            return false;
    }
    return true;
}

bool FeatureDiscovery::looksWrong(const EngineEvaluation &before, const EngineEvaluation &after,
                                  const ChessPosition &start, const ChessMove &played)
{
    if (before.depth < kMinDepthBefore || after.depth < kMinDepthAfter)
        return false;
    return TrainingTutor::judge(before, after, start.sideToMove(), played, start) != TrainingTutor::Alert::None;
}
