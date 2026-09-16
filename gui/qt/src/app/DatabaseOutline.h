#pragma once

#include "GameRecord.h"

#include <QMap>
#include <QString>

/// What the games of a database are about, for navigating it: ECO codes
/// grouped by letter, events and years, each with its number of games.
/// Only values that games actually have appear.
struct DatabaseOutline {
    /// "E" → {"E10": 3, "E11": 1}
    QMap<QString, QMap<QString, int>> eco;
    QMap<QString, int> events;
    QMap<int, int> years;

    void add(const GameRecord &game);

    /// "e10", "E10a", " E10 " → "E10"; empty when not an ECO code.
    static QString ecoCode(const QString &eco);
    /// "1858.??.??" → 1858; 0 when the year is unknown.
    static int year(const QString &date);
    /// The event name, or empty for unknown events ("?", "-").
    static QString eventName(const QString &event);
};
