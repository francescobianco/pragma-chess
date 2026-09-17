#pragma once

#include "SourceFetch.h"

#include <QDate>
#include <QStringList>
#include <QUrl>

#include <functional>
#include <optional>

class QNetworkAccessManager;
class QTimer;

/// Keys of GameSource::settings for torneionline.com.
namespace TorneiOnlineSettings {
/// Which federation ID GameSource::account is: "fide" or "fsi".
inline constexpr char idType[] = "idType";
/// The player's name as the site shows it, found when the source is checked.
inline constexpr char player[] = "player";
} // namespace TorneiOnlineSettings

/// The tournaments a player played in Italy, from torneionline.com (the
/// Italian Chess Federation's rating site), found by FIDE or FSI ID.
///
/// The site has no moves: each game of the player's score cards becomes a
/// record with players, Elo, tournament, round and result, so the moves can be
/// entered later. Byes and forfeits are left out. The site has no API: pages
/// are read one at a time, with a pause between them.
///
/// State: {"progre": <the site's player number>, "done": [tournament codes
/// ended before the last sync, not read again]}.
class TorneiOnlineFetch : public SourceFetch {
    Q_OBJECT

public:
    /// A tournament of the player's list.
    struct Tournament {
        QString code;
        QString name;
        /// Province, e.g. "TP".
        QString province;
        QDate start;
        QDate end;
    };

    /// A player found by the search.
    struct Player {
        QString progre;
        QString name;
    };

    TorneiOnlineFetch(const GameSource &source, QNetworkAccessManager *network, QObject *parent = nullptr);

    void start() override;
    void abort() override;

    /// The page searching the player with `id`; `idType` is "fide" or "fsi".
    static QUrl searchUrl(const QString &idType, const QString &id);

    /// Pages of the site mix Latin-1 and UTF-8; decodes each byte run as it is.
    static QString decodePage(const QByteArray &page);
    /// The player with exactly `id` in a search result page.
    static std::optional<Player> parsePlayerSearch(const QString &page, const QString &id);
    /// The tournaments of a player's "Tornei disputati" page, newest first.
    static QList<Tournament> parseTournaments(const QString &page);
    /// The player's number (gix) in a tournament's list of participants.
    static QString parseParticipantNumber(const QString &page, const QString &progre);
    /// The games of a player's score card in `tournament`.
    static QList<ImportedGame> parseScoreCard(const QString &page, const Tournament &tournament);

private:
    void fetchTournaments();
    void fetchNextTournament();
    void get(const QUrl &url, std::function<void(const QString &)> handle);

    GameSource m_source;
    QNetworkAccessManager *m_network;
    QNetworkReply *m_reply = nullptr;
    /// Paces the requests; `m_pending` sends the next one.
    QTimer *m_pause;
    std::function<void()> m_pending;
    bool m_requested = false;
    QList<Tournament> m_tournaments;
    QJsonObject m_state;
};
