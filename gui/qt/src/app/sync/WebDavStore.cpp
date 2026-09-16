#include "WebDavStore.h"

#include <QBuffer>
#include <QFile>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QSaveFile>

WebDavStore::WebDavStore(const QUrl &folder, const QString &user, const QString &password, QObject *parent)
    : RemoteStore(parent)
    , m_folder(folder)
    , m_user(user)
    , m_password(password)
    , m_network(new QNetworkAccessManager(this))
{
    if (!m_folder.path().endsWith(QLatin1Char('/')))
        m_folder.setPath(m_folder.path() + QLatin1Char('/'));
}

QString WebDavStore::identity() const
{
    QUrl url = m_folder;
    url.setPassword(QString());
    url.setUserName(m_user);
    return url.toString();
}

QUrl WebDavStore::urlFor(const QString &path) const
{
    QByteArray url = m_folder.toString(QUrl::FullyEncoded | QUrl::RemoveUserInfo).toUtf8();
    const QStringList segments = path.split(QLatin1Char('/'), Qt::SkipEmptyParts);
    for (qsizetype i = 0; i < segments.size(); ++i)
        url += (i > 0 ? "/" : "") + QUrl::toPercentEncoding(segments.at(i));
    if (path.endsWith(QLatin1Char('/')) && !segments.isEmpty())
        url += '/';
    return QUrl::fromEncoded(url);
}

QNetworkReply *WebDavStore::send(const QByteArray &verb, const QString &path, const QByteArray &body,
                                 const QList<std::pair<QByteArray, QByteArray>> &headers)
{
    QNetworkRequest request(urlFor(path));
    request.setHeader(QNetworkRequest::UserAgentHeader, QByteArrayLiteral("PragmaChess/" APP_VERSION));
    if (!m_user.isEmpty())
        request.setRawHeader("Authorization", "Basic " + (m_user + QLatin1Char(':') + m_password).toUtf8().toBase64());
    for (const auto &[name, value] : headers)
        request.setRawHeader(name, value);
    request.setTransferTimeout(60000);
    return m_network->sendCustomRequest(request, verb, body);
}

QString WebDavStore::replyError(QNetworkReply *reply)
{
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (status == 401 || status == 403)
        return tr("The server refused the user name or password.");
    if (status == 507)
        return tr("The server has no space left.");
    if (status > 0)
        return tr("The server answered %1 %2.").arg(status)
            .arg(reply->attribute(QNetworkRequest::HttpReasonPhraseAttribute).toString());
    return reply->errorString();
}

void WebDavStore::makeFolders(const QString &path, std::function<void(const QString &error)> done)
{
    QStringList folders{QString()}; // The synced folder itself.
    const QStringList segments = path.split(QLatin1Char('/'), Qt::SkipEmptyParts);
    QString prefix;
    for (qsizetype i = 0; i + 1 < segments.size(); ++i) {
        prefix += segments.at(i) + QLatin1Char('/');
        folders << prefix;
    }
    folders.removeIf([this](const QString &folder) { return m_knownFolders.contains(folder); });

    const int generation = m_generation;
    auto next = std::make_shared<std::function<void()>>();
    *next = [this, folders, done, generation, next]() mutable {
        if (generation != m_generation)
            return;
        if (folders.isEmpty()) {
            done(QString());
            return;
        }
        const QString folder = folders.takeFirst();
        QNetworkReply *reply = send("MKCOL", folder.isEmpty() ? QStringLiteral("/") : folder);
        connect(reply, &QNetworkReply::finished, this, [this, reply, folder, done, next, generation] {
            reply->deleteLater();
            if (generation != m_generation)
                return;
            const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            // 201 created; 405 (or 301) already there.
            if (status == 201 || status == 405 || status == 301 || status == 200) {
                m_knownFolders.insert(folder);
                (*next)();
                return;
            }
            done(replyError(reply));
        });
    };
    (*next)();
}

void WebDavStore::read(const QString &path, Callback done)
{
    const int generation = m_generation;
    QNetworkReply *reply = send("GET", path);
    connect(reply, &QNetworkReply::finished, this, [this, reply, done, generation] {
        reply->deleteLater();
        if (generation != m_generation)
            return;
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (status == 404) {
            Result result;
            result.ok = true;
            result.notFound = true;
            done(result);
            return;
        }
        if (reply->error() != QNetworkReply::NoError) {
            done(failure(replyError(reply)));
            return;
        }
        done(success(reply->readAll()));
    });
}

