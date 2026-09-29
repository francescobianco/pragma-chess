#include "DatabaseProperties.h"

namespace {

const QString kIdKey = QStringLiteral("id");
const QString kTypeKey = QStringLiteral("type");
const QString kDescriptionKey = QStringLiteral("description");
const QString kNameKey = QStringLiteral("name");
const QString kNamePrefix = QStringLiteral("name.");

} // namespace

DatabaseProperties DatabaseProperties::fromValues(const QHash<QString, QString> &values)
{
    DatabaseProperties properties;
    properties.id = values.value(kIdKey);
    properties.type = typeFromKey(values.value(kTypeKey));
    properties.description = values.value(kDescriptionKey);
    properties.name = values.value(kNameKey);
    for (auto it = values.cbegin(); it != values.cend(); ++it) {
        if (it.key().startsWith(kNamePrefix) && it.key().size() > kNamePrefix.size() && !it.value().isEmpty())
            properties.localizedNames.insert(it.key().mid(kNamePrefix.size()).toLower(), it.value());
    }
    return properties;
}

QHash<QString, QString> DatabaseProperties::values() const
{
    QHash<QString, QString> result{{kTypeKey, typeKey(type)}, {kDescriptionKey, description}};
    if (!id.isEmpty())
        result.insert(kIdKey, id);
    if (!name.isEmpty())
        result.insert(kNameKey, name);
    for (auto it = localizedNames.cbegin(); it != localizedNames.cend(); ++it) {
        if (!it.value().isEmpty())
            result.insert(kNamePrefix + it.key(), it.value());
    }
    return result;
}

QString DatabaseProperties::displayName(const QString &languageCode, const QString &fileBaseName) const
{
    const QString code = languageCode.toLower().replace(QLatin1Char('-'), QLatin1Char('_'));
    if (const QString exact = localizedNames.value(code); !exact.isEmpty())
        return exact;
    if (const QString language = localizedNames.value(code.section(QLatin1Char('_'), 0, 0)); !language.isEmpty())
        return language;
    return name.isEmpty() ? fileBaseName : name;
}

QString DatabaseProperties::typeKey(DatabaseType type)
{
    switch (type) {
    case DatabaseType::OpeningBook: return QStringLiteral("opening-book");
    case DatabaseType::GameCollection: break;
    }
    return QStringLiteral("games");
}

DatabaseType DatabaseProperties::typeFromKey(const QString &key)
{
    return key == QLatin1String("opening-book") ? DatabaseType::OpeningBook : DatabaseType::GameCollection;
}
