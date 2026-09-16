#pragma once

#include "SourceFetch.h"

#include <optional>

class QNetworkAccessManager;

/// Games of a lichess.org account, streamed as NDJSON oldest first, signed in
/// with the source's OAuth token.
///
/// State: {"createdAt": <creation of the last game, ms since epoch>}.
class LichessFetch : public SourceFetch {
    Q_OBJECT

public:
    LichessFetch(const GameSource &source, QNetworkAccessManager *network, QObject *parent = nullptr);

    void start() override;
    void abort() override;

    /// A game of the export; nothing for unfinished games or other variants.
    static std::optional<ImportedGame> parseGame(const QJsonObject &game);

private:
    void readLines(bool flush);

    GameSource m_source;
    QNetworkAccessManager *m_network;
    QNetworkReply *m_reply = nullptr;
    QByteArray m_buffer;
    QList<ImportedGame> m_batch;
    QJsonObject m_state;
};
