#pragma once

#include <QJsonObject>
#include <QList>
#include <QString>
#include <QStringList>

#include <optional>

class NostrKey;

/// A Nostr event (NIP-01). Ids, keys and signatures are lowercase hex, as on
/// the wire.
struct NostrEvent {
    QString id;
    QString pubkey;
    qint64 createdAt = 0;
    int kind = 0;
    QList<QStringList> tags;
    QString content;
    QString sig;

    /// A new event signed by `key`, stamped now unless `createdAt` is given.
    static NostrEvent create(const NostrKey &key, int kind, const QList<QStringList> &tags, const QString &content,
                             qint64 createdAt = 0);

    /// The canonical serialization `[0,pubkey,created_at,kind,tags,content]`
    /// whose SHA-256 is the id (NIP-01 escaping rules).
    QByteArray serializeForId() const;
    /// The id computed from the content, in hex.
    QString computeId() const;
    /// Whether the id matches the content and the signature is valid.
    bool verify() const;

    /// The first value of a tag, e.g. tagValue("p").
    QString tagValue(const QString &name) const;

    QJsonObject toJson() const;
    /// The event of a JSON object, if it has the fields of an event.
    static std::optional<NostrEvent> fromJson(const QJsonObject &object);
};
