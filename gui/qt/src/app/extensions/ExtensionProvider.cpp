#include "ExtensionProvider.h"

#include "EnCroissantCatalog.h"
#include "app/sources/SourceFetch.h"

#include <QNetworkAccessManager>
#include <QNetworkReply>

EnCroissantProvider::EnCroissantProvider(QObject *parent)
    : ExtensionProvider(parent)
    , m_network(new QNetworkAccessManager(this))
{
}

QString EnCroissantProvider::description() const
{
    return tr("The registry of En Croissant, another free chess program: engines downloaded from their "
              "authors' own pages, and the databases it converted.");
}

void EnCroissantProvider::fetch()
{
    if (m_pending > 0)
        return;
    m_error.clear();
    const QString system = ThisComputer::system();
    const bool bmi2 = ThisComputer::hasBmi2();
    const auto get = [this](const QString &url, auto read) {
        ++m_pending;
        QNetworkRequest request{QUrl(url)};
        request.setHeader(QNetworkRequest::UserAgentHeader, SourceFetch::userAgent());
        QNetworkReply *reply = m_network->get(request);
        connect(reply, &QNetworkReply::finished, this, [this, reply, read] {
            reply->deleteLater();
            if (reply->error() == QNetworkReply::NoError)
                read(reply->readAll());
            else
                m_error = reply->errorString();
            if (--m_pending > 0)
                return;
            m_extensions = m_engines + m_databases + m_puzzles;
            if (m_extensions.isEmpty() && !m_error.isEmpty())
                Q_EMIT failed(m_error);
            else
                Q_EMIT ready();
        });
    };
    get(EnCroissantCatalog::enginesUrl(system, bmi2),
        [this, system, bmi2](const QByteArray &json) { m_engines = EnCroissantCatalog::engines(json, system, bmi2); });
    get(EnCroissantCatalog::databasesUrl(), [this](const QByteArray &json) {
        m_databases = EnCroissantCatalog::databases(json, Extension::Kind::Database);
    });
    get(EnCroissantCatalog::puzzlesUrl(), [this](const QByteArray &json) {
        m_puzzles = EnCroissantCatalog::databases(json, Extension::Kind::Puzzles);
    });
}
