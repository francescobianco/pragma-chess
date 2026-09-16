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
/// This is deliberately not a rules engine: move legality, SAN and hashing
/// belong to the chess database engine. It only knows how to place pieces
/// from a FEN and relocate them for an already validated move.
class BoardState {
public:
    static BoardState startingPosition();
    static std::optional<BoardState> fromFen(const QString &fen);

    Piece at(int square) const { return m_squares[square]; }
    Side sideToMove() const { return m_sideToMove; }
    QString fen() const { return m_fen; }

    /// Applies a UCI move (e.g. "e2e4", "e7e8q") that is known to be legal.
    bool applyUci(const QString &uci);

    static int squareFromName(QStringView name);
    static QString squareName(int square);

private:
    std::array<Piece, 64> m_squares{};
    Side m_sideToMove = Side::White;
    QString m_fen;
};
