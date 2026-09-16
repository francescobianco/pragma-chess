#include "SyncSettings.h"

#include "FtpStore.h"
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
                                                          : Service::None;
    result.host = settings.value(QStringLiteral("host")).toString();
    result.port = quint16(settings.value(QStringLiteral("port"), 21).toUInt());
    result.tls = settings.value(QStringLiteral("tls"), false).toBool();
    result.folder = settings.value(QStringLiteral("folder")).toString();
    result.url = settings.value(QStringLiteral("url")).toUrl();
    result.user = settings.value(QStringLiteral("user")).toString();
    result.password = settings.value(QStringLiteral("password")).toString();
    return result;
}

void SyncSettings::save() const
{
    QSettings settings;
    settings.beginGroup(QStringLiteral("sync"));
    settings.setValue(QStringLiteral("service"), service == Service::Ftp      ? QStringLiteral("ftp")
                                                 : service == Service::WebDav ? QStringLiteral("webdav")
                                                                              : QStringLiteral("none"));
    settings.setValue(QStringLiteral("host"), host);
    settings.setValue(QStringLiteral("port"), port);
    settings.setValue(QStringLiteral("tls"), tls);
    settings.setValue(QStringLiteral("folder"), folder);
    settings.setValue(QStringLiteral("url"), url);
    settings.setValue(QStringLiteral("user"), user);
    settings.setValue(QStringLiteral("password"), password);
}

RemoteStore *SyncSettings::createStore(QObject *parent) const
{
    if (!isConfigured())
        return nullptr;
    if (service == Service::Ftp)
        return new FtpStore(host.trimmed(), port, tls, user, password, folder.trimmed(), parent);
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
