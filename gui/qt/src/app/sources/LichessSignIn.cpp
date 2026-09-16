#include "LichessSignIn.h"

#include "SourceFetch.h"

#include <QCryptographicHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QRandomGenerator>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
#include <QUrlQuery>

namespace {

const char kClientId[] = "pragma-chess";
constexpr int kTimeoutMs = 5 * 60 * 1000;

QByteArray randomUrlSafe(int bytes)
{
    QByteArray data(bytes, Qt::Uninitialized);
    for (char &c : data)
        c = char(QRandomGenerator::system()->bounded(256));
    return data.toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals);
}

} // namespace

LichessSignIn::LichessSignIn(QObject *parent)
    : QObject(parent)
    , m_server(new QTcpServer(this))
    , m_network(new QNetworkAccessManager(this))
    , m_timeout(new QTimer(this))
{
    m_timeout->setSingleShot(true);
    connect(m_timeout, &QTimer::timeout, this, [this] { finish({}, {}, tr("Signing in took too long.")); });
    connect(m_server, &QTcpServer::newConnection, this, &LichessSignIn::handleConnection);
}

LichessSignIn::~LichessSignIn() = default;

void LichessSignIn::start()
{
    if (!m_server->listen(QHostAddress::LocalHost)) {
        finish({}, {}, m_server->errorString());
        return;
    }
    m_verifier = randomUrlSafe(48);
    m_state = QString::fromLatin1(randomUrlSafe(16));
    m_redirect = QUrl(QStringLiteral("http://127.0.0.1:%1/").arg(m_server->serverPort()));
    const QByteArray challenge = QCryptographicHash::hash(m_verifier, QCryptographicHash::Sha256)
                                     .toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals);

    QUrl url(QStringLiteral("https://lichess.org/oauth"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("response_type"), QStringLiteral("code"));
    query.addQueryItem(QStringLiteral("client_id"), QLatin1String(kClientId));
    query.addQueryItem(QStringLiteral("redirect_uri"), m_redirect.toString());
    query.addQueryItem(QStringLiteral("code_challenge_method"), QStringLiteral("S256"));
    query.addQueryItem(QStringLiteral("code_challenge"), QString::fromLatin1(challenge));
    query.addQueryItem(QStringLiteral("state"), m_state);
    url.setQuery(query);
    m_timeout->start(kTimeoutMs);
    Q_EMIT openBrowser(url);
}

void LichessSignIn::cancel()
{
    m_done = true;
    m_timeout->stop();
    m_server->close();
}

void LichessSignIn::handleConnection()
{
    QTcpSocket *socket = m_server->nextPendingConnection();
    connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
    connect(socket, &QTcpSocket::readyRead, this, [this, socket] {
        const QByteArray request = socket->readAll();
        // "GET /?code=…&state=… HTTP/1.1"
        const QList<QByteArray> requestLine = request.left(request.indexOf('\r')).split(' ');
        const QUrl url(QString::fromLatin1(requestLine.value(1)));
        const QUrlQuery query(url);

        const bool ok = query.queryItemValue(QStringLiteral("state")) == m_state
            && query.hasQueryItem(QStringLiteral("code"));
        const QByteArray page = ok ? tr("Signed in to lichess.org. You can close this page and return to Pragma Chess.").toUtf8()
                                   : tr("Signing in to lichess.org did not succeed. Return to Pragma Chess and try again.").toUtf8();
        socket->write("HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\nConnection: close\r\n\r\n"
                      "<!doctype html><meta charset=utf-8><title>Pragma Chess</title>"
                      "<body style=\"font-family:sans-serif;margin:3em\"><p>" + page + "</p></body>");
        socket->disconnectFromHost();
        if (url.path() != QLatin1String("/"))
            return; // e.g. favicon.ico
        m_server->close();
        if (!ok) {
            const QString error = query.queryItemValue(QStringLiteral("error_description"));
            finish({}, {}, error.isEmpty() ? tr("lichess.org did not grant access.") : error);
            return;
        }
        exchangeCode(query.queryItemValue(QStringLiteral("code")));
    });
}

void LichessSignIn::exchangeCode(const QString &code)
{
    QNetworkRequest request(QUrl(QStringLiteral("https://lichess.org/api/token")));
    request.setHeader(QNetworkRequest::UserAgentHeader, SourceFetch::userAgent());
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/x-www-form-urlencoded"));
    QUrlQuery form;
    form.addQueryItem(QStringLiteral("grant_type"), QStringLiteral("authorization_code"));
    form.addQueryItem(QStringLiteral("code"), code);
    form.addQueryItem(QStringLiteral("code_verifier"), QString::fromLatin1(m_verifier));
    form.addQueryItem(QStringLiteral("redirect_uri"), m_redirect.toString());
    form.addQueryItem(QStringLiteral("client_id"), QLatin1String(kClientId));
    QNetworkReply *reply = m_network->post(request, form.toString(QUrl::FullyEncoded).toUtf8());
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        reply->deleteLater();
        const QString token = QJsonDocument::fromJson(reply->readAll()).object().value(QStringLiteral("access_token")).toString();
        if (reply->error() != QNetworkReply::NoError || token.isEmpty()) {
            finish({}, {}, SourceFetch::httpError(reply, QStringLiteral("lichess.org")));
            return;
        }
        fetchAccount(token);
    });
}

void LichessSignIn::fetchAccount(const QString &token)
{
    QNetworkRequest request(QUrl(QStringLiteral("https://lichess.org/api/account")));
    request.setHeader(QNetworkRequest::UserAgentHeader, SourceFetch::userAgent());
    request.setRawHeader("Authorization", "Bearer " + token.toUtf8());
    QNetworkReply *reply = m_network->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, token] {
        reply->deleteLater();
        const QString username = QJsonDocument::fromJson(reply->readAll()).object().value(QStringLiteral("username")).toString();
        finish(token, username, QString());
    });
}

void LichessSignIn::finish(const QString &token, const QString &username, const QString &errorMessage)
{
    if (m_done)
        return;
    m_done = true;
    m_timeout->stop();
    m_server->close();
    Q_EMIT finished(token, username, errorMessage);
}
