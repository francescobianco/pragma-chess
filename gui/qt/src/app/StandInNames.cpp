#include "StandInNames.h"

#include <QStringList>

Side StandInNames::traineeSide(const GameRecord &game)
{
    // The second field of the FEN; no FEN, the standard position.
    const QString toMove = game.startFen.section(QLatin1Char(' '), 1, 1, QString::SectionSkipEmpty);
    return toMove == QLatin1String("b") ? Side::Black : Side::White;
}

bool StandInNames::isUnnamed(const QString &name)
{
    const QString trimmed = name.trimmed();
    return trimmed.isEmpty() || trimmed == QLatin1String("?") || trimmed == QLatin1String("-");
}

QString StandInNames::name(const GameRecord &game, Side side) const
{
    const QString &own = side == Side::White ? game.white : game.black;
    if (!isUnnamed(own))
        return own;
    return side == traineeSide(game) ? trainee : trainer;
}

GameRecord StandInNames::appliedTo(GameRecord game) const
{
    const QString white = name(game, Side::White);
    const QString black = name(game, Side::Black);
    game.white = white;
    game.black = black;
    return game;
}
