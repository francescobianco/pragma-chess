#pragma once

#include <QByteArray>
#include <QObject>
#include <QString>

#include <functional>

/// A folder on a server that files are synced with (FTP, WebDAV…). Paths are
/// relative to the folder, with '/' separators. Operations run one at a time,
/// asynchronously, and always call their callback exactly once.
class RemoteStore : public QObject {
    Q_OBJECT

public:
    struct Result {
        bool ok = false;
        /// The file does not exist (not an error for reads and removals).
        bool notFound = false;
        QString error;
        /// Content, for read().
        QByteArray data;
    };
    using Callback = std::function<void(const Result &result)>;

    using QObject::QObject;

    /// "ftp://user@host:21/Chess", for telling folders apart; no password.
    virtual QString identity() const = 0;

    /// Reads a small file (the sync manifest) into memory.
    virtual void read(const QString &path, Callback done) = 0;
    /// Replaces a small file, creating its folders.
    virtual void write(const QString &path, const QByteArray &data, Callback done) = 0;
    /// Downloads to `localFile`, which is only replaced once complete.
    virtual void download(const QString &path, const QString &localFile, Callback done) = 0;
    /// Uploads under a temporary name, then renames it, creating folders; a
    /// half-uploaded file is never seen under its real name.
    virtual void upload(const QString &localFile, const QString &path, Callback done) = 0;
    /// Deletes a file; a missing file counts as removed.
    virtual void remove(const QString &path, Callback done) = 0;
    /// Cancels everything; pending callbacks are not called.
    virtual void abort() = 0;

protected:
    static Result failure(const QString &error)
    {
        Result result;
        result.error = error;
        return result;
    }
    static Result success(const QByteArray &data = {})
    {
        Result result;
        result.ok = true;
        result.data = data;
        return result;
    }
};
