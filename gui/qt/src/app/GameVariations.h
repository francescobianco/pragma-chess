#pragma once

#include "GameRecord.h"

#include <QList>
#include <QString>

class ChessPosition;

/// The variations of a game: how they are stored, found and kept legal.
///
/// A variation is an alternative to one move of the line it hangs off,
/// played from the position before that move (`Variation::atPly`, 1-based
/// among the moves of that line alone: for a variation off a variation, the
/// count starts at the parent variation's first move). Variations nest, so
/// a game is a tree whose trunk is the main line (`GameRecord::moves`). A place in the tree is a `path`: the
/// index of the variation taken at each branch, from the main line down;
/// the empty path is the main line itself.
namespace GameVariations {

/// The stored text of the variations, one word per move as `moves_san` has
/// them (annotations glued), each variation in parentheses after the ply it
/// is an alternative to: "(50 Rfe8 Re5 Qb4) (24 f6 Bf3 (3 Be2) Qa3)".
QString toText(const QList<Variation> &variations);
/// Reads toText()'s form; the UCI of the moves is left empty for resolve().
QList<Variation> fromText(const QString &text);

/// Replays every line of `game` with the rules, from `start`: fills in the
/// UCI and SAN each move lacks and cuts a line at its first illegal move.
/// A variation left with no move, or at a ply its line does not reach, is
/// dropped. The main line is left as it is (GameSession cuts it).
void resolve(GameRecord &game, const ChessPosition &start);

/// The moves of the line `path` leads to, from the game's first move: the
/// main line up to the branch, then the variation, and so on. An invalid
/// path gives the main line.
QList<MoveRecord> lineMoves(const GameRecord &game, const QList<int> &path);
/// The variations hanging off the line `path` leads to; nullptr for an invalid path.
QList<Variation> *variationsOf(GameRecord &game, const QList<int> &path);
const QList<Variation> *variationsOf(const GameRecord &game, const QList<int> &path);
/// The ply of the line `path` leads to at which its last variation branches
/// off its parent, counted from the game's first move; 0 for the main line.
int branchPly(const GameRecord &game, const QList<int> &path);

} // namespace GameVariations
