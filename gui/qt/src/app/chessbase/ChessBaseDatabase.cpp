#include "ChessBaseDatabase.h"

#include "CbgDecoder.h"
#include "ChessBase2Database.h"
#include "app/ChessPosition.h"
#include "app/GameVariations.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>

namespace {

struct Text {
    Q_DECLARE_TR_FUNCTIONS(ChessBaseDatabase)
};

constexpr int kHeaderRecord = 46;
/// The entity files start with a header, the count first (little-endian):
/// 28 bytes in older databases, 32 in those of recent ChessBase versions.
constexpr int kEntityHeader = 28;
constexpr int kPlayerRecord = 67;
constexpr int kTournamentRecord = 99;

quint32 bigEndian(const QByteArray &bytes, int offset, int size)
{
    quint32 value = 0;
    for (int i = 0; i < size; ++i)
        value = (value << 8) | quint8(bytes.at(offset + i));
    return value;
}

/// A fixed-width text field: Latin-1, up to the first NUL.
QString field(const QByteArray &bytes, int offset, int size)
{
    const QByteArray raw = bytes.mid(offset, size);
    const int end = raw.indexOf('\0');
    return QString::fromLatin1(end < 0 ? raw : raw.left(end)).trimmed();
}

/// Where the records of an entity file start: after the header, whose
/// length is what the count of records leaves of the file.
int entityStart(const QByteArray &bytes, int recordSize)
{
    if (bytes.size() < 4)
        return kEntityHeader;
    const qint64 count = qint64(quint8(bytes.at(0))) | qint64(quint8(bytes.at(1))) << 8
                         | qint64(quint8(bytes.at(2))) << 16 | qint64(quint8(bytes.at(3))) << 24;
    const qint64 header = bytes.size() - count * recordSize;
    return header >= kEntityHeader && header <= 64 ? int(header) : kEntityHeader;
}

QByteArray readAll(const QString &path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

} // namespace

QString ChessBaseDatabase::sibling(const QString &path, const QString &suffix)
{
    const QFileInfo info(path);
    for (const QString &candidate : {suffix, suffix.toUpper()}) {
        const QString found = info.dir().filePath(info.completeBaseName() + QLatin1Char('.') + candidate);
        if (QFile::exists(found))
            return found;
    }
    return info.dir().filePath(info.completeBaseName() + QLatin1Char('.') + suffix);
}

std::unique_ptr<ChessBaseDatabase> ChessBaseDatabase::open(const QString &cbhPath, QString *errorMessage)
{
    if (cbhPath.endsWith(QLatin1String(".2cbh"), Qt::CaseInsensitive))
        return ChessBase2Database::open(cbhPath, errorMessage);
    const auto fail = [&](const QString &why) {
        if (errorMessage)
            *errorMessage = why;
        return std::unique_ptr<ChessBaseDatabase>();
    };
    QFile cbh(cbhPath);
    if (!cbh.open(QIODevice::ReadOnly))
        return fail(Text::tr("Cannot read %1.").arg(QFileInfo(cbhPath).fileName()));
    std::unique_ptr<ChessBaseDatabase> database(new ChessBaseDatabase);
    database->m_headers = cbh.readAll();
    if (database->m_headers.size() < kHeaderRecord)
        return fail(Text::tr("%1 is not a ChessBase database.").arg(QFileInfo(cbhPath).fileName()));
    database->m_count = database->m_headers.size() / kHeaderRecord - 1; // The first record is the file's own.
    database->m_cbgPath = sibling(cbhPath, QStringLiteral("cbg"));
    if (!QFile::exists(database->m_cbgPath))
        return fail(Text::tr("The moves file (.cbg) is missing beside %1.").arg(QFileInfo(cbhPath).fileName()));

    const QByteArray players = readAll(sibling(cbhPath, QStringLiteral("cbp")));
    for (int offset = entityStart(players, kPlayerRecord); offset + kPlayerRecord <= players.size(); offset += kPlayerRecord)
        database->m_players << playerName(field(players, offset + 9, 30), field(players, offset + 39, 20));
    const QByteArray tournaments = readAll(sibling(cbhPath, QStringLiteral("cbt")));
    for (int offset = entityStart(tournaments, kTournamentRecord); offset + kTournamentRecord <= tournaments.size();
         offset += kTournamentRecord)
        database->m_tournaments << Tournament{field(tournaments, offset + 9, 40), field(tournaments, offset + 49, 30)};
    return database;
}

QString ChessBaseDatabase::playerName(const QString &last, const QString &first)
{
    if (first.isEmpty())
        return last;
    if (last.isEmpty())
        return first;
    return QStringLiteral("%1, %2").arg(last, first);
}

QString ChessBaseDatabase::dateText(quint32 date)
{
    const int day = date & 31;
    const int month = (date >> 5) & 15;
    const int year = date >> 9;
    if (year == 0)
        return QString();
    const auto part = [](int value, int width) {
        return value > 0 ? QStringLiteral("%1").arg(value, width, 10, QLatin1Char('0')) : QString(width, QLatin1Char('?'));
    };
    return QStringLiteral("%1.%2.%3").arg(part(year, 4), part(month, 2), part(day, 2));
}

ChessBaseDatabase::Entry ChessBaseDatabase::entry(int index) const
{
    Entry entry;
    if (index < 0 || index >= m_count)
        return entry;
    const int at = (index + 1) * kHeaderRecord;
    const QByteArray &h = m_headers;
    const quint8 flags = quint8(h.at(at));
    entry.isGame = (flags & 0x01) && !(flags & 0x02); // Bit 1 marks a guiding text.
    entry.deleted = flags & 0x80;
    GameRecord &game = entry.header;
    game.white = m_players.value(int(bigEndian(h, at + 9, 3)));
    game.black = m_players.value(int(bigEndian(h, at + 12, 3)));
    const Tournament tournament = m_tournaments.value(int(bigEndian(h, at + 15, 3)));
    game.event = tournament.title;
    game.site = tournament.place;
    game.date = dateText(bigEndian(h, at + 24, 3));
    switch (quint8(h.at(at + 27))) {
    case 0: game.result = QStringLiteral("0-1"); break;
    case 1: game.result = QStringLiteral("1/2-1/2"); break;
    case 2: game.result = QStringLiteral("1-0"); break;
    default: game.result = QStringLiteral("*"); break;
    }
    if (const int round = quint8(h.at(at + 29)); round > 0)
        game.round = QString::number(round);
    game.whiteElo = int(bigEndian(h, at + 31, 2));
    game.blackElo = int(bigEndian(h, at + 33, 2));
    // ECO: 1 to 500 for A00 to E99, after a seven-bit sub-code.
    if (const int eco = int(bigEndian(h, at + 35, 2) >> 7); eco >= 1 && eco <= 500)
        game.eco = QStringLiteral("%1%2").arg(QChar(u'A' + (eco - 1) / 100)).arg((eco - 1) % 100, 2, 10, QLatin1Char('0'));
    game.plyCount = quint8(h.at(at + 45)) * 2;
    return entry;
}

GameRecord ChessBaseDatabase::game(int index, QString *errorMessage) const
{
    const Entry found = entry(index);
    GameRecord game = found.header;
    game.plyCount = 0;
    if (!found.isGame)
        return game;
    const quint32 offset = bigEndian(m_headers, (index + 1) * kHeaderRecord + 1, 4);
    QFile cbg(m_cbgPath);
    if (!cbg.open(QIODevice::ReadOnly) || !cbg.seek(offset)) {
        if (errorMessage)
            *errorMessage = Text::tr("The moves file cannot be read.");
        return game;
    }
    const QByteArray head = cbg.read(4);
    const int size = CbgDecoder::recordSize(head);
    const QByteArray record = head + cbg.read(qMax(0, size - 4));
    const CbgDecoder::Decoded decoded = CbgDecoder::decode(record);
    fillMoves(game, decoded, errorMessage);
    return game;
}

void ChessBaseDatabase::fillMoves(GameRecord &game, const CbgDecoder::Decoded &decoded, QString *errorMessage)
{
    if (!decoded.error.isEmpty() && errorMessage)
        *errorMessage = decoded.error;
    // SAN, and the line cut at the first move that is not legal.
    game.startFen = decoded.startFen;
    std::optional<ChessPosition> position = decoded.startFen.isEmpty() ? ChessPosition::startingPosition()
                                                                        : ChessPosition::fromFen(decoded.startFen);
    if (!position) {
        if (errorMessage)
            *errorMessage = Text::tr("The start position is not valid: %1").arg(decoded.startFen);
        game.startFen.clear();
        return;
    }
    const ChessPosition start = *position;
    for (const QString &uci : decoded.uciMoves) {
        const std::optional<ChessMove> move = position->moveFromUci(uci);
        if (!move) {
            if (errorMessage && errorMessage->isEmpty())
                *errorMessage = Text::tr("Move %1 (%2) is not legal.").arg(game.moves.size() + 1).arg(uci);
            break;
        }
        game.moves << MoveRecord{position->san(*move), uci, {}};
        position->play(*move);
    }
    game.plyCount = int(game.moves.size());
    // The variations: SAN filled in, illegal tails cut, with the rules.
    game.variations = decoded.variations;
    GameVariations::resolve(game, start);
}
