#include "LichessStudyFetch.h"

#include "SourceCredentials.h"

#include "app/GameDatabase.h"

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>

namespace {

const QString kSite = QStringLiteral("lichess.org");
constexpr char kLastModified[] = "lastModified";

} // namespace

LichessStudyFetch::LichessStudyFetch(const GameSource &source, QNetworkAccessManager *network, GameDatabase *database,
                                     QObject *parent)
    : SourceFetch(parent)
    , m_source(source)
    , m_network(network)
    , m_database(database)
{
}

PgnFilePlan::Mode LichessStudyFetch::mode(const GameSource &source)
{
    return PgnFilePlan::modeFromKey(source.settings.value(QLatin1String(LichessStudySettings::mode)).toString());
}

QNetworkRequest LichessStudyFetch::exportRequest(const QString &studyId, const QString &token)
{
    // Comments, variations and clocks are in the export unless turned off.
    QNetworkRequest request(QUrl(QStringLiteral("https://lichess.org/api/study/%1.pgn").arg(studyId)));
    request.setHeader(QNetworkRequest::UserAgentHeader, userAgent());
    request.setTransferTimeout(60000);
    if (!token.isEmpty())
        request.setRawHeader("Authorization", "Bearer " + token.toUtf8());
    return request;
}

void LichessStudyFetch::abort()
{
    m_aborted = true;
    if (m_reply)
        m_reply->abort();
}

void LichessStudyFetch::start()
{
    const std::optional<QString> id =
        LichessStudy::studyId(m_source.settings.value(QLatin1String(LichessStudySettings::url)).toString());
    if (!id || !m_database) {
        QTimer::singleShot(0, this, [this] { Q_EMIT finished(tr("The address of the lichess study is not valid.")); });
        return;
    }
    if (!PgnFilePlan::reads(mode(m_source))) {
        // Writing to the study is not there yet: nothing to do.
        QTimer::singleShot(0, this, [this] { Q_EMIT finished(QString()); });
        return;
    }
    QNetworkRequest request = exportRequest(*id, SourceCredentials::token(m_source.uuid));
    // Unchanged since the last sync: lichess answers 304 and sends nothing.
    const QString lastModified = m_source.state.value(QLatin1String(kLastModified)).toString();
    if (!lastModified.isEmpty())
        request.setRawHeader("If-Modified-Since", lastModified.toLatin1());
    m_reply = m_network->get(request);
    connect(m_reply, &QNetworkReply::finished, this, [this] {
        QNetworkReply *reply = m_reply;
        m_reply = nullptr;
        reply->deleteLater();
        if (m_aborted)
            return;
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (status == 304) {
            Q_EMIT finished(QString());
            return;
        }
        if (status == 404) {
            Q_EMIT finished(tr("lichess.org did not find the study."));
            return;
        }
        if (status == 403) {
            // Public to look at is not enough: the author chooses who may export it.
            Q_EMIT finished(tr("lichess.org does not let this study be downloaded: it is private, or its author "
                               "lets only its members export it (Share & export)."));
            return;
        }
        if (reply->error() != QNetworkReply::NoError) {
            Q_EMIT finished(httpError(reply, kSite));
            return;
        }
        apply(reply->readAll(), QString::fromLatin1(reply->rawHeader("Last-Modified")));
    });
}

QHash<QString, PgnFilePlan::DatabaseGame> LichessStudyFetch::databaseGames(QHash<QString, qint64> *indexes) const
{
    QHash<QString, PgnFilePlan::DatabaseGame> games;
    for (qint64 index = 0; index < m_database->gameCount(); ++index) {
        const GameRecord header = m_database->header(index);
        if (header.uid.isEmpty())
            continue;
        games.insert(header.uid, {header.modified, header.state == GameState::Live});
        if (indexes)
            indexes->insert(header.uid, index);
    }
    return games;
}

QHash<QString, QString> LichessStudyFetch::linked() const
{
    QHash<QString, QString> result;
    for (const SourceLink &link : m_database->sourceLinks()) {
        if (link.sourceUuid == m_source.uuid)
            result.insert(link.externalId, link.gameUid);
    }
    return result;
}

void LichessStudyFetch::apply(const QByteArray &pgn, const QString &lastModified)
{
    int unreadable = 0;
    const QList<LichessStudy::Chapter> chapters = LichessStudy::chapters(pgn, &unreadable);
    const PgnFilePlan::Base base = PgnFilePlan::baseFromJson(m_source.state);
    const LichessStudy::ReadPlan plan = LichessStudy::planRead(chapters, base, databaseGames(), linked());

    // New chapters, and lichess's version of those changed on both sides.
    QList<ImportedGame> imports;
    for (const int i : plan.imports)
        imports << ImportedGame{chapters.at(i).id, chapters.at(i).game};
    for (const int i : plan.conflicts)
        imports << ImportedGame{LichessStudy::conflictId(chapters.at(i)), chapters.at(i).game};
    QString error;
    int added = 0;
    if (!imports.isEmpty()) {
        added = m_database->importGames(m_source.id, imports, &error);
        if (added < 0) {
            Q_EMIT finished(tr("Could not save the games: %1").arg(error));
            return;
        }
    }

    // Changed on lichess only: the game takes the new version.
    QHash<QString, qint64> indexes;
    databaseGames(&indexes);
    QList<qint64> updated;
    for (const auto &[i, uid] : plan.updates) {
        const qint64 index = indexes.value(uid, -1);
        if (index < 0)
            continue;
        GameRecord game = chapters.at(i).game;
        game.uid = uid;
        if (!m_database->replaceGame(index, game, &error)) {
            Q_EMIT finished(tr("Could not save the games: %1").arg(error));
            return;
        }
        updated << index;
    }
    if (added > 0 || !updated.isEmpty())
        Q_EMIT gamesChanged(added, updated);

    QJsonObject state =
        PgnFilePlan::baseToJson(LichessStudy::nextBase(base, chapters, databaseGames(), linked()));
    if (!lastModified.isEmpty())
        state.insert(QLatin1String(kLastModified), lastModified);
    saveState(state, LichessStudy::studyName(chapters));
    Q_EMIT finished(unreadable > 0 ? tr("%n chapter(s) of the study could not be read.", nullptr, unreadable)
                                   : QString());
}

void LichessStudyFetch::saveState(const QJsonObject &state, const QString &studyName)
{
    for (GameSource source : m_database->sources()) {
        if (source.id != m_source.id)
            continue;
        source.state = state;
        if (!studyName.isEmpty()) // It may have been renamed on lichess.
            source.settings.insert(QLatin1String(LichessStudySettings::name), studyName);
        m_database->updateSource(source, nullptr);
        return;
    }
}
