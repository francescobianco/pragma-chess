#include "CbgDecoder.h"

#include <QCoreApplication>
#include <QList>

#include <array>

namespace {

struct Text {
    Q_DECLARE_TR_FUNCTIONS(CbgDecoder)
};

// The table of the common encoding: a byte of the stream, less the number
// of moves decoded before it, looked up here gives the code of the move.
constexpr std::array<quint8, 256> kTable{
    0xA2, 0x95, 0x43, 0xF5, 0xC1, 0x3D, 0x4A, 0x6C, 0x53, 0x83, 0xCC, 0x7C, 0xFF, 0xAE, 0x68, 0xAD,
    0xD1, 0x92, 0x8B, 0x8D, 0x35, 0x81, 0x5E, 0x74, 0x26, 0x8E, 0xAB, 0xCA, 0xFD, 0x9A, 0xF3, 0xA0,
    0xA5, 0x15, 0xFC, 0xB1, 0x1E, 0xED, 0x30, 0xEA, 0x22, 0xEB, 0xA7, 0xCD, 0x4E, 0x6F, 0x2E, 0x24,
    0x32, 0x94, 0x41, 0x8C, 0x6E, 0x58, 0x82, 0x50, 0xBB, 0x02, 0x8A, 0xD8, 0xFA, 0x60, 0xDE, 0x52,
    0xBA, 0x46, 0xAC, 0x29, 0x9D, 0xD7, 0xDF, 0x08, 0x21, 0x01, 0x66, 0xA3, 0xF1, 0x19, 0x27, 0xB5,
    0x91, 0xD5, 0x42, 0x0E, 0xB4, 0x4C, 0xD9, 0x18, 0x5F, 0xBC, 0x25, 0xA6, 0x96, 0x04, 0x56, 0x6A,
    0xAA, 0x33, 0x1C, 0x2B, 0x73, 0xF0, 0xDD, 0xA4, 0x37, 0xD3, 0xC5, 0x10, 0xBF, 0x5A, 0x23, 0x34,
    0x75, 0x5B, 0xB8, 0x55, 0xD2, 0x6B, 0x09, 0x3A, 0x57, 0x12, 0xB3, 0x77, 0x48, 0x85, 0x9B, 0x0F,
    0x9E, 0xC7, 0xC8, 0xA1, 0x7F, 0x7A, 0xC0, 0xBD, 0x31, 0x6D, 0xF6, 0x3E, 0xC3, 0x11, 0x71, 0xCE,
    0x7D, 0xDA, 0xA8, 0x54, 0x90, 0x97, 0x1F, 0x44, 0x40, 0x16, 0xC9, 0xE3, 0x2C, 0xCB, 0x84, 0xEC,
    0x9F, 0x3F, 0x5C, 0xE6, 0x76, 0x0B, 0x3C, 0x20, 0xB7, 0x36, 0x00, 0xDC, 0xE7, 0xF9, 0x4F, 0xF7,
    0xAF, 0x06, 0x07, 0xE0, 0x1A, 0x0A, 0xA9, 0x4B, 0x0C, 0xD6, 0x63, 0x87, 0x89, 0x1D, 0x13, 0x1B,
    0xE4, 0x70, 0x05, 0x47, 0x67, 0x7B, 0x2F, 0xEE, 0xE2, 0xE8, 0x98, 0x0D, 0xEF, 0xCF, 0xC4, 0xF4,
    0xFB, 0xB0, 0x17, 0x99, 0x64, 0xF2, 0xD4, 0x2A, 0x03, 0x4D, 0x78, 0xC6, 0xFE, 0x65, 0x86, 0x88,
    0x79, 0x45, 0x3B, 0xE5, 0x49, 0x8F, 0x2D, 0xB9, 0xBE, 0x62, 0x93, 0x14, 0xE9, 0xD0, 0x38, 0x9C,
    0xB2, 0xC2, 0x59, 0x5D, 0xB6, 0x72, 0x51, 0xF8, 0x28, 0x7E, 0x61, 0x39, 0xE1, 0xDB, 0x69, 0x80,
};

// The codes that are not moves. A branch code says that the moves from
// here to the matching end of line are the main continuation and that
// alternatives to them follow that end; the main line never needs it.
constexpr int kNullMove = 0;
constexpr int kTwoBytes = 235;
constexpr int kIgnore = 236;
constexpr int kBranch = 254;
constexpr int kEndLine = 255;

enum Piece : quint8 { None = 0, King, Queen, Rook, Bishop, Knight, Pawn };
constexpr int kWhite = 0;
constexpr int kBlack = 1;
/// The kinds the codes number: queens, rooks, bishops, knights.
constexpr Piece kKinds[4] = {Queen, Rook, Bishop, Knight};

struct Delta {
    int dx;
    int dy;
};
constexpr Delta kKing[8] = {{0, 1}, {1, 1}, {1, 0}, {1, -1}, {0, -1}, {-1, -1}, {-1, 0}, {-1, 1}};
constexpr Delta kKnight[8] = {{2, 1}, {1, 2}, {-1, 2}, {-2, 1}, {-2, -1}, {-1, -2}, {1, -2}, {2, -1}};
constexpr Delta kQueenDirections[4] = {{0, 1}, {1, 0}, {1, 1}, {1, -1}};
constexpr Delta kRookDirections[2] = {{0, 1}, {1, 0}};
constexpr Delta kBishopDirections[2] = {{1, 1}, {1, -1}};

/// A square as ChessBase counts them: a1 = 0, a2 = 1, … file by file.
struct Square {
    int file;
    int rank;
    static Square at(int index) { return {index / 8, index % 8}; }
    int index() const { return file * 8 + rank; }
    QString uci() const { return QString(QChar(u'a' + file)) + QChar(u'1' + rank); }
};

struct Man {
    Piece piece = None;
    int side = kWhite;
};

/// The board and the lists the codes count on.
struct State {
    std::array<Man, 64> board{};
    /// Queens, rooks, bishops and knights of each side, in encoding order.
    QList<int> kinds[2][4];
    /// The pawns of each side by their number, -1 once gone.
    int pawns[2][8];
    int sideToMove = kWhite;

