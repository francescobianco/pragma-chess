#include "FolderSync.h"

#include "RemoteStore.h"
#include "app/DatabaseDedupe.h"

#include <QCryptographicHash>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QTemporaryFile>
#include <QTimer>
#include <QUuid>

namespace {

/// Rounds of "the manifest changed meanwhile, start over" before giving up.
constexpr int kMaxRounds = 3;
/// A lock older than this was left by a device that stopped mid-sync.
constexpr qint64 kLockExpirySeconds = 180;
/// Waiting for another device's lock: tries, and the pause between them.
constexpr int kLockTries = 20;
constexpr int kLockPauseMs = 3000;

bool isSyncedName(const QString &name)
{
    // Hidden files, partial transfers, SQLite side files and temporary copies stay local.
    return !name.startsWith(QLatin1Char('.')) && !name.endsWith(QLatin1String(".part"))
        && !name.endsWith(QLatin1String(".saving")) && !name.endsWith(QLatin1String("-journal"))
        && !name.endsWith(QLatin1String("-wal")) && !name.endsWith(QLatin1String("-shm"));
}

} // namespace

/// One sync in progress: the plan and what it produced so far.
struct FolderSync::Run {
    int round = 0;
    /// The manifest as read; `updated` starts as it plus this device's merges.
    SyncManifest remote;
    QMap<QString, LocalFileState> local;
    QList<SyncAction> actions;
    SyncManifest updated;
    QMap<QString, QString> base;
    int changes = 0;
};

FolderSync::FolderSync(const QString &localRoot, const QString &statePath, const QString &deviceName, QObject *parent)
    : QObject(parent)
    , m_root(QDir(localRoot).absolutePath())
    , m_statePath(statePath)
    , m_device(deviceName)
{
    loadState();
}

FolderSync::~FolderSync() = default;

void FolderSync::setStore(RemoteStore *store)
{
    if (m_store == store)
        return;
    ++m_generation;
    if (m_store)
        m_store->abort();
    m_store = store;
    m_running = false;
    m_again = false;
    if (store && store->identity() != m_storeIdentity) {
        // Another remote folder: nothing is known about it yet.
        m_storeIdentity = store->identity();
        m_base.clear();
        saveState();
    }
}

int FolderSync::mergeDuplicates()
{
    if (!m_hooks.lineage || !m_hooks.uids || !m_hooks.merge)
        return 0;
    QList<DatabaseDedupe::Candidate> candidates;
    QHash<QString, QString> lineages;
    const QDir root(m_root);
    QDirIterator it(m_root, {QStringLiteral("*.pdb")}, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString absolute = it.next();
        const QString relative = root.relativeFilePath(absolute);
        if (relative.split(QLatin1Char('/')).first().startsWith(QLatin1Char('.')) || !isSyncedName(it.fileName()))
            continue;
        DatabaseDedupe::Candidate candidate;
        candidate.path = relative;
        candidate.lineage = m_hooks.lineage(absolute);
        candidate.uids = m_hooks.uids(absolute);
        candidate.canonical = m_hooks.canonical && m_hooks.canonical(relative);
        lineages.insert(relative, candidate.lineage);
        candidates << candidate;
    }

    int merged = 0;
    for (const DatabaseDedupe::Group &group : DatabaseDedupe::groups(candidates)) {
        if (group.lineage.isEmpty())
            continue; // Nothing to tell the other devices where the games went.
        const QString keeper = root.filePath(group.keeper);
        Q_EMIT localFileAboutToChange(keeper);
        for (const QString &other : group.others) {
            const QString lineage = lineages.value(other);
            if (lineage.isEmpty())
                continue;
            const QString absolute = root.filePath(other);
            Q_EMIT localFileAboutToChange(absolute);
            QString error;
            if (!m_hooks.merge(absolute, keeper, group.lineage, &error)) {
                Q_EMIT progress(tr("Could not merge %1: %2").arg(other, error));
                Q_EMIT localFileChanged(absolute);
                continue;
            }
            // Its games are all in the keeper now.
            QFile::remove(absolute);
            Q_EMIT localFileMerged(absolute, keeper);
            m_pendingMerges.insert(other, SyncMergeRecord{other, lineage, group.lineage, group.keeper,
                                                          QDateTime::currentDateTimeUtc(), m_device});
            ++merged;
        }
        Q_EMIT localFileChanged(keeper);
    }
    if (merged > 0)
        saveState();
    return merged;
}

