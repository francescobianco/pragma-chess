#include "LobbyIdentity.h"

#include <QSettings>

namespace {

const QString kSetting = QStringLiteral("lobby/key");

std::optional<NostrKey> parse(const QString &text)
{
    const QByteArray secret = QByteArray::fromHex(text.trimmed().toLatin1());
    if (secret.size() != 32 || text.trimmed().size() != 64)
        return std::nullopt;
    return NostrKey::fromSecret(secret);
}

} // namespace

namespace LobbyIdentity {

NostrKey key()
{
    QSettings settings;
    if (const std::optional<NostrKey> kept = parse(settings.value(kSetting).toString()))
        return *kept;
    const NostrKey made = NostrKey::generate();
    settings.setValue(kSetting, QString::fromLatin1(made.secret().toHex()));
    return made;
}

QString secretText()
{
    return QString::fromLatin1(key().secret().toHex());
}

bool isValidSecretText(const QString &text)
{
    return parse(text).has_value();
}

bool setSecretText(const QString &text)
{
    const std::optional<NostrKey> key = parse(text);
    if (!key)
        return false;
    QSettings().setValue(kSetting, QString::fromLatin1(key->secret().toHex()));
    return true;
}

} // namespace LobbyIdentity
