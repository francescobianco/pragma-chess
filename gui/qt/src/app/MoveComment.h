#pragma once

#include "GameRecord.h"

#include <QString>

/// The comments of a game: text between the moves, as PGN writes it in
/// braces. Lichess studies are mostly made of them, often with commands for
/// programs in square brackets ("[%eval 0.18]", "[%clk 0:05:00]", arrows
/// "[%cal Gd2d4]"): kept as they came, so the game goes back out the same,
/// and left out of what is shown. Pure, unit-tested.
namespace MoveComment {

/// What a person reads: the comment without its commands, its spaces
/// collapsed; empty when it held only commands.
QString displayText(const QString &comment);

/// The comment ready to go between PGN braces: it cannot hold a closing
/// brace, which becomes a parenthesis.
QString forPgn(const QString &comment);

/// Joins a comment to one already there (PGN allows several in a row).
QString joined(const QString &first, const QString &second);

/// Every comment of the game — before its first move, after its moves, in
/// its variations at any depth — as the JSON stored in `games.comments`:
/// {"<path>": {"<ply>": "text"}}, the path of a line as GameVariations has
/// it ("" for the main line, "0/2" for the third variation off the first),
/// ply 0 for the comment before the line's first move and n for the one
/// after its n-th move. Empty when the game has no comments.
QString toJson(const GameRecord &game);

/// Puts the comments of toJson()'s text back on `game`'s moves; a comment
/// whose line or move the game does not have is dropped.
void fromJson(GameRecord &game, const QString &json);

/// Whether the game has any comment.
bool hasComments(const GameRecord &game);

} // namespace MoveComment
