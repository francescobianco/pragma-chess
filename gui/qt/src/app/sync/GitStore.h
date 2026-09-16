#pragma once

#include "RemoteStore.h"

#include <QStringList>

class QProcess;

/// A Git repository as the synced folder.
///
/// The real files never live in Git: syncs work on a clone kept in the app's
/// data folder. begin() brings the clone to the remote branch, the sync copies
/// files in and out of it, and publish() commits and pushes. A push refused
/// because another device pushed first makes the sync start over.
///
/// Authentication is the user's Git setup (SSH keys, credential helper); for
/// HTTPS a user name and token can be given instead.
class GitStore : public RemoteStore {
    Q_OBJECT

public:
    GitStore(const QString &repository, const QString &branch, const QString &user, const QString &token,
             const QString &cloneDirectory, const QString &deviceName, QObject *parent = nullptr);
    ~GitStore() override;

    /// The clone for a repository and branch, under the app's data folder.
    static QString defaultCloneDirectory(const QString &repository, const QString &branch);

    QString identity() const override;
    void begin(Callback done) override;
    bool publishesAtomically() const override { return true; }
    void publish(Callback done) override;

    void read(const QString &path, Callback done) override;
    void write(const QString &path, const QByteArray &data, Callback done) override;
    void download(const QString &path, const QString &localFile, Callback done) override;
    void upload(const QString &localFile, const QString &path, Callback done) override;
    void remove(const QString &path, Callback done) override;
    void abort() override;

private:
    struct Output {
        int exitCode = -1;
        QString output;
    };
    using Step = std::function<void(const Output &)>;

    /// Runs git with the given arguments in the clone (or `workingDirectory`).
    void git(const QStringList &arguments, Step done, const QString &workingDirectory = QString());
    QString filePath(const QString &path) const;
    static Result gitFailure(const QString &what, const Output &output);

    QString m_repository;
    QString m_branch;
    QString m_user;
    QString m_token;
    QString m_clone;
    QString m_device;
    QString m_executable;
    QList<QProcess *> m_running;
    /// Commit of the remote branch the clone was aligned to by begin(); empty for a new branch.
    QString m_baseCommit;
    int m_generation = 0;
};