void WebDavStore::write(const QString &path, const QByteArray &data, Callback done)
{
    const int generation = m_generation;
    makeFolders(path, [this, path, data, done, generation](const QString &error) {
        if (!error.isEmpty()) {
            done(failure(error));
            return;
        }
        QNetworkReply *reply = send("PUT", path, data);
        connect(reply, &QNetworkReply::finished, this, [this, reply, done, generation] {
            reply->deleteLater();
            if (generation != m_generation)
                return;
            done(reply->error() == QNetworkReply::NoError ? success() : failure(replyError(reply)));
        });
    });
}

void WebDavStore::download(const QString &path, const QString &localFile, Callback done)
{
    const int generation = m_generation;
    auto file = std::make_shared<QSaveFile>(localFile);
    if (!file->open(QIODevice::WriteOnly)) {
        done(failure(file->errorString()));
        return;
    }
    QNetworkReply *reply = send("GET", path);
    connect(reply, &QNetworkReply::readyRead, this, [reply, file] { file->write(reply->readAll()); });
    connect(reply, &QNetworkReply::finished, this, [this, reply, file, done, generation] {
        reply->deleteLater();
        if (generation != m_generation) {
            file->cancelWriting();
            return;
        }
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (reply->error() != QNetworkReply::NoError) {
            file->cancelWriting();
            Result result = failure(replyError(reply));
            result.notFound = status == 404;
            done(result);
            return;
        }
        file->write(reply->readAll());
        done(file->commit() ? success() : failure(file->errorString()));
    });
}

void WebDavStore::upload(const QString &localFile, const QString &path, Callback done)
{
    const int generation = m_generation;
    makeFolders(path, [this, localFile, path, done, generation](const QString &error) {
        if (!error.isEmpty()) {
            done(failure(error));
            return;
        }
        auto *file = new QFile(localFile, this);
        if (!file->open(QIODevice::ReadOnly)) {
            done(failure(file->errorString()));
            delete file;
            return;
        }
        const QString partial = path + QStringLiteral(".part");
        QNetworkRequest request(urlFor(partial));
        request.setHeader(QNetworkRequest::UserAgentHeader, QByteArrayLiteral("PragmaChess/" APP_VERSION));
        if (!m_user.isEmpty())
            request.setRawHeader("Authorization", "Basic " + (m_user + QLatin1Char(':') + m_password).toUtf8().toBase64());
        request.setHeader(QNetworkRequest::ContentLengthHeader, file->size());
        request.setTransferTimeout(120000);
        QNetworkReply *put = m_network->put(request, file);
        file->setParent(put);
        connect(put, &QNetworkReply::finished, this, [this, put, path, partial, done, generation] {
            put->deleteLater();
            if (generation != m_generation)
                return;
            if (put->error() != QNetworkReply::NoError) {
                done(failure(replyError(put)));
                return;
            }
            // Only a complete upload takes the real name.
            QNetworkReply *move = send("MOVE", partial, {},
                                       {{"Destination", urlFor(path).toString(QUrl::FullyEncoded).toUtf8()},
                                        {"Overwrite", "T"}});
            connect(move, &QNetworkReply::finished, this, [this, move, done, generation] {
                move->deleteLater();
                if (generation != m_generation)
                    return;
                done(move->error() == QNetworkReply::NoError ? success() : failure(replyError(move)));
            });
        });
    });
}

void WebDavStore::remove(const QString &path, Callback done)
{
    const int generation = m_generation;
    QNetworkReply *reply = send("DELETE", path);
    connect(reply, &QNetworkReply::finished, this, [this, reply, done, generation] {
        reply->deleteLater();
        if (generation != m_generation)
            return;
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (status == 404 || reply->error() == QNetworkReply::NoError) {
            Result result = success();
            result.notFound = status == 404;
            done(result);
            return;
        }
        done(failure(replyError(reply)));
    });
}

void WebDavStore::abort()
{
    ++m_generation;
    for (QNetworkReply *reply : m_network->findChildren<QNetworkReply *>())
        reply->abort();
}
