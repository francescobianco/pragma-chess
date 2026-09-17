#include "SyncPipeline.h"

#include <QTimer>

SyncTask::SyncTask(QString name, QObject *parent)
    : QObject(parent)
    , m_name(std::move(name))
{
}

SyncStepTask::SyncStepTask(QString name, std::function<QString()> step, QObject *parent)
    : SyncTask(std::move(name), parent)
    , m_step(std::move(step))
{
}

void SyncStepTask::run()
{
    Q_EMIT finished(m_step ? m_step() : QString());
}

SyncPipeline::SyncPipeline(QObject *parent)
    : QObject(parent)
{
}

void SyncPipeline::addTask(SyncTask *task)
{
    if (!task)
        return;
    task->setParent(this);
    m_tasks << task;
}

void SyncPipeline::clear()
{
    if (m_running)
        return;
    qDeleteAll(m_tasks);
    m_tasks.clear();
}

void SyncPipeline::run()
{
    if (m_running)
        return;
    m_running = true;
    m_cancelled = false;
    m_next = 0;
    m_errors.clear();
    Q_EMIT started();
    startNext();
}

void SyncPipeline::cancel()
{
    if (!m_running)
        return;
    m_cancelled = true;
    if (m_current)
        m_current->cancel();
}

void SyncPipeline::startNext()
{
    while (m_next < m_tasks.size()) {
        SyncTask *task = m_tasks.at(m_next++);
        if (m_cancelled || !task->isNeeded())
            continue;

        m_current = task;
        connect(task, &SyncTask::progress, this, &SyncPipeline::progress, Qt::UniqueConnection);
        connect(task, &SyncTask::finished, this, &SyncPipeline::finishCurrent, Qt::UniqueConnection);
        Q_EMIT taskStarted(task->name());
        task->run(); // May call finishCurrent() before returning.
        return;
    }

    m_current = nullptr;
    m_running = false;
    Q_EMIT finished(m_errors);
}

void SyncPipeline::finishCurrent(const QString &errorMessage)
{
    SyncTask *task = m_current;
    if (!task)
        return;
    m_current = nullptr;
    task->disconnect(this);

    if (!errorMessage.isEmpty()) {
        m_errors << QStringLiteral("%1: %2").arg(task->name(), errorMessage);
        if (task->isCritical())
            m_next = m_tasks.size(); // Nothing after a critical failure makes sense.
    }
    // Never recurse into the next task from inside the previous one's signal.
    QTimer::singleShot(0, this, &SyncPipeline::startNext);
}
