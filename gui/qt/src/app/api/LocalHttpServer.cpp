#include "LocalHttpServer.h"

#include <QHostAddress>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QUrl>
#include <QUrlQuery>

namespace {

/// Requests larger than this are refused: the API takes lines of moves, not files.
constexpr qsizetype kMaxRequest = 1024 * 1024;

QByteArray reason(int status)
{
    switch (status) {
    case 200: return "OK";
    case 400: return "Bad Request";
    case 401: return "Unauthorized";
    case 404: return "Not Found";
    case 405: return "Method Not Allowed";
    case 409: return "Conflict";
    case 413: return "Payload Too Large";
    default: return "Error";
    }
}

} // namespace

LocalHttpServer::LocalHttpServer(QObject *parent)
    : QObject(parent)
    , m_server(new QTcpServer(this))
{
    connect(m_server, &QTcpServer::newConnection, this, [this] {
        while (QTcpSocket *socket = m_server->nextPendingConnection()) {
            connect(socket, &QTcpSocket::readyRead, this, [this, socket] { read(socket); });
            connect(socket, &QTcpSocket::disconnected, this, [this, socket] {
                m_buffers.remove(socket);
                socket->deleteLater();
            });
        }
    });
}

LocalHttpServer::~LocalHttpServer() = default;

void LocalHttpServer::route(const QString &method, const QString &path, Handler handler)
{
    m_routes.insert(method.toUpper() + QLatin1Char(' ') + path, std::move(handler));
}

bool LocalHttpServer::start(quint16 port, const QByteArray &token)
{
    stop();
    m_token = token;
    return m_server->listen(QHostAddress::LocalHost, port);
}

void LocalHttpServer::stop()
{
    m_server->close();
}

bool LocalHttpServer::isListening() const
{
    return m_server->isListening();
}

quint16 LocalHttpServer::port() const
{
    return m_server->serverPort();
}

QStringList LocalHttpServer::routes() const
{
    QStringList keys = m_routes.keys();
    keys.sort();
    return keys;
}

LocalHttpServer::Response LocalHttpServer::error(int status, const QString &message)
{
    return {status, "application/json", QJsonDocument(QJsonObject{{QStringLiteral("error"), message}}).toJson()};
}

void LocalHttpServer::read(QTcpSocket *socket)
{
    QByteArray &buffer = m_buffers[socket];
    buffer += socket->readAll();
    if (buffer.size() > kMaxRequest) {
        m_buffers.remove(socket);
        send(socket, error(413, QStringLiteral("request too large")));
        return;
    }
    const qsizetype headerEnd = buffer.indexOf("\r\n\r\n");
    if (headerEnd < 0)
        return; // The headers are not all there yet.

    Request request;
    const QList<QByteArray> lines = buffer.left(headerEnd).split('\n');
    const QList<QByteArray> start = lines.value(0).trimmed().split(' ');
    if (start.size() < 2) {
        m_buffers.remove(socket);
        send(socket, error(400, QStringLiteral("not an HTTP request")));
        return;
    }
    request.method = QString::fromLatin1(start.at(0)).toUpper();
    const QUrl url(QString::fromUtf8(start.at(1)));
    request.path = url.path();
    for (const auto &[key, value] : QUrlQuery(url).queryItems(QUrl::FullyDecoded))
        request.query.insert(key, value);
    for (qsizetype i = 1; i < lines.size(); ++i) {
        const QByteArray line = lines.at(i).trimmed();
        const qsizetype colon = line.indexOf(':');
        if (colon > 0)
            request.headers.insert(QString::fromLatin1(line.left(colon)).toLower(),
                                   QString::fromUtf8(line.mid(colon + 1).trimmed()));
    }
    const qsizetype length = request.headers.value(QStringLiteral("content-length")).toLongLong();
    if (buffer.size() - headerEnd - 4 < length)
        return; // The body is not all there yet.
    request.body = buffer.mid(headerEnd + 4, length);
    m_buffers.remove(socket); // One request per connection.
    send(socket, answer(request));
}

LocalHttpServer::Response LocalHttpServer::answer(const Request &request) const
{
    const QString authorization = request.headers.value(QStringLiteral("authorization"));
    const QString given = authorization.startsWith(QLatin1String("Bearer "))
        ? authorization.mid(7).trimmed()
        : request.query.value(QStringLiteral("token"));
    if (!m_token.isEmpty() && given.toUtf8() != m_token)
        return error(401, QStringLiteral("a token is needed"));
    const auto handler = m_routes.constFind(request.method + QLatin1Char(' ') + request.path);
    if (handler != m_routes.cend())
        return (*handler)(request);
    for (const QString &key : m_routes.keys()) {
        if (key.section(QLatin1Char(' '), 1) == request.path)
            return error(405, QStringLiteral("%1 is not answered on %2").arg(request.method, request.path));
    }
    return error(404, QStringLiteral("no such path: %1").arg(request.path));
}

void LocalHttpServer::send(QTcpSocket *socket, const Response &response)
{
    QByteArray head = "HTTP/1.1 " + QByteArray::number(response.status) + ' ' + reason(response.status) + "\r\n";
    head += "Content-Type: " + response.contentType + "\r\n";
    head += "Content-Length: " + QByteArray::number(response.body.size()) + "\r\n";
    head += "Connection: close\r\n\r\n";
    socket->write(head + response.body);
    socket->disconnectFromHost();
}
