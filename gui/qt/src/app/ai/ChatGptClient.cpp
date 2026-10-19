#include "ChatGptClient.h"

#include "app/sources/SourceFetch.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QUrlQuery>

namespace {

/// A token this close to its end is renewed before it is used.
constexpr int kRenewBeforeSeconds = 120;

QByteArray formBody(const QUrlQuery &form)
{
    QByteArray body;
    for (const auto &[key, value] : form.queryItems(QUrl::FullyDecoded)) {
        if (!body.isEmpty())
            body += '&';
        body += QUrl::toPercentEncoding(key) + '=' + QUrl::toPercentEncoding(value);
    }
    return body;
}

} // namespace

ChatGptClient::ChatGptClient(QObject *parent)
    : QObject(parent)
    , m_network(new QNetworkAccessManager(this))
{
}

void ChatGptClient::ask(const QString &instructions, const QJsonArray &input, const QJsonArray &tools)
{
    if (isBusy())
        return;
    m_instructions = instructions;
    m_input = input;
    m_tools = tools;
    m_renewedAfterRefusal = false;
    m_preparing = true;
    withToken([this] { withModel([this] { send(); }); });
}

void ChatGptClient::cancel()
{
    m_preparing = false;
    if (m_reply) {
        QNetworkReply *reply = m_reply;
        m_reply = nullptr;
        reply->abort();
        reply->deleteLater();
    }
}

QNetworkRequest ChatGptClient::apiRequest(const QUrl &url) const
{
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, SourceFetch::userAgent());
    request.setRawHeader("Authorization", "Bearer " + m_credentials.accessToken.toUtf8());
    return request;
}

void ChatGptClient::withToken(const std::function<void()> &then)
{
    const QDateTime now = QDateTime::currentDateTimeUtc();
    if (m_credentials.expiresAt.isValid() && now.secsTo(m_credentials.expiresAt) > kRenewBeforeSeconds) {
        then();
        return;
    }
    if (m_credentials.refreshToken.isEmpty()) {
        m_preparing = false;
        Q_EMIT signedOut();
        return;
    }
    QNetworkRequest request(QUrl(QString::fromLatin1(ChatGptAuth::kTokenUrl)));
    request.setHeader(QNetworkRequest::UserAgentHeader, SourceFetch::userAgent());
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/x-www-form-urlencoded"));
    QUrlQuery form;
    form.addQueryItem(QStringLiteral("grant_type"), QStringLiteral("refresh_token"));
    form.addQueryItem(QStringLiteral("client_id"), m_credentials.clientId);
    form.addQueryItem(QStringLiteral("refresh_token"), m_credentials.refreshToken);
    form.addQueryItem(QStringLiteral("resource"), QString::fromLatin1(ChatGptAuth::kResource));
    QNetworkReply *reply = m_network->post(request, formBody(form));
    connect(reply, &QNetworkReply::finished, this, [this, reply, then] {
        reply->deleteLater();
        if (!m_preparing)
            return; // Cancelled meanwhile.
        const QJsonObject response = QJsonDocument::fromJson(reply->readAll()).object();
        ChatGptAuth::Credentials renewed = m_credentials;
        QString error;
        if (ChatGptAuth::applyTokenResponse(renewed, response, QDateTime::currentDateTimeUtc(), &error)) {
            m_credentials = renewed;
            Q_EMIT credentialsChanged(m_credentials);
            then();
            return;
        }
        m_preparing = false;
        // A refresh token no longer good is a connection gone; a network failure is not.
        if (ChatGptResponses::isSignedOut(response.value(QStringLiteral("error")).toString()))
            Q_EMIT signedOut();
        else
            Q_EMIT failed(error.isEmpty() ? SourceFetch::httpError(reply, QStringLiteral("ChatGPT")) : error);
    });
}

void ChatGptClient::listModels()
{
    if (isBusy())
        return;
    m_preparing = true;
    withToken([this] {
        QNetworkReply *reply = m_network->get(apiRequest(QUrl(QString::fromLatin1(ChatGptResponses::kModelsUrl))));
        connect(reply, &QNetworkReply::finished, this, [this, reply] {
            reply->deleteLater();
            if (!m_preparing)
                return;
            m_preparing = false;
            const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            if (status == 401) {
                Q_EMIT signedOut();
                return;
            }
            if (reply->error() != QNetworkReply::NoError) {
                Q_EMIT failed(SourceFetch::httpError(reply, QStringLiteral("ChatGPT")));
                return;
            }
            Q_EMIT modelsListed(ChatGptResponses::listedModels(QJsonDocument::fromJson(reply->readAll()).object()));
        });
    });
}