    State()
    {
        for (int side : {kWhite, kBlack})
            for (int &pawn : pawns[side])
                pawn = -1;
    }

    static int kindOf(Piece piece)
    {
        for (int kind = 0; kind < 4; ++kind)
            if (kKinds[kind] == piece)
                return kind;
        return -1;
    }

    /// Numbers the men on the board in square order, as ChessBase does.
    void scan()
    {
        int nextPawn[2] = {0, 0};
        for (int index = 0; index < 64; ++index) {
            const Man man = board[index];
            if (man.piece == Pawn) {
                if (nextPawn[man.side] < 8)
                    pawns[man.side][nextPawn[man.side]++] = index;
            } else if (const int kind = kindOf(man.piece); kind >= 0) {
                kinds[man.side][kind] << index;
            }
        }
    }

    void setInitial()
    {
        const Piece backRank[8] = {Rook, Knight, Bishop, Queen, King, Bishop, Knight, Rook};
        for (int file = 0; file < 8; ++file) {
            board[Square{file, 0}.index()] = {backRank[file], kWhite};
            board[Square{file, 1}.index()] = {Pawn, kWhite};
            board[Square{file, 6}.index()] = {Pawn, kBlack};
            board[Square{file, 7}.index()] = {backRank[file], kBlack};
        }
        scan();
    }

    int king(int side) const
    {
        for (int index = 0; index < 64; ++index)
            if (board[index].piece == King && board[index].side == side)
                return index;
        return -1;
    }

    /// Takes the man on `index` off the lists (and the board).
    void remove(int index)
    {
        const Man man = board[index];
        if (man.piece == Pawn) {
            for (int &pawn : pawns[man.side])
                if (pawn == index)
                    pawn = -1;
        } else if (const int kind = kindOf(man.piece); kind >= 0) {
            kinds[man.side][kind].removeAll(index); // The ones after it move up.
        }
        board[index] = {};
    }

