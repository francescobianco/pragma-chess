#pragma once

#include "BoardState.h"

#include <QList>
#include <QString>
#include <QStringList>

#include <array>
#include <optional>

/// A move between two squares (a1 = 0 ... h8 = 63), with the promotion piece
/// for pawns reaching the last rank. Castling is the king moving two files.
struct ChessMove {
    int from = -1;
    int to = -1;
    PieceType promotion = PieceType::None;

    bool operator==(const ChessMove &) const = default;

    /// "e2e4", "e7e8q".
    QString uci() const;
};

/// A complete chess position with the rules the desktop client needs to enter
/// moves and reason about lines: legal moves, SAN, check and attacks.
///
/// Interim implementation living in the GUI, like SqliteGameDatabase: the
/// rules belong to the chess database engine (core/, Rust), which will
/// replace this through its C API. Kept simple rather than fast.
class ChessPosition {
public:
    static ChessPosition startingPosition();
    /// Parses a FEN; rejects malformed FENs and positions where the side that
    /// just moved is in check or a king is missing.
    static std::optional<ChessPosition> fromFen(const QString &fen);
    QString fen() const;
    /// The FEN without move counters, identifying the position for caches.
    QString positionKey() const;

    /// Board contents for display.
    BoardState boardState() const;

    Piece at(int square) const { return m_squares[square]; }
    Side sideToMove() const { return m_sideToMove; }
    int kingSquare(Side side) const;
    int fullMoveNumber() const { return m_fullMove; }

    QList<ChessMove> legalMoves() const;
    bool isLegal(const ChessMove &move) const;
    /// The legal move written in UCI notation, if any.
    std::optional<ChessMove> moveFromUci(QStringView uci) const;

    /// Plays a legal move.
    void play(const ChessMove &move);
    /// Standard algebraic notation of a legal move, with check and mate marks.
    QString san(const ChessMove &move) const;
    /// Piece captured by a legal move (en passant included), or a null piece.
    Piece capturedPiece(const ChessMove &move) const;

    bool inCheck() const;
    bool isCheckmate() const;
    bool isStalemate() const;

    bool isAttacked(int square, Side by) const;
    /// Squares of the pieces of `side` attacking `square`.
    QList<int> attackers(int square, Side side) const;

    /// Material from White's point of view, in centipawns (P=100 … Q=900).
    int material() const;
    static int pieceValue(PieceType type);

    /// SAN moves of a UCI line with move numbers ("12.Nf3 Nc6 13.d4", "12…Nc6").
    /// Stops at the first illegal move or after `maxPlies`.
    QString lineText(const QStringList &uciMoves, int maxPlies = -1) const;
    /// "12." for White, "12…" for Black to move.
    QString moveNumberText() const;

private:
    void generatePseudoLegal(QList<ChessMove> &moves) const;
    bool leavesKingSafe(const ChessMove &move) const;

    std::array<Piece, 64> m_squares{};
    Side m_sideToMove = Side::White;
    /// Bits: 1 = White O-O, 2 = White O-O-O, 4 = Black O-O, 8 = Black O-O-O.
    int m_castling = 0;
    int m_enPassant = -1;
    int m_halfMoveClock = 0;
    int m_fullMove = 1;
};
