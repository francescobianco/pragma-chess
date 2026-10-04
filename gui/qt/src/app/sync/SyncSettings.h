#pragma once

#include <QString>
#include <QUrl>

class QObject;
class RemoteStore;

/// Where the Pragma folder is synced to, as configured in Options ▸ Sync Settings.
/// Stored in the user's settings; the password too, until it moves to the
/// system keychain (it never goes into the synced folder).
struct SyncSettings {
    enum class Service { None, Ftp, WebDav, Git };

    Service service = Service::None;
    // FTP
    QString host;
    quint16 port = 21;
    bool tls = false;
    QString folder;
    // WebDAV
    QUrl url;
    // Git: repository URL (SSH or HTTPS) and branch; the password is an HTTPS token.
    QString repository;
    QString branch = QStringLiteral("main");
    // Both
    QString user;
    QString password;

    /// Sync everything when Pragma Chess is closed, asked once and remembered.
    bool syncBeforeClosing = false;

    bool isConfigured() const;
    bool operator==(const SyncSettings &) const = default;

    static SyncSettings load();
    void save() const;

    /// The remote folder these settings point at, or nullptr when not configured.
    RemoteStore *createStore(QObject *parent) const;

    /// This device's name in the sync manifest and in conflict file names.
    static QString deviceName();
    /// Where older versions remembered what this device last synced; the
    /// state now lives in the Pragma folder (FolderSync), moved from here once.
    static QString statePath();
};
