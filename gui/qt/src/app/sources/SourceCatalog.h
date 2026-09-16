#pragma once

#include "GameSource.h"

#include <QList>
#include <QNetworkRequest>
#include <QString>

#include <optional>

class QNetworkAccessManager;
class QObject;
class SourceFetch;

/// A kind of source that can be connected to a database.
struct SourceKind {
    QString id;
    /// "lichess.org"
    QString name;
    QString description;
    /// Whether the source needs signing in (OAuth) to download games.
    bool needsSignIn = false;
};

/// The kinds of sources Pragma Chess can sync with.
namespace SourceCatalog {

QList<SourceKind> kinds();
std::optional<SourceKind> kind(const QString &id);

/// "lichess.org · DrNykterstein"
QString displayName(const GameSource &source);

/// A request answered with 200 when `account` exists on the site.
QNetworkRequest accountRequest(const QString &kind, const QString &account);

/// The fetch that downloads the new games of `source`, or nullptr for an unknown kind.
SourceFetch *createFetch(const GameSource &source, QNetworkAccessManager *network, QObject *parent);

} // namespace SourceCatalog
