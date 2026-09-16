#pragma once

#include <QString>
#include <QUrl>

class QObject;
class RemoteStore;

/// Where the Pragma folder is synced to, as configured in File ▸ Sync.
/// Stored in the user's settings; the password too, until it moves to the
/// system keychain (it never goes into the synced folder).
struct SyncSettings {
    enum class Service { None, Ftp, WebDav };

    Service service = Service::None;
    // FTP
    QString host;
    quint16 port = 21;
    bool tls = false;
    QString folder;
    // WebDAV
    QUrl url;
    // Both
    QString user;
    QString password;

    bool isConfigured() const;

    static SyncSettings load();
    void save() const;

    /// The remote folder these settings point at, or nullptr when not configured.
    RemoteStore *createStore(QObject *parent) const;

    /// This device's name in the sync manifest and in conflict file names.
    static QString deviceName();
    /// Where this device remembers what it last synced.
    static QString statePath();
};
