#include "SyncSettings.h"

#include "FtpStore.h"
#include "GitStore.h"
#include "WebDavStore.h"

#include <QDir>
#include <QSettings>
#include <QStandardPaths>
#include <QSysInfo>

bool SyncSettings::isConfigured() const
{
    switch (service) {
    case Service::None: return false;
    case Service::Ftp: return !host.trimmed().isEmpty();
    case Service::WebDav: return url.isValid() && !url.host().isEmpty();
    case Service::Git: return !repository.trimmed().isEmpty();
    }
    return false;
}

SyncSettings SyncSettings::load()
{
    QSettings settings;
    settings.beginGroup(QStringLiteral("sync"));
    SyncSettings result;
    const QString service = settings.value(QStringLiteral("service")).toString();
    result.service = service == QLatin1String("ftp")      ? Service::Ftp
        : service == QLatin1String("webdav")              ? Service::WebDav
        : service == QLatin1String("git")                 ? Service::Git
                                                          : Service::None;
    result.repository = settings.value(QStringLiteral("repository")).toString();
    result.branch = settings.value(QStringLiteral("branch"), QStringLiteral("main")).toString();
    result.host = settings.value(QStringLiteral("host")).toString();
    result.port = quint16(settings.value(QStringLiteral("port"), 21).toUInt());
    result.tls = settings.value(QStringLiteral("tls"), false).toBool();
    result.folder = settings.value(QStringLiteral("folder")).toString();
    result.url = settings.value(QStringLiteral("url")).toUrl();
    result.user = settings.value(QStringLiteral("user")).toString();
    result.password = settings.value(QStringLiteral("password")).toString();
    result.syncBeforeClosing = settings.value(QStringLiteral("beforeClosing"), false).toBool();
    return result;
}

void SyncSettings::save() const
{
    QSettings settings;
    settings.beginGroup(QStringLiteral("sync"));
    settings.setValue(QStringLiteral("service"), service == Service::Ftp      ? QStringLiteral("ftp")
                                                 : service == Service::WebDav ? QStringLiteral("webdav")
                                                 : service == Service::Git    ? QStringLiteral("git")
                                                                              : QStringLiteral("none"));
    settings.setValue(QStringLiteral("repository"), repository);
    settings.setValue(QStringLiteral("branch"), branch);
    settings.setValue(QStringLiteral("host"), host);
    settings.setValue(QStringLiteral("port"), port);
    settings.setValue(QStringLiteral("tls"), tls);
    settings.setValue(QStringLiteral("folder"), folder);
    settings.setValue(QStringLiteral("url"), url);
    settings.setValue(QStringLiteral("beforeClosing"), syncBeforeClosing);
    settings.setValue(QStringLiteral("user"), user);
    settings.setValue(QStringLiteral("password"), password);
}

RemoteStore *SyncSettings::createStore(QObject *parent) const
{
    if (!isConfigured())
        return nullptr;
    if (service == Service::Ftp)
        return new FtpStore(host.trimmed(), port, tls, user, password, folder.trimmed(), parent);
    if (service == Service::Git)
        return new GitStore(repository, branch, user, password, GitStore::defaultCloneDirectory(repository, branch),
                            deviceName(), parent);
    return new WebDavStore(url, user, password, parent);
}

QString SyncSettings::deviceName()
{
    const QString name = QSysInfo::machineHostName();
    return name.isEmpty() ? QStringLiteral("device") : name;
}

QString SyncSettings::statePath()
{
    return QDir(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation))
        .filePath(QStringLiteral("folder-sync.json"));
}
