#include "GitStore.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QSaveFile>
#include <QStandardPaths>

namespace {

/// Git that never stops to ask anything: the sync runs in the background.
QProcessEnvironment quietEnvironment()
{
    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    environment.insert(QStringLiteral("GIT_TERMINAL_PROMPT"), QStringLiteral("0"));
    environment.insert(QStringLiteral("GIT_ASKPASS"), QStringLiteral("echo"));
    environment.insert(QStringLiteral("SSH_ASKPASS"), QStringLiteral("echo"));
    environment.insert(QStringLiteral("GIT_SSH_COMMAND"), QStringLiteral("ssh -o BatchMode=yes"));
    environment.insert(QStringLiteral("LC_ALL"), QStringLiteral("C"));
    return environment;
}

bool copyReplacing(const QString &from, const QString &to, QString *error)
{
    QFile source(from);
    if (!source.open(QIODevice::ReadOnly)) {
        *error = source.errorString();
        return false;
    }
    QDir().mkpath(QFileInfo(to).absolutePath());
    QSaveFile target(to);
    if (!target.open(QIODevice::WriteOnly)) {
        *error = target.errorString();
        return false;
    }
    while (!source.atEnd())
        target.write(source.read(1 << 20));
    if (!target.commit()) {
        *error = target.errorString();
        return false;
    }
    return true;
}

} // namespace

GitStore::GitStore(const QString &repository, const QString &branch, const QString &user, const QString &token,
                   const QString &cloneDirectory, const QString &deviceName, QObject *parent)
    : RemoteStore(parent)
    , m_repository(repository.trimmed())
    , m_branch(branch.trimmed().isEmpty() ? QStringLiteral("main") : branch.trimmed())
    , m_user(user)
    , m_token(token)
    , m_clone(cloneDirectory)
    , m_device(deviceName)
    , m_executable(QStandardPaths::findExecutable(QStringLiteral("git")))
{
}

GitStore::~GitStore()
{
    abort();
}

QString GitStore::defaultCloneDirectory(const QString &repository, const QString &branch)
{
    const QByteArray key = QCryptographicHash::hash((repository.trimmed() + QLatin1Char('#') + branch).toUtf8(),
                                                    QCryptographicHash::Sha1).toHex().left(16);
    return QDir(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation))
        .filePath(QStringLiteral("git-sync/") + QString::fromLatin1(key));
}

QString GitStore::identity() const
{
    return QStringLiteral("git:%1#%2").arg(m_repository, m_branch);
}

QString GitStore::filePath(const QString &path) const
{
    return QDir(m_clone).filePath(path);
}

GitStore::Result GitStore::gitFailure(const QString &what, const Output &output)
{
    QString detail = output.output.trimmed();
    const QStringList lines = detail.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    detail = lines.isEmpty() ? QString() : lines.last().trimmed();
    return failure(detail.isEmpty() ? what : QStringLiteral("%1: %2").arg(what, detail));
}

