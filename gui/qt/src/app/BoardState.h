#pragma once

#include <QString>

#include <array>
#include <optional>

enum class PieceType : quint8 { None, Pawn, Knight, Bishop, Rook, Queen, King };
enum class Side : quint8 { White, Black };

struct Piece {
    PieceType type = PieceType::None;
    Side side = Side::White;

    bool isNull() const { return type == PieceType::None; }
    bool operator==(const Piece &) const = default;
};

/// Board contents for display. Squares are indexed a1 = 0 ... h8 = 63.
///
/// This is deliberately not a rules engine (see ChessPosition): it only
/// knows how to place pieces from a FEN.
class BoardState {
public:
    static BoardState startingPosition();
    static std::optional<BoardState> fromFen(const QString &fen);

    Piece at(int square) const { return m_squares[square]; }
    Side sideToMove() const { return m_sideToMove; }
    QString fen() const { return m_fen; }

    static int squareFromName(QStringView name);
    static QString squareName(int square);

private:
    std::array<Piece, 64> m_squares{};
    Side m_sideToMove = Side::White;
    QString m_fen;
};
