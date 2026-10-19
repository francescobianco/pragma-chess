#pragma once

#include <QByteArray>
#include <QJsonObject>
#include <QList>
#include <QString>

/// The pure half of asking ChatGPT through OpenAI's Responses API with the
/// user's plan (POST https://api.openai.com/v1/responses, streamed, not
/// stored): the request, the stream's events, the model to ask.
namespace ChatGptResponses {

inline constexpr char kResponsesUrl[] = "https://api.openai.com/v1/responses";
inline constexpr char kModelsUrl[] = "https://api.openai.com/v1/models";

/// One message of the conversation.
struct Turn {
    bool fromUser = true;
    QString text;
};

/// The request: `instructions` say what the AI is for, `turns` the
/// conversation so far, the last one the question.
QJsonObject requestBody(const QString &model, const QString &instructions, const QList<Turn> &turns);

/// The model to ask among those the plan lists (`visibility` "list"; its
/// `slug`, or the `id` of a plain model list), empty when there is none.
QString pickModel(const QJsonObject &models);

/// What the stream says.
struct Event {
    enum class Kind { Delta, Completed, Failed, Other };
    Kind kind = Kind::Other;
    /// Delta: the text that came.
    QString text;
    /// Failed: OpenAI's code ("subscription_sharing_usage_limit_exceeded") and message.
    QString code;
    QString message;
};

/// Reads the server-sent events of a streamed response as they arrive,
/// in pieces cut anywhere.
class StreamReader {
public:
    QList<Event> feed(const QByteArray &bytes);

private:
    QByteArray m_pending;
};

/// An error code that means the tokens are no longer good: sign in again.
bool isSignedOut(const QString &code);

} // namespace ChatGptResponses
