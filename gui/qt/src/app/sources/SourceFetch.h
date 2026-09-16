#pragma once

#include "GameSource.h"

#include <QJsonObject>
#include <QList>
#include <QObject>

class QNetworkReply;
class QNetworkRequest;

/// Downloads the games of a source that are newer than its sync state.
/// One fetch per sync; the kind decides how (SourceCatalog::createFetch).
class SourceFetch : public QObject {
    Q_OBJECT

public:
    using QObject::QObject;

    virtual void start() = 0;
    /// Stops without emitting finished().
    virtual void abort() = 0;

    /// User agent sent to the sites, as their API terms ask.
    static QByteArray userAgent();
    /// Explains an HTTP failure; `site` names the service ("chess.com").
    static QString httpError(QNetworkReply *reply, const QString &site);

Q_SIGNALS:
    /// Games fetched, oldest first, and the state to store once they are saved.
    void gamesFetched(const QList<ImportedGame> &games, const QJsonObject &state);
    /// The fetch is over; `errorMessage` is empty on success.
    void finished(const QString &errorMessage);
};
