#pragma once

#include "RemoteStore.h"

#include <QList>

class QAbstractSocket;
class QSslSocket;
class QTimer;

/// An FTP folder, optionally over explicit TLS (FTPS: AUTH TLS, protected
/// data connections). Passive mode, binary transfers, one operation at a time
/// on one control connection, logged in again if the server drops it.
class FtpStore : public RemoteStore {
    Q_OBJECT

public:
    FtpStore(const QString &host, quint16 port, bool tls, const QString &user, const QString &password,
             const QString &folder, QObject *parent = nullptr);
    ~FtpStore() override;

    QString identity() const override;
    void read(const QString &path, Callback done) override;
    void write(const QString &path, const QByteArray &data, Callback done) override;
    void download(const QString &path, const QString &localFile, Callback done) override;
    void upload(const QString &localFile, const QString &path, Callback done) override;
    void remove(const QString &path, Callback done) override;
    void abort() override;

private:
    using Reply = std::function<void(int code, const QString &text)>;
    using Operation = std::function<void(Callback finish)>;

    /// Queues an operation; it runs once the ones before it finished.
    void enqueue(Operation operation, Callback done);
    void runNext();

    void connectAndLogIn(std::function<void(const QString &error)> done);
    /// Sends a command; `reply` gets the next complete reply.
    void command(const QByteArray &line, Reply reply);
    void expectReply(Reply reply);
    void readControl();
    void failAll(const QString &error);

    /// Opens a passive data connection.
    void openData(std::function<void(QSslSocket *data, const QString &error)> done);
    /// RETR into a sink; STOR from a source.
    void retrieve(const QString &path, std::function<void(const QByteArray &)> sink, Callback done);
    void store(const QString &path, std::function<QByteArray(qint64 maxSize)> source, Callback done);
    /// Uploads with a temporary name then renames over `path`.
    void storeAndRename(const QString &path, std::function<QByteArray(qint64)> source, Callback done);
    void makeFolders(const QString &path, std::function<void()> done);

    QString remotePath(const QString &path) const;
    QByteArray encodePath(const QString &path) const;

    QString m_host;
    quint16 m_port;
    bool m_tls;
    QString m_user;
    QString m_password;
    QString m_folder;

    QSslSocket *m_control;
    QByteArray m_buffer;
    QList<Reply> m_waiting;
    bool m_loggedIn = false;

    QList<std::pair<Operation, Callback>> m_queue;
    bool m_running = false;
    /// Callback of the running operation, to fail it if the connection breaks.
    Callback m_current;
    QTimer *m_watchdog;
    int m_generation = 0;
};
