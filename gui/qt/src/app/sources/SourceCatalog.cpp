#include "SourceCatalog.h"

#include "ChessComFetch.h"
#include "LichessFetch.h"

#include <QCoreApplication>
#include <QUrl>

namespace {

QString tr(const char *text)
{
    return QCoreApplication::translate("SourceCatalog", text);
}

} // namespace

namespace SourceCatalog {

QList<SourceKind> kinds()
{
    return {
        {QStringLiteral("lichess"), QStringLiteral("lichess.org"),
         tr("Games played on lichess.org by an account. Needs signing in to lichess.org."), true},
        {QStringLiteral("chesscom"), QStringLiteral("chess.com"),
         tr("Games played on chess.com by an account, from its public archives."), false},
    };
}

std::optional<SourceKind> kind(const QString &id)
{
    for (const SourceKind &kind : kinds()) {
        if (kind.id == id)
            return kind;
    }
    return std::nullopt;
}

QString displayName(const GameSource &source)
{
    const std::optional<SourceKind> sourceKind = kind(source.kind);
    return QStringLiteral("%1 · %2").arg(sourceKind ? sourceKind->name : source.kind, source.account);
}

QNetworkRequest accountRequest(const QString &kind, const QString &account)
{
    const QString encoded = QString::fromLatin1(QUrl::toPercentEncoding(account.trimmed()));
    QUrl url;
    if (kind == QLatin1String("lichess"))
        url = QUrl(QStringLiteral("https://lichess.org/api/user/%1").arg(encoded));
    else if (kind == QLatin1String("chesscom"))
        url = QUrl(QStringLiteral("https://api.chess.com/pub/player/%1").arg(encoded.toLower()));
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, SourceFetch::userAgent());
    return request;
}

SourceFetch *createFetch(const GameSource &source, QNetworkAccessManager *network, QObject *parent)
{
    if (source.kind == QLatin1String("lichess"))
        return new LichessFetch(source, network, parent);
    if (source.kind == QLatin1String("chesscom"))
        return new ChessComFetch(source, network, parent);
    return nullptr;
}

} // namespace SourceCatalog
