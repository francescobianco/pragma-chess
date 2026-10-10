#pragma once

#include "PositionIndex.h"

#include <QObject>

#include <atomic>
#include <memory>

class QThread;

/// Builds a PositionIndex on a worker thread, so a large database does not
/// freeze the window. A newer build supersedes one still running: only the
/// index of the last games given is ever delivered.
class PositionIndexBuilder : public QObject {
    Q_OBJECT

public:
    explicit PositionIndexBuilder(QObject *parent = nullptr);
    ~PositionIndexBuilder() override;

    /// Starts indexing `games`. The current index stays until the new one is
    /// ready (games were added): clear() first when it is another database.
    void build(const QList<GameLine> &games);
    /// Forgets the index and any build still running.
    void clear();

    /// The last index built, or null before the first build is ready.
    const PositionIndex *index() const { return m_index.get(); }
    /// How long the last index delivered took to build, in milliseconds.
    qint64 lastBuildMs() const { return m_lastBuildMs; }

Q_SIGNALS:
    /// The index changed: built, or dropped for a new build or clear().
    void indexChanged();

private:
    void cancelWorkers();

    std::shared_ptr<const PositionIndex> m_index;
    quint64 m_generation = 0;
    qint64 m_lastBuildMs = 0;
    struct Worker {
        QThread *thread = nullptr;
        std::shared_ptr<std::atomic_bool> cancelled;
    };
    /// Workers still running, stopped and waited for on destruction.
    QList<Worker> m_workers;
};