QString FolderSync::findDatabase(const QString &lineage, const QString &hint, const QString &except) const
{
    if (!m_hooks.lineage || lineage.isEmpty())
        return {};
    const QDir root(m_root);
    if (!hint.isEmpty() && hint != except && QFileInfo::exists(root.filePath(hint))
        && m_hooks.lineage(root.filePath(hint)) == lineage)
        return hint;
    QDirIterator it(m_root, {QStringLiteral("*.pdb")}, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString absolute = it.next();
        const QString relative = root.relativeFilePath(absolute);
        if (relative == except || relative.split(QLatin1Char('/')).first().startsWith(QLatin1Char('.'))
            || !isSyncedName(it.fileName()))
            continue;
        if (m_hooks.lineage(absolute) == lineage)
            return relative;
    }
    return {};
}

QDateTime FolderSync::lastSync() const
{
    return m_lastSync;
}

QString FolderSync::lastError() const
{
    return m_lastError;
}

void FolderSync::sync()
{
    if (!m_store)
        return;
    if (m_running) {
        m_again = true;
        return;
    }
    m_running = true;
    Q_EMIT started();
    if (m_store->publishesAtomically()) {
        attempt(1);
        return;
    }
    const int generation = m_generation;
    acquireLock(kLockTries, [this, generation](const QString &error) {
        if (generation != m_generation)
            return;
        if (!error.isEmpty()) {
            finish(error, 0);
            return;
        }
        attempt(1);
    });
}

void FolderSync::acquireLock(int tries, std::function<void(const QString &error)> done)
{
    const int generation = m_generation;
    const QString lock = QLatin1String(SyncManifest::lockFileName);
    m_store->read(lock, [this, tries, done, lock, generation](const RemoteStore::Result &held) {
        if (generation != m_generation)
            return;
        if (!held.ok) {
            done(held.error);
            return;
        }
        // "token device iso-time"
        const QList<QByteArray> fields = held.data.trimmed().split(' ');
        const QDateTime since = QDateTime::fromString(QString::fromLatin1(fields.value(2)), Qt::ISODate);
        const bool heldByOther = !held.notFound && fields.value(0) != m_lockToken && since.isValid()
            && since.secsTo(QDateTime::currentDateTimeUtc()) < kLockExpirySeconds;
        if (heldByOther) {
            if (tries <= 1) {
                done(tr("%1 is syncing; trying again later.").arg(QString::fromUtf8(fields.value(1))));
                return;
            }
            Q_EMIT progress(tr("Waiting for %1 to finish syncing…").arg(QString::fromUtf8(fields.value(1))));
            QTimer::singleShot(kLockPauseMs, this, [this, tries, done] { acquireLock(tries - 1, done); });
            return;
        }
        m_lockToken = QUuid::createUuid().toByteArray(QUuid::WithoutBraces);
        const QByteArray content = m_lockToken + ' ' + m_device.toUtf8().replace(' ', '_') + ' '
            + QDateTime::currentDateTimeUtc().toString(Qt::ISODate).toLatin1();
        m_store->write(lock, content, [this, tries, done, lock, generation](const RemoteStore::Result &written) {
            if (generation != m_generation)
                return;
            if (!written.ok) {
                done(written.error);
                return;
            }
            // Two devices may write at once: the lock is ours only if our token stayed.
            QTimer::singleShot(700, this, [this, tries, done, lock, generation] {
                m_store->read(lock, [this, tries, done, generation](const RemoteStore::Result &check) {
                    if (generation != m_generation)
                        return;
                    if (check.ok && check.data.trimmed().startsWith(m_lockToken)) {
                        done(QString());
                        return;
                    }
                    m_lockToken.clear();
                    if (tries <= 1) {
                        done(tr("Another device is syncing; trying again later."));
                        return;
                    }
                    QTimer::singleShot(kLockPauseMs, this, [this, tries, done] { acquireLock(tries - 1, done); });
                });
            });
        });
    });
}

