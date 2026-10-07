#include "ChessBase2Database.h"

#include "Cbg2Decoder.h"
#include "app/UiLanguage.h"

#include <QCoreApplication>
#include <QFileInfo>
#include <QtEndian>

#include <limits>

namespace {

struct Text {
    Q_DECLARE_TR_FUNCTIONS(ChessBaseDatabase)
};

/// The `.2cbh` header: the record size at 0x0A, the next game id at 0x10.
constexpr int kRecordSizeAt = 0x0A;
constexpr int kMinimumRecord = 0xC0;

/// Fields of a game record.
constexpr int kMovesAt = 0x08;
constexpr int kAnnotationsAt = 0x10;
constexpr int kWhiteAt = 0x18;
constexpr int kBlackAt = 0x20;
constexpr int kTournamentAt = 0x28;
constexpr int kResultAt = 0x58;
constexpr int kRoundAt = 0x5A;
constexpr int kSubroundAt = 0x5C;
constexpr int kWhiteEloAt = 0x60;
constexpr int kBlackEloAt = 0x70;
constexpr int kEcoAt = 0x80;
constexpr int kFullMovesAt = 0x8A;
constexpr int kDateAt = 0xBC;

/// The `.2lid`'s types, in the order of its containers.
constexpr int kPlayers = 0;
constexpr int kTournaments = 1;

/// A text of an entity: its length in bytes (little-endian), then UTF-8.
QString text(const QByteArray &bytes, qsizetype &at)
{
    if (at + 4 > bytes.size())
        return {};
    const qint32 length = qFromLittleEndian<qint32>(bytes.constData() + at);
    at += 4;
    if (length <= 0 || at + length > bytes.size())
        return {};
    const QString value = QString::fromUtf8(bytes.constData() + at, length).trimmed();
    at += length;
    return value;
}

} // namespace

bool ChessBase2Database::Mapped::open(const QString &path)
{
    file = std::make_unique<QFile>(path);
    if (!file->open(QIODevice::ReadOnly))
        return false;
    size = file->size();
    data = size > 0 ? file->map(0, size) : nullptr;
    return size == 0 || data;
}

QByteArray ChessBase2Database::Mapped::bytes(qint64 offset, qint64 length) const
{
    if (!data || offset < 0 || length <= 0 || offset + length > size)
        return {};
    return QByteArray::fromRawData(reinterpret_cast<const char *>(data + offset), length);
}

std::unique_ptr<ChessBaseDatabase> ChessBase2Database::open(const QString &path, QString *errorMessage)
{
    const QString name = QFileInfo(path).fileName();
    const auto fail = [&](const QString &why) {
        if (errorMessage)
            *errorMessage = why;
        return std::unique_ptr<ChessBaseDatabase>();
    };
    std::unique_ptr<ChessBase2Database> database(new ChessBase2Database);
    if (!database->m_headers.open(path))
        return fail(Text::tr("Cannot read %1.").arg(name));
    const Mapped &headers = database->m_headers;
    if (headers.size < kRecordSizeAt + 2)
        return fail(Text::tr("%1 is not a ChessBase database.").arg(name));
    database->m_recordSize = qFromLittleEndian<quint16>(headers.data + kRecordSizeAt);
    if (database->m_recordSize < kMinimumRecord)
        return fail(Text::tr("%1 is not a ChessBase database.").arg(name));
    database->m_count = int(qMin<qint64>(headers.size / database->m_recordSize - 1, std::numeric_limits<int>::max()));

    if (!database->m_moves.open(sibling(path, QStringLiteral("2cbg"))))
        return fail(Text::tr("The moves file (.2cbg) is missing beside %1.").arg(name));

    // The annotations: without them the games still come, unannotated.
    database->m_annotations.open(sibling(path, QStringLiteral("2cba")));

    // Players and tournaments: without them the games still come, unnamed.
    if (database->m_entities.open(sibling(path, QStringLiteral("2lid"))) && database->m_entities.size >= 8) {
        const uchar *lid = database->m_entities.data;
        database->m_entityHeader = qFromBigEndian<qint32>(lid);
        const qint32 types = qFromBigEndian<qint32>(lid + 4);
        for (qint32 type = 0; type < types && 8 + 20 * (type + 1) <= database->m_entityHeader
                              && 8 + 20 * (type + 1) <= database->m_entities.size;
             ++type) {
            const uchar *entry = lid + 8 + 20 * type;
            database->m_containerSizes.push_back(qFromBigEndian<qint32>(entry));
            database->m_entityCounts.push_back(qFromBigEndian<qint64>(entry + 4));
            database->m_blockSize += database->m_containerSizes.back();
        }
    }
    return database;
}

const uchar *ChessBase2Database::record(int index) const
{
    const qint64 at = qint64(index + 1) * m_recordSize;
    return index >= 0 && index < m_count && at + m_recordSize <= m_headers.size ? m_headers.data + at : nullptr;
}

