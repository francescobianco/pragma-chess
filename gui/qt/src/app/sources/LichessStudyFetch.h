#pragma once

#include "LichessStudy.h"
#include "PgnFilePlan.h"
#include "SourceFetch.h"

class GameDatabase;
class QNetworkAccessManager;
class QNetworkReply;

/// A lichess.org study kept in the database (LichessStudy): each chapter a
/// game, with its comments, variations and the tags that say which study
/// and chapter it is. Public studies need no account; a private one needs
/// signing in. Chapters changed on lichess replace their game, unless it
/// changed here too: then lichess's version comes in as a game of its own.
/// The export is asked for only when lichess says the study changed.
///
/// Settings: {"url": "https://lichess.org/study/…", "name": "the study's
/// name", "mode": "readwrite" | "read" | "write" (PgnFilePlan's keys)}.
/// State: the base by chapter id (PgnFilePlan::baseToJson) and
/// "lastModified", lichess's date of the study at the last sync.
class LichessStudyFetch : public SourceFetch {
    Q_OBJECT

public:
    LichessStudyFetch(const GameSource &source, QNetworkAccessManager *network, GameDatabase *database,
                      QObject *parent = nullptr);

    void start() override;
    void abort() override;

    static PgnFilePlan::Mode mode(const GameSource &source);
    /// The export of a study, the token added when there is one.
    static QNetworkRequest exportRequest(const QString &studyId, const QString &token);

private:
    void apply(const QByteArray &pgn, const QString &lastModified);
    QHash<QString, PgnFilePlan::DatabaseGame> databaseGames(QHash<QString, qint64> *indexes = nullptr) const;
    /// Chapter id (or conflict id) → uid of the games this source imported.
    QHash<QString, QString> linked() const;
    /// Stores the state, and the study's name as lichess has it now.
    void saveState(const QJsonObject &state, const QString &studyName);

    GameSource m_source;
    QNetworkAccessManager *m_network;
    GameDatabase *m_database;
    QNetworkReply *m_reply = nullptr;
    bool m_aborted = false;
};

namespace LichessStudySettings {
inline constexpr char url[] = "url";
inline constexpr char name[] = "name";
inline constexpr char mode[] = "mode";
} // namespace LichessStudySettings
