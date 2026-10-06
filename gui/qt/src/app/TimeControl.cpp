#include "TimeControl.h"

#include <QCoreApplication>
#include <QRegularExpression>
#include <QStringList>

#include <limits>

namespace {

struct Text {
    Q_DECLARE_TR_FUNCTIONS(TimeControl)
};

const QString kTag = QStringLiteral("TimeControl");

/// One period of a PGN time control: "moves/seconds", "seconds+increment",
/// "seconds" or "*seconds" (sandclock).
struct Period {
    int moves = 0;
    qint64 seconds = 0;
    qint64 increment = 0;
};

std::optional<Period> period(const QString &text)
{
    static const QRegularExpression form(QStringLiteral(R"(^(?:(\d+)/)?\*?(\d+)(?:\+(\d+))?$)"));
    const QRegularExpressionMatch match = form.match(text.trimmed());
    if (!match.hasMatch())
        return std::nullopt;
    return Period{match.captured(1).toInt(), match.captured(2).toLongLong(), match.captured(3).toLongLong()};
}

std::optional<QList<Period>> periods(const QString &value)
{
    QList<Period> list;
    for (const QString &part : value.split(QLatin1Char(':'))) {
        const std::optional<Period> one = period(part);
        if (!one)
            return std::nullopt;
        list << *one;
    }
    return list.isEmpty() ? std::nullopt : std::optional(list);
}

bool isCorrespondence(const QList<Period> &list)
{
    // A move in a day or more: "1/86400".
    return list.size() == 1 && list.first().moves > 0 && list.first().seconds >= 86400
           && list.first().seconds / list.first().moves >= 86400;
}

/// Minutes as players write them: "3", "90", "½", "¼", "1.5"; seconds when
/// they are not a quarter of a minute: "20s".
QString minutes(qint64 seconds)
{
    if (seconds == 15)
        return QString(QChar(0x00BC)); // ¼
    if (seconds == 30)
        return QString(QChar(0x00BD)); // ½
    if (seconds == 45)
        return QString(QChar(0x00BE)); // ¾
    if (seconds % 15 != 0)
        return QStringLiteral("%1s").arg(seconds);
    return QString::number(double(seconds) / 60.0, 'g', 4);
}

} // namespace

namespace TimeControl {

QString of(const GameRecord &game)
{
    for (const PgnTag &tag : game.tags) {
        if (tag.name == kTag) {
            const QString value = tag.value.trimmed();
            return value == QLatin1String("?") ? QString() : value;
        }
    }
    return {};
}

void set(GameRecord &game, const QString &value)
{
    const QString trimmed = value.trimmed();
    for (qsizetype i = 0; i < game.tags.size(); ++i) {
        if (game.tags.at(i).name != kTag)
            continue;
        if (trimmed.isEmpty())
            game.tags.removeAt(i);
        else
            game.tags[i].value = trimmed;
        return;
    }
    if (!trimmed.isEmpty())
        game.tags << PgnTag{kTag, trimmed};
}

Speed speed(const QString &value)
{
    if (value.trimmed() == QLatin1String("-"))
        return Speed::Unlimited;
    const std::optional<QList<Period>> list = periods(value);
    if (!list)
        return Speed::Unknown;
    if (isCorrespondence(*list))
        return Speed::Correspondence;
    const qint64 total = estimatedSeconds(value);
    if (total < 30)
        return Speed::UltraBullet;
    if (total < 180)
        return Speed::Bullet;
    if (total < 480)
        return Speed::Blitz;
    if (total < 1500)
        return Speed::Rapid;
    return Speed::Classical;
}

QString speedName(Speed speed)
{
    switch (speed) {
    case Speed::UltraBullet: return Text::tr("UltraBullet");
    case Speed::Bullet: return Text::tr("Bullet");
    case Speed::Blitz: return Text::tr("Blitz");
    case Speed::Rapid: return Text::tr("Rapid");
    case Speed::Classical: return Text::tr("Classical");
    case Speed::Correspondence: return Text::tr("Correspondence");
    case Speed::Unlimited: return Text::tr("No clock");
    case Speed::Unknown: break;
    }
    return {};
}

qint64 estimatedSeconds(const QString &value)
{
    constexpr qint64 kLast = std::numeric_limits<qint32>::max();
    if (value.trimmed() == QLatin1String("-"))
        return kLast + 1;
    const std::optional<QList<Period>> list = periods(value);
    if (!list)
        return kLast + 2;
    if (isCorrespondence(*list))
        return kLast;
    // The first period, which most games are played in: its time for forty
    // moves (a period of fewer moves counts as such), with its increments.
    const Period &first = list->first();
    return first.seconds + 40 * first.increment;
}

QString shortText(const QString &value)
{
    const QString trimmed = value.trimmed();
    if (trimmed == QLatin1String("-"))
        return Text::tr("no clock");
    const std::optional<QList<Period>> list = periods(trimmed);
    if (!list)
        return trimmed;
    if (isCorrespondence(*list)) {
        const qint64 days = list->first().seconds / 86400 / list->first().moves;
        return days == 1 ? Text::tr("1 day") : Text::tr("%1 days").arg(days);
    }
    QStringList parts;
    for (const Period &one : *list) {
        QString text = one.moves > 0 ? QStringLiteral("%1/").arg(one.moves) : QString();
        text += minutes(one.seconds);
        if (one.increment > 0 || (list->size() == 1 && one.moves == 0))
            text += QStringLiteral("+%1").arg(one.increment);
        parts << text;
    }
    return parts.join(QLatin1Char(':'));
}

QString label(const QString &value)
{
    const QString name = speedName(speed(value));
    if (speed(value) == Speed::Unlimited)
        return name;
    return name.isEmpty() ? shortText(value) : QStringLiteral("%1 %2").arg(name, shortText(value));
}

QString inputText(const QString &value)
{
    const std::optional<QList<Period>> list = periods(value);
    if (!list || list->size() != 1 || list->first().moves > 0 || list->first().seconds % 15 != 0)
        return value.trimmed();
    return shortText(value);
}

std::optional<QString> fromInput(const QString &text)
{
    const QString typed = text.trimmed().replace(QLatin1Char(','), QLatin1Char('.')).remove(QLatin1Char(' '));
    if (typed.isEmpty())
        return QString();
    if (typed == QLatin1String("-"))
        return typed;
    // Already in PGN's form: periods of moves, or seconds with ":".
    if (typed.contains(QLatin1Char('/')) || typed.contains(QLatin1Char(':')))
        return periods(typed) ? std::optional(typed) : std::nullopt;
    // As players say it: minutes, then the seconds a move; a quarter, half
    // or three quarters of a minute as lichess writes them.
    QString decimal = typed;
    decimal.replace(QChar(0x00BD), QLatin1String("0.5")).replace(QChar(0x00BC), QLatin1String("0.25"))
        .replace(QChar(0x00BE), QLatin1String("0.75"));
    static const QRegularExpression said(QStringLiteral(R"(^(\d+(?:\.\d+)?)(?:\+(\d+))?$)"));
    const QRegularExpressionMatch match = said.match(decimal);
    if (!match.hasMatch())
        return std::nullopt;
    const double minutesTyped = match.captured(1).toDouble();
    const qint64 seconds = qRound64(minutesTyped * 60);
    if (seconds <= 0)
        return std::nullopt;
    return QStringLiteral("%1+%2").arg(seconds).arg(match.captured(2).isEmpty() ? QStringLiteral("0") : match.captured(2));
}

} // namespace TimeControl
