#include "ExtensionInstaller.h"

#include "Archive.h"
#include "app/sources/SourceFetch.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QSettings>
#include <QStandardPaths>

QList<InstalledExtension> InstalledExtension::load(QSettings &settings)
{
    QList<InstalledExtension> installed;
    const int count = settings.beginReadArray(QStringLiteral("extensions/installed"));
    for (int i = 0; i < count; ++i) {
        settings.setArrayIndex(i);
        InstalledExtension extension;
        extension.provider = settings.value(QStringLiteral("provider")).toString();
        extension.id = settings.value(QStringLiteral("id")).toString();
        extension.name = settings.value(QStringLiteral("name")).toString();
        extension.version = settings.value(QStringLiteral("version")).toString();
        extension.folder = settings.value(QStringLiteral("folder")).toString();
        extension.executable = settings.value(QStringLiteral("executable")).toString();
        extension.engineId = settings.value(QStringLiteral("engineId")).toString();
        installed << extension;
    }
    settings.endArray();
    return installed;
}

void InstalledExtension::save(QSettings &settings, const QList<InstalledExtension> &installed)
{
    settings.remove(QStringLiteral("extensions/installed"));
    settings.beginWriteArray(QStringLiteral("extensions/installed"), int(installed.size()));
    for (int i = 0; i < installed.size(); ++i) {
        settings.setArrayIndex(i);
        const InstalledExtension &extension = installed.at(i);
        settings.setValue(QStringLiteral("provider"), extension.provider);
        settings.setValue(QStringLiteral("id"), extension.id);
        settings.setValue(QStringLiteral("name"), extension.name);
        settings.setValue(QStringLiteral("version"), extension.version);
        settings.setValue(QStringLiteral("folder"), extension.folder);
        settings.setValue(QStringLiteral("executable"), extension.executable);
        settings.setValue(QStringLiteral("engineId"), extension.engineId);
    }
    settings.endArray();
}

ExtensionInstaller::ExtensionInstaller(QObject *parent)
    : QObject(parent)
    , m_network(new QNetworkAccessManager(this))
{
}

ExtensionInstaller::~ExtensionInstaller()
{
    cancel();
}

QString ExtensionInstaller::extensionsDir()
{
    return QDir(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)).filePath(QStringLiteral("extensions"));
}

QString ExtensionInstaller::folderOf(const QString &provider, const QString &id)
{
    return QDir(extensionsDir()).filePath(provider + QLatin1Char('/') + id);
}

void ExtensionInstaller::install(const QString &provider, const Extension &extension)
{
    if (m_reply || !extension.installable())
        return;
    m_provider = provider;
    m_extension = extension;
    QDir().mkpath(extensionsDir());
    m_file = new QFile(QDir(extensionsDir()).filePath(QStringLiteral(".download-") + extension.id), this);
    if (!m_file->open(QIODevice::WriteOnly)) {
        Q_EMIT failed(m_file->errorString());
        delete m_file;
        m_file = nullptr;
        return;
    }
    QNetworkRequest request{QUrl(extension.downloadUrl)};
    request.setHeader(QNetworkRequest::UserAgentHeader, SourceFetch::userAgent());
    m_reply = m_network->get(request);
    connect(m_reply, &QNetworkReply::readyRead, this, [this] { m_file->write(m_reply->readAll()); });
    connect(m_reply, &QNetworkReply::downloadProgress, this, [this](qint64 received, qint64 total) {
        Q_EMIT progress(received, total > 0 ? total : m_extension.downloadSize);
    });
    connect(m_reply, &QNetworkReply::finished, this, &ExtensionInstaller::finished);
}

void ExtensionInstaller::cancel()
{
    if (!m_reply)
        return;
    QNetworkReply *reply = m_reply;
    m_reply = nullptr;
    reply->disconnect(this);
    reply->abort();
    reply->deleteLater();
    if (m_file) {
        m_file->remove();
        delete m_file;
        m_file = nullptr;
    }
}

void ExtensionInstaller::finished()
{
    QNetworkReply *reply = m_reply;
    m_reply = nullptr;
    reply->deleteLater();
    m_file->write(reply->readAll());
    m_file->close();
    const QString download = m_file->fileName();
    delete m_file;
    m_file = nullptr;
    const auto fail = [this, &download](const QString &message) {
        QFile::remove(download);
        Q_EMIT failed(message);
    };
    if (reply->error() != QNetworkReply::NoError)
        return fail(reply->errorString());

    // A clean folder: what was there before (an older version) goes.
    const QString folder = folderOf(m_provider, m_extension.id);
    QDir(folder).removeRecursively();
    QString error;
    if (!Archive::extract(download, Archive::kindOf(m_extension.downloadUrl), folder, &error,
                          QFileInfo(QUrl(m_extension.downloadUrl).path()).fileName())) {
        QDir(folder).removeRecursively();
        return fail(error);
    }
    QFile::remove(download);

    InstalledExtension done;
    done.provider = m_provider;
    done.id = m_extension.id;
    done.name = m_extension.name;
    done.version = m_extension.version;
    done.folder = folder;
    if (m_extension.kind == Extension::Kind::Engine) {
        done.executable = QDir(folder).filePath(m_extension.executable);
        QFile executable(done.executable);
        if (!executable.exists()) {
            QDir(folder).removeRecursively();
            return fail(tr("The engine's program is not where its catalog says: %1").arg(m_extension.executable));
        }
        executable.setPermissions(executable.permissions() | QFileDevice::ExeOwner | QFileDevice::ExeUser
                                  | QFileDevice::ExeGroup | QFileDevice::ExeOther);
    }
    Q_EMIT installed(done);
}

bool ExtensionInstaller::remove(const InstalledExtension &installed, QString *error)
{
    // Only what is under the extensions' folder is ever removed.
    const QString folder = QFileInfo(installed.folder).absoluteFilePath();
    if (!folder.startsWith(QFileInfo(extensionsDir()).absoluteFilePath() + QLatin1Char('/'))) {
        if (error)
            *error = tr("%1 is not an extension's folder.").arg(folder);
        return false;
    }
    if (!QDir(folder).removeRecursively()) {
        if (error)
            *error = tr("Could not remove %1.").arg(folder);
        return false;
    }
    return true;
}