void FolderSync::releaseLock(std::function<void()> then)
{
    if (m_lockToken.isEmpty() || !m_store) {
        then();
        return;
    }
    const QByteArray token = m_lockToken;
    m_lockToken.clear();
    const QString lock = QLatin1String(SyncManifest::lockFileName);
    const QPointer<RemoteStore> store = m_store;
    m_store->read(lock, [store, lock, token, then](const RemoteStore::Result &held) {
        // Never remove a lock another device took after ours expired.
        if (!store || !held.ok || held.notFound || !held.data.trimmed().startsWith(token)) {
            then();
            return;
        }
        store->remove(lock, [then](const RemoteStore::Result &) { then(); });
    });
}

void FolderSync::attempt(int round)
{
    const int generation = m_generation;
    Q_EMIT progress(tr("Comparing files…"));
    auto run = std::make_shared<Run>();
    run->round = round;
    mergeDuplicates();
    run->local = scanLocal();

    m_store->begin([this, run, generation](const RemoteStore::Result &begun) {
    if (generation != m_generation)
        return;
    if (!begun.ok) {
        finish(begun.error, 0);
        return;
    }
    m_store->read(QLatin1String(SyncManifest::fileName), [this, run, generation](const RemoteStore::Result &result) {
        if (generation != m_generation)
            return;
        if (!result.ok) {
            finish(result.error, 0);
            return;
        }
        if (!result.notFound) {
            QString error;
            const std::optional<SyncManifest> manifest = SyncManifest::fromJson(result.data, &error);
            if (!manifest) {
                finish(error, 0);
                return;
            }
            run->remote = *manifest;
        }
        // The merges made here go into the manifest with this sync.
        SyncManifest effective = run->remote;
        for (auto it = m_pendingMerges.cbegin(); it != m_pendingMerges.cend(); ++it)
            effective.merged.insert(it.key(), it.value());
        // A file at a merged path with another lineage is a new database that
        // took the name: it is synced as any other, and the record goes.
        if (m_hooks.lineage) {
            for (const QString &path : effective.merged.keys()) {
                if (!run->local.contains(path))
                    continue;
                const QString lineage = m_hooks.lineage(QDir(m_root).filePath(path));
                if (!lineage.isEmpty() && lineage != effective.merged.value(path).lineage) {
                    effective.merged.remove(path);
                    m_pendingMerges.remove(path);
                }
            }
        }
        run->actions = planSync(run->local, m_base, effective);
        // A store that can look at its own folder (a Git clone) may hold files
        // the manifest lost track of. This device has never seen them, so bring
        // them back; the next sync puts them in the manifest again. Merged
        // files stay out: their games are elsewhere.
        for (const QString &path : m_store->listFiles()) {
            if (effective.files.contains(path) || effective.merged.contains(path) || run->local.contains(path))
                continue;
            run->actions << SyncAction{SyncAction::Kind::Download, path};
        }
        // Merges last: the database they go into may be one this sync downloads.
        std::stable_partition(run->actions.begin(), run->actions.end(),
                              [](const SyncAction &action) { return action.kind != SyncAction::Kind::Merge; });
        run->updated = effective;
        run->base = m_base;
        execute(run, 0);
    });
    });
}

