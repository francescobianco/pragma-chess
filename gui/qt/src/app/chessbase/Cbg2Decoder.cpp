#include "Cbg2Decoder.h"

#include <QCoreApplication>
#include <QtEndian>

#include <array>
#include <vector>

namespace {

struct Text {
    Q_DECLARE_TR_FUNCTIONS(Cbg2Decoder)
};

constexpr quint16 kNullMove = 0xFFFA;
constexpr quint16 kSetUp = 0xFFFB;
constexpr quint16 kMoves = 0xFFFC;
constexpr quint16 kHasAlternative = 0xFFFD;
constexpr quint16 kEndLine = 0xFFFF;

/// A record of the `.2cbg`: the magic number, then the sizes, a checksum
/// and the tag; the content follows at kContent.
constexpr char kMagic[] = "\x88\x77\x66\x55\x44\x33\x22\x11";
constexpr int kContent = 0x1A;

/// A move of the table: squares as ChessBase numbers them, file-major
/// (a1 = 0, a2 = 1, …, b1 = 8), and the promotion letter or 0.
struct TableMove {
    quint8 from = 0;
    quint8 to = 0;
    char promotion = 0;
    bool valid = false;
};

struct Step {
    int file, rank;
};

/// The words, in the order ChessBase enumerates them: for each side the
/// king, queen, knight, bishop and rook moves — every origin square, every
/// direction, outward, six words per destination (a quiet move and the
/// capture of each kind of piece, which name the same move) —, then the
/// white pawns, the black pawns, and castling. Word 0 is no move.
const std::vector<TableMove> &table()
{
    static const std::vector<TableMove> moves = [] {
        std::vector<TableMove> list(1);
        const auto square = [](int file, int rank) { return quint8(file * 8 + rank); };
        const auto add = [&](int from, int to, char promotion, int times) {
            for (int i = 0; i < times; ++i)
                list.push_back({quint8(from), quint8(to), promotion, true});
        };
        static const std::array<Step, 8> king{{{-1, -1}, {-1, 0}, {-1, 1}, {0, -1}, {0, 1}, {1, -1}, {1, 0}, {1, 1}}};
        static const std::array<Step, 8> knight{{{-2, -1}, {-2, 1}, {2, -1}, {2, 1}, {-1, -2}, {-1, 2}, {1, -2}, {1, 2}}};
        static const std::array<Step, 4> bishop{{{-1, -1}, {1, -1}, {1, 1}, {-1, 1}}};
        static const std::array<Step, 4> rook{{{-1, 0}, {0, -1}, {1, 0}, {0, 1}}};
        static const std::array<Step, 8> queen{{{-1, -1}, {1, -1}, {1, 1}, {-1, 1}, {-1, 0}, {0, -1}, {1, 0}, {0, 1}}};
        const auto piece = [&](const auto &steps, bool slides) {
            for (int origin = 0; origin < 64; ++origin) {
                for (const Step &step : steps) {
                    int file = origin / 8 + step.file;
                    int rank = origin % 8 + step.rank;
                    while (file >= 0 && file < 8 && rank >= 0 && rank < 8) {
                        add(origin, square(file, rank), 0, 6);
                        if (!slides)
                            break;
                        file += step.file;
                        rank += step.rank;
                    }
                }
            }
        };
        for (int side = 0; side < 2; ++side) {
            piece(king, false);
            piece(queen, true);
            piece(knight, false);
            piece(bishop, true);
            piece(rook, true);
        }
        static const char promotions[] = {'q', 'n', 'b', 'r'};
        for (const bool white : {true, false}) {
            const int forward = white ? 1 : -1;
            const int start = white ? 1 : 6;
            const int beforePromotion = white ? 6 : 1;
            const int enPassant = white ? 4 : 3;
            for (int file = 0; file < 8; ++file) {
                for (int rank = 1; rank <= 6; ++rank) {
                    const int origin = square(file, rank);
                    const int ahead = square(file, rank + forward);
                    if (rank == start) {
                        add(origin, square(file, rank + 2 * forward), 0, 1);
                        add(origin, ahead, 0, 1);
                    } else if (rank == beforePromotion) {
                        for (const char promotion : promotions)
                            add(origin, ahead, promotion, 1);
                    } else {
                        add(origin, ahead, 0, 1);
                    }
                    // Captures towards the lower file, then the higher: one word
                    // per piece taken (en passant too on its rank); on the
                    // promotion rank, per piece taken and per promotion.
                    for (const int side : {-1, 1}) {
                        if (file + side < 0 || file + side > 7)
                            continue;
                        const int target = square(file + side, rank + forward);
                        if (rank == beforePromotion) {
                            for (int taken = 0; taken < 4; ++taken)
                                for (const char promotion : promotions)
                                    add(origin, target, promotion, 1);
                        } else {
                            add(origin, target, 0, rank == enPassant ? 6 : 5);
                        }
                    }
                }
            }
        }
        // Castling: White long, White short, Black long, Black short.
        add(square(4, 0), square(2, 0), 0, 1);
        add(square(4, 0), square(6, 0), 0, 1);
        add(square(4, 7), square(2, 7), 0, 1);
        add(square(4, 7), square(6, 7), 0, 1);
        return list;
    }();
    return moves;
}

QString uciOf(const TableMove &move)
{
    const auto name = [](int square) {
        return QString(QChar(u'a' + square / 8)) + QChar(u'1' + square % 8);
    };
    QString uci = name(move.from) + name(move.to);
    if (move.promotion)
        uci += QLatin1Char(move.promotion);
    return uci;
}

/// Reads the words of a content, line by line, as CbgDecoder's reader does.
struct Reader {
    const QByteArray &content;
    qsizetype next = 0;

