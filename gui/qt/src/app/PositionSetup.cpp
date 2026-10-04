#include "PositionSetup.h"

#include "ChessPosition.h"

#include <QCoreApplication>

namespace {

struct Text {
    Q_DECLARE_TR_FUNCTIONS(PositionSetup)
};

QChar letter(Piece piece)
{
    static const char letters[] = " pnbrqk";
    const QChar c = QLatin1Char(letters[int(piece.type)]);
    return piece.side == Side::White ? c.toUpper() : c;
}

} // namespace

PositionSetup PositionSetup::empty()
{
    return PositionSetup();
}

PositionSetup PositionSetup::startingPosition()
{
    return *fromFen(QStringLiteral("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"));
}

std::optional<PositionSetup> PositionSetup::fromFen(const QString &fen)
{
    const std::optional<BoardState> board = BoardState::fromFen(fen);
    if (!board)
        return std::nullopt;
    const QStringList fields = fen.simplified().split(QLatin1Char(' '));
    PositionSetup setup;
    for (int square = 0; square < 64; ++square)
        setup.m_squares[square] = board->at(square);
    setup.sideToMove = board->sideToMove();
    const QString castling = fields.value(2);
    setup.whiteKingSide = castling.contains(QLatin1Char('K'));
    setup.whiteQueenSide = castling.contains(QLatin1Char('Q'));
    setup.blackKingSide = castling.contains(QLatin1Char('k'));
    setup.blackQueenSide = castling.contains(QLatin1Char('q'));
    setup.enPassant = BoardState::squareFromName(fields.value(3));
    setup.halfMove = qMax(0, fields.value(4).toInt());
    setup.fullMove = qMax(1, fields.value(5).toInt());
    return setup;
}

bool PositionSetup::canCastle(Side side, bool kingSide) const
{
    const int home = side == Side::White ? 0 : 56;
    return m_squares[home + 4] == Piece{PieceType::King, side}
        && m_squares[home + (kingSide ? 7 : 0)] == Piece{PieceType::Rook, side};
}

QList<int> PositionSetup::enPassantSquares() const
{
    // The side that just moved is the other one: its pawn went two squares.
    const Side moved = sideToMove == Side::White ? Side::Black : Side::White;
    const int pawnRank = moved == Side::Black ? 4 : 3;
    const int step = moved == Side::Black ? 8 : -8;
    QList<int> squares;
    for (int file = 0; file < 8; ++file) {
        const int pawn = pawnRank * 8 + file;
        const int behind = pawn + step;
        if (m_squares[pawn] == Piece{PieceType::Pawn, moved} && m_squares[behind].isNull()
            && m_squares[behind + step].isNull())
            squares << behind;
    }
    return squares;
}

QString PositionSetup::fen() const
{
    QString placement;
    for (int rank = 7; rank >= 0; --rank) {
        int empty = 0;
        for (int file = 0; file < 8; ++file) {
            const Piece piece = m_squares[rank * 8 + file];
            if (piece.isNull()) {
                ++empty;
                continue;
            }
            if (empty > 0)
                placement += QString::number(empty);
            empty = 0;
            placement += letter(piece);
        }
        if (empty > 0)
            placement += QString::number(empty);
        if (rank > 0)
            placement += QLatin1Char('/');
    }
    QString castling;
    if (whiteKingSide && canCastle(Side::White, true))
        castling += QLatin1Char('K');
    if (whiteQueenSide && canCastle(Side::White, false))
        castling += QLatin1Char('Q');
    if (blackKingSide && canCastle(Side::Black, true))
        castling += QLatin1Char('k');
    if (blackQueenSide && canCastle(Side::Black, false))
        castling += QLatin1Char('q');
    const QString enPassantSquare =
        enPassantSquares().contains(enPassant) ? BoardState::squareName(enPassant) : QStringLiteral("-");
    return QStringLiteral("%1 %2 %3 %4 %5 %6")
        .arg(placement, sideToMove == Side::White ? QStringLiteral("w") : QStringLiteral("b"),
             castling.isEmpty() ? QStringLiteral("-") : castling, enPassantSquare)
        .arg(qMax(0, halfMove))
        .arg(qMax(1, fullMove));
}

QString PositionSetup::problem() const
{
    int kings[2] = {0, 0};
    int pieces[2] = {0, 0};
    int pawns[2] = {0, 0};
    bool pawnOnEdge = false;
    for (int square = 0; square < 64; ++square) {
        const Piece piece = m_squares[square];
        if (piece.isNull())
            continue;
        const int side = int(piece.side);
        ++pieces[side];
        if (piece.type == PieceType::King)
            ++kings[side];
        if (piece.type == PieceType::Pawn) {
            ++pawns[side];
            pawnOnEdge = pawnOnEdge || square < 8 || square >= 56;
        }
    }
    if (kings[0] != 1)
        return kings[0] == 0 ? Text::tr("White has no king.") : Text::tr("White has more than one king.");
    if (kings[1] != 1)
        return kings[1] == 0 ? Text::tr("Black has no king.") : Text::tr("Black has more than one king.");
    if (pawnOnEdge)
        return Text::tr("A pawn cannot stand on the first or the last rank.");
    if (pawns[0] > 8 || pawns[1] > 8)
        return Text::tr("A side cannot have more than eight pawns.");
    if (pieces[0] > 16 || pieces[1] > 16)
        return Text::tr("A side cannot have more than sixteen pieces.");
    if (!ChessPosition::fromFen(fen()))
        return sideToMove == Side::White ? Text::tr("Black is in check, but it is White to move.")
                                         : Text::tr("White is in check, but it is Black to move.");
    return QString();
}
