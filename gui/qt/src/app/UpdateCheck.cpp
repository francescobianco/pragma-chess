#include "UpdateCheck.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QStringList>

namespace UpdateCheck {

QString latestUrl()
{
    return QStringLiteral("https://github.com/francescobianco/pragma-chess/releases/latest/download/version.json");
}

std::optional<Release> parse(const QByteArray &json)
{
    const QJsonObject object = QJsonDocument::fromJson(json).object();
    const QString version = object.value(QStringLiteral("version")).toString().trimmed();
    if (version.isEmpty())
        return std::nullopt;
    return Release{version, object.value(QStringLiteral("url")).toString()};
}

bool isNewer(const QString &candidate, const QString &current)
{
    const auto parts = [](QString version) {
        if (version.startsWith(QLatin1Char('v')))
            version.remove(0, 1);
        const QString release = version.section(QLatin1Char('-'), 0, 0);
        const bool pre = version.contains(QLatin1Char('-'));
        QList<int> numbers;
        for (const QString &part : release.split(QLatin1Char('.')))
            numbers << part.toInt();
        while (numbers.size() < 3)
            numbers << 0;
        return std::pair{numbers, pre};
    };
    const auto [a, aPre] = parts(candidate);
    const auto [b, bPre] = parts(current);
    if (a != b)
        return std::lexicographical_compare(b.begin(), b.end(), a.begin(), a.end());
    return bPre && !aPre; // The release after its pre-release.
}

} // namespace UpdateCheck
