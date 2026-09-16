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

} // namespace SourceCredentials
