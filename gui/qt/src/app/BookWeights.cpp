#include "BookWeights.h"

#include <QtGlobal>

#include <cmath>

namespace BookWeights {

namespace {

constexpr int kMaxWeight = 65535;
/// A book of small weights is rewritten on a finer scale, so a few per cent
/// of a move can still be told apart.
constexpr int kMinScale = 10000;
/// A move at zero, or with next to nothing, gains nothing by a percentage of
/// itself: an increase first brings it to this share, taken from the others.
constexpr double kSeed = 0.01;

/// Shares back to integer weights that sum to about the same total, and to
/// at least `kMinScale`; a share that is not zero never rounds to zero.
QList<int> toWeights(const QList<double> &shares, int total)
{
    const int scale = qBound(kMinScale, total, kMaxWeight);
    QList<int> weights;
    for (const double share : shares) {
        const int weight = int(std::lround(share * scale));
        weights << (share > 0 && weight == 0 ? 1 : qMin(weight, kMaxWeight));
    }
    return weights;
}

QList<double> sharesOf(const QList<int> &weights, int total)
{
    QList<double> shares;
    for (const int weight : weights)
        shares << (total > 0 ? double(weight) / total : 0.0);
    return shares;
}

/// Gives the move at `index` the share `wanted`, the rest scaled to fill the whole.
void setShare(QList<double> &shares, int index, double wanted)
{
    const double others = 1.0 - shares.at(index);
    const double room = 1.0 - wanted;
    for (int i = 0; i < shares.size(); ++i) {
        if (i == index)
            shares[i] = wanted;
        else
            shares[i] = others > 0 ? shares.at(i) * room / others : 0.0;
    }
}

} // namespace

QList<int> adjusted(const QList<int> &weights, int index, int percent)
{
    if (index < 0 || index >= weights.size() || percent == 0)
        return weights;
    int total = 0;
    for (const int weight : weights)
        total += weight;
    if (total == 0) {
        // Nobody has anything: an increase makes this the only move that counts.
        if (percent < 0)
            return weights;
        QList<int> result(weights.size(), 0);
        result[index] = 1;
        return result;
    }
    QList<double> shares = sharesOf(weights, total);
    if (shares.at(index) == 0.0 && percent < 0)
        return weights; // Nothing to take.
    if (shares.at(index) < kSeed && percent > 0)
        setShare(shares, index, kSeed); // Seeded from the others, then grown.
    if (shares.at(index) >= 1.0)
        return weights; // The only move that counts: there is nobody to trade with.
    const double wanted = qBound(0.0, shares.at(index) * (1.0 + percent / 100.0), 1.0);
    setShare(shares, index, wanted);
    return toWeights(shares, total);
}

QList<int> zeroed(const QList<int> &weights, int index)
{
    if (index < 0 || index >= weights.size() || weights.at(index) == 0)
        return weights;
    int total = 0;
    for (const int weight : weights)
        total += weight;
    if (total == weights.at(index))
        return weights; // The only move that counts: nobody to give it to.
    QList<double> shares = sharesOf(weights, total);
    setShare(shares, index, 0.0);
    return toWeights(shares, total);
}

} // namespace BookWeights
