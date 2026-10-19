#pragma once

#include <QByteArray>
#include <QJsonArray>
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

/// A message as an item of the request's input.
QJsonObject messageItem(const Turn &turn);
/// What a tool answered to the call `callId`.
QJsonObject toolOutputItem(const QString &callId, const QJsonObject &output);
/// An item the answer brought, as it goes back into the next request's
/// input: requests are not stored, so the conversation travels whole. A
/// function call keeps its call id, name and arguments; a message becomes
/// the assistant's text; reasoning stays behind (empty object).
QJsonObject replayItem(const QJsonObject &item);
/// The text of a message item, empty for other items.
QString messageText(const QJsonObject &item);

/// The request: `instructions` say what the AI is for, `input` the
/// conversation so far as items, `tools` the functions it may call.
QJsonObject requestBody(const QString &model, const QString &instructions, const QJsonArray &input,
                        const QJsonArray &tools = {});
/// A function the AI may call, its parameters a JSON schema.
QJsonObject functionTool(const QString &name, const QString &description, const QJsonObject &parameters);

/// A model the plan offers: its id for requests, its name for people.
struct Model {
    QString slug;
    QString name;
};
/// The models the plan lists (`models`: `visibility` "list", `slug`,
/// `display_name`, best first by `priority`; or the `id`s of a plain
/// OpenAI list, `data`).
QList<Model> listedModels(const QJsonObject &models);

/// The model to ask: the first listedModels gives, empty when there is none.
QString pickModel(const QJsonObject &models);

/// What the stream says.
struct Event {
    enum class Kind { Delta, ItemDone, Completed, Failed, Other };
    Kind kind = Kind::Other;
    /// Delta: the text that came.
    QString text;
    /// ItemDone: an item of the answer, whole (a message, a function call).
    QJsonObject item;
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
