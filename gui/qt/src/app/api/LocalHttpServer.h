#pragma once

#include <QByteArray>
#include <QHash>
#include <QObject>
#include <QString>

#include <functional>

class QTcpServer;
class QTcpSocket;

/// A small HTTP/1.1 server for this computer only: it listens on 127.0.0.1,
/// answers each request once (Connection: close) and asks every request for
/// a token (`Authorization: Bearer <token>`, or `?token=`), so only a
/// program that can read the token — the user's own — can use it. The
/// routes are callbacks; what they do belongs to the caller (DesktopApi).
class LocalHttpServer : public QObject {
    Q_OBJECT

public:
    struct Request {
        QString method;
        /// The path without the query, e.g. "/api/state".
        QString path;
        QHash<QString, QString> query;
        QHash<QString, QString> headers; // Names lower case.
        QByteArray body;
    };
    struct Response {
        int status = 200;
        QByteArray contentType = "application/json";
        QByteArray body;
    };
    using Handler = std::function<Response(const Request &request)>;

    explicit LocalHttpServer(QObject *parent = nullptr);
    ~LocalHttpServer() override;

    /// Answers `method` on `path` with `handler`.
    void route(const QString &method, const QString &path, Handler handler);
    /// Listens on 127.0.0.1:`port` (0: any free port). False if it cannot.
    bool start(quint16 port, const QByteArray &token);
    void stop();
    bool isListening() const;
    quint16 port() const;
    /// The paths answered, "GET /api/state" and so on, for an index.
    QStringList routes() const;

    /// A JSON error body: {"error": message}.
    static Response error(int status, const QString &message);

private:
    void read(QTcpSocket *socket);
    Response answer(const Request &request) const;
    static void send(QTcpSocket *socket, const Response &response);

    QTcpServer *m_server;
    QByteArray m_token;
    QHash<QString, Handler> m_routes; // "GET /api/state" → handler.
    QHash<QTcpSocket *, QByteArray> m_buffers;
};
