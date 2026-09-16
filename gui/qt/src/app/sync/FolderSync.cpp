#include "FolderSync.h"

#include "RemoteStore.h"

#include <QCryptographicHash>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QTemporaryFile>

namespace {

/// Rounds of "the manifest changed meanwhile, start over" before giving up.
constexpr int kMaxRounds = 3;

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
    attempt(1);
}

void FolderSync::attempt(int round)
{
    const int generation = m_generation;
    Q_EMIT progress(tr("Comparing files…"));
    auto run = std::make_shared<Run>();
    run->round = round;
    run->local = scanLocal();

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
        run->actions = planSync(run->local, m_base, run->remote);
        run->updated = run->remote;
        run->base = m_base;
        execute(run, 0);
    });
}

void FolderSync::execute(std::shared_ptr<Run> run, qsizetype index)
{
    if (index >= run->actions.size()) {
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
                                run->updated.files.insert(path, SyncFileState{sha, size, modified, m_device, false});
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
                run->base.insert(action.path, remote.hash);
                ++run->changes;
            }
            Q_EMIT localFileChanged(absolute);
            next(result);
        });
        return;
    }
    case SyncAction::Kind::DeleteRemote:
        Q_EMIT progress(tr("Deleting %1 on the server…").arg(action.path));
        m_store->remove(action.path, [this, run, action, next, generation](const RemoteStore::Result &result) {
            if (generation != m_generation)
                return;
            if (result.ok) {
                run->updated.files.insert(action.path,
                                          SyncFileState{QString(), 0, QDateTime::currentDateTimeUtc(), m_device, true});
                run->base.remove(action.path);
                ++run->changes;
            }
            next(result);
        });
        return;
    case SyncAction::Kind::DeleteLocal: {
        Q_EMIT localFileAboutToChange(absolute);
        QFile file(absolute);
        // To the trash when the system has one, so a deletion can be undone.
        if (file.exists() && !file.moveToTrash() && !file.remove()) {
            RemoteStore::Result failed;
            failed.error = tr("Could not delete “%1”: %2").arg(action.path, file.errorString());
            Q_EMIT localFileChanged(absolute);
            next(failed);
            return;
        }
        run->base.remove(action.path);
        ++run->changes;
        Q_EMIT localFileChanged(absolute);
        RemoteStore::Result done;
        done.ok = true;
        next(done);
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
    }
}

void FolderSync::commit(std::shared_ptr<Run> run)
{
    const int generation = m_generation;
    if (run->changes == 0 && run->updated.files == run->remote.files) {
        // Nothing moved: no need to touch the manifest.
        m_base = run->base;
        finish(QString(), 0);
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
        run->updated.revision = run->remote.revision + 1;
        run->updated.updatedBy = m_device;
        run->updated.updatedAt = QDateTime::currentDateTimeUtc();
        m_store->write(QLatin1String(SyncManifest::fileName), run->updated.toJson(),
                       [this, run, generation](const RemoteStore::Result &written) {
                           if (generation != m_generation)
                               return;
                           if (written.ok)
                               m_base = run->base;
                           finish(written.ok ? QString() : written.error, run->changes);
                       });
    });
}

void FolderSync::finish(const QString &errorMessage, int changes)
{
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

    QDir().mkpath(QFileInfo(m_statePath).absolutePath());
    QSaveFile file(m_statePath);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(QJsonDocument(root).toJson());
        file.commit();
    }
}
