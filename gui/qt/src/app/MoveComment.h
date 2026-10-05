#pragma once

#include "GameRecord.h"

#include <QList>
#include <QString>
#include <QStringList>

class ChessPosition;

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

/// The comment of the line `path` (GameVariations) after its own move
/// `index` — 1-based among the moves of that line alone, as toJson() counts
/// them —, or before its first move for 0. Empty where the game has no such
/// move.
QString at(const GameRecord &game, const QList<int> &path, int index);
/// Sets that comment; false where the game has no such move.
bool set(GameRecord &game, const QList<int> &path, int index, const QString &comment);

/// `comment` with what a person reads replaced by `text`: its commands stay,
/// after the text, so a comment edited by hand keeps its evaluation, clock
/// and arrows.
QString withText(const QString &comment, const QString &text);

/// A mark drawn on the board by a comment's commands, as lichess draws them:
/// a coloured circle on a square ("[%csl Gd4,Re5]") or an arrow ("[%cal
/// Gd2d4]"). `color` is the command's letter: G, R, Y or B.
struct Mark {
    int from = -1;
    /// -1 for a circle on `from`.
    int to = -1;
    QChar color;
    bool isCircle() const { return to < 0; }
    bool operator==(const Mark &) const = default;
};
/// The marks of a comment, in the order written; malformed ones are skipped.
QList<Mark> marks(const QString &comment);

/// A move written in a comment's text ("14.Bxd4 Qxd4", "better is Nf3"),
/// and the line it ends, as the rules read it.
struct TextMove {
    /// Where it is in the text (displayText's), its number included.
    qsizetype start = 0;
    qsizetype length = 0;
    /// The ply of the comment's line the written line starts from, and its
    /// moves (UCI) up to this one.
    int basePly = 0;
    QStringList uci;
};
/// The moves written in `text`, a comment of a line whose positions, from
/// the game's start, are `line`, placed after its ply `at`. A run of moves
/// is one line. Its first move, when numbered, is played from the position
/// of the line with that number and side to move; otherwise as the next
/// move (a continuation), or else in place of the move commented (an
/// alternative). Words that are no legal move there are left as text.
QList<TextMove> movesIn(const QString &text, const QList<ChessPosition> &line, int at);

} // namespace MoveComment
