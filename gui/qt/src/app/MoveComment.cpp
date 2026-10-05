#include "MoveComment.h"

#include "ChessPosition.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QStringList>

namespace MoveComment {

namespace {

QString pathKey(const QList<int> &path)
{
    QStringList parts;
    for (const int index : path)
        parts << QString::number(index);
    return parts.join(QLatin1Char('/'));
}

void collect(const QList<MoveRecord> &moves, const QList<Variation> &variations, const QString &startComment,
             QList<int> &path, QJsonObject &root)
{
    QJsonObject line;
    if (!startComment.isEmpty())
        line.insert(QStringLiteral("0"), startComment);
    for (qsizetype i = 0; i < moves.size(); ++i) {
        if (!moves.at(i).comment.isEmpty())
            line.insert(QString::number(i + 1), moves.at(i).comment);
    }
    if (!line.isEmpty())
        root.insert(pathKey(path), line);
    for (qsizetype v = 0; v < variations.size(); ++v) {
        const Variation &variation = variations.at(v);
        path << int(v);
        collect(variation.moves, variation.variations, variation.startComment, path, root);
        path.removeLast();
    }
}

/// The moves, variations and comment before the first move of the line `path`.
struct Line {
    QList<MoveRecord> *moves = nullptr;
    QString *startComment = nullptr;
};

template <typename Game>
Line lineOf(Game &game, const QList<int> &path)
{
    auto *moves = const_cast<QList<MoveRecord> *>(&game.moves);
    auto *variations = const_cast<QList<Variation> *>(&game.variations);
    auto *startComment = const_cast<QString *>(&game.startComment);
    for (const int index : path) {
        if (index < 0 || index >= variations->size())
            return {};
        Variation &variation = (*variations)[index];
        moves = &variation.moves;
        variations = &variation.variations;
        startComment = &variation.startComment;
    }
    return {moves, startComment};
}

bool anyComment(const QList<MoveRecord> &moves, const QList<Variation> &variations)
{
    for (const MoveRecord &move : moves) {
        if (!move.comment.isEmpty())
            return true;
    }
    for (const Variation &variation : variations) {
        if (!variation.startComment.isEmpty() || anyComment(variation.moves, variation.variations))
            return true;
    }
    return false;
}

} // namespace

QString displayText(const QString &comment)
{
    static const QRegularExpression command(QStringLiteral(R"(\[%[^\]]*\])"));
    static const QRegularExpression spaces(QStringLiteral(R"(\s+)"));
    QString text = comment;
    text.remove(command);
    return text.replace(spaces, QStringLiteral(" ")).trimmed();
}

QString forPgn(const QString &comment)
{
    QString text = comment;
    return text.replace(QLatin1Char('}'), QLatin1Char(')')).trimmed();
}

QString joined(const QString &first, const QString &second)
{
    if (first.isEmpty())
        return second;
    if (second.isEmpty())
        return first;
    return first + QLatin1Char(' ') + second;
}

QString toJson(const GameRecord &game)
{
    QJsonObject root;
    QList<int> path;
    collect(game.moves, game.variations, game.startComment, path, root);
    // Empty, never null: the column is NOT NULL.
    return root.isEmpty() ? QStringLiteral("") : QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Compact));
}

void fromJson(GameRecord &game, const QString &json)
{
    if (json.trimmed().isEmpty())
        return;
    const QJsonObject root = QJsonDocument::fromJson(json.toUtf8()).object();
    for (auto it = root.constBegin(); it != root.constEnd(); ++it) {
        // Follow the path down to its line.
        QList<MoveRecord> *moves = &game.moves;
        QList<Variation> *variations = &game.variations;
        QString *startComment = &game.startComment;
        bool found = true;
        for (const QString &part : it.key().split(QLatin1Char('/'), Qt::SkipEmptyParts)) {
            bool number = false;
            const int index = part.toInt(&number);
            if (!number || index < 0 || index >= variations->size()) {
                found = false;
                break;
            }
            Variation &variation = (*variations)[index];
            moves = &variation.moves;
            variations = &variation.variations;
            startComment = &variation.startComment;
        }
        if (!found)
            continue;
        const QJsonObject line = it.value().toObject();
        for (auto entry = line.constBegin(); entry != line.constEnd(); ++entry) {
            const int ply = entry.key().toInt();
            const QString text = entry.value().toString();
            if (ply == 0)
                *startComment = text;
            else if (ply >= 1 && ply <= moves->size())
                (*moves)[ply - 1].comment = text;
        }
    }
}