void FolderSync::execute(std::shared_ptr<Run> run, qsizetype index)
{
    if (index >= run->actions.size()) {
        // A duplicate may just have been downloaded (from a device that does
        // not merge, or a copy removed here without a merge): merge it now,
        // and sync again to record the merge and take it off the server.
        if (mergeDuplicates() > 0)
            m_again = true;
        commit(run);
        return;
    }
    const int generation = m_generation;
    const SyncAction action = run->actions.at(index);
    const QString absolute = QDir(m_root).filePath(action.path);
    const auto next = [this, run, index, generation](const RemoteStore::Result &result) {
        if (generation != m_generation)
            return;
        if (!result.ok) {
            finish(result.error, run->changes);
            return;
        }
        execute(run, index + 1);
    };

    // Uploads send a snapshot: the app may be writing to a database meanwhile.
    const auto uploadFile = [this, run, generation](const QString &path, RemoteStore::Callback done) {
        const QString absolutePath = QDir(m_root).filePath(path);
        auto snapshot = std::make_shared<QTemporaryFile>(QDir::temp().filePath(QStringLiteral("pragma-sync-XXXXXX")));
        QFile source(absolutePath);
        if (!snapshot->open() || !source.open(QIODevice::ReadOnly)) {
            done([&] { RemoteStore::Result r; r.error = tr("Could not read “%1”.").arg(path); return r; }());
            return;
        }
        QCryptographicHash hash(QCryptographicHash::Sha256);
        while (!source.atEnd()) {
            const QByteArray chunk = source.read(1 << 20);
            hash.addData(chunk);
            snapshot->write(chunk);
        }
        snapshot->flush();
        const QString sha = QString::fromLatin1(hash.result().toHex());
        const qint64 size = snapshot->size();
        const QDateTime modified = QFileInfo(absolutePath).lastModified();
        Q_EMIT progress(tr("Uploading %1…").arg(path));
        m_store->upload(snapshot->fileName(), path,
                        [this, run, path, sha, size, modified, snapshot, done, generation](const RemoteStore::Result &result) {
                            if (generation != m_generation)
                                return;
                            if (result.ok) {
                                run->updated.files.insert(path, SyncFileState{sha, size, modified, m_device});
                                run->base.insert(path, sha);
                                ++run->changes;
                            }
                            done(result);
                        });
    };

    switch (action.kind) {
    case SyncAction::Kind::Upload:
        uploadFile(action.path, next);
        return;
    case SyncAction::Kind::Download: {
        const SyncFileState remote = run->remote.files.value(action.path);
        Q_EMIT progress(tr("Downloading %1…").arg(action.path));
        QDir().mkpath(QFileInfo(absolute).absolutePath());
        Q_EMIT localFileAboutToChange(absolute);
        m_store->download(action.path, absolute, [this, run, action, absolute, remote, next, generation](const RemoteStore::Result &result) {
            if (generation != m_generation)
                return;
            if (result.ok) {
                if (!remote.hash.isEmpty()) // Unknown to the manifest: no base to record yet.
                    run->base.insert(action.path, remote.hash);
                ++run->changes;
            }
            Q_EMIT localFileChanged(absolute);
            next(result);
        });
        return;
    }
    case SyncAction::Kind::KeepBoth: {
        const SyncFileState remote = run->remote.files.value(action.path);
        const QString conflict = conflictPath(action.path, remote.device, remote.modified);
        const QString conflictAbsolute = QDir(m_root).filePath(conflict);
        Q_EMIT progress(tr("Keeping both versions of %1…").arg(action.path));
        m_store->download(action.path, conflictAbsolute,
                          [this, run, action, conflict, conflictAbsolute, uploadFile, next, generation](const RemoteStore::Result &downloaded) {
                              if (generation != m_generation)
                                  return;
                              if (!downloaded.ok) {
                                  next(downloaded);
                                  return;
                              }
                              Q_EMIT localFileChanged(conflictAbsolute);
                              uploadFile(conflict, [uploadFile, action, next](const RemoteStore::Result &conflictUploaded) {
                                  if (!conflictUploaded.ok) {
                                      next(conflictUploaded);
                                      return;
                                  }
                                  uploadFile(action.path, next);
                              });
                          });
        return;
    }
    case SyncAction::Kind::Record:
        run->base.insert(action.path, run->local.value(action.path).hash);
        execute(run, index + 1);
        return;
    case SyncAction::Kind::Merge:
    case SyncAction::Kind::Forget: {
        // Takes the merged file out of the remote folder, if it is there.
        const auto forget = [this, run, action, next, generation] {
            if (!run->remote.files.contains(action.path)) {
                run->updated.files.remove(action.path);
                run->base.remove(action.path);
                RemoteStore::Result done;
                done.ok = true;
                next(done);
                return;
            }
            Q_EMIT progress(tr("Removing %1, merged into another database…").arg(action.path));
            m_store->remove(action.path, [this, run, action, next, generation](const RemoteStore::Result &result) {
                if (generation != m_generation)
                    return;
                if (result.ok) {
                    run->updated.files.remove(action.path);
                    run->base.remove(action.path);
                    ++run->changes;
                }
                next(result);
            });
        };
        if (action.kind == SyncAction::Kind::Forget) {
            forget();
            return;
        }
        const SyncMergeRecord record = run->updated.merged.value(action.path);
        const QString into = findDatabase(record.into, record.intoPath, action.path);
        if (!m_hooks.merge || into.isEmpty()) {
            // Nowhere to put its games yet: keep the file here, out of the sync.
            execute(run, index + 1);
            return;
        }
        const QString intoAbsolute = QDir(m_root).filePath(into);
        Q_EMIT progress(tr("Merging %1 into %2…").arg(action.path, into));
        Q_EMIT localFileAboutToChange(intoAbsolute);
        Q_EMIT localFileAboutToChange(absolute);
        QString error;
        if (!m_hooks.merge(absolute, intoAbsolute, QString(), &error)) {
            Q_EMIT localFileChanged(absolute);
            Q_EMIT localFileChanged(intoAbsolute);
            finish(tr("Could not merge %1: %2").arg(action.path, error), run->changes);
            return;
        }
        QFile::remove(absolute);
        Q_EMIT localFileMerged(absolute, intoAbsolute);
        Q_EMIT localFileChanged(intoAbsolute);
        ++run->changes;
        m_again = true; // The database it went into changed: send it.
        forget();
        return;
    }
    }
}

