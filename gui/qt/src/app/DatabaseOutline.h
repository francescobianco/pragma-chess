#pragma once

#include "GameRecord.h"
#include "PlayerRole.h"

#include <QMap>
#include <QString>

/// What the games of a database are about, for navigating it: the players the
/// user said are them, friends or opponents, ECO codes grouped by letter,
/// events and years, each with its number of games.
/// Only values that games actually have appear.
struct DatabaseOutline {
    /// Role → player → games; a game counts for each side with a role.
    QMap<PlayerRole, QMap<QString, int>> players;
    /// "E" → {"E10": 3, "E11": 1}
    QMap<QString, QMap<QString, int>> eco;
    QMap<QString, int> events;
    QMap<int, int> years;
    /// TimeControl tag → games (TimeControl::of).
    QMap<QString, int> timeControls;

    /// A study the games came from (a lichess study: its tags StudyName,
    /// ChapterName, ChapterURL), with its chapters in the order met.
    struct StudyChapter {
        QString key;
        QString name;
        int games = 0;
    };
    struct Study {
        QString key;
        QString name;
        int games = 0;
        QList<StudyChapter> chapters;
    };
    /// In the order the games have them.
    QList<Study> studies;

    void add(const GameRecord &game, const PlayerRoles &roles = {});
    /// Whether a player with `role` plays in `game`.
    static bool hasRole(const GameRecord &game, const PlayerRoles &roles, PlayerRole role);

    /// The study a game belongs to: the study's id when its ChapterURL has
    /// one, else its StudyName; empty for a game of no study.
    static QString studyKey(const GameRecord &game);
    /// The chapter of that study: "<study>/<chapter id or ChapterName>".
    static QString chapterKey(const GameRecord &game);

    /// "e10", "E10a", " E10 " → "E10"; empty when not an ECO code.
    static QString ecoCode(const QString &eco);
    /// "1858.??.??" → 1858; 0 when the year is unknown.
    static int year(const QString &date);
    /// The event name, or empty for unknown events ("?", "-").
    static QString eventName(const QString &event);
};
