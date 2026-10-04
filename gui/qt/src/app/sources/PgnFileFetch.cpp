#include "PgnFileFetch.h"

#include "app/GameDatabase.h"

#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QTimer>

namespace {

/// Games imported at a time: the application breathes between batches.
constexpr int kBatch = 200;

QByteArray entryBytes(const QByteArray &bytes, const PgnFile::Entry &entry)
{
    return bytes.mid(entry.offset, entry.length);
}

} // namespace

PgnFileFetch::PgnFileFetch(const GameSource &source, GameDatabase *database, QObject *parent)
    : SourceFetch(parent)
    , m_source(source)
    , m_database(database)
    , m_mode(mode(source))
{
}

QString PgnFileFetch::path(const GameSource &source)
{
    return source.settings.value(QLatin1String(PgnFileSettings::path)).toString();
}

PgnFilePlan::Mode PgnFileFetch::mode(const GameSource &source)
{
    return PgnFilePlan::modeFromKey(source.settings.value(QLatin1String(PgnFileSettings::mode)).toString());
}

void PgnFileFetch::abort()
{
    m_aborted = true;
}

PgnFileFetch::Snapshot PgnFileFetch::snapshot() const
{
    Snapshot snapshot;
    for (qint64 index = 0; index < m_database->gameCount(); ++index) {
        const GameRecord header = m_database->header(index);
        if (header.uid.isEmpty())
            continue;
        snapshot.games.insert(header.uid, {header.modified, header.state == GameState::Live});
        snapshot.indexes.insert(header.uid, index);
        snapshot.order << header.uid;
    }
    return snapshot;
}

QHash<QString, QString> PgnFileFetch::linked() const
{
    QHash<QString, QString> result;
    for (const SourceLink &link : m_database->sourceLinks()) {
        if (link.sourceUuid == m_source.uuid)
            result.insert(link.externalId, link.gameUid);
    }
    return result;
}

void PgnFileFetch::start()
{
    QTimer::singleShot(0, this, [this] {
        if (m_aborted)
            return;
        const QString file = path(m_source);
        QFile pgn(file);
        if (!m_database || !pgn.open(QIODevice::ReadOnly)) {
            Q_EMIT finished(tr("The PGN file was not found at %1.").arg(file));
            return;
        }
        m_bytes = pgn.readAll();
        m_fileTime = QFileInfo(file).lastModified();
        const QString hash = PgnFile::fileHash(m_bytes);
        // The index is kept only while the file is the one it describes.
        m_index = PgnFile::readIndex(file, hash).value_or(PgnFile::Index{hash, PgnFile::scan(m_bytes)});
        m_base = PgnFilePlan::baseFromJson(m_source.state);

        const QHash<QString, QString> known = linked();
        m_plan = PgnFilePlan::planRead(m_mode, m_index.entries, m_base, snapshot().games,
                                       QSet<QString>(known.keyBegin(), known.keyEnd()));
        for (const int i : m_plan.imports) {
            const PgnFile::Entry &entry = m_index.entries.at(i);
            QString ignored;
            if (std::optional<GameRecord> game = PgnFile::read(entryBytes(m_bytes, entry), &ignored))
                m_toImport << ImportedGame{PgnFilePlan::externalId(entry), *game};
            else
                ++m_unreadable;
        }
        // Changed on both sides: the file's version is a game of its own.
        for (const int i : m_plan.conflicts) {
            const PgnFile::Entry &entry = m_index.entries.at(i);
            QString ignored;
            if (std::optional<GameRecord> game = PgnFile::read(entryBytes(m_bytes, entry), &ignored)) {
                game->uid.clear();
                m_toImport << ImportedGame{QStringLiteral("sha:") + entry.hash, *game};
            }
        }
        importBatch();
    });
}

void PgnFileFetch::importBatch()
{
    if (m_aborted)
        return;
    if (!m_toImport.isEmpty()) {
        const QList<ImportedGame> batch = m_toImport.mid(0, kBatch);
        m_toImport.remove(0, batch.size());
        QString error;
        const int added = m_database->importGames(m_source.id, batch, &error);
        if (added < 0) {
            Q_EMIT finished(tr("Could not save the games: %1").arg(error));
            return;
        }
        m_added += added;
        if (added > 0)
            Q_EMIT gamesChanged(added, {});
        QTimer::singleShot(0, this, &PgnFileFetch::importBatch);
        return;
    }
    finish();
}

