#include "MoveComment.h"

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

} // namespace MoveComment
