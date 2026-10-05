#include "PgnFile.h"

#include "app/Pgn.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStringDecoder>

namespace PgnFile {

namespace {

constexpr int kIndexVersion = 1;

/// The first character of a line that is not a space, or 0.
char firstChar(const QByteArray &bytes, qint64 from, qint64 to)
{
    for (qint64 i = from; i < to; ++i) {
        const char c = bytes.at(i);
        if (c != ' ' && c != '\t' && c != '\r' && c != '\n')
            return c;
    }
    return 0;
}

bool isUidTag(const QByteArray &trimmedLine)
{
    static const QByteArray prefix = QByteArray("[") + uidTag;
    if (!trimmedLine.startsWith(prefix))
        return false;
    const char next = trimmedLine.size() > prefix.size() ? trimmedLine.at(prefix.size()) : 0;
    return next == ' ' || next == '\t' || next == '"';
}

QString uidOf(const QByteArray &entry)
{
    static const QRegularExpression tag(QStringLiteral(R"re(^\s*\[%1\s+"([^"]*)"\s*\])re").arg(QLatin1String(uidTag)),
                                        QRegularExpression::MultilineOption);
    return tag.match(QString::fromLatin1(entry)).captured(1).trimmed();
}

QString hashOf(QByteArray text)
{
    text.replace('\r', QByteArray());
    return QString::fromLatin1(QCryptographicHash::hash(text.trimmed(), QCryptographicHash::Sha1).toHex().left(16));
}

Entry entryOf(const QByteArray &bytes, qint64 from, qint64 to, bool hasTags)
{
    const QByteArray text = bytes.mid(from, to - from);
    Entry entry;
    entry.offset = from;
    entry.length = to - from;
    entry.hash = hashOf(text);
    entry.uid = uidOf(text);
    entry.isGame = hasTags;
    return entry;
}

QString unescape(QString value)
{
    value.replace(QLatin1String("\\\""), QLatin1String("\"")).replace(QLatin1String("\\\\"), QLatin1String("\\"));
    return value;
}

/// "?" is PGN's "unknown": the database leaves it empty.
QString known(const QString &value)
{
    const QString text = value.trimmed();
    return text == QLatin1String("?") || text == QLatin1String("-") ? QString() : text;
}

} // namespace

QList<Entry> scan(const QByteArray &bytes)
{
    QList<Entry> entries;
    if (bytes.isEmpty())
        return entries;
    qint64 start = 0;
    bool inMoves = false;
    bool hasTags = false;
    for (qint64 position = 0; position < bytes.size();) {
        const qint64 newline = bytes.indexOf('\n', position);
        const qint64 end = newline < 0 ? bytes.size() : newline + 1;
        const char first = firstChar(bytes, position, end);
        if (first == '[') {
            // The tags of the next game: the one before ends here. What
            // precedes the first tags (a comment, a title) stays with them.
            if (inMoves && hasTags) {
                entries << entryOf(bytes, start, position, hasTags);
                start = position;
            }
            inMoves = false;
            hasTags = true;
        } else if (first != 0 && first != '%') {
            inMoves = true;
        }
        position = end;
    }
    entries << entryOf(bytes, start, bytes.size(), hasTags);
    return entries;
}

QString decode(const QByteArray &bytes)
{
    QStringDecoder utf8(QStringDecoder::Utf8);
    const QString text = utf8.decode(bytes);
    return utf8.hasError() ? QString::fromLatin1(bytes) : text;
}

std::optional<GameRecord> read(const QByteArray &entry, QString *errorMessage)
{
    const QString text = decode(entry);
    const std::optional<Pgn::ParsedLine> line = Pgn::parseLine(text, QString(), errorMessage);
    if (!line)
        return std::nullopt;

    QHash<QString, QString> tags;
    GameRecord game;
    // The tags with a field of their own, or that say how to read the game.
    static const QStringList ownFields{QStringLiteral("Event"), QStringLiteral("Site"), QStringLiteral("Date"),
                                       QStringLiteral("Round"), QStringLiteral("White"), QStringLiteral("Black"),
                                       QStringLiteral("Result"), QStringLiteral("WhiteElo"),
                                       QStringLiteral("BlackElo"), QStringLiteral("ECO"), QStringLiteral("FEN"),
                                       QStringLiteral("SetUp"), QStringLiteral("PlyCount"),
                                       QLatin1String(uidTag)};
    static const QRegularExpression tag(QStringLiteral(R"re(^\s*\[(\w+)\s+"((?:[^"\\]|\\.)*)"\s*\])re"),
                                        QRegularExpression::MultilineOption);
    for (auto it = tag.globalMatch(text); it.hasNext();) {
        const QRegularExpressionMatch match = it.next();
        const QString name = match.captured(1);
        const QString value = unescape(match.captured(2));
        if (ownFields.contains(name))
            tags.insert(name, value);
        else if (!value.trimmed().isEmpty())
            game.tags << PgnTag{name, value.trimmed()};
    }
    game.event = known(tags.value(QStringLiteral("Event")));
    game.site = known(tags.value(QStringLiteral("Site")));
    game.round = known(tags.value(QStringLiteral("Round")));
    game.white = known(tags.value(QStringLiteral("White")));
    game.black = known(tags.value(QStringLiteral("Black")));
    const QString date = tags.value(QStringLiteral("Date")).trimmed();
    game.date = date == QLatin1String("????.??.??") ? QString() : date;
    const QString result = tags.value(QStringLiteral("Result")).trimmed();
    game.result = result == QLatin1String("*") ? QString() : result;
    game.whiteElo = tags.value(QStringLiteral("WhiteElo")).toInt();
    game.blackElo = tags.value(QStringLiteral("BlackElo")).toInt();
    game.eco = known(tags.value(QStringLiteral("ECO")));
    game.uid = tags.value(QLatin1String(uidTag)).trimmed();
    game.startFen = line->startFen;
    game.moves = line->moves;
    game.variations = line->variations;
    game.startComment = line->startComment;
    game.plyCount = int(line->moves.size());
    return game;
}

QByteArray write(const GameRecord &game)
{
    return withUid(Pgn::game(game).toUtf8(), game.uid) + '\n';
}

QByteArray withUid(const QByteArray &entry, const QString &uid)
{
    const QByteArray newline = entry.contains("\r\n") ? QByteArrayLiteral("\r\n") : QByteArrayLiteral("\n");
    const QByteArray line = QStringLiteral("[%1 \"%2\"]").arg(QLatin1String(uidTag), uid).toUtf8() + newline;
    QByteArray result;
    bool seenTag = false;
    bool inserted = false;
    for (qint64 position = 0; position < entry.size();) {
        const qint64 found = entry.indexOf('\n', position);
        const qint64 end = found < 0 ? entry.size() : found + 1;
        const QByteArray current = entry.mid(position, end - position);
        position = end;
        const QByteArray trimmed = current.trimmed();
        if (isUidTag(trimmed))
            continue;
        const bool isTag = trimmed.startsWith('[');
        if (!inserted && seenTag && !isTag) {
            result += line;
            inserted = true;
        }
        seenTag = seenTag || isTag;
        result += current;
    }
    if (!inserted) {
        if (!seenTag)
            return line + result;
        if (!result.endsWith('\n'))
            result += newline;
        result += line;
    }
    return result;
}

QString fileHash(const QByteArray &bytes)
{
    return QString::fromLatin1(QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex());
}

QString indexPath(const QString &pgnPath)
{
    const QFileInfo file(pgnPath);
    return file.dir().filePath(QLatin1Char('.') + file.fileName() + QStringLiteral(".pragma-index"));
}

std::optional<Index> readIndex(const QString &pgnPath, const QString &hash)
{
    QFile file(indexPath(pgnPath));
    if (!file.open(QIODevice::ReadOnly))
        return std::nullopt;
    const QJsonObject root = QJsonDocument::fromJson(file.readAll()).object();
    if (root.value(QStringLiteral("version")).toInt() != kIndexVersion
        || root.value(QStringLiteral("file")).toString() != hash)
        return std::nullopt;
    Index index;
    index.fileHash = hash;
    for (const QJsonValue &value : root.value(QStringLiteral("games")).toArray()) {
        const QJsonArray fields = value.toArray();
        Entry entry;
        entry.offset = qint64(fields.at(0).toDouble());
        entry.length = qint64(fields.at(1).toDouble());
        entry.hash = fields.at(2).toString();
        entry.uid = fields.at(3).toString();
        entry.isGame = fields.at(4).toBool();
        index.entries << entry;
    }
    return index;
}

void writeIndex(const QString &pgnPath, const Index &index)
{
    QJsonArray games;
    for (const Entry &entry : index.entries)
        games.append(QJsonArray{double(entry.offset), double(entry.length), entry.hash, entry.uid, entry.isGame});
    const QJsonObject root{{QStringLiteral("version"), kIndexVersion},
                           {QStringLiteral("file"), index.fileHash},
                           {QStringLiteral("games"), games}};
    QSaveFile file(indexPath(pgnPath));
    if (file.open(QIODevice::WriteOnly)) {
        file.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
        file.commit();
    }
}

} // namespace PgnFile
