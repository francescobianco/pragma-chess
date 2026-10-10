#include "RoomName.h"

#include <algorithm>
#include <QCoreApplication>
#include <QRandomGenerator>

#include <iterator>

namespace {

/// The terms, each a phrase with the champion as %1. Append only.
const char *const kTerms[] = {
    QT_TRANSLATE_NOOP("RoomName", "%1's Gambit"),
    QT_TRANSLATE_NOOP("RoomName", "%1's Fortress"),
    QT_TRANSLATE_NOOP("RoomName", "%1's Zugzwang"),
    QT_TRANSLATE_NOOP("RoomName", "%1's Fianchetto"),
    QT_TRANSLATE_NOOP("RoomName", "%1's Outpost"),
    QT_TRANSLATE_NOOP("RoomName", "%1's Pin"),
    QT_TRANSLATE_NOOP("RoomName", "%1's Fork"),
    QT_TRANSLATE_NOOP("RoomName", "%1's Skewer"),
    QT_TRANSLATE_NOOP("RoomName", "%1's Battery"),
    QT_TRANSLATE_NOOP("RoomName", "%1's Castle"),
    QT_TRANSLATE_NOOP("RoomName", "%1's Endgame"),
    QT_TRANSLATE_NOOP("RoomName", "%1's Opening"),
    QT_TRANSLATE_NOOP("RoomName", "%1's Combination"),
    QT_TRANSLATE_NOOP("RoomName", "%1's Sacrifice"),
    QT_TRANSLATE_NOOP("RoomName", "%1's Attack"),
    QT_TRANSLATE_NOOP("RoomName", "%1's Defence"),
    QT_TRANSLATE_NOOP("RoomName", "%1's Breakthrough"),
    QT_TRANSLATE_NOOP("RoomName", "%1's Blockade"),
    QT_TRANSLATE_NOOP("RoomName", "%1's Tempo"),
    QT_TRANSLATE_NOOP("RoomName", "%1's Diagonal"),
    QT_TRANSLATE_NOOP("RoomName", "%1's Open File"),
    QT_TRANSLATE_NOOP("RoomName", "%1's Seventh Rank"),
    QT_TRANSLATE_NOOP("RoomName", "%1's Passed Pawn"),
    QT_TRANSLATE_NOOP("RoomName", "%1's Bishop Pair"),
    QT_TRANSLATE_NOOP("RoomName", "%1's Knight"),
    QT_TRANSLATE_NOOP("RoomName", "%1's Rook"),
    QT_TRANSLATE_NOOP("RoomName", "%1's Queen"),
    QT_TRANSLATE_NOOP("RoomName", "%1's King March"),
    QT_TRANSLATE_NOOP("RoomName", "%1's Swindle"),
    QT_TRANSLATE_NOOP("RoomName", "%1's Stalemate"),
    QT_TRANSLATE_NOOP("RoomName", "%1's Mating Net"),
    QT_TRANSLATE_NOOP("RoomName", "%1's Isolated Pawn"),
    QT_TRANSLATE_NOOP("RoomName", "%1's Novelty"),
    QT_TRANSLATE_NOOP("RoomName", "%1's Initiative"),
    QT_TRANSLATE_NOOP("RoomName", "%1's Counterattack"),
    QT_TRANSLATE_NOOP("RoomName", "%1's Opposition"),
    QT_TRANSLATE_NOOP("RoomName", "%1's Exchange"),
    QT_TRANSLATE_NOOP("RoomName", "%1's Desperado"),
    QT_TRANSLATE_NOOP("RoomName", "%1's Windmill"),
    QT_TRANSLATE_NOOP("RoomName", "%1's Brilliancy"),
};

/// World champions and masters of the game's history, written as they are
/// known in the West. Append only.
const char *const kChampions[] = {
    "Steinitz", "Lasker", "Capablanca", "Alekhine", "Euwe", "Botvinnik", "Smyslov", "Tal",
    "Petrosian", "Spassky", "Fischer", "Karpov", "Kasparov", "Kramnik", "Anand", "Carlsen",
    "Ding", "Gukesh", "Topalov", "Menchik", "Rudenko", "Bykova", "Gaprindashvili", "Chiburdanidze",
    "Xie Jun", "Hou Yifan", "Polgar", "Morphy", "Philidor", "Greco", "Anderssen", "Rubinstein",
    "Nimzowitsch", "Réti", "Keres", "Bronstein", "Tarrasch", "Marshall", "Larsen", "Korchnoi",
};

constexpr int kTermCount = int(std::size(kTerms));
constexpr int kChampionCount = int(std::size(kChampions));

} // namespace

namespace RoomName {

int termCount()
{
    return kTermCount;
}

int championCount()
{
    return kChampionCount;
}

int termOf(quint32 seed)
{
    return int(seed % quint32(kTermCount));
}

QString champion(int index)
{
    return QString::fromUtf8(kChampions[std::clamp(index, 0, kChampionCount - 1)]);
}

int championOf(quint32 seed)
{
    return int((seed / quint32(kTermCount)) % quint32(kChampionCount));
}

QString text(quint32 seed)
{
    return QCoreApplication::translate("RoomName", kTerms[termOf(seed)])
        .arg(QString::fromUtf8(kChampions[championOf(seed)]));
}

QString englishText(quint32 seed)
{
    return QString::fromLatin1(kTerms[termOf(seed)]).arg(QString::fromUtf8(kChampions[championOf(seed)]));
}

quint32 newSeed(const QSet<quint32> &taken)
{
    // Two seeds name the same room when they pick the same term and champion.
    QSet<int> names;
    for (quint32 seed : taken)
        names.insert(termOf(seed) * kChampionCount + championOf(seed));
    const int all = kTermCount * kChampionCount;
    quint32 seed = QRandomGenerator::global()->generate();
    for (int tries = 0; tries < all && names.contains(termOf(seed) * kChampionCount + championOf(seed)); ++tries)
        seed = QRandomGenerator::global()->generate();
    return seed;
}

} // namespace RoomName