void FolderSync::commit(std::shared_ptr<Run> run)
{
    const int generation = m_generation;
    if (run->changes == 0 && run->updated.files == run->remote.files && run->updated.merged == run->remote.merged) {
        // Nothing moved: no need to touch the manifest.
        m_base = run->base;
        finish(QString(), 0);
        return;
    }
    run->updated.revision = run->remote.revision + 1;
    run->updated.updatedBy = m_device;
    run->updated.updatedAt = QDateTime::currentDateTimeUtc();

    if (m_store->publishesAtomically()) {
        // The store refuses to publish over another device's sync: start over then.
        m_store->write(QLatin1String(SyncManifest::fileName), run->updated.toJson(),
                       [this, run, generation](const RemoteStore::Result &written) {
                           if (generation != m_generation)
                               return;
                           if (!written.ok) {
                               finish(written.error, run->changes);
                               return;
                           }
                           m_store->publish([this, run, generation](const RemoteStore::Result &published) {
                               if (generation != m_generation)
                                   return;
                               if (published.outdated && run->round < kMaxRounds) {
                                   attempt(run->round + 1);
                                   return;
                               }
                               if (published.ok) {
                                   m_base = run->base;
                                   forgetPublishedMerges(run->updated);
                               }
                               finish(published.ok ? QString() : published.error, run->changes);
                           });
                       });
        return;
    }

    // Written last, and only if no other device synced meanwhile.
    m_store->read(QLatin1String(SyncManifest::fileName), [this, run, generation](const RemoteStore::Result &result) {
        if (generation != m_generation)
            return;
        if (!result.ok) {
            finish(result.error, run->changes);
            return;
        }
        const std::optional<SyncManifest> current = result.notFound
            ? std::optional<SyncManifest>(SyncManifest())
            : SyncManifest::fromJson(result.data, nullptr);
        if (!current || current->revision != run->remote.revision) {
            if (run->round >= kMaxRounds) {
                finish(tr("Another device keeps syncing; trying again later."), run->changes);
                return;
            }
            attempt(run->round + 1);
            return;
        }
        m_store->write(QLatin1String(SyncManifest::fileName), run->updated.toJson(),
                       [this, run, generation](const RemoteStore::Result &written) {
                           if (generation != m_generation)
                               return;
                           if (written.ok) {
                               m_base = run->base;
                               forgetPublishedMerges(run->updated);
                           }
                           finish(written.ok ? QString() : written.error, run->changes);
                       });
    });
}

void FolderSync::forgetPublishedMerges(const SyncManifest &published)
{
    for (auto it = published.merged.cbegin(); it != published.merged.cend(); ++it)
        m_pendingMerges.remove(it.key());
}

void FolderSync::finish(const QString &errorMessage, int changes)
{
    if (!m_lockToken.isEmpty()) {
        // Other devices may start as soon as this one says it finished.
        const int generation = m_generation;
        releaseLock([this, errorMessage, changes, generation] {
            if (generation == m_generation)
                finish(errorMessage, changes);
        });
        return;
    }
    m_running = false;
    m_lastError = errorMessage;
    if (errorMessage.isEmpty())
        m_lastSync = QDateTime::currentDateTimeUtc();
    saveState();
    Q_EMIT finished(errorMessage, changes);
    if (m_again) {
        m_again = false;
        sync();
    }
}