void ChatGptClient::withModel(const std::function<void()> &then)
{
    if (!m_chosenModel.isEmpty())
        m_model = m_chosenModel;
    if (!m_model.isEmpty()) {
        then();
        return;
    }
    QNetworkReply *reply = m_network->get(apiRequest(QUrl(QString::fromLatin1(ChatGptResponses::kModelsUrl))));
    connect(reply, &QNetworkReply::finished, this, [this, reply, then] {
        reply->deleteLater();
        if (!m_preparing)
            return;
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        m_model = ChatGptResponses::pickModel(QJsonDocument::fromJson(reply->readAll()).object());
        if (m_model.isEmpty()) {
            m_preparing = false;
            if (status == 401)
                Q_EMIT signedOut();
            else
                Q_EMIT failed(reply->error() != QNetworkReply::NoError
                                  ? SourceFetch::httpError(reply, QStringLiteral("ChatGPT"))
                                  : tr("Your ChatGPT plan offers no model to Pragma Chess."));
            return;
        }
        then();
    });
}

void ChatGptClient::send()
{
    m_preparing = false;
    m_finished = false;
    m_reader = {};
    m_items = {};
    QNetworkRequest request = apiRequest(QUrl(QString::fromLatin1(ChatGptResponses::kResponsesUrl)));
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    request.setRawHeader("Accept", "text/event-stream");
    const QJsonObject body = ChatGptResponses::requestBody(m_model, m_instructions, m_input, m_tools);
    m_reply = m_network->post(request, QJsonDocument(body).toJson(QJsonDocument::Compact));
    QNetworkReply *reply = m_reply;
    connect(reply, &QNetworkReply::readyRead, this, [this, reply] {
        if (reply != m_reply || reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() >= 400)
            return; // An error's body is read whole when it ends.
        for (const ChatGptResponses::Event &event : m_reader.feed(reply->readAll())) {
            switch (event.kind) {
            case ChatGptResponses::Event::Kind::Delta:
                Q_EMIT answerDelta(event.text);
                break;
            case ChatGptResponses::Event::Kind::ItemDone:
                m_items.append(event.item);
                break;
            case ChatGptResponses::Event::Kind::Completed:
                m_finished = true;
                Q_EMIT answerCompleted(m_items);
                break;
            case ChatGptResponses::Event::Kind::Failed:
                m_finished = true;
                if (event.code == QLatin1String("subscription_sharing_usage_limit_exceeded"))
                    Q_EMIT failed(tr("Your ChatGPT plan has reached its usage limit: see Usage in ChatGPT's settings."));
                else
                    Q_EMIT failed(event.message.isEmpty() ? event.code : event.message);
                break;
            case ChatGptResponses::Event::Kind::Other:
                break;
            }
        }
    });
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        reply->deleteLater();
        if (reply != m_reply)
            return; // Cancelled.
        m_reply = nullptr;
        if (m_finished)
            return;
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QJsonObject error = QJsonDocument::fromJson(reply->readAll()).object().value(QStringLiteral("error")).toObject();
        if (status == 401) {
            // Refused once, the token is renewed and the request sent again; refused twice, the connection is gone.
            if (!m_renewedAfterRefusal && !m_credentials.refreshToken.isEmpty()) {
                m_renewedAfterRefusal = true;
                m_credentials.expiresAt = {};
                m_preparing = true;
                withToken([this] { send(); });
                return;
            }
            Q_EMIT signedOut();
            return;
        }
        if (status == 403 && error.value(QStringLiteral("code")).toString()
                                 == QLatin1String("subscription_sharing_user_not_eligible")) {
            Q_EMIT failed(tr("This ChatGPT account cannot share its plan with apps: only personal Plus and Pro plans can."));
            return;
        }
        if (status == 429 && error.value(QStringLiteral("code")).toString()
                                 == QLatin1String("subscription_sharing_usage_limit_exceeded")) {
            Q_EMIT failed(tr("Your ChatGPT plan has reached its usage limit: see Usage in ChatGPT's settings."));
            return;
        }
        const QString message = error.value(QStringLiteral("message")).toString();
        Q_EMIT failed(!message.isEmpty() ? message : SourceFetch::httpError(reply, QStringLiteral("ChatGPT")));
    });
}
