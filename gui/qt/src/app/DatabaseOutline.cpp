#include "DatabaseOutline.h"

#include <QRegularExpression>

#include <algorithm>

namespace {

QString tag(const GameRecord &game, const QString &name)
{
    for (const PgnTag &tag : game.tags) {
        if (tag.name == name)
            return tag.value.trimmed();
    }
    return {};
}

/// "https://lichess.org/study/<study>/<chapter>" → the two ids.
QRegularExpressionMatch chapterUrl(const GameRecord &game)
{
    static const QRegularExpression ids(QStringLiteral(R"(/study/([A-Za-z0-9]+)(?:/([A-Za-z0-9]+))?)"));
    return ids.match(tag(game, QStringLiteral("ChapterURL")));
}

} // namespace

void DatabaseOutline::add(const GameRecord &game, const PlayerRoles &roles)
{
    for (const QString &player : {game.white, game.black}) {
        if (const PlayerRole role = roles.value(player); role != PlayerRole::None && !player.isEmpty())
            ++players[role][player];
    }
    if (const QString code = ecoCode(game.eco); !code.isEmpty())
        ++eco[code.left(1)][code];
    if (const QString event = eventName(game.event); !event.isEmpty())
        ++events[event];
    if (const int gameYear = year(game.date); gameYear > 0)
        ++years[gameYear];
    if (const QString key = studyKey(game); !key.isEmpty()) {
        auto study = std::find_if(studies.begin(), studies.end(), [&](const Study &s) { return s.key == key; });
        if (study == studies.end()) {
            const QString name = tag(game, QStringLiteral("StudyName"));
            studies.append({key, name.isEmpty() ? key : name, 0, {}});
            study = studies.end() - 1;
        }
        ++study->games;
        const QString chapter = chapterKey(game);
        auto entry = std::find_if(study->chapters.begin(), study->chapters.end(),
                                  [&](const StudyChapter &c) { return c.key == chapter; });
        if (entry == study->chapters.end()) {
            QString name = tag(game, QStringLiteral("ChapterName"));
            if (name.isEmpty())
                name = eventName(game.event);
            study->chapters.append({chapter, name.isEmpty() ? chapter.section(QLatin1Char('/'), 1) : name, 0});
            entry = study->chapters.end() - 1;
        }
        ++entry->games;
    }
}

QString DatabaseOutline::studyKey(const GameRecord &game)
{
    if (const QRegularExpressionMatch match = chapterUrl(game); match.hasMatch())
        return match.captured(1);
    return tag(game, QStringLiteral("StudyName"));
}

QString DatabaseOutline::chapterKey(const GameRecord &game)
{
    const QString study = studyKey(game);
    if (study.isEmpty())
        return {};
    const QRegularExpressionMatch match = chapterUrl(game);
    const QString chapter = match.hasMatch() && !match.captured(2).isEmpty() ? match.captured(2)
                                                                             : tag(game, QStringLiteral("ChapterName"));
    return study + QLatin1Char('/') + chapter;
}

QString DatabaseOutline::ecoCode(const QString &eco)
{
    const QString code = eco.trimmed().left(3).toUpper();
    if (code.size() != 3 || code.at(0) < QLatin1Char('A') || code.at(0) > QLatin1Char('E') || !code.at(1).isDigit()
        || !code.at(2).isDigit())
        return {};
    return code;
}

int DatabaseOutline::year(const QString &date)
{
    bool ok = false;
    const int value = date.trimmed().left(4).toInt(&ok);
    return ok && value > 0 ? value : 0;
}

QString DatabaseOutline::eventName(const QString &event)
{
    const QString name = event.trimmed();
    return name == QLatin1String("?") || name == QLatin1String("-") ? QString() : name;
}

bool DatabaseOutline::hasRole(const GameRecord &game, const PlayerRoles &roles, PlayerRole role)
{
    return roles.value(game.white) == role || roles.value(game.black) == role;
}