    /// Moves the man on `from` to `to` on the board and in the lists.
    void relocate(int from, int to)
    {
        const Man man = board[from];
        if (man.piece == Pawn) {
            for (int &pawn : pawns[man.side])
                if (pawn == from)
                    pawn = to;
        } else if (const int kind = kindOf(man.piece); kind >= 0) {
            for (int &square : kinds[man.side][kind])
                if (square == from)
                    square = to;
        }
        board[to] = man;
        board[from] = {};
    }

    /// Plays a move, keeping the lists in step: captures, en passant,
    /// castling by the king's two-square step, promotion.
    void play(int from, int to, Piece promotion)
    {
        const Man man = board[from];
        const Square source = Square::at(from);
        const Square target = Square::at(to);
        if (board[to].piece != None && board[to].side != man.side)
            remove(to);
        else if (man.piece == Pawn && source.file != target.file && board[to].piece == None)
            remove(Square{target.file, source.rank}.index()); // En passant.
        if (man.piece == King && qAbs(target.file - source.file) == 2) {
            const int rookFrom = Square{target.file > source.file ? 7 : 0, source.rank}.index();
            const int rookTo = Square{target.file > source.file ? 5 : 3, source.rank}.index();
            if (board[rookFrom].piece == Rook)
                relocate(rookFrom, rookTo);
        }
        relocate(from, to);
        if (promotion != None && man.piece == Pawn) {
            for (int &pawn : pawns[man.side])
                if (pawn == to)
                    pawn = -1;
            board[to] = {promotion, man.side};
            if (const int kind = kindOf(promotion); kind >= 0)
                kinds[man.side][kind] << to; // A new man goes last.
        }
        sideToMove = 1 - sideToMove;
    }

    QString fen(int castling, int enPassantFile, int moveNumber) const
    {
        QString text;
        for (int rank = 7; rank >= 0; --rank) {
            int empty = 0;
            for (int file = 0; file < 8; ++file) {
                const Man man = board[Square{file, rank}.index()];
                if (man.piece == None) {
                    ++empty;
                    continue;
                }
                if (empty > 0)
                    text += QString::number(empty);
                empty = 0;
                const QChar letter = QLatin1Char("?KQRBNP"[man.piece]);
                text += man.side == kWhite ? letter : letter.toLower();
            }
            if (empty > 0)
                text += QString::number(empty);
            if (rank > 0)
                text += QLatin1Char('/');
        }
        text += sideToMove == kWhite ? QStringLiteral(" w ") : QStringLiteral(" b ");
        QString rights;
        if (castling & 2)
            rights += QLatin1Char('K');
        if (castling & 1)
            rights += QLatin1Char('Q');
        if (castling & 8)
            rights += QLatin1Char('k');
        if (castling & 4)
            rights += QLatin1Char('q');
        text += rights.isEmpty() ? QStringLiteral("-") : rights;
        if (enPassantFile >= 1 && enPassantFile <= 8)
            text += QStringLiteral(" %1%2").arg(QChar(u'a' + enPassantFile - 1)).arg(sideToMove == kWhite ? 6 : 3);
        else
            text += QStringLiteral(" -");
        text += QStringLiteral(" 0 %1").arg(qMax(1, moveNumber));
        return text;
    }
};

/// A line of `steps` squares along `direction`; the codes count seven of
/// each direction.
Delta along(int code, const Delta *directions)
{
    const Delta direction = directions[code / 7];
    const int steps = code % 7 + 1;
    return {direction.dx * steps, direction.dy * steps};
}

/// The square `delta` away from `from`, each coordinate taken modulo 8, as
/// the format does.
int stepped(int from, Delta delta)
{
    const Square square = Square::at(from);
    return Square{((square.file + delta.dx) % 8 + 8) % 8, ((square.rank + delta.dy) % 8 + 8) % 8}.index();
}

/// Reads the start position stored before the moves: side to move and the
/// en passant file, the castling rights, the move number, then 192 bits of
/// board, a 0 bit for an empty square or a 1, a colour bit and three bits of piece.
bool readStart(const QByteArray &start, State &state, QString *fen)
{
    const auto byte = [&](int i) { return quint8(start.at(i)); };
    state.sideToMove = byte(1) & 0x10 ? kBlack : kWhite;
    const int enPassantFile = byte(1) & 0x0F;
    const int castling = byte(2) & 0x0F;
    const int moveNumber = byte(3);
    int bit = 0;
    const auto nextBit = [&] {
        const int value = (byte(4 + bit / 8) >> (7 - bit % 8)) & 1;
        ++bit;
        return value;
    };
    for (int index = 0; index < 64; ++index) {
        if (bit >= 192)
            return false;
        if (nextBit() == 0)
            continue;
        if (bit + 4 > 192)
            return false;
        const int side = nextBit() ? kBlack : kWhite;
        int code = 0;
        for (int i = 0; i < 3; ++i)
            code = code * 2 + nextBit();
        static const Piece pieces[8] = {None, King, Queen, Knight, Bishop, Rook, Pawn, None};
        if (pieces[code] == None)
            return false;
        state.board[index] = {pieces[code], side};
    }
    state.scan();
    *fen = state.fen(castling, enPassantFile, moveNumber);
    return true;
}


/// Reads the stream of moves as ChessBase writes the tree: the main line
/// first, and at a branch code the continuation of the line up to the
/// matching end code, then the alternatives to that continuation's first
/// move, which end with the line they belong to.
struct Reader {
    const QByteArray &record;
    int end;
    int i;
    int decodedMoves; // Also the key of the table.

