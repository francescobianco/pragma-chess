#pragma once

#include "GameRecord.h"

#include <QString>
#include <QUuid>

/// Universal ids of games and databases, shared by every copy of a database
/// on every device (docs/phone-link.md, "One corpus: reconciliation").
namespace GameIdentity {

/// Namespace of the content-derived (version 5) game uids.
inline const QUuid kGameNamespace{QStringLiteral("7b0c5a1e-3f0d-4a55-9e3c-6f1d0a9b4c00")};
/// Lineage ids of the databases we ship: every install and update is the same database.
inline const QString kOpeningNamesLineage = QStringLiteral("7b0c5a1e-3f0d-4a55-9e3c-6f1d0a9b4c01");
inline const QString kClassicGamesLineage = QStringLiteral("7b0c5a1e-3f0d-4a55-9e3c-6f1d0a9b4c02");
inline const QString kItalianOpeningNamesLineage = QStringLiteral("7b0c5a1e-3f0d-4a55-9e3c-6f1d0a9b4c03");

/// What a game's uid is made from: white, black, event, site, date, round,
/// result, start FEN and UCI moves, trimmed and joined by newlines.
QString content(const GameRecord &game);

/// The uid of a game created with this content. `occurrence` > 1 tells
/// identical games in one database apart ("\n#2", "\n#3", …), so they stay
/// deterministic across devices.
QString uid(const GameRecord &game, int occurrence = 1);

/// RFC 4122 version 5 (SHA-1) UUID, lowercase with dashes and no braces.
QString uuidV5(const QUuid &nameSpace, const QByteArray &name);

/// A new random lineage id for a database.
QString newLineageId();

/// The current time as stored in `modified`.
QString now();

/// Whether two versions of a game (same uid) say the same thing: the uid
/// content plus ECO and ratings.
bool sameContent(const GameRecord &a, const GameRecord &b);

} // namespace GameIdentity
