#include "BoardState.h"

#include <QStringList>

namespace {

const char kStartingFen[] = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

PieceType typeFromChar(QChar c)
{
    switch (c.toLower().unicode()) {
    case 'p': return PieceType::Pawn;
    case 'n': return PieceType::Knight;
    case 'b': return PieceType::Bishop;
    case 'r': return PieceType::Rook;
    case 'q': return PieceType::Queen;
    case 'k': return PieceType::King;
    default: return PieceType::None;
    }
}

} // namespace

BoardState BoardState::startingPosition()
{
    return *fromFen(QString::fromLatin1(kStartingFen));
}

std::optional<BoardState> BoardState::fromFen(const QString &fen)
{
    const QStringList fields = fen.simplified().split(QLatin1Char(' '));
    const QStringList ranks = fields.value(0).split(QLatin1Char('/'));
    if (ranks.size() != 8)
        return std::nullopt;

    BoardState state;
    for (int i = 0; i < 8; ++i) {
        const int rank = 7 - i;
        int file = 0;
        for (QChar c : ranks.at(i)) {
            if (c.isDigit()) {
                file += c.digitValue();
            } else {
                const PieceType type = typeFromChar(c);
                if (type == PieceType::None || file >= 8)
                    return std::nullopt;
                state.m_squares[rank * 8 + file] = {type, c.isUpper() ? Side::White : Side::Black};
                ++file;
            }
        }
        if (file != 8)
            return std::nullopt;
    }

    const QString side = fields.value(1, QStringLiteral("w"));
    if (side != QLatin1String("w") && side != QLatin1String("b"))
        return std::nullopt;
    state.m_sideToMove = side == QLatin1String("w") ? Side::White : Side::Black;
    state.m_fen = fen.simplified();
    return state;
}

bool BoardState::applyUci(const QString &uci)
{
    if (uci.size() < 4)
        return false;
    const int from = squareFromName(QStringView(uci).mid(0, 2));
    const int to = squareFromName(QStringView(uci).mid(2, 2));
    if (from < 0 || to < 0 || m_squares[from].isNull())
        return false;

    Piece piece = m_squares[from];
    const int fileDelta = to % 8 - from % 8;

    // En passant: a pawn moving diagonally onto an empty square.
    if (piece.type == PieceType::Pawn && fileDelta != 0 && m_squares[to].isNull())
        m_squares[(from / 8) * 8 + to % 8] = {};

    // Castling: the king moves two files, bring the rook along.
    if (piece.type == PieceType::King && (fileDelta == 2 || fileDelta == -2)) {
        const int rank = from / 8;
        const int rookFrom = rank * 8 + (fileDelta > 0 ? 7 : 0);
        const int rookTo = rank * 8 + (fileDelta > 0 ? 5 : 3);
        m_squares[rookTo] = m_squares[rookFrom];
        m_squares[rookFrom] = {};
    }

    if (uci.size() >= 5) {
        const PieceType promotion = typeFromChar(uci.at(4));
        if (promotion != PieceType::None)
            piece.type = promotion;
    }

    m_squares[to] = piece;
    m_squares[from] = {};
    m_sideToMove = m_sideToMove == Side::White ? Side::Black : Side::White;
    m_fen.clear(); // Castling rights and clocks are unknown without the engine.
    return true;
}

int BoardState::squareFromName(QStringView name)
{
    if (name.size() != 2)
        return -1;
    const int file = name.at(0).unicode() - 'a';
    const int rank = name.at(1).unicode() - '1';
    if (file < 0 || file > 7 || rank < 0 || rank > 7)
        return -1;
    return rank * 8 + file;
}

QString BoardState::squareName(int square)
{
    return QString(QChar('a' + square % 8)) + QChar('1' + square / 8);
}