bool hasComments(const GameRecord &game)
{
    return !game.startComment.isEmpty() || anyComment(game.moves, game.variations);
}

QString at(const GameRecord &game, const QList<int> &path, int index)
{
    const Line line = lineOf(game, path);
    if (!line.moves || index < 0 || index > line.moves->size())
        return {};
    return index == 0 ? *line.startComment : line.moves->at(index - 1).comment;
}

bool set(GameRecord &game, const QList<int> &path, int index, const QString &comment)
{
    const Line line = lineOf(game, path);
    if (!line.moves || index < 0 || index > line.moves->size())
        return false;
    (index == 0 ? *line.startComment : (*line.moves)[index - 1].comment) = comment;
    return true;
}

QString withText(const QString &comment, const QString &text)
{
    static const QRegularExpression command(QStringLiteral(R"(\[%[^\]]*\])"));
    QStringList parts;
    if (!text.trimmed().isEmpty())
        parts << text.trimmed();
    for (const QRegularExpressionMatch &match : command.globalMatch(comment))
        parts << match.captured();
    return parts.join(QLatin1Char(' '));
}

QList<TextMove> movesIn(const QString &text, const QList<ChessPosition> &line, int at)
{
    // A move number ("14.", "14...", "14…") and a move in SAN, a word of its own.
    static const QRegularExpression written(QStringLiteral(
        R"((?<![\w.])(?:(\d+)\s*(\.\.\.|…|\.)\s*)?)"
        R"(((?:[KQRBN][a-h]?[1-8]?x?[a-h][1-8]|[a-h](?:x[a-h])?[1-8](?:=?[QRBN])?|O-O(?:-O)?|0-0(?:-0)?)[+#]?)[!?]{0,2}(?![\w-]))"));
    QList<TextMove> found;
    if (line.isEmpty())
        return found;
    at = qBound(0, at, int(line.size()) - 1);
    // The line being read: where it started, where it is, its moves.
    std::optional<ChessPosition> position;
    int basePly = 0;
    QStringList uci;
    qsizetype lastEnd = -1;
    for (const QRegularExpressionMatch &match : written.globalMatch(text)) {
        const bool numbered = !match.captured(1).isEmpty();
        const Side side = match.captured(2) == QLatin1String(".") ? Side::White : Side::Black;
        const int number = match.captured(1).toInt();
        const auto fits = [&](const ChessPosition &where) {
            return !numbered || (where.fullMoveNumber() == number && where.sideToMove() == side);
        };
        // Only spaces since the last move: the line goes on.
        const bool goesOn = position && lastEnd >= 0
            && QStringView(text).mid(lastEnd, match.capturedStart() - lastEnd).trimmed().isEmpty();
        std::optional<ChessMove> move;
        if (goesOn && fits(*position))
            move = position->moveFromSan(match.captured(3));
        if (!move) {
            // A line of its own: from the numbered position, or as the next
            // move, or in place of the move commented.
            QList<int> starts;
            if (numbered) {
                for (int ply = 0; ply < line.size(); ++ply) {
                    if (fits(line.at(ply)))
                        starts << ply;
                }
            } else {
                starts << at;
                if (at > 0)
                    starts << at - 1;
            }
            position.reset();
            for (const int ply : starts) {
                if ((move = line.at(ply).moveFromSan(match.captured(3)))) {
                    position = line.at(ply);
                    basePly = ply;
                    uci.clear();
                    break;
                }
            }
        }
        if (!move) {
            position.reset();
            continue;
        }
        position->play(*move);
        uci << move->uci();
        lastEnd = match.capturedEnd();
        found << TextMove{match.capturedStart(), match.capturedLength(), basePly, uci};
    }
    return found;
}

} // namespace MoveComment
