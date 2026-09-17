#pragma once

#include <QList>
#include <QObject>
#include <QStringList>

#include <functional>

/// One step of a sync, finishing asynchronously.
///
/// Synchronizing is not one operation but a growing list of them, and their
/// order matters: games come down from the sources into the database, then
/// the project file is written, and only then does the folder go to the
/// server, so that what is pushed is what the user is looking at. A task
/// knows nothing about the others; SyncPipeline decides the order.
class SyncTask : public QObject {
    Q_OBJECT

public:
    explicit SyncTask(QString name, QObject *parent = nullptr);

    /// Short name of the step, shown while it runs ("Sources", "Project", …).
    QString name() const { return m_name; }

    /// Whether the rest of the sync is pointless once this step fails.
    /// Pushing a folder is still worth doing when a source was unreachable,
    /// so tasks are not critical unless they say so.
    bool isCritical() const { return m_critical; }
    void setCritical(bool critical) { m_critical = critical; }

    /// Whether there is anything to do; skipped steps never report an error.
    virtual bool isNeeded() const { return true; }

    /// Does the work. Ends by emitting finished(), now or later.
    virtual void run() = 0;
    /// Gives up as soon as possible. The task still has to emit finished().
    virtual void cancel() {}

Q_SIGNALS:
    void progress(const QString &text);
    /// Empty `errorMessage` means the step succeeded.
    void finished(const QString &errorMessage);

private:
    QString m_name;
    bool m_critical = false;
};

/// A step that does its work at once, such as writing a file.
/// The callable returns an error message, or an empty string on success.
class SyncStepTask : public SyncTask {
    Q_OBJECT

public:
    SyncStepTask(QString name, std::function<QString()> step, QObject *parent = nullptr);

    void run() override;

private:
    std::function<QString()> m_step;
};

/// Runs sync tasks one at a time, in the order they were added.
///
/// Errors do not stop the sync: each one is collected and reported at the
/// end, so a server that is down does not keep the games of a source from
/// being imported. A task marked critical does stop the ones after it.
class SyncPipeline : public QObject {
    Q_OBJECT

public:
    explicit SyncPipeline(QObject *parent = nullptr);

    /// Appends a step and takes ownership of it. Order is the order of the sync.
    void addTask(SyncTask *task);
    /// Forgets the steps of the previous run; only allowed while idle.
    void clear();

    /// Starts the sync. Does nothing when one is already running.
    void run();
    bool isRunning() const { return m_running; }
    /// Asks the running step to give up; finished() still arrives.
    void cancel();

Q_SIGNALS:
    void started();
    void taskStarted(const QString &name);
    /// What is happening right now, for a status bar.
    void progress(const QString &text);
    /// One line per failed step; empty when the whole sync succeeded.
    void finished(const QStringList &errors);

private:
    void startNext();
    void finishCurrent(const QString &errorMessage);

    QList<SyncTask *> m_tasks;
    qsizetype m_next = 0;
    bool m_running = false;
    bool m_cancelled = false;
    SyncTask *m_current = nullptr;
    QStringList m_errors;
};
