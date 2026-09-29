#include "PolyglotBook.h"

#include <QtEndian>

#include <algorithm>

namespace {

constexpr qint64 kEntrySize = 16;
constexpr int kCastlingOffset = 768;
constexpr int kEnPassantOffset = 772;
constexpr int kTurnOffset = 780;

/// Polyglot orders pieces pawn … king, black before white.
int pieceIndex(Piece piece)
{
    return (int(piece.type) - 1) * 2 + (piece.side == Side::White ? 1 : 0);
}

} // namespace

bool PolyglotBook::open(const QString &path, QString *errorMessage)
{
    close();
    m_file.setFileName(path);
    if (!m_file.open(QIODevice::ReadOnly)) {
        if (errorMessage)
            *errorMessage = m_file.errorString();
        return false;
    }
    if (m_file.size() == 0 || m_file.size() % kEntrySize != 0) {
        if (errorMessage)
            *errorMessage = QObject::tr("Not a Polyglot opening book.");
        m_file.close();
        return false;
    }
    m_data = m_file.map(0, m_file.size());
    if (!m_data) {
        if (errorMessage)
            *errorMessage = m_file.errorString();
        m_file.close();
        return false;
    }
    m_count = m_file.size() / kEntrySize;
    return true;
}

void PolyglotBook::close()
{
    if (m_data)
        m_file.unmap(const_cast<uchar *>(m_data));
    m_data = nullptr;
    m_count = 0;
    m_file.close();
}

qint64 PolyglotBook::lowerBound(quint64 wanted) const
{
    qint64 low = 0;
    qint64 high = m_count;
    while (low < high) {
        const qint64 middle = low + (high - low) / 2;
        if (qFromBigEndian<quint64>(m_data + middle * kEntrySize) < wanted)
            low = middle + 1;
        else
            high = middle;
    }
    return low;
}

QList<PolyglotBook::Move> PolyglotBook::moves(const ChessPosition &position) const
{
    QList<Move> result;
    if (!m_data)
        return result;
    const quint64 wanted = key(position);
    for (qint64 index = lowerBound(wanted);
         index < m_count && qFromBigEndian<quint64>(m_data + index * kEntrySize) == wanted; ++index) {
        const uchar *entry = m_data + index * kEntrySize;
        const std::optional<ChessMove> move = decodeMove(position, qFromBigEndian<quint16>(entry + 8));
        if (!move)
            continue;
        const int weight = qFromBigEndian<quint16>(entry + 10);
        const quint32 learn = qFromBigEndian<quint32>(entry + 12);
        const auto same = std::find_if(result.begin(), result.end(), [&](const Move &m) { return m.move == *move; });
        if (same != result.end()) {
            same->weight += weight;
            same->learn |= learn;
        } else {
            result << Move{*move, weight, learn};
        }
    }
    std::stable_sort(result.begin(), result.end(), [](const Move &a, const Move &b) {
        if (a.inRepertoire() != b.inRepertoire())
            return a.inRepertoire();
        return a.weight > b.weight;
    });
    return result;
}

bool PolyglotBook::setInRepertoire(const ChessPosition &position, const ChessMove &move, bool inRepertoire,
                                   QString *errorMessage)
{
    const auto fail = [&](const QString &message) {
        if (errorMessage)
            *errorMessage = message;
        return false;
    };
    if (!m_data)
        return fail(QObject::tr("No book is open."));

    // Every entry of the move (a book may repeat one), with its new learn bits.
    const quint64 wanted = key(position);
    QList<std::pair<qint64, quint32>> changes;
    for (qint64 index = lowerBound(wanted);
         index < m_count && qFromBigEndian<quint64>(m_data + index * kEntrySize) == wanted; ++index) {
        const uchar *entry = m_data + index * kEntrySize;
        if (decodeMove(position, qFromBigEndian<quint16>(entry + 8)) != move)
            continue;
        const quint32 learn = qFromBigEndian<quint32>(entry + 12);
        changes << std::pair{index, inRepertoire ? learn | kLearnRepertoire : learn & ~kLearnRepertoire};
    }
    if (changes.isEmpty())
        return fail(QObject::tr("The move is not in the book."));

    // Written through a handle of its own, with the map released, as every
    // platform allows; the book is mapped again afterwards.
    const QString bookPath = path();
    close();
    QFile file(bookPath);
    bool written = file.open(QIODevice::ReadWrite);
    for (const auto &[index, learn] : std::as_const(changes)) {
        uchar bytes[4];
        qToBigEndian(learn, bytes);
        written = written && file.seek(index * kEntrySize + 12)
                  && file.write(reinterpret_cast<const char *>(bytes), 4) == 4;
    }
    const QString writeError = file.errorString();
    file.close();
    QString reopenError;
    if (!open(bookPath, &reopenError))
        return fail(reopenError);
    return written || fail(writeError);
}