QMap<QString, LocalFileState> FolderSync::scanLocal()
{
    QMap<QString, LocalFileState> files;
    QMap<QString, std::tuple<qint64, qint64, QString>> seen; // Forget hashes of files gone.
    QDirIterator it(m_root, QDir::Files | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString absolute = it.next();
        const QFileInfo info = it.fileInfo();
        const QString relative = QDir(m_root).relativeFilePath(absolute);
        if (relative.split(QLatin1Char('/')).first().startsWith(QLatin1Char('.')) || !isSyncedName(info.fileName()))
            continue;

        LocalFileState state;
        state.size = info.size();
        state.modified = info.lastModified();
        const auto cached = m_hashes.constFind(relative);
        const qint64 stamp = state.modified.toMSecsSinceEpoch();
        if (cached != m_hashes.cend() && std::get<0>(*cached) == state.size && std::get<1>(*cached) == stamp) {
            state.hash = std::get<2>(*cached);
        } else {
            state.hash = hashFile(absolute);
        }
        seen.insert(relative, {state.size, stamp, state.hash});
        if (!state.hash.isEmpty())
            files.insert(relative, state);
    }
    m_hashes = seen;
    return files;
}

QString FolderSync::hashFile(const QString &absolutePath) const
{
    QFile file(absolutePath);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    QCryptographicHash hash(QCryptographicHash::Sha256);
    hash.addData(&file);
    return QString::fromLatin1(hash.result().toHex());
}

void FolderSync::loadState()
{
    QFile file(m_statePath);
    if (!file.open(QIODevice::ReadOnly))
        return;
    const QJsonObject root = QJsonDocument::fromJson(file.readAll()).object();
    m_storeIdentity = root.value(QStringLiteral("remote")).toString();
    m_lastSync = QDateTime::fromString(root.value(QStringLiteral("lastSync")).toString(), Qt::ISODate);
    m_lastError = root.value(QStringLiteral("lastError")).toString();
    const QJsonObject base = root.value(QStringLiteral("base")).toObject();
    for (auto it = base.constBegin(); it != base.constEnd(); ++it)
        m_base.insert(it.key(), it.value().toString());
    // Merges not published yet, kept in the manifest's format.
    const std::optional<SyncManifest> pending = SyncManifest::fromJson(
        QJsonDocument(QJsonObject{{QStringLiteral("pragma-chess-sync"), SyncManifest::formatVersion},
                                  {QStringLiteral("merged"), root.value(QStringLiteral("pendingMerges"))}})
            .toJson(),
        nullptr);
    if (pending)
        m_pendingMerges = pending->merged;
    const QJsonObject hashes = root.value(QStringLiteral("hashes")).toObject();
    for (auto it = hashes.constBegin(); it != hashes.constEnd(); ++it) {
        const QJsonObject entry = it.value().toObject();
        m_hashes.insert(it.key(), {qint64(entry.value(QStringLiteral("size")).toDouble()),
                                   qint64(entry.value(QStringLiteral("modified")).toDouble()),
                                   entry.value(QStringLiteral("sha256")).toString()});
    }
}

void FolderSync::saveState() const
{
    QJsonObject base;
    for (auto it = m_base.cbegin(); it != m_base.cend(); ++it)
        base.insert(it.key(), it.value());
    QJsonObject hashes;
    for (auto it = m_hashes.cbegin(); it != m_hashes.cend(); ++it) {
        hashes.insert(it.key(), QJsonObject{{QStringLiteral("size"), double(std::get<0>(*it))},
                                            {QStringLiteral("modified"), double(std::get<1>(*it))},
                                            {QStringLiteral("sha256"), std::get<2>(*it)}});
    }
    QJsonObject root;
    root.insert(QStringLiteral("remote"), m_storeIdentity);
    root.insert(QStringLiteral("lastSync"), m_lastSync.toString(Qt::ISODate));
    root.insert(QStringLiteral("lastError"), m_lastError);
    root.insert(QStringLiteral("base"), base);
    root.insert(QStringLiteral("hashes"), hashes);
    if (!m_pendingMerges.isEmpty()) {
        SyncManifest pending;
        pending.merged = m_pendingMerges;
        root.insert(QStringLiteral("pendingMerges"),
                    QJsonDocument::fromJson(pending.toJson()).object().value(QStringLiteral("merged")));
    }

    QDir().mkpath(QFileInfo(m_statePath).absolutePath());
    QSaveFile file(m_statePath);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(QJsonDocument(root).toJson());
        file.commit();
    }
}
