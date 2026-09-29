#include "PairingLink.h"

#include <QUrl>
#include <QUrlQuery>

namespace {

std::optional<QByteArray> fromHex32(const QString &text)
{
    if (text.size() != 64)
        return std::nullopt;
    for (const QChar c : text) {
        if (!c.isDigit() && !(c.toLower() >= QLatin1Char('a') && c.toLower() <= QLatin1Char('f')))
            return std::nullopt;
    }
    return QByteArray::fromHex(text.toLatin1());
}

} // namespace

QString PairingLink::toString() const
{
    // Every value is percent-encoded, so the link survives any QR reader.
    const auto item = [](const char *name, const QString &value) {
        return QString::fromLatin1(name) + QLatin1Char('=') + QString::fromLatin1(QUrl::toPercentEncoding(value));
    };
    QStringList items{item("k", QString::fromLatin1(computerKey.toHex())),
                      item("s", QString::fromLatin1(secret.toHex())), item("n", computerName)};
    for (const QString &relay : relays)
        items << item("r", relay);
    return QStringLiteral("pragma-chess://pair?") + items.join(QLatin1Char('&'));
}

std::optional<PairingLink> PairingLink::parse(const QString &text)
{
    const QUrl url(text.trimmed());
    if (url.scheme() != QLatin1String("pragma-chess") || url.host() != QLatin1String("pair"))
        return std::nullopt;
    const QUrlQuery query(url);
    const std::optional<QByteArray> key = fromHex32(query.queryItemValue(QStringLiteral("k")));
    const std::optional<QByteArray> secret = fromHex32(query.queryItemValue(QStringLiteral("s")));
    if (!key || !secret)
        return std::nullopt;
    PairingLink link;
    link.computerKey = *key;
    link.secret = *secret;
    link.computerName = query.queryItemValue(QStringLiteral("n"), QUrl::FullyDecoded);
    for (const QString &relay : query.allQueryItemValues(QStringLiteral("r"), QUrl::FullyDecoded)) {
        if (relay.startsWith(QLatin1String("wss://")) || relay.startsWith(QLatin1String("ws://")))
            link.relays << relay;
    }
    if (link.relays.isEmpty())
        return std::nullopt;
    return link;
}