void PgnFileFetch::finish()
{
    // The file's changes to games the database did not change.
    Snapshot now = snapshot();
    QList<qint64> updated;
    for (const auto &[i, uid] : std::as_const(m_plan.updates)) {
        QString ignored;
        std::optional<GameRecord> game = PgnFile::read(entryBytes(m_bytes, m_index.entries.at(i)), &ignored);
        const qint64 index = now.indexes.value(uid, -1);
        if (!game || index < 0) {
            ++m_unreadable;
            continue;
        }
        QString error;
        if (!m_database->replaceGame(index, *game, &error)) {
            Q_EMIT finished(tr("Could not save the games: %1").arg(error));
            return;
        }
        updated << index;
    }
    if (!updated.isEmpty()) {
        now = snapshot();
        Q_EMIT gamesChanged(0, updated);
    }

    const PgnFilePlan::WritePlan write =
        PgnFilePlan::planWrite(m_mode, m_index.entries, m_base, now.games, linked(), now.order);
    QSet<QString> pending;
    QString error;
    if (!m_plan.rewrites.isEmpty() || !write.tags.isEmpty() || !write.appends.isEmpty()) {
        if (!writeFile(m_plan, write, now, &error)) {
            for (const auto &[i, uid] : std::as_const(m_plan.rewrites))
                pending.insert(uid);
        }
    }
    const QString file = path(m_source);
    PgnFile::writeIndex(file, m_index);

    if (std::optional<GameSource> current = [this]() -> std::optional<GameSource> {
            for (const GameSource &source : m_database->sources()) {
                if (source.id == m_source.id)
                    return source;
            }
            return std::nullopt;
        }()) {
        current->state = PgnFilePlan::baseToJson(PgnFilePlan::nextBase(m_base, m_index.entries, now.games, pending));
        m_database->updateSource(*current, nullptr);
    }
    if (error.isEmpty() && m_unreadable > 0)
        error = tr("%n game(s) of the file could not be read.", nullptr, m_unreadable);
    Q_EMIT finished(error);
}

bool PgnFileFetch::writeFile(const PgnFilePlan::ReadPlan &read, const PgnFilePlan::WritePlan &write,
                             const Snapshot &now, QString *errorMessage)
{
    const auto gameText = [this, &now](const QString &uid) -> QByteArray {
        const std::optional<GameRecord> game = m_database->loadGame(now.indexes.value(uid, -1));
        return game ? PgnFile::write(*game) : QByteArray();
    };
    QHash<int, QByteArray> replaced;
    for (const auto &[i, uid] : read.rewrites) {
        if (const QByteArray text = gameText(uid); !text.isEmpty())
            replaced.insert(i, text);
    }
    for (const auto &[i, uid] : write.tags)
        replaced.insert(i, PgnFile::withUid(entryBytes(m_bytes, m_index.entries.at(i)), uid));

    QByteArray bytes;
    bytes.reserve(m_bytes.size());
    for (int i = 0; i < m_index.entries.size(); ++i)
        bytes += replaced.contains(i) ? replaced.value(i) : entryBytes(m_bytes, m_index.entries.at(i));
    for (const QString &uid : write.appends) {
        // A blank line between games.
        if (!bytes.isEmpty() && !bytes.endsWith("\n\n") && !bytes.endsWith("\r\n\r\n"))
            bytes += bytes.endsWith('\n') ? "\n" : "\n\n";
        bytes += gameText(uid);
    }

    // Someone wrote the file meanwhile: their change wins, this waits for the next sync.
    const QString file = path(m_source);
    QFile current(file);
    if (QFileInfo(file).lastModified() != m_fileTime || !current.open(QIODevice::ReadOnly)
        || current.readAll() != m_bytes) {
        *errorMessage = tr("The PGN file changed while it was being synced; it is synced again next time.");
        return false;
    }
    current.close();
    QSaveFile save(file);
    if (!save.open(QIODevice::WriteOnly) || save.write(bytes) != bytes.size() || !save.commit()) {
        *errorMessage = tr("Could not write the PGN file: %1").arg(save.errorString());
        return false;
    }
    m_bytes = bytes;
    m_index = PgnFile::Index{PgnFile::fileHash(bytes), PgnFile::scan(bytes)};
    return true;
}
