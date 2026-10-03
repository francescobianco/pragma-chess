#include "SourceCredentials.h"

#include <QSettings>

namespace {

QString key(const QString &sourceUuid)
{
    return QStringLiteral("sourceCredentials/%1/token").arg(sourceUuid);
}

} // namespace

namespace SourceCredentials {

QString token(const QString &sourceUuid)
{
    return QSettings().value(key(sourceUuid)).toString();
}

void setToken(const QString &sourceUuid, const QString &token)
{
    QSettings().setValue(key(sourceUuid), token);
}

void remove(const QString &sourceUuid)
{
    QSettings().remove(QStringLiteral("sourceCredentials/%1").arg(sourceUuid));
}

bool isIgnoredHere(const QString &sourceUuid)
{
    return QSettings().value(QStringLiteral("sourceCredentials/%1/ignoredHere").arg(sourceUuid)).toBool();
}

void setIgnoredHere(const QString &sourceUuid, bool ignored)
{
    const QString key = QStringLiteral("sourceCredentials/%1/ignoredHere").arg(sourceUuid);
    if (ignored)
        QSettings().setValue(key, true);
    else
        QSettings().remove(key);
}

} // namespace SourceCredentials
