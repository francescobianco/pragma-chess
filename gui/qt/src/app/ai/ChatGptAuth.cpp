#include "ChatGptAuth.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>
#include <QStandardPaths>
#include <QUrlQuery>
#include <QUuid>

namespace ChatGptAuth {

void Credentials::signOut()
{
    accessToken.clear();
    refreshToken.clear();
    idToken.clear();
    scopes.clear();
    expiresAt = {};
    email.clear();
    name.clear();
}

QJsonObject Credentials::toJson() const
{
    QJsonObject object;
    object.insert(QStringLiteral("hostId"), hostId);
    if (!clientId.isEmpty())
        object.insert(QStringLiteral("clientId"), clientId);
    if (isSignedIn()) {
        object.insert(QStringLiteral("accessToken"), accessToken);
        object.insert(QStringLiteral("refreshToken"), refreshToken);
        object.insert(QStringLiteral("idToken"), idToken);
        object.insert(QStringLiteral("scopes"), QJsonArray::fromStringList(scopes));
        object.insert(QStringLiteral("expiresAt"), expiresAt.toUTC().toString(Qt::ISODate));
        object.insert(QStringLiteral("email"), email);
        object.insert(QStringLiteral("name"), name);
    }
    return object;
}

Credentials Credentials::fromJson(const QJsonObject &object)
{
    Credentials credentials;
    credentials.hostId = object.value(QStringLiteral("hostId")).toString();
    credentials.clientId = object.value(QStringLiteral("clientId")).toString();
    credentials.accessToken = object.value(QStringLiteral("accessToken")).toString();
    credentials.refreshToken = object.value(QStringLiteral("refreshToken")).toString();
    credentials.idToken = object.value(QStringLiteral("idToken")).toString();
    for (const QJsonValue &scope : object.value(QStringLiteral("scopes")).toArray())
        credentials.scopes << scope.toString();
    credentials.expiresAt = QDateTime::fromString(object.value(QStringLiteral("expiresAt")).toString(), Qt::ISODate);
    credentials.email = object.value(QStringLiteral("email")).toString();
    credentials.name = object.value(QStringLiteral("name")).toString();
    return credentials;
}

QUrl authorizeUrl(const Credentials &credentials, const QUrl &redirect, const QString &state, const QString &nonce,
                  const QByteArray &codeChallenge, const QString &appName)
{
    QUrlQuery query;
    const bool registering = credentials.clientId.isEmpty();
    query.addQueryItem(QStringLiteral("client_id"),
                       registering ? QString::fromLatin1(kDynamicClient) : credentials.clientId);
    if (registering)
        query.addQueryItem(QStringLiteral("agent_name_hint"), appName);
    query.addQueryItem(QStringLiteral("ext_agent_host_id"), credentials.hostId);
    if (!credentials.idToken.isEmpty())
        query.addQueryItem(QStringLiteral("id_token_hint"), credentials.idToken);
    query.addQueryItem(QStringLiteral("response_type"), QStringLiteral("code"));
    query.addQueryItem(QStringLiteral("redirect_uri"), redirect.toString());
    query.addQueryItem(QStringLiteral("scope"),
                       QStringLiteral("openid profile email offline_access resource.invoke ") + QLatin1String(kPlanScope));
    query.addQueryItem(QStringLiteral("resource"), QString::fromLatin1(kResource));
    query.addQueryItem(QStringLiteral("state"), state);
    query.addQueryItem(QStringLiteral("nonce"), nonce);
    query.addQueryItem(QStringLiteral("code_challenge_method"), QStringLiteral("S256"));
    query.addQueryItem(QStringLiteral("code_challenge"), QString::fromLatin1(codeChallenge));
    QUrl url(QString::fromLatin1(kAuthorizeUrl));
    // Every reserved character encoded: a redirect address inside a query
    // must not be read as part of it.
    QString encoded;
    for (const auto &[key, value] : query.queryItems(QUrl::FullyDecoded)) {
        if (!encoded.isEmpty())
            encoded += QLatin1Char('&');
        encoded += QString::fromLatin1(QUrl::toPercentEncoding(key)) + QLatin1Char('=')
            + QString::fromLatin1(QUrl::toPercentEncoding(value));
    }
    url.setQuery(encoded, QUrl::StrictMode);
    return url;
}

QJsonObject jwtClaims(const QString &jwt)
{
    const QStringList parts = jwt.split(QLatin1Char('.'));
    if (parts.size() < 2)
        return {};
    const QByteArray payload = QByteArray::fromBase64(parts.at(1).toLatin1(),
                                                      QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals);
    return QJsonDocument::fromJson(payload).object();
}

bool applyTokenResponse(Credentials &credentials, const QJsonObject &response, const QDateTime &now, QString *error)
{
    const QString accessToken = response.value(QStringLiteral("access_token")).toString();
    if (accessToken.isEmpty()) {
        if (error) {
            QString text = response.value(QStringLiteral("error_description")).toString();
            if (text.isEmpty())
                text = response.value(QStringLiteral("error")).toString();
            *error = text;
        }
        return false;
    }
    credentials.accessToken = accessToken;
    // A refresh rotates the refresh token: the new one replaces the old, together with the rest.
    const QString refreshToken = response.value(QStringLiteral("refresh_token")).toString();
    if (!refreshToken.isEmpty())
        credentials.refreshToken = refreshToken;
    const QString idToken = response.value(QStringLiteral("id_token")).toString();
    if (!idToken.isEmpty()) {
        credentials.idToken = idToken;
        const QJsonObject claims = jwtClaims(idToken);
        credentials.email = claims.value(QStringLiteral("email")).toString();
        credentials.name = claims.value(QStringLiteral("name")).toString();
    }
    if (response.contains(QStringLiteral("scope")))
        credentials.scopes = response.value(QStringLiteral("scope")).toString().split(QLatin1Char(' '), Qt::SkipEmptyParts);
    const int expiresIn = response.value(QStringLiteral("expires_in")).toInt(3600);
    credentials.expiresAt = now.addSecs(expiresIn);
    return true;
}

QString defaultPath()
{
    return QDir(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation))
        .filePath(QStringLiteral("chatgpt-auth.json"));
}

Credentials load(const QString &path)
{
    QFile file(path);
    Credentials credentials;
    if (file.open(QIODevice::ReadOnly))
        credentials = Credentials::fromJson(QJsonDocument::fromJson(file.readAll()).object());
    // This computer's id is chosen before its first sign-in and never changes.
    if (credentials.hostId.isEmpty())
        credentials.hostId = QStringLiteral("urn:uuid:") + QUuid::createUuid().toString(QUuid::WithoutBraces);
    return credentials;
}

bool save(const Credentials &credentials, const QString &path)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return false;
    file.write(QJsonDocument(credentials.toJson()).toJson());
    if (!file.commit())
        return false;
    // Owner-only; later writes keep the file's permissions.
    return QFile::setPermissions(path, QFile::ReadOwner | QFile::WriteOwner);
}

} // namespace ChatGptAuth
