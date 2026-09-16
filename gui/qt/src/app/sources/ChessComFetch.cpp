#include "ChessComFetch.h"

#include "app/Pgn.h"

#include <QDate>
#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QRegularExpression>

#include <algorithm>

namespace {

const QString kSite = QStringLiteral("chess.com");

QString pgnTag(const QString &pgn, const char *name)
{
    const QRegularExpression tag(QStringLiteral(R"re(\[%1\s+"([^"]*)"\])re").arg(QLatin1String(name)));
    return tag.match(pgn).captured(1);
}

/// "https://api.chess.com/pub/player/x/games/2024/05" → "2024/05".
QString archiveMonth(const QString &url)
{
    return url.section(QLatin1Char('/'), -2);
}

} // namespace

ChessComFetch::ChessComFetch(const GameSource &source, QNetworkAccessManager *network, QObject *parent)
    : SourceFetch(parent)
    , m_source(source)
    , m_network(network)
    , m_state(source.state)
{
}

std::optional<ImportedGame> ChessComFetch::parseGame(const QJsonObject &game)
{
    if (game.value(QStringLiteral("rules")).toString() != QLatin1String("chess"))
        return std::nullopt;
    const QString pgn = game.value(QStringLiteral("pgn")).toString();
    QString error;
    const std::optional<Pgn::ParsedLine> line = Pgn::parseLine(pgn, QString(), &error);
    if (!line)
        return std::nullopt;

    const QJsonObject white = game.value(QStringLiteral("white")).toObject();
    const QJsonObject black = game.value(QStringLiteral("black")).toObject();
    ImportedGame imported;
    imported.externalId = game.value(QStringLiteral("uuid")).toString();
    if (imported.externalId.isEmpty())
        imported.externalId = game.value(QStringLiteral("url")).toString();
    GameRecord &record = imported.game;
    record.white = white.value(QStringLiteral("username")).toString();
    record.black = black.value(QStringLiteral("username")).toString();
    record.whiteElo = white.value(QStringLiteral("rating")).toInt();
    record.blackElo = black.value(QStringLiteral("rating")).toInt();
    record.event = pgnTag(pgn, "Event");
    record.site = game.value(QStringLiteral("url")).toString();
    record.date = pgnTag(pgn, "Date");
    record.round = QStringLiteral("-");
    record.result = pgnTag(pgn, "Result");
    record.eco = pgnTag(pgn, "ECO");
    record.startFen = line->startFen;
    record.moves = line->moves;
    record.plyCount = int(line->moves.size());
    return imported;
}

void ChessComFetch::start()
{
    const QUrl url(QStringLiteral("https://api.chess.com/pub/player/%1/games/archives")
                       .arg(QString::fromLatin1(QUrl::toPercentEncoding(m_source.account.toLower()))));
    get(url, [this](const QByteArray &body) {
        const QJsonArray archives = QJsonDocument::fromJson(body).object().value(QStringLiteral("archives")).toArray();
        const QString since = m_source.settings.value(QLatin1String(SourceSettings::since)).toString();
        const QString sinceMonth = since.isEmpty() ? QString() : since.left(7).replace(QLatin1Char('-'), QLatin1Char('/'));
        const QString syncedMonth = m_state.value(QStringLiteral("month")).toString();
        for (const QJsonValue &archive : archives) {
            const QString month = archiveMonth(archive.toString());
            // Months are "yyyy/MM", so they compare as text.
            if (month >= sinceMonth && month >= syncedMonth)
                m_archives << archive.toString();
        }
        fetchNextArchive();
    });
}

void ChessComFetch::abort()
{
    if (m_reply) {
        m_reply->disconnect(this);
        m_reply->abort();
        m_reply->deleteLater();
        m_reply = nullptr;
    }
    m_archives.clear();
}

void ChessComFetch::fetchNextArchive()
{
    if (m_archives.isEmpty()) {
        Q_EMIT finished(QString());
        return;
    }
    const QString url = m_archives.takeFirst();
    get(QUrl(url), [this, url](const QByteArray &body) {
        const QJsonArray games = QJsonDocument::fromJson(body).object().value(QStringLiteral("games")).toArray();
        const qint64 syncedEnd = qint64(m_state.value(QStringLiteral("endTime")).toDouble());
        const QString since = m_source.settings.value(QLatin1String(SourceSettings::since)).toString();
        const qint64 sinceTime = since.isEmpty() ? 0 : QDate::fromString(since, Qt::ISODate).startOfDay(Qt::UTC).toSecsSinceEpoch();
        const bool ratedOnly = m_source.settings.value(QLatin1String(SourceSettings::ratedOnly)).toBool();

        QList<std::pair<qint64, ImportedGame>> fetched;
        qint64 lastEnd = syncedEnd;
        for (const QJsonValue &value : games) {
            const QJsonObject game = value.toObject();
            const qint64 endTime = qint64(game.value(QStringLiteral("end_time")).toDouble());
            lastEnd = qMax(lastEnd, endTime);
            if (endTime <= syncedEnd || endTime < sinceTime)
                continue;
            if (ratedOnly && !game.value(QStringLiteral("rated")).toBool())
                continue;
            if (std::optional<ImportedGame> imported = parseGame(game))
                fetched.append({endTime, *imported});
        }
        std::stable_sort(fetched.begin(), fetched.end(),
                         [](const auto &a, const auto &b) { return a.first < b.first; });
        QList<ImportedGame> batch;
        for (const auto &[endTime, imported] : std::as_const(fetched))
            batch << imported;

        m_state.insert(QStringLiteral("month"), archiveMonth(url));
        m_state.insert(QStringLiteral("endTime"), double(lastEnd));
        Q_EMIT gamesFetched(batch, m_state);
        fetchNextArchive();
    });
}

void ChessComFetch::get(const QUrl &url, std::function<void(const QByteArray &)> handle)
{
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, userAgent());
    m_reply = m_network->get(request);
    connect(m_reply, &QNetworkReply::finished, this, [this, handle] {
        QNetworkReply *reply = m_reply;
        m_reply = nullptr;
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            Q_EMIT finished(httpError(reply, kSite));
            return;
        }
        handle(reply->readAll());
    });
}
