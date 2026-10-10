#pragma once

#include "app/BoardState.h"
#include "app/GameDatabase.h"

#include <QString>

/// Stand-in names: what is shown for a player (or a puzzle's theme) a game leaves unnamed, in
/// the databases that are about the user — the training sets, whose games
/// the user plays, by the side to move at the start, against a trainer.
/// They are shown (games list, the board's header) and never stored: the
/// file keeps its names empty, so the stand-ins follow the user's name and
/// the interface's language, and the list sorts by what it shows.
struct StandInNames {
    /// Who plays the side to move: the user's name, or "You".
    QString trainee;
    /// The other side: "Your Trainer", in the interface's language.
    QString trainer;

    /// The side the user plays in a game of a training database: the one to
    /// move at its start (White from the standard position).
    static Side traineeSide(const GameRecord &game);
    /// A name a game leaves unnamed: empty, "?" or "-" (as PGN writes it).
    static bool isUnnamed(const QString &name);

    /// The name shown for `side` in `game`: its own, or the stand-in.
    QString name(const GameRecord &game, Side side) const;
    /// The event shown for `game`: a puzzle whose event says only where it
    /// comes from (none, "lichess.org puzzles", "Puzzle <id>") shows its
    /// theme (TrainingSets::puzzleTheme) in the interface's language; any
    /// other event is its own.
    static QString event(const GameRecord &game);
    /// `game` with its unnamed players and its puzzle's event given their stand-ins.
    GameRecord appliedTo(GameRecord game) const;
};
