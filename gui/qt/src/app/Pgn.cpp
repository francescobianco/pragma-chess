#include "Pgn.h"

#include "ChessPosition.h"
#include "MoveAnnotation.h"
#include "MoveComment.h"

#include <QObject>
#include <QRegularExpression>
#include <QStringList>

namespace {

QString tagValue(const QString &value, const QString &unknown = QStringLiteral("?"))
{
    const QString text = value.trimmed().isEmpty() ? unknown : value.trimmed();
    QString escaped = text;
    escaped.replace(QLatin1Char('\\'), QLatin1String("\\\\")).replace(QLatin1Char('"'), QLatin1String("\\\""));
    return escaped;
}

QString tag(const char *name, const QString &value)
{
    return QStringLiteral("[%1 \"%2\"]\n").arg(QLatin1String(name), value);
}

QString wrap(const QString &text, int width)
{
    QString result;
    int lineLength = 0;
    for (const QString &token : text.split(QLatin1Char(' '), Qt::SkipEmptyParts)) {
        if (lineLength > 0 && lineLength + 1 + token.size() > width) {
            result += QLatin1Char('\n');
            lineLength = 0;
        } else if (lineLength > 0) {
            result += QLatin1Char(' ');
            ++lineLength;
        }
        result += token;
        lineLength += int(token.size());
    }
    return result;
}

} // namespace

