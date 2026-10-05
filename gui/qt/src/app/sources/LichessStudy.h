#pragma once

#include "PgnFilePlan.h"

#include "app/GameRecord.h"

#include <QByteArray>
#include <QHash>
#include <QList>
#include <QSet>
#include <QString>

#include <optional>

/// A lichess.org study as a source of games (LichessStudyFetch): what does
/// not need the network, pure and unit-tested.
///
/// A study is a list of chapters, each a game with its comments and
/// variations; the export (/api/study/{id}.pgn) gives every chapter as a PGN
/// game with the tags StudyName, ChapterName and ChapterURL. Each chapter
/// becomes a game of the database, its chapter told by those tags — the
/// study's chapters have nothing to do with the chapters of a project. A
/// chapter is known by its id (the last part of ChapterURL), which lichess
/// never changes, so a chapter changed on lichess replaces its game.
namespace LichessStudy {

/// The id of the study a URL (or the id itself) names:
/// "https://lichess.org/study/iob5mNFl/nUeONbjs" → "iob5mNFl".
std::optional<QString> studyId(const QString &urlOrId);

/// The study's page.
QString studyUrl(const QString &studyId);

/// The tag of a game: its value, empty when it has none.
QString tag(const GameRecord &game, const QString &name);

/// A chapter of the export: its id, a hash of its text (whether it changed)
/// and its game.
struct Chapter {
    QString id;
    QString hash;
    GameRecord game;
};

/// The chapters of an export, in the study's order; `unreadable` counts the
/// games whose moves could not be read or that carry no chapter id.
QList<Chapter> chapters(const QByteArray &pgn, int *unreadable = nullptr);

/// The name of the study, from its chapters; empty when they do not say.
QString studyName(const QList<Chapter> &chapters);

/// What a read does, chapter by chapter, against the base (by chapter id:
/// the chapter's hash and its game's `modified` at the last sync) and the
/// games this source imported (chapter id → uid).
struct ReadPlan {
    /// Chapters the database has never had: new games.
    QList<int> imports;
    /// Chapters changed on lichess, whose game did not change here: it takes
    /// lichess's version (index of the chapter, uid of the game).
    QList<QPair<int, QString>> updates;
    /// Chapters changed on both sides: lichess's version comes in as a game
    /// of its own, the database's keeps its place.
    QList<int> conflicts;
};

ReadPlan planRead(const QList<Chapter> &chapters, const PgnFilePlan::Base &base,
                  const QHash<QString, PgnFilePlan::DatabaseGame> &games, const QHash<QString, QString> &linked);

/// The base after a read: each chapter's hash, and its game's `modified` now.
/// Chapters gone from the study are remembered, so they do not come back.
PgnFilePlan::Base nextBase(const PgnFilePlan::Base &before, const QList<Chapter> &chapters,
                           const QHash<QString, PgnFilePlan::DatabaseGame> &games,
                           const QHash<QString, QString> &linked);

/// External id of a chapter changed on both sides, imported again: its id
/// and the hash of that version, so each version comes in once.
QString conflictId(const Chapter &chapter);

} // namespace LichessStudy
