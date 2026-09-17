#include "SourceCatalog.h"

#include "ChessComFetch.h"
#include "LichessFetch.h"
#include "TorneiOnlineFetch.h"

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
        {QStringLiteral("torneionline"), QStringLiteral("torneionline.com"),
         tr("Tournament games of a player rated in Italy, by FIDE or FSI ID: players, round and result, "
            "without moves."),
         false, true},
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
    const QString name = sourceKind ? sourceKind->name : source.kind;
    if (sourceKind && sourceKind->playerId) {
        const QString idType =
            source.settings.value(QLatin1String(TorneiOnlineSettings::idType)).toString() == QLatin1String("fsi")
                ? QStringLiteral("FSI")
                : QStringLiteral("FIDE");
        const QString player = source.settings.value(QLatin1String(TorneiOnlineSettings::player)).toString();
        const QString id = QStringLiteral("%1 %2").arg(idType, source.account);
        return player.isEmpty() ? QStringLiteral("%1 · %2").arg(name, id)
                                : QStringLiteral("%1 · %2 (%3)").arg(name, player, id);
    }
    return QStringLiteral("%1 · %2").arg(name, source.account);
}

QNetworkRequest accountRequest(const GameSource &source)
{
    const QString &kind = source.kind;
    const QString encoded = QString::fromLatin1(QUrl::toPercentEncoding(source.account.trimmed()));
    QUrl url;
    if (kind == QLatin1String("torneionline"))
        url = TorneiOnlineFetch::searchUrl(source.settings.value(QLatin1String(TorneiOnlineSettings::idType)).toString(),
                                           source.account);
    else if (kind == QLatin1String("lichess"))
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
    if (source.kind == QLatin1String("torneionline"))
        return new TorneiOnlineFetch(source, network, parent);
    return nullptr;
}

} // namespace SourceCatalog
