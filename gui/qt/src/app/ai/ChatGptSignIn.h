#pragma once

#include "ChatGptAuth.h"

#include <QObject>
#include <QUrl>

class QNetworkAccessManager;
class QTcpServer;
class QTimer;

/// Signs in with ChatGPT (authorization code with PKCE): the browser shows
/// OpenAI's own sign-in page and comes back to 127.0.0.1, where this object
/// listens; the code is then exchanged for the tokens. As LichessSignIn.
class ChatGptSignIn : public QObject {
    Q_OBJECT

public:
    explicit ChatGptSignIn(QObject *parent = nullptr);
    ~ChatGptSignIn() override;

    /// Starts listening and emits openBrowser() with the sign-in page, for
    /// `credentials` (this computer's id, the client id issued before).
    void start(const ChatGptAuth::Credentials &credentials);
    void cancel();

Q_SIGNALS:
    void openBrowser(const QUrl &url);
    /// On success `credentials` are signed in, otherwise `errorMessage` says why.
    void finished(const ChatGptAuth::Credentials &credentials, const QString &errorMessage);

private:
    void handleConnection();
    void exchangeCode(const QString &code);
    void finish(const QString &errorMessage);

    QTcpServer *m_server;
    QNetworkAccessManager *m_network;
    QTimer *m_timeout;
    ChatGptAuth::Credentials m_credentials;
    QByteArray m_verifier;
    QString m_state;
    QUrl m_redirect;
    bool m_done = false;
};
