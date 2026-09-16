#include "ChessPosition.h"

namespace {

const char kStartingFen[] = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

constexpr int kWhiteKingSide = 1;
constexpr int kWhiteQueenSide = 2;
constexpr int kBlackKingSide = 4;
constexpr int kBlackQueenSide = 8;

constexpr int kKnightSteps[8][2] = {{1, 2}, {2, 1}, {2, -1}, {1, -2}, {-1, -2}, {-2, -1}, {-2, 1}, {-1, 2}};
constexpr int kKingSteps[8][2] = {{1, 0}, {1, 1}, {0, 1}, {-1, 1}, {-1, 0}, {-1, -1}, {0, -1}, {1, -1}};
constexpr int kRookDirections[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
constexpr int kBishopDirections[4][2] = {{1, 1}, {1, -1}, {-1, 1}, {-1, -1}};

/// Square `fileStep` files and `rankStep` ranks away, or -1 off the board.
int offset(int square, int fileStep, int rankStep)
{
    const int file = square % 8 + fileStep;
    const int rank = square / 8 + rankStep;
    if (file < 0 || file > 7 || rank < 0 || rank > 7)
        return -1;
    return rank * 8 + file;
}

Side opposite(Side side)
{
    return side == Side::White ? Side::Black : Side::White;
}

PieceType typeFromLetter(QChar c)
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

QChar letterFor(PieceType type)
{
    static const char letters[] = " PNBRQK";
    return QLatin1Char(letters[int(type)]);
}

/// Castling rights kept when a move starts or ends on `square`.
int castlingMask(int square)
{
    switch (square) {
    case 0: return ~kWhiteQueenSide;                      // a1
    case 4: return ~(kWhiteKingSide | kWhiteQueenSide);   // e1
    case 7: return ~kWhiteKingSide;                       // h1
    case 56: return ~kBlackQueenSide;                     // a8
    case 60: return ~(kBlackKingSide | kBlackQueenSide);  // e8
    case 63: return ~kBlackKingSide;                      // h8
    default: return ~0;
    }
}

} // namespace

QString figurineSan(const QString &san)
{
    const auto figurine = [](QChar letter) -> QChar {
        switch (letter.unicode()) {
        case 'K': return QChar(0x2654);
        case 'Q': return QChar(0x2655);
        case 'R': return QChar(0x2656);
        case 'B': return QChar(0x2657);
        case 'N': return QChar(0x2658);
        default: return letter;
        }
    };
    QString text = san;
    if (!text.isEmpty())
        text[0] = figurine(text.at(0));
    if (const qsizetype promotion = text.indexOf(QLatin1Char('=')); promotion >= 0 && promotion + 1 < text.size())
        text[promotion + 1] = figurine(text.at(promotion + 1));
    return text;
}

QString ChessMove::uci() const
{
    QString text = BoardState::squareName(from) + BoardState::squareName(to);
    if (promotion != PieceType::None)
        text += letterFor(promotion).toLower();
    return text;
}

ChessPosition ChessPosition::startingPosition()
{
    return *fromFen(QString::fromLatin1(kStartingFen));
}

std::optional<ChessPosition> ChessPosition::fromFen(const QString &fen)
{
    const QStringList fields = fen.simplified().split(QLatin1Char(' '));
    const QStringList ranks = fields.value(0).split(QLatin1Char('/'));
    if (ranks.size() != 8)
        return std::nullopt;

    ChessPosition position;
    for (int i = 0; i < 8; ++i) {
        const int rank = 7 - i;
        int file = 0;
        for (QChar c : ranks.at(i)) {
            if (c.isDigit()) {
                file += c.digitValue();
            } else {
                const PieceType type = typeFromLetter(c);
                if (type == PieceType::None || file >= 8)
                    return std::nullopt;
                if (type == PieceType::Pawn && (rank == 0 || rank == 7))
                    return std::nullopt;
                position.m_squares[rank * 8 + file] = {type, c.isUpper() ? Side::White : Side::Black};
                ++file;
            }
        }
        if (file != 8)
            return std::nullopt;
    }

    const QString side = fields.value(1, QStringLiteral("w"));
    if (side != QLatin1String("w") && side != QLatin1String("b"))
        return std::nullopt;
    position.m_sideToMove = side == QLatin1String("w") ? Side::White : Side::Black;

    const QString castling = fields.value(2, QStringLiteral("-"));
    for (QChar c : castling) {
        switch (c.unicode()) {
        case 'K': position.m_castling |= kWhiteKingSide; break;
        case 'Q': position.m_castling |= kWhiteQueenSide; break;
        case 'k': position.m_castling |= kBlackKingSide; break;
        case 'q': position.m_castling |= kBlackQueenSide; break;
        case '-': break;
        default: return std::nullopt;
        }
    }
    // Drop rights the placement contradicts, so castling never moves a ghost rook.
    const auto has = [&](int square, PieceType type, Side side) {
        return position.m_squares[square] == Piece{type, side};
    };
    if (!has(4, PieceType::King, Side::White))
        position.m_castling &= ~(kWhiteKingSide | kWhiteQueenSide);
    if (!has(7, PieceType::Rook, Side::White))
        position.m_castling &= ~kWhiteKingSide;
    if (!has(0, PieceType::Rook, Side::White))
        position.m_castling &= ~kWhiteQueenSide;
    if (!has(60, PieceType::King, Side::Black))
        position.m_castling &= ~(kBlackKingSide | kBlackQueenSide);
    if (!has(63, PieceType::Rook, Side::Black))
        position.m_castling &= ~kBlackKingSide;
    if (!has(56, PieceType::Rook, Side::Black))
        position.m_castling &= ~kBlackQueenSide;

    const QString enPassant = fields.value(3, QStringLiteral("-"));
    if (enPassant != QLatin1String("-")) {
        const int square = BoardState::squareFromName(enPassant);
        const int expectedRank = position.m_sideToMove == Side::White ? 5 : 2;
        if (square < 0 || square / 8 != expectedRank)
            return std::nullopt;
        position.m_enPassant = square;
    }

    bool ok = true;
    position.m_halfMoveClock = qMax(0, fields.value(4, QStringLiteral("0")).toInt(&ok));
    if (!ok)
        return std::nullopt;
    position.m_fullMove = qMax(1, fields.value(5, QStringLiteral("1")).toInt(&ok));
    if (!ok)
        return std::nullopt;

    int whiteKings = 0;
    int blackKings = 0;
    for (const Piece &piece : position.m_squares) {
        if (piece.type == PieceType::King)
            ++(piece.side == Side::White ? whiteKings : blackKings);
    }
    if (whiteKings != 1 || blackKings != 1)
        return std::nullopt;
    const Side justMoved = opposite(position.m_sideToMove);
    if (position.isAttacked(position.kingSquare(justMoved), position.m_sideToMove))
        return std::nullopt;
    return position;
}

QString ChessPosition::positionKey() const
{
    QString text;
    for (int rank = 7; rank >= 0; --rank) {
        int empty = 0;
        for (int file = 0; file < 8; ++file) {
            const Piece piece = m_squares[rank * 8 + file];
            if (piece.isNull()) {
                ++empty;
                continue;
            }
            if (empty > 0)
                text += QString::number(empty);
            empty = 0;
            const QChar letter = letterFor(piece.type);
            text += piece.side == Side::White ? letter : letter.toLower();
        }
        if (empty > 0)
            text += QString::number(empty);
        if (rank > 0)
            text += QLatin1Char('/');
    }

    text += m_sideToMove == Side::White ? QStringLiteral(" w ") : QStringLiteral(" b ");
    QString castling;
    if (m_castling & kWhiteKingSide)
        castling += QLatin1Char('K');
    if (m_castling & kWhiteQueenSide)
        castling += QLatin1Char('Q');
    if (m_castling & kBlackKingSide)
        castling += QLatin1Char('k');
    if (m_castling & kBlackQueenSide)
        castling += QLatin1Char('q');
    text += castling.isEmpty() ? QStringLiteral("-") : castling;
    text += QLatin1Char(' ');
    text += m_enPassant >= 0 ? BoardState::squareName(m_enPassant) : QStringLiteral("-");
    return text;
}

QString ChessPosition::fen() const
{
    return QStringLiteral("%1 %2 %3").arg(positionKey()).arg(m_halfMoveClock).arg(m_fullMove);
}

BoardState ChessPosition::boardState() const
{
    return *BoardState::fromFen(fen());
}

int ChessPosition::kingSquare(Side side) const
{
    for (int square = 0; square < 64; ++square) {
        if (m_squares[square] == Piece{PieceType::King, side})
            return square;
    }
    return -1;
}

bool ChessPosition::isAttacked(int square, Side by) const
{
    const auto holds = [&](int target, PieceType type) {
        return target >= 0 && m_squares[target] == Piece{type, by};
    };

    // Pawns of `by` attack diagonally forward, so they sit one rank behind.
    const int pawnRank = by == Side::White ? -1 : 1;
    if (holds(offset(square, -1, pawnRank), PieceType::Pawn) || holds(offset(square, 1, pawnRank), PieceType::Pawn))
        return true;
    for (const auto &step : kKnightSteps) {
        if (holds(offset(square, step[0], step[1]), PieceType::Knight))
            return true;
    }
    for (const auto &step : kKingSteps) {
        if (holds(offset(square, step[0], step[1]), PieceType::King))
            return true;
    }
    const auto slides = [&](const int directions[4][2], PieceType slider) {
        for (int d = 0; d < 4; ++d) {
            for (int target = offset(square, directions[d][0], directions[d][1]); target >= 0;
                 target = offset(target, directions[d][0], directions[d][1])) {
                const Piece piece = m_squares[target];
                if (piece.isNull())
                    continue;
                if (piece.side == by && (piece.type == slider || piece.type == PieceType::Queen))
                    return true;
                break;
            }
        }
        return false;
    };
    return slides(kRookDirections, PieceType::Rook) || slides(kBishopDirections, PieceType::Bishop);
}

QList<int> ChessPosition::attackers(int square, Side side) const
{
    QList<int> result;
    const int file = square % 8;
    const int rank = square / 8;
    for (int from = 0; from < 64; ++from) {
        const Piece piece = m_squares[from];
        if (piece.isNull() || piece.side != side || from == square)
            continue;
        const int fileDelta = file - from % 8;
        const int rankDelta = rank - from / 8;
        const int fileDistance = qAbs(fileDelta);
        const int rankDistance = qAbs(rankDelta);

        bool attacks = false;
        switch (piece.type) {
        case PieceType::Pawn:
            attacks = fileDistance == 1 && rankDelta == (side == Side::White ? 1 : -1);
            break;
        case PieceType::Knight:
            attacks = (fileDistance == 1 && rankDistance == 2) || (fileDistance == 2 && rankDistance == 1);
            break;
        case PieceType::King:
            attacks = qMax(fileDistance, rankDistance) == 1;
            break;
        case PieceType::Bishop:
        case PieceType::Rook:
        case PieceType::Queen: {
            const bool straight = fileDelta == 0 || rankDelta == 0;
            const bool diagonal = fileDistance == rankDistance;
            if ((piece.type == PieceType::Bishop && !diagonal) || (piece.type == PieceType::Rook && !straight)
                || (!straight && !diagonal))
                break;
            const int fileStep = (fileDelta > 0) - (fileDelta < 0);
            const int rankStep = (rankDelta > 0) - (rankDelta < 0);
            attacks = true;
            for (int between = offset(from, fileStep, rankStep); between != square;
                 between = offset(between, fileStep, rankStep)) {
                if (!m_squares[between].isNull()) {
                    attacks = false;
                    break;
                }
            }
            break;
        }
        case PieceType::None:
            break;
        }
        if (attacks)
            result << from;
    }
    return result;
}

bool ChessPosition::inCheck() const
{
    return isAttacked(kingSquare(m_sideToMove), opposite(m_sideToMove));
}

bool ChessPosition::isCheckmate() const
{
    return inCheck() && legalMoves().isEmpty();
}

bool ChessPosition::isStalemate() const
{
    return !inCheck() && legalMoves().isEmpty();
}

void ChessPosition::generatePseudoLegal(QList<ChessMove> &moves) const
{
    const Side us = m_sideToMove;
    const Side them = opposite(us);

    for (int from = 0; from < 64; ++from) {
        const Piece piece = m_squares[from];
        if (piece.isNull() || piece.side != us)
            continue;

        const auto addStep = [&](int to) {
            if (to >= 0 && (m_squares[to].isNull() || m_squares[to].side == them))
                moves << ChessMove{from, to};
        };
        const auto addSlides = [&](const int directions[4][2]) {
            for (int d = 0; d < 4; ++d) {
                for (int to = offset(from, directions[d][0], directions[d][1]); to >= 0;
                     to = offset(to, directions[d][0], directions[d][1])) {
                    if (m_squares[to].isNull()) {
                        moves << ChessMove{from, to};
                        continue;
                    }
                    if (m_squares[to].side == them)
                        moves << ChessMove{from, to};
                    break;
                }
            }
        };

        switch (piece.type) {
        case PieceType::Pawn: {
            const int forward = us == Side::White ? 1 : -1;
            const int startRank = us == Side::White ? 1 : 6;
            const int lastRank = us == Side::White ? 7 : 0;
            const auto addPawnMove = [&](int to) {
                if (to / 8 == lastRank) {
                    for (PieceType promotion : {PieceType::Queen, PieceType::Rook, PieceType::Bishop, PieceType::Knight})
                        moves << ChessMove{from, to, promotion};
                } else {
                    moves << ChessMove{from, to};
                }
            };
            const int one = offset(from, 0, forward);
            if (one >= 0 && m_squares[one].isNull()) {
                addPawnMove(one);
                const int two = offset(from, 0, 2 * forward);
                if (from / 8 == startRank && m_squares[two].isNull())
                    moves << ChessMove{from, two};
            }
            for (int fileStep : {-1, 1}) {
                const int to = offset(from, fileStep, forward);
                if (to < 0)
                    continue;
                if (!m_squares[to].isNull() && m_squares[to].side == them)
                    addPawnMove(to);
                else if (to == m_enPassant)
                    moves << ChessMove{from, to};
            }
            break;
        }
        case PieceType::Knight:
            for (const auto &step : kKnightSteps)
                addStep(offset(from, step[0], step[1]));
            break;
        case PieceType::Bishop:
            addSlides(kBishopDirections);
            break;
        case PieceType::Rook:
            addSlides(kRookDirections);
            break;
        case PieceType::Queen:
            addSlides(kBishopDirections);
            addSlides(kRookDirections);
            break;
        case PieceType::King: {
            for (const auto &step : kKingSteps)
                addStep(offset(from, step[0], step[1]));
            const int home = us == Side::White ? 4 : 60;
            if (from != home || isAttacked(home, them))
                break;
            const int kingSide = us == Side::White ? kWhiteKingSide : kBlackKingSide;
            const int queenSide = us == Side::White ? kWhiteQueenSide : kBlackQueenSide;
            if ((m_castling & kingSide) && m_squares[home + 1].isNull() && m_squares[home + 2].isNull()
                && !isAttacked(home + 1, them) && !isAttacked(home + 2, them))
                moves << ChessMove{from, home + 2};
            if ((m_castling & queenSide) && m_squares[home - 1].isNull() && m_squares[home - 2].isNull()
                && m_squares[home - 3].isNull() && !isAttacked(home - 1, them) && !isAttacked(home - 2, them))
                moves << ChessMove{from, home - 2};
            break;
        }
        case PieceType::None:
            break;
        }
    }
}

bool ChessPosition::leavesKingSafe(const ChessMove &move) const
{
    ChessPosition next = *this;
    next.play(move);
    return !next.isAttacked(next.kingSquare(m_sideToMove), next.m_sideToMove);
}

QList<ChessMove> ChessPosition::legalMoves() const
{
    QList<ChessMove> moves;
    moves.reserve(48);
    generatePseudoLegal(moves);
    moves.removeIf([this](const ChessMove &move) { return !leavesKingSafe(move); });
    return moves;
}

bool ChessPosition::isLegal(const ChessMove &move) const
{
    if (move.from < 0 || move.from > 63 || move.to < 0 || move.to > 63)
        return false;
    return legalMoves().contains(move);
}

std::optional<ChessMove> ChessPosition::moveFromUci(QStringView uci) const
{
    if (uci.size() != 4 && uci.size() != 5)
        return std::nullopt;
    ChessMove move{BoardState::squareFromName(uci.mid(0, 2)), BoardState::squareFromName(uci.mid(2, 2))};
    if (uci.size() == 5) {
        move.promotion = typeFromLetter(uci.at(4));
        if (move.promotion == PieceType::None)
            return std::nullopt;
    }
    if (!isLegal(move))
        return std::nullopt;
    return move;
}

std::optional<ChessMove> ChessPosition::moveFromSan(QStringView san) const
{
    QString text;
    for (QChar c : san) {
        if (!QStringView(u"+#!?x=").contains(c))
            text += c;
    }
    if (text.endsWith(QLatin1String("e.p.")))
        text.chop(4);
    text.replace(QLatin1Char('0'), QLatin1Char('O'));

    const int kingHome = m_sideToMove == Side::White ? 4 : 60;
    const bool longCastle = text == QLatin1String("O-O-O");
    if (longCastle || text == QLatin1String("O-O")) {
        const ChessMove castle{kingHome, kingHome + (longCastle ? -2 : 2)};
        if (m_squares[kingHome].type == PieceType::King && isLegal(castle))
            return castle;
        return std::nullopt;
    }

    PieceType promotion = PieceType::None;
    if (text.size() >= 3 && QStringView(u"QRBN").contains(text.back()) && text.at(text.size() - 2).isDigit()) {
        promotion = typeFromLetter(text.back());
        text.chop(1);
    }
    if (text.size() < 2)
        return std::nullopt;
    const int target = BoardState::squareFromName(QStringView(text).right(2));
    if (target < 0)
        return std::nullopt;

    PieceType type = PieceType::Pawn;
    QStringView disambiguation = QStringView(text).left(text.size() - 2);
    if (!disambiguation.isEmpty() && QStringView(u"KQRBN").contains(disambiguation.front())) {
        type = typeFromLetter(disambiguation.front());
        disambiguation = disambiguation.mid(1);
    }

    std::optional<ChessMove> found;
    for (const ChessMove &move : legalMoves()) {
        if (move.to != target || m_squares[move.from].type != type)
            continue;
        if (move.promotion != promotion)
            continue;
        bool matches = true;
        for (QChar c : disambiguation) {
            if (c >= QLatin1Char('a') && c <= QLatin1Char('h'))
                matches = matches && move.from % 8 == c.unicode() - 'a';
            else if (c >= QLatin1Char('1') && c <= QLatin1Char('8'))
                matches = matches && move.from / 8 == c.unicode() - '1';
            else
                matches = false;
        }
        if (!matches)
            continue;
        if (found)
            return std::nullopt; // Ambiguous.
        found = move;
    }
    return found;
}

Piece ChessPosition::capturedPiece(const ChessMove &move) const
{
    const Piece mover = m_squares[move.from];
    if (mover.type == PieceType::Pawn && move.to == m_enPassant && m_squares[move.to].isNull())
        return {PieceType::Pawn, opposite(mover.side)};
    return m_squares[move.to];
}

void ChessPosition::play(const ChessMove &move)
{
    Piece piece = m_squares[move.from];
    const bool capture = !m_squares[move.to].isNull();
    const int fileDelta = move.to % 8 - move.from % 8;

    if (piece.type == PieceType::Pawn && fileDelta != 0 && !capture)
        m_squares[(move.from / 8) * 8 + move.to % 8] = {}; // En passant.

    if (piece.type == PieceType::King && (fileDelta == 2 || fileDelta == -2)) {
        const int rank = move.from / 8;
        const int rookFrom = rank * 8 + (fileDelta > 0 ? 7 : 0);
        const int rookTo = rank * 8 + (fileDelta > 0 ? 5 : 3);
        m_squares[rookTo] = m_squares[rookFrom];
        m_squares[rookFrom] = {};
    }

    m_halfMoveClock = piece.type == PieceType::Pawn || capture ? 0 : m_halfMoveClock + 1;
    m_castling &= castlingMask(move.from) & castlingMask(move.to);

    m_enPassant = -1;
    if (piece.type == PieceType::Pawn && qAbs(move.to - move.from) == 16) {
        // Only record the square when a pawn can actually take, like most tools.
        const Piece enemyPawn{PieceType::Pawn, opposite(piece.side)};
        const int left = offset(move.to, -1, 0);
        const int right = offset(move.to, 1, 0);
        if ((left >= 0 && m_squares[left] == enemyPawn) || (right >= 0 && m_squares[right] == enemyPawn))
            m_enPassant = (move.from + move.to) / 2;
    }

    if (move.promotion != PieceType::None)
        piece.type = move.promotion;
    m_squares[move.to] = piece;
    m_squares[move.from] = {};

    if (m_sideToMove == Side::Black)
        ++m_fullMove;
    m_sideToMove = opposite(m_sideToMove);
}

QString ChessPosition::san(const ChessMove &move) const
{
    const Piece piece = m_squares[move.from];
    QString text;

    if (piece.type == PieceType::King && qAbs(move.to % 8 - move.from % 8) == 2) {
        text = move.to % 8 == 6 ? QStringLiteral("O-O") : QStringLiteral("O-O-O");
    } else {
        const bool capture = !capturedPiece(move).isNull();
        if (piece.type == PieceType::Pawn) {
            if (capture)
                text += QLatin1Char('a' + move.from % 8);
        } else {
            text += letterFor(piece.type);
            bool ambiguous = false;
            bool sameFile = false;
            bool sameRank = false;
            for (const ChessMove &other : legalMoves()) {
                if (other.to != move.to || other.from == move.from || m_squares[other.from].type != piece.type)
                    continue;
                ambiguous = true;
                sameFile |= other.from % 8 == move.from % 8;
                sameRank |= other.from / 8 == move.from / 8;
            }
            if (ambiguous) {
                if (!sameFile)
                    text += QLatin1Char('a' + move.from % 8);
                else if (!sameRank)
                    text += QLatin1Char('1' + move.from / 8);
                else
                    text += BoardState::squareName(move.from);
            }
        }
        if (capture)
            text += QLatin1Char('x');
        text += BoardState::squareName(move.to);
        if (move.promotion != PieceType::None)
            text += QLatin1Char('=') + letterFor(move.promotion);
    }

    ChessPosition next = *this;
    next.play(move);
    if (next.inCheck())
        text += next.legalMoves().isEmpty() ? QLatin1Char('#') : QLatin1Char('+');
    return text;
}

int ChessPosition::pieceValue(PieceType type)
{
    switch (type) {
    case PieceType::Pawn: return 100;
    case PieceType::Knight: return 300;
    case PieceType::Bishop: return 300;
    case PieceType::Rook: return 500;
    case PieceType::Queen: return 900;
    case PieceType::King:
    case PieceType::None: break;
    }
    return 0;
}

int ChessPosition::material() const
{
    int total = 0;
    for (const Piece &piece : m_squares)
        total += (piece.side == Side::White ? 1 : -1) * pieceValue(piece.type);
    return total;
}

QString ChessPosition::moveNumberText() const
{
    return m_sideToMove == Side::White ? QStringLiteral("%1.").arg(m_fullMove)
                                       : QStringLiteral("%1…").arg(m_fullMove);
}

QString ChessPosition::lineText(const QStringList &uciMoves, int maxPlies, SanStyle style) const
{
    QStringList parts;
    ChessPosition position = *this;
    for (qsizetype i = 0; i < uciMoves.size() && (maxPlies < 0 || i < maxPlies); ++i) {
        const std::optional<ChessMove> move = position.moveFromUci(uciMoves.at(i));
        if (!move)
            break;
        const QString san = style == SanStyle::Figurines ? figurineSan(position.san(*move)) : position.san(*move);
        if (i == 0 || position.sideToMove() == Side::White)
            parts << position.moveNumberText() + san;
        else
            parts << san;
        position.play(*move);
    }
    return parts.join(QLatin1Char(' '));
}
