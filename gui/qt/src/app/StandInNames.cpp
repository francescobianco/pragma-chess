#include "StandInNames.h"

#include "TrainingSets.h"

#include <QCoreApplication>

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

QString StandInNames::event(const GameRecord &game)
{
    const QString theme = TrainingSets::puzzleTheme(game);
    if (theme.isEmpty())
        return game.event;
    const QString own = game.event.trimmed();
    QString id;
    for (const PgnTag &tag : game.tags)
        if (tag.name == QLatin1String("PuzzleId"))
            id = tag.value;
    // Where the puzzle comes from, in whatever language the copy was made
    // ("lichess.org puzzles", "Problemi di lichess.org"), or older copies' "Puzzle <id>".
    const bool generic = isUnnamed(own) || (!id.isEmpty() && own.contains(QLatin1String("lichess"), Qt::CaseInsensitive))
        || own == QCoreApplication::translate("TrainingSets", "lichess.org puzzles")
        || (!id.isEmpty() && own == QStringLiteral("Puzzle ") + id);
    return generic ? TrainingSets::puzzleThemeName(theme) : game.event;
}

GameRecord StandInNames::appliedTo(GameRecord game) const
{
    const QString white = name(game, Side::White);
    const QString black = name(game, Side::Black);
    game.white = white;
    game.black = black;
    game.event = event(game);
    return game;
}
