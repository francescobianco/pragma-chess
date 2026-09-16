#pragma once

#include <QObject>
#include <QUrl>

class QNetworkAccessManager;
class QTcpServer;
class QTimer;

/// Signs in to lichess.org with OAuth 2 (authorization code with PKCE): the
/// browser shows lichess.org's own sign-in page and returns to a local
/// address this object listens on; the code is then exchanged for a token.
class LichessSignIn : public QObject {
    Q_OBJECT

public:
    explicit LichessSignIn(QObject *parent = nullptr);
    ~LichessSignIn() override;

    /// Starts listening and emits openBrowser() with the sign-in page.
    void start();
    void cancel();

Q_SIGNALS:
    void openBrowser(const QUrl &url);
    /// On success `token` and `username` are set, otherwise `errorMessage`.
    void finished(const QString &token, const QString &username, const QString &errorMessage);

private:
    void handleConnection();
    void exchangeCode(const QString &code);
    void fetchAccount(const QString &token);
    void finish(const QString &token, const QString &username, const QString &errorMessage);

    QTcpServer *m_server;
    QNetworkAccessManager *m_network;
    QTimer *m_timeout;
    QByteArray m_verifier;
    QString m_state;
    QUrl m_redirect;
    bool m_done = false;
};
