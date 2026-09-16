#include "DatabaseOutline.h"

void DatabaseOutline::add(const GameRecord &game)
{
    if (const QString code = ecoCode(game.eco); !code.isEmpty())
        ++eco[code.left(1)][code];
    if (const QString event = eventName(game.event); !event.isEmpty())
        ++events[event];
    if (const int gameYear = year(game.date); gameYear > 0)
        ++years[gameYear];
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
