#include "LichessFetch.h"

#include "SourceCredentials.h"
#include "app/Pgn.h"

#include <QDate>
#include <QJsonDocument>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QUrlQuery>

namespace {

const QString kSite = QStringLiteral("lichess.org");
constexpr qsizetype kBatchSize = 100;

QString playerName(const QJsonObject &player)
{
    const QString name = player.value(QStringLiteral("user")).toObject().value(QStringLiteral("name")).toString();
    if (!name.isEmpty())
        return name;
    if (player.contains(QStringLiteral("aiLevel")))
        return QObject::tr("Stockfish level %1").arg(player.value(QStringLiteral("aiLevel")).toInt());
    return QObject::tr("Anonymous");
}

} // namespace

LichessFetch::LichessFetch(const GameSource &source, QNetworkAccessManager *network, QObject *parent)
    : SourceFetch(parent)
    , m_source(source)
    , m_network(network)
    , m_state(source.state)
{
}

std::optional<ImportedGame> LichessFetch::parseGame(const QJsonObject &game)
{
    const QString variant = game.value(QStringLiteral("variant")).toString();
    if (variant != QLatin1String("standard") && variant != QLatin1String("fromPosition"))
        return std::nullopt;
    const QString status = game.value(QStringLiteral("status")).toString();
    if (status == QLatin1String("created") || status == QLatin1String("started")
        || status == QLatin1String("aborted"))
        return std::nullopt;

    QString error;
    const std::optional<Pgn::ParsedLine> line = Pgn::parseLine(
        game.value(QStringLiteral("moves")).toString(), game.value(QStringLiteral("initialFen")).toString(), &error);
    if (!line)
        return std::nullopt;

    const QJsonObject players = game.value(QStringLiteral("players")).toObject();
    const QJsonObject white = players.value(QStringLiteral("white")).toObject();
    const QJsonObject black = players.value(QStringLiteral("black")).toObject();
    const QString winner = game.value(QStringLiteral("winner")).toString();
    const QString id = game.value(QStringLiteral("id")).toString();

    ImportedGame imported;
    imported.externalId = id;
    GameRecord &record = imported.game;
    record.white = playerName(white);
    record.black = playerName(black);
    record.whiteElo = white.value(QStringLiteral("rating")).toInt();
    record.blackElo = black.value(QStringLiteral("rating")).toInt();
    const QString speed = game.value(QStringLiteral("speed")).toString();
    record.event = game.value(QStringLiteral("rated")).toBool() ? QStringLiteral("Rated %1 game").arg(speed)
                                                                : QStringLiteral("Casual %1 game").arg(speed);
    record.site = QStringLiteral("https://lichess.org/") + id;
    record.date = QDateTime::fromMSecsSinceEpoch(qint64(game.value(QStringLiteral("createdAt")).toDouble()),
                                                 Qt::UTC).toString(QStringLiteral("yyyy.MM.dd"));
    record.round = QStringLiteral("-");
    record.result = winner == QLatin1String("white") ? QStringLiteral("1-0")
        : winner == QLatin1String("black")           ? QStringLiteral("0-1")
                                                     : QStringLiteral("1/2-1/2");
    record.eco = game.value(QStringLiteral("opening")).toObject().value(QStringLiteral("eco")).toString();
    record.startFen = line->startFen;
    record.moves = line->moves;
    record.plyCount = int(line->moves.size());
    return imported;
}

void LichessFetch::start()
{
    const QString token = SourceCredentials::token(m_source.uuid);
    if (token.isEmpty()) {
        Q_EMIT finished(tr("Sign in to lichess.org to download the games."));
        return;
    }

    QUrl url(QStringLiteral("https://lichess.org/api/games/user/%1")
                 .arg(QString::fromLatin1(QUrl::toPercentEncoding(m_source.account))));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("moves"), QStringLiteral("true"));
    query.addQueryItem(QStringLiteral("opening"), QStringLiteral("true"));
    query.addQueryItem(QStringLiteral("sort"), QStringLiteral("dateAsc"));
    qint64 since = qint64(m_state.value(QStringLiteral("createdAt")).toDouble());
    if (since > 0) {
        ++since; // Games created after the last one imported.
    } else {
        const QString date = m_source.settings.value(QLatin1String(SourceSettings::since)).toString();
        if (!date.isEmpty())
            since = QDate::fromString(date, Qt::ISODate).startOfDay(Qt::UTC).toMSecsSinceEpoch();
    }
    if (since > 0)
        query.addQueryItem(QStringLiteral("since"), QString::number(since));
    if (m_source.settings.value(QLatin1String(SourceSettings::ratedOnly)).toBool())
        query.addQueryItem(QStringLiteral("rated"), QStringLiteral("true"));
    url.setQuery(query);

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, userAgent());
    request.setRawHeader("Accept", "application/x-ndjson");
    request.setRawHeader("Authorization", "Bearer " + token.toUtf8());
    m_reply = m_network->get(request);
    connect(m_reply, &QNetworkReply::readyRead, this, [this] { readLines(false); });
    connect(m_reply, &QNetworkReply::finished, this, [this] {
        QNetworkReply *reply = m_reply;
        m_reply = nullptr;
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            Q_EMIT finished(httpError(reply, kSite));
            return;
        }
        m_buffer += reply->readAll();
        readLines(true);
        Q_EMIT finished(QString());
    });
}

void LichessFetch::abort()
{
    if (!m_reply)
        return;
    m_reply->disconnect(this);
    m_reply->abort();
    m_reply->deleteLater();
    m_reply = nullptr;
}

void LichessFetch::readLines(bool flush)
{
    if (m_reply)
        m_buffer += m_reply->readAll();
    qsizetype newline;
    while ((newline = m_buffer.indexOf('\n')) >= 0) {
        const QByteArray line = m_buffer.left(newline).trimmed();
        m_buffer.remove(0, newline + 1);
        if (line.isEmpty())
            continue;
        const QJsonObject game = QJsonDocument::fromJson(line).object();
        // The cursor moves past every game, imported or skipped.
        m_state.insert(QStringLiteral("createdAt"), game.value(QStringLiteral("createdAt")).toDouble());
        if (std::optional<ImportedGame> imported = parseGame(game))
            m_batch << *imported;
        if (m_batch.size() >= kBatchSize) {
            Q_EMIT gamesFetched(m_batch, m_state);
            m_batch.clear();
        }
    }
    if (flush && (!m_batch.isEmpty() || m_state != m_source.state)) {
        Q_EMIT gamesFetched(m_batch, m_state);
        m_batch.clear();
    }
}
