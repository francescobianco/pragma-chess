#include "PositionIndexBuilder.h"

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
    cancelWorkers();
    const quint64 generation = m_generation;
    auto cancelled = std::make_shared<std::atomic_bool>(false);
    auto result = std::make_shared<std::shared_ptr<const PositionIndex>>();
    QThread *thread = QThread::create([games, cancelled, result] {
        *result = std::make_shared<const PositionIndex>(PositionIndex::build(games, cancelled.get()));
    });
    connect(thread, &QThread::finished, this, [this, thread, generation, cancelled, result] {
        m_workers.removeIf([thread](const Worker &worker) { return worker.thread == thread; });
        thread->deleteLater();
        if (generation == m_generation && !cancelled->load()) {
            m_index = *result;
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
