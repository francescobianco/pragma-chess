#pragma once

#include "Extension.h"

#include <QList>
#include <QObject>
#include <QNetworkReply>
#include <QPointer>

class QFile;
class QNetworkAccessManager;
class QSettings;

/// An extension installed on this computer (user settings: paths are per machine).
struct InstalledExtension {
    QString provider;
    QString id;
    QString name;
    QString version;
    /// Its folder, under extensionsDir().
    QString folder;
    /// For an engine, its executable, and the id it has in Manage Engines.
    QString executable;
    QString engineId;

    static QList<InstalledExtension> load(QSettings &settings);
    static void save(QSettings &settings, const QList<InstalledExtension> &installed);
};

/// Installs an extension: downloads it (to a file, never whole in memory),
/// opens its archive in its own folder under extensionsDir(), and makes an
/// engine executable. Configuring it (Manage Engines) is the window's.
class ExtensionInstaller : public QObject {
    Q_OBJECT

public:
    explicit ExtensionInstaller(QObject *parent = nullptr);
    ~ExtensionInstaller() override;

    /// Where extensions go: AppLocalData/extensions/<provider>/<id>.
    static QString extensionsDir();
    static QString folderOf(const QString &provider, const QString &id);

    bool isBusy() const { return m_reply != nullptr; }
    void install(const QString &provider, const Extension &extension);
    void cancel();
    /// Removes an installed extension's folder.
    static bool remove(const InstalledExtension &installed, QString *error);

Q_SIGNALS:
    void progress(qint64 received, qint64 total);
    void installed(const InstalledExtension &extension);
    void failed(const QString &message);

private:
    void finished();

    QNetworkAccessManager *m_network;
    QPointer<QNetworkReply> m_reply;
    QFile *m_file = nullptr;
    QString m_provider;
    Extension m_extension;
};
