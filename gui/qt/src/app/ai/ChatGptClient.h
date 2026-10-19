#pragma once

#include "ChatGptAuth.h"
#include "ChatGptResponses.h"

#include <QNetworkRequest>
#include <QObject>
#include <functional>

class QNetworkAccessManager;
class QNetworkReply;

/// Asks ChatGPT on the user's plan: renews the access token when it is
/// about to expire (the refresh token rotates: the new credentials are
/// handed back to be kept), finds the model the plan offers, and streams
/// the answer. One question at a time.
class ChatGptClient : public QObject {
    Q_OBJECT

public:
    explicit ChatGptClient(QObject *parent = nullptr);

    void setCredentials(const ChatGptAuth::Credentials &credentials) { m_credentials = credentials; }
    bool isBusy() const { return m_reply != nullptr || m_preparing; }
    void ask(const QString &instructions, const QList<ChatGptResponses::Turn> &turns);
    void cancel();

Q_SIGNALS:
    /// A piece of the answer.
    void answerDelta(const QString &text);
    void answerCompleted();
    void failed(const QString &message);
    /// The tokens were renewed: keep them.
    void credentialsChanged(const ChatGptAuth::Credentials &credentials);
    /// ChatGPT no longer honours the tokens: the connection is gone, sign in again.
    void signedOut();

private:
    /// Runs `then` with a token good for a while, refreshing it first when needed.
    void withToken(const std::function<void()> &then);
    void withModel(const std::function<void()> &then);
    void send();
    QNetworkRequest apiRequest(const QUrl &url) const;

    QNetworkAccessManager *m_network;
    ChatGptAuth::Credentials m_credentials;
    QString m_model;
    QString m_instructions;
    QList<ChatGptResponses::Turn> m_turns;
    QNetworkReply *m_reply = nullptr;
    ChatGptResponses::StreamReader m_reader;
    bool m_preparing = false;
    bool m_finished = false;
};
