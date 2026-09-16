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