void GitStore::git(const QStringList &arguments, Step done, const QString &workingDirectory)
{
    const int generation = m_generation;
    if (m_executable.isEmpty()) {
        done(Output{-1, tr("Git is not installed.")});
        return;
    }
    auto *process = new QProcess(this);
    process->setProcessEnvironment(quietEnvironment());
    process->setProcessChannelMode(QProcess::MergedChannels);
    process->setWorkingDirectory(workingDirectory.isEmpty() ? m_clone : workingDirectory);

    QStringList fullArguments{QStringLiteral("-c"), QStringLiteral("user.name=Pragma Chess"),
                              QStringLiteral("-c"), QStringLiteral("user.email=sync@%1.pragma-chess").arg(m_device),
                              QStringLiteral("-c"), QStringLiteral("core.autocrlf=false")};
    if (!m_token.isEmpty() && m_repository.startsWith(QLatin1String("http"), Qt::CaseInsensitive)) {
        // A token for HTTPS, sent as a header instead of being written in the clone's config.
        const QByteArray credentials = (m_user.isEmpty() ? QStringLiteral("git") : m_user).toUtf8() + ':' + m_token.toUtf8();
        fullArguments << QStringLiteral("-c")
                      << QStringLiteral("http.extraHeader=Authorization: Basic %1").arg(QString::fromLatin1(credentials.toBase64()));
    }
    fullArguments += arguments;

    m_running << process;
    connect(process, &QProcess::finished, this, [this, process, done, generation](int exitCode, QProcess::ExitStatus status) {
        m_running.removeAll(process);
        process->deleteLater();
        if (generation != m_generation)
            return;
        done(Output{status == QProcess::NormalExit ? exitCode : -1, QString::fromUtf8(process->readAll())});
    });
    connect(process, &QProcess::errorOccurred, this, [this, process, done, generation](QProcess::ProcessError error) {
        if (error != QProcess::FailedToStart)
            return;
        m_running.removeAll(process);
        process->deleteLater();
        if (generation == m_generation)
            done(Output{-1, tr("Could not run Git: %1").arg(process->errorString())});
    });
    process->start(m_executable, fullArguments);
}

void GitStore::begin(Callback done)
{
    const QString remoteBranch = QStringLiteral("origin/") + m_branch;
    // Once cloned: fetch, then make the clone exactly the remote branch.
    const auto alignToRemote = [this, remoteBranch, done] {
        git({QStringLiteral("rev-parse"), QStringLiteral("--verify"), QStringLiteral("--quiet"), remoteBranch},
            [this, remoteBranch, done](const Output &verified) {
                m_baseCommit = verified.exitCode == 0 ? verified.output.trimmed() : QString();
                if (verified.exitCode != 0) {
                    // A new, empty repository or branch: start it on this device.
                    git({QStringLiteral("checkout"), QStringLiteral("--orphan"), m_branch}, [this, done](const Output &) {
                        // Nothing of an earlier, unpublished attempt stays in the index or the folder.
                        git({QStringLiteral("read-tree"), QStringLiteral("--empty")}, [this, done](const Output &) {
                            git({QStringLiteral("clean"), QStringLiteral("-fd")}, [done](const Output &) { done(success()); });
                        });
                    });
                    return;
                }
                git({QStringLiteral("checkout"), QStringLiteral("-B"), m_branch, remoteBranch},
                    [this, remoteBranch, done](const Output &checkedOut) {
                        if (checkedOut.exitCode != 0) {
                            done(gitFailure(tr("Git could not check out the branch"), checkedOut));
                            return;
                        }
                        git({QStringLiteral("reset"), QStringLiteral("--hard"), remoteBranch}, [this, done](const Output &reset) {
                            if (reset.exitCode != 0) {
                                done(gitFailure(tr("Git could not update the clone"), reset));
                                return;
                            }
                            git({QStringLiteral("clean"), QStringLiteral("-fd")}, [done](const Output &) { done(success()); });
                        });
                    });
            });
    };

    if (QFileInfo::exists(QDir(m_clone).filePath(QStringLiteral(".git")))) {
        git({QStringLiteral("remote"), QStringLiteral("set-url"), QStringLiteral("origin"), m_repository},
            [this, alignToRemote, done](const Output &) {
                git({QStringLiteral("fetch"), QStringLiteral("--prune"), QStringLiteral("origin")},
                    [alignToRemote, done](const Output &fetched) {
                        if (fetched.exitCode != 0) {
                            done(gitFailure(tr("Git could not fetch the repository"), fetched));
                            return;
                        }
                        alignToRemote();
                    });
            });
        return;
    }

    QDir().mkpath(QFileInfo(m_clone).absolutePath());
    QDir(m_clone).removeRecursively();
    git({QStringLiteral("clone"), QStringLiteral("--no-checkout"), m_repository, m_clone},
        [alignToRemote, done](const Output &cloned) {
            if (cloned.exitCode != 0) {
                done(gitFailure(tr("Git could not clone the repository"), cloned));
                return;
            }
            alignToRemote();
        },
        QFileInfo(m_clone).absolutePath());
}

