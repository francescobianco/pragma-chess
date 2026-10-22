#include "EnCroissantGames.h"

#include <algorithm>
#include <tuple>

namespace EnCroissantGames {

namespace {

constexpr quint8 kVariationStart = 255;
constexpr quint8 kVariationEnd = 254;
constexpr quint8 kComment = 253;
constexpr quint8 kNag = 252;

/// The groups of shakmaty's generation, in its order.
enum Group {
    EnPassant,
    KingInCheck,
    PawnCaptureWest,
    PawnCaptureEast,
    PawnStep,
    PawnStepPromotion,
    PawnDoubleStep,
    Knight,
    Bishop,
    Rook,
    Queen,
    King,
    CastleKingSide,
    CastleQueenSide,
};

int promotionRank(PieceType promotion)
{
    switch (promotion) {
    case PieceType::Queen:
        return 0;
    case PieceType::Rook:
        return 1;
    case PieceType::Bishop:
        return 2;
    case PieceType::Knight:
        return 3;
    default:
        return -1;
    }
}

/// Where `move` comes in shakmaty's list: (group, promotions after the
/// others, square, square, promotion).
std::tuple<int, int, int, int, int> orderOf(const ChessPosition &position, const ChessMove &move, bool inCheck)
{
    const PieceType piece = position.at(move.from).type;
    const int fileFrom = move.from % 8;
    const int fileTo = move.to % 8;
    const int promotion = promotionRank(move.promotion);
    const int promoted = promotion >= 0 ? 1 : 0;
    switch (piece) {
    case PieceType::Pawn:
        if (fileFrom != fileTo) {
            if (move.to == position.enPassantSquare() && position.at(move.to).isNull())
                return {EnPassant, 0, move.from, 0, 0};
            return {fileTo < fileFrom ? PawnCaptureWest : PawnCaptureEast, promoted, move.to, 0, promotion};
        }
        if (qAbs(move.to - move.from) == 16)
            return {PawnDoubleStep, 0, move.to, 0, 0};
        return {promoted ? PawnStepPromotion : PawnStep, 0, move.to, 0, promotion};
    case PieceType::Knight:
        return {Knight, 0, move.from, move.to, 0};
    case PieceType::Bishop:
        return {Bishop, 0, move.from, move.to, 0};
    case PieceType::Rook:
        return {Rook, 0, move.from, move.to, 0};
    case PieceType::Queen:
        return {Queen, 0, move.from, move.to, 0};
    case PieceType::King:
        if (qAbs(fileTo - fileFrom) == 2)
            return {fileTo > fileFrom ? CastleKingSide : CastleQueenSide, 0, 0, 0, 0};
        return {inCheck ? KingInCheck : King, 0, move.to, 0, 0};
    case PieceType::None:
        break;
    }
    return {CastleQueenSide + 1, 0, move.from, move.to, 0};
}

quint16 lengthAt(const QByteArray &bytes, qsizetype at)
{
    return quint16(quint8(bytes.at(at)) | quint8(bytes.at(at + 1)) << 8);
}

} // namespace

QList<ChessMove> orderedMoves(const ChessPosition &position)
{
    QList<ChessMove> moves = position.legalMoves();
    const bool inCheck = position.inCheck();
    std::sort(moves.begin(), moves.end(), [&](const ChessMove &a, const ChessMove &b) {
        return orderOf(position, a, inCheck) < orderOf(position, b, inCheck);
    });
    return moves;
}

std::optional<QList<MoveRecord>> mainLine(const QByteArray &bytes, const ChessPosition &start)
{
    QList<MoveRecord> line;
    ChessPosition position = start;
    int depth = 0;
    for (qsizetype at = 0; at < bytes.size();) {
        const quint8 byte = quint8(bytes.at(at++));
        if (byte == kVariationStart) {
            ++depth;
        } else if (byte == kVariationEnd) {
            depth = qMax(0, depth - 1);
        } else if (byte == kComment || byte == kNag) {
            if (at + 2 > bytes.size())
                return std::nullopt;
            at += 2 + lengthAt(bytes, at);
        } else if (depth == 0) {
            const QList<ChessMove> moves = orderedMoves(position);
            if (byte >= moves.size())
                return std::nullopt;
            const ChessMove &move = moves.at(byte);
            line << MoveRecord{position.san(move), move.uci()};
            position.play(move);
        }
    }
    return line;
}

} // namespace EnCroissantGames
