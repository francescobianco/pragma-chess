#include "NostrEvent.h"

#include "NostrKey.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QJsonArray>

namespace {

// NIP-01: escape only these characters, everything else is raw UTF-8.
void appendJsonString(QByteArray &out, const QString &text)
{
    out.append('"');
    for (const char c : text.toUtf8()) {
        switch (c) {
        case '\n': out.append("\\n"); break;
        case '"': out.append("\\\""); break;
        case '\\': out.append("\\\\"); break;
        case '\r': out.append("\\r"); break;
        case '\t': out.append("\\t"); break;
        case '\b': out.append("\\b"); break;
        case '\f': out.append("\\f"); break;
        default: out.append(c); break;
        }
    }
    out.append('"');
}

bool isHex(const QString &text, int length)
{
    if (text.size() != length)
        return false;
    for (const QChar c : text) {
        if (!((c >= QLatin1Char('0') && c <= QLatin1Char('9')) || (c >= QLatin1Char('a') && c <= QLatin1Char('f'))))
            return false;
    }
    return true;
}

} // namespace

NostrEvent NostrEvent::create(const NostrKey &key, int kind, const QList<QStringList> &tags, const QString &content,
                              qint64 createdAt)
{
    NostrEvent event;
    event.pubkey = key.publicKeyHex();
    event.createdAt = createdAt > 0 ? createdAt : QDateTime::currentSecsSinceEpoch();
    event.kind = kind;
    event.tags = tags;
    event.content = content;
    event.id = event.computeId();
    event.sig = QString::fromLatin1(key.sign(QByteArray::fromHex(event.id.toLatin1())).toHex());
    return event;
}

QByteArray NostrEvent::serializeForId() const
{
    QByteArray out = "[0,";
    appendJsonString(out, pubkey);
    out.append(',').append(QByteArray::number(createdAt)).append(',').append(QByteArray::number(kind)).append(",[");
    for (qsizetype i = 0; i < tags.size(); ++i) {
        if (i > 0)
            out.append(',');
        out.append('[');
        for (qsizetype j = 0; j < tags.at(i).size(); ++j) {
            if (j > 0)
                out.append(',');
            appendJsonString(out, tags.at(i).at(j));
        }
        out.append(']');
    }
    out.append("],");
    appendJsonString(out, content);
    out.append(']');
    return out;
}

QString NostrEvent::computeId() const
{
    return QString::fromLatin1(QCryptographicHash::hash(serializeForId(), QCryptographicHash::Sha256).toHex());
}

bool NostrEvent::verify() const
{
    if (!isHex(id, 64) || !isHex(pubkey, 64) || !isHex(sig, 128) || computeId() != id)
        return false;
    return NostrKey::verify(QByteArray::fromHex(pubkey.toLatin1()), QByteArray::fromHex(id.toLatin1()),
                            QByteArray::fromHex(sig.toLatin1()));
}

QString NostrEvent::tagValue(const QString &name) const
{
    for (const QStringList &tag : tags) {
        if (tag.size() >= 2 && tag.at(0) == name)
            return tag.at(1);
    }
    return {};
}

QJsonObject NostrEvent::toJson() const
{
    QJsonArray tagArray;
    for (const QStringList &tag : tags)
        tagArray.append(QJsonArray::fromStringList(tag));
    return {
        {QStringLiteral("id"), id},
        {QStringLiteral("pubkey"), pubkey},
        {QStringLiteral("created_at"), createdAt},
        {QStringLiteral("kind"), kind},
        {QStringLiteral("tags"), tagArray},
        {QStringLiteral("content"), content},
        {QStringLiteral("sig"), sig},
    };
}

std::optional<NostrEvent> NostrEvent::fromJson(const QJsonObject &object)
{
    const QJsonValue createdAt = object.value(QStringLiteral("created_at"));
    const QJsonValue kind = object.value(QStringLiteral("kind"));
    if (!object.value(QStringLiteral("id")).isString() || !object.value(QStringLiteral("pubkey")).isString()
        || !createdAt.isDouble() || !kind.isDouble() || !object.value(QStringLiteral("tags")).isArray()
        || !object.value(QStringLiteral("content")).isString() || !object.value(QStringLiteral("sig")).isString())
        return std::nullopt;
    NostrEvent event;
    event.id = object.value(QStringLiteral("id")).toString();
    event.pubkey = object.value(QStringLiteral("pubkey")).toString();
    event.createdAt = qint64(createdAt.toDouble());
    event.kind = kind.toInt();
    event.content = object.value(QStringLiteral("content")).toString();
    event.sig = object.value(QStringLiteral("sig")).toString();
    for (const QJsonValue &tag : object.value(QStringLiteral("tags")).toArray()) {
        QStringList values;
        for (const QJsonValue &value : tag.toArray())
            values << value.toString();
        event.tags << values;
    }
    return event;
}
