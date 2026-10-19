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
/// the answer. One request at a time.
class ChatGptClient : public QObject {
    Q_OBJECT

public:
    explicit ChatGptClient(QObject *parent = nullptr);

    void setCredentials(const ChatGptAuth::Credentials &credentials) { m_credentials = credentials; }
    bool isBusy() const { return m_reply != nullptr || m_preparing; }
    /// Asks with the conversation as items and the functions the AI may call.
    void ask(const QString &instructions, const QJsonArray &input, const QJsonArray &tools = {});
    void cancel();
    /// The model asked; empty for the first the plan lists.
    void setModel(const QString &slug)
    {
        if (slug != m_chosenModel)
            m_model.clear(); // Found again: the chosen one, or the plan's first.
        m_chosenModel = slug;
    }
    /// Asks the plan for its models: modelsListed, or failed.
    void listModels();

Q_SIGNALS:
    /// A piece of the answer.
    void answerDelta(const QString &text);
    void modelsListed(const QList<ChatGptResponses::Model> &models);
    /// The answer is whole: every item it brought (messages, function calls).
    void answerCompleted(const QJsonArray &items);
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
    QString m_chosenModel;
    QString m_instructions;
    QJsonArray m_input;
    QJsonArray m_tools;
    QJsonArray m_items;
    QNetworkReply *m_reply = nullptr;
    ChatGptResponses::StreamReader m_reader;
    bool m_preparing = false;
    bool m_finished = false;
    /// The access token was renewed after a refusal (401), for this request.
    bool m_renewedAfterRefusal = false;
};
