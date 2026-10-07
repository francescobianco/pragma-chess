#include "TrainingSets.h"

#include <QCoreApplication>

#include <algorithm>
#include <array>

namespace {

struct Text {
    Q_DECLARE_TR_FUNCTIONS(TrainingSets)
};

QString tagValue(const GameRecord &game, const QString &name)
{
    for (const PgnTag &tag : game.tags) {
        if (tag.name == name)
            return tag.value;
    }
    return {};
}

/// A side's pieces besides the king, strongest first: "QRBNP…" with each pawn.
QString piecesOf(const ChessPosition &position, Side side)
{
    QString pieces;
    for (const auto &[type, letter] : std::array<std::pair<PieceType, QChar>, 5>{
             {{PieceType::Queen, u'Q'}, {PieceType::Rook, u'R'}, {PieceType::Bishop, u'B'},
              {PieceType::Knight, u'N'}, {PieceType::Pawn, u'P'}}}) {
        for (int square = 0; square < 64; ++square) {
            const Piece piece = position.at(square);
            if (piece.type == type && piece.side == side)
                pieces += letter;
        }
    }
    return pieces;
}

int valueOf(const QString &pieces)
{
    int value = 0;
    for (const QChar letter : pieces)
        value += letter == u'Q' ? 9 : letter == u'R' ? 5 : letter == u'B' || letter == u'N' ? 3 : 1;
    return value;
}

/// "RPP" → "RP": a pawn once, however many.
QString withOnePawn(const QString &pieces)
{
    QString result = pieces;
    result.remove(u'P');
    return pieces.contains(u'P') ? result + u'P' : result;
}

QString letterName(QChar letter)
{
    switch (letter.unicode()) {
    case 'K': return Text::tr("K", "piece letter: king");
    case 'Q': return Text::tr("Q", "piece letter: queen");
    case 'R': return Text::tr("R", "piece letter: rook");
    case 'B': return Text::tr("B", "piece letter: bishop");
    case 'N': return Text::tr("N", "piece letter: knight");
    default: return Text::tr("P", "piece letter: pawn");
    }
}

/// "KRP" → "K+R+P", in the interface's letters.
QString sideName(const QString &side)
{
    QStringList letters;
    for (const QChar letter : side)
        letters << letterName(letter);
    return letters.join(QLatin1Char('+'));
}

struct Theory {
    const char *name;
    const char *fen;
    const char *result;
    const char *goal;
};

/// Checked with Stockfish: the result is what correct play gives.
const Theory kTheory[] = {
    {QT_TRANSLATE_NOOP("TrainingSets", "Mate with the queen"), "8/8/8/4k3/8/8/8/3QK3 w - - 0 1", "1-0",
     QT_TRANSLATE_NOOP("TrainingSets", "White to play and mate.")},
    {QT_TRANSLATE_NOOP("TrainingSets", "Mate with the rook"), "8/8/8/4k3/8/8/8/R3K3 w - - 0 1", "1-0",
     QT_TRANSLATE_NOOP("TrainingSets", "White to play and mate.")},
    {QT_TRANSLATE_NOOP("TrainingSets", "Mate with two bishops"), "8/8/8/4k3/8/8/8/2B1KB2 w - - 0 1", "1-0",
     QT_TRANSLATE_NOOP("TrainingSets", "White to play and mate.")},
    {QT_TRANSLATE_NOOP("TrainingSets", "Mate with bishop and knight"), "8/8/8/4k3/8/8/8/2B1KN2 w - - 0 1", "1-0",
     QT_TRANSLATE_NOOP("TrainingSets", "White to play and mate: in the corner of the bishop's colour.")},
    {QT_TRANSLATE_NOOP("TrainingSets", "King in front of the pawn"), "4k3/8/4K3/4P3/8/8/8/8 w - - 0 1", "1-0",
     QT_TRANSLATE_NOOP("TrainingSets", "White to play and win: the king on the sixth rank, in front of its pawn.")},
    {QT_TRANSLATE_NOOP("TrainingSets", "The opposition"), "8/8/8/3k4/8/3K4/3P4/8 w - - 0 1", "1/2-1/2",
     QT_TRANSLATE_NOOP("TrainingSets", "White to play: Black holds the opposition, it is a draw. Try to win, and see why not.")},
    {QT_TRANSLATE_NOOP("TrainingSets", "Defending with the opposition"), "3k4/8/8/3K4/3P4/8/8/8 b - - 0 1", "1/2-1/2",
     QT_TRANSLATE_NOOP("TrainingSets", "Black to play and draw: take the opposition.")},
    {QT_TRANSLATE_NOOP("TrainingSets", "The square of the pawn"), "7k/8/8/p7/8/8/8/5K2 w - - 0 1", "1/2-1/2",
     QT_TRANSLATE_NOOP("TrainingSets", "White to play and draw: step into the square of the pawn.")},
    {QT_TRANSLATE_NOOP("TrainingSets", "The rook pawn"), "8/8/8/8/8/k7/p7/1K6 w - - 0 1", "1/2-1/2",
     QT_TRANSLATE_NOOP("TrainingSets", "White to play and draw: the king in the corner.")},
    {QT_TRANSLATE_NOOP("TrainingSets", "The wrong bishop"), "7k/8/7K/7P/8/8/8/5B2 w - - 0 1", "1/2-1/2",
     QT_TRANSLATE_NOOP("TrainingSets", "White to play: the bishop does not control h8, it is a draw. Try to win, and see why not.")},
    {QT_TRANSLATE_NOOP("TrainingSets", "Triangulation"), "8/8/2k5/2P5/3K4/8/8/8 w - - 0 1", "1/2-1/2",
     QT_TRANSLATE_NOOP("TrainingSets", "White to play: Black keeps the opposition, it is a draw.")},
    {QT_TRANSLATE_NOOP("TrainingSets", "The outside passed pawn"), "8/5k2/8/p4p2/P4P2/8/5K2/8 w - - 0 1", "1/2-1/2",
     QT_TRANSLATE_NOOP("TrainingSets", "White to play: with best play it is a draw.")},
    {QT_TRANSLATE_NOOP("TrainingSets", "Pawn breakthrough"), "7k/ppp5/8/PPP5/8/8/8/7K w - - 0 1", "1-0",
     QT_TRANSLATE_NOOP("TrainingSets", "White to play and win: break through with a pawn.")},
    {QT_TRANSLATE_NOOP("TrainingSets", "Lucena position"), "1K1k4/1P6/8/8/8/8/r7/2R5 w - - 0 1", "1-0",
     QT_TRANSLATE_NOOP("TrainingSets", "White to play and win: build the bridge.")},
    {QT_TRANSLATE_NOOP("TrainingSets", "Philidor position"), "3k4/7R/8/3KP3/8/6r1/8/8 b - - 0 1", "1/2-1/2",
     QT_TRANSLATE_NOOP("TrainingSets", "Black to play and draw: the rook on the third rank.")},
    {QT_TRANSLATE_NOOP("TrainingSets", "Queen against a centre pawn"), "8/8/8/8/8/8/3pk3/K5Q1 w - - 0 1", "1-0",
     QT_TRANSLATE_NOOP("TrainingSets", "White to play and win: bring the king closer with checks.")},
    {QT_TRANSLATE_NOOP("TrainingSets", "Queen against a bishop pawn"), "8/8/8/8/8/1Q6/2pk4/K7 w - - 0 1", "1-0",
     QT_TRANSLATE_NOOP("TrainingSets", "White to play and win: here the king is close enough.")},
    {QT_TRANSLATE_NOOP("TrainingSets", "Rook against bishop"), "8/8/8/4k3/8/3b4/8/R3K3 w - - 0 1", "1/2-1/2",
     QT_TRANSLATE_NOOP("TrainingSets", "White to play: rook against bishop is usually a draw.")},
};

struct Theme {
    const char *id;
    const char *name;
};

const Theme kTactics[] = {
    {"pin", QT_TRANSLATE_NOOP("TrainingSets", "Pin")},
    {"skewer", QT_TRANSLATE_NOOP("TrainingSets", "Skewer")},
    {"fork", QT_TRANSLATE_NOOP("TrainingSets", "Fork")},
    {"discoveredAttack", QT_TRANSLATE_NOOP("TrainingSets", "Discovered Attack")},
    {"doubleCheck", QT_TRANSLATE_NOOP("TrainingSets", "Double Check")},
    {"deflection", QT_TRANSLATE_NOOP("TrainingSets", "Deflection")},
    {"attraction", QT_TRANSLATE_NOOP("TrainingSets", "Attraction")},
    {"xRayAttack", QT_TRANSLATE_NOOP("TrainingSets", "X-Ray Attack")},
    {"interference", QT_TRANSLATE_NOOP("TrainingSets", "Interference")},
    {"intermezzo", QT_TRANSLATE_NOOP("TrainingSets", "Intermezzo")},
    {"clearance", QT_TRANSLATE_NOOP("TrainingSets", "Clearance")},
    {"capturingDefender", QT_TRANSLATE_NOOP("TrainingSets", "Capturing the Defender")},
    {"hangingPiece", QT_TRANSLATE_NOOP("TrainingSets", "Hanging Piece")},
    {"trappedPiece", QT_TRANSLATE_NOOP("TrainingSets", "Trapped Piece")},
    {"sacrifice", QT_TRANSLATE_NOOP("TrainingSets", "Sacrifice")},
    {"backRankMate", QT_TRANSLATE_NOOP("TrainingSets", "Back-Rank Mate")},
    {"smotheredMate", QT_TRANSLATE_NOOP("TrainingSets", "Smothered Mate")},
    {"quietMove", QT_TRANSLATE_NOOP("TrainingSets", "Quiet Move")},
    {"zugzwang", QT_TRANSLATE_NOOP("TrainingSets", "Zugzwang")},
    {"promotion", QT_TRANSLATE_NOOP("TrainingSets", "Promotion")},
};

struct Family {
    const char *id;
    const char *name;
};

const Family kFamilies[] = {
    {"mate", QT_TRANSLATE_NOOP("TrainingSets", "Basic Mates")},
    {"pawn", QT_TRANSLATE_NOOP("TrainingSets", "Pawn Endings")},
    {"knight", QT_TRANSLATE_NOOP("TrainingSets", "Knight Endings")},
    {"bishop", QT_TRANSLATE_NOOP("TrainingSets", "Bishop Endings")},
    {"minor", QT_TRANSLATE_NOOP("TrainingSets", "Minor Piece Endings")},
    {"rook", QT_TRANSLATE_NOOP("TrainingSets", "Rook Endings")},
    {"rook-minor", QT_TRANSLATE_NOOP("TrainingSets", "Rook and Minor Piece")},
    {"queen", QT_TRANSLATE_NOOP("TrainingSets", "Queen Endings")},
    {"queen-other", QT_TRANSLATE_NOOP("TrainingSets", "Queen and Other Pieces")},
    {"other", QT_TRANSLATE_NOOP("TrainingSets", "Other Endings")},
};

/// Every move of `uci` played from `start`, with its SAN; nothing if one is illegal.
std::optional<QList<MoveRecord>> replay(ChessPosition position, const QStringList &uci)
{
    QList<MoveRecord> moves;
    for (const QString &text : uci) {
        const std::optional<ChessMove> move = position.moveFromUci(text);
        if (!move)
            return std::nullopt;
        moves << MoveRecord{position.san(*move), text};
        position.play(*move);
    }
    return moves;
}

} // namespace

