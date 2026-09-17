#include "SourceSync.h"

#include "SourceCatalog.h"
#include "SourceFetch.h"
#include "app/GameDatabase.h"

#include <QNetworkAccessManager>
#include <QTimer>

namespace {

/// How often the sources of an open database are synced again.
constexpr int kPeriodMs = 20 * 60 * 1000;

std::optional<GameSource> findSource(GameDatabase *database, qint64 id)
{
    if (!database)
        return std::nullopt;
    for (const GameSource &source : database->sources()) {
        if (source.id == id)
            return source;
    }
    return std::nullopt;
}

} // namespace

SourceSync::SourceSync(QObject *parent)
    : QObject(parent)
    , m_network(new QNetworkAccessManager(this))
    , m_periodic(new QTimer(this))
{
    m_periodic->setInterval(kPeriodMs);
    connect(m_periodic, &QTimer::timeout, this, &SourceSync::syncAll);
}

SourceSync::~SourceSync()
{
    setDatabase(nullptr);
}

void SourceSync::setDatabase(GameDatabase *database)
{
    if (m_fetch) {
        m_fetch->abort();
        m_fetch->deleteLater();
        m_fetch = nullptr;
    }
    m_queue.clear();
    m_current = 0;
    m_database = database;
    Q_EMIT activityChanged(QString());
    if (!database) {
        m_periodic->stop();
        return;
    }
    m_periodic->start();
    syncAll();
}

void SourceSync::syncAll()
{
    if (!m_database)
        return;
    for (const GameSource &source : m_database->sources()) {
        if (source.enabled && source.id != m_current && !m_queue.contains(source.id))
            m_queue << source.id;
    }
    startNext();
}

void SourceSync::syncSource(qint64 sourceId)
{
    if (!m_database || sourceId == m_current || m_queue.contains(sourceId))
        return;
    m_queue << sourceId;
    startNext();
}

void SourceSync::cancelSource(qint64 sourceId)
{
    m_queue.removeAll(sourceId);
    if (m_current != sourceId || !m_fetch)
        return;
    m_fetch->abort();
    m_fetch->deleteLater();
    m_fetch = nullptr;
    m_current = 0;
    Q_EMIT activityChanged(QString());
    QTimer::singleShot(0, this, &SourceSync::startNext);
}

void SourceSync::startNext()
{
    if (m_fetch || !m_database)
        return;
    while (!m_queue.isEmpty()) {
        const std::optional<GameSource> source = findSource(m_database, m_queue.takeFirst());
        if (!source)
            continue;
        m_fetch = SourceCatalog::createFetch(*source, m_network, this);
        if (!m_fetch)
            continue;

        m_current = source->id;
        m_importedThisSync = 0;
        const QString name = SourceCatalog::displayName(*source);
        Q_EMIT activityChanged(tr("Syncing %1…").arg(name));

        connect(m_fetch, &SourceFetch::gamesFetched, this,
                [this, name](const QList<ImportedGame> &games, const QJsonObject &state) {
                    std::optional<GameSource> current = findSource(m_database, m_current);
                    if (!current)
                        return;
                    QString error;
                    const int added = m_database->importGames(current->id, games, &error);
                    if (added < 0) {
                        m_fetch->abort();
                        finishCurrent(tr("Could not save the games: %1").arg(error));
                        return;
                    }
                    // The cursor only moves once the games are safely stored.
                    current->state = state;
                    m_database->updateSource(*current, nullptr);
                    if (added > 0) {
                        m_importedThisSync += added;
                        Q_EMIT gamesImported(added);
                        Q_EMIT activityChanged(tr("Syncing %1… %n new game(s)", nullptr, m_importedThisSync).arg(name));
                    }
                });
        connect(m_fetch, &SourceFetch::finished, this, &SourceSync::finishCurrent);
        m_fetch->start();
        return;
    }
    Q_EMIT activityChanged(QString());
    Q_EMIT idle();
}

bool SourceSync::hasSources() const
{
    if (!m_database)
        return false;
    for (const GameSource &source : m_database->sources()) {
        if (source.enabled)
            return true;
    }
    return false;
}

void SourceSync::finishCurrent(const QString &errorMessage)
{
    if (!m_fetch)
        return;
    if (std::optional<GameSource> source = findSource(m_database, m_current)) {
        source->lastSyncAt = QDateTime::currentDateTimeUtc();
        source->lastError = errorMessage;
        m_database->updateSource(*source, nullptr);
    }
    m_fetch->deleteLater();
    m_fetch = nullptr;
    m_current = 0;
    Q_EMIT sourcesChanged();
    // Let the fetch unwind before the next one starts.
    QTimer::singleShot(0, this, &SourceSync::startNext);
}
