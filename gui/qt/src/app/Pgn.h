#pragma once

#include "GameRecord.h"

#include <QString>

#include <optional>

/// PGN export for the games the client holds in memory.
///
/// Interim, like ChessPosition: import and export belong to the chess
/// database engine.
namespace Pgn {

/// Numbered SAN movetext of the first `plies` moves (all when negative),
/// e.g. "1.e4 e5 2.Nf3". Games starting with Black to move begin with "1…".
/// Annotated moves carry their glyphs: "2.Nf3! $14". The whole game (plies
/// negative) carries its variations too, in parentheses after the move each
/// is an alternative to: "2.Nf3 Nc6 (2…d6 3.d4) 3.Bb5".
QString moveText(const GameRecord &game, int plies = -1);

/// How a game begins, for lists: its first moves as a numbered line with the
/// symbols of their annotations, "1.e4 e5 2.Nf3! Nc6…", from the SAN as
/// stored (annotations glued, MoveAnnotation::storedSuffix) and without
/// replaying it. `startFen` only says who moves first and at which number;
/// the "…" ends a game of `plyCount` plies that goes on after `sanMoves`.
QString preview(const QString &startFen, const QStringList &sanMoves, int plyCount);

/// A game read from text people paste: PGN (tags are skipped — PgnFile::read
/// keeps them —, comments are kept on the move before them; a FEN tag sets
/// the start; "!", "?" and the NAGs that have a symbol are kept
/// as annotations; variations in parentheses are kept, each an alternative
/// to the move before it), plain SAN with or without move numbers, or UCI
/// moves.
struct ParsedLine {
    /// Empty for the standard starting position.
    QString startFen;
    QList<MoveRecord> moves;
    QList<Variation> variations;
    /// The comment before the first move.
    QString startComment;
};

/// Parses a game starting from `startFen` (unless the text has a FEN tag).
/// On an unreadable or illegal move of the main line, returns nothing and
/// explains why; a variation is cut at its first illegal move.
std::optional<ParsedLine> parseLine(const QString &text, const QString &startFen, QString *errorMessage);

/// Tags as PGN writes them, one per line: `[StudyName "Endgames"]`. How
/// `games.tags` stores the tags that have no column.
QString tagsText(const QList<PgnTag> &tags);
/// Reads tagsText()'s form (or any PGN tag lines).
QList<PgnTag> tagsFromText(const QString &text);

/// A complete PGN game: the seven tag roster, ratings, ECO, the game's other
/// tags (GameRecord::tags), the start position for games not starting from the initial one, and the movetext
/// wrapped at 80 columns. A game cut before its end gets the result "*".
QString game(const GameRecord &game, int plies = -1);

} // namespace Pgn
