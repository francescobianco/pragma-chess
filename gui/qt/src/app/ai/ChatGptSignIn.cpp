#include "ChatGptSignIn.h"

#include "app/sources/SourceFetch.h"

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

constexpr int kTimeoutMs = 5 * 60 * 1000;

QByteArray randomUrlSafe(int bytes)
{
    QByteArray data(bytes, Qt::Uninitialized);
    for (char &c : data)
        c = char(QRandomGenerator::system()->bounded(256));
    return data.toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals);
}

} // namespace

ChatGptSignIn::ChatGptSignIn(QObject *parent)
    : QObject(parent)
    , m_server(new QTcpServer(this))
    , m_network(new QNetworkAccessManager(this))
    , m_timeout(new QTimer(this))
{
    m_timeout->setSingleShot(true);
    connect(m_timeout, &QTimer::timeout, this, [this] { finish(tr("Signing in took too long.")); });
    connect(m_server, &QTcpServer::newConnection, this, &ChatGptSignIn::handleConnection);
}

ChatGptSignIn::~ChatGptSignIn() = default;

void ChatGptSignIn::start(const ChatGptAuth::Credentials &credentials)
{
    m_credentials = credentials;
    // OpenAI's port when it is free, any other otherwise.
    if (!m_server->listen(QHostAddress::LocalHost, ChatGptAuth::kCallbackPort)
        && !m_server->listen(QHostAddress::LocalHost)) {
        finish(m_server->errorString());
        return;
    }
    m_verifier = randomUrlSafe(48);
    m_state = QString::fromLatin1(randomUrlSafe(16));
    // 127.0.0.1, never "localhost": OpenAI compares the address as written.
    m_redirect = QUrl(QStringLiteral("http://127.0.0.1:%1/auth/callback").arg(m_server->serverPort()));
    const QByteArray challenge = QCryptographicHash::hash(m_verifier, QCryptographicHash::Sha256)
                                     .toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals);
    m_timeout->start(kTimeoutMs);
    Q_EMIT openBrowser(ChatGptAuth::authorizeUrl(m_credentials, m_redirect, m_state,
                                                 QString::fromLatin1(randomUrlSafe(16)), challenge,
                                                 QStringLiteral("Pragma Chess")));
}

void ChatGptSignIn::cancel()
{
    m_done = true;
    m_timeout->stop();
    m_server->close();
}

void ChatGptSignIn::handleConnection()
{
    QTcpSocket *socket = m_server->nextPendingConnection();
    connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
    connect(socket, &QTcpSocket::readyRead, this, [this, socket] {
        const QByteArray request = socket->readAll();
        // "GET /auth/callback?code=…&state=… HTTP/1.1"
        const QList<QByteArray> requestLine = request.left(request.indexOf('\r')).split(' ');
        const QUrl url(QString::fromLatin1(requestLine.value(1)));
        const QUrlQuery query(url);
        const bool callback = url.path() == QLatin1String("/auth/callback");
        const bool stateOk = query.queryItemValue(QStringLiteral("state")) == m_state;
        const QString issued = query.queryItemValue(QStringLiteral("client_id"));
        // A sign-in again may leave the client id out; one that differs is not ours.
        const bool clientOk = issued.isEmpty() || m_credentials.clientId.isEmpty() || issued == m_credentials.clientId;
        const bool ok = callback && stateOk && clientOk && query.hasQueryItem(QStringLiteral("code"));
        const QByteArray page = ok ? tr("Signed in with ChatGPT. You can close this page and return to Pragma Chess.").toUtf8()
                                   : tr("Signing in with ChatGPT did not succeed. Return to Pragma Chess and try again.").toUtf8();
        socket->write("HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\nConnection: close\r\n\r\n"
                      "<!doctype html><meta charset=utf-8><title>Pragma Chess</title>"
                      "<body style=\"font-family:sans-serif;margin:3em\"><p>" + page + "</p></body>");
        socket->disconnectFromHost();
        if (!callback)
            return; // e.g. favicon.ico
        m_server->close();
        if (!ok) {
            const QString error = query.queryItemValue(QStringLiteral("error_description"));
            finish(error.isEmpty() ? tr("ChatGPT did not grant access.") : error);
            return;
        }
        if (m_credentials.clientId.isEmpty())
            m_credentials.clientId = issued;
        if (m_credentials.clientId.isEmpty()) {
            finish(tr("ChatGPT did not register Pragma Chess."));
            return;
        }
        exchangeCode(query.queryItemValue(QStringLiteral("code")));
    });
}

void ChatGptSignIn::exchangeCode(const QString &code)
{
    QNetworkRequest request(QUrl(QString::fromLatin1(ChatGptAuth::kTokenUrl)));
    request.setHeader(QNetworkRequest::UserAgentHeader, SourceFetch::userAgent());
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/x-www-form-urlencoded"));
    QUrlQuery form;
    form.addQueryItem(QStringLiteral("grant_type"), QStringLiteral("authorization_code"));
    form.addQueryItem(QStringLiteral("client_id"), m_credentials.clientId);
    form.addQueryItem(QStringLiteral("code"), code);
    form.addQueryItem(QStringLiteral("code_verifier"), QString::fromLatin1(m_verifier));
    form.addQueryItem(QStringLiteral("redirect_uri"), m_redirect.toString());
    form.addQueryItem(QStringLiteral("resource"), QString::fromLatin1(ChatGptAuth::kResource));
    QByteArray body;
    for (const auto &[key, value] : form.queryItems(QUrl::FullyDecoded)) {
        if (!body.isEmpty())
            body += '&';
        body += QUrl::toPercentEncoding(key) + '=' + QUrl::toPercentEncoding(value);
    }
    QNetworkReply *reply = m_network->post(request, body);
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        reply->deleteLater();
        const QJsonObject response = QJsonDocument::fromJson(reply->readAll()).object();
        QString error;
        if (!ChatGptAuth::applyTokenResponse(m_credentials, response, QDateTime::currentDateTimeUtc(), &error)) {
            finish(error.isEmpty() ? SourceFetch::httpError(reply, QStringLiteral("ChatGPT")) : error);
            return;
        }
        finish(QString());
    });
}

void ChatGptSignIn::finish(const QString &errorMessage)
{
    if (m_done)
        return;
    m_done = true;
    m_timeout->stop();
    m_server->close();
    Q_EMIT finished(m_credentials, errorMessage);
}
