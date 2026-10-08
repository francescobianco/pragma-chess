#pragma once

#include "BoardState.h"

#include <QList>
#include <QString>
#include <QStringList>

#include <array>
#include <optional>

/// How SAN is written: letters (PGN, clipboard, command line) or figurines
/// ("♘f3") for display.
enum class SanStyle { Letters, Figurines };

/// Replaces the piece letters of one SAN move with figurines: "Nxe5+" → "♘xe5+",
/// "e8=Q" → "e8=♕". Castling and pawn moves are unchanged.
QString figurineSan(const QString &san);
/// The same for a whole line of moves, "1.e4 e5 2.Nf3 Nc6": in SAN the
/// capital letters K, Q, R, B and N are always pieces.
QString figurineLine(const QString &line);

/// Counts of pieces by side and type: `counts[int(side)][int(type)]`.
using PieceCounts = std::array<std::array<int, 7>, 2>;

/// A move between two squares (a1 = 0 ... h8 = 63), with the promotion piece
/// for pawns reaching the last rank. Castling is the king moving two files.
struct ChessMove {
    int from = -1;
    int to = -1;
    PieceType promotion = PieceType::None;

    bool operator==(const ChessMove &) const = default;

    /// A null move: the side to move passes, as a game written by hand may
    /// do ("--" in PGN, ChessBase's null move) where a move was not known or
    /// a plan is shown. It has no squares. Only a game's own moves are read
    /// with them (NullMoves::Allowed): an engine or a player never passes.
    static ChessMove null() { return {}; }
    bool isNull() const { return from < 0 && to < 0; }

    /// "e2e4", "e7e8q"; "0000" for the null move, as UCI writes it.
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
    /// Whether a position needs its two kings. A game may start from a
    /// diagram without them (a lichess study's chapter of text, on an empty
    /// board): it is shown, and no move can be played from it.
    enum class Kings { Required, Optional };
    /// Parses a FEN; rejects malformed FENs and positions where the side that
    /// just moved is in check or a king is missing (unless `kings` is
    /// Optional; a side never has two).
    static std::optional<ChessPosition> fromFen(const QString &fen, Kings kings = Kings::Required);
    /// Both kings are on the board: moves can be played, an engine can search.
    bool hasKings() const;
    /// The same position with the other side to move, as if the side to move
    /// passed: what it would face if it did nothing (threats). Nothing when
    /// the side to move is in check, which cannot pass.
    std::optional<ChessPosition> passed() const;
    QString fen() const;
    /// The FEN without move counters, identifying the position for caches.
    QString positionKey() const;

    /// Board contents for display.
    BoardState boardState() const;

    Piece at(int square) const { return m_squares[square]; }
    Side sideToMove() const { return m_sideToMove; }
    int kingSquare(Side side) const;
    int fullMoveNumber() const { return m_fullMove; }
    /// Bits: 1 = White O-O, 2 = White O-O-O, 4 = Black O-O, 8 = Black O-O-O.
    int castlingRights() const { return m_castling; }
    /// The square a pawn can capture en passant on, or -1.
    int enPassantSquare() const { return m_enPassant; }

    QList<ChessMove> legalMoves() const;
    bool isLegal(const ChessMove &move) const;
    /// Whether a null move is read: in a game's moves only.
    enum class NullMoves { Refused, Allowed };
    /// Whether the side to move may pass: not in check, and with both kings.
    bool canPass() const;
    /// The legal move written in UCI notation, if any; "0000" is the null
    /// move when `nullMoves` allows it.
    std::optional<ChessMove> moveFromUci(QStringView uci, NullMoves nullMoves = NullMoves::Refused) const;
    /// The legal move written in SAN, if exactly one matches. Lenient like
    /// people write: check marks, annotations, "x" and "=" are optional,
    /// "0-0" means castling and extra disambiguation is accepted. "--" (and
    /// "Z0", as some programs write it) is the null move when allowed.
    std::optional<ChessMove> moveFromSan(QStringView san, NullMoves nullMoves = NullMoves::Refused) const;

    /// Plays a legal move, or the null move when the side may pass.
    void play(const ChessMove &move);
    /// Standard algebraic notation of a legal move, with check and mate
    /// marks; "--" for the null move.
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
    /// Pieces of each side taken off the board between `start` and this
    /// position. A pawn that promoted is not counted as captured. Against
    /// startingPosition(), what each side misses of its full set (the board's
    /// captured pieces, for a game from any position).
    PieceCounts capturedSince(const ChessPosition &start) const;

    /// SAN moves of a UCI line with move numbers ("12.Nf3 Nc6 13.d4", "12…Nc6").
    /// Stops at the first illegal move or after `maxPlies`.
    QString lineText(const QStringList &uciMoves, int maxPlies = -1, SanStyle style = SanStyle::Letters) const;
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
