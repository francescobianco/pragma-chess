#pragma once

#include "SourceFetch.h"

#include <QStringList>

#include <functional>

#include <optional>

class QNetworkAccessManager;

/// Games of a chess.com account, from its public API: the list of monthly
/// archives, then each month from the last one synced.
///
/// State: {"month": "2024/05", "endTime": <last game end, seconds since epoch>}.
class ChessComFetch : public SourceFetch {
    Q_OBJECT

public:
    ChessComFetch(const GameSource &source, QNetworkAccessManager *network, QObject *parent = nullptr);

    void start() override;
    void abort() override;

    /// A game of a monthly archive; nothing for other variants or unreadable games.
    static std::optional<ImportedGame> parseGame(const QJsonObject &game);

private:
    void fetchNextArchive();
    void get(const QUrl &url, std::function<void(const QByteArray &)> handle);

    GameSource m_source;
    QNetworkAccessManager *m_network;
    QNetworkReply *m_reply = nullptr;
    QStringList m_archives;
    QJsonObject m_state;
};
