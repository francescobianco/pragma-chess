#include "SourceFetch.h"

#include <QNetworkReply>

QByteArray SourceFetch::userAgent()
{
    return QByteArrayLiteral("PragmaChess/" APP_VERSION " (+https://github.com/francescobianco/pragma-chess)");
}

QString SourceFetch::httpError(QNetworkReply *reply, const QString &site)
{
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    switch (status) {
    case 401:
    case 403:
        return QObject::tr("%1 refused access: sign in again.").arg(site);
    case 404:
        return QObject::tr("%1 did not find the account or its games.").arg(site);
    case 429:
        return QObject::tr("%1 asked to slow down; syncing again later.").arg(site);
    default:
        break;
    }
    if (status >= 500)
        return QObject::tr("%1 is not available right now (%2).").arg(site).arg(status);
    return QObject::tr("Could not reach %1: %2").arg(site, reply->errorString());
}
