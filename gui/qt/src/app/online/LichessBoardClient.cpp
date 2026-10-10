#include "LichessBoardClient.h"

#include "app/sources/SourceFetch.h"

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrlQuery>

namespace {

const QString kBase = QStringLiteral("https://lichess.org");

} // namespace

LichessBoardClient::LichessBoardClient(const QString &token, QObject *parent)
    : QObject(parent)
    , m_token(token)
    , m_network(new QNetworkAccessManager(this))
{
}

LichessBoardClient::~LichessBoardClient()
{
    for (QNetworkReply *reply : {m_events, m_seek, m_gameStream}) {
        if (reply) {
            reply->disconnect(this);
            reply->abort();
            reply->deleteLater();
        }
    }
}

void LichessBoardClient::seek(const Seek &seek)
{
    if (m_seek || m_gameStream)
        return;
    m_game = OnlineGame();
    openEventStream();
    // The seek lives as long as this request: the server keeps it open until
    // an opponent is found, and the game arrives on the event stream.
    QNetworkRequest request(QUrl(kBase + QStringLiteral("/api/board/seek")));
    request.setHeader(QNetworkRequest::UserAgentHeader, SourceFetch::userAgent());
    request.setRawHeader("Authorization", "Bearer " + m_token.toUtf8());
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/x-www-form-urlencoded"));
    QUrlQuery form;
    form.addQueryItem(QStringLiteral("rated"), seek.rated ? QStringLiteral("true") : QStringLiteral("false"));
    form.addQueryItem(QStringLiteral("time"), QString::number(seek.minutes));
    form.addQueryItem(QStringLiteral("increment"), QString::number(seek.increment));
    form.addQueryItem(QStringLiteral("color"), seek.color);
    m_seek = m_network->post(request, form.toString(QUrl::FullyEncoded).toUtf8());
    connect(m_seek, &QNetworkReply::finished, this, [this] {
        QNetworkReply *reply = m_seek;
        m_seek = nullptr;
        reply->deleteLater();
        // Closed by the server when a game starts (the event stream tells),
        // or by us; anything else is a refusal.
        if (reply->error() != QNetworkReply::NoError && reply->error() != QNetworkReply::OperationCanceledError
            && !m_gameStream)
            Q_EMIT failed(SourceFetch::httpError(reply, QStringLiteral("lichess.org")));
    });
    Q_EMIT seeking();
}

void LichessBoardClient::cancelSeek()
{
    if (m_seek)
        m_seek->abort();
    if (m_events && !m_gameStream) {
        m_events->disconnect(this);
        m_events->abort();
        m_events->deleteLater();
        m_events = nullptr;
    }
}

void LichessBoardClient::resume(const QString &gameId)
{
    if (m_seek || m_gameStream || gameId.isEmpty())
        return;
    openGameStream(gameId);
}

void LichessBoardClient::openEventStream()
{
    if (m_events)
        return;
    QNetworkRequest request(QUrl(kBase + QStringLiteral("/api/stream/event")));
    request.setHeader(QNetworkRequest::UserAgentHeader, SourceFetch::userAgent());
    request.setRawHeader("Authorization", "Bearer " + m_token.toUtf8());
    m_eventBuffer.clear();
    m_events = m_network->get(request);
    connect(m_events, &QNetworkReply::readyRead, this,
            [this] { readLines(m_events, m_eventBuffer, &LichessBoardClient::eventLine); });
    connect(m_events, &QNetworkReply::finished, this, [this] {
        QNetworkReply *reply = m_events;
        m_events = nullptr;
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError && reply->error() != QNetworkReply::OperationCanceledError)
            Q_EMIT failed(SourceFetch::httpError(reply, QStringLiteral("lichess.org")));
    });
}

void LichessBoardClient::readLines(QNetworkReply *reply, QByteArray &buffer,
                                   void (LichessBoardClient::*line)(const QByteArray &))
{
    buffer += reply->readAll();
    int at = 0;
    while (true) {
        const int end = int(buffer.indexOf('\n', at));
        if (end < 0)
            break;
        const QByteArray one = buffer.mid(at, end - at);
        at = end + 1;
        if (!one.trimmed().isEmpty())
            (this->*line)(one);
    }
    buffer.remove(0, at);
}

void LichessBoardClient::eventLine(const QByteArray &line)
{
    if (const std::optional<QString> gameId = LichessBoard::gameStarted(line); gameId && !m_gameStream)
        openGameStream(*gameId);
}

void LichessBoardClient::openGameStream(const QString &gameId)
{
    QNetworkRequest request(QUrl(kBase + QStringLiteral("/api/board/game/stream/%1").arg(gameId)));
    request.setHeader(QNetworkRequest::UserAgentHeader, SourceFetch::userAgent());
    request.setRawHeader("Authorization", "Bearer " + m_token.toUtf8());
    m_gameBuffer.clear();
    m_game = OnlineGame();
    m_game.id = gameId;
    m_gameStream = m_network->get(request);
    connect(m_gameStream, &QNetworkReply::readyRead, this,
            [this] { readLines(m_gameStream, m_gameBuffer, &LichessBoardClient::gameLine); });
    connect(m_gameStream, &QNetworkReply::finished, this, [this] {
        QNetworkReply *reply = m_gameStream;
        m_gameStream = nullptr;
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError && reply->error() != QNetworkReply::OperationCanceledError) {
            Q_EMIT failed(SourceFetch::httpError(reply, QStringLiteral("lichess.org")));
        } else if (!m_game.isOver()) {
            // The stream closed without an end: the game is over for us.
            Q_EMIT failed(tr("The connection with lichess.org was lost."));
        }
        closeGame();
    });
}

void LichessBoardClient::gameLine(const QByteArray &line)
{
    const bool first = m_game.white.isEmpty() && m_game.black.isEmpty();
    if (!LichessBoard::applyGameLine(line, m_game))
        return;
    if (first && !m_game.white.isEmpty())
        Q_EMIT gameStarted(m_game);
    Q_EMIT gameUpdated(m_game);
    if (m_game.isOver())
        Q_EMIT gameFinished(m_game);
}

void LichessBoardClient::closeGame()
{
    if (m_events) {
        m_events->disconnect(this);
        m_events->abort();
        m_events->deleteLater();
        m_events = nullptr;
    }
}

void LichessBoardClient::post(const QString &path, const QByteArray &form)
{
    QNetworkRequest request(QUrl(kBase + path));
    request.setHeader(QNetworkRequest::UserAgentHeader, SourceFetch::userAgent());
    request.setRawHeader("Authorization", "Bearer " + m_token.toUtf8());
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/x-www-form-urlencoded"));
    QNetworkReply *reply = m_network->post(request, form);
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError)
            Q_EMIT failed(SourceFetch::httpError(reply, QStringLiteral("lichess.org")));
    });
}

void LichessBoardClient::move(const QString &uci)
{
    if (m_game.id.isEmpty())
        return;
    post(QStringLiteral("/api/board/game/%1/move/%2").arg(m_game.id, uci));
}

void LichessBoardClient::resign()
{
    if (!m_game.id.isEmpty())
        post(QStringLiteral("/api/board/game/%1/resign").arg(m_game.id));
}

void LichessBoardClient::offerDraw()
{
    if (!m_game.id.isEmpty())
        post(QStringLiteral("/api/board/game/%1/draw/yes").arg(m_game.id));
}

void LichessBoardClient::abort()
{
    if (!m_game.id.isEmpty())
        post(QStringLiteral("/api/board/game/%1/abort").arg(m_game.id));
}
