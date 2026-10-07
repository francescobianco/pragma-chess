#pragma once

#include "ChessPosition.h"
#include "GameRecord.h"

#include <QList>
#include <QString>
#include <QStringList>

/// The training sets Pragma Chess ships — endgames and tactics, from the
/// lichess puzzle database (CC0, resources/training) and the classic
/// theoretical endgames — and how the games of any database are classified
/// for them in the tree: endgames by the material they start from, tactics by
/// the puzzle themes they carry (the Themes tag).
namespace TrainingSets {

/// Puzzles of a training TSV (make-training.py: id, FEN, UCI moves, rating,
/// themes, URL) as games: each starts after the opponent's first move, so the
/// side to move is the one to find the solution, which follows as the moves.
/// Rows that do not replay are skipped.
QList<GameRecord> puzzleGames(const QString &tsv);

/// The classic theoretical endgames — the basic mates, the opposition, the
/// square, Lucena, Philidor, the wrong bishop… —, named in the interface's
/// language, their goal as the comment before the first move and the result
/// as what correct play gives.
QList<GameRecord> theoryEndgames();

/// The material of an endgame the game starts from, stronger side first:
/// "KRP-KR" (pawns once, however many); empty when the game starts from the
/// usual position or from one with more than four pieces besides kings and
/// pawns.
QString endgameOf(const GameRecord &game);
QString endgameOf(const ChessPosition &position);
/// The family of an endgame: "mate" (one side has only its king, the other no
/// pawn), "pawn", "rook", "queen", "bishop", "knight", "minor", "rook-minor",
/// "queen-other" or "other".
QString endgameFamily(const QString &endgame);
/// In the interface's language: "Rook Endings"; "K+R+P vs K+R".
QString endgameFamilyName(const QString &family);
QString endgameName(const QString &endgame);
/// The families in the order the tree lists them.
QStringList endgameFamilies();

/// The tactical themes of a game, from its Themes tag (lichess's names: "pin",
/// "skewer"…), only those the tree lists, in that order.
QStringList tacticsOf(const GameRecord &game);
/// Every tactical theme the tree lists, and its name: "Pin", "Skewer"…
QStringList tacticThemes();
QString tacticName(const QString &theme);

} // namespace TrainingSets
