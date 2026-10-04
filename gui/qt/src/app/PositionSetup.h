#pragma once

#include "BoardState.h"

#include <QList>
#include <QString>

#include <array>
#include <optional>

/// A position being set up by hand (Game ▸ Set Up Position…): the pieces,
/// who moves, castling, en passant and the move number, and whether it is a
/// position a game can start from. Pure, unit-tested.
class PositionSetup {
public:
    static PositionSetup empty();
    static PositionSetup startingPosition();
    /// Nothing for a FEN whose fields cannot be read.
    static std::optional<PositionSetup> fromFen(const QString &fen);

    Piece at(int square) const { return m_squares[square]; }
    void setPiece(int square, Piece piece) { m_squares[square] = piece; }

    Side sideToMove = Side::White;
    bool whiteKingSide = false;
    bool whiteQueenSide = false;
    bool blackKingSide = false;
    bool blackQueenSide = false;
    /// The square a pawn can be taken on en passant, or -1.
    int enPassant = -1;
    /// Half-moves since the last capture or pawn move (the fifty-move rule).
    int halfMove = 0;
    int fullMove = 1;

    /// Whether the pieces allow that castling: king and rook at home.
    bool canCastle(Side side, bool kingSide) const;
    /// The squares en passant can be on: behind a pawn of the side that just
    /// moved that could have come two squares.
    QList<int> enPassantSquares() const;

    /// The FEN, with only the castling and en passant the pieces allow.
    QString fen() const;
    /// What is wrong with the position, or empty when a game can start from it.
    QString problem() const;

private:
    std::array<Piece, 64> m_squares{};
};
