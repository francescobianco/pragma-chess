#pragma once

#include "GameSource.h"

#include <QList>
#include <QNetworkRequest>
#include <QString>

#include <optional>

class GameDatabase;
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
    /// Whether the account is a federation player ID (FIDE or FSI) rather than a username.
    bool playerId = false;
    /// Whether the source is a file on this computer (a ChessBase database)
    /// rather than an account on a site: the "account" is the file's name.
    bool localFile = false;
    /// Whether signing in is offered: required (needsSignIn), or optional
    /// when the games can be downloaded without it.
    bool canSignIn = false;
};

/// The kinds of sources Pragma Chess can sync with.
namespace SourceCatalog {

QList<SourceKind> kinds();
std::optional<SourceKind> kind(const QString &id);

/// "lichess.org · DrNykterstein", "torneionline.com · BIANCO Francesco (FIDE 896489)"
QString displayName(const GameSource &source);

/// A request answered with 200 when `account` exists on the site; for kinds
/// with a player ID, the search page to read with TorneiOnlineFetch.
QNetworkRequest accountRequest(const GameSource &source);

/// The file of a source on this computer (SourceKind::localFile), and
/// whether it is there.
QString localPath(const GameSource &source);
bool isLocalFileAvailable(const GameSource &source);

/// The fetch that downloads the new games of `source`, or nullptr for an
/// unknown kind. `database` is the one the source belongs to: a PGN file
/// works on it directly, both ways.
SourceFetch *createFetch(const GameSource &source, QNetworkAccessManager *network, GameDatabase *database,
                         QObject *parent);

} // namespace SourceCatalog