    bool atEnd() const { return next + 2 > content.size(); }
    quint16 word()
    {
        const quint16 value = qFromLittleEndian<quint16>(content.constData() + next);
        next += 2;
        return value;
    }

    /// Reads a line to its end into `moves`, the alternatives to its moves
    /// into `variations`. A null move is recorded as "0000". In a variation
    /// (`sisters` given), an alternative to its first move is not a line of
    /// its own: it is a sister, an alternative to the same move, and goes to
    /// `sisters` — the order PGN lists them in, and ChessBase counts.
    QString readLine(QList<MoveRecord> &moves, QList<Variation> &variations, bool recording,
                     QList<Variation> *sisters = nullptr)
    {
        while (!atEnd()) {
            const quint16 code = word();
            if (code == kEndLine)
                return {};
            if (code == kHasAlternative) {
                // The move just read has an alternative: the line goes on to
                // its end, then the alternative follows, from before that move.
                const int branch = int(moves.size()) - 1;
                if (const QString error = readLine(moves, variations, recording, sisters); !error.isEmpty())
                    return error;
                Variation alternative;
                QList<Variation> more;
                const QString error = readLine(alternative.moves, alternative.variations, recording, &more);
                more.prepend(alternative);
                more.removeIf([](const Variation &line) { return line.moves.isEmpty(); });
                if (branch == 0 && sisters) {
                    *sisters << more;
                    return error;
                }
                for (Variation &line : more)
                    line.atPly = branch + 1;
                if (recording && branch >= 0)
                    variations << more;
                return error;
            }
            if (code == kNullMove) {
                if (recording)
                    moves << MoveRecord{QString(), QStringLiteral("0000"), {}};
                continue;
            }
            if (code == kSetUp || code == kMoves)
                return Text::tr("Unexpected marker %1 among the moves.").arg(code, 4, 16, QLatin1Char('0'));
            const std::vector<TableMove> &moveTable = table();
            if (code >= moveTable.size() || !moveTable[code].valid)
                return Text::tr("Unknown move word %1.").arg(code, 4, 16, QLatin1Char('0'));
            if (recording)
                moves << MoveRecord{QString(), uciOf(moveTable[code]), {}};
        }
        return {};
    }
};

} // namespace

namespace Cbg2Decoder {

QString moveOf(quint16 word)
{
    const std::vector<TableMove> &moves = table();
    return word < moves.size() && moves[word].valid ? uciOf(moves[word]) : QString();
}

int lastMoveWord()
{
    return int(table().size()) - 1;
}

QByteArray contentAt(const QByteArray &data, qint64 offset, bool *chess960, QString *error)
{
    const auto fail = [&](const QString &why) {
        if (error)
            *error = why;
        return QByteArray();
    };
    if (offset < 0 || offset + kContent > data.size() || data.mid(offset, 8) != QByteArray(kMagic, 8))
        return fail(Text::tr("The moves of the game are not where its header says."));
    const qint32 size = qFromLittleEndian<qint32>(data.constData() + offset + 8);
    if (size < 0 || offset + kContent + size > data.size())
        return fail(Text::tr("The move record is cut short."));
    if (chess960)
        *chess960 = qFromLittleEndian<quint16>(data.constData() + offset + 0x18) == 2;
    return data.mid(offset + kContent, size);
}

CbgDecoder::Decoded decode(const QByteArray &content)
{
    CbgDecoder::Decoded decoded;
    Reader reader{content};
    if (reader.atEnd())
        return decoded;
    const quint16 first = reader.word();
    if (first == kSetUp) {
        decoded.error = Text::tr("Games from a set-up position are not read yet.");
        return decoded;
    }
    if (first != kMoves) {
        decoded.error = Text::tr("The move record does not start with its moves.");
        return decoded;
    }
    QList<MoveRecord> moves;
    decoded.error = reader.readLine(moves, decoded.variations, true);
    for (const MoveRecord &move : std::as_const(moves))
        decoded.uciMoves << move.uci;
    return decoded;
}

} // namespace Cbg2Decoder