quint64 PolyglotBook::key(const ChessPosition &position)
{
    quint64 hash = 0;
    for (int square = 0; square < 64; ++square) {
        const Piece piece = position.at(square);
        if (!piece.isNull())
            hash ^= kRandom[64 * pieceIndex(piece) + square];
    }

    for (int i = 0; i < 4; ++i) {
        if (position.castlingRights() & (1 << i))
            hash ^= kRandom[kCastlingOffset + i];
    }

    // The en passant file counts only if a pawn of the side to move can take.
    const int enPassant = position.enPassantSquare();
    const Side side = position.sideToMove();
    if (enPassant >= 0) {
        const int pawnRank = side == Side::White ? 4 : 3;
        const int file = enPassant % 8;
        for (int adjacent : {file - 1, file + 1}) {
            if (adjacent < 0 || adjacent > 7)
                continue;
            const Piece piece = position.at(pawnRank * 8 + adjacent);
            if (piece.type == PieceType::Pawn && piece.side == side) {
                hash ^= kRandom[kEnPassantOffset + file];
                break;
            }
        }
    }

    if (side == Side::White)
        hash ^= kRandom[kTurnOffset];
    return hash;
}

quint16 PolyglotBook::encodeMove(const ChessPosition &position, const ChessMove &move)
{
    int to = move.to;
    if (position.at(move.from).type == PieceType::King && qAbs(move.to - move.from) == 2)
        to = move.to > move.from ? move.from + 3 : move.from - 4; // The rook's square.
    int promotion = 0;
    switch (move.promotion) {
    case PieceType::Knight: promotion = 1; break;
    case PieceType::Bishop: promotion = 2; break;
    case PieceType::Rook: promotion = 3; break;
    case PieceType::Queen: promotion = 4; break;
    default: break;
    }
    return quint16((promotion << 12) | ((move.from / 8) << 9) | ((move.from % 8) << 6) | ((to / 8) << 3) | (to % 8));
}

std::optional<ChessMove> PolyglotBook::decodeMove(const ChessPosition &position, quint16 raw)
{
    ChessMove move;
    move.to = ((raw >> 3) & 7) * 8 + (raw & 7);
    move.from = ((raw >> 9) & 7) * 8 + ((raw >> 6) & 7);
    static constexpr PieceType promotions[] = {PieceType::None, PieceType::Knight, PieceType::Bishop,
                                               PieceType::Rook, PieceType::Queen};
    const int promotion = (raw >> 12) & 7;
    if (promotion > 4)
        return std::nullopt;
    move.promotion = promotions[promotion];

    const Piece piece = position.at(move.from);
    const Piece target = position.at(move.to);
    if (piece.type == PieceType::King && target.type == PieceType::Rook && target.side == piece.side)
        move.to = move.to > move.from ? move.from + 2 : move.from - 2;
    if (!position.isLegal(move))
        return std::nullopt;
    return move;
}

QByteArray PolyglotBook::write(QList<Entry> entries)
{
    std::stable_sort(entries.begin(), entries.end(), [](const Entry &a, const Entry &b) {
        return a.key != b.key ? a.key < b.key : a.weight > b.weight;
    });
    QByteArray data(entries.size() * kEntrySize, '\0');
    auto *out = reinterpret_cast<uchar *>(data.data());
    for (const Entry &entry : std::as_const(entries)) {
        qToBigEndian(entry.key, out);
        qToBigEndian(entry.move, out + 8);
        qToBigEndian(entry.weight, out + 10);
        qToBigEndian(entry.learn, out + 12);
        out += kEntrySize;
    }
    return data;
}
