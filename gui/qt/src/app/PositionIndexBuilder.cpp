#include "PositionIndexBuilder.h"

#include <QElapsedTimer>
#include <QThread>

PositionIndexBuilder::PositionIndexBuilder(QObject *parent)
    : QObject(parent)
{
}

PositionIndexBuilder::~PositionIndexBuilder()
{
    // Workers only touch their own copies; stop them and wait before going.
    for (const Worker &worker : std::as_const(m_workers)) {
        worker.cancelled->store(true);
        worker.thread->wait();
        delete worker.thread;
    }
}

void PositionIndexBuilder::build(const QList<GameLine> &games)
{
    build([games] { return games; });
}

void PositionIndexBuilder::build(std::function<QList<GameLine>()> read)
{
    build([read = std::move(read)](const std::atomic_bool *cancelled) {
        const QList<GameLine> games = read();
        return cancelled->load() ? PositionIndex() : PositionIndex::build(games, cancelled);
    });
}

void PositionIndexBuilder::build(std::function<PositionIndex(const std::atomic_bool *cancelled)> make)
{
    cancelWorkers();
    const quint64 generation = m_generation;
    auto cancelled = std::make_shared<std::atomic_bool>(false);
    auto result = std::make_shared<std::shared_ptr<const PositionIndex>>();
    auto ms = std::make_shared<qint64>(0);
    QThread *thread = QThread::create([make = std::move(make), cancelled, result, ms] {
        QElapsedTimer clock;
        clock.start();
        *result = std::make_shared<const PositionIndex>(make(cancelled.get()));
        *ms = clock.elapsed();
    });
    connect(thread, &QThread::finished, this, [this, thread, generation, cancelled, result, ms] {
        m_workers.removeIf([thread](const Worker &worker) { return worker.thread == thread; });
        thread->deleteLater();
        if (generation == m_generation && !cancelled->load()) {
            m_index = *result;
            m_lastBuildMs = *ms;
            Q_EMIT indexChanged();
        }
    });
    m_workers << Worker{thread, cancelled};
    thread->start(QThread::LowPriority);
}

void PositionIndexBuilder::cancelWorkers()
{
    ++m_generation;
    for (const Worker &worker : std::as_const(m_workers))
        worker.cancelled->store(true); // Superseded.
}

void PositionIndexBuilder::clear()
{
    cancelWorkers();
    const bool had = m_index != nullptr;
    m_index.reset();
    if (had)
        Q_EMIT indexChanged();
}
