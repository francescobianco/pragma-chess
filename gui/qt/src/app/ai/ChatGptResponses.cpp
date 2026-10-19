#include "ChatGptResponses.h"

#include <QJsonArray>
#include <QJsonDocument>

#include <algorithm>

namespace ChatGptResponses {

QJsonObject messageItem(const Turn &turn)
{
    // The text typed as the plan's requests take it: the user's as input, the assistant's as output.
    const QJsonObject text{{QStringLiteral("type"), turn.fromUser ? QStringLiteral("input_text") : QStringLiteral("output_text")},
                           {QStringLiteral("text"), turn.text}};
    return {{QStringLiteral("role"), turn.fromUser ? QStringLiteral("user") : QStringLiteral("assistant")},
            {QStringLiteral("content"), QJsonArray{text}}};
}

QJsonObject toolOutputItem(const QString &callId, const QJsonObject &output)
{
    return {{QStringLiteral("type"), QStringLiteral("function_call_output")},
            {QStringLiteral("call_id"), callId},
            {QStringLiteral("output"), QString::fromUtf8(QJsonDocument(output).toJson(QJsonDocument::Compact))}};
}

QString messageText(const QJsonObject &item)
{
    if (item.value(QStringLiteral("type")).toString() != QLatin1String("message"))
        return {};
    QString text;
    for (const QJsonValue &part : item.value(QStringLiteral("content")).toArray()) {
        const QJsonObject object = part.toObject();
        if (object.value(QStringLiteral("type")).toString() == QLatin1String("output_text"))
            text += object.value(QStringLiteral("text")).toString();
    }
    return text;
}

QJsonObject replayItem(const QJsonObject &item)
{
    const QString type = item.value(QStringLiteral("type")).toString();
    if (type == QLatin1String("function_call"))
        return {{QStringLiteral("type"), type},
                {QStringLiteral("call_id"), item.value(QStringLiteral("call_id"))},
                {QStringLiteral("name"), item.value(QStringLiteral("name"))},
                {QStringLiteral("arguments"), item.value(QStringLiteral("arguments"))}};
    if (type == QLatin1String("message")) {
        const QString text = messageText(item);
        return text.isEmpty() ? QJsonObject() : messageItem({false, text});
    }
    return {};
}

QJsonObject requestBody(const QString &model, const QString &instructions, const QJsonArray &input, const QJsonArray &tools)
{
    // Streamed and not stored, as OpenAI asks of requests on a user's plan.
    QJsonObject body{{QStringLiteral("model"), model},
                     {QStringLiteral("instructions"), instructions},
                     {QStringLiteral("input"), input},
                     {QStringLiteral("stream"), true},
                     {QStringLiteral("store"), false}};
    if (!tools.isEmpty())
        body.insert(QStringLiteral("tools"), tools);
    return body;
}

QJsonObject functionTool(const QString &name, const QString &description, const QJsonObject &parameters)
{
    return {{QStringLiteral("type"), QStringLiteral("function")},
            {QStringLiteral("name"), name},
            {QStringLiteral("description"), description},
            {QStringLiteral("parameters"), parameters}};
}

QList<Model> listedModels(const QJsonObject &models)
{
    // The plan's list: {"models": [{slug, display_name, visibility, priority}]},
    // best first by priority; a plain OpenAI list ({"data": [{id}]}) otherwise.
    struct Listed {
        Model model;
        int priority;
    };
    QList<Listed> listed;
    for (const QJsonValue &value : models.value(QStringLiteral("models")).toArray()) {
        const QJsonObject model = value.toObject();
        const QString slug = model.value(QStringLiteral("slug")).toString();
        if (slug.isEmpty() || model.value(QStringLiteral("visibility")).toString(QStringLiteral("list")) != QLatin1String("list"))
            continue;
        const QString name = model.value(QStringLiteral("display_name")).toString();
        listed << Listed{{slug, name.isEmpty() ? slug : name}, model.value(QStringLiteral("priority")).toInt(999)};
    }
    std::stable_sort(listed.begin(), listed.end(), [](const Listed &a, const Listed &b) { return a.priority < b.priority; });
    QList<Model> list;
    for (const Listed &entry : std::as_const(listed))
        list << entry.model;
    if (list.isEmpty()) {
        for (const QJsonValue &value : models.value(QStringLiteral("data")).toArray()) {
            const QString id = value.toObject().value(QStringLiteral("id")).toString();
            if (!id.isEmpty())
                list << Model{id, id};
        }
    }
    return list;
}

QString pickModel(const QJsonObject &models)
{
    const QList<Model> list = listedModels(models);
    return list.isEmpty() ? QString() : list.first().slug;
}

QList<Event> StreamReader::feed(const QByteArray &bytes)
{
    m_pending += bytes;
    m_pending.replace("\r\n", "\n");
    QList<Event> events;
    // An event ends at an empty line; its data is on its "data:" lines.
    qsizetype end;
    while ((end = m_pending.indexOf("\n\n")) >= 0) {
        const QByteArray block = m_pending.left(end);
        m_pending.remove(0, end + 2);
        QByteArray data;
        for (const QByteArray &line : block.split('\n')) {
            if (line.startsWith("data:"))
                data += line.mid(5).trimmed();
        }
        if (data.isEmpty() || data == "[DONE]")
            continue;
        const QJsonObject object = QJsonDocument::fromJson(data).object();
        const QString type = object.value(QStringLiteral("type")).toString();
        Event event;
        if (type == QLatin1String("response.output_text.delta")) {
            event.kind = Event::Kind::Delta;
            event.text = object.value(QStringLiteral("delta")).toString();
        } else if (type == QLatin1String("response.output_item.done")) {
            event.kind = Event::Kind::ItemDone;
            event.item = object.value(QStringLiteral("item")).toObject();
        } else if (type == QLatin1String("response.completed")) {
            event.kind = Event::Kind::Completed;
        } else if (type == QLatin1String("response.failed") || type == QLatin1String("error")) {
            event.kind = Event::Kind::Failed;
            QJsonObject error = object.value(QStringLiteral("response")).toObject().value(QStringLiteral("error")).toObject();
            if (error.isEmpty())
                error = object.value(QStringLiteral("error")).toObject();
            if (error.isEmpty())
                error = object;
            event.code = error.value(QStringLiteral("code")).toString();
            event.message = error.value(QStringLiteral("message")).toString();
        }
        events << event;
    }
    return events;
}

bool isSignedOut(const QString &code)
{
    static const QStringList codes{
        QStringLiteral("invalid_grant"),         QStringLiteral("invalid_refresh_token"),
        QStringLiteral("token_expired"),         QStringLiteral("refresh_token_expired"),
        QStringLiteral("refresh_token_invalidated"), QStringLiteral("refresh_token_reused"),
    };
    return codes.contains(code);
}

} // namespace ChatGptResponses