QByteArray ChessBase2Database::entity(int type, qint64 id) const
{
    if (type >= int(m_containerSizes.size()) || id < 0 || id >= m_entityCounts[type] || m_blockSize <= 0)
        return {};
    qint64 at = m_entityHeader + id * m_blockSize;
    for (int before = 0; before < type; ++before)
        at += m_containerSizes[before];
    const QByteArray head = m_entities.bytes(at, 4);
    if (head.isEmpty())
        return {};
    const qint32 length = qFromLittleEndian<qint32>(head.constData());
    if (length <= 0 || length > m_containerSizes[type] - 4)
        return {};
    return m_entities.bytes(at + 4, length);
}

QString ChessBase2Database::playerName(qint64 id) const
{
    const QByteArray player = entity(kPlayers, id);
    qsizetype at = 0;
    const QString last = text(player, at);
    const QString first = text(player, at);
    return ChessBaseDatabase::playerName(last, first);
}

ChessBaseDatabase::Entry ChessBase2Database::entry(int index) const
{
    Entry entry;
    const uchar *r = record(index);
    if (!r)
        return entry;
    // Bit 0 a record, bit 1 a guiding text, bit 7 deleted; kind 1 a game
    // (2 is an analysis, laid out otherwise).
    const quint8 flags = r[0];
    entry.isGame = (flags & 0x01) && !(flags & 0x02) && r[2] == 1;
    entry.deleted = flags & 0x80;
    if (!entry.isGame)
        return entry;
    GameRecord &game = entry.header;
    game.white = playerName(qFromLittleEndian<qint64>(r + kWhiteAt));
    game.black = playerName(qFromLittleEndian<qint64>(r + kBlackAt));
    // A tournament: the place first, then the title.
    const QByteArray tournament = entity(kTournaments, qFromLittleEndian<qint64>(r + kTournamentAt));
    qsizetype at = 0;
    game.site = text(tournament, at);
    game.event = text(tournament, at);
    game.date = dateText(qFromLittleEndian<quint32>(r + kDateAt));
    switch (r[kResultAt]) {
    case 0: game.result = QStringLiteral("0-1"); break;
    case 1: game.result = QStringLiteral("1/2-1/2"); break;
    case 2: game.result = QStringLiteral("1-0"); break;
    default: game.result = QStringLiteral("*"); break; // A line, a forfeit, 0-0.
    }
    if (const qint16 round = qFromLittleEndian<qint16>(r + kRoundAt); round > 0) {
        game.round = QString::number(round);
        if (const qint16 subround = qFromLittleEndian<qint16>(r + kSubroundAt); subround > 0)
            game.round += QLatin1Char('.') + QString::number(subround);
    }
    game.whiteElo = qMax<qint16>(0, qFromLittleEndian<qint16>(r + kWhiteEloAt));
    game.blackElo = qMax<qint16>(0, qFromLittleEndian<qint16>(r + kBlackEloAt));
    // ECO: 1 to 500 for A00 to E99, after a seven-bit sub-code, as in the .cbh.
    if (const int eco = qFromLittleEndian<quint16>(r + kEcoAt) >> 7; eco >= 1 && eco <= 500)
        game.eco = QStringLiteral("%1%2").arg(QChar(u'A' + (eco - 1) / 100)).arg((eco - 1) % 100, 2, 10, QLatin1Char('0'));
    game.plyCount = qMax<qint16>(0, qFromLittleEndian<qint16>(r + kFullMovesAt)) * 2;
    return entry;
}

GameRecord ChessBase2Database::game(int index, QString *errorMessage) const
{
    const Entry found = entry(index);
    GameRecord game = found.header;
    game.plyCount = 0;
    if (!found.isGame)
        return game;
    bool chess960 = false;
    QString error;
    const QByteArray mapped = m_moves.bytes(0, m_moves.size);
    const QByteArray content =
        Cbg2Decoder::contentAt(mapped, qFromLittleEndian<qint64>(record(index) + kMovesAt), &chess960, &error);
    if (!error.isEmpty()) {
        if (errorMessage)
            *errorMessage = error;
        return game;
    }
    if (chess960) {
        if (errorMessage)
            *errorMessage = Text::tr("Chess960 games are not read yet.");
        return game;
    }
    // Every game has a record of annotations, if only their end.
    std::optional<Cba2Decoder::Decoded> notes;
    if (m_annotations.data) {
        QString missing;
        const QByteArray annotations = Cbg2Decoder::contentAt(m_annotations.bytes(0, m_annotations.size),
                                                              qFromLittleEndian<qint64>(record(index) + kAnnotationsAt),
                                                              nullptr, &missing);
        if (missing.isEmpty())
            notes = Cba2Decoder::decode(annotations, UiLanguage::effective());
    }
    fillMoves(game, Cbg2Decoder::decode(content), errorMessage, notes ? &*notes : nullptr);
    return game;
}
