#pragma once

#include "RemoteStore.h"

#include <QSet>
#include <QUrl>

class QNetworkAccessManager;
class QNetworkReply;

/// A WebDAV folder (Nextcloud, ownCloud, a NAS, Apache…) over HTTP(S).
class WebDavStore : public RemoteStore {
    Q_OBJECT

public:
    WebDavStore(const QUrl &folder, const QString &user, const QString &password, QObject *parent = nullptr);

    QString identity() const override;
    void read(const QString &path, Callback done) override;
    void write(const QString &path, const QByteArray &data, Callback done) override;
    void download(const QString &path, const QString &localFile, Callback done) override;
    void upload(const QString &localFile, const QString &path, Callback done) override;
    void remove(const QString &path, Callback done) override;
    void abort() override;

private:
    QUrl urlFor(const QString &path) const;
    QNetworkReply *send(const QByteArray &verb, const QString &path, const QByteArray &body = {},
                        const QList<std::pair<QByteArray, QByteArray>> &headers = {});
    /// Creates the folders of `path` that are not known to exist.
    void makeFolders(const QString &path, std::function<void(const QString &error)> done);
    static QString replyError(QNetworkReply *reply);

    QUrl m_folder;
    QString m_user;
    QString m_password;
    QNetworkAccessManager *m_network;
    QSet<QString> m_knownFolders;
    /// Increases on abort(), so replies of earlier requests are ignored.
    int m_generation = 0;
};