namespace Pgn {

namespace {

/// A comment between braces, or nothing for an empty one.
void writeComment(const QString &comment, QStringList &parts)
{
    const QString text = MoveComment::forPgn(comment);
    if (!text.isEmpty())
        parts << QLatin1Char('{') + text + QLatin1Char('}');
}

/// Writes `moves` from `position`, numbered, with the variations off them
/// and the comments when `withVariations` (the whole game): each variation
/// in parentheses after the move it replaces, and the move after a
/// variation or a comment numbered again.
void writeLine(const QList<MoveRecord> &moves, const QList<Variation> &variations, ChessPosition position,
               int maxPlies, bool withVariations, QStringList &parts, const QString &startComment = QString())
{
    bool numbered = false;
    if (withVariations)
        writeComment(startComment, parts);
    for (qsizetype i = 0; i < moves.size() && (maxPlies < 0 || i < maxPlies); ++i) {
        const MoveRecord &record = moves.at(i);
        std::optional<ChessMove> move = position.moveFromUci(record.uci);
        if (!move && !record.san.isEmpty())
            move = position.moveFromSan(record.san);
        if (!move)
            break;
        const QString san = position.san(*move) + MoveAnnotation::pgnSuffix(record.nags);
        parts << (!numbered || position.sideToMove() == Side::White ? position.moveNumberText() + san : san);
        numbered = true;
        const ChessPosition before = position;
        position.play(*move);
        if (!withVariations)
            continue;
        if (!MoveComment::forPgn(record.comment).isEmpty()) {
            writeComment(record.comment, parts);
            numbered = false; // "3…Nc6" after the comment.
        }
        for (const Variation &variation : variations) {
            if (variation.atPly != i + 1)
                continue;
            parts << QStringLiteral("(");
            writeLine(variation.moves, variation.variations, before, -1, true, parts, variation.startComment);
            parts << QStringLiteral(")");
            numbered = false; // "3…Nc6" after the parenthesis.
        }
    }
}

/// Marks a token that is a comment's text rather than a move.
constexpr QChar kCommentMark = QChar(0x1);

/// The tokens of a movetext: tags and line comments gone, each comment in
/// braces one token (kCommentMark and its text), the parentheses of
/// variations tokens of their own.
QStringList tokenize(const QString &text)
{
    QString movetext;
    QStringList comments;
    QString comment;
    bool inComment = false;
    bool inTag = false;
    bool inLineComment = false;
    for (QChar c : text) {
        if (inLineComment) {
            inLineComment = c != QLatin1Char('\n');
            continue;
        }
        if (inComment) {
            if (c == QLatin1Char('}')) {
                inComment = false;
                // The comment stands in the movetext as a placeholder word,
                // its text kept aside: the split below must not cut it.
                movetext += QStringLiteral(" %1%2 ").arg(kCommentMark).arg(comments.size());
                comments << comment.simplified();
                comment.clear();
            } else {
                comment += c;
            }
            continue;
        }
        if (inTag) {
            inTag = c != QLatin1Char(']');
            continue;
        }
        if (c == QLatin1Char('{')) {
            inComment = true;
        } else if (c == QLatin1Char('[')) {
            inTag = true;
        } else if (c == QLatin1Char(';')) {
            inLineComment = true;
        } else if (c == QLatin1Char('(') || c == QLatin1Char(')')) {
            movetext += QLatin1Char(' ') + c + QLatin1Char(' ');
        } else {
            movetext += c;
        }
    }
    static const QRegularExpression separators(QStringLiteral(R"([\s,]+)"));
    QStringList tokens = movetext.split(separators, Qt::SkipEmptyParts);
    for (QString &token : tokens) {
        if (token.startsWith(kCommentMark))
            token = kCommentMark + comments.value(token.mid(1).toInt());
    }
    return tokens;
}

/// Reads one line of tokens from `i`, to its closing parenthesis or the end,
/// with the variations that open after its moves. Returns false with
/// `errorMessage` on an illegal move when `strict`; otherwise the line stops
/// there and the rest of it is skipped.
bool readLine(const QStringList &tokens, qsizetype &i, const ChessPosition &start, QList<MoveRecord> &moves,
              QList<Variation> &variations, QString &startComment, bool strict, QString *errorMessage)
{
    static const QRegularExpression moveNumber(QStringLiteral(R"(^\d+\s*(\.+|…)\s*)"));
    static const QRegularExpression result(QStringLiteral(R"(^(1-0|0-1|1/2-1/2|½-½|\*)$)"));
    QList<ChessPosition> positions{start};
    bool stopped = false;
    while (i < tokens.size()) {
        QString token = tokens.at(i++);
        if (token == QLatin1String(")"))
            return true;
        if (token == QLatin1String("(")) {
            // An alternative to the last move of this line, from before it.
            Variation variation;
            variation.atPly = int(moves.size());
            const ChessPosition &before = moves.isEmpty() ? positions.first() : positions.at(moves.size() - 1);
            if (!readLine(tokens, i, before, variation.moves, variation.variations, variation.startComment, false,
                          errorMessage))
                return false;
            if (!stopped && variation.atPly >= 1 && !variation.moves.isEmpty())
                variations << variation;
            continue;
        }
        if (stopped)
            continue; // The rest of a cut variation, up to its parenthesis.
        if (token.startsWith(kCommentMark)) {
            // After a move it is that move's; before any, the line's.
            QString &into = moves.isEmpty() ? startComment : moves.last().comment;
            into = MoveComment::joined(into, token.mid(1));
            continue;
        }
        token.remove(moveNumber);
        if (token.startsWith(QLatin1Char('$')) && !moves.isEmpty())
            MoveAnnotation::split(token, &moves.last().nags); // A NAG belongs to the move before it.
        if (token.isEmpty() || token.startsWith(QLatin1Char('$')) || result.match(token).hasMatch())
            continue;
        if (token.front().isDigit() && token.back() == QLatin1Char('.'))
            continue;
        if (token.count(QLatin1Char('.')) + token.count(QChar(0x2026)) == token.size())
            continue; // "..." or "…" standing alone before a Black move.
        QList<int> nags;
        const QString bare = MoveAnnotation::split(token, &nags);
        ChessPosition &position = positions.last();
        std::optional<ChessMove> move = position.moveFromSan(bare);
        if (!move)
            move = position.moveFromUci(bare);
        if (!move) {
            if (!strict) {
                stopped = true;
                continue;
            }
            GameRecord sofar;
            sofar.startFen = start.fen();
            sofar.moves = moves;
            if (errorMessage)
                *errorMessage = QObject::tr("“%1” is not a legal move after %2")
                                    .arg(token, moves.isEmpty() ? QObject::tr("the start position") : moveText(sofar));
            return false;
        }
        moves << MoveRecord{position.san(*move), move->uci(), nags, {}};
        ChessPosition next = position;
        next.play(*move);
        positions << next;
    }
    return true;
}

} // namespace

QString moveText(const GameRecord &game, int plies)
{
    const std::optional<ChessPosition> start = game.startFen.isEmpty()
        ? ChessPosition::startingPosition()
        : ChessPosition::fromFen(game.startFen);
    if (!start)
        return {};
    QStringList parts;
    writeLine(game.moves, game.variations, *start, plies, plies < 0, parts, game.startComment);
    return parts.join(QLatin1Char(' '));
}

QString preview(const QString &startFen, const QStringList &sanMoves, int plyCount)
{
    const QStringList fen = startFen.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    bool whiteToMove = fen.value(1) != QLatin1String("b");
    int number = qMax(1, fen.value(5).toInt());
    QStringList parts;
    for (const QString &stored : sanMoves) {
        QList<int> nags;
        QString san = MoveAnnotation::split(stored, &nags);
        san += MoveAnnotation::symbols(nags);
        if (whiteToMove)
            parts << QString::number(number) + QLatin1Char('.') + san;
        else
            parts << (parts.isEmpty() ? QString::number(number) + QChar(0x2026) + san : san);
        if (!whiteToMove)
            ++number;
        whiteToMove = !whiteToMove;
    }
    const QString text = parts.join(QLatin1Char(' '));
    return plyCount > sanMoves.size() ? text + QChar(0x2026) : text;
}

std::optional<ParsedLine> parseLine(const QString &text, const QString &startFen, QString *errorMessage)
{
    const auto fail = [&](const QString &message) {
        if (errorMessage)
            *errorMessage = message;
        return std::nullopt;
    };

    ParsedLine line;
    line.startFen = startFen;
    static const QRegularExpression fenTag(QStringLiteral(R"re(\[\s*FEN\s+"([^"]*)"\s*\])re"));
    if (const QRegularExpressionMatch match = fenTag.match(text); match.hasMatch())
        line.startFen = match.captured(1).trimmed();
    const std::optional<ChessPosition> position = line.startFen.isEmpty() ? ChessPosition::startingPosition()
                                                                           : ChessPosition::fromFen(line.startFen);
    if (!position)
        return fail(QObject::tr("Invalid FEN: %1").arg(line.startFen));

    const QStringList tokens = tokenize(text);
    qsizetype i = 0;
    QString error;
    if (!readLine(tokens, i, *position, line.moves, line.variations, line.startComment, true, &error))
        return fail(error);
    return line;
}

QString tagsText(const QList<PgnTag> &tags)
{
    QStringList lines;
    for (const PgnTag &extra : tags)
        lines << QStringLiteral("[%1 \"%2\"]").arg(extra.name, tagValue(extra.value, QString()));
    return lines.isEmpty() ? QStringLiteral("") : lines.join(QLatin1Char('\n')); // Never null: the column is NOT NULL.
}

QList<PgnTag> tagsFromText(const QString &text)
{
    QList<PgnTag> tags;
    static const QRegularExpression tag(QStringLiteral(R"re(^\s*\[(\w+)\s+"((?:[^"\\]|\\.)*)"\s*\])re"),
                                        QRegularExpression::MultilineOption);
    for (auto it = tag.globalMatch(text); it.hasNext();) {
        const QRegularExpressionMatch match = it.next();
        QString value = match.captured(2);
        value.replace(QLatin1String("\\\""), QLatin1String("\"")).replace(QLatin1String("\\\\"), QLatin1String("\\"));
        tags << PgnTag{match.captured(1), value};
    }
    return tags;
}

QString game(const GameRecord &game, int plies)
{
    const bool complete = plies < 0 || plies >= game.moves.size();
    const QString result = complete && !game.result.trimmed().isEmpty() ? game.result.trimmed()
                                                                        : QStringLiteral("*");
    QString pgn;
    pgn += tag("Event", tagValue(game.event));
    pgn += tag("Site", tagValue(game.site));
    pgn += tag("Date", tagValue(game.date, QStringLiteral("????.??.??")));
    pgn += tag("Round", tagValue(game.round));
    pgn += tag("White", tagValue(game.white));
    pgn += tag("Black", tagValue(game.black));
    pgn += tag("Result", result);
    if (game.whiteElo > 0)
        pgn += tag("WhiteElo", QString::number(game.whiteElo));
    if (game.blackElo > 0)
        pgn += tag("BlackElo", QString::number(game.blackElo));
    if (!game.eco.trimmed().isEmpty())
        pgn += tag("ECO", tagValue(game.eco));
    if (!game.tags.isEmpty())
        pgn += tagsText(game.tags) + QLatin1Char('\n');
    if (!game.startFen.isEmpty()) {
        pgn += tag("SetUp", QStringLiteral("1"));
        pgn += tag("FEN", tagValue(game.startFen));
    }
    pgn += QLatin1Char('\n');

    const QString moves = moveText(game, plies);
    pgn += wrap(moves.isEmpty() ? result : moves + QLatin1Char(' ') + result, 80);
    pgn += QLatin1Char('\n');
    return pgn;
}

} // namespace Pgn