namespace TrainingSets {

QList<GameRecord> puzzleGames(const QString &tsv)
{
    QList<GameRecord> games;
    const QStringList lines = tsv.split(QLatin1Char('\n'));
    for (const QString &line : lines) {
        if (line.trimmed().isEmpty() || line.startsWith(QLatin1Char('#')))
            continue;
        const QStringList fields = line.split(QLatin1Char('\t'));
        if (fields.size() < 6)
            continue;
        std::optional<ChessPosition> position = ChessPosition::fromFen(fields.at(1));
        QStringList moves = fields.at(2).split(QLatin1Char(' '), Qt::SkipEmptyParts);
        if (!position || moves.size() < 2)
            continue;
        // The puzzle starts once the opponent has moved.
        const std::optional<ChessMove> first = position->moveFromUci(moves.takeFirst());
        if (!first)
            continue;
        position->play(*first);
        const std::optional<QList<MoveRecord>> solution = replay(*position, moves);
        if (!solution)
            continue;
        GameRecord game;
        // One event for them all (the tree lists events), the puzzle's id as the round.
        game.event = Text::tr("lichess.org puzzles");
        game.round = fields.at(0);
        game.site = fields.at(5);
        game.result = QStringLiteral("*");
        game.startFen = position->fen();
        game.moves = *solution;
        game.tags = {{QStringLiteral("PuzzleId"), fields.at(0)},
                     {QStringLiteral("PuzzleRating"), fields.at(3)},
                     {QStringLiteral("Themes"), fields.at(4)},
                     {QStringLiteral("Annotator"), QStringLiteral("lichess.org puzzle database (CC0)")}};
        games << game;
    }
    return games;
}

QList<GameRecord> theoryEndgames()
{
    QList<GameRecord> games;
    for (const Theory &theory : kTheory) {
        GameRecord game;
        game.event = Text::tr(theory.name);
        game.result = QLatin1String(theory.result);
        game.startFen = QLatin1String(theory.fen);
        game.startComment = Text::tr(theory.goal);
        games << game;
    }
    return games;
}

QString endgameOf(const ChessPosition &position)
{
    const QString white = piecesOf(position, Side::White);
    const QString black = piecesOf(position, Side::Black);
    QString pieces = white + black;
    pieces.remove(u'P');
    if (pieces.size() > 4)
        return {};
    QString strong = QLatin1Char('K') + withOnePawn(white);
    QString weak = QLatin1Char('K') + withOnePawn(black);
    if (valueOf(black) > valueOf(white))
        std::swap(strong, weak);
    return strong + QLatin1Char('-') + weak;
}

QString endgameOf(const GameRecord &game)
{
    if (game.startFen.isEmpty())
        return {};
    const std::optional<ChessPosition> position = ChessPosition::fromFen(game.startFen);
    return position ? endgameOf(*position) : QString();
}

QString endgameFamily(const QString &endgame)
{
    const QString strong = endgame.section(QLatin1Char('-'), 0, 0).mid(1);
    const QString weak = endgame.section(QLatin1Char('-'), 1).mid(1);
    QString pieces = strong + weak;
    if ((strong.isEmpty() || weak.isEmpty()) && !pieces.contains(u'P') && !pieces.isEmpty())
        return QStringLiteral("mate");
    pieces.remove(u'P');
    const auto only = [&](const QString &letters) {
        return !pieces.isEmpty() && std::all_of(pieces.cbegin(), pieces.cend(), [&](QChar c) { return letters.contains(c); });
    };
    if (pieces.isEmpty())
        return QStringLiteral("pawn");
    if (only(QStringLiteral("R")))
        return QStringLiteral("rook");
    if (only(QStringLiteral("Q")))
        return QStringLiteral("queen");
    if (only(QStringLiteral("B")))
        return QStringLiteral("bishop");
    if (only(QStringLiteral("N")))
        return QStringLiteral("knight");
    if (only(QStringLiteral("BN")))
        return QStringLiteral("minor");
    if (only(QStringLiteral("RBN")))
        return QStringLiteral("rook-minor");
    if (pieces.contains(u'Q'))
        return QStringLiteral("queen-other");
    return QStringLiteral("other");
}

QStringList endgameFamilies()
{
    QStringList families;
    for (const Family &family : kFamilies)
        families << QLatin1String(family.id);
    return families;
}

QString endgameFamilyName(const QString &family)
{
    for (const Family &entry : kFamilies) {
        if (family == QLatin1String(entry.id))
            return Text::tr(entry.name);
    }
    return family;
}

QString endgameName(const QString &endgame)
{
    return Text::tr("%1 vs %2")
        .arg(sideName(endgame.section(QLatin1Char('-'), 0, 0)), sideName(endgame.section(QLatin1Char('-'), 1)));
}

QStringList tacticsOf(const GameRecord &game)
{
    const QStringList themes = tagValue(game, QStringLiteral("Themes")).split(QLatin1Char(' '), Qt::SkipEmptyParts);
    QStringList found;
    for (const Theme &theme : kTactics) {
        if (themes.contains(QLatin1String(theme.id)))
            found << QLatin1String(theme.id);
    }
    return found;
}

QStringList tacticThemes()
{
    QStringList themes;
    for (const Theme &theme : kTactics)
        themes << QLatin1String(theme.id);
    return themes;
}

QString tacticName(const QString &theme)
{
    for (const Theme &entry : kTactics) {
        if (theme == QLatin1String(entry.id))
            return Text::tr(entry.name);
    }
    return theme;
}

} // namespace TrainingSets
