#include "ChatGptResponses.h"

#include <QJsonArray>
#include <QJsonDocument>

namespace ChatGptResponses {

QJsonObject requestBody(const QString &model, const QString &instructions, const QList<Turn> &turns)
{
    QJsonArray input;
    for (const Turn &turn : turns)
        input.append(QJsonObject{{QStringLiteral("role"), turn.fromUser ? QStringLiteral("user") : QStringLiteral("assistant")},
                                 {QStringLiteral("content"), turn.text}});
    // Streamed and not stored, as OpenAI asks of requests on a user's plan.
    return QJsonObject{{QStringLiteral("model"), model},
                       {QStringLiteral("instructions"), instructions},
                       {QStringLiteral("input"), input},
                       {QStringLiteral("stream"), true},
                       {QStringLiteral("store"), false}};
}

QString pickModel(const QJsonObject &models)
{
    for (const QJsonValue &value : models.value(QStringLiteral("data")).toArray()) {
        const QJsonObject model = value.toObject();
        const QString visibility = model.value(QStringLiteral("visibility")).toString();
        if (!visibility.isEmpty() && visibility != QLatin1String("list"))
            continue;
        const QString slug = model.value(QStringLiteral("slug")).toString();
        const QString id = slug.isEmpty() ? model.value(QStringLiteral("id")).toString() : slug;
        if (!id.isEmpty())
            return id;
    }
    return {};
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