void GitStore::publish(Callback done)
{
    git({QStringLiteral("add"), QStringLiteral("--all")}, [this, done](const Output &added) {
        if (added.exitCode != 0) {
            done(gitFailure(tr("Git could not add the files"), added));
            return;
        }
        git({QStringLiteral("diff"), QStringLiteral("--cached"), QStringLiteral("--quiet")}, [this, done](const Output &diff) {
            if (diff.exitCode == 0) {
                done(success()); // Nothing changed.
                return;
            }
            git({QStringLiteral("commit"), QStringLiteral("--quiet"), QStringLiteral("-m"),
                 QStringLiteral("Sync from %1").arg(m_device)},
                [this, done](const Output &committed) {
                    if (committed.exitCode != 0) {
                        done(gitFailure(tr("Git could not commit"), committed));
                        return;
                    }
                    git({QStringLiteral("push"), QStringLiteral("origin"), QStringLiteral("HEAD:refs/heads/") + m_branch},
                        [this, done](const Output &pushed) {
                            if (pushed.exitCode == 0) {
                                done(success());
                                return;
                            }
                            // Refused because another device pushed first? Then the branch moved
                            // (or is moving, if both pushed at the same instant).
                            git({QStringLiteral("ls-remote"), QStringLiteral("origin"), QStringLiteral("refs/heads/") + m_branch},
                                [this, pushed, done](const Output &remote) {
                                    Result result = gitFailure(tr("Git could not push"), pushed);
                                    const QString remoteCommit = remote.output.section(QLatin1Char('\t'), 0, 0).trimmed();
                                    result.outdated = (remote.exitCode == 0 && remoteCommit != m_baseCommit)
                                        || pushed.output.contains(QLatin1String("[rejected]"))
                                        || pushed.output.contains(QLatin1String("[remote rejected]"))
                                        || pushed.output.contains(QLatin1String("cannot lock ref"));
                                    done(result);
                                });
                        });
                });
        });
    });
}

void GitStore::read(const QString &path, Callback done)
{
    QFile file(filePath(path));
    if (!file.exists()) {
        Result missing = success();
        missing.notFound = true;
        done(missing);
        return;
    }
    if (!file.open(QIODevice::ReadOnly)) {
        done(failure(file.errorString()));
        return;
    }
    done(success(file.readAll()));
}

void GitStore::write(const QString &path, const QByteArray &data, Callback done)
{
    QDir().mkpath(QFileInfo(filePath(path)).absolutePath());
    QSaveFile file(filePath(path));
    if (!file.open(QIODevice::WriteOnly)) {
        done(failure(file.errorString()));
        return;
    }
    file.write(data);
    done(file.commit() ? success() : failure(file.errorString()));
}

void GitStore::download(const QString &path, const QString &localFile, Callback done)
{
    if (!QFileInfo::exists(filePath(path))) {
        Result missing = failure(tr("“%1” is not in the repository.").arg(path));
        missing.notFound = true;
        done(missing);
        return;
    }
    QString error;
    done(copyReplacing(filePath(path), localFile, &error) ? success() : failure(error));
}

void GitStore::upload(const QString &localFile, const QString &path, Callback done)
{
    QString error;
    done(copyReplacing(localFile, filePath(path), &error) ? success() : failure(error));
}

void GitStore::remove(const QString &path, Callback done)
{
    QFile::remove(filePath(path));
    done(success());
}

void GitStore::abort()
{
    ++m_generation;
    const QList<QProcess *> running = m_running;
    m_running.clear();
    for (QProcess *process : running) {
        process->disconnect(this);
        process->kill();
        process->waitForFinished(2000);
        process->deleteLater();
    }
}
