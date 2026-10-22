#pragma once

#include "app/ChessPosition.h"
#include "app/GameRecord.h"

#include <QByteArray>
#include <QList>

#include <optional>

/// The moves of a game as En Croissant stores them (the `Moves` blob of its
/// `.db3` databases, src-tauri/src/db/encoding.rs): one byte a move, its
/// index among the legal moves in the order shakmaty generates them, and
/// markers for variations (255 opens, 254 closes), comments (253) and NAGs
/// (252), each of these two followed by a little-endian length and its text.
/// Pure, unit-tested.
namespace EnCroissantGames {

/// The legal moves of `position` in shakmaty's order (0.27, `legal_moves`):
/// en passant; in check the king's moves; the pawns' captures towards the
/// a-file then towards the h-file, each with the promotions after, by
/// target square; single steps, their promotions (Q, R, B, N), double
/// steps; knights, bishops, rooks and queens by origin and target square;
/// not in check the king's moves, then castling king side and queen side.
QList<ChessMove> orderedMoves(const ChessPosition &position);

/// The main line of `moves` from `start`, in SAN and UCI; variations,
/// comments and NAGs are passed over. Nothing when a byte is no legal move.
std::optional<QList<MoveRecord>> mainLine(const QByteArray &moves, const ChessPosition &start);

} // namespace EnCroissantGames
