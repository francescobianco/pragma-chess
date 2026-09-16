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
QString moveText(const GameRecord &game, int plies = -1);

/// A main line read from text people paste: PGN (tags, comments, variations
/// and NAGs are skipped; a FEN tag sets the start), plain SAN with or without
/// move numbers, or UCI moves.
struct ParsedLine {
    /// Empty for the standard starting position.
    QString startFen;
    QList<MoveRecord> moves;
};

/// Parses a main line starting from `startFen` (unless the text has a FEN
/// tag). On an unreadable or illegal move, returns nothing and explains why.
std::optional<ParsedLine> parseLine(const QString &text, const QString &startFen, QString *errorMessage);

/// A complete PGN game: the seven tag roster, ratings, ECO, the start
/// position for games not starting from the initial one, and the movetext
/// wrapped at 80 columns. A game cut before its end gets the result "*".
QString game(const GameRecord &game, int plies = -1);

} // namespace Pgn