    int translate(int at) const { return int(kTable[quint8(quint8(record.at(at)) - quint8(decodedMoves))]); }

    /// Decodes the move the code names in `state`; `from` stays -1 for a null move.
    QString moveOf(int code, const State &state, int &from, int &to, Piece &promotion)
    {
        const int us = state.sideToMove;
        const int forward = us == kWhite ? 1 : -1;
        from = to = -1;
        promotion = None;
        if (code == kTwoBytes) {
            if (i + 2 > end)
                return Text::tr("A two-byte move runs past the record.");
            const int word = (translate(i) << 8) | translate(i + 1);
            i += 2;
            from = word & 0x3F;
            to = (word >> 6) & 0x3F;
            if (state.board[from].piece == Pawn && (Square::at(to).rank == 0 || Square::at(to).rank == 7))
                promotion = Piece(std::array<Piece, 4>{Queen, Rook, Bishop, Knight}[(word >> 12) & 3]);
            return {};
        }
        if (code == kNullMove)
            return {};
        if (code >= 1 && code <= 8) {
            from = state.king(us);
            if (from < 0)
                return Text::tr("The king is missing.");
            to = stepped(from, kKing[code - 1]);
            return {};
        }
        if (code == 9 || code == 10) {
            from = state.king(us);
            if (from < 0)
                return Text::tr("The king is missing.");
            to = Square{code == 9 ? 6 : 2, Square::at(from).rank}.index();
            return {};
        }
        if (code >= 111 && code <= 142) {
            const int pawn = (code - 111) / 4;
            const Delta ways[4] = {{0, forward}, {0, 2 * forward}, {forward, forward}, {-forward, forward}};
            from = state.pawns[us][pawn];
            if (from < 0)
                return Text::tr("Pawn %1 is gone.").arg(pawn + 1);
            to = stepped(from, ways[(code - 111) % 4]);
            return {};
        }
        // Queens, rooks, bishops and knights, three of each, by number.
        struct Range {
            int first, last, kind, number;
        };
        static const Range ranges[] = {{11, 38, 0, 0},   {39, 52, 1, 0},   {53, 66, 1, 1},   {67, 80, 2, 0},
                                       {81, 94, 2, 1},   {95, 102, 3, 0},  {103, 110, 3, 1}, {143, 170, 0, 1},
                                       {171, 198, 0, 2}, {199, 212, 1, 2}, {213, 226, 2, 2}, {227, 234, 3, 2}};
        const Range *range = nullptr;
        for (const Range &candidate : ranges)
            if (code >= candidate.first && code <= candidate.last)
                range = &candidate;
        if (!range)
            return Text::tr("Unknown move code %1.").arg(code);
        const QList<int> &list = state.kinds[us][range->kind];
        if (range->number >= list.size())
            return Text::tr("There is no piece number %1 of that kind.").arg(range->number + 1);
        from = list.at(range->number);
        const int rel = code - range->first;
        Delta delta{};
        switch (range->kind) {
        case 0: delta = along(rel, kQueenDirections); break;
        case 1: delta = along(rel, kRookDirections); break;
        case 2: delta = along(rel, kBishopDirections); break;
        default: delta = kKnight[rel]; break;
        }
        to = stepped(from, delta);
        return {};
    }

    /// Reads a line from `state` to its end code (or the record's end) into
    /// `moves`, with the alternatives to its moves into `variations`. A null
    /// move is recorded as UCI writes it, "0000". Returns an error message,
    /// or nothing.
    QString readLine(State &state, QList<MoveRecord> &moves, QList<Variation> &variations, bool recording)
    {
        while (i < end) {
            const int code = translate(i++);
            if (code == kIgnore)
                continue;
            if (code == kEndLine)
                return {};
            if (code == kBranch) {
                // The line goes on inside the block; what follows the block
                // is an alternative to the block's first move.
                State before = state;
                const qsizetype branch = moves.size();
                if (const QString error = readLine(state, moves, variations, recording); !error.isEmpty())
                    return error;
                Variation alternative;
                alternative.atPly = int(branch) + 1;
                const QString error = readLine(before, alternative.moves, alternative.variations, recording && moves.size() > branch);
                if (recording && !alternative.moves.isEmpty() && moves.size() > branch)
                    variations << alternative;
                return error;
            }
            if (code > kTwoBytes && code < kBranch)
                return Text::tr("Unknown move code %1.").arg(code);
            int from, to;
            Piece promotion;
            if (const QString error = moveOf(code, state, from, to, promotion); !error.isEmpty())
                return error;
            if (from < 0) {
                state.sideToMove = 1 - state.sideToMove; // A null move: the side passes.
                if (recording)
                    moves << MoveRecord{QString(), QStringLiteral("0000"), {}};
            } else {
                if (state.board[from].piece == None)
                    return Text::tr("Move %1: there is no piece on %2.").arg(decodedMoves + 1).arg(Square::at(from).uci());
                QString uci = Square::at(from).uci() + Square::at(to).uci();
                if (promotion != None)
                    uci += QLatin1Char("?kqrbnp"[promotion]);
                if (recording)
                    moves << MoveRecord{QString(), uci, {}};
                state.play(from, to, promotion);
            }
            ++decodedMoves;
        }
        return {};
    }
};

} // namespace

namespace CbgDecoder {

int recordSize(const QByteArray &head)
{
    if (head.size() < 4)
        return 0;
    return (quint8(head.at(1)) << 16) | (quint8(head.at(2)) << 8) | quint8(head.at(3));
}

Decoded decode(const QByteArray &record)
{
    Decoded decoded;
    const auto fail = [&](const QString &why) {
        decoded.error = why;
        return decoded;
    };
    if (record.size() < 4 || recordSize(record) > record.size())
        return fail(Text::tr("The move record is cut short."));
    const quint8 flags = quint8(record.at(0));
    const int mode = flags & 0x3F;
    if (mode != 0)
        return fail(Text::tr("The moves use an encoding Pragma Chess does not read (mode %1).").arg(mode));

    State state;
    int offset = 4;
    if (flags & 0x40) {
        if (record.size() < 4 + 28)
            return fail(Text::tr("The start position is cut short."));
        if (!readStart(record.mid(4, 28), state, &decoded.startFen))
            return fail(Text::tr("The start position cannot be read."));
        offset += 28;
    } else {
        state.setInitial();
    }

    const int end = recordSize(record);
    Reader reader{record, end, offset, 0};
    QList<MoveRecord> moves;
    const QString error = reader.readLine(state, moves, decoded.variations, true);
    for (const MoveRecord &move : moves)
        decoded.uciMoves << move.uci;
    if (!error.isEmpty())
        return fail(error);
    return decoded;
}

} // namespace CbgDecoder
